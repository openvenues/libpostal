import os
import sys
import ujson as json

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.coordinates.conversion import latlon_to_decimal
from geodata.osm.extract import parse_osm


def convert_osm_to_geojson(input_file, output_file):
    out = open(output_file, 'w')
    for key, props, deps in parse_osm(input_file):

        lat, lon = latlon_to_decimal(props.pop('lat', None), props.pop('lon', None))
        if not lat or not lon:
            continue
        geojson = {
            'type': 'Feature',
            'geometry': {
                'coordinates': [lon, lat]
            },
            'properties': props
        }

        out.write(json.dumps(geojson) + u'\n')


if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit('Usage: python osm_to_geojson.py input_file output_file')

    input_file, output_file = sys.argv[1], sys.argv[2]
    convert_osm_to_geojson(input_file, output_file)
