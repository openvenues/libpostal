'''
scripts.py

This code uses the latest copy of Scripts.txt from unicode.org
to generate a C file (and header) defining which script every character
belongs to.
'''

import csv
import os
import requests
import sys
import tempfile
import requests

from cStringIO import StringIO

from collections import OrderedDict, defaultdict

from lxml import etree

from operator import itemgetter

from zipfile import ZipFile

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.encoding import safe_encode, safe_decode

from word_breaks import script_regex, regex_char_range
from cldr_languages import *

SCRIPTS_HEADER = 'unicode_script_types.h'
SCRIPTS_DATA_FILENAME = 'unicode_scripts_data.c'

SCRIPTS_URL = 'http://unicode.org/Public/UNIDATA/Scripts.txt'

ISO_15924_URL = 'http://unicode.org/iso15924/iso15924.txt.zip'

NUM_CHARS = 65536

scripts_header_template = u'''#ifndef UNICODE_SCRIPT_TYPES_H
#define UNICODE_SCRIPT_TYPES_H

#include <stdlib.h>

#define NUM_CHARS {num_chars}
#define MAX_LANGS {max_langs}

typedef enum {{
    {script_enum}
}} script_t;

#endif
'''

scripts_c_data_template = u'''
script_t char_scripts[] = {{
    {char_scripts}
}};

script_code_t script_codes[] = {{
    {script_codes}
}};

script_language_t script_languages[] = {{
    {script_languages}
}};
'''

script_code_template = '{{SCRIPT_{name}, "{code}"}}'

script_language_template = '{{{num_langs}, {languages}}}'


def unicode_to_integer(u):
    return int('0x{}'.format(u), 16)


def script_name_constant(i, u):
    return u'SCRIPT_{} = {}'.format(u.upper(), i)


UNKNOWN_SCRIPT = 'Unknown'


def extract_language_scripts(xml):
    language_scripts = defaultdict(list)

    for lang in xml.xpath('//languageData/language'):
        language_code = lang.attrib['type'].lower()
        scripts = lang.get('scripts')
        if not scripts:
            continue
        for script in scripts.split():
            language_scripts[language_code].append(script)

    return language_scripts


def main(out_dir):
    response = requests.get(SCRIPTS_URL)

    # Output is a C header and data file, see templates
    out_file = open(os.path.join(out_dir, SCRIPTS_DATA_FILENAME), 'w')
    out_header = open(os.path.join(out_dir, SCRIPTS_HEADER), 'w')

    temp_dir = tempfile.gettempdir()
    script_codes_filename = os.path.join(temp_dir, ISO_15924_URL.rsplit('/')[-1])

    # This comes as a .zip
    script_codes_response = requests.get(ISO_15924_URL)
    zf = ZipFile(StringIO(script_codes_response.content))
    iso15924_filename = [name for name in zf.namelist() if name.startswith('iso15924')][0]

    # Strip out the comments, etc.
    temp_iso15924_file = u'\n'.join([line.rstrip() for line in safe_decode(zf.read(iso15924_filename)).split('\n')
                                    if line.strip() and not line.strip().startswith('#')])

    script_codes_file = StringIO(safe_encode(temp_iso15924_file))
    chars = [None] * NUM_CHARS

    # Lines look like:
    # 0041..005A    ; Latin # L&  [26] LATIN CAPITAL LETTER A..LATIN CAPITAL LETTER Z
    for char_range, script, char_class in script_regex.findall(response.content):
        script_range = [unicode_to_integer(u) for u in char_range.split('..') if len(u) < 5]
        if len(script_range) == 2:
            for i in xrange(script_range[0], script_range[1] + 1):
                chars[i] = script.upper()
        elif script_range:
            chars[script_range[0]] = script.upper()

    all_scripts = OrderedDict.fromkeys(filter(bool, chars))

    for i, script in enumerate(all_scripts.keys()):
        all_scripts[script] = i + 1

    # Unknown script for all characters not covered
    all_scripts[UNKNOWN_SCRIPT.upper()] = 0

    script_codes = {}
    seen_scripts = set()

    # Scripts in the CLDR repos use 4-letter ISO-15924 codes, so map those
    for code, _, name, _, _, _ in csv.reader(script_codes_file, delimiter=';'):
        if name.upper() in all_scripts:
            script_codes[code] = name.upper()
            seen_scripts.add(name.upper())
        else:
            normalized_name = name.split('(')[0].strip().upper()
            if normalized_name in all_scripts and normalized_name not in seen_scripts:
                script_codes[code] = normalized_name
                seen_scripts.add(normalized_name)

    # For some languages (Greek, Thai, etc.), use of an unambiguous script is sufficient
    # to identify the language. We keep track of those single_language scripts to inform
    # the language classifier

    cldr_response = requests.get(CLDR_SUPPLEMENTAL_DATA)
    cldr_xml = etree.fromstring(cldr_response.content)
    language_scripts = extract_language_scripts(cldr_xml)

    country_languages_path = os.path.join(DEFAULT_LANGUAGES_DIR, COUNTRY_LANGUAGES_FILENAME)
    if not os.path.exists(country_languages_path):
        fetch_cldr_languages(DEFAULT_LANGUAGES_DIR)

    country_language_file = open(country_languages_path)
    country_language_reader = csv.reader(country_language_file, delimiter='\t')

    spoken_languages = set([lang for country, lang, script, pct, is_official
                            in country_language_reader])

    script_languages = defaultdict(list)
    for language, scripts in language_scripts.iteritems():
        if language not in spoken_languages:
            continue
        for script in scripts:
            script_languages[script].append(language)

    single_language_scripts = set([script for script, languages in script_languages.iteritems()
                                  if len(languages) == 1])

    all_official_scripts = set.union(*(set(scripts) for language, scripts in language_scripts.iteritems()))

    max_langs = 0

    script_types = defaultdict(list)
    for script_code, script_name in script_codes.iteritems():
        langs = script_languages.get(script_code, [])
        num_langs = len(langs)
        if num_langs > max_langs:
            max_langs = num_langs
        script_types[script_name] = langs

    script_types = dict(script_types)

    for name in all_scripts.iterkeys():
        if name not in script_types:
            script_types[name] = []

    # Generate C header and constants

    script_enum = u''',
    '''.join(['SCRIPT_{} = {}'.format(s, i) for s, i in sorted(all_scripts.iteritems(), key=itemgetter(1))])

    out_header.write(scripts_header_template.format(num_chars=NUM_CHARS,
                     max_langs=max_langs,
                     script_enum=script_enum))
    out_header.close()

    # Generate C data file

    char_scripts_data = u''',
    '''.join(['SCRIPT_{}'.format(script or UNKNOWN_SCRIPT.upper()) for script in chars])

    script_codes_data = u''',
    '''.join([script_code_template.format(name=name, code=code) for code, name in script_codes.iteritems()])

    sorted_script_types = [script_types[s] for s, i in sorted(all_scripts.iteritems(), key=itemgetter(1))]

    script_language_data = u''',
    '''.join([script_language_template.format(num_langs=len(langs),
              languages='{{{}}}'.format(', '.join(['"{}"'.format(l) for l in langs])) if langs else 'NULL')
              for langs in sorted_script_types])

    out_file.write(scripts_c_data_template.format(header_name=SCRIPTS_HEADER,
                   char_scripts=char_scripts_data,
                   script_codes=script_codes_data,
                   script_languages=script_language_data))
    out_file.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print 'Usage: python scripts.py out_dir'
        sys.exit(1)

    main(sys.argv[1])
