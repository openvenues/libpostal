import os

from setuptools import setup, Extension, find_packages

PROJECT_DIR = os.pardir
SRC_DIR = os.path.join(PROJECT_DIR, 'src')
RESOURCES_DIR = 'resources'


def main():
    setup(
        name='geodata',
        version='0.1',
        packages=find_packages(),
        ext_modules=[
            Extension('geodata.text._tokenize',
                      sources=[os.path.join(SRC_DIR, f)
                               for f in ('scanner.c',
                                         'string_utils.c',
                                         'tokens.c',
                                         'utf8proc/utf8proc.c',
                                         )
                               ] + ['geodata/text/pytokenize.c'],
                      include_dirs=[PROJECT_DIR],
                      extra_compile_args=['-O0', '-std=gnu99',
                                          '-Wno-unused-function'],
                      ),
            Extension('geodata.text._normalize',
                      sources=[os.path.join(SRC_DIR, f)
                               for f in ('normalize.c',
                                         'string_utils.c',
                                         'utf8proc/utf8proc.c',
                                         'tokens.c',
                                         'numex.c',
                                         'unicode_scripts.c',
                                         'transliterate.c',
                                         'file_utils.c',
                                         'trie.c',
                                         'trie_search.c',)
                               ] + ['geodata/text/pynormalize.c'],
                      include_dirs=[PROJECT_DIR],
                      extra_compile_args=['-std=gnu99', '-DHAVE_CONFIG_H',
                                          '-DLIBPOSTAL_DATA_DIR="{}"'.format(os.getenv('LIBPOSTAL_DATA_DIR', os.path.realpath(os.path.join(PROJECT_DIR, 'data')))),
                                          '-Wno-unused-function'],
                      ),
        ],
        data_files=[
            (os.path.join('resources', os.path.relpath(d, RESOURCES_DIR)), [os.path.join(d, filename) for filename in filenames])
            for d, _, filenames in os.walk(RESOURCES_DIR)
        ],
        package_data={
            'geodata': ['**/*.sh']
        },
        include_package_data=True,
        zip_safe=False,
        url='http://github.com/openvenues/libpostal',
        description='Utilities for working with geographic data',
        license='MIT License',
        maintainer='Al Barrentine',
        maintainer_email='libpostal@gmail.com'
    )

if __name__ == '__main__':
    main()
