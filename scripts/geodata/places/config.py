import copy
import operator
import os
import random
import six
import yaml

from collections import defaultdict

from geodata.addresses.dependencies import ComponentDependencies
from geodata.address_expansions.address_dictionaries import address_phrase_dictionaries
from geodata.address_formatting.formatter import AddressFormatter
from geodata.configs.utils import nested_get, recursive_merge
from geodata.math.sampling import cdf, weighted_choice

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
        AddressFormatter.COUNTRY_REGION,
        AddressFormatter.COUNTRY,
        AddressFormatter.WORLD_REGION,
    }

    numeric_ops = {'lte': operator.le,
                   'gt': operator.gt,
                   'lt': operator.lt,
                   'gte': operator.ge,
                   }

    def __init__(self, config_file=PLACE_CONFIG_FILE):
        self.cache = {}
        place_config = yaml.load(open(config_file))

        self.global_config = place_config['global']
        self.country_configs = {}

        self.cdf_cache = {}

        countries = place_config.pop('countries', {})

        for k, v in six.iteritems(countries):
            country_config = countries[k]
            global_config_copy = copy.deepcopy(self.global_config)
            self.country_configs[k] = recursive_merge(global_config_copy, country_config)

        self.country_configs[None] = self.global_config

        self.setup_component_dependencies()

    def setup_component_dependencies(self):
        self.component_dependencies = {}

        for country, conf in six.iteritems(self.country_configs):
            graph = {k: c['dependencies'] for k, c in six.iteritems(conf['components']) if 'dependencies' in c}
            graph.update({c: [] for c in self.ADMIN_COMPONENTS if c not in graph})

            self.component_dependencies[country] = ComponentDependencies(graph)

    def get_property(self, key, country=None, default=None):
        if isinstance(key, six.string_types):
            key = key.split('.')

        config = self.global_config

        if country:
            country_config = self.country_configs.get(country.lower(), {})
            if country_config:
                config = country_config

        return nested_get(config, key, default=default)

    def include_by_population_exceptions(self, population_exceptions, population):
        if population_exceptions:
            try:
                population = int(population)
            except (TypeError, ValueError):
                population = 0

            for exc in population_exceptions:
                support = 0

                for k in exc:
                    op = self.numeric_ops.get(k)
                    if not op:
                        continue
                    res = op(population, exc[k])
                    if not res:
                        support = 0
                        break

                    support += 1

                if support > 0:
                    probability = exc.get('probability', 0.0)
                    if random.random() < probability:
                        return True
        return False

    def include_component_simple(self, component, containing_ids, country=None):
        containing = self.get_property(('components', component, 'containing'), country=country, default=None)

        if containing is not None:
            for c in containing:
                if (c['type'], safe_encode(c['id'])) in containing_ids:
                    return random.random() < c['probability']

        probability = self.get_property(('components', component, 'probability'), country=country, default=0.0)

        return random.random() < probability

    def include_component(self, component, containing_ids, country=None, population=None, check_population=True, unambiguous_city=False):
        if check_population and not unambiguous_city:
            population_exceptions = self.get_property(('components', component, 'population'), country=country, default=None)
            if population_exceptions and self.include_by_population_exceptions(population_exceptions, population=population or 0):
                return True
        return self.include_component_simple(component, containing_ids, country=country)

    def drop_invalid_components(self, address_components, country, original_bitset=None):
        if not address_components:
            return
        component_bitset = ComponentDependencies.component_bitset(address_components)

        deps = self.component_dependencies.get(country, self.component_dependencies[None])
        dep_order = deps.dependency_order

        for c in dep_order:
            if c not in address_components:
                continue
            if c in deps and not component_bitset & deps[c] and (original_bitset is None or original_bitset & deps[c]):
                address_components.pop(c)
                component_bitset ^= ComponentDependencies.component_bit_values[c]

    def city_replacements(self, country):
        return set(self.get_property(('city_replacements', ), country=country))

    def dropout_components(self, components, boundaries=(), country=None, population=None, unambiguous_city=False):
        containing_ids = set()

        for boundary in boundaries:
            object_type = boundary.get('type')
            object_id = safe_encode(boundary.get('id', ''))
            if not (object_type and object_id):
                continue
            containing_ids.add((object_type, object_id))

        original_bitset = ComponentDependencies.component_bitset(components)

        names = defaultdict(list)
        admin_components = [c for c in components if c in self.ADMIN_COMPONENTS]
        for c in admin_components:
            names[components[c]].append(c)

        same_name = set()
        for c, v in six.iteritems(names):
            if len(v) > 1:
                same_name |= set(v)

        new_components = components.copy()

        city_replacements = set()
        if AddressFormatter.CITY not in components:
            city_replacements = self.city_replacements(country)

        for component in admin_components:
            include = self.include_component(component, containing_ids, country=country, population=population, unambiguous_city=unambiguous_city)

            if not include and component not in city_replacements:
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

        for component in self.ADMIN_COMPONENTS:
            value = self.get_property(('components', component, 'value'), country=country, default=None)

            if not value:
                values, probs = self.cdf_cache.get((country, component), (None, None))
                if values is None:
                    values = self.get_property(('components', component, 'values'), country=country, default=None)
                    if values is not None:
                        values, probs = zip(*[(v['value'], float(v['probability'])) for v in values])
                        probs = cdf(probs)
                        self.cdf_cache[(country, component)] = (values, probs)

                if values is not None:
                    value = weighted_choice(values, probs)

            if value is not None and component not in components and self.include_component(component, containing_ids, country=country, population=population, unambiguous_city=unambiguous_city):
                new_components[component] = value

        self.drop_invalid_components(new_components, country, original_bitset=original_bitset)

        return new_components


place_config = PlaceConfig()
