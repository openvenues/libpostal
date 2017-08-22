import os
import shutil
import subprocess
import sys
import tempfile

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.insert(0, os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.i18n.unicode_paths import CLDR_DIR
from geodata.file_utils import ensure_dir, download_file

CLDR_PATH = '/inputs/unicode/cldr/'
CLDR_URL = 'https://libpostal.s3.amazonaws.com' + CLDR_PATH + 'core.zip'


def download_cldr(temp_dir=None):
    if os.path.exists(CLDR_DIR):
        shutil.rmtree(CLDR_DIR)
    ensure_dir(CLDR_DIR)

    if not temp_dir:
        temp_dir = tempfile.gettempdir()

    cldr_filename = os.path.join(temp_dir, CLDR_URL.rsplit('/', 1)[-1])

    download_file(CLDR_URL, cldr_filename)
    subprocess.check_call(['unzip', cldr_filename, '-d', CLDR_DIR])


if __name__ == '__main__':
    download_cldr(*sys.argv[1:])
