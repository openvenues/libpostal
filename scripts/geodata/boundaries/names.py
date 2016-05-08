import os
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

        self.include_probabilities = {}
        self.omit_conditions = defaultdict(set)

        for props in nested_get(config, ('names', 'omissions'), default=[]):
            object_type = props['type']
            object_id = safe_encode(props['id'])
            include_probability = props.get('include_probability')

            if include_probability is not None:
                self.include_probabilities[(object_type, object_id)] = float(include_probability)

            for condition in nested_get(props, ('omit', 'conditions'), default=[]):
                condition_object_id = safe_encode(condition['id'])
                condition_object_type = condition['type']
                self.omit_conditions[(object_type, object_id)].add((condition_object_type, condition_object_id))

    def name_key(self, props):
        object_type = props.get('type')
        object_id = safe_encode(props.get('id', ''))

        if (object_type, object_id) in self.exceptions:
            values, probs = self.exceptions[(object_type, object_id)]
            return weighted_choice(values, probs)

        return weighted_choice(self.name_keys, self.name_key_probs)

    def remove_excluded_components(self, components):
        all_ids = set()
        for component in components:
            object_type = component.get('type')
            object_id = safe_encode(component.get('id', ''))
            all_ids.add((object_type, object_id))

        for object_type, object_id in list(all_ids):
            if (object_type, object_id) in self.omit_conditions:
                conditions = self.omit_conditions[(object_type, object_id)]
                if all_ids & conditions:
                    all_ids.remove((object_type, object_id))

            if (object_type, object_id) in self.include_probabilities and random.random() > self.include_probabilities[(object_type, object_id)]:
                all_ids.remove((object_type, object_id))

        if len(all_ids) == len(components):
            return components

        return [c for c in components if (c.get('type'), safe_encode(c.get('id', ''))) in all_ids]

boundary_names = BoundaryNames()
