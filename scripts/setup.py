import os

from setuptools import setup, Extension, find_packages

RESOURCES_DIR = 'resources'


def main():
    setup(
        name='geodata',
        version='0.1',
        packages=find_packages(),
        ext_modules=[
            Extension('geodata.text._tokenize',
                      sources=['geodata/text/pytokenize.c'],
                      libraries=['postal'],
                      include_dirs=['/usr/local/include'],
                      library_dirs=['/usr/local/lib'],
                      extra_compile_args=['-std=c99',
                                          '-Wno-unused-function'],
                      ),
            Extension('geodata.text._normalize',
                      sources=['geodata/text/pynormalize.c'],
                      libraries=['postal'],
                      include_dirs=['/usr/local/include'],
                      library_dirs=['/usr/local/lib'],
                      extra_compile_args=['-std=c99',
                                          '-Wno-unused-function'],
                      ),
        ],
        data_files=[
            (os.path.join(RESOURCES_DIR, os.path.relpath(d, RESOURCES_DIR)), [os.path.join(d, filename) for filename in filenames])
            for d, _, filenames in os.walk(RESOURCES_DIR)
        ],
        package_data={
            'geodata': ['**/*.sh']
        },
        include_package_data=True,
        zip_safe=False,
        url='http://mapzen.com',
        description='Utilities for working with geographic data',
        license='MIT License',
        maintainer='mapzen.com',
        maintainer_email='pelias@mapzen.com'
    )

if __name__ == '__main__':
    main()
