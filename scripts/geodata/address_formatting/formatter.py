# -*- coding: utf-8 -*-
import os
import pystache
import re
import six
import subprocess
import yaml

from geodata.address_formatting.aliases import Aliases
from geodata.text.tokenize import tokenize, tokenize_raw, token_types
from geodata.encoding import safe_decode
from collections import OrderedDict
from itertools import ifilter

FORMATTER_GIT_REPO = 'https://github.com/OpenCageData/address-formatting'


class AddressFormatter(object):
    '''
    Approximate Python port of lokku's Geo::Address::Formatter

    Usage:
        address_formatter = AddressFormatter()
        components = {
            'house': u'Anticafé',
            'addr:housenumber': '2',
            'addr:street': u'Calle de la Unión',
            'addr:postcode': '28013',
            'addr:city': u'Madrid',
        }
        address_formatter.format_address('es', components)
    '''

    whitespace_component_regex = re.compile('[\r\n]+[\s\r\n]*')

    splitter = ' | '

    separator_tag = 'SEP'
    field_separator_tag = 'FSEP'

    CATEGORY = 'category'
    NEAR = 'near'
    HOUSE = 'house'
    HOUSE_NUMBER = 'house_number'
    PO_BOX = 'po_box'
    CARE_OF = 'care_of'
    BLOCK = 'block'
    BUILDING = 'building'
    LEVEL = 'level'
    UNIT = 'unit'
    INTERSECTION = 'intersection'
    ROAD = 'road'
    SUBURB = 'suburb'
    CITY_DISTRICT = 'city_district'
    CITY = 'city'
    ISLAND = 'island'
    STATE = 'state'
    STATE_DISTRICT = 'state_district'
    POSTCODE = 'postcode'
    COUNTRY = 'country'

    address_formatter_fields = set([
        CATEGORY,
        NEAR,
        HOUSE,
        HOUSE_NUMBER,
        PO_BOX,
        CARE_OF,
        BLOCK,
        BUILDING,
        LEVEL,
        UNIT,
        INTERSECTION,
        ROAD,
        SUBURB,
        CITY,
        CITY_DISTRICT,
        ISLAND,
        STATE,
        STATE_DISTRICT,
        POSTCODE,
        COUNTRY,
    ])

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
            ('postal_code', POSTCODE),
            ('post_code', POSTCODE),
        ])
    )

    template_address_parts = [HOUSE, HOUSE_NUMBER, ROAD]
    template_admin_parts = [CITY, STATE, COUNTRY]

    template_address_parts_re = re.compile('|'.join(['\{{{key}\}}'.format(key=key) for key in template_address_parts]))
    template_admin_parts_re = re.compile('|'.join(['\{{{key}\}}'.format(key=key) for key in template_admin_parts]))

    MINIMAL_COMPONENT_KEYS = [
        (ROAD, HOUSE_NUMBER),
        (ROAD, HOUSE),
        (ROAD, POSTCODE)
    ]

    def __init__(self, scratch_dir='/tmp', splitter=None):
        if splitter is not None:
            self.splitter = splitter

        self.formatter_repo_path = os.path.join(scratch_dir, 'address-formatting')
        self.clone_repo()
        self.load_config()

    def clone_repo(self):
        subprocess.check_call(['rm', '-rf', self.formatter_repo_path])
        subprocess.check_call(['git', 'clone', FORMATTER_GIT_REPO, self.formatter_repo_path])

    def load_config(self):
        config = yaml.load(open(os.path.join(self.formatter_repo_path,
                                'conf/countries/worldwide.yaml')))
        for key, value in config.items():
            if hasattr(value, 'items'):
                address_template = value.get('address_template')
                if address_template:
                    value['address_template'] = self.add_postprocessing_tags(address_template)

                post_format_replacements = value.get('postformat_replace')
                if post_format_replacements:
                    value['postformat_replace'] = [[pattern, replacement.replace('$', '\\')] for pattern, replacement in post_format_replacements]
            else:
                address_template = value
                config[key] = self.add_postprocessing_tags(value)
        self.config = config

    def country_template(self, c):
        return self.config.get(c, self.config['default'])

    postprocessing_tags = [
        (SUBURB, (ROAD,), (CITY_DISTRICT, CITY, ISLAND, STATE_DISTRICT, STATE, POSTCODE, COUNTRY)),
        (CITY_DISTRICT, (ROAD, SUBURB), (CITY, ISLAND, STATE_DISTRICT, STATE)),
        (STATE_DISTRICT, (SUBURB, CITY_DISTRICT, CITY, ISLAND), (STATE,)),
        (STATE, (SUBURB, CITY_DISTRICT, CITY, ISLAND, STATE_DISTRICT), (COUNTRY,)),
    ]

    template_tag_replacements = [
        ('county', STATE_DISTRICT),
    ]

    def is_reverse(self, key, template):
        address_parts_match = self.template_address_parts_re.search(template)
        admin_parts_match = list(self.template_admin_parts_re.finditer(template))

        if not address_parts_match:
            raise ValueError('Template for {} does not contain any address parts'.format(key))
        elif not admin_parts_match:
            raise ValueError('Template for {} does not contain any admin parts'.format(key))

        # last instance of city/state/country occurs before the first instance of house_number/road
        return admin_parts_match[-1].start() < address_parts_match.start()

    def build_first_of_template(self, keys):
        """ For constructing """
        return '{{{{#first}}}} {keys} {{{{/first}}}}'.format(keys=' || '.join(['{{{{{{{key}}}}}}}'.format(key=key) for key in keys]))

    def insert_component(self, template, tag, before=(), after=(), separate=True, is_reverse=False):
        if not before and not after:
            return

        tag_match = re.compile('\{{{key}\}}'.format(key=tag)).search(template)

        if before:
            before_match = re.compile('|'.join(['\{{{key}\}}'.format(key=key) for key in before])).search(template)
            if before_match and tag_match and before_match.start() > tag_match.start():
                return template

        if after:
            after_match = re.compile('|'.join(['\{{{key}\}}'.format(key=key) for key in after])).search(template)
            if after_match and tag_match and tag_match.start() > after_match.start():
                return template

        before = set(before)
        after = set(after)

        key_added = False
        skip_next_non_token = False
        new_components = []

        tag_token = '{{{{{{{key}}}}}}}'.format(key=tag)

        parsed = pystache.parse(safe_decode(template))
        num_tokens = len(parsed._parse_tree)
        for i, el in enumerate(parsed._parse_tree):

            if hasattr(el, 'parsed'):
                keys = [e.key for e in el.parsed._parse_tree if hasattr(e, 'key')]
                if set(keys) & before and not key_added:
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

                if set(keys) & after and not key_added:
                    token = '\n'
                    if i < num_tokens - 1 and isinstance(parsed._parse_tree[i + 1], six.string_types):
                        token = parsed._parse_tree[i + 1]
                    new_components.extend([token, tag_token])
                    key_added = True

            elif hasattr(el, 'key'):
                if el.key == tag:
                    skip_next_non_token = True
                    continue

                if el.key in before and not key_added:
                    token = '\n'
                    if new_components and '{' not in new_components[-1]:
                        token = new_components[-1]
                    new_components.extend([tag_token, token])
                    key_added = True

                new_components.append('{{{{{{{key}}}}}}}'.format(key=el.key))

                if el.key in after and not key_added:
                    token = '\n'
                    if i < num_tokens - 1 and isinstance(parsed._parse_tree[i + 1], six.string_types):
                        token = parsed._parse_tree[i + 1]
                    new_components.extend([token, tag_token])
                    key_added = True
            elif not skip_next_non_token:
                new_components.append(el)

            skip_next_non_token = False

        return ''.join(new_components)

    def add_postprocessing_tags(self, template):
        is_reverse = self.is_reverse(template)
        for key, pre_keys, post_keys in self.postprocessing_tags:
            key_included = key in template
            new_components = []
            if key_included:
                continue

            for line in template.split('\n'):
                pre_key = re.compile('|'.join(pre_keys)).search(line)
                post_key = re.compile('|'.join(post_keys)).search(line)
                if post_key and not pre_key and not key_included:
                    if not is_reverse:
                        new_components.append(u'{{{{{{{key}}}}}}}'.format(key=key))
                        key_included = True
                new_components.append(line.rstrip('\n'))
                if post_key and not pre_key and not key_included and is_reverse:
                    new_components.append(u'{{{{{{{key}}}}}}}'.format(key=key))
                    key_included = True
            template = u'\n'.join(new_components)
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

    def apply_replacements(self, template, components):
        if not template.get('replace'):
            return
        for key in components.keys():
            value = components[key]
            for regex, replacement in template['replace']:
                value = re.sub(regex, replacement, value)
                components[key] = value

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

    def tag_template_separators(self, template):
        template = re.sub(r'},', '}} ,/{} '.format(self.separator_tag), template)
        template = re.sub(r'}-', '}} -/{} '.format(self.separator_tag), template)
        template = re.sub(r' - ', ' -/{} '.format(self.separator_tag), template)
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

            return u' '.join(tokens[start:end])

    def format_address(self, country, components, 
                       minimal_only=True, tag_components=True, replace_aliases=True,
                       template_replacements=False):
        template = self.config.get(country.upper())
        if not template:
            return None
        template_text = template['address_template']
        if replace_aliases:
            self.replace_aliases(components)

        if minimal_only and not self.minimal_components(components):
            return None

        if template_replacements:
            self.apply_replacements(template, components)

        if tag_components:
            template_text = self.tag_template_separators(template_text)
            components = {k: u' '.join([u'{}/{}'.format(t.replace(' ', ''), k.replace(' ', '_'))
                                        for t, c in tokenize(v)])
                          for k, v in components.iteritems()}

        text = self.render_template(template_text, components, tagged=tag_components)

        text = self.post_replacements(template, text)
        return text
