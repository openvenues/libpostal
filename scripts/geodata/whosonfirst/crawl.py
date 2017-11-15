import gevent
import gevent.pool

import os
import six
import ujson as json

from geodata.whosonfirst.client import WhosOnFirst
from geodata.encoding import safe_encode
from geodata.file_utils import ensure_dir


class WhosOnFirstCrawler(object):
    def __init__(self, wof_dir, cache_size=10000, **s3_args):
        self.wof_dir = wof_dir
        self.admin_dir = os.path.join(wof_dir, 'admin')
        ensure_dir(self.admin_dir)
        self.client = WhosOnFirst(self.admin_dir, **s3_args)

    def walk_files(self, base_dir):
        for root, dirs, files in os.walk(os.path.join(base_dir, 'data')):
            if not files:
                continue
            for filename in files:
                yield os.path.join(root, filename)

    def download_dependencies(self, path):
        data = json.load(open(path))
        props = data['properties']

        _, filename = os.path.split(path)
        current_wof_id = filename.rsplit('.geojson', 1)[0]

        for hierarchy in props.get('wof:hierarchy', []):
            for key, wof_id in six.iteritems(hierarchy):
                wof_id = safe_encode(wof_id)

                if wof_id != current_wof_id and wof_id != '-1' and not self.client.exists_locally(wof_id):
                    if not self.client.download_file(wof_id):
                        print('error downloading {}'.format(wof_id))
                        continue
        return props.get('name')

    def data_and_dependencies(self, path):
        data = json.load(open(path))
        props = data['properties']

        _, filename = os.path.split(path)
        current_wof_id = filename.rsplit('.geojson', 1)[0]

        dependencies = {}

        for hierarchy in props.get('wof:hierarchy', []):
            for key, wof_id in six.iteritems(hierarchy):
                wof_id = safe_encode(wof_id)
                if wof_id in dependencies or wof_id == current_wof_id:
                    continue

                if not self.client.exists_locally(wof_id):
                    continue

                value = self.client.load(wof_id)

                # Only include properties, not all the polygon data
                dependencies[wof_id] = value.get('properties', {})

        return data, dependencies

    def load(self, repo_dir):
        return (self.data_and_dependencies(filename) for filename in self.walk_files(repo_dir))

    def crawl(self, repo_dir, workers=10):
        workers = gevent.pool.Pool(workers)
        return workers.imap_unordered(self.download_dependencies, self.walk_files(repo_dir))
