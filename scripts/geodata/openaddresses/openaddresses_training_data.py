# -*- coding: utf-8 -*-
'''
openaddresses_training_data.py
------------------------------

This script generates several training sets from OpenAddresses.
'''

import argparse
import os

from geodata.openaddresses.formatter import OpenAddressesFormatter
from geodata.polygons.language_polys import LanguagePolygonIndex


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

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    args = parser.parse_args()

    language_rtree = LanguagePolygonIndex.load(args.language_rtree_dir)

    if args.openaddresses_dir and args.format:
        oa_formatter = OpenAddressesFormatter(language_rtree)
        oa_formatter.build_training_data(args.openaddresses_dir, args.out_dir, tag_components=not args.untagged)
