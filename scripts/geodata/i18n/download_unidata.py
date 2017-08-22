import os
import shutil
import subprocess
import sys
import tempfile

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.insert(0, os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.i18n.unicode_properties import *
from geodata.file_utils import ensure_dir, download_file

UNIDATA_PATH = '/inputs/unidata/'
ISO_15924_PATH = '/inputs/iso15924/'
LIBPOSTAL_BASE_S3_URL = 'https://libpostal.s3.amazonaws.com'
UNIDATA_BASE_URL = LIBPOSTAL_BASE_S3_URL + UNIDATA_PATH
ISO_15924_BASE_URL = LIBPOSTAL_BASE_S3_URL + ISO_15924_PATH

SCRIPTS_URL = UNIDATA_BASE_URL + os.path.split(LOCAL_SCRIPTS_FILE)[-1]
BLOCKS_URL = UNIDATA_BASE_URL + os.path.split(LOCAL_BLOCKS_FILE)[-1]
PROPS_URL = UNIDATA_BASE_URL + os.path.split(LOCAL_PROPS_FILE)[-1]
PROP_ALIASES_URL = UNIDATA_BASE_URL + os.path.split(LOCAL_PROP_ALIASES_FILE)[-1]
PROP_VALUE_ALIASES_URL = UNIDATA_BASE_URL + os.path.split(LOCAL_PROP_VALUE_ALIASES_FILE)[-1]
DERIVED_CORE_PROPS_URL = UNIDATA_BASE_URL + os.path.split(LOCAL_DERIVED_CORE_PROPS_FILE)[-1]
WORD_BREAKS_URL = UNIDATA_BASE_URL + os.path.split(LOCAL_WORD_BREAKS_FILE)[-1]
ISO_15924_URL = ISO_15924_BASE_URL + os.path.split(LOCAL_ISO_15924_FILE)[-1]


def download_unidata():
    for d in (UNICODE_DATA_DIR, SCRIPTS_DATA_DIR, BLOCKS_DATA_DIR, PROPS_DATA_DIR, WORD_BREAKS_DIR):
        ensure_dir(d)

    download_file(SCRIPTS_URL, LOCAL_SCRIPTS_FILE)
    download_file(BLOCKS_URL, LOCAL_BLOCKS_FILE)
    download_file(PROPS_URL, LOCAL_PROPS_FILE)
    download_file(PROP_ALIASES_URL, LOCAL_PROP_ALIASES_FILE)
    download_file(PROP_VALUE_ALIASES_URL, LOCAL_PROP_VALUE_ALIASES_FILE)
    download_file(DERIVED_CORE_PROPS_URL, LOCAL_DERIVED_CORE_PROPS_FILE)
    download_file(WORD_BREAKS_URL, LOCAL_WORD_BREAKS_FILE)
    download_file(ISO_15924_URL, LOCAL_ISO_15924_FILE)

if __name__ == '__main__':
    download_unidata()
