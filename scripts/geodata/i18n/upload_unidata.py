import os
import sys
import tempfile

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.insert(0, os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.i18n.download_unidata import UNIDATA_PATH, ISO_15924_PATH
from geodata.i18n.unicode_properties import *
from geodata.file_utils import ensure_dir, download_file, upload_file_s3

UNIDATA_S3_PATH = 's3://libpostal' + UNIDATA_PATH
ISO_15924_S3_PATH = 's3://libpostal' + ISO_15924_PATH


def download_iso15924():
    temp_dir = tempfile.gettempdir()

    # This comes as a .zip
    script_codes_response = requests.get(ISO_15924_REMOTE_URL)
    zf = ZipFile(StringIO(script_codes_response.content))
    iso15924_filename = [name for name in zf.namelist() if name.startswith('iso15924')][0]

    # Strip out the comments, etc.
    temp_iso15924_file = u'\n'.join([line.rstrip() for line in safe_decode(zf.read(iso15924_filename)).split('\n')
                                    if line.strip() and not line.strip().startswith('#')])

    f = open(LOCAL_ISO_15924_FILE, 'w')
    f.write(safe_encode(temp_iso15924_file))
    f.close()


def upload_unidata():
    for d in (UNICODE_DATA_DIR, SCRIPTS_DATA_DIR, BLOCKS_DATA_DIR, PROPS_DATA_DIR, WORD_BREAKS_DIR):
        ensure_dir(d)

    download_file(SCRIPTS_REMOTE_URL, LOCAL_SCRIPTS_FILE)
    download_file(BLOCKS_REMOTE_URL, LOCAL_BLOCKS_FILE)
    download_file(PROPS_REMOTE_URL, LOCAL_PROPS_FILE)
    download_file(PROP_ALIASES_REMOTE_URL, LOCAL_PROP_ALIASES_FILE)
    download_file(PROP_VALUE_ALIASES_REMOTE_URL, LOCAL_PROP_VALUE_ALIASES_FILE)
    download_file(DERIVED_CORE_PROPS_REMOTE_URL, LOCAL_DERIVED_CORE_PROPS_FILE)
    download_file(WORD_BREAKS_REMOTE_URL, LOCAL_WORD_BREAKS_FILE)

    for local_path in (LOCAL_SCRIPTS_FILE, LOCAL_BLOCKS_FILE, LOCAL_PROPS_FILE, LOCAL_PROP_ALIASES_FILE,
                       LOCAL_PROP_VALUE_ALIASES_FILE, LOCAL_DERIVED_CORE_PROPS_FILE, LOCAL_WORD_BREAKS_FILE):
        _, filename = os.path.split(local_path)
        s3_path = UNIDATA_S3_PATH + filename
        upload_file_s3(local_path, s3_path)

    download_iso15924()

    _, iso_15924_filename = os.path.split(LOCAL_ISO_15924_FILE)
    s3_path = ISO_15924_S3_PATH + iso_15924_filename
    upload_file_s3(LOCAL_ISO_15924_FILE, s3_path)

if __name__ == '__main__':
    upload_unidata()
