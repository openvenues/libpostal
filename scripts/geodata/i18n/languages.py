import os
import csv
import sys

from collections import defaultdict, OrderedDict

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.csv_utils import unicode_csv_reader

LANGUAGES_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                             'resources', 'language')

country_languages = defaultdict(OrderedDict)
# Only official and de facto official, no official_regional
official_languages = defaultdict(OrderedDict)

regional_languages = defaultdict(OrderedDict)
road_language_overrides = defaultdict(OrderedDict)

languages = set()
all_languages = languages

osm_admin1_ids = set()

languages_initialized = False


def init_languages(languages_dir=LANGUAGES_DIR):
    global languages_initialized
    if languages_initialized:
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
        if lang not in languages:
            languages.add(lang)

    path = os.path.join(languages_dir, 'regional', 'adm1.tsv')

    for country, key, value, langs, default in unicode_csv_reader(open(path), delimiter='\t'):
        if key == 'osm':
            osm_admin1_ids.add(tuple(value.split(':')))
        for lang in langs.split(','):
            regional_languages[(country, key, value)][lang] = int(default)
            if lang not in country_languages[country]:
                country_languages[country][lang] = 0
            if lang not in languages:
                languages.add(lang)

    languages_initialized = True


init_languages()


def get_country_languages(country, official=True, overrides=True):
    if official:
        languages = official_languages[country]
    else:
        languages = country_languages[country]

    if overrides:
        road_overrides = road_language_overrides.get(country)
        if road_overrides and road_overrides.values()[0]:
            languages = road_overrides
        elif road_overrides:
            languages.update(road_overrides)
    return languages


def get_regional_languages(country, key, value):
    return regional_languages.get((country, key, value), OrderedDict())
