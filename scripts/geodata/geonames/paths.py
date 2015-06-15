import os

this_dir = os.path.realpath(os.path.dirname(__file__))

GEONAMES_DB_NAME = 'geonames.db'

DEFAULT_GEONAMES_DB_PATH = os.path.join(this_dir, os.path.pardir,
                                        os.path.pardir, os.path.pardir,
                                        'data', 'geonames', GEONAMES_DB_NAME)
