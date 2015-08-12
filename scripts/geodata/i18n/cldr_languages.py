import argparse
import csv
import os
import requests

from collections import Counter

from cStringIO import StringIO
from lxml import etree

from unicode_paths import CLDR_DIR

this_dir = os.path.realpath(os.path.dirname(__file__))
DEFAULT_LANGUAGES_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                     'resources', 'language', 'countries')

CLDR_SUPPLEMENTAL_DATA = os.path.join(CLDR_DIR, 'common', 'supplemental',
                                      'supplementalData.xml')

ISO_639_3 = 'http://www-01.sil.org/iso639-3/iso-639-3.tab'
ISO_MACROLANGUAGES = 'http://www-01.sil.org/iso639-3/iso-639-3-macrolanguages.tab'

ISO_LANGUAGES_FILENAME = 'iso_languages.tsv'
MACROLANGUAGES_FILENAME = 'iso_macrolanguages.tsv'
COUNTRY_LANGUAGES_FILENAME = 'country_language.tsv'
SCRIPT_LANGUAGES_FILENAME = 'script_languages.tsv'

REGIONAL = 'official_regional'
UNKNOWN_COUNTRY = 'zz'
UNKNOWN_LANGUAGES = ('und', 'zxx')


def write_country_official_languages_file(xml, out_dir):
    lang_file = open(os.path.join(out_dir, COUNTRY_LANGUAGES_FILENAME), 'w')
    lang_writer = csv.writer(lang_file, delimiter='\t')

    def get_population_pct(lang):
        return int(lang.attrib.get('populationPercent', 0))

    lang_scripts = {}
    for lang in xml.xpath('//languageData/language'):
        language_code = lang.attrib['type'].lower()
        scripts = lang.get('scripts')
        if not scripts:
            continue
        territories = lang.get('territories')
        if (language_code, None) not in lang_scripts:
            lang_scripts[(language_code, None)] = scripts

        if not territories:
            continue
        for territory in territories.strip().split():
            lang_scripts[(language_code, territory.lower())] = scripts

    for territory in xml.xpath('//territoryInfo/territory'):
        country_code = territory.attrib['type'].lower()
        if country_code == UNKNOWN_COUNTRY:
            continue
        langs = territory.xpath('languagePopulation')
        languages = Counter()
        official = set()
        regional = set()
        for lang in langs:
            language = lang.attrib['type'].lower().split('_')[0]
            official_status = lang.attrib.get('officialStatus')
            languages[language] += float(lang.attrib['populationPercent'])
            if official_status and official_status != REGIONAL:
                official.add(language)
            elif official_status == REGIONAL:
                regional.add(language)

        if official:
            languages = Counter({l: c for l, c in languages.iteritems()
                                 if l in official or l in regional})
        else:
            languages = Counter({l: c for l, c in languages.most_common(1)})

        for lang, pct in languages.most_common():
            if lang in UNKNOWN_LANGUAGES:
                continue

            script = lang_scripts.get((lang, country_code), lang_scripts.get((lang, None), ''))

            lang_writer.writerow((country_code, lang, script.replace(' ', ','),
                                  str(min(pct, 100.0)), str(int(lang in official))))

RETIRED = 'R'
INDIVIDUAL = 'I'
MACRO = 'M'
LIVING = 'L'


def write_languages_file(langs, macro, out_dir):
    lang_file = open(os.path.join(out_dir, 'iso_languages.tsv'), 'w')
    writer = csv.writer(lang_file, delimiter='\t')
    writer.writerow(('ISO 639-3', 'ISO 639-2B', 'ISO 639-2T',
                     'ISO 639-1', 'type', 'macro'))

    macro_reader = csv.reader(StringIO(macro), delimiter='\t')
    headers = macro_reader.next()
    assert len(headers) == 3
    macros = {minor_code: macro_code for (macro_code, minor_code, status)
              in macro_reader if status != RETIRED}

    lang_reader = csv.reader(StringIO(langs), delimiter='\t')
    headers = lang_reader.next()
    assert headers[:6] == ['Id', 'Part2B', 'Part2T',
                           'Part1', 'Scope', 'Language_Type']

    for line in lang_reader:
        iso639_3, iso639_2b, iso639_2t, iso639_1, scope, lang_type = line[:6]
        macro = macros.get(iso639_3, '')
        # Only living languages that are either individual or macro
        if scope in (INDIVIDUAL, MACRO) and lang_type == LIVING:
            writer.writerow((iso639_3, iso639_2b, iso639_2t,
                             iso639_1, scope, macro))


def fetch_cldr_languages(out_dir=DEFAULT_LANGUAGES_DIR):
    response = requests.get(ISO_639_3)
    langs = response.content

    response = requests.get(ISO_MACROLANGUAGES)
    macro = response.content
    write_languages_file(langs, macro, out_dir)

    supplemental = open(CLDR_SUPPLEMENTAL_DATA)
    xml = etree.parse(supplemental)
    write_country_official_languages_file(xml, out_dir)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-o', '--out',
                        default=DEFAULT_LANGUAGES_DIR,
                        help='Out directory')
    args = parser.parse_args()

    fetch_cldr_languages(args.out)
