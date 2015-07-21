import os
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

DATA_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir, 'resources')

UNICODE_DATA_DIR = os.path.join(DATA_DIR, 'unicode')

CLDR_DIR = os.path.join(UNICODE_DATA_DIR, 'cldr')
