import os
import shutil
import subprocess
import sys
import tempfile

from cldr_data import CLDR_DIR

CLDR_URL = 'http://www.unicode.org/Public/cldr/latest/core.zip'

CLDR_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                        'data', 'unicode', 'cldr')


def download_cldr(temp_dir=None):
    if os.path.exists(CLDR_DIR):
        shutil.rmtree(CLDR_DIR)
        os.mkdir(CLDR_DIR)

    if not temp_dir:
        temp_dir = tempfile.gettempdir()

    cldr_filename = os.path.join(temp_dir, CLDR_URL.rsplit('/', 1)[-1])

    subprocess.check_call(['wget', CLDR_URL, '-O', cldr_filename])
    subprocess.check_call(['unzip', cldr_filename, '-d', CLDR_DIR])

if __name__ == '__main__':
    download_cldr(*sys.argv[1:])
