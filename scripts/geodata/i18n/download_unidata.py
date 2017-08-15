import os
import shutil
import subprocess
import sys
import tempfile

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.insert(0, os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.i18n.unicode_properties import *
from geodata.file_utils import ensure_dir, download_file

CLDR_URL = 'http://www.unicode.org/Public/cldr/latest/core.zip'


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

if __name__ == '__main__':
    download_unidata()
