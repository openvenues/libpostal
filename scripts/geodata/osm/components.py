import collections
import os
import six
import yaml

from copy import deepcopy

from geodata.address_formatting.formatter import AddressFormatter
from geodata.configs.utils import recursive_merge, DoesNotExist

from geodata.encoding import safe_encode

this_dir = os.path.realpath(os.path.dirname(__file__))

OSM_BOUNDARIES_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                  'resources', 'boundaries', 'osm')


class OSMAddressComponents(object):
    '''
    Keeps a map of OSM keys and values to the standard components
    of an address like city, state, etc. used for address formatting.
    When we reverse geocode a point, it will fall into a number of
    polygons, and we simply need to assign the names of said polygons
    to an address field.
    '''

    ADMIN_LEVEL = 'admin_level'

    # These keys override country-level
    global_keys_override = {
        'place': {
            'island': AddressFormatter.ISLAND,
            'islet': AddressFormatter.ISLAND,
            'municipality': AddressFormatter.CITY,
            'city': AddressFormatter.CITY,
            'town': AddressFormatter.CITY,
            'township': AddressFormatter.CITY,
            'village': AddressFormatter.CITY,
            'hamlet': AddressFormatter.CITY,
            'suburb': AddressFormatter.SUBURB,
            'quarter': AddressFormatter.SUBURB,
            'neighbourhood': AddressFormatter.SUBURB
        },
        'border_type': {
            'city': AddressFormatter.CITY
        }
    }

    # These keys are fallback in case we haven't added a country or there is no admin_level=
    global_keys = {
        'place': {
            'country': AddressFormatter.COUNTRY,
            'state': AddressFormatter.STATE,
            'region': AddressFormatter.STATE,
            'province': AddressFormatter.STATE,
            'county': AddressFormatter.STATE_DISTRICT,
        },
        'gnis:class': {
            'populated place': AddressFormatter.CITY,
        }
    }

    def __init__(self, boundaries_dir=OSM_BOUNDARIES_DIR):
        self.config = {}

        self.use_admin_center = {}

        for filename in os.listdir(boundaries_dir):
            if not filename.endswith('.yaml'):
                continue

            country_code = filename.rsplit('.yaml', 1)[0]
            data = yaml.load(open(os.path.join(boundaries_dir, filename)))

            for prop, values in six.iteritems(data):
                if not hasattr(values, 'items'):
                    # non-dict key
                    continue

                for k, v in values.iteritems():
                    if isinstance(v, six.string_types) and v not in AddressFormatter.address_formatter_fields:
                        raise ValueError(u'Invalid value in {} for prop={}, key={}: {}'.format(filename, prop, k, v))

                if prop == 'overrides':
                    self.use_admin_center.update({(r['type'], safe_encode(r['id'])): r.get('probability', 1.0) for r in values.get('use_admin_center', [])})

                    containing_overrides = values.get('contained_by', {})

                    if not containing_overrides:
                        continue

                    for id_type, vals in six.iteritems(containing_overrides):
                        for element_id in vals:

                            override_config = vals[element_id]

                            config = deepcopy(data)
                            config.pop('overrides')

                            recursive_merge(config, override_config)

                            vals[element_id] = config

            self.config[country_code] = data

    def component(self, country, prop, value):
        component = self.global_keys_override.get(prop, {}).get(value, None)
        if component is not None:
            return component

        component = self.config.get(country, {}).get(prop, {}).get(value, None)
        if component is not None:
            return component

        return self.global_keys.get(prop, {}).get(value, None)

    def component_from_properties(self, country, properties, containing=(), global_keys=True):
        country_config = self.config.get(country, {})

        config = country_config

        overrides = country_config.get('overrides')
        if overrides:
            id_overrides = overrides.get('id', {})
            element_type = properties.get('type')
            element_id = properties.get('id')

            override_value = id_overrides.get(element_type, {})
            element_id = six.binary_type(element_id or '')
            if element_id in override_value:
                return override_value[element_id]

            contained_by_overrides = overrides.get('contained_by')
            if contained_by_overrides and containing:
                # Note, containing should be passed in from smallest to largest
                for containing_type, containing_id in containing:
                    override_config = contained_by_overrides.get(containing_type, {}).get(six.binary_type(containing_id or ''), None)
                    if override_config:
                        config = override_config
                        break

        values = [(k.lower(), v.lower()) for k, v in six.iteritems(properties) if isinstance(v, six.string_types)]

        global_overrides_last = config.get('global_overrides_last', False)

        # place=city, place=suburb, etc. override per-country boundaries
        if not global_overrides_last:
            for k, v in values:
                containing_component = self.global_keys_override.get(k, {}).get(v, DoesNotExist)

                if containing_component is not DoesNotExist:
                    return containing_component

                if k != self.ADMIN_LEVEL and k in config:
                    containing_component = config.get(k, {}).get(v, DoesNotExist)
                    if containing_component is not DoesNotExist:
                        return containing_component

        # admin_level tags are mapped per country
        for k, v in values:
            containing_component = config.get(k, {}).get(v, DoesNotExist)

            if containing_component is not DoesNotExist:
                return containing_component

        # other place keys like place=state, etc. serve as a backup
        # when no admin_level tags are available
        for k, v in values:
            containing_component = self.global_keys.get(k, {}).get(v, DoesNotExist)

            if containing_component is not DoesNotExist:
                return containing_component

        if global_overrides_last:
            for k, v in values:
                containing_component = self.global_keys_override.get(k, {}).get(v, DoesNotExist)

                if containing_component is not DoesNotExist:
                    return containing_component

        return None

osm_address_components = OSMAddressComponents()
