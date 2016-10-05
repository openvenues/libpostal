'''
geodata.osm.extract
-------------------

Extracts nodes/ways/relations, their metadata and dependencies
from .osm XML files.
'''

import re
import six
import urllib
import HTMLParser

from collections import OrderedDict
from lxml import etree


from geodata.csv_utils import unicode_csv_reader
from geodata.text.normalize import normalize_string, NORMALIZE_STRING_DECOMPOSE, NORMALIZE_STRING_LATIN_ASCII
from geodata.encoding import safe_decode, safe_encode


WAY_OFFSET = 10 ** 15
RELATION_OFFSET = 2 * 10 ** 15

NODE = 'node'
WAY = 'way'
RELATION = 'relation'

ALL_OSM_TAGS = set([NODE, WAY, RELATION])
WAYS_RELATIONS = set([WAY, RELATION])

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

OSM_BASE_NAME_TAGS = (
    'tiger:name_base',
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
            attrs['type'] = item_type
            attrs['id'] = safe_encode(elem_id)

            top_level_attrs = set(attrs)
            deps = [] if dependencies else None

            for e in elem.getchildren():
                if e.tag == 'tag':
                    # Prevent user-defined lat/lon keys from overriding the lat/lon on the node
                    key = e.attrib['k']
                    if key not in top_level_attrs:
                        attrs[key] = e.attrib['v']
                elif dependencies and item_type == 'way' and e.tag == 'nd':
                    deps.append(long(e.attrib['ref']))
                elif dependencies and item_type == 'relation' and e.tag == 'member' and 'role' in e.attrib:
                    deps.append((long(e.attrib['ref']), e.attrib.get('type'), e.attrib['role']))

            key = elem_id if single_type else '{}:{}'.format(item_type, elem_id)
            yield key, attrs, deps

        if elem.tag in ALL_OSM_TAGS:
            elem.clear()
            while elem.getprevious() is not None:
                del elem.getparent()[0]


def osm_type_and_id(element_id):
    element_id = long(element_id)
    if element_id >= RELATION_OFFSET:
        id_type = RELATION
        element_id -= RELATION_OFFSET
    elif element_id >= WAY_OFFSET:
        id_type = WAY
        element_id -= WAY_OFFSET
    else:
        id_type = NODE

    return id_type, element_id

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


non_breaking_dash = six.u('[-\u058a\u05be\u1400\u1806\u2010-\u2013\u2212\u2e17\u2e1a\ufe32\ufe63\uff0d]')
simple_number = six.u('(?:{})?[0-9]+(?:\.[0-9]+)?').format(non_breaking_dash)
simple_number_regex = re.compile(simple_number, re.UNICODE)

non_breaking_dash_regex = re.compile(non_breaking_dash, re.UNICODE)
number_range_regex = re.compile(six.u('({}){}({})').format(simple_number, non_breaking_dash, simple_number), re.UNICODE)
letter_range_regex = re.compile(r'([^\W\d_]){}([^\W\d_])'.format(non_breaking_dash.encode('unicode-escape')), re.UNICODE)

number_split_regex = re.compile('[,;]')


def parse_osm_number_range(value, parse_letter_range=True, max_range=100):
    value = normalize_string(value, string_options=NORMALIZE_STRING_LATIN_ASCII | NORMALIZE_STRING_DECOMPOSE)
    numbers = []
    values = number_split_regex.split(value)
    for val in values:
        val = val.strip()
        match = number_range_regex.match(val)
        if match:
            start_num, end_num = match.groups()
            start_num_len = len(start_num)

            zfill = 0
            if start_num.startswith('0'):
                zfill = start_num_len

            try:
                start_num = int(start_num)
                end_num = int(end_num)

                if end_num > start_num:
                    if end_num - start_num > max_range:
                        end_num = start_num + max_range

                    for i in xrange(start_num, end_num + 1):
                        numbers.append(safe_decode(i).zfill(zfill))
                else:
                    numbers.append(val.strip().zfill(zfill))
                    continue
            except (TypeError, ValueError):
                numbers.append(safe_decode(val).strip().zfill(zfill))
                continue

        else:
            letter_match = letter_range_regex.match(val)
            if letter_match and parse_letter_range:
                start_num, end_num = letter_match.groups()
                start_num = ord(start_num)
                end_num = ord(end_num)
                if end_num > start_num:
                    if end_num - start_num > max_range:
                        end_num = start_num + max_range
                    for i in xrange(start_num, end_num + 1):
                        numbers.append(six.unichr(i))
                else:
                    numbers.extend([six.unichr(start_num), six.unichr(end_num)])
                    continue
            else:
                numbers.append(safe_decode(val.strip()))
    return numbers
