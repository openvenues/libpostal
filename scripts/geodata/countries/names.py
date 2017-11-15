# -*- coding: utf-8 -*-
import os
import six
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

COUNTRY_CONFIG = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                              'resources', 'countries', 'names.yaml')

IGNORE_COUNTRIES = set([six.u('ZZ')])

COUNTRY_USE_SHORT_NAME = set([six.u('HK'), six.u('MM'), six.u('MO'), six.u('PS')])
COUNTRY_USE_VARIANT_NAME = set([six.u('CD'), six.u('CG'), six.u('CI'), six.u('TL')])

LANGUAGE_COUNTRY_OVERRIDES = {
    'en': {
        'CD': safe_decode('Democratic Republic of the Congo'),
        'CG': safe_decode('Republic of the Congo'),
    },

    # Countries where the local language is absent from CLDR

    # Tajik / Tajikistan
    'tg': {
        'TJ': safe_decode('Тоҷикистон'),
    },

    # Maldivan / Maldives
    'dv': {
        'MV': safe_decode('ދިވެހިރާއްޖެ'),
    }

}


class CountryNames(object):
    def __init__(self, base_dir=CLDR_MAIN_PATH):
        self.base_dir = base_dir

        self.country_alpha3_codes = {c.alpha2.lower(): c.alpha3.lower() for c in pycountry.countries}
        self.iso_3166_names = {c.alpha2.lower(): c.name for c in pycountry.countries}

        self.language_country_names = {}
        self.country_language_names = defaultdict(dict)

        self.country_official_names = defaultdict(OrderedDict)
        self.country_local_names = defaultdict(OrderedDict)

        local_languages = {}

        country_local_language_names = defaultdict(dict)

        for filename in os.listdir(base_dir):
            lang = filename.split('.xml')[0]
            if len(lang) > 3:
                continue

            names = self.cldr_country_names(lang)
            lang = lang.lower()
            self.language_country_names[lang] = names

            for country, name in names.iteritems():
                country = country.lower()

                languages = get_country_languages(country, official=False) or OrderedDict([('en', 1)])
                local_languages[country] = languages

                self.country_language_names[country.lower()][lang.lower()] = name

                if lang in local_languages.get(country, {}):
                    country_local_language_names[country][lang] = name

        for l, names in six.iteritems(LANGUAGE_COUNTRY_OVERRIDES):
            if l not in self.language_country_names:
                self.language_country_names[l.lower()] = names

            for c, name in six.iteritems(names):
                self.country_language_names[c.lower()][l.lower()] = name
                if c.lower() not in country_local_language_names:
                    country_local_language_names[c.lower()][l.lower()] = name

        for country, langs in six.iteritems(local_languages):
            names = country_local_language_names[country]
            num_defaults = sum((1 for lang in names.keys() if langs.get(lang)))
            for i, (lang, default) in enumerate(langs.iteritems()):
                name = names.get(lang)
                if not name:
                    continue
                if default or num_defaults == 0:
                    self.country_official_names[country][lang] = name
                    if num_defaults == 0:
                        break
                self.country_local_names[country][lang] = name

    def cldr_country_names(self, language):
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
        filename = os.path.join(self.base_dir, '{}.xml'.format(language))
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

    def localized_name(self, country_code, language=None):
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
            >>> country_names.localized_name('jp')     # returns '日本'
            >>> country_names.localized_name('be')     # returns 'België / Belgique / Belgien'
        '''

        country_code = country_code.lower()
        if language is None:
            return six.u(' / ').join(OrderedDict.fromkeys(n.replace(six.u('-'), six.u(' '))
                                     for n in self.country_official_names[country_code].values()).keys())
        else:
            return self.country_language_names.get(country_code, {}).get(language)

    def alpha3_code(self, alpha2_code):
        alpha3 = self.country_alpha3_codes.get(alpha2_code.lower())
        return alpha3.upper() if alpha3 else None

    def iso_3166_name(self, alpha2_code):
        return self.iso_3166_names.get(alpha2_code.lower())

country_names = CountryNames()
