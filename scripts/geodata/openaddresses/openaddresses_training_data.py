# -*- coding: utf-8 -*-
'''
openaddresses_training_data.py
------------------------------

This script generates several training sets from OpenAddresses.
'''

import argparse
import os

from geodata.openaddresses.formatter import OpenAddressesFormatter

from geodata.addresses.components import AddressComponents
from geodata.geonames.db import GeoNamesDB
from geodata.polygons.language_polys import LanguagePolygonIndex
from geodata.neighborhoods.reverse_geocode import NeighborhoodReverseGeocoder
from geodata.places.reverse_geocode import PlaceReverseGeocoder
from geodata.polygons.reverse_geocode import OSMReverseGeocoder, OSMCountryReverseGeocoder, QuattroshapesReverseGeocoder


if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-i', '--openaddresses-dir',
                        help='Path to OpenAddresses directory')

    parser.add_argument('-f', '--format',
                        action='store_true',
                        default=False,
                        help='Save formatted addresses (slow)')

    parser.add_argument('-u', '--untagged',
                        action='store_true',
                        default=False,
                        help='Save untagged formatted addresses (slow)')

    parser.add_argument('--country-rtree-dir',
                        required=True,
                        help='Country RTree directory')

    parser.add_argument('--rtree-dir',
                        default=None,
                        help='OSM reverse geocoder RTree directory')

    parser.add_argument('--quattroshapes-rtree-dir',
                        default=None,
                        help='Quattroshapes reverse geocoder RTree directory')

    parser.add_argument('--places-index-dir',
                        default=None,
                        help='Places index directory')

    parser.add_argument('--geonames-db',
                        default=None,
                        help='GeoNames db file')

    parser.add_argument('--neighborhoods-rtree-dir',
                        default=None,
                        help='Neighborhoods reverse geocoder RTree directory')

    parser.add_argument('--debug',
                        action='store_true',
                        default=False,
                        help='Test on a sample of each file to debug config')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    args = parser.parse_args()

    country_rtree = OSMCountryReverseGeocoder.load(args.country_rtree_dir)

    osm_rtree = None
    if args.rtree_dir:
        osm_rtree = OSMReverseGeocoder.load(args.rtree_dir)

    neighborhoods_rtree = None
    if args.neighborhoods_rtree_dir:
        neighborhoods_rtree = NeighborhoodReverseGeocoder.load(args.neighborhoods_rtree_dir)

    places_index = None
    if args.places_index_dir:
        places_index = PlaceReverseGeocoder.load(args.places_index_dir)

    quattroshapes_rtree = None
    if args.quattroshapes_rtree_dir:
        quattroshapes_rtree = QuattroshapesReverseGeocoder.load(args.quattroshapes_rtree_dir)

    geonames = None

    if args.geonames_db:
        geonames = GeoNamesDB(args.geonames_db)

    if args.openaddresses_dir and args.format:
        components = AddressComponents(osm_rtree, neighborhoods_rtree, places_index, quattroshapes_rtree, geonames)

        oa_formatter = OpenAddressesFormatter(components, country_rtree, debug=args.debug)
        oa_formatter.build_training_data(args.openaddresses_dir, args.out_dir, tag_components=not args.untagged)
