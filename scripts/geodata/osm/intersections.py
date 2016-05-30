import array
import logging
import numpy
import os
import six
import ujson as json

from bisect import bisect_left
from leveldb import LevelDB
from itertools import izip, groupby

from geodata.coordinates.conversion import latlon_to_decimal
from geodata.file_utils import ensure_dir
from geodata.osm.extract import *
from geodata.encoding import safe_decode, safe_encode


class OSMIntersectionReader(object):
    def __init__(self, filename, db_dir):
        self.filename = filename

        self.node_ids = array.array('l')
        self.node_coordinates = array.array('d')

        self.logger = logging.getLogger('osm.intersections')

        # Store these in a LevelDB
        ensure_dir(db_dir)
        ways_dir = os.path.join(db_dir, 'ways')
        ensure_dir(ways_dir)
        self.way_props = LevelDB(ways_dir)
        # These form a graph and should always have the same length
        self.intersection_edges_nodes = array.array('l')
        self.intersection_edges_ways = array.array('l')

    def binary_search(self, a, x):
        '''Locate the leftmost value exactly equal to x'''
        i = bisect_left(a, x)
        if i != len(a) and a[i] == x:
            return i
        return None

    def intersections(self):
        '''
        Generator which yields tuples like:

        (node_id, lat, lon, {way_id: way_props})
        '''
        i = 0

        node_ids = array.array('l')
        node_counts = array.array('i')

        for element_id, props, deps in parse_osm(self.filename, dependencies=True):
            props = {safe_decode(k): safe_decode(v) for k, v in six.iteritems(props)}
            if element_id.startswith('node'):
                node_id = long(element_id.split(':')[-1])
                node_ids.append(node_id)
                node_counts.append(0)
            elif element_id.startswith('way'):
                # Don't care about the ordering of the nodes, and want uniques e.g. for circular roads
                deps = set(deps)

                # Get node indices by binary search
                try:
                    node_indices = [self.binary_search(node_ids, node_id) for node_id in deps]
                except ValueError:
                    continue

                # way_deps is the list of dependent node ids
                # way_coords is a copy of coords indexed by way ids
                for node_id, node_index in izip(deps, node_indices):
                    node_counts[node_index] += 1

            if i % 1000 == 0 and i > 0:
                self.logger.info('doing {}s, at {}'.format(element_id.split(':')[0], i))
            i += 1

        for i, count in enumerate(node_counts):
            if count > 1:
                self.node_ids.append(node_ids[i])

        del node_ids
        del node_counts

        i = 0

        for element_id, props, deps in parse_osm(self.filename, dependencies=True):
            if element_id.startswith('node'):
                node_id = long(element_id.split(':')[-1])
                node_index = self.binary_search(self.node_ids, node_id)
                if node_index is not None:
                    lat = props.get('lat')
                    lon = props.get('lon')
                    lat, lon = latlon_to_decimal(lat, lon)
                    self.node_coordinates.extend([lat, lon])
            elif element_id.startswith('way'):
                props = {safe_decode(k): safe_decode(v) for k, v in six.iteritems(props)}
                way_id = long(element_id.split(':')[-1])
                props['id'] = way_id
                for node_id in deps:
                    node_index = self.binary_search(self.node_ids, node_id)
                    if node_index is not None:
                        way_ids.append(way_id)
                        self.intersection_edges_nodes.append(node_id)
                        self.intersection_edges_ways.append(way_id)
                        self.way_props.Put(safe_encode(way_id), json.dumps(props))

            if i % 1000 == 0 and i > 0:
                self.logger.info('second pass, doing {}s, at {}'.format(element_id.split(':')[0], i))
            i += 1

        i = 0

        indices = numpy.argsort(self.intersection_edges_nodes)
        self.intersection_edges_nodes = numpy.fromiter((self.intersection_edges_nodes[i] for i in indices), dtype=numpy.uint64)
        self.intersection_edges_ways = numpy.fromiter((self.intersection_edges_ways[i] for i in indices), dtype=numpy.uint64)
        del indices

        idx = 0

        # Need to make a copy here otherwise will change dictionary size during iteration
        for node_id, g in groupby(self.intersection_edges_nodes):
            group_len = sum((1 for j in g))

            way_indices = self.intersection_edges_ways[idx:idx + group_len]
            all_ways = [json.loads(reader.way_props.Get(safe_encode(w))) for w in way_indices]
            way_names = set()
            ways = []
            for way in all_ways:
                if way['name'] in way_names:
                    continue
                ways.append(way)
                way_names.add(way['name'])

            idx += group_len

            if i % 1000 == 0 and i > 0:
                self.logger.info('checking intersections, did {}'.format(i))
            i += 1

            if len(ways) > 1:
                node_index = self.binary_search(self.node_ids, node_id)
                lat, lon = self.node_coordinates[node_index * 2], self.node_coordinates[node_index * 2 + 1]
                yield self.node_ids[node_index], lat, lon, ways
