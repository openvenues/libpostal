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
from geodata.points.index import PointIndex
from geodata.encoding import safe_decode


class MetroStationReverseGeocoder(PointIndex):
    include_property_patterns = set([
        'name',
        'name:*',
        'operator',
        'network',
        'station',
        'int_name',
        'official_name',
        'official_name:*',
        'alt_name',
        'alt_name:*',
        'short_name',
        'short_name:*',
        'place',
        'description',
        'wikipedia',
        'wikipedia:*',
    ])

    def nearest_metro(self, lat, lon, n=1, language_rtree=None):
        nearest = self.nearest_point(lat, lon)
        if not nearest:
            return None

    @classmethod
    def create_from_osm_file(cls, filename, output_dir, precision=PointIndex.GEOHASH_PRECISION):
        '''
        Given an OSM file (planet or some other bounds) containing relations
        and their dependencies, create an R-tree index for coarse-grained
        reverse geocoding.

        Note: the input file is expected to have been created using
        osmfilter. Use fetch_osm_address_data.sh for planet or copy the
        admin borders commands if using other bounds.
        '''
        index = cls(save_dir=output_dir, precision=precision)

        i = 0
        for element_id, props, deps in parse_osm(filename):
            props = {safe_decode(k): safe_decode(v) for k, v in six.iteritems(props)}

            node_id = long(element_id.split(':')[-1])
            lat = props.get('lat')
            lon = props.get('lon')
            if lat is None or lon is None:
                continue
            lat, lon = latlon_to_decimal(lat, lon)
            if lat is None or lon is None:
                continue

            if isclose(lon, 180.0):
                lon = 179.999

            props = {k: v for k, v in six.iteritems(props)
                     if k in ('id', 'type') or k in cls.include_property_patterns or (six.u(':') in k and
                     six.u('{}:*').format(k.split(six.u(':'), 1)[0]) in cls.include_property_patterns)}

            props['type'] = 'node'
            props['id'] = node_id

            index.add_point(lat, lon, props)

            if i % 1000 == 0 and i > 0:
                print('did {} points'.format(i))
            i += 1

        return index

if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-m', '--osm-metro-stations-file',
                        help='Path to OSM metro stations file')

    parser.add_argument('-p', '--precision',
                        type=int,
                        default=PointIndex.GEOHASH_PRECISION,
                        help='Geohash precision')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    logging.basicConfig(level=logging.INFO)

    args = parser.parse_args()
    if args.osm_metro_stations_file:
        index = MetroStationReverseGeocoder.create_from_osm_file(args.osm_metro_stations_file, save_dir=args.out_dir, precision=args.precision)
    else:
        parser.error('Must specify metro stations file')

    index.save()
