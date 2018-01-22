'''
reverse_geocode.py
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
import operator
import os
import re
import requests
import shutil
import six
import subprocess
import sys
import tempfile

from functools import partial

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.coordinates.conversion import latlon_to_decimal
from geodata.countries.constants import Countries
from geodata.encoding import safe_decode
from geodata.file_utils import ensure_dir, download_file
from geodata.i18n.unicode_properties import get_chars_by_script
from geodata.i18n.languages import *
from geodata.i18n.word_breaks import ideographic_scripts
from geodata.names.deduping import NameDeduper
from geodata.osm.extract import parse_osm, osm_type_and_id, NODE, WAY, RELATION, OSM_NAME_TAGS
from geodata.osm.admin_boundaries import *
from geodata.polygons.index import *
from geodata.statistics.tf_idf import IDFIndex

from geodata.text.tokenize import tokenize, token_types
from geodata.text.normalize import *

from shapely.topology import TopologicalError

decode_latin1 = partial(safe_decode, encoding='latin1')


def str_id(v):
    v = int(v)
    if v <= 0:
        return None
    return str(v)


class QuattroshapesReverseGeocoder(RTreePolygonIndex):
    '''
    Quattroshapes polygons, for levels up to localities, are relatively
    accurate and provide concordance with GeoPlanet and in some cases
    GeoNames (which is used in other parts of this project).
    '''
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

    PRIORITIES_FILENAME = 'priorities.json'

    persistent_polygons = True
    cache_size = 100000

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
                               index_filename=None,
                               polys_filename=DEFAULT_POLYS_FILENAME,
                               use_all_props=False):

        index = cls(save_dir=output_dir, index_filename=index_filename)

        for input_file in input_files:
            f = fiona.open(input_file)

            filename = os.path.split(input_file)[-1]

            aliases = cls.polygon_properties.get(filename)

            if not use_all_props:
                include_props = aliases
            else:
                include_props = None

            for rec in f:
                if not rec or not rec.get('geometry') or 'type' not in rec['geometry']:
                    continue

                properties = rec['properties']

                if filename == cls.NEIGHBORHOODS_FILENAME:
                    properties['qs_level'] = 'neighborhood'

                have_all_props = False
                for k, (prop, func) in aliases.iteritems():
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
                    poly = cls.to_polygon(rec['geometry']['coordinates'][0])
                    index.index_polygon(poly)
                    poly = index.simplify_polygon(poly)
                    index.add_polygon(poly, dict(rec['properties']), include_only_properties=include_props)
                elif poly_type == 'MultiPolygon':
                    polys = []
                    for coords in rec['geometry']['coordinates']:
                        poly = cls.to_polygon(coords[0])
                        polys.append(poly)
                        index.index_polygon(poly)

                    multi_poly = index.simplify_polygon(MultiPolygon(polys))
                    index.add_polygon(multi_poly, dict(rec['properties']), include_only_properties=include_props)
                else:
                    continue

        return index

    @classmethod
    def create_with_quattroshapes(cls, quattroshapes_dir,
                                  output_dir,
                                  index_filename=None,
                                  polys_filename=DEFAULT_POLYS_FILENAME):

        admin0_filename = os.path.join(quattroshapes_dir, cls.COUNTRIES_FILENAME)
        admin1_filename = os.path.join(quattroshapes_dir, cls.ADMIN1_FILENAME)
        admin1r_filename = os.path.join(quattroshapes_dir, cls.ADMIN1_REGION_FILENAME)
        admin2_filename = os.path.join(quattroshapes_dir, cls.ADMIN2_FILENAME)
        admin2r_filename = os.path.join(quattroshapes_dir, cls.ADMIN2_REGION_FILENAME)
        localities_filename = os.path.join(quattroshapes_dir, cls.LOCALITIES_FILENAME)

        return cls.create_from_shapefiles([admin0_filename, admin1_filename, admin1r_filename,
                                          admin2_filename, admin2r_filename,
                                          localities_filename],
                                          output_dir, index_filename=index_filename,
                                          polys_filename=polys_filename)

    def setup(self):
        self.priorities = []

    def index_polygon_properties(self, properties):
        self.priorities.append(self.sort_levels.get(properties[self.LEVEL], 0))

    def load_polygon_properties(self, d):
        self.priorities = json.load(open(os.path.join(d, self.PRIORITIES_FILENAME)))

    def save_polygon_properties(self, d):
        json.dump(self.priorities, open(os.path.join(d, self.PRIORITIES_FILENAME), 'w'))

    def sort_level(self, i):
        return self.priorities[i]

    def get_candidate_polygons(self, lat, lon):
        candidates = super(QuattroshapesReverseGeocoder, self).get_candidate_polygons(lat, lon)
        return sorted(candidates, key=self.sort_level, reverse=True)


class OSMReverseGeocoder(RTreePolygonIndex):
    '''
    OSM has among the best, most carefully-crafted, accurate administrative
    polygons in the business in addition to using local language naming
    conventions which is desirable for creating a truly multilingual address
    parser.

    The storage of these polygons is byzantine. See geodata.osm.osm_admin_boundaries
    for more details.

    Suffice to say, this reverse geocoder builds an R-tree index on OSM planet
    in a reasonable amount of memory using arrays of C integers and binary search
    for the dependency lookups and Tarjan's algorithm for finding strongly connected
    components to stitch together the polygons.
    '''

    ADMIN_LEVEL = 'admin_level'

    ADMIN_LEVELS_FILENAME = 'admin_levels.json'

    polygon_reader = OSMAdminPolygonReader

    persistent_polygons = True
    # Cache almost everything
    cache_size = 250000
    simplify_polygons = False

    fix_invalid_polygons = True

    include_property_patterns = set([
        'id',
        'type',
        'name',
        'name:*',
        'ISO3166-1:alpha2',
        'ISO3166-1:alpha3',
        'ISO3166-2',
        'int_name',
        'official_name',
        'official_name:*',
        'alt_name',
        'alt_name:*',
        'short_name',
        'short_name:*',
        'admin_level',
        'place',
        'population',
        'designation',
        'wikipedia',
        'wikipedia:*',
    ])

    @classmethod
    def create_from_osm_file(cls, filename, output_dir,
                             index_filename=None,
                             polys_filename=DEFAULT_POLYS_FILENAME):
        '''
        Given an OSM file (planet or some other bounds) containing relations
        and their dependencies, create an R-tree index for coarse-grained
        reverse geocoding.

        Note: the input file is expected to have been created using
        osmfilter. Use fetch_osm_address_data.sh for planet or copy the
        admin borders commands if using other bounds.
        '''
        index = cls(save_dir=output_dir, index_filename=index_filename)

        reader = cls.polygon_reader(filename)
        polygons = reader.polygons()

        logger = logging.getLogger('osm.reverse_geocode')

        for element_id, props, admin_center, outer_polys, inner_polys in polygons:
            props = {k: v for k, v in six.iteritems(props)
                     if k in cls.include_property_patterns or (six.u(':') in k and
                     six.u('{}:*').format(k.split(six.u(':'), 1)[0]) in cls.include_property_patterns)}

            id_type, element_id = osm_type_and_id(element_id)

            test_point = None

            if admin_center:
                admin_center_props = {k: v for k, v in six.iteritems(admin_center)
                                      if k in ('id', 'type', 'lat', 'lon') or k in cls.include_property_patterns or (six.u(':') in k and
                                      six.u('{}:*').format(k.split(six.u(':'), 1)[0]) in cls.include_property_patterns)}

                if cls.fix_invalid_polygons:
                    center_lat, center_lon = latlon_to_decimal(admin_center_props['lat'], admin_center_props['lon'])
                    test_point = Point(center_lon, center_lat)

                props['admin_center'] = admin_center_props

            if inner_polys and not outer_polys:
                logger.warn('inner polygons with no outer')
                continue
            if len(outer_polys) == 1 and not inner_polys:
                poly = cls.to_polygon(outer_polys[0])
                if poly is None or not poly.bounds or len(poly.bounds) != 4:
                    continue
                if poly.type != 'MultiPolygon':
                    index.index_polygon(poly)
                else:
                    for p in poly:
                        index.index_polygon(p)
            else:
                multi = []
                inner = []
                # Validate inner polygons (holes)
                for p in inner_polys:
                    poly = cls.to_polygon(p)
                    if poly is None or not poly.bounds or len(poly.bounds) != 4 or not poly.is_valid:
                        continue

                    if poly.type != 'MultiPolygon':
                        inner.append(poly)
                    else:
                        inner.extend(poly)

                # Validate outer polygons
                for p in outer_polys:
                    poly = cls.to_polygon(p, test_point=test_point)
                    if poly is None or not poly.bounds or len(poly.bounds) != 4:
                        continue

                    interior = []
                    try:
                        # Figure out which outer polygon contains each inner polygon
                        interior = [p2 for p2 in inner if poly.contains(p2)]
                    except TopologicalError:
                        continue

                    if interior:
                        # Polygon with holes constructor
                        poly = cls.to_polygon(p, [zip(*p2.exterior.coords.xy) for p2 in interior], test_point=test_point)
                        if poly is None or not poly.bounds or len(poly.bounds) != 4:
                            continue
                    # R-tree only stores the bounding box, so add the whole polygon
                    if poly.type != 'MultiPolygon':
                        index.index_polygon(poly)
                        multi.append(poly)
                    else:
                        for p in poly:
                            index.index_polygon(p)
                        multi.extend(poly)

                if len(multi) > 1:
                    poly = MultiPolygon(multi)
                elif multi:
                    poly = multi[0]
                else:
                    continue
            if index.simplify_polygons:
                poly = index.simplify_polygon(poly)
            index.add_polygon(poly, props)

        return index

    def setup(self):
        self.admin_levels = []

    def index_polygon_properties(self, properties):
        admin_level = properties.get(self.ADMIN_LEVEL, 0)
        try:
            admin_level = int(admin_level)
        except ValueError:
            admin_level = 0
        self.admin_levels.append(admin_level)

    def load_polygon_properties(self, d):
        self.admin_levels = json.load(open(os.path.join(d, self.ADMIN_LEVELS_FILENAME)))

    def save_polygon_properties(self, d):
        json.dump(self.admin_levels, open(os.path.join(d, self.ADMIN_LEVELS_FILENAME), 'w'))

    def sort_level(self, i):
        return self.admin_levels[i]

    def get_candidate_polygons(self, lat, lon):
        candidates = super(OSMReverseGeocoder, self).get_candidate_polygons(lat, lon)
        return sorted(candidates, key=self.sort_level, reverse=True)


class OSMSubdivisionReverseGeocoder(OSMReverseGeocoder):
    persistent_polygons = True
    cache_size = 10000
    simplify_polygons = False
    polygon_reader = OSMSubdivisionPolygonReader
    include_property_patterns = OSMReverseGeocoder.include_property_patterns | set(['landuse', 'place', 'amenity'])


class OSMBuildingReverseGeocoder(OSMReverseGeocoder):
    persistent_polygons = True
    cache_size = 10000
    simplify_polygons = False

    fix_invalid_polygons = False

    polygon_reader = OSMBuildingPolygonReader
    include_property_patterns = OSMReverseGeocoder.include_property_patterns | set(['building', 'building:levels', 'building:part', 'addr:*'])


class OSMPostalCodeReverseGeocoder(OSMReverseGeocoder):
    persistent_polygons = True
    cache_size = 10000
    simplify_polygons = False
    polygon_reader = OSMPostalCodesPolygonReader
    include_property_patterns = OSMReverseGeocoder.include_property_patterns | set(['postal_code'])


class OSMAirportReverseGeocoder(OSMReverseGeocoder):
    persistent_polygons = True
    cache_size = 10000
    simplify_polygons = False
    polygon_reader = OSMAirportsPolygonReader
    include_property_patterns = OSMReverseGeocoder.include_property_patterns | set(['iata', 'aerodrome', 'aerodrome:type', 'city_served'])


class OSMCountryReverseGeocoder(OSMReverseGeocoder):
    persistent_polygons = True
    cache_size = 10000
    simplify_polygons = False
    polygon_reader = OSMCountryPolygonReader

    @classmethod
    def country_and_languages_from_components(cls, osm_components):
        country = None
        for c in osm_components:
            country = c.get('ISO3166-1:alpha2')
            if country:
                break
        else:
            # See if there's an ISO3166-2 code that matches
            # in case the country polygon is wacky
            for c in osm_components:
                admin1 = c.get('ISO3166-2')
                if admin1:
                    # If so, and if the country is valid, use that
                    country = admin1[:2]
                    if not Countries.is_valid_country_code(country.lower()):
                        return None, []
                    break

        country = country.lower()

        regional = None

        for c in osm_components:
            place_id = '{}:{}'.format(c.get('type', 'relation'), c.get('id', '0'))

            regional = get_regional_languages(country, 'osm', place_id)

            if regional:
                break

        languages = []
        if not regional:
            languages = get_country_languages(country).items()
        else:
            if not all(regional.values()):
                languages = get_country_languages(country)
                languages.update(regional)
                languages = languages.items()
            else:
                languages = regional.items()

        default_languages = sorted(languages, key=operator.itemgetter(1), reverse=True)

        return country, default_languages

    def country_and_languages(self, lat, lon):
        osm_components = self.point_in_poly(lat, lon, return_all=True)
        return self.country_and_languages_from_components(osm_components)


if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-q', '--quattroshapes-dir',
                        help='Path to quattroshapes dir')

    parser.add_argument('-a', '--osm-admin-file',
                        help='Path to OSM borders file (with dependencies, .osm format)')

    parser.add_argument('-s', '--osm-subdivisions-file',
                        help='Path to OSM subdivisions file (with dependencies, .osm format)')

    parser.add_argument('-b', '--osm-building-polygons-file',
                        help='Path to OSM building polygons file (with dependencies, .osm format)')

    parser.add_argument('-p', '--osm-postal-code-polygons-file',
                        help='Path to OSM postal code polygons file (with dependencies, .osm format)')

    parser.add_argument('-r', '--osm-airport-polygons-file',
                        help='Path to OSM airport polygons file (with dependencies, .osm format)')

    parser.add_argument('-c', '--osm-country-polygons-file',
                        help='Path to OSM country polygons file (with dependencies, .osm format)')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    logging.basicConfig(level=logging.INFO)

    args = parser.parse_args()
    if args.osm_admin_file:
        index = OSMReverseGeocoder.create_from_osm_file(args.osm_admin_file, args.out_dir)
    elif args.osm_subdivisions_file:
        index = OSMSubdivisionReverseGeocoder.create_from_osm_file(args.osm_subdivisions_file, args.out_dir)
    elif args.osm_building_polygons_file:
        index = OSMBuildingReverseGeocoder.create_from_osm_file(args.osm_building_polygons_file, args.out_dir)
    elif args.osm_country_polygons_file:
        index = OSMCountryReverseGeocoder.create_from_osm_file(args.osm_country_polygons_file, args.out_dir)
    elif args.osm_postal_code_polygons_file:
        index = OSMPostalCodeReverseGeocoder.create_from_osm_file(args.osm_postal_code_polygons_file, args.out_dir)
    elif args.osm_airport_polygons_file:
        index = OSMAirportReverseGeocoder.create_from_osm_file(args.osm_airport_polygons_file, args.out_dir)
    elif args.quattroshapes_dir:
        index = QuattroshapesReverseGeocoder.create_with_quattroshapes(args.quattroshapes_dir, args.out_dir)
    else:
        parser.error('Must specify quattroshapes dir or osm admin borders file')

    index.save()
