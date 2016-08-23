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
from geodata.polygons.reverse_geocode import OSMReverseGeocoder, QuattroshapesReverseGeocoder


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

    parser.add_argument('--language-rtree-dir',
                        required=True,
                        help='Language RTree directory')

    parser.add_argument('--rtree-dir',
                        default=None,
                        help='OSM reverse geocoder RTree directory')

    parser.add_argument('--quattroshapes-rtree-dir',
                        default=None,
                        help='Quattroshapes reverse geocoder RTree directory')

    parser.add_argument('--geonames-db',
                        default=None,
                        help='GeoNames db file')

    parser.add_argument('--neighborhoods-rtree-dir',
                        default=None,
                        help='Neighborhoods reverse geocoder RTree directory')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    args = parser.parse_args()

    language_rtree = LanguagePolygonIndex.load(args.language_rtree_dir)

    osm_rtree = None
    if args.rtree_dir:
        osm_rtree = OSMReverseGeocoder.load(args.rtree_dir)

    neighborhoods_rtree = None
    if args.neighborhoods_rtree_dir:
        neighborhoods_rtree = NeighborhoodReverseGeocoder.load(args.neighborhoods_rtree_dir)

    quattroshapes_rtree = None
    if args.quattroshapes_rtree_dir:
        quattroshapes_rtree = QuattroshapesReverseGeocoder.load(args.quattroshapes_rtree_dir)

    geonames = None

    if args.geonames_db:
        geonames = GeoNamesDB(args.geonames_db)

    if args.openaddresses_dir and args.format:
        components = AddressComponents(osm_rtree, language_rtree, neighborhoods_rtree, quattroshapes_rtree, geonames)

        oa_formatter = OpenAddressesFormatter(components)
        oa_formatter.build_training_data(args.openaddresses_dir, args.out_dir, tag_components=not args.untagged)
