import os
import random
import re
import six
import yaml

from collections import defaultdict

from geodata.configs.utils import nested_get, DoesNotExist, alternative_probabilities
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

    def name(self, country, name):
        all_replacements = self.country_regex_replacements.get(country, []) + self.country_regex_replacements.get(None, [])
        if not all_replacements:
            return name

        for regex, group, prob in all_replacements:
            match = regex.match(name)
            if match and random.random() < prob:
                name = match.group(group)
        return name


boundary_names = BoundaryNames()
