import os
import subprocess


def download_file(url, dest_dir):
    return subprocess.call(['wget', url, '-O', dest_dir, '--quiet']) == 0


def unzip_file(filename, dest_dir):
    subprocess.call(['unzip', '-o', filename, '-d', dest_dir])


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
