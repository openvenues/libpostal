import fiona
import gc
import geohash
import os
import rtree
import six
import ujson as json

from collections import OrderedDict, defaultdict
from leveldb import LevelDB
from lru import LRU
from shapely.geometry import Point, Polygon, MultiPolygon
from shapely.prepared import prep
from shapely.geometry.geo import mapping

from geodata.polygons.area import polygon_bounding_box_area

DEFAULT_POLYS_FILENAME = 'polygons.geojson'
DEFAULT_PROPS_FILENAME = 'properties.json'


class PolygonIndex(object):
    include_only_properties = None
    simplify_tolerance = 0.0001
    preserve_topology = True
    persistent_polygons = False
    cache_size = 0
    fix_invalid_polygons = False

    INDEX_FILENAME = None
    POLYGONS_DB_DIR = 'polygons'

    def __init__(self, index=None, polygons=None, polygons_db=None, save_dir=None,
                 index_filename=None,
                 polygons_db_path=None,
                 include_only_properties=None):
        if save_dir:
            self.save_dir = save_dir
        else:
            self.save_dir = None

        if not index_filename:
            index_filename = self.INDEX_FILENAME

        self.index_path = os.path.join(save_dir or '.', index_filename)

        if not index:
            self.create_index(overwrite=True)
        else:
            self.index = index

        if include_only_properties and hasattr(include_only_properties, '__contains__'):
            self.include_only_properties = include_only_properties

        if not polygons and not self.persistent_polygons:
            self.polygons = {}
        elif polygons and not self.persistent_polygons:
            self.polygons = polygons
        elif self.persistent_polygons and self.cache_size > 0:
            self.polygons = LRU(self.cache_size)
            if polygons:
                for key, value in six.iteritems(polygons):
                    self.polygons[key] = value

            self.cache_hits = 0
            self.cache_misses = 0

            self.get_polygon = self.get_polygon_cached

        if not polygons_db_path:
            polygons_db_path = os.path.join(save_dir or '.', self.POLYGONS_DB_DIR)

        if not polygons_db:
            self.polygons_db = LevelDB(polygons_db_path)
        else:
            self.polygons_db = polygons_db

        self.setup()

        self.i = 0

    def create_index(self, overwrite=False):
        raise NotImplementedError('Children must implement')

    def index_polygon(self, polygon):
        raise NotImplementedError('Children must implement')

    def setup(self):
        pass

    def clear_cache(self, garbage_collect=True):
        if self.persistent_polygons and self.cache_size > 0:
            self.polygons.clear()
            if garbage_collect:
                gc.collect()

    def simplify_polygon(self, poly, simplify_tolerance=None, preserve_topology=None):
        if simplify_tolerance is None:
            simplify_tolerance = self.simplify_tolerance
        if preserve_topology is None:
            preserve_topology = self.preserve_topology
        return poly.simplify(simplify_tolerance, preserve_topology=preserve_topology)

    def index_polygon_properties(self, properties):
        pass

    def polygon_geojson(self, poly, properties):
        return {
            'type': 'Feature',
            'geometry': mapping(poly),
        }

    def add_polygon(self, poly, properties, cache=False, include_only_properties=None):
        if include_only_properties is not None:
            properties = {k: v for k, v in properties.iteritems() if k in include_only_properties}

        if not self.persistent_polygons or cache:
            self.polygons[self.i] = prep(poly)

        if self.persistent_polygons:
            self.polygons_db.Put(self.polygon_key(self.i), json.dumps(self.polygon_geojson(poly, properties)))

        self.polygons_db.Put(self.properties_key(self.i), json.dumps(properties))
        self.index_polygon_properties(properties)
        self.i += 1

    @classmethod
    def create_from_shapefiles(cls, inputs, output_dir,
                               index_filename=None,
                               include_only_properties=None):
        index = cls(save_dir=output_dir, index_filename=index_filename or cls.INDEX_FILENAME)
        for input_file in inputs:
            if include_only_properties is not None:
                include_props = include_only_properties.get(input_file, cls.include_only_properties)
            else:
                include_props = cls.include_only_properties

            f = fiona.open(input_file)

            index.add_geojson_like_file(f)

        return index

    @classmethod
    def fix_polygon(cls, poly):
        '''
        Coerce to valid polygon
        '''
        if not poly.is_valid:
            poly = poly.buffer(0)
            if not poly.is_valid:
                return None
        return poly

    @classmethod
    def to_polygon(cls, coords, holes=None, test_point=None):
        '''
        Create shapely polygon from list of coordinate tuples if valid
        '''
        if not coords or len(coords) < 3:
            return None

        # Fix for polygons crossing the 180th meridian
        lons = [lon for lon, lat in coords]
        if (max(lons) - min(lons) > 180):
            coords = [(lon + 360.0 if lon < 0 else lon, lat) for lon, lat in coords]
            if holes:
                holes = [(lon + 360.0 if lon < 0 else lon, lat) for lon, lat in holes]

        poly = Polygon(coords, holes)
        try:
            if test_point is None:
                test_point = poly.representative_point()
            invalid = cls.fix_invalid_polygons and not poly.is_valid and not poly.contains(test_point)
        except Exception:
            invalid = True

        if invalid:
            try:
                poly_fix = cls.fix_polygon(poly)

                if poly_fix is not None and poly_fix.bounds and len(poly_fix.bounds) == 4 and poly_fix.is_valid and poly_fix.type == poly.type:
                    if test_point is None:
                        test_point = poly_fix.representative_point()

                    if poly_fix.contains(test_point):
                        poly = poly_fix
            except Exception:
                pass

        return poly

    def add_geojson_like_record(self, rec, include_only_properties=None):
        if not rec or not rec.get('geometry') or 'type' not in rec['geometry']:
            return
        poly_type = rec['geometry']['type']
        if poly_type == 'Polygon':
            coords = rec['geometry']['coordinates'][0]
            poly = self.to_polygon(coords)
            if poly is None or not poly.bounds or len(poly.bounds) != 4:
                return
            self.index_polygon(poly)
            self.add_polygon(poly, rec['properties'], include_only_properties=include_only_properties)
        elif poly_type == 'MultiPolygon':
            polys = []
            poly_coords = rec['geometry']['coordinates']
            for coords in poly_coords:
                poly = self.to_polygon(coords[0])
                if poly is None or not poly.bounds or len(poly.bounds) != 4:
                    continue
                polys.append(poly)
                self.index_polygon(poly)

            self.add_polygon(MultiPolygon(polys), rec['properties'], include_only_properties=include_only_properties)
        else:
            return

    def add_geojson_like_file(self, f, include_only_properties=None):
        '''
        Add either GeoJSON or a shapefile record to the index
        '''

        for rec in f:
            self.add_geojson_like_record(rec, include_only_properties=include_only_properties)

    @classmethod
    def create_from_geojson_files(cls, inputs, output_dir,
                                  index_filename=None,
                                  polys_filename=DEFAULT_POLYS_FILENAME,
                                  include_only_properties=None):
        index = cls(save_dir=output_dir, index_filename=index_filename or cls.INDEX_FILENAME)
        for input_file in inputs:
            if include_only_properties is not None:
                include_props = include_only_properties.get(input_file, cls.include_only_properties)
            else:
                include_props = cls.include_only_properties

            f = json.load(open(input_file))

            index.add_geojson_like_file(f['features'], include_only_properties=include_props)

        return index

    def compact_polygons_db(self):
        self.polygons_db.CompactRange('\x00', '\xff')

    def save(self):
        self.save_index()
        self.save_properties(os.path.join(self.save_dir, DEFAULT_PROPS_FILENAME))
        if not self.persistent_polygons:
            self.save_polygons(os.path.join(self.save_dir, DEFAULT_POLYS_FILENAME))
        self.compact_polygons_db()
        self.save_polygon_properties(self.save_dir)

    def load_properties(self, filename):
        properties = json.load(open(filename))
        self.i = int(properties.get('num_polygons', self.i))

    def save_properties(self, out_filename):
        out = open(out_filename, 'w')
        json.dump({'num_polygons': str(self.i)}, out)

    def save_polygons(self, out_filename):
        out = open(out_filename, 'w')
        for i in xrange(self.i):
            poly = self.polygons[i]
            feature = {
                'type': 'Feature',
                'geometry': mapping(poly.context),
            }
            out.write(json.dumps(feature) + u'\n')

    def save_index(self):
        raise NotImplementedError('Children must implement')

    def load_polygon_properties(self, d):
        pass

    def save_polygon_properties(self, d):
        pass

    @classmethod
    def polygon_from_geojson(cls, feature):
        poly_type = feature['geometry']['type']
        if poly_type == 'Polygon':
            coords = feature['geometry']['coordinates']
            poly = cls.to_polygon(coords[0], holes=coords[1:] or None)
            return poly
        elif poly_type == 'MultiPolygon':
            polys = []
            for coords in feature['geometry']['coordinates']:
                poly = cls.to_polygon(coords[0], holes=coords[1:] or None)
                polys.append(poly)

            return MultiPolygon(polys)

    @classmethod
    def load_polygons(cls, filename):
        f = open(filename)
        polygons = {}
        cls.i = 0
        for line in f:
            feature = json.loads(line.rstrip())
            polygons[cls.i] = prep(cls.polygon_from_geojson(feature))
            cls.i += 1
        return polygons

    @classmethod
    def load_index(cls, d, index_name=None):
        raise NotImplementedError('Children must implement')

    @classmethod
    def load(cls, d, index_name=None, polys_filename=DEFAULT_POLYS_FILENAME,
             properties_filename=DEFAULT_PROPS_FILENAME,
             polys_db_dir=POLYGONS_DB_DIR):
        index = cls.load_index(d, index_name=index_name or cls.INDEX_FILENAME)
        if not cls.persistent_polygons:
            polys = cls.load_polygons(os.path.join(d, polys_filename))
        else:
            polys = None
        polygons_db = LevelDB(os.path.join(d, polys_db_dir))
        polygon_index = cls(index=index, polygons=polys, polygons_db=polygons_db, save_dir=d)
        polygon_index.load_properties(os.path.join(d, properties_filename))
        polygon_index.load_polygon_properties(d)
        return polygon_index

    def get_candidate_polygons(self, lat, lon):
        raise NotImplementedError('Children must implement')

    def get_properties(self, i):
        return json.loads(self.polygons_db.Get(self.properties_key(i)))

    def get_polygon(self, i):
        return self.polygons[i]

    def get_polygon_cached(self, i):
        poly = self.polygons.get(i, None)
        if poly is None:
            data = json.loads(self.polygons_db.Get(self.polygon_key(i)))
            poly = prep(self.polygon_from_geojson(data))
            self.polygons[i] = poly
            self.cache_misses += 1
        else:
            self.cache_hits += 1
        return poly

    def __iter__(self):
        for i in xrange(self.i):
            yield self.get_properties(i), self.get_polygon(i)

    def __len__(self):
        return self.i

    def polygons_contain(self, candidates, point, return_all=False):
        containing = None
        if return_all:
            containing = []
        for i in candidates:
            poly = self.get_polygon(i)
            contains = poly.contains(point)
            if contains:
                properties = self.get_properties(i)
                if not return_all:
                    return properties
                else:
                    containing.append(properties)
        return containing

    def polygon_key(self, i):
        return 'poly:{}'.format(i)

    def properties_key(self, i):
        return 'props:{}'.format(i)

    def point_in_poly(self, lat, lon, return_all=False):
        candidates = self.get_candidate_polygons(lat, lon)
        point = Point(lon, lat)
        return self.polygons_contain(candidates, point, return_all=return_all)


