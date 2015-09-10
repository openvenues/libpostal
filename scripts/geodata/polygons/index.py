import fiona
import os
import rtree
import ujson as json

from collections import OrderedDict
from shapely.geometry import Point, Polygon, MultiPolygon
from shapely.prepared import prep
from shapely.geometry.geo import mapping

DEFAULT_INDEX_FILENAME = 'rtree'
DEFAULT_POLYS_FILENAME = 'polygons.geojson'


class RTreePolygonIndex(object):
    include_only_properties = None
    simplify_tolerance = 0.0001
    preserve_topology = True

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
            self.index_path = None

        if not index and self.index_path:
            self.index = rtree.index.Index(self.index_path, overwrite=True)
        elif not index:
            self.index = rtree.index.Index()
        else:
            self.index = index

        if include_only_properties and hasattr(include_only_properties, '__contains__'):
            self.include_only_properties = include_only_properties

        if not polygons:
            self.polygons = []
        else:
            self.polygons = polygons

        self.post_init()

    def post_init(self):
        pass

    def index_polygon(self, id, polygon):
        self.index.insert(id, polygon.bounds)

    def simplify_polygon(self, poly, simplify_tolerance=None, preserve_topology=None):
        if simplify_tolerance is None:
            simplify_tolerance = self.simplify_tolerance
        if preserve_topology is None:
            preserve_topology = self.preserve_topology
        return poly.simplify(simplify_tolerance, preserve_topology=preserve_topology)

    def add_polygon(self, poly, properties):
        if self.include_only_properties:
            properties = {k: v for k, v in properties.iteritems() if k in self.include_only_properties}
        self.polygons.append((properties, prep(poly)))

    @classmethod
    def create_from_shapefiles(cls, inputs, output_dir,
                               index_filename=DEFAULT_INDEX_FILENAME,
                               polys_filename=DEFAULT_POLYS_FILENAME):
        index = cls(save_dir=output_dir, index_filename=index_filename)
        for input_file in inputs:
            f = fiona.open(input_file)

            i = 0

            for rec in f:
                if not rec or not rec.get('geometry') or 'type' not in rec['geometry']:
                    continue
                poly_type = rec['geometry']['type']
                if poly_type == 'Polygon':
                    poly = Polygon(rec['geometry']['coordinates'][0])
                    index.index_polygon(i, poly)
                    index.add_polygon(poly, rec['properties'])
                elif poly_type == 'MultiPolygon':
                    polys = []
                    for coords in rec['geometry']['coordinates']:
                        poly = Polygon(coords[0])
                        polys.append(poly)
                        index.index_polygon(i, poly)

                    index.add_polygon(MultiPolygon(polys), rec['properties'])
                i += 1
        return index

    def save_polygons(self, out_filename):
        out = open(out_filename, 'w')
        features = []
        for props, poly in self.polygons:
            features.append({
                'type': 'Feature',
                'geometry': mapping(poly.context),
                'properties': props
            })
        json.dump({'type': 'FeatureCollection',
                   'features': features},
                  out)

    def save(self, polys_filename=DEFAULT_POLYS_FILENAME):
        self.save_polygons(os.path.join(self.save_dir, polys_filename))
        # need to close index before loading it
        self.index.close()

    @classmethod
    def load_polygons(cls, filename):
        feature_collection = json.load(open(filename))
        polygons = []
        for feature in feature_collection['features']:
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
    def load(cls, d, index_name=DEFAULT_INDEX_FILENAME, polys_filename=DEFAULT_POLYS_FILENAME):
        index = rtree.index.Index(os.path.join(d, index_name))
        polys = cls.load_polygons(os.path.join(d, polys_filename))
        return cls(index=index, polygons=polys, save_dir=d)

    def get_candidate_polygons(self, lat, lon):
        return OrderedDict.fromkeys(self.index.intersection((lon, lat, lon, lat))).keys()

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
