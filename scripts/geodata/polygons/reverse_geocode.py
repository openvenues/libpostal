'''
reverse_geocoder.py
-------------------

In-memory reverse geocoder using polygons from Quattroshapes or OSM.
This should be useful for filling in the blanks both in constructing
training data from OSM addresses and for OpenVenues.

Usage:
    python reverse_geocode.py -o /data/quattroshapes/rtree/reverse -q /data/quattroshapes/
    python reverse_geocode.py -o /data/quattroshapes/rtree/reverse -a /data/osm/planet-admin-borders.osm
'''
import argparse
import logging
import os
import sys

from functools import partial

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.encoding import safe_decode
from geodata.osm.osm_admin_boundaries import OSMAdminPolygonReader
from geodata.polygons.index import *


decode_latin1 = partial(safe_decode, encoding='latin1')


def str_id(v):
    v = int(v)
    if v <= 0:
        return None
    return str(v)


class QuattroshapesReverseGeocoder(RTreePolygonIndex):
    COUNTRIES_FILENAME = 'qs_adm0.shp'
    ADMIN1_FILENAME = 'qs_adm1.shp'
    ADMIN1_REGION_FILENAME = 'qs_adm1_region.shp'
    ADMIN2_FILENAME = 'qs_adm2.shp'
    ADMIN2_REGION_FILENAME = 'qs_adm2_region.shp'
    LOCAL_ADMIN_FILENAME = 'qs_localadmin.shp'
    LOCALITIES_FILENAME = 'qs_localities.shp'
    NEIGHBORHOODS_FILENAME = 'qs_neighborhoods.shp'

    COUNTRY = 'adm0'
    ADMIN1 = 'adm1'
    ADMIN1_REGION = 'adm1_region'
    ADMIN2 = 'adm2'
    ADMIN2_REGION = 'adm2_region'
    LOCAL_ADMIN = 'localadmin'
    LOCALITY = 'locality'
    NEIGHBORHOOD = 'neighborhood'

    sorted_levels = (COUNTRY,
                     ADMIN1_REGION,
                     ADMIN1,
                     ADMIN2_REGION,
                     ADMIN2,
                     LOCAL_ADMIN,
                     LOCALITY,
                     NEIGHBORHOOD,
                     )

    sort_levels = {k: i for i, k in enumerate(sorted_levels)}

    NAME = 'name'
    CODE = 'code'
    LEVEL = 'level'
    GEONAMES_ID = 'geonames_id'
    WOE_ID = 'woe_id'

    polygon_properties = {
        COUNTRIES_FILENAME: {
            NAME: ('qs_a0', safe_decode),
            CODE: ('qs_iso_cc', safe_decode),
            LEVEL: ('qs_level', safe_decode),
            GEONAMES_ID: ('qs_gn_id', str_id),
            WOE_ID: ('qs_woe_id', str_id),
        },
        ADMIN1_FILENAME: {
            NAME: ('qs_a1', safe_decode),
            CODE: ('qs_a1_lc', safe_decode),
            LEVEL: ('qs_level', safe_decode),
            GEONAMES_ID: ('qs_gn_id', str_id),
            WOE_ID: ('qs_woe_id', str_id),
        },
        ADMIN1_REGION_FILENAME: {
            NAME: ('qs_a1r', safe_decode),
            CODE: ('qs_a1r_lc', safe_decode),
            LEVEL: ('qs_level', safe_decode),
            GEONAMES_ID: ('qs_gn_id', str_id),
            WOE_ID: ('qs_woe_id', str_id),
        },
        ADMIN2_FILENAME: {
            NAME: ('qs_a2', decode_latin1),
            CODE: ('qs_a2_lc', safe_decode),
            LEVEL: ('qs_level', safe_decode),
            GEONAMES_ID: ('qs_gn_id', str_id),
            WOE_ID: ('qs_woe_id', str_id),
        },
        ADMIN2_REGION_FILENAME: {
            NAME: ('qs_a2r', safe_decode),
            CODE: ('qs_a2r_lc', safe_decode),
            LEVEL: ('qs_level', safe_decode),
            GEONAMES_ID: ('qs_gn_id', str_id),
            WOE_ID: ('qs_woe_id', str_id),
        },
        LOCAL_ADMIN_FILENAME: {
            NAME: ('qs_la', safe_decode),
            CODE: ('qs_la_lc', safe_decode),
            LEVEL: ('qs_level', safe_decode),
            GEONAMES_ID: ('qs_gn_id', str_id),
            WOE_ID: ('qs_woe_id', str_id),
        },
        LOCALITIES_FILENAME: {
            NAME: ('qs_loc', safe_decode),
            LEVEL: ('qs_level', safe_decode),
            GEONAMES_ID: ('qs_gn_id', str),
            WOE_ID: ('qs_woe_id', str),
        },
        NEIGHBORHOODS_FILENAME: {
            NAME: ('name', safe_decode),
            CODE: ('name_en', safe_decode),
            LEVEL: ('qs_level', safe_decode),
            GEONAMES_ID: ('gn_id', str_id),
            WOE_ID: ('woe_id', str_id),
        }
    }

    @classmethod
    def create_from_shapefiles(cls,
                               input_files,
                               output_dir,
                               index_filename=DEFAULT_INDEX_FILENAME,
                               polys_filename=DEFAULT_POLYS_FILENAME):

        index = cls(save_dir=output_dir, index_filename=index_filename)

        i = 0

        for input_file in input_files:
            f = fiona.open(input_file)

            filename = os.path.split(input_file)[-1]
            include_props = cls.polygon_properties.get(filename)

            for rec in f:
                if not rec or not rec.get('geometry') or 'type' not in rec['geometry']:
                    continue

                properties = rec['properties']

                if filename == cls.NEIGHBORHOODS_FILENAME:
                    properties['qs_level'] = 'neighborhood'

                if include_props:
                    have_all_props = False
                    for k, (prop, func) in include_props.iteritems():
                        v = properties.get(prop, None)
                        if v is not None:
                            try:
                                properties[k] = func(v)
                            except Exception:
                                break
                    else:
                        have_all_props = True
                    if not have_all_props or not properties.get(cls.NAME):
                        continue

                poly_type = rec['geometry']['type']
                if poly_type == 'Polygon':
                    poly = Polygon(rec['geometry']['coordinates'][0])
                    index.index_polygon(i, poly)
                    poly = index.simplify_polygon(poly)
                    index.add_polygon(poly, dict(rec['properties']), include_only_properties=include_props)
                elif poly_type == 'MultiPolygon':
                    polys = []
                    for coords in rec['geometry']['coordinates']:
                        poly = Polygon(coords[0])
                        polys.append(poly)
                        index.index_polygon(i, poly)

                    multi_poly = index.simplify_polygon(MultiPolygon(polys))
                    index.add_polygon(multi_poly, dict(rec['properties']), include_only_properties=include_props)
                else:
                    continue

                i += 1
        return index

    @classmethod
    def create_with_quattroshapes(cls, quattroshapes_dir,
                                  output_dir,
                                  index_filename=DEFAULT_INDEX_FILENAME,
                                  polys_filename=DEFAULT_POLYS_FILENAME):

        admin0_filename = os.path.join(quattroshapes_dir, cls.COUNTRIES_FILENAME)
        admin1_filename = os.path.join(quattroshapes_dir, cls.ADMIN1_FILENAME)
        admin1r_filename = os.path.join(quattroshapes_dir, cls.ADMIN1_REGION_FILENAME)
        admin2_filename = os.path.join(quattroshapes_dir, cls.ADMIN2_FILENAME)
        admin2r_filename = os.path.join(quattroshapes_dir, cls.ADMIN2_REGION_FILENAME)
        local_admin_filename = os.path.join(quattroshapes_dir, cls.LOCAL_ADMIN_FILENAME)
        localities_filename = os.path.join(quattroshapes_dir, cls.LOCALITIES_FILENAME)
        neighborhoods_filename = os.path.join(quattroshapes_dir, cls.NEIGHBORHOODS_FILENAME)

        return cls.create_from_shapefiles([admin0_filename, admin1_filename, admin1r_filename,
                                          admin2_filename, admin2r_filename, local_admin_filename,
                                          localities_filename, neighborhoods_filename],
                                          output_dir, index_filename=index_filename,
                                          polys_filename=polys_filename)

    def sort_level(self, i):
        props, p = self.polygons[i]
        return self.sort_levels.get(props[self.LEVEL], 0)

    def get_candidate_polygons(self, lat, lon):
        candidates = OrderedDict.fromkeys(self.index.intersection((lon, lat, lon, lat))).keys()
        return sorted(candidates, key=self.sort_level, reverse=True)


