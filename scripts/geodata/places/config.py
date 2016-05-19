import copy
import os
import random
import six
import yaml

from collections import Mapping

from geodata.address_expansions.address_dictionaries import address_phrase_dictionaries
from geodata.configs.utils import nested_get, DoesNotExist, recursive_merge
from geodata.math.sampling import cdf, check_probability_distribution

from geodata.encoding import safe_encode

this_dir = os.path.realpath(os.path.dirname(__file__))

PLACE_CONFIG_FILE = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                 'resources', 'places', 'countries', 'global.yaml')


class PlaceConfig(object):
    def __init__(self, config_file=PLACE_CONFIG_FILE):
        self.cache = {}
        place_config = yaml.load(open(config_file))

        self.global_config = place_config['global']
        self.country_configs = {}

        countries = place_config.pop('countries', {})

        for k, v in six.iteritems(countries):
            country_config = countries[k]
            global_config_copy = copy.deepcopy(self.global_config)
            self.country_configs[k] = recursive_merge(global_config_copy, country_config)

    def get_property(self, key, country=None, default=None):
        if isinstance(key, six.string_types):
            key = key.split('.')

        config = self.global_config

        if country:
            country_config = self.country_configs.get(country, {})
            if country_config:
                config = country_config

        return nested_get(config, key, default=default)

    def include_component(self, component, containing_ids, country=None):
        containing = self.get_property(('components', component, 'containing'), country=country, default=None)

        if containing is not None:
            for c in containing:
                if (c['type'], safe_encode(c['id'])) in containing_ids:
                    return random.random() < c['probability']

        probability = self.get_property(('components', component, 'probability'), country=country, default=0.0)

        return random.random() < probability

    def drop_components(self, components, boundaries=(), country=None):
        containing_ids = set()
        for boundary in boundaries:
            object_type = boundary.get('type')
            object_id = safe_encode(boundary.get('id', ''))
            containing_ids.add((object_type, object_id))

        return {c: v for c, v in six.iteritems(components) if self.include_component(c, containing_ids, country=country)}

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

place_config = PlaceConfig()
