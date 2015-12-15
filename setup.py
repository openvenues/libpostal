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


def main():
    setup(
        name='pypostal',
        version='0.2',
        install_requires=[
            'six',
        ],
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
                      libraries=['postal'],
                      extra_compile_args=['-std=c99',
                                          '-Wno-unused-function'],
                      ),
            Extension('postal._parser',
                      sources=['python/postal/pyparser.c'],
                      libraries=['postal'],
                      extra_compile_args=['-std=c99',
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