class OSMReverseGeocoder(RTreePolygonIndex):
    include_property_patterns = set([
        'name',
        'name:*',
        'int_name',
        'official_name',
        'official_name:*',
        'short_name',
        'short_name:*',
        'admin_level',
        'wikipedia',
        'wikipedia:*',
    ])

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
        return cls.fix_polygon(poly)

    @classmethod
    def create_from_osm_file(cls, filename, output_dir,
                             index_filename=DEFAULT_INDEX_FILENAME,
                             polys_filename=DEFAULT_POLYS_FILENAME):
        '''
        Given an OSM file (planet or some other bounds) containing relations
        and their dependencies, create an R-tree index for coarse-grained
        reverse geocoding.

        Note: the input file is expected to have been created using
        osmfilter. Use fetch_osm_address_data.sh for planet or copy the
        admin borders commands if using other geometries.
        '''
        index = cls(save_dir=output_dir, index_filename=index_filename)

        reader = OSMAdminPolygonReader(filename)
        polygons = reader.polygons()

        handler = logging.StreamHandler(sys.stderr)
        reader.logger.addHandler(handler)
        reader.logger.setLevel(logging.INFO)

        polygon_index = 0

        for relation_id, props, outer_polys, inner_polys in polygons:
            props = {k: v for k, v in props.iteritems() if k in cls.include_property_patterns
                     or (':' in k and '{}:*'.format(k.split(':', 1)[0]) in cls.include_property_patterns)}

            if inner_polys and not outer_polys:
                self.logger.warn('inner polygons with no outer')
                continue
            if len(outer_polys) == 1 and not inner_polys:
                poly = cls.to_polygon(outer_polys[0])
                if poly is None or not poly.bounds or len(poly.bounds) != 4:
                    continue
                if poly.type != 'MultiPolygon':
                    index.index_polygon(polygon_index, poly)
                else:
                    for p in poly:
                        index.index_polygon(polygon_index, p)
            else:
                multi = []
                inner = []
                # Validate inner polygons (holes)
                for p in inner_polys:
                    poly = cls.to_polygon(p)
                    if poly is None or not poly.bounds or len(poly.bounds) != 4:
                        continue
                    if poly.type != 'MultiPolygon':
                        inner.append(poly)
                    else:
                        inner.extend(poly)

                # Validate outer polygons
                for p in outer_polys:
                    poly = cls.to_polygon(p)
                    if poly is None or not poly.bounds or len(poly.bounds) != 4:
                        continue
                    # Figure out which outer polygon contains each inner polygon
                    interior = [p2 for p2 in inner if poly.contains(p2)]

                    if interior:
                        # Polygon with holes constructor
                        poly = Polygon(p, [zip(*p2.exterior.coords.xy) for p2 in interior])
                        poly = cls.fix_polygon(poly)
                        if poly is None or not poly.bounds or len(poly.bounds) != 4:
                            continue
                    # R-tree only stores the bounding box, so add the whole polygon
                    if poly.type != 'MultiPolygon':
                        index.index_polygon(polygon_index, poly)
                        multi.append(poly)
                    else:
                        for p in poly:
                            index.index_polygon(polygon_index, p)
                        multi.extend(poly)

                if len(multi) > 1:
                    poly = MultiPolygon(multi)
                elif multi:
                    poly = multi[0]
                else:
                    continue
            poly = index.simplify_polygon(poly)
            index.add_polygon(poly, props)
            # Even if this is a MultiPolygon, only increment the id once per relation
            polygon_index += 1

        return index

if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-q', '--quattroshapes-dir',
                        help='Path to quattroshapes dir')

    parser.add_argument('-a', '--osm-admin-file',
                        help='Path to OSM borders file (with dependencies, .osm format)')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    args = parser.parse_args()
    if args.quattroshapes_dir:
        index = QuattroshapesReverseGeocoder.create_with_quattroshapes(args.quattroshapes_dir, args.out_dir)
    elif args.osm_admin_file:
        index = OSMReverseGeocoder.create_from_osm_file(args.osm_admin_file, args.out_dir)
    else:
        parser.error('Must specify quattroshapes dir or osm admin borders file')

    index.save()
