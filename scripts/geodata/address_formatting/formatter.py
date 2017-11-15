# -*- coding: utf-8 -*-
import copy
import os
import pystache
import random
import re
import six
import subprocess
import yaml

from collections import OrderedDict, defaultdict
from itertools import ifilter

from geodata.address_formatting.aliases import Aliases
from geodata.configs.utils import nested_get, recursive_merge
from geodata.math.floats import isclose
from geodata.math.sampling import weighted_choice, cdf
from geodata.text.tokenize import tokenize, tokenize_raw, token_types
from geodata.encoding import safe_decode

FORMATTER_GIT_REPO = 'https://github.com/OpenCageData/address-formatting'

this_dir = os.path.realpath(os.path.dirname(__file__))

FORMATTER_CONFIG = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                'resources', 'formatting', 'global.yaml')


class AddressFormatter(object):
    '''
    Approximate Python port of lokku's Geo::Address::Formatter

    Usage:
        address_formatter = AddressFormatter()
        components = {
            'house': u'Anticafé',
            'house_number': '2',
            'road': u'Calle de la Unión',
            'postcode': '28013',
            'city': u'Madrid',
        }
        country = 'es'
        language = 'es'
        address_formatter.format_address(components, country, language)
    '''

    whitespace_component_regex = re.compile('[\r\n]+[\s\r\n]*')

    splitter = ' | '

    separator_tag = 'SEP'
    field_separator_tag = 'FSEP'

    CATEGORY = 'category'
    NEAR = 'near'
    ATTENTION = 'attention'
    CARE_OF = 'care_of'
    HOUSE = 'house'
    HOUSE_NUMBER = 'house_number'
    PO_BOX = 'po_box'
    ROAD = 'road'
    BUILDING = 'building'
    ENTRANCE = 'entrance'
    STAIRCASE = 'staircase'
    LEVEL = 'level'
    UNIT = 'unit'
    INTERSECTION = 'intersection'
    SUBDIVISION = 'subdivision'
    METRO_STATION = 'metro_station'
    SUBURB = 'suburb'
    CITY_DISTRICT = 'city_district'
    CITY = 'city'
    ISLAND = 'island'
    STATE = 'state'
    STATE_DISTRICT = 'state_district'
    POSTCODE = 'postcode'
    COUNTRY_REGION = 'country_region'
    COUNTRY = 'country'
    WORLD_REGION = 'world_region'

    component_order = {k: i for i, k in enumerate([
        CATEGORY,
        NEAR,
        ATTENTION,
        CARE_OF,
        HOUSE,
        PO_BOX,
        HOUSE_NUMBER,
        BUILDING,
        ENTRANCE,
        STAIRCASE,
        LEVEL,
        UNIT,
        ROAD,
        INTERSECTION,
        SUBDIVISION,
        METRO_STATION,
        SUBURB,
        CITY,
        CITY_DISTRICT,
        ISLAND,
        STATE,
        STATE_DISTRICT,
        POSTCODE,
        COUNTRY_REGION,
        COUNTRY,
        WORLD_REGION,
    ])}

    BOUNDARY_COMPONENTS_ORDERED = [
        SUBDIVISION,
        METRO_STATION,
        SUBURB,
        CITY_DISTRICT,
        CITY,
        ISLAND,
        STATE_DISTRICT,
        STATE,
        COUNTRY_REGION,
        COUNTRY,
        WORLD_REGION,
    ]

    BOUNDARY_COMPONENTS = set(BOUNDARY_COMPONENTS_ORDERED)

    SUB_BUILDING_COMPONENTS = {
        ENTRANCE,
        STAIRCASE,
        LEVEL,
        UNIT,
    }

    STREET_COMPONENTS = {
        HOUSE_NUMBER,
        ROAD,
    }

    ADDRESS_LEVEL_COMPONENTS = STREET_COMPONENTS | SUB_BUILDING_COMPONENTS

    NAME_COMPONENTS = {
        ATTENTION,
        CARE_OF,
        HOUSE,
    }

    address_formatter_fields = set(component_order)

    aliases = Aliases(
        OrderedDict([
            ('street', ROAD),
            ('street_name', ROAD),
            ('hamlet', CITY),
            ('village', CITY),
            ('neighborhood', SUBURB),
            ('neighbourhood', SUBURB),
            ('city_district', CITY_DISTRICT),
            ('county', STATE_DISTRICT),
            ('state_code', STATE),
            ('country_name', COUNTRY),
            ('continent', WORLD_REGION),
            ('postal_code', POSTCODE),
            ('post_code', POSTCODE),
        ])
    )

    category_template = '{{{category}}} {{{near}}} {{{place}}}'
    chain_template = '{{{house}}} {{{near}}} {{{place}}}'
    intersection_template = '{{{road1}}} {{{intersection}}} {{{road2}}} {{{place}}}'

    template_address_parts = [HOUSE, HOUSE_NUMBER, ROAD]
    template_admin_parts = [CITY, STATE, COUNTRY]

    template_address_parts_re = re.compile('|'.join(['\{{{key}\}}'.format(key=key) for key in template_address_parts]))
    template_admin_parts_re = re.compile('|'.join(['\{{{key}\}}'.format(key=key) for key in template_admin_parts]))

    MINIMAL_COMPONENT_KEYS = [
        (ROAD, HOUSE_NUMBER),
        (ROAD, HOUSE),
        (ROAD, POSTCODE)
    ]

    FIRST, BEFORE, AFTER, LAST = range(4)

    def __init__(self, scratch_dir='/tmp', splitter=None):
        if splitter is not None:
            self.splitter = splitter

        self.formatter_repo_path = os.path.join(scratch_dir, 'address-formatting')
        self.clone_repo()

        self.load_config()
        self.load_country_formats()

        self.language_code_replacements = self.config['language_code_replacements']

        self.setup_insertion_probabilities()
        self.setup_no_name_templates()
        self.setup_place_only_templates()

        self.template_cache = {}
        self.parsed_cache = {}

    def clone_repo(self):
        subprocess.check_call(['rm', '-rf', self.formatter_repo_path])
        subprocess.check_call(['git', 'clone', FORMATTER_GIT_REPO, self.formatter_repo_path])

    def load_country_formats(self):
        config = yaml.load(open(os.path.join(self.formatter_repo_path,
                                'conf', 'countries', 'worldwide.yaml')))
        self.country_aliases = {}
        self.house_number_ordering = {}

        for key in list(config):
            country = key
            language = None
            if '_' in key:
                country, language = country.split('_', 1)
            value = config[key]
            if hasattr(value, 'items'):
                address_template = value.get('address_template')
                if not address_template and 'use_country' in value:
                    # Temporary fix for Norway territories (NO unquoted is a boolean) and recursive references
                    if value['use_country'] in (country, False):
                        continue
                    self.country_aliases[country] = value['use_country']
                    address_template = config[value['use_country']]['address_template']

                if address_template:
                    value['address_template'] = self.add_postprocessing_tags(address_template, country, language=language)

                post_format_replacements = value.get('postformat_replace')
                if post_format_replacements:
                    value['postformat_replace'] = [[pattern, replacement.replace('$', '\\')] for pattern, replacement in post_format_replacements]
            else:
                address_template = value
                config[country] = self.add_postprocessing_tags(value, country, language=language)

            try:
                house_number_index = address_template.index(self.tag_token(self.HOUSE_NUMBER))
                road_index = address_template.index(self.tag_token(self.ROAD))

                if house_number_index < road_index:
                    self.house_number_ordering[key.lower()] = -1
                else:
                    self.house_number_ordering[key.lower()] = 1
            except ValueError:
                self.house_number_ordering[key.lower()] = 0

        self.country_formats = config

    def load_config(self):
        config = yaml.load(open(FORMATTER_CONFIG))
        self.config = config.get('global', {})
        language_configs = config.get('languages', {})

        self.language_configs = {}
        for language in language_configs:
            language_config = language_configs[language]
            config_copy = copy.deepcopy(self.config)
            self.language_configs[language] = recursive_merge(config_copy, language_config)

        country_configs = config.get('countries', {})

        self.country_configs = {}
        for country in country_configs:
            country_config = country_configs[country]
            config_copy = copy.deepcopy(self.config)
            self.country_configs[country] = recursive_merge(config_copy, country_config)

    def get_property(self, keys, country, language=None, default=None):
        if isinstance(keys, six.string_types):
            keys = keys.split('.')
        keys = tuple(keys)
        value = nested_get(self.language_configs, (language,) + keys, default=default)
        if not value:
            value = nested_get(self.country_configs, (country,) + keys, default=default)
        if not value:
            value = nested_get(self.config, keys, default=default)
        return value

    def insertion_distribution(self, insertions):
        values = []
        probs = []

        for k, v in six.iteritems(insertions):
            if k == 'conditional' or not v:
                continue

            if 'before' in v:
                val = (self.BEFORE, v['before'])
            elif 'after' in v:
                val = (self.AFTER, v['after'])
            elif 'last' in v:
                val = (self.LAST, None)
            elif 'first' in v:
                val = (self.FIRST, None)
            else:
                raise ValueError('Insertions must contain one of {{first, before, after, last}}. Value was: {}'.format(v))

            prob = v['probability']
            values.append(val)
            probs.append(prob)

        # If the probabilities don't sum to 1, add a "do nothing" action
        if not isclose(sum(probs), 1.0):
            probs.append(1.0 - sum(probs))
            values.append((None, None, False))

        return values, cdf(probs)

    def insertion_probs(self, config):
        component_insertions = {}
        for component, insertions in six.iteritems(config):
            component_insertions[component] = self.insertion_distribution(insertions)

        return component_insertions

    def inverted(self, template):
        lines = template.split(six.u('\n'))
        return six.u('\n').join(reversed(lines))

    def house_number_before_road(self, country, language=None):
        key = value = None
        if language is not None:
            key = six.u('_').join((country.lower(), language.lower()))
            if key in self.house_number_ordering:
                value = self.house_number_ordering[key]

        if value is None:
            key = country
            if key in self.house_number_ordering:
                value = self.house_number_ordering[key]

        if value is None:
            value = 0

        if value <= 0:
            return True
        else:
            return False

    def conditional_insertion_probs(self, conditionals):
        conditional_insertions = defaultdict(OrderedDict)
        for component, value in six.iteritems(conditionals):
            if 'conditional' in value:
                conditionals = value['conditional']

                for c in conditionals:
                    other = c['component']
                    conditional_insertions[component][other] = self.insertion_distribution(c['probabilities'])
        return conditional_insertions

    def setup_insertion_probabilities(self):
        config = self.config['insertions']
        self.global_insertions = self.insertion_probs(config)
        self.global_conditionals = self.conditional_insertion_probs(config)

        self.global_invert_probability = self.config.get('invert_probability', 0.0)

        self.country_insertions = {}
        self.country_conditionals = {}

        self.country_invert_probabilities = {}

        for country, config in six.iteritems(self.country_configs):
            if 'insertions' in config:
                self.country_insertions[country.lower()] = self.insertion_probs(config['insertions'])
                self.country_conditionals[country.lower()] = self.conditional_insertion_probs(config['insertions'])

            if 'invert_probability' in config:
                self.country_invert_probabilities[country] = config['invert_probability']

        self.language_insertions = {}
        self.language_conditionals = {}

        for language, config in six.iteritems(self.language_configs):
            if 'insertions' in config:
                self.language_insertions[language.lower()] = self.insertion_probs(config['insertions'])
                self.language_conditionals[language.lower()] = self.conditional_insertion_probs(config['insertions'])

    def setup_no_name_templates(self):
        self.templates_no_name = {}

        for country, config in six.iteritems(self.country_formats):
            if hasattr(config, 'items') and 'address_template' in config:
                address_template = self.remove_components(config['address_template'], self.NAME_COMPONENTS)
                self.templates_no_name[country] = address_template

    def setup_place_only_templates(self):
        self.templates_place_only = {}

        for country, config in six.iteritems(self.country_formats):
            if hasattr(config, 'items') and 'address_template' in config:
                address_template = self.remove_components(config['address_template'], self.NAME_COMPONENTS | self.ADDRESS_LEVEL_COMPONENTS)
                self.templates_place_only[country] = address_template

    def country_template(self, c):
        return self.country_formats.get(c, self.country_formats['default'])

    def is_reverse(self, template):
        address_parts_match = self.template_address_parts_re.search(template)
        admin_parts_match = list(self.template_admin_parts_re.finditer(template))

        # last instance of city/state/country occurs before the first instance of house_number/road
        return admin_parts_match[-1].start() < address_parts_match.start()

    def build_first_of_template(self, keys):
        """ For constructing """
        return '{{{{#first}}}} {keys} {{{{/first}}}}'.format(keys=' || '.join(['{{{{{{{key}}}}}}}'.format(key=key) for key in keys]))

    def tag_token(self, key):
        return '{{{{{{{key}}}}}}}'.format(key=key)

    def remove_components(self, template, tags):
        new_components = []
        tags = set(tags)

        parsed = pystache.parse(safe_decode(template))

        last_removed = False
        for i, el in enumerate(parsed._parse_tree):
            if hasattr(el, 'parsed'):
                keys = [e.key for e in el.parsed._parse_tree if hasattr(e, 'key') and e.key not in tags]
                if keys:
                    new_components.append(self.build_first_of_template(keys))
                    last_removed = False
                else:
                    last_removed = True
            elif hasattr(el, 'key'):
                if el.key not in tags:
                    new_components.append('{{{{{{{key}}}}}}}'.format(key=el.key))
                    last_removed = False
                else:
                    last_removed = True

            elif not last_removed:
                new_components.append(el)
            else:
                last_removed = False
        return ''.join(new_components).strip()

    def insert_component(self, template, tag, before=None, after=None, first=False, last=False,
                         separate=True, is_reverse=False, exact_order=True):
        if not before and not after and not first and not last:
            return

        template = template.rstrip()

        if not exact_order:
            first_template_regex = re.compile(six.u('{{#first}}.*?{{/first}}'), re.UNICODE)
            sans_firsts = first_template_regex.sub(six.u(''), template)

            tag_match = re.compile(self.tag_token(tag)).search(sans_firsts)

            if before:
                before_match = re.compile(self.tag_token(before)).search(sans_firsts)
                if before_match and tag_match and before_match.start() > tag_match.start():
                    return template

            if after:
                after_match = re.compile(self.tag_token(after)).search(sans_firsts)
                if after_match and tag_match and tag_match.start() > after_match.start():
                    return template

        key_added = False
        skip_next_non_token = False
        new_components = []

        tag_token = self.tag_token(tag)

        parsed = pystache.parse(safe_decode(template))
        num_tokens = len(parsed._parse_tree)
        for i, el in enumerate(parsed._parse_tree):

            if hasattr(el, 'parsed'):
                keys = [e.key for e in el.parsed._parse_tree if hasattr(e, 'key')]
                if (before in set(keys) or first) and not key_added:
                    token = new_components[-1] if new_components and '{' not in new_components[-1] else '\n'
                    new_components.extend([tag_token, token])
                    key_added = True

                keys = [k for k in keys if self.aliases.get(k, k) != tag]
                if keys:
                    new_components.append(self.build_first_of_template(keys))
                else:
                    while new_components and '{' not in new_components[-1]:
                        new_components.pop()
                    continue

                if (after in set(keys) or i == num_tokens - 1) and not key_added:
                    token = '\n'
                    if i < num_tokens - 1 and isinstance(parsed._parse_tree[i + 1], six.string_types):
                        token = parsed._parse_tree[i + 1]
                    new_components.extend([token, tag_token])
                    key_added = True

            elif hasattr(el, 'key'):
                if el.key == tag:
                    if i == num_tokens - 1 and last:
                        new_components.append('{{{{{{{key}}}}}}}'.format(key=el.key))

                    skip_next_non_token = True
                    continue

                if (el.key == before or first) and not key_added:
                    token = '\n'
                    if new_components and '{' not in new_components[-1]:
                        token = new_components[-1]
                    new_components.extend([tag_token, token])
                    key_added = True

                new_components.append('{{{{{{{key}}}}}}}'.format(key=el.key))

                if (el.key == after or i == num_tokens - 1) and not key_added:
                    token = '\n'
                    if i < num_tokens - 1 and isinstance(parsed._parse_tree[i + 1], six.string_types):
                        token = parsed._parse_tree[i + 1]
                    new_components.extend([token, tag_token])
                    key_added = True
            elif not skip_next_non_token:
                new_components.append(el)

            if i == num_tokens - 1 and not key_added:
                key_added = True
                new_components.append(tag_token)

            skip_next_non_token = False

        return ''.join(new_components)

    def add_postprocessing_tags(self, template, country, language=None):
        is_reverse = self.is_reverse(template)

        i = None
        pivot = None

        pivot_keys = (AddressFormatter.CITY, AddressFormatter.STATE, AddressFormatter.COUNTRY)

        for component in pivot_keys:
            token = self.tag_token(component)
            if token in template:
                i = self.BOUNDARY_COMPONENTS_ORDERED.index(component)
                pivot = component
                break

        if i is None:
            raise ValueError('Template {} does not contain one of {{{}}}'.format(country, ','.join(pivot_keys)))

        prev = pivot

        if i > 1:
            for component in self.BOUNDARY_COMPONENTS_ORDERED[i - 1:0:-1]:
                kw = {'before': prev} if not is_reverse else {'after': prev}
                template = self.insert_component(template, component, exact_order=False, **kw)
                prev = component

        prev = pivot

        if i < len(self.BOUNDARY_COMPONENTS_ORDERED) - 1:
            for component in self.BOUNDARY_COMPONENTS_ORDERED[i + 1:]:
                kw = {'after': prev} if not is_reverse else {'before': prev}
                template = self.insert_component(template, component, exact_order=False, **kw)
                prev = component

        return template

    def render_template(self, template, components, tagged=False):
        def render_first(text):
            text = pystache.render(text, **components)
            splits = (e.strip() for e in text.split('||'))
            selected = next(ifilter(bool, splits), '')
            return selected

        output = pystache.render(template, first=render_first,
                                 **components).strip()

        values = self.whitespace_component_regex.split(output)

        splitter = self.splitter if not tagged else ' {}/{} '.format(self.splitter.strip(), self.field_separator_tag)

        values = [self.strip_component(val, tagged=tagged) for val in values]

        output = splitter.join([
            val for val in values if val.strip()
        ])

        return output

    def minimal_components(self, components):
        for component_list in self.MINIMAL_COMPONENT_KEYS:
            if all((c in components for c in component_list)):
                return True
        return False

    def post_replacements(self, template, text):
        components = []
        seen = set()
        for component in text.split(self.splitter):
            component = component.strip()
            if component not in seen:
                components.append(component)
                seen.add(component)
        text = self.splitter.join(components)
        post_format_replacements = template.get('postformat_replace')
        if post_format_replacements:
            for regex, replacement in post_format_replacements:
                text = re.sub(regex, replacement, text)
        return text

    def revised_template(self, template, components, country, language=None):
        if not template:
            return None

        country_language = None
        if language:
            country_language = '{}_{}'.format(country, language)

        alias_country = self.country_aliases.get(country.upper(), country).lower()
        for term in (country, country_language):
            if term in self.country_insertions or term in self.country_conditionals:
                break
        else:
            country = alias_country

        cache_keys = []

        invert_probability = self.country_invert_probabilities.get(country, self.global_invert_probability)
        if random.random() < invert_probability:
            cache_keys.append('inverted')
            cache_key = tuple(sorted(cache_keys))
            if cache_key in self.template_cache:
                template = self.template_cache[cache_key]
            else:
                template = self.inverted(template)
                self.template_cache[cache_key] = template

        for component in sorted(components, key=self.component_order.get):
            scope = country
            insertions = nested_get(self.country_insertions, (country, component), default=None)
            conditionals = nested_get(self.country_conditionals, (country, component), default=None)

            if insertions is None and language:
                insertions = nested_get(self.country_insertions, (country_language, component), default=None)
                scope = country_language

            if conditionals is None and language:
                conditionals = nested_get(self.country_conditionals, (country_language, component), default=None)

            if insertions is None and language:
                insertions = nested_get(self.language_insertions, (language, component), default=None)
                scope = 'lang:{}'.format(language)

            if conditionals is None and language:
                conditionals = nested_get(self.language_conditionals, (language, component), default=None)

            if insertions is None:
                insertions = nested_get(self.global_insertions, (component,), default=None)
                scope = None

            if conditionals is None:
                conditionals = nested_get(self.global_conditionals, (component,), default=None)

            if insertions is not None:
                conditional_insertions = None
                if conditionals is not None:
                    for k, v in six.iteritems(conditionals):
                        if k in components:
                            conditional_insertions = v
                            break

                order, other = None, None

                # Check the conditional probabilities first
                if conditional_insertions is not None:
                    values, probs = conditional_insertions
                    order, other = weighted_choice(values, probs)

                # If there are no conditional probabilites or the "default" value was chosen, sample from the marginals
                if other is None:
                    values, probs = insertions
                    order, other = weighted_choice(values, probs)

                # Even though we may change the value of "other" below, use
                # the original cache key because changes from here on are
                # deterministic and should be cached.
                insertion_id = (scope, component, order, other)
                cache_keys.append(insertion_id)

                cache_key = tuple(sorted(cache_keys))

                if cache_key in self.template_cache:
                    template = self.template_cache[cache_key]
                    continue

                other_token = self.tag_token(other)

                # Don't allow insertions between road and house_number
                # This can happen if e.g. "level" is supposed to be inserted
                # after house number assuming that it's a continental European
                # address where house number comes after road. If in a previous
                # insertion we were to swap house_number and road to create an
                # English-style address, the final ordering would be
                # house_number, unit, road, which we don't want. So effectively
                # treat house_number and road as an atomic unit.

                if other == self.HOUSE_NUMBER and component != self.ROAD:
                    road_tag = self.tag_token(self.ROAD)
                    house_number_tag = other_token

                    if house_number_tag in template and road_tag in template:
                        road_after_house_number = template.index(road_tag) > template.index(house_number_tag)

                        if road_after_house_number and order == self.AFTER:
                            other = self.ROAD
                        elif not road_after_house_number and order == self.BEFORE:
                            other = self.ROAD
                elif other == self.ROAD and component != self.HOUSE_NUMBER:
                    house_number_tag = self.tag_token(self.HOUSE_NUMBER)
                    road_tag = other_token

                    if house_number_tag in template and road_tag in template:
                        road_before_house_number = template.index(road_tag) < template.index(house_number_tag)

                        if road_before_house_number and order == self.AFTER:
                            other = self.HOUSE_NUMBER
                        elif not road_before_house_number and order == self.BEFORE:
                            other = self.HOUSE_NUMBER

                if order == self.BEFORE and other_token in template:
                    template = self.insert_component(template, component, before=other)
                elif order == self.AFTER and other_token in template:
                    template = self.insert_component(template, component, after=other)
                elif order == self.LAST:
                    template = self.insert_component(template, component, last=True)
                elif order == self.FIRST:
                    template = self.insert_component(template, component, first=True)
                else:
                    continue

                self.template_cache[cache_key] = template

        return template

    def remove_repeat_template_separators(self, template):
        return re.sub('(?:[\s]*([,;\-]/{})[\s]*){{2,}}'.format(self.separator_tag), r' \1 ', template)

    def tag_template_separators(self, template):
        template = re.sub(r'}\s*([,\-;])\s*', r'}} \1/{} '.format(self.separator_tag), template)
        return template

    def strip_component(self, value, tagged=False):
        if not tagged:
            comma = token_types.COMMA.value
            hyphen = token_types.HYPHEN.value

            start = end = 0
            tokens = tokenize_raw(value.strip())
            for token_start, token_length, token_type in tokens:
                start = token_start
                if token_type not in (comma, hyphen):
                    break
                else:
                    start = token_start + token_length

            for token_start, token_length, token_type in reversed(tokens):
                end = token_start + token_length
                if token_type not in (comma, hyphen):
                    break
                else:
                    end = token_start

            return value[start:end]
        else:
            start = end = 0
            tokens = value.split()

            separator_tag = self.separator_tag

            for i, t in enumerate(tokens):
                t, c = t.rsplit('/', 1)
                start = i
                if c != separator_tag:
                    break
                else:
                    start = i + 1

            num_tokens = len(tokens)

            for j, t in enumerate(reversed(tokens)):
                t, c = t.rsplit('/', 1)
                end = num_tokens - j
                if c != separator_tag:
                    break
                else:
                    end = num_tokens - j - 1

            return six.u(' ').join(tokens[start:end])

    def get_template_from_config(self, config, country, language=None):
        template = None
        if language:
            language = self.language_code_replacements.get(language, language.split('_')[0])
            # For countries like China and Japan where the country format varies
            # based on which language is being used
            template = config.get('{}_{}'.format(country.upper(), language.lower()), None)

        if not template:
            template = config.get(country.upper())

        if not template:
            return None

        return template

    def get_template(self, country, language=None):
        return self.get_template_from_config(self.country_formats, country, language=language)

    def get_no_name_template(self, country, language=None):
        return self.get_template_from_config(self.templates_no_name, country, language=language)

    def get_place_template(self, country, language=None):
        return self.get_template_from_config(self.templates_place_only, country, language=language)

    def tagged_tokens(self, name, label):
        return six.u(' ').join([six.u('{}/{}').format(t.replace(' ', ''), label if t != ',' else self.separator_tag) for t, c in tokenize(name)])

    def template_language_matters(self, country, language):
        return '{}_{}'.format(country.upper(), language) in self.country_formats or '{}_{}'.format(country, language) in self.country_formats

    def format_category_query(self, category_query, address_components, country, language, tag_components=True):
        if tag_components:
            components = {self.CATEGORY: self.tagged_tokens(category_query.category, self.CATEGORY)}
            if category_query.prep is not None:
                components[self.NEAR] = self.tagged_tokens(category_query.prep, self.NEAR)
        else:
            components = {self.CATEGORY: category_query.category}
            if category_query.prep is not None:
                components[self.NEAR] = category_query.prep

        if category_query.add_place_name or category_query.add_address:
            place_formatted = self.format_address(address_components, country, language=language,
                                                  minimal_only=False, tag_components=tag_components)
            if not place_formatted:
                return None
            components['place'] = place_formatted

        return self.render_template(self.category_template, components, tagged=tag_components)

    def format_chain_query(self, chain_query, address_components, country, language, tag_components=True):
        if tag_components:
            components = {self.HOUSE: self.tagged_tokens(chain_query.name, self.HOUSE)}
            if chain_query.prep is not None:
                components[self.NEAR] = self.tagged_tokens(chain_query.prep, self.NEAR)
        else:
            components = {self.HOUSE: chain_query.name}
            if chain_query.prep is not None:
                components[self.NEAR] = chain_query.prep

        if chain_query.add_place_name or chain_query.add_address:
            place_formatted = self.format_address(address_components, country, language=language,
                                                  minimal_only=False, tag_components=tag_components)
            if not place_formatted:
                return None
            components['place'] = place_formatted

        return self.render_template(self.chain_template, components, tagged=tag_components)

    def format_intersection(self, intersection_query, place_components, country, language, tag_components=True):
        components = {}
        if tag_components:
            components = {'road1': self.tagged_tokens(intersection_query.road1, self.ROAD),
                          'intersection': self.tagged_tokens(intersection_query.intersection_phrase, self.INTERSECTION),
                          'road2': self.tagged_tokens(intersection_query.road2, self.ROAD),
                          }
        else:
            components = {'road1': intersection_query.road1,
                          'intersection': intersection_query.intersection_phrase,
                          'road2': intersection_query.road2}

        if place_components:
            place_formatted = self.format_address(place_components, country, language=language,
                                                  minimal_only=False, tag_components=tag_components)

            if place_formatted:
                components['place'] = place_formatted
        return self.render_template(self.intersection_template, components, tagged=tag_components)

    def format_address(self, components, country, language,
                       minimal_only=True, tag_components=True, replace_aliases=True):
        if minimal_only and not self.minimal_components(components):
            return None

        template = self.get_template(country, language=language)
        if not template:
            return None

        if not template or 'address_template' not in template:
            return None
        template_text = template['address_template']

        template_text = self.revised_template(template_text, components, country, language=language)
        if template_text is None:
            return None

        if tag_components:
            template_text = self.tag_template_separators(template_text)

        if template_text in self.parsed_cache:
            template = self.parsed_cache[template_text]
        else:
            template = pystache.parse(template_text)
            self.parsed_cache[template_text] = template

        if replace_aliases:
            self.aliases.replace(components)

        if tag_components:
            components = {k: self.tagged_tokens(v, k) for k, v in six.iteritems(components)}

        text = self.render_template(template, components, tagged=tag_components)

        text = self.remove_repeat_template_separators(text)

        return text
