import os
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

UNICODE_DATA_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                'data', 'unicode')

CLDR_DIR = os.path.join(UNICODE_DATA_DIR, 'cldr')
