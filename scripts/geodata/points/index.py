import array
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

    POINTS_DB_DIR = 'points'

    GEOHASH_PRECISION = 7
    PROPS_FILENAME = 'properties.json'
    POINTS_FILENAME = 'points.json'
    INDEX_FILENAME = 'index.json'

    def __init__(self, index=None, save_dir=None,
                 points=None,
                 points_path=None,
                 points_db=None,
                 points_db_path=None,
                 index_path=None,
                 include_only_properties=None,
                 precision=GEOHASH_PRECISION):
        if save_dir:
            self.save_dir = save_dir
        else:
            self.save_dir = None

        if include_only_properties and hasattr(include_only_properties, '__contains__'):
            self.include_only_properties = include_only_properties

        if not index_path:
            index_path = os.path.join(save_dir or '.', self.INDEX_FILENAME)

        self.index_path = index_path

        if not index:
            self.index = defaultdict(list)
        else:
            self.index = index

        if not points_path:
            points_path = os.path.join(save_dir or '.', self.POINTS_FILENAME)
        self.points_path = points_path

        if not points:
            self.points = array.array('d')
        else:
            self.points = points

        if not points_db_path:
            points_db_path = os.path.join(save_dir or '.', self.POINTS_DB_DIR)

        if not points_db:
            self.points_db = LevelDB(points_db_path)
        else:
            self.points_db = points_db

        self.precision = precision

        self.i = 0

    def index_point(self, lat, lon):
        code = geohash.encode(lat, lon)[:self.precision]

        for key in [code] + geohash.neighbors(code):
            self.index[key].append(self.i)
        self.points.extend([lat, lon])

    def add_point(self, lat, lon, properties, cache=False, include_only_properties=None):
        if include_only_properties is None and self.include_only_properties:
            include_only_properties = self.include_only_properties
        if include_only_properties is not None:
            properties = {k: v for k, v in properties.iteritems() if k in include_only_properties}

        self.index_point(lat, lon)
        self.points_db.Put(self.properties_key(self.i), json.dumps(properties))
        self.i += 1

    def load_properties(self, filename):
        properties = json.load(open(filename))
        self.i = int(properties.get('num_points', self.i))
        self.precision = int(properties.get('precision', self.precision))

    def save_properties(self, out_filename):
        out = open(out_filename, 'w')
        json.dump({'num_points': str(self.i),
                  'precision': self.precision}, out)

    def save_index(self):
        if not self.index_path:
            self.index_path = os.path.join(self.save_dir or '.', self.INDEX_FILENAME)
        json.dump(self.index, open(self.index_path, 'w'))

    @classmethod
    def load_index(cls, d, index_name=None):
        return json.load(open(os.path.join(d, index_name or cls.INDEX_FILENAME)))

    def save_points(self):
        json.dump(self.points, open(self.points_path, 'w'))

    @classmethod
    def load_points(cls, d):
        return array.array('d', json.load(open(os.path.join(d, cls.POINTS_FILENAME))))

    def properties_key(self, i):
        return 'props:{}'.format(i)

    def get_properties(self, i):
        return json.loads(self.points_db.Get(self.properties_key(i)))

    def compact_points_db(self):
        self.points_db.CompactRange('\x00', '\xff')

    def save(self):
        self.save_index()
        self.save_points()
        self.compact_points_db()
        self.save_properties(os.path.join(self.save_dir, self.PROPS_FILENAME))

    @classmethod
    def load(cls, d):
        index = cls.load_index(d)
        points = cls.load_points(d)
        points_db = LevelDB(os.path.join(d, cls.POINTS_DB_DIR))
        point_index = cls(index=index, points=points, points_db=points_db)
        point_index.load_properties(os.path.join(d, cls.PROPS_FILENAME))
        return point_index

    def __iter__(self):
        for i in xrange(self.i):
            lat, lon = self.points[i * 2], self.points[i * 2 + 1]
            yield self.get_properties(i), lat, lon

    def __len__(self):
        return self.i

    def get_candidate_points(self, latitude, longitude):
        code = geohash.encode(latitude, longitude)[:self.precision]
        candidates = OrderedDict()

        candidates.update([(k, None) for k in self.index.get(code, [])])

        for neighbor in geohash.neighbors(code):
            candidates.update([(k, None) for k in self.index.get(neighbor, [])])

        return candidates.keys()

    def point_distances(self, latitude, longitude):
        candidates = self.get_candidate_points(latitude, longitude)

        return [(i, self.points[i * 2], self.points[i * 2 + 1],
                 haversine_distance(latitude, longitude,
                                    self.points[i * 2],
                                    self.points[i * 2 + 1]))
                for i in candidates]

    def all_nearby_points(self, latitude, longitude):
        distances = self.point_distances(latitude, longitude)
        if not distances:
            return []
        return sorted(distances, key=operator.itemgetter(-1))

    def points_with_properties(self, results):
        return [(self.get_properties(i), lat, lon, distance)
                for i, lat, lon, distance in results]

    def nearest_points(self, latitude, longitude):
        return self.points_with_properties(self.all_nearby_points(latitude, longitude))

    def nearest_n_points(self, latitude, longitude, n=2):
        return self.points_with_properties(self.all_nearby_points(latitude, longitude)[:n])

    def nearest_point(self, latitude, longitude):
        distances = self.all_nearby_points(latitude, longitude)
        if not distances:
            return None
        return self.points_with_properties(distances[:1])[0]
