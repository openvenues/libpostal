import os
import pycountry
import subprocess
import sys


this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))


WOF_DATA_ADMIN_REPO_URL_PREFIX = "https://github.com/whosonfirst-data/whosonfirst-data/"
WOF_DATA_ADMIN_REPO_PREFIX = "whosonfirst-data-admin-"


def download_wof_data_admin(wof_dir):
    for country_object in pycountry.countries:
        repo_name = WOF_DATA_ADMIN_REPO_PREFIX + country_object.alpha2.lower()
        repo_location = os.path.join(wof_dir, repo_name)
        if not os.path.exists(repo_location):
            subprocess.call(["git", "clone", WOF_DATA_ADMIN_REPO_URL_PREFIX + repo_name])


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit('Usage: python download_whosonfirst_data.py wof_dir')

    download_wof_data_admin(sys.argv[1])
