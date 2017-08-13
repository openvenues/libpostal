import os
import random
import re
import six
import yaml

from collections import defaultdict

from geodata.configs.utils import nested_get, DoesNotExist, alternative_probabilities, RESOURCES_DIR
from geodata.encoding import safe_decode
from geodata.math.floats import isclose
from geodata.math.sampling import cdf, weighted_choice

from geodata.encoding import safe_encode

this_dir = os.path.realpath(os.path.dirname(__file__))

BOUNDARY_NAMES_DIR = os.path.join(RESOURCES_DIR, 'boundaries', 'names')

BOUNDARY_NAMES_CONFIG = os.path.join(BOUNDARY_NAMES_DIR, 'global.yaml')


class BoundaryNames(object):
    DEFAULT_NAME_KEY = 'name'

    def __init__(self, config_file=BOUNDARY_NAMES_CONFIG):
        config = yaml.load(open(config_file))

        default_names = nested_get(config, ('names', 'keys'))
        name_keys, probs = alternative_probabilities(default_names)

        self.name_keys = name_keys
        self.name_key_probs = cdf(probs)

        self.fallbacks = {}

        if 'fallback' in default_names:
            self.fallbacks[default_names['default']] = default_names['fallback']

        for alt in default_names.get('alternatives', []):
            if 'fallback' in alt:
                self.fallbacks[alt['alternative']] = alt['fallback']

        self.component_name_keys = {}
        self.component_name_key_fallbacks = defaultdict(dict)

        for component, component_config in six.iteritems(nested_get(config, ('names', 'components'), default={})):
            component_names = component_config.get('keys')
            component_name_keys, component_probs = alternative_probabilities(component_names)
            self.component_name_keys[component] = (component_name_keys, cdf(component_probs))

            if 'fallback' in component_names:
                self.component_name_key_fallbacks[component][component_names['default']] = component_names['fallback']

            for alt in default_names.get('alternatives', []):
                if 'fallback' in alt:
                    self.component_name_key_fallbacks[component][alt['alternative']] = alt['fallback']

        self.component_name_key_fallbacks = dict(self.component_name_key_fallbacks)

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

        self.affixes = {}
        self.prefix_regexes = {}
        self.suffix_regexes = {}

        self.conditional_affixes = defaultdict(dict)

        for language, components in six.iteritems(nested_get(config, ('names', 'affixes', 'language'), default={}) ):
            for component, affixes in six.iteritems(components):
                if component != 'conditionals':
                    affix_values, probs = alternative_probabilities(affixes)
                    for val in affix_values:
                        if 'affix' not in val:
                            raise AssertionError(six.u('Invalid prefix value for (language={}, component={}): {} ').format(language, component, val))
                    direction = affixes['direction']
                    is_prefix = direction == 'left'

                    if is_prefix:
                        prefix_regex = u'|'.join([u'(?:{}{})'.format(self._string_as_regex(v['affix']), u' ' if v.get('whitespace', True) else u'') for v in affix_values])
                        self.prefix_regexes[(language, component)] = re.compile(six.u('^{}').format(prefix_regex), re.I | re.U)
                    else:
                        suffix_regex = u'|'.join([u'(?:{}{})'.format(u' ' if v.get('whitespace', True) else u'', self._string_as_regex(v['affix'])) for v in affix_values])
                        self.suffix_regexes[(language, component)] = re.compile(u'{}$'.format(suffix_regex), re.I | re.U)

                    if not isclose(sum(probs), 1.0):
                        affix_values.append(None)
                        probs.append(1.0 - sum(probs))
                    affix_probs_cdf = cdf(probs)

                    self.affixes[(language, component)] = affix_values, affix_probs_cdf, direction
                else:
                    for cond in affixes:
                        key = cond['key']
                        val = cond['value']

                        direction = cond['direction']

                        is_prefix = direction == 'left'

                        affix_values, probs = alternative_probabilities(cond)

                        if is_prefix:
                            prefix_regex = u'|'.join([u'(?:{}{})'.format(self._string_as_regex(v['affix']), u' ' if v.get('whitespace', True) else u'') for v in affix_values])
                            self.prefix_regexes[(key, val)] = re.compile(u'^{}'.format(prefix_regex), re.I | re.U)
                        else:
                            suffix_regex = u'|'.join([u'(?:{}{})'.format(u' ' if v.get('whitespace', True) else u'', self._string_as_regex(v['affix'])) for v in affix_values])
                            self.suffix_regexes[(key, val)] = re.compile(u'{}$'.format(suffix_regex), re.I | re.U)

                        if not isclose(sum(probs), 1.0):
                            affix_values.append(None)
                            probs.append(1.0 - sum(probs))

                        affix_probs_cdf = cdf(probs)
                        self.conditional_affixes[key][val] = affix_values, affix_probs_cdf, direction

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
        else:
            values, probs = self.component_name_keys.get(component, (self.name_keys, self.name_key_probs))

        return values, probs

    def name_key(self, props, component):
        name_keys, probs = self.name_key_dist(props, component)

        key = weighted_choice(name_keys, probs)

        if key not in props:
            fallback = self.component_name_key_fallbacks.get(component, {}).get(key, self.fallbacks.get(key))
            if fallback:
                return fallback
        return key

    def name(self, country, language, component, name, props):
        all_replacements = self.country_regex_replacements.get(country, []) + self.country_regex_replacements.get(None, [])

        affixes = affix_probs = direction = regexes = None
        is_prefix = False

        for key, value in six.iteritems(props):
            if not isinstance(value, six.string_types):
                continue

            affixes, affix_probs, direction = self.conditional_affixes.get(key, {}).get(value, (None, None, None))
            is_prefix = direction == 'left'

            regexes = self.prefix_regexes if is_prefix else self.suffix_regexes

            pattern = regexes.get((key, value), None)
            if pattern and pattern.search(name):
                return name

            if affixes is not None:
                break

        if affixes is None:
            affixes, affix_probs, direction = self.affixes.get((language, component), (None, None, None))
            is_prefix = direction == 'left'

            regexes = self.prefix_regexes if is_prefix else self.suffix_regexes

            pattern = regexes.get((language, component), None)
            if pattern and pattern.search(name):
                return name

        if not all_replacements and not affixes:
            return name

        for regex, group, prob in all_replacements:
            match = regex.search(name)
            if match and random.random() < prob:
                name = match.group(group)

        if affixes is not None:
            affix = weighted_choice(affixes, affix_probs)

            if affix is not None:
                whitespace = affix.get('whitespace', True)
                space_val = u' ' if whitespace else u''
                affix = affix['affix']
                if is_prefix:
                    return u'{}{}{}'.format(affix, space_val, safe_decode(name))
                else:
                    return u'{}{}{}'.format(safe_decode(name), space_val, affix)

        return name


boundary_names = BoundaryNames()
