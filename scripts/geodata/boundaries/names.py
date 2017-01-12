import os
import random
import re
import six
import yaml

from collections import defaultdict

from geodata.configs.utils import nested_get, DoesNotExist, alternative_probabilities
from geodata.encoding import safe_decode
from geodata.math.floats import isclose
from geodata.math.sampling import cdf, weighted_choice

from geodata.encoding import safe_encode

this_dir = os.path.realpath(os.path.dirname(__file__))

BOUNDARY_NAMES_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                  'resources', 'boundaries', 'names')

BOUNDARY_NAMES_CONFIG = os.path.join(BOUNDARY_NAMES_DIR, 'global.yaml')


class BoundaryNames(object):
    DEFAULT_NAME_KEY = 'name'

    def __init__(self, config_file=BOUNDARY_NAMES_CONFIG):
        config = yaml.load(open(config_file))

        default_names = nested_get(config, ('names', 'keys'))
        name_keys, probs = alternative_probabilities(default_names)

        self.name_keys = name_keys
        self.name_key_probs = cdf(probs)

        self.component_name_keys = {}

        for component, component_config in six.iteritems(nested_get(config, ('names', 'components'), default={})):
            component_names = component_config.get('keys')
            component_name_keys, component_probs = alternative_probabilities(component_names)
            self.component_name_keys[component] = (component_name_keys, cdf(component_probs))

        self.country_regex_replacements = defaultdict(list)
        for props in nested_get(config, ('names', 'regex_replacements',), default=[]):
            country = props.get('country')
            re_flags = re.I | re.UNICODE
            if not props.get('case_insensitive', True):
                re.flags ^= re.I

            pattern = re.compile(props['pattern'], re_flags)
            replace_group = props['replace_with_group']
            replace_probability = props['replace_probability']
            self.country_regex_replacements[country].append((pattern, replace_group, replace_probability))

        self.country_regex_replacements = dict(self.country_regex_replacements)

        self.prefixes = {}
        self.prefix_regexes = {}
        self.suffixes = {}
        self.suffix_regexes = {}

        for language, components in six.iteritems(nested_get(config, ('names', 'prefixes', 'language'), default={}) ):
            for component, affixes in six.iteritems(components):
                affix_values, probs = alternative_probabilities(affixes)

                for val in affix_values:
                    if 'prefix' not in val:
                        raise AssertionError(six.u('Invalid prefix value for (language={}, component={}): {} ').format(language, component, val))

                prefix_regex = six.u('|').join([six.u('(?:{} )').format(self._string_as_regex(v['prefix'])) if v.get('whitespace') else self._string_as_regex(v['prefix']) for v in affix_values])
                self.prefix_regexes[(language, component)] = re.compile(six.u('^{}').format(prefix_regex), re.I | re.U)

                if not isclose(sum(probs), 1.0):
                    affix_values.append(None)
                    probs.append(1.0 - sum(probs))
                affix_probs_cdf = cdf(probs)
                self.prefixes[(language, component)] = affix_values, affix_probs_cdf

        for language, components in six.iteritems(nested_get(config, ('names', 'suffixes', 'language'), default={}) ):
            for component, affixes in six.iteritems(components):
                affix_values, probs = alternative_probabilities(affixes)

                for val in affix_values:
                    if 'suffix' not in val:
                        raise AssertionError(six.u('Invalid suffix value for (language={}, component={}): {} ').format(language, component, val))

                suffix_regex = six.u('|').join([six.u('(?: {})').format(self._string_as_regex(v['suffix'])) if v.get('whitespace') else self._string_as_regex(v['suffix']) for v in affix_values])
                self.suffix_regexes[(language, component)] = re.compile(six.u('{}$').format(suffix_regex), re.I | re.U)

                if not isclose(sum(probs), 1.0):
                    affix_values.append(None)
                    probs.append(1.0 - sum(probs))
                affix_probs_cdf = cdf(probs)
                self.suffixes[(language, component)] = affix_values, affix_probs_cdf

        self.exceptions = {}

        for props in nested_get(config, ('names', 'exceptions'), default=[]):
            object_type = props['type']
            object_id = safe_encode(props['id'])
            keys = [props['default']]
            probs = [props['probability']]
            for alt in props.get('alternatives', []):
                keys.append(alt['alternative'])
                probs.append(alt['probability'])

            probs = cdf(probs)
            self.exceptions[(object_type, object_id)] = (keys, probs)

    def _string_as_regex(self, s):
        return safe_decode(s).replace(six.u('.'), six.u('\\.'))

    def valid_name(self, object_type, object_id, name):
        exceptions, probs  = self.exceptions.get((object_type, object_id), ((), ()))
        return not exceptions or name in exceptions

    def name_key_dist(self, props, component):
        object_type = props.get('type')
        object_id = safe_encode(props.get('id', ''))

        if (object_type, object_id) in self.exceptions:
            values, probs = self.exceptions[(object_type, object_id)]
            return values, probs

        name_keys, probs = self.component_name_keys.get(component, (self.name_keys, self.name_key_probs))
        return name_keys, probs

    def name_key(self, props, component):
        name_keys, probs = self.name_key_dist(props, component)
        return weighted_choice(name_keys, probs)

    def name(self, country, language, component, name):
        all_replacements = self.country_regex_replacements.get(country, []) + self.country_regex_replacements.get(None, [])

        prefixes, prefix_probs = self.prefixes.get((language, component), (None, None))
        suffixes, suffix_probs = self.suffixes.get((language, component), (None, None))

        if not all_replacements and not prefixes and not suffixes:
            return name

        for regex, group, prob in all_replacements:
            match = regex.match(name)
            if match and random.random() < prob:
                name = match.group(group)

        for affixes, affix_probs, regexes, key, direction in ((prefixes, prefix_probs, self.prefix_regexes, 'prefix', 0),
                                                              (suffixes, suffix_probs, self.suffix_regexes, 'suffix', 1)):
            if affixes is not None:
                regex = regexes[language, component]
                if regex.match(name):
                    continue

                affix = weighted_choice(affixes, affix_probs)

                if affix is not None:
                    whitespace = affix.get('whitespace', True)
                    space_val = six.u(' ') if whitespace else six.u('')
                    affix = affix[key]
                    if direction == 0:
                        return six.u('{}{}{}').format(affix, space_val, safe_decode(name))
                    else:
                        return six.u('{}{}{}').format(safe_decode(name), space_val, affix)

        return name


boundary_names = BoundaryNames()
