import os
import csv
import sys

from collections import defaultdict, OrderedDict

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.unicode_csv import unicode_csv_reader

LANGUAGES_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                             'data', 'language')

country_languages = defaultdict(OrderedDict)
# Only official and de facto official, no official_regional
official_languages = defaultdict(OrderedDict)

regional_languages = {}
road_language_overrides = defaultdict(OrderedDict)

languages = set()

initialized = False


def init_languages(languages_dir=LANGUAGES_DIR):
    global initialized
    if initialized:
        return
    path = os.path.join(languages_dir, 'countries', 'country_language.tsv')
    if not os.path.exists(path):
        raise ValueError('File does not exist: {}'.format(path))

    for country, lang, script, pct, is_official in unicode_csv_reader(open(path), delimiter='\t'):
        country_languages[country][lang] = int(is_official)
        languages.add(lang)

    for country, lang, script, pct, is_official in unicode_csv_reader(open(path), delimiter='\t'):
        if int(is_official) or len(country_languages[country]) == 1:
            official_languages[country][lang] = 1

    path = os.path.join(languages_dir, 'countries', 'road_sign_languages.tsv')
    for country, lang, default in csv.reader(open(path), delimiter='\t'):
        road_language_overrides[country][lang] = int(default)

    path = os.path.join(languages_dir, 'regional', 'adm1.tsv')

    for country, key, value, lang, default in unicode_csv_reader(open(path), delimiter='\t'):
        regional_languages[(country, key, value)] = (lang, int(default))
        if lang not in country_languages[country]:
            country_languages[country][lang] = 0

    initialized = True
