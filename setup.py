import os

from setuptools import setup, Extension, find_packages

SRC_DIR = 'src'


def main():
    setup(
        name='postal',
        version='0.1',
        install_requires=[
            'six',
        ],
        ext_modules=[
            Extension('postal.text._tokenize',
                      sources=[
                          os.path.join(SRC_DIR, 'scanner.c'),
                          os.path.join(SRC_DIR, 'string_utils.c'),
                          os.path.join(SRC_DIR, 'tokens.c'),
                          os.path.join(SRC_DIR, 'utf8proc/utf8proc.c'),
                          'python/postal/text/pytokenize.c',
                      ],
                      include_dirs=[SRC_DIR],
                      extra_compile_args=['-O0'],
                      ),

        ],
        packages=find_packages(),
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
