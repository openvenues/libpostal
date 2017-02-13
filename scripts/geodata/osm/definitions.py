import os
import re
import six

from collections import defaultdict

from geodata.graph.topsort import topsort

this_dir = os.path.realpath(os.path.dirname(__file__))

DEFAULT_SCRIPT_PATH = os.path.join(this_dir, 'fetch_osm_address_data.sh')

valid_key_regex = re.compile('VALID_(.*?)_KEYS="(.*)"')
variable_regex = re.compile(r'\$VALID_(.*?)_KEYS(?=\b)')
kv_regex = re.compile('([^\s]*)=([^\s]*)')


class OSMDefinitions(object):
    ALL = '*'

    ADMIN_BORDER = 'admin_border'
    ADMIN_NODE = 'admin_node'
    AEROWAY = 'aeroway'
    AMENITY = 'amenity'
    BUILDING = 'building'
    HISTORIC = 'historic'
    LANDUSE = 'landuse'
    NATURAL = 'natural'
    LOCALITY = 'locality'
    NEIGHBORHOOD = 'neighborhood'
    EXTENDED_NEIGHBORHOOD = 'extended_neighborhood'
    OFFICE = 'office'
    PLACE = 'place'
    POPULATED_PLACE = 'populated_place'
    SHOP = 'shop'
    TOURISM = 'tourism'
    VENUE = 'venue'
    WATERWAY = 'waterway'

    def __init__(self, filename=DEFAULT_SCRIPT_PATH):
        script = open(filename).read()

        dependencies = defaultdict(list)

        definitions = {}

        matches = valid_key_regex.findall(script)

        match_text = {d.lower(): t for d, t in matches}

        for definition, text in matches:
            variables = variable_regex.findall(text)
            if not variables:
                dependencies[definition.lower()] = []
            for v in variables:
                dependencies[definition.lower()].append(v.lower())

        for definition in topsort(dependencies):
            definition = definition.lower()
            text = match_text[definition]
            variables = variable_regex.findall(text)
            for v in variables:
                v = v.lower()
                text = text.replace('$VALID_{}_KEYS'.format(v.upper()), match_text[v])

            kvs = defaultdict(set)

            for k, v in kv_regex.findall(text):
                if v != '':
                    kvs[k].add(v.lower())
                else:
                    kvs[k].add(self.ALL)

            definitions[definition] = kvs

        self.definitions = definitions

    def meets_definition(self, props, category):
        defs = self.definitions.get(category, {})
        if not defs:
            return False
        elif self.ALL in defs:
            return True
        for k, v in six.iteritems(props):
            if v.lower() in defs.get(k.lower(), set()):
                return True
        return False

osm_definitions = OSMDefinitions()
