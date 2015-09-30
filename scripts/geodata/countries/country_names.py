import os
import sys

import pycountry

from collections import OrderedDict

from lxml import etree

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.i18n.unicode_paths import CLDR_DIR
from geodata.i18n.languages import *
from geodata.encoding import safe_encode

CLDR_MAIN_PATH = os.path.join(CLDR_DIR, 'common', 'main')


IGNORE_COUNTRIES = set(['ZZ'])

COUNTRY_USE_SHORT_NAME = set(['HK', 'MM', 'MO', 'PS'])
COUNTRY_USE_VARIANT_NAME = set(['CD', 'CG', 'CI', 'TL'])

LANGUAGE_COUNTRY_OVERRIDES = {
    'en': {
        'CD': 'Democratic Republic of the Congo',
        'CG': 'Republic of the Congo',
    }
}


def cldr_country_names(language, base_dir=CLDR_MAIN_PATH):
    filename = os.path.join(base_dir, '{}.xml'.format(language))
    xml = etree.parse(open(filename))

    country_names = {}

    for territory in xml.xpath('*//territories/*'):
        country_code = territory.attrib['type']
        if country_code in IGNORE_COUNTRIES or country_code.isdigit():
            continue
        elif country_code in LANGUAGE_COUNTRY_OVERRIDES.get(language, {}):
            country_names[country_code] = safe_decode(LANGUAGE_COUNTRY_OVERRIDES[language][country_code])
            continue
        elif country_code in COUNTRY_USE_SHORT_NAME and territory.attrib.get('alt') != 'short':
            continue
        elif country_code in COUNTRY_USE_VARIANT_NAME and territory.attrib.get('alt') != 'variant':
            continue
        elif country_code not in COUNTRY_USE_SHORT_NAME and country_code not in COUNTRY_USE_VARIANT_NAME and territory.attrib.get('alt'):
            continue

        country_names[country_code] = safe_decode(territory.text)

    return country_names

country_alpha2_codes = set([c.alpha2.lower() for c in pycountry.countries])
country_alpha3_codes = set([c.alpha3.lower() for c in pycountry.countries])

country_alpha3_map = {c.alpha3.lower(): c.alpha2.lower() for c in pycountry.countries}

language_country_names = {}

country_official_names = defaultdict(OrderedDict)
country_local_names = defaultdict(OrderedDict)


def init_country_names(base_dir=CLDR_MAIN_PATH):
    global language_country_names
    init_languages()

    local_languages = {country: get_country_languages(country, official=False,
                       overrides=False) for country in country_alpha2_codes}

    country_language_names = defaultdict(dict)

    for filename in os.listdir(CLDR_MAIN_PATH):
        lang = filename.split('.xml')[0]
        if len(lang) > 3:
            continue

        names = cldr_country_names(lang, base_dir=base_dir)
        lang = lang.lower()
        language_country_names[lang] = names

        for country, name in names.iteritems():
            country = country.lower()

            if lang in local_languages.get(country, {}):
                country_language_names[country][lang] = name

    for country, langs in local_languages.iteritems():
        names = country_language_names[country]
        for lang, default in langs.iteritems():
            name = names.get(lang)
            if not name:
                continue
            if default:
                country_official_names[country][lang] = name
            country_local_names[country][lang] = name
