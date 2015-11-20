'''
geodata.osm.extract
-------------------

Extracts nodes/ways/relations, their metadata and dependencies
from .osm XML files.
'''

import os
import re
import sys
import urllib
import ujson as json
import HTMLParser

from collections import OrderedDict
from lxml import etree

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.address_formatting.formatter import AddressFormatter
from geodata.csv_utils import unicode_csv_reader


OSM_BOUNDARIES_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                  'resources', 'boundaries', 'osm')

from geodata.encoding import safe_decode

WAY_OFFSET = 10 ** 15
RELATION_OFFSET = 2 * 10 ** 15

ALL_OSM_TAGS = set(['node', 'way', 'relation'])
WAYS_RELATIONS = set(['way', 'relation'])

OSM_NAME_TAGS = (
    'name',
    'alt_name',
    'int_name',
    'nat_name',
    'reg_name',
    'loc_name',
    'official_name',
    'commonname',
    'common_name',
    'place_name',
    'short_name',
)


def parse_osm(filename, allowed_types=ALL_OSM_TAGS, dependencies=False):
    '''
    Parse a file in .osm format iteratively, generating tuples like:
    ('node:1', OrderedDict([('lat', '12.34'), ('lon', '23.45')])),
    ('node:2', OrderedDict([('lat', '12.34'), ('lon', '23.45')])),
    ('node:3', OrderedDict([('lat', '12.34'), ('lon', '23.45')])),
    ('node:4', OrderedDict([('lat', '12.34'), ('lon', '23.45')])),
    ('way:4444', OrderedDict([('name', 'Main Street')]), [1,2,3,4])
    '''
    f = open(filename)
    parser = etree.iterparse(f)

    single_type = len(allowed_types) == 1

    for (_, elem) in parser:
        elem_id = long(elem.attrib.pop('id', 0))
        item_type = elem.tag
        if elem_id >= WAY_OFFSET and elem_id < RELATION_OFFSET:
            elem_id -= WAY_OFFSET
            item_type = 'way'
        elif elem_id >= RELATION_OFFSET:
            elem_id -= RELATION_OFFSET
            item_type = 'relation'

        if item_type in allowed_types:
            attrs = OrderedDict(elem.attrib)
            deps = [] if dependencies else None

            for e in elem.getchildren():
                if e.tag == 'tag':
                    attrs[e.attrib['k']] = e.attrib['v']
                elif dependencies and item_type == 'way' and e.tag == 'nd':
                    deps.append(long(e.attrib['ref']))
                elif dependencies and item_type == 'relation' and e.tag == 'member' and \
                        e.attrib.get('type') in ('way', 'relation') and \
                        e.attrib.get('role') in ('inner', 'outer'):
                    deps.append((long(e.attrib['ref']), e.attrib.get('role')))

            key = elem_id if single_type else '{}:{}'.format(item_type, elem_id)
            yield key, attrs, deps

        if elem.tag in ALL_OSM_TAGS:
            elem.clear()
            while elem.getprevious() is not None:
                del elem.getparent()[0]

apposition_regex = re.compile('(.*[^\s])[\s]*\([\s]*(.*[^\s])[\s]*\)$', re.I)

html_parser = HTMLParser.HTMLParser()


def normalize_wikipedia_title(title):
    match = apposition_regex.match(title)
    if match:
        title = match.group(1)

    title = safe_decode(title)
    title = html_parser.unescape(title)
    title = urllib.unquote_plus(title)

    return title.replace(u'_', u' ').strip()


def osm_wikipedia_title_and_language(key, value):
    language = None
    if u':' in key:
        key, language = key.rsplit(u':', 1)

    if u':' in value:
        possible_language = value.split(u':', 1)[0]
        if len(possible_language) == 2 and language is None:
            language = possible_language
            value = value.rsplit(u':', 1)[-1]

    return normalize_wikipedia_title(value), language


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
            'municipality': AddressFormatter.CITY,
            'city': AddressFormatter.CITY,
            'town': AddressFormatter.CITY,
            'village': AddressFormatter.CITY,
            'hamlet': AddressFormatter.CITY,
            'borough': AddressFormatter.CITY_DISTRICT,
            'suburb': AddressFormatter.SUBURB,
            'quarter': AddressFormatter.SUBURB,
            'neighbourhood': AddressFormatter.SUBURB
        }
    }

    def __init__(self):
        self.config = {}

    def configure(self, boundaries_dir=OSM_BOUNDARIES_DIR):
        for filename in os.listdir(boundaries_dir):
            if not filename.endswith('.json'):
                continue

            country_code = filename.rsplit('.json', 1)[0]
            data = json.load(open(os.path.join(boundaries_dir, filename)))
            for prop, values in data.iteritems():
                for k, v in values.iteritems():
                    if v not in AddressFormatter.address_formatter_fields:
                        raise ValueError(u'Invalid value in {} for prop={}, key={}: {}'.format(filename, prop, k, v))
            self.config[country_code] = data

    def get_component(self, country, prop, value):
        props = self.config.get(country, {}).get(prop, {})
        if not props and prop in self.global_keys:
            props = self.global_keys[prop]
        return props.get(value, None)

osm_address_components = OSMAddressComponents()
