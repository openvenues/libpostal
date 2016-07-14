import os
import six
import yaml

from geodata.address_formatting.formatter import AddressFormatter

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

    # These keys are country-independent
    global_keys = {
        'place': {
            'country': AddressFormatter.COUNTRY,
            'state': AddressFormatter.STATE,
            'region': AddressFormatter.STATE,
            'province': AddressFormatter.STATE,
            'county': AddressFormatter.STATE_DISTRICT,
            'island': AddressFormatter.ISLAND,
            'islet': AddressFormatter.ISLAND,
            'municipality': AddressFormatter.CITY,
            'city': AddressFormatter.CITY,
            'town': AddressFormatter.CITY,
            'township': AddressFormatter.CITY,
            'village': AddressFormatter.CITY,
            'hamlet': AddressFormatter.CITY,
            'borough': AddressFormatter.CITY_DISTRICT,
            'suburb': AddressFormatter.SUBURB,
            'quarter': AddressFormatter.SUBURB,
            'neighbourhood': AddressFormatter.SUBURB
        }
    }

    def __init__(self, boundaries_dir=OSM_BOUNDARIES_DIR):
        self.config = {}

        for filename in os.listdir(boundaries_dir):
            if not filename.endswith('.yaml'):
                continue

            country_code = filename.rsplit('.yaml', 1)[0]
            data = yaml.load(open(os.path.join(boundaries_dir, filename)))
            for prop, values in six.iteritems(data):
                for k, v in values.iteritems():
                    if v not in AddressFormatter.address_formatter_fields:
                        raise ValueError(u'Invalid value in {} for prop={}, key={}: {}'.format(filename, prop, k, v))
            self.config[country_code] = data

    def get_component(self, country, prop, value):
        if prop in self.global_keys:
            props = self.global_keys[prop]
        else:
            props = self.config.get(country, {}).get(prop, {})
        return props.get(value, None)

    def get_first_component(self, country, properties):
        for k, v in six.iteritems(props):
            containing_component = self.get_component(country, k, v)
            break
        else:
            containing_component = None
        return containing_component


osm_address_components = OSMAddressComponents()
