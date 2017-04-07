import boto3
import os
import six
import ujson as json

from geodata.encoding import safe_encode
from geodata.file_utils import ensure_dir


class WhosOnFirst(object):
    WOF_S3_BUCKET = 'whosonfirst.mapzen.com'

    def __init__(self, wof_dir, **s3_args):
        self.s3 = boto3.client('s3')
        self.wof_dir = wof_dir

    @classmethod
    def path_and_filename(cls, wof_id):
        id_str = safe_encode(wof_id)
        n = 3
        parts = [id_str[i:i + n] for i in six.moves.xrange(0, len(id_str), n)]
        filename = six.u('{}.geojson').format(wof_id)
        return six.u('/').join(parts), filename

    def local_path(self, wof_id):
        s3_path, filename = self.path_and_filename(wof_id)
        local_path = s3_path
        if os.sep != six.u('/'):
            local_path = s3_path.replace(six.u('/'), os.sep)
        return os.path.join(self.wof_dir, local_path, filename)

    def exists_locally(self, wof_id):
        local_path = self.local_path(wof_id)
        return os.path.exists(local_path)

    def load(self, wof_id):
        local_path = self.local_path(wof_id)
        return json.load(open(local_path))

    def download_file(self, wof_id):
        s3_path, filename = self.path_and_filename(wof_id)

        local_path = self.local_path(wof_id)
        local_dir = os.path.dirname(local_path)

        s3_key = six.u('/').join(('data', s3_path, filename))
        try:
            bucket = self.WOF_S3_BUCKET
            self.s3.head_object(Bucket=bucket, Key=s3_key)
            ensure_dir(local_dir)
            if not os.path.exists(local_path):
                self.s3.download_file(self.WOF_S3_BUCKET, s3_key, local_path)
            return True
        except Exception:
            return False
