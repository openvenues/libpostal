import fiona
import geohash
import os
import rtree
import ujson as json

from collections import OrderedDict, defaultdict
from shapely.geometry import Point, Polygon, MultiPolygon
from shapely.prepared import prep
from shapely.geometry.geo import mapping

from geodata.polygons.area import polygon_bounding_box_area

DEFAULT_POLYS_FILENAME = 'polygons.geojson'


class PolygonIndex(object):
    include_only_properties = None
    simplify_tolerance = 0.0001
    preserve_topology = True

    INDEX_FILENAME = None

    def __init__(self, index=None, polygons=None, save_dir=None,
                 index_filename=None,
                 include_only_properties=None):
        if save_dir:
            self.save_dir = save_dir
        else:
            self.save_dir = None

        if index_filename:
            self.index_path = os.path.join(save_dir or '.', index_filename)
        else:
            self.index_path = os.path.join(save_dir or '.', self.INDEX_FILENAME)

        if not index:
            self.create_index(overwrite=True)
        else:
            self.index = index

        if include_only_properties and hasattr(include_only_properties, '__contains__'):
            self.include_only_properties = include_only_properties

        if not polygons:
            self.polygons = []
        else:
            self.polygons = polygons

        self.i = 0

    def create_index(self, overwrite=False):
        raise NotImplementedError('Children must implement')

    def index_polygon(self, polygon):
        raise NotImplementedError('Children must implement')

    def simplify_polygon(self, poly, simplify_tolerance=None, preserve_topology=None):
        if simplify_tolerance is None:
            simplify_tolerance = self.simplify_tolerance
        if preserve_topology is None:
            preserve_topology = self.preserve_topology
        return poly.simplify(simplify_tolerance, preserve_topology=preserve_topology)

    def add_polygon(self, poly, properties, include_only_properties=None):
        if include_only_properties is not None:
            properties = {k: v for k, v in properties.iteritems() if k in include_only_properties}
        self.polygons.append((properties, prep(poly)))
        self.i += 1

    @classmethod
    def create_from_shapefiles(cls, inputs, output_dir,
                               index_filename=None,
                               polys_filename=DEFAULT_POLYS_FILENAME,
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
    def to_polygon(cls, coords):
        '''
        Create shapely polygon from list of coordinate tuples if valid
        '''
        if not coords or len(coords) < 3:
            return None
        poly = Polygon(coords)
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

    def save(self, polys_filename=DEFAULT_POLYS_FILENAME):
        self.save_polygons(os.path.join(self.save_dir, polys_filename))
        self.save_index()

    def save_polygons(self, out_filename):
        out = open(out_filename, 'w')
        for props, poly in self.polygons:
            feature = {
                'type': 'Feature',
                'geometry': mapping(poly.context),
                'properties': props
            }
            out.write(json.dumps(feature) + u'\n')

    def save_index(self):
        raise NotImplementedError('Children must implement')

    @classmethod
    def load_polygons(cls, filename):
        f = open(filename)
        polygons = []
        for line in f:
            feature = json.loads(line.rstrip())
            poly_type = feature['geometry']['type']

            if poly_type == 'Polygon':
                poly = Polygon(feature['geometry']['coordinates'][0])
                polygons.append((feature['properties'], prep(poly)))
            elif poly_type == 'MultiPolygon':
                polys = []
                for coords in feature['geometry']['coordinates']:
                    poly = Polygon(coords[0])
                    polys.append(poly)

                polygons.append((feature['properties'], prep(MultiPolygon(polys))))
        return polygons

    @classmethod
    def load_index(cls, d, index_name=None):
        raise NotImplementedError('Children must implement')

    @classmethod
    def load(cls, d, index_name=None, polys_filename=DEFAULT_POLYS_FILENAME):
        index = cls.load_index(d, index_name=index_name or cls.INDEX_FILENAME)
        polys = cls.load_polygons(os.path.join(d, polys_filename))
        return cls(index=index, polygons=polys, save_dir=d)

    def get_candidate_polygons(self, lat, lon):
        raise NotImplementedError('Children must implement')

    def point_in_poly(self, lat, lon, return_all=False):
        polys = self.get_candidate_polygons(lat, lon)
        pt = Point(lon, lat)
        containing = None
        if return_all:
            containing = []
        for i in polys:
            props, poly = self.polygons[i]
            contains = poly.contains(pt)
            if contains and not return_all:
                return props
            elif contains:
                containing.append(props)
        return containing


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