class RTreePolygonIndex(PolygonIndex):
    INDEX_FILENAME = 'rtree'

    def create_index(self, overwrite=False):
        self.index = rtree.index.Index(self.index_path, overwrite=overwrite)

    def index_polygon(self, polygon):
        self.index.insert(self.i, polygon.bounds)

    def get_candidate_polygons(self, lat, lon):
        return OrderedDict.fromkeys(self.index.intersection((lon, lat, lon, lat))).keys()

    def save_index(self):
        # need to close index before loading it
        self.index.close()

    @classmethod
    def load_index(cls, d, index_name=None):
        return rtree.index.Index(os.path.join(d, index_name or cls.INDEX_FILENAME))


class GeohashPolygonIndex(PolygonIndex):
    # Reference: https://www.elastic.co/guide/en/elasticsearch/guide/current/geohashes.html
    GEOHASH_PRECISIONS = [
        (7, 152.8 ** 2),
        (6, 1200.0 * 610.0),
        (5, 4900.0 * 4900.0),
        (4, 39000.0 * 19500.0),
        (3, 156000.0 * 156000.0),
        (2, 1251000.0 * 625000.0),
    ]

    INDEX_FILENAME = 'index.json'

    def create_index(self, overwrite=False):
        self.index = defaultdict(list)

    def index_point(self, lat, lon, geohash_level):
        code = geohash.encode(lat, lon)[:geohash_level]

        for key in [code] + geohash.neighbors(code):
            self.index[key].append(self.i)

    def index_polygon(self, polygon):
        poly_area = polygon_bounding_box_area(polygon)
        for geohash_level, area in self.GEOHASH_PRECISIONS:
            if area * 9.0 >= poly_area:
                break

        bbox = polygon.bounds
        lat, lon = (bbox[1] + bbox[3]) / 2.0, (bbox[0] + bbox[2]) / 2.0
        self.index_point(lat, lon, geohash_level)

    def get_candidate_polygons(self, lat, lon, return_all=False):
        candidates = []
        code = geohash.encode(lat, lon)
        for level, area in self.GEOHASH_PRECISIONS:
            indices = self.index.get(code[:level])
            if not indices:
                continue
            candidates.extend(indices)
            if not return_all:
                break
        return candidates

    def save_index(self):
        if not self.index_path:
            self.index_path = os.path.join(self.save_dir or '.', self.INDEX_FILENAME)
        json.dump(self.index, open(self.index_path, 'w'))

    @classmethod
    def load_index(cls, d, index_name=None):
        return json.load(open(os.path.join(d, index_name or cls.INDEX_FILENAME)))
