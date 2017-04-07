import os
import requests
import subprocess
import sys
import ujson as json

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.file_utils import ensure_dir


WOF_PLACE_DATA_REPO = 'https://github.com/whosonfirst-data/whosonfirst-data'
SEED_URLS_JSON = 'https://raw.githubusercontent.com/whosonfirst-data/whosonfirst-data-postalcode/master/data.json'


def clone_repo(wof_dir, repo):
    repo_name = repo.rstrip('/').rsplit('/', 1)[-1]
    repo_dir = os.path.join(wof_dir, repo_name)

    subprocess.check_call(['rm', '-rf', repo_dir])
    subprocess.check_call(['git', 'clone', repo, repo_dir])

    return repo_dir


def download_wof_postcodes(wof_dir):
    ensure_dir(wof_dir)

    clone_repo(wof_dir, WOF_PLACE_DATA_REPO)

    response = requests.get(SEED_URLS_JSON)
    if response.ok:
        content = json.loads(response.content)

        for d in content:
            repo_name = d['name']

            if int(d.get('count', 0)) > 0:
                repo = d['url']
                print('doing {}'.format(repo_name))

                repo_dir = clone_repo(wof_dir, repo)

            else:
                print('skipping {}'.format(repo_name))

if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit('Usage: python download_wof_postal_codes.py wof_base_dir')
    download_wof_postcodes(sys.argv[1])
