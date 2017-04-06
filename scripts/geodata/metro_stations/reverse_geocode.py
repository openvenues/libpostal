import argparse
import logging
import os
import sys
import six

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.address_expansions.abbreviations import abbreviate
from geodata.coordinates.conversion import latlon_to_decimal
from geodata.math.floats import isclose
from geodata.osm.extract import parse_osm
from geodata.places.reverse_geocode import PlaceReverseGeocoder
from geodata.encoding import safe_decode


class MetroStationReverseGeocoder(PlaceReverseGeocoder):
    GEOHASH_PRECISION = 7

    include_property_patterns = PlaceReverseGeocoder.include_property_patterns | set([
        'operator',
        'network',
        'station',
    ])


if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-m', '--osm-metro-stations-file',
                        help='Path to OSM metro stations file')

    parser.add_argument('-p', '--precision',
                        type=int,
                        default=MetroStationReverseGeocoder.GEOHASH_PRECISION,
                        help='Geohash precision')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    logging.basicConfig(level=logging.INFO)

    args = parser.parse_args()
    if args.osm_metro_stations_file:
        index = MetroStationReverseGeocoder.create_from_osm_file(args.osm_metro_stations_file, args.out_dir, precision=args.precision)
    else:
        parser.error('Must specify metro stations file')

    index.save()
