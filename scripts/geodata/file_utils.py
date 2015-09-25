import os
import subprocess


def download_file(url, dest):
    ensure_dir(os.path.dirname(dest))
    return subprocess.call(['wget', url, '-O', dest, '--quiet']) == 0


def unzip_file(filename, dest):
    ensure_dir(dest)
    subprocess.call(['unzip', '-o', filename, '-d', dest])


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
