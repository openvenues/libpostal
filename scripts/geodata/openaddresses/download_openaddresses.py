import os
import six
import sys
import requests
import subprocess
import yaml

from six.moves.urllib_parse import urljoin

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.openaddresses.formatter import OPENADDRESSES_PARSER_DATA_CONFIG
from geodata.file_utils import ensure_dir, download_file, cd, remove_file

BASE_OPENADDRESSES_DATA_URL = 'http://results.openaddresses.io'
OPENADDRESSES_LATEST_URL = BASE_OPENADDRESSES_URL + '/latest/run/'

OPENADDRESSES_EXTENSION = '.zip'


def main(out_dir):
    ensure_dir(out_dir)

    config = yaml.load(open(OPENADDRESSES_PARSER_DATA_CONFIG))

    with cd(out_dir):
        for path in openaddresses_config.sources:
            source = '/'.join(path)
            zip_file = path[-1] + OPENADDRESSES_EXTENSION
            zip_url = source + OPENADDRESSES_EXTENSION
            url = OPENADDRESSES_LATEST_URL + zip_url

            zip_path = os.path.join(out_dir, zip_file)

            print('downloading: {}', source)
            if download_file(url, zip_path):
                subprocess.check_call(['unzip', zip_path])
                remove_file(zip_path)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage: python download_openaddresses.py out_dir')
        sys.exit(1)
    main(sys.argv[1])
