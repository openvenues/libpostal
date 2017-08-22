import os
import sys
import tempfile

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.insert(0, os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.i18n.download_cldr import CLDR_PATH
from geodata.file_utils import ensure_dir, download_file, upload_file_s3

CLDR_S3_PATH = 's3://libpostal' + CLDR_PATH
CLDR_REMOTE_URL = 'http://www.unicode.org/Public/cldr/latest/core.zip'


def upload_cldr(temp_dir=None):
    if not temp_dir:
        temp_dir = tempfile.gettempdir()
    cldr_local_filename = os.path.join(temp_dir, CLDR_REMOTE_URL.rsplit('/', 1)[-1])
    return download_file(CLDR_REMOTE_URL, cldr_local_filename) and upload_file_s3(cldr_local_filename, CLDR_S3_PATH)


if __name__ == '__main__':
    upload_cldr(*sys.argv[1:])
