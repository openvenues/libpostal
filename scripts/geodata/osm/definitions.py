import os
import re
import six

from collections import defaultdict

this_dir = os.path.realpath(os.path.dirname(__file__))

DEFAULT_SCRIPT_PATH = os.path.join(this_dir, 'fetch_osm_address_data.sh')

valid_key_regex = re.compile('VALID_(.*?)_KEYS="(.*)"')
kv_regex = re.compile('([^\s]*)=([^\s]*)')


class OSMDefinitions(object):
    ALL = '*'

    ADMIN_BORDER = 'admin_border'
    AEROWAY = 'aeroway'
    AMENITY = 'amenity'
    BUILDING = 'building'
    HISTORIC = 'historic'
    LANDUSE = 'landuse'
    NATURAL = 'natural'
    NEIGHBORHOOD = 'neighborhood'
    OFFICE = 'office'
    PLACE = 'place'
    SHOP = 'shop'
    TOURISM = 'tourism'
    WATERWAY = 'waterway'

    def __init__(self, filename=DEFAULT_SCRIPT_PATH):
        script = open(filename).read()

        definitions = {}

        for definition, text in valid_key_regex.findall(script):
            definition = definition.lower()

            kvs = defaultdict(set)

            for k, v in kv_regex.findall(text):
                if v != '':
                    kvs[k].add(v)
                else:
                    kvs[k].add(self.ALL)

            definitions[definition] = kvs

        self.definitions = definitions

    def meets_definition(self, props, category):
        defs = self.definitions.get(category, {})
        if not defs:
            return False
        for k, v in six.iteritems(props):
            if v in defs.get(k, set()):
                return True
        return False

osm_definitions = OSMDefinitions()
