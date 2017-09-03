import os
import sys

from geodata.file_utils import download_file, ensure_dir

WOF_BUNDLE_FILENAME_TEMPLATE = 'wof-{}-latest-bundle.tar.bz2'
WOF_BUNDLE_BASE_URL = 'https://whosonfirst.mapzen.com/bundles/'

WOF_HIERARCHY_BUNDLES = [
    'continent',
    'country',
    'dependency',
    'disputed',
    'macroregion',
    'region',
    'macrocounty',
    'county',
    'localadmin',
    'locality',
    'borough',
    'macrohood',
    'neighbourhood'
]


def main(out_dir, bundles=WOF_HIERARCHY_BUNDLES):
    for bundle in bundles:
        print('doing {}'.format(bundle))
        ensure_dir(os.path.join(out_dir, bundle))
        filename = WOF_BUNDLE_FILENAME_TEMPLATE.format(bundle)

        url = WOF_BUNDLE_BASE_URL + filename

        dest_path = os.path.join(out_dir, bundle, filename)
        download_file(url, dest_path)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit('Usage: python download_wof_hierarchy.py outdir')

    out_dir = sys.argv[1]
    main(out_dir)
