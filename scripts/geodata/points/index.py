import geohash
import os
import math
import operator
import six
import ujson as json

from collections import defaultdict, OrderedDict

from leveldb import LevelDB

from geodata.distance.haversine import haversine_distance


class PointIndex(object):
    include_only_properties = None
    persistent_index = False
    cache_size = 0

    INDEX_FILENAME = None
    POINTS_DB_DIR = 'points'

    DEFAULT_GEOHASH_PRECISION = 7
    DEFAULT_PROPS_FILENAME = 'properties.json'

    INDEX_FILENAME = 'index.json'

    def __init__(self, index=None,
                 points=None,
                 points_db=None, save_dir=None,
                 points_db_path=None,
                 index_path=None,
                 include_only_properties=None,
                 precision=DEFAULT_GEOHASH_PRECISION):
        if save_dir:
            self.save_dir = save_dir
        else:
            self.save_dir = None

        if include_only_properties and hasattr(include_only_properties, '__contains__'):
            self.include_only_properties = include_only_properties

        if not index_path:
            index_path = os.path.join(save_dir or '.', self.INDEX_FILENAME)

        if not index:
            self.index = defaultdict(list)
        else:
            self.index = index

        if not points_db_path:
            points_db_path = os.path.join(save_dir or '.', self.POINTS_DB_DIR)

        if not points_db:
            self.points_db = LevelDB(points_db_path)
        else:
            self.points_db = points_db

        self.precision = precision

        self.i = 0

    def create_index(self, overwrite=False):
        self.index = defaultdict(list)

    def index_point(self, lat, lon):
        code = geohash.encode(lat, lon)[:self.precision]

        for key in [code] + geohash.neighbors(code):
            self.index[key].append((self.i, lat, lon))

    def add_point(self, lat, lon, properties, cache=False, include_only_properties=None):
        if include_only_properties is not None:
            properties = {k: v for k, v in properties.iteritems() if k in include_only_properties}

        self.index_point(lat, lon)
        self.points_db.Put(self.properties_key(self.i), json.dumps(properties))
        self.i += 1

    def load_properties(self, filename):
        properties = json.load(open(filename))
        self.i = int(properties.get('num_polygons', self.i))
        self.precision = int(properties.get('precision', self.precision))

    def save_properties(self, out_filename):
        out = open(out_filename, 'w')
        json.dump({'num_polygons': str(self.i),
                  'precision': self.precision}, out)

    def save_index(self):
        if not self.index_path:
            self.index_path = os.path.join(self.save_dir or '.', self.INDEX_FILENAME)
        json.dump(self.index, open(self.index_path, 'w'))

    @classmethod
    def load_index(cls, d, index_name=None):
        return json.load(open(os.path.join(d, index_name or cls.INDEX_FILENAME)))

    def properties_key(self, i):
        return 'props:{}'.format(i)

    def save(self):
        self.save_index()
        self.save_properties(os.path.join(self.save_dir, self.DEFAULT_PROPS_FILENAME))

    def get_candidate_points(self, latitude, longitude):
        code = geohash.encode(latitude, longitude)[:self.precision]
        candidates = OrderedDict()

        candidates.update([(k, None) for k in self.index.get(code, [])])

        for neighbor in geohash.neighbors(code):
            candidates.update([(k, None) for k in self.index.get(neighbor, [])])

        return candidates.keys()

    def point_distances(self, latitude, longitude):
        candidates = self.get_candidate_points(latitude, longitude)
        return [(i, lat, lon, haversine_distance(latitude, longitude, lat, lon)) for i, lat, lon in candidates]

    def nearest_n_points(self, latitude, longitude, n=2):
        distances = self.point_distances(latitude, longitude)
        if not distances:
            return None
        return sorted(distances, key=operator.itemgetter(-1))[:n]

    def nearest_point(self, latitude, longitude):
        distances = self.nearest_n_points(latitude, longitude, n=1)
        if not distances:
            return None
        return distances[0]
