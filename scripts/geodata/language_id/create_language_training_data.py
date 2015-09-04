import argparse
import logging
import os
import subprocess
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.osm.osm_address_training_data import WAYS_LANGUAGE_DATA_FILENAME, ADDRESS_LANGUAGE_DATA_FILENAME, ADDRESS_FORMAT_DATA_LANGUAGE_FILENAME, TOPONYM_LANGUAGE_DATA_FILENAME

LANGUAGES_ALL_FILE = 'languages.all'
LANGAUGES_RANDOM_FILE = 'languages.random'
LANGUAGES_TRAIN_FILE = 'languages.train'
LANGUAGES_CV_FILE = 'languages.cv'
LANGUAGES_TEST_FILE = 'languages.test'


def create_language_training_data(osm_dir, split_data=True, train_split=0.8, cv_split=0.1):
    language_all_path = os.path.join(osm_dir, LANGUAGES_ALL_FILE)

    ways_path = os.path.join(osm_dir, WAYS_LANGUAGE_DATA_FILENAME)

    if os.system(' '.join(['cat', ways_path, '>', language_all_path])) != 0:
        raise SystemError('Could not find {}'.format(ways_path))

    addresses_path = os.path.join(osm_dir, ADDRESS_LANGUAGE_DATA_FILENAME)

    if os.system(' '.join(['cat', addresses_path, '>>', language_all_path])) != 0:
        raise SystemError('Could not find {}'.format(addresses_path))

    formatted_path = os.path.join(osm_dir, ADDRESS_FORMAT_DATA_LANGUAGE_FILENAME)

    if os.system(' '.join(['cat', formatted_path, '>>', language_all_path])) != 0:
        raise SystemError('Could not find {}'.format(formatted_path))

    toponyms_path = os.path.join(osm_dir, TOPONYM_LANGUAGE_DATA_FILENAME)

    if os.system(' '.join(['cat', toponyms_path, '>>', language_all_path])) != 0:
        raise SystemError('Could not find {}'.format(toponyms_path))

    languages_random_path = os.path.join(osm_dir, LANGAUGES_RANDOM_FILE)

    if os.system(u' '.join(['shuf', '--random-source=/dev/urandom', language_all_path, '>', languages_random_path])) != 0:
        raise SystemError('shuffle failed')

    languages_train_path = os.path.join(osm_dir, LANGUAGES_TRAIN_FILE)

    if split_data:
        languages_test_path = os.path.join(osm_dir, LANGUAGES_TEST_FILE)

        num_lines = sum((1 for line in open(languages_random_path)))
        train_lines = int(train_split * num_lines)

        test_lines = num_lines - train_lines
        cv_lines = int(test_lines * (cv_split / (1.0 - train_split))) + 1

        subprocess.check_call(['split', '-l', str(train_lines), languages_random_path, os.path.join(osm_dir, 'language-split-')])
        subprocess.check_call(['mv', os.path.join(osm_dir, 'language-split-aa'), languages_train_path])
        subprocess.check_call(['mv', os.path.join(osm_dir, 'language-split-ab'), languages_test_path])

        languages_cv_path = os.path.join(osm_dir, LANGUAGES_CV_FILE)

        subprocess.check_call(['split', '-l', str(cv_lines), languages_test_path, os.path.join(osm_dir, 'language-split-')])
        subprocess.check_call(['mv', os.path.join(osm_dir, 'language-split-aa'), languages_cv_path])
        subprocess.check_call(['mv', os.path.join(osm_dir, 'language-split-ab'), languages_test_path])
    else:
        subprocess.check_call(['mv', languages_random_path, languages_train_path])

if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-n', '--no-split',
                        action='store_false',
                        default=True,
                        help='Do not split data into train/cv/test')

    parser.add_argument('-t', '--train-split',
                        type=float,
                        default=0.8,
                        help='Train split percentage as a float (default 0.8)')

    parser.add_argument('-c', '--cv-split',
                        type=float,
                        default=0.1,
                        help='Cross-validation split percentage as a float (default 0.1)')

    parser.add_argument('-o', '--osm-dir',
                        default=os.getcwd(),
                        help='OSM directory')

    args = parser.parse_args()
    if args.train_split + args.cv_split >= 1.0:
        raise ValueError('Train split + cross-validation split must be less than 1.0')

    if not os.path.exists(args.osm_dir):
        raise ValueError('OSM directory does not exist')

    create_language_training_data(args.osm_dir, split_data=args.no_split, train_split=args.train_split, cv_split=args.cv_split)
