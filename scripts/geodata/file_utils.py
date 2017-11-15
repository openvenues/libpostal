import os
import subprocess
import six


def download_file(url, dest, retries=3, retry_delay=5):
    ensure_dir(os.path.dirname(dest))
    return subprocess.check_output(['curl', url, '-L', '-w', '%{http_code}',
                                    '--retry', six.text_type(retries),
                                    '--retry-delay', six.text_type(retry_delay),
                                    '-o', dest, '--silent']) == '200'


def unzip_file(filename, dest):
    ensure_dir(dest)
    return subprocess.check_call(['unzip', '-o', filename, '-d', dest]) == 0


def remove_file(filename):
    os.unlink(filename)


def ensure_dir(d):
    if not os.path.exists(d):
        os.makedirs(d)


class cd:
    """Context manager for changing the current working directory"""
    def __init__(self, path):
        self.path = path

    def __enter__(self):
        self.saved_path = os.getcwd()
        os.chdir(self.path)

    def __exit__(self, etype, value, traceback):
        os.chdir(self.saved_path)
