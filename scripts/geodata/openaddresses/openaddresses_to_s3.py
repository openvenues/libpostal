import csv
import os
import sys
import ujson as json

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.insert(0, os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.coordinates.conversion import latlon_to_decimal
from geodata.csv_utils import unicode_csv_reader
from geodata.encoding import safe_encode
from geodata.file_utils import upload_file_s3
from geodata.openaddresses.config import openaddresses_config

OPENADDRESSES_S3_PATH = 's3://libpostal/inputs/openaddresses'


def convert_openaddresses_file_to_geojson(input_file, output_file):
    out = open(output_file, 'w')

    reader = csv.reader(open(input_file))
    headers = reader.next()

    for row in reader:
        props = dict(zip(headers, row))
        lat = props.pop('LAT', None)
        lon = props.pop('LON', None)
        if not lat or not lon:
            continue

        try:
            lat, lon = latlon_to_decimal(lat, lon)
        except (TypeError, ValueError):
            continue

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


def upload_openaddresses_file_to_s3(path, base_s3_path=OPENADDRESSES_S3_PATH):
    base_dir = os.path.dirname(path)
    _, filename = os.path.split(path)
    dest_geojson_path = os.path.join(base_dir, '{}.geojson'.format(safe_encode(filename.rsplit(u'.', 1)[0])))

    print('converting {} to {}'.format(path, dest_geojson_path))

    convert_openaddresses_file_to_geojson(path, dest_geojson_path)

    s3_path = os.path.join(base_s3_path, dest_geojson_path)
    print('uploading {} to S3'.format(dest_geojson_path))
    upload_file_s3(dest_geojson_path, s3_path, public_read=True)
    print('done uploading to S3')

    os.unlink(dest_geojson_path)


def upload_openaddresses_dir_to_s3(base_dir, base_s3_path=OPENADDRESSES_S3_PATH):
    for source in openaddresses_config.sources:
        source_dir = os.path.join(*map(safe_encode, source[:-1]))
        source_csv_path = os.path.join(source_dir, '{}.csv'.format(safe_encode(source[-1])))
        input_filename = os.path.join(base_dir, source_csv_path)

        upload_openaddresses_file_to_s3(input_filename, base_s3_path=base_s3_path)


def main(path):
    if os.path.isdir(path):
        upload_openaddresses_dir_to_s3(path)
    else:
        upload_openaddresses_file_to_s3(path)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit('Usage: python openaddresses_to_s3.py base_dir_or_single_file')

    path = sys.argv[1]
    main(path)
