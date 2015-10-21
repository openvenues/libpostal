'''
geodata.osm.extract
-------------------

Extracts nodes/ways/relations, their metadata and dependencies
from .osm XML files.
'''

from collections import OrderedDict
from lxml import etree


WAY_OFFSET = 10 ** 15
RELATION_OFFSET = 2 * 10 ** 15

ALL_OSM_TAGS = set(['node', 'way', 'relation'])
WAYS_RELATIONS = set(['way', 'relation'])


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
