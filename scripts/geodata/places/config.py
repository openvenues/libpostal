import copy
import os
import random
import six
import yaml

from collections import defaultdict

from geodata.address_expansions.address_dictionaries import address_phrase_dictionaries
from geodata.address_formatting.formatter import AddressFormatter
from geodata.configs.utils import nested_get, DoesNotExist, recursive_merge
from geodata.math.sampling import cdf, check_probability_distribution

from geodata.encoding import safe_encode

this_dir = os.path.realpath(os.path.dirname(__file__))

PLACE_CONFIG_FILE = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                 'resources', 'places', 'countries', 'global.yaml')


class PlaceConfig(object):
    ADMIN_COMPONENTS = {
        AddressFormatter.SUBURB,
        AddressFormatter.CITY_DISTRICT,
        AddressFormatter.CITY,
        AddressFormatter.ISLAND,
        AddressFormatter.STATE_DISTRICT,
        AddressFormatter.STATE,
        AddressFormatter.COUNTRY,
    }

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

    def dropout_components(self, components, boundaries=(), country=None):
        containing_ids = set()

        for boundary in boundaries:
            object_type = boundary.get('type')
            object_id = safe_encode(boundary.get('id', ''))
            if not (object_type and object_id):
                continue
            containing_ids.add((object_type, object_id))

        names = defaultdict(list)
        admin_components = [c for c in components if c in self.ADMIN_COMPONENTS]
        for c in admin_components:
            names[components[c]].append(c)

        same_name = set()
        for c, v in six.iteritems(names):
            if len(v) > 1:
                same_name |= set(v)

        new_components = components.copy()

        for component in admin_components:
            include = self.include_component(component, containing_ids, country=country)

            if not include:
                # Note: this check is for cities that have the same name as their admin
                # areas e.g. Luxembourg, Luxembourg. In cases like this, if we were to drop
                # city, we don't want to include country on its own. This should help the parser
                # default to the city in ambiguous cases where only one component is specified.
                if not (component == AddressFormatter.CITY and component in same_name):
                    new_components.pop(component, None)
                else:
                    value = components[component]
                    for c in names[value]:
                        new_components.pop(c, None)
        return new_components


place_config = PlaceConfig()
