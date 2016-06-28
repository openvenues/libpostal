#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import unicode_literals

import os
import sys
import unittest

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.i18n.languages import init_languages, get_country_languages, get_regional_languages
from geodata.language_id.disambiguation import disambiguate_language, street_types_gazetteer, UNKNOWN_LANGUAGE, AMBIGUOUS_LANGUAGE


country_test_cases = [
    # String, country, expected language
    ('Division Street', 'us', 'en'),
    ('Kalfarveien', 'no', 'nb'),
    ('Upper Glenburn Road', 'gb', 'en'),
    ('Zafer Caddesi', 'cy', 'tr'),

    # US has some Spanish and French street names
    ('Avenue P', 'us', 'en'),
    ('Avenue du Champs', 'us', 'fr'),
    ('Avenida de la Plata', 'us', 'es'),
    ('Pl', 'us', UNKNOWN_LANGUAGE),
    ('No 2 School House', 'us', UNKNOWN_LANGUAGE),
    ('E Thetford Rd', 'us', 'en'),
    ('El Camino', 'us', 'es'),
    ('The El Camino', 'us', 'en'),
    ('Via Antiqua Street', 'us', 'en'),
    ('Salt Evaporator Plan Road', 'us', 'en'),
    ('Calle Las Brisas North', 'us', 'en'),
    ('Chateau Estates', 'us', 'en'),
    ('Grand Boulevard', 'us', 'en'),
    ('Rue Louis Phillippe', 'us', 'fr'),
    ('Calle Street', 'us', 'en'),
    ('Del Rio Avenue', 'us', 'en'),
    ('South Signal Butte Road', 'us', 'en'),
    ('Chief All Over', 'us', UNKNOWN_LANGUAGE),
    ('South Alameda Street', 'us', 'en'),
    ('The Alameda', 'us', 'en'),
    ('Rincon Road', 'us', 'en'),

    # Avenue + stopword
    ('Avenue du Bourget-du-Lac', 'je', 'fr'),

    # UAE, English is non-default, has abbreviation
    ('128 A St', 'ae', 'en'),
    ('128 A St.', 'ae', 'en'),

    # English / Arabic street address
    ('Omar Street شارع عمر', 'iq', AMBIGUOUS_LANGUAGE),

    # Random script
    ('Bayard Street - 擺也街', 'us', AMBIGUOUS_LANGUAGE),


    # Brussels address
    ('Avenue Paul Héger - Paul Hégerlaan', 'be', AMBIGUOUS_LANGUAGE),
    ('Smaragdstraat', 'be', 'nl'),


    # India
    ('Kidwai nagar', 'in', 'hi'),
    ('Mavoor Rd.', 'in', 'en'),

    # Sri Lanka
    ('Sri Sadathissa Mawatha', 'lk', 'si'),

    # Russian
    ('Фрунзе улица', 'kg', 'ru'),
]

regional_test_cases = [
    # Spain
    ('Carrer de la Morella', 'es', 'qs_a1r', 'Cataluña/Catalunya', 'ca'),
    ('Avinguda Diagonal', 'es', 'qs_a1r', 'Cataluña/Catalunya', 'ca'),
    ('Avinguda de Filipines  -  Avenida de Filipinas', 'es', 'qs_a1r', 'Cataluña/Catalunya', AMBIGUOUS_LANGUAGE),
    ('Calle de la Morella', 'es', 'qs_a1r', 'Cataluña/Catalunya', 'es'),
    ('autobidea', 'es', 'qs_a1r', 'Comunidad Foral de Navarra', 'eu'),
    ('Calle', 'es', 'qs_a1r', 'Comunidad Foral de Navarra', 'es'),
    ('Txurruka', 'es', 'qs_a1r', 'País Vasco/Euskadi', UNKNOWN_LANGUAGE),

    # Belgium
    ('Lutticherstrasse', 'be', 'qs_a1', 'Liège', 'de'),
    ('Chaussée de Charleroi', 'be', 'qs_a1', 'Namur', 'fr'),

    # France / Occitan
    ('Carriera de Brasinvert', 'fr', 'qs_a1r', 'Rhône-Alpes', 'oc'),

]


class TestNormalization(unittest.TestCase):
    def test_countries(self):
        for s, country, expected in country_test_cases:
            languages = get_country_languages(country)
            self.assertTrue(bool(languages))

            lang = disambiguate_language(s, languages.items())
            self.assertEqual(lang, expected, '{} != {} for {}, langs={}'.format(lang, expected, s, languages.items()))

    def test_regional(self):
        for s, country, k, v, expected in regional_test_cases:
            languages = get_country_languages(country)
            self.assertTrue(bool(languages))
            regional = get_regional_languages(country, k, v)
            self.assertTrue(bool(regional))
            regional.update(languages)

            lang = disambiguate_language(s, regional.items())

            self.assertEqual(lang, expected, '{} != {} for {}, langs={}'.format(lang, expected, s, regional.items()))

if __name__ == '__main__':
    unittest.main()
