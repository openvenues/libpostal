# -*- coding: utf-8 -*-
from __future__ import unicode_literals

import os
import sys

import pycountry

from collections import OrderedDict

from lxml import etree

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.i18n.unicode_paths import CLDR_DIR
from geodata.i18n.languages import *
from geodata.encoding import safe_decode

CLDR_MAIN_PATH = os.path.join(CLDR_DIR, 'common', 'main')


IGNORE_COUNTRIES = set(['ZZ'])

COUNTRY_USE_SHORT_NAME = set(['HK', 'MM', 'MO', 'PS'])
COUNTRY_USE_VARIANT_NAME = set(['CD', 'CG', 'CI', 'TL'])

LANGUAGE_COUNTRY_OVERRIDES = {
    'en': {
        'CD': 'Democratic Republic of the Congo',
        'CG': 'Republic of the Congo',
    },

    # Countries where the local language is absent from CLDR

    # Tajik / Tajikistan
    'tg': {
        'TJ': 'Тоҷикистон',
    },

    # Maldivan / Maldives
    'dv': {
        'MV': 'ދިވެހިރާއްޖެ',
    }


}


def cldr_country_names(language, base_dir=CLDR_MAIN_PATH):
    '''
    Country names are tricky as there can be several versions
    and levels of verbosity e.g. United States of America
    vs. the more commonly used United States. Most countries
    have a similarly verbose form.

    The CLDR repo (http://cldr.unicode.org/) has the most
    comprehensive localized database of country names
    (among other things), organized by language. This function
    parses CLDR XML for a given language and returns a dictionary
    of {country_code: name} for that language.
    '''
    filename = os.path.join(base_dir, '{}.xml'.format(language))
    xml = etree.parse(open(filename))

    country_names = defaultdict(dict)

    for territory in xml.xpath('*//territories/*'):
        country_code = territory.attrib['type']

        if country_code in IGNORE_COUNTRIES or country_code.isdigit():
            continue

        country_names[country_code][territory.attrib.get('alt')] = safe_decode(territory.text)

    display_names = {}

    for country_code, names in country_names.iteritems():
        if country_code in LANGUAGE_COUNTRY_OVERRIDES.get(language, {}):
            display_names[country_code] = safe_decode(LANGUAGE_COUNTRY_OVERRIDES[language][country_code])
            continue

        default_name = names.get(None)

        if country_code in COUNTRY_USE_SHORT_NAME:
            display_names[country_code] = names.get('short', default_name)
        elif country_code in COUNTRY_USE_VARIANT_NAME:
            display_names[country_code] = names.get('variant', default_name)
        elif default_name is not None:
            display_names[country_code] = default_name

    return display_names


country_alpha2_codes = set([c.alpha2.lower() for c in pycountry.countries])
country_alpha3_codes = set([c.alpha3.lower() for c in pycountry.countries])

country_alpha3_map = {c.alpha3.lower(): c.alpha2.lower() for c in pycountry.countries}

language_country_names = {}

country_official_names = defaultdict(OrderedDict)
country_local_names = defaultdict(OrderedDict)


def init_country_names(base_dir=CLDR_MAIN_PATH):
    '''
    Call init_country_names to initialized the module. Sets up the above dictionaries.
    '''
    global language_country_names
    init_languages()

    local_languages = {}

    country_language_names = defaultdict(dict)

    for filename in os.listdir(base_dir):
        lang = filename.split('.xml')[0]
        if len(lang) > 3:
            continue

        names = cldr_country_names(lang, base_dir=base_dir)
        lang = lang.lower()
        language_country_names[lang] = names

        for country, name in names.iteritems():
            country = country.lower()

            languages = get_country_languages(country, official=False) or OrderedDict([('en', 1)])
            local_languages[country] = languages

            if lang in local_languages.get(country, {}):
                country_language_names[country][lang] = name

    for l, names in LANGUAGE_COUNTRY_OVERRIDES.iteritems():
        if l not in language_country_names:
            language_country_names[l.lower()] = names

        for c, name in names.iteritems():
            if c.lower() not in country_language_names:
                country_language_names[c.lower()][l.lower()] = name

    for country, langs in local_languages.iteritems():
        names = country_language_names[country]
        num_defaults = sum((1 for lang in names.keys() if langs.get(lang)))
        for i, (lang, default) in enumerate(langs.iteritems()):
            name = names.get(lang)
            if not name:
                continue
            if default or num_defaults == 0:
                country_official_names[country][lang] = name
                if num_defaults == 0:
                    break
            country_local_names[country][lang] = name


def country_localized_display_name(country_code):
    '''
    Get the display name for a country code in the local language
    e.g. Россия for Russia, España for Spain, etc.

    For most countries there is a single official name. For countries
    with more than one official language, this will return a concatenated
    version separated by a slash e.g. Maroc / المغرب for Morocco.

    Note that all of the exceptions in road_sign_languages.tsv are also
    taken into account here so India for example uses the English name
    rather than concatenating all 27 toponyms.

    This method should be roughly consistent with OSM's display names.

    Usage:
        >>> country_official_name('jp')     # returns '日本'
        >>> country_official_name('be')     # returns 'België / Belgique / Belgien'
    '''

    country_code = country_code.lower()
    if not country_official_names:
        init_country_names()
    return ' / '.join(OrderedDict.fromkeys(n.replace('-', ' ')
                      for n in country_official_names[country_code].values()).keys())
