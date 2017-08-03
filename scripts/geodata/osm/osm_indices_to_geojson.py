import argparse
import os
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.metro_stations.reverse_geocode import MetroStationReverseGeocoder
from geodata.neighborhoods.reverse_geocode import NeighborhoodReverseGeocoder
from geodata.places.reverse_geocode import PlaceReverseGeocoder
from geodata.polygons.reverse_geocode import *


if __name__ == '__main__':

    parser = argparse.ArgumentParser()

    parser.add_argument('--country-rtree-dir',
                        required=True,
                        help='Country RTree directory')

    parser.add_argument('--rtree-dir',
                        default=None,
                        help='OSM reverse geocoder RTree directory')

    parser.add_argument('--places-index-dir',
                        default=None,
                        help='Places index directory')

    parser.add_argument('--metro-stations-index-dir',
                        default=None,
                        help='Metro stations reverse geocoder directory')

    parser.add_argument('--subdivisions-rtree-dir',
                        default=None,
                        help='Subdivisions reverse geocoder RTree directory')

    parser.add_argument('--buildings-rtree-dir',
                        default=None,
                        help='Buildings reverse geocoder RTree directory')

    parser.add_argument('--neighborhoods-rtree-dir',
                        default=None,
                        help='Neighborhoods reverse geocoder RTree directory')

    parser.add_argument('-o', '--output-dir',
                        required=True,
                        default=None,
                        )

    args = parser.parse_args()

    if args.country_rtree_dir:
        country_rtree = OSMCountryReverseGeocoder.load(args.country_rtree_dir)
        out_filename = os.path.join(args.output_dir, 'osm_countries.geojson')
        country_rtree.save_geojson(out_filename)

    if args.rtree_dir:
        osm_rtree = OSMReverseGeocoder.load(args.rtree_dir)
        out_filename = os.path.join(args.output_dir, 'osm_admin.geojson')
        osm_rtree.save_geojson(out_filename)

    if args.neighborhoods_rtree_dir:
        neighborhoods_rtree = NeighborhoodReverseGeocoder.load(args.neighborhoods_rtree_dir)
        out_filename = os.path.join(args.output_dir, 'neighborhoods.geojson')
        neighborhoods_rtree.save_geojson(out_filename)

    if args.places_index_dir:
        places_index = PlaceReverseGeocoder.load(args.places_index_dir)
        out_filename = os.path.join(args.output_dir, 'osm_place_points.geojson')
        places_index.save_geojson(out_filename)

    if args.metro_stations_index_dir:
        metro_stations_index = MetroStationReverseGeocoder.load(args.metro_stations_index_dir)
        out_filename = os.path.join(args.output_dir, 'metro_stations.geojson')
        metro_stations_index.save_geojson(out_filename)

    if args.subdivisions_rtree_dir:
        subdivisions_rtree = OSMSubdivisionReverseGeocoder.load(args.subdivisions_rtree_dir)
        out_filename = os.path.join(args.output_dir, 'osm_subdivisions.geojson')
        subdivisions_rtree.save_geojson(out_filename)

    if args.buildings_rtree_dir:
        buildings_rtree = OSMBuildingReverseGeocoder.load(args.buildings_rtree_dir)
        out_filename = os.path.join(args.output_dir, 'osm_buildings.geojson')
        buildings_rtree.save_geojson(out_filename)
