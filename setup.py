import os

from setuptools import setup, Extension, find_packages

SRC_DIR = 'src'


def main():
    setup(
        name='pypostal',
        version='0.1',
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
                      include_dirs=['.'],
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
                      include_dirs=['.'],
                      extra_compile_args=['-O0', '-std=c99', '-DHAVE_CONFIG_H',
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
