import argparse
import os
import subprocess
import sys

from setuptools import setup, Extension, Command, find_packages
from setuptools.command.build_py import build_py
from setuptools.command.build_ext import build_ext
from setuptools.command.install import install
from distutils.errors import DistutilsArgError

SRC_DIR = 'src'
this_dir = os.path.realpath(os.path.dirname(__file__))

virtualenv_path = os.environ.get('VIRTUAL_ENV')
site_packages_dir = None
if virtualenv_path:
    virtualenv_path = os.path.abspath(virtualenv_path)
    site_packages_dirs = [d for d in sys.path if d.startswith(virtualenv_path) and d.rstrip(os.sep).endswith('site-packages')]
    if site_packages_dirs:
        site_packages_dir = site_packages_dirs[0]


def build_dependencies(data_dir=None, prefix=None, lib_dir=None):
    subprocess.check_call(['sh', os.path.join(this_dir, 'bootstrap.sh')])
    configure_command = ['sh', os.path.join(this_dir, 'configure')]
    if data_dir:
        configure_command.append('--datadir={}'.format(data_dir))
    if prefix:
        configure_command.append('--prefix={}'.format(prefix))
    if lib_dir:
        configure_command.append('--libdir={}'.format(lib_dir))
    subprocess.check_call(configure_command)
    subprocess.check_call(['make', 'install'])


def normalized_path(path):
    return os.path.abspath(os.path.expanduser(os.path.expandvars(path)))


def get_prefix_path(prefix=None):
    if prefix is None and virtualenv_path:
        prefix = virtualenv_path

    if prefix is not None:
        prefix = normalized_path(prefix)

    return prefix


class InstallWithDependencies(install):
    user_options = install.user_options + [
        ('datadir=', None, 'Data directory for libpostal models'),
        ('prefix=', None, 'Install prefix for libpostal'),
        ('libdir=', None, 'lib directory for libpostal'),
    ]

    def initialize_options(self):
        self.datadir = None
        self.prefix = None
        self.libdir = None
        install.initialize_options(self)

    def finalize_options(self):
        self.prefix = get_prefix_path(self.prefix)

        if self.prefix is not None and not os.path.exists(self.prefix):
            os.mkdir(self.prefix)

        if self.datadir is None and self.prefix:
            self.datadir = os.path.join(self.prefix, 'data')

        if self.datadir is not None:
            self.datadir = normalized_path(self.datadir)
            if not os.path.exists(self.datadir):
                os.mkdir(self.datadir)

        if self.libdir is None and site_packages_dir is not None:
            self.libdir = site_packages_dir

        install.finalize_options(self)

    def run(self):
        build_dependencies(self.datadir, prefix=self.prefix, lib_dir=self.libdir)
        install.run(self)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--prefix')
    args, _ = parser.parse_known_args()

    prefix_path = get_prefix_path(args.prefix)
    include_dirs = []
    library_dirs = []

    if prefix_path is not None:
        include_dirs.append(os.path.join(prefix_path, 'include'))
        library_dirs.append(os.path.join(prefix_path, 'lib'))

    setup(
        name='pypostal',
        version='0.2',
        install_requires=[
            'six',
        ],
        cmdclass={
            'install': InstallWithDependencies,
        },
        ext_modules=[
            Extension('postal.text._tokenize',
                      sources=[os.path.join(SRC_DIR, f)
                               for f in ('scanner.c',
                                         'string_utils.c',
                                         'tokens.c',
                                         'utf8proc/utf8proc.c',
                                         )
                               ] + ['python/postal/text/pytokenize.c'],
                      include_dirs=[this_dir],
                      extra_compile_args=['-O0', '-std=c99',
                                          '-Wno-unused-function'],
                      ),
            Extension('postal.text._normalize',
                      sources=[os.path.join(SRC_DIR, f)
                               for f in ('normalize.c',
                                         'string_utils.c',
                                         'utf8proc/utf8proc.c',
                                         'tokens.c',
                                         'unicode_scripts.c',
                                         'transliterate.c',
                                         'file_utils.c',
                                         'trie.c',
                                         'trie_search.c',)
                               ] + ['python/postal/text/pynormalize.c'],
                      include_dirs=[this_dir],
                      extra_compile_args=['-std=c99', '-DHAVE_CONFIG_H',
                                          '-Wno-unused-function'],
                      ),
            Extension('postal._expand',
                      sources=['python/postal/pyexpand.c'],
                      include_dirs=[this_dir] + include_dirs,
                      libraries=['postal'],
                      library_dirs=library_dirs,
                      extra_compile_args=['-std=c99',
                                          '-DHAVE_CONFIG_H',
                                          '-Wno-unused-function'],
                      ),
            Extension('postal._parser',
                      sources=['python/postal/pyparser.c'],
                      include_dirs=[this_dir] + include_dirs,
                      libraries=['postal'],
                      library_dirs=library_dirs,
                      extra_compile_args=['-std=c99',
                                          '-DHAVE_CONFIG_H',
                                          '-Wno-unused-function'],
                      ),
        ],
        packages=find_packages('python'),
        package_dir={'': 'python'},
        include_package_data=True,
        zip_safe=False,
        url='http://mapzen.com',
        description='Fast address standardization and deduplication',
        license='MIT License',
        maintainer='mapzen.com',
        maintainer_email='pelias@mapzen.com'
    )


if __name__ == '__main__':
    main()
