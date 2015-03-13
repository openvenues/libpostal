import os
import subprocess


def unzip_file(filename, dest_dir):
    subprocess.call(['unzip', '-o', filename, '-d', dest_dir])


def remove_file(filename):
    os.unlink(filename)


def ensure_dir(d):
    if not os.path.exists(d):
        os.makedirs(d)
