import array
import logging
import six

from bisect import bisect_left
from collections import defaultdict, OrderedDict
from itertools import izip, combinations

from geodata.coordinates.conversion import latlon_to_decimal
from geodata.osm.extract import *


class OSMIntersectionReader(object):
    def __init__(self, filename):
        self.filename = filename

        self.node_ids = array.array('l')
        self.node_coordinates = array.array('d')

        # Store these in memory, could be LevelDB if needed
        self.way_props = {}
        self.intersections_graph = defaultdict(list)


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
                print('doing {}s, at {}'.format(element_id.split(':')[0], i))
            i += 1

        for i, count in enumerate(node_counts):
            if count > 1:
                self.node_ids.append(node_ids[i])

        del node_ids
        del node_counts

        i = 0

        for element_id, props, deps in parse_osm(self.filename, dependencies=True):
            if element_id.startswith('node'):
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
                        self.intersections_graph[node_index].append(way_id)
                        self.way_props[way_id] = props

            if i % 1000 == 0 and i > 0:
                print('second pass, doing {}s, at {}'.format(element_id.split(':')[0], i))
            i += 1

        for node_index, way_indices in six.iteritems(self.intersections_graph):
            lat, lon = self.node_coordinates[node_index * 2], self.node_coordinates[node_index * 2 + 1]
            ways = [self.way_props[w] for w in way_indices]
            yield self.node_ids[node_index], lat, lon, ways
