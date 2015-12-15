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
if virtualenv_path:
    virtualenv_path = os.path.abspath(virtualenv_path)


def build_dependencies(data_dir=None, prefix=None):
    subprocess.check_call(['sh', os.path.join(this_dir, 'bootstrap.sh')])
    configure_command = ['sh', os.path.join(this_dir, 'configure')]
    if data_dir:
        configure_command.append('--datadir={}'.format(data_dir))
    if prefix:
        configure_command.append('--prefix={}'.format(prefix))
    subprocess.check_call(configure_command)
    subprocess.check_call(['make', 'install'])


def normalized_path(path):
    return os.path.abspath(os.path.expanduser(os.path.expandvars(path)))


class InstallWithDependencies(install):
    user_options = install.user_options + [
        ('datadir=', None, 'Data directory for libpostal models'),
        ('prefix=', None, 'Install prefix for libpostal'),
    ]

    def initialize_options(self):
        self.datadir = None
        self.prefix = None
        install.initialize_options(self)

    def finalize_options(self):
        if self.prefix is None and virtualenv_path:
            self.prefix = virtualenv_path

        if self.prefix is not None:
            self.prefix = normalized_path(self.prefix)
            if not os.path.exists(self.prefix):
                os.mkdir(self.prefix)

        if self.datadir is None and self.prefix:
            self.datadir = os.path.join(self.prefix, 'data')

        if self.datadir is not None:
            self.datadir = normalized_path(self.datadir)
            if not os.path.exists(self.datadir):
                os.mkdir(self.datadir)
        install.finalize_options(self)

    def run(self):
        build_dependencies(self.datadir, prefix=self.prefix)
        install.run(self)


class BuildExtensionWithDependencies(build_ext):
    user_options = build_ext.user_options + [
        ('prefix=', None, 'Install prefix for libpostal'),
    ]

    def initialize_options(self):
        self.prefix = None
        build_ext.initialize_options(self)

    def finalize_options(self):
        if self.prefix is None and virtualenv_path:
            self.prefix = virtualenv_path

        if self.prefix is not None:
            self.prefix = normalized_path(self.prefix)
            if not os.path.exists(self.prefix):
                os.mkdir(self.prefix)

            self.include_dirs = (self.include_dirs or []) + [os.path.join(self.prefix, 'include')]
            self.library_dirs = (self.library_dirs or []) + [os.path.join(self.prefix, 'lib')]

        build_ext.finalize_options(self)


def main():
    setup(
        name='pypostal',
        version='0.2',
        install_requires=[
            'six',
        ],
        cmdclass={
            'install': InstallWithDependencies,
            'build_ext': BuildExtensionWithDependencies,
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
                      include_dirs=[this_dir],
                      libraries=['postal'],
                      extra_compile_args=['-std=c99',
                                          '-DHAVE_CONFIG_H',
                                          '-Wno-unused-function'],
                      ),
            Extension('postal._parser',
                      sources=['python/postal/pyparser.c'],
                      include_dirs=[this_dir],
                      libraries=['postal'],
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
