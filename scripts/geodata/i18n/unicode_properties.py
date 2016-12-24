'''
scripts.py

This code uses the latest copy of Scripts.txt from unicode.org
to generate a C file (and header) defining which script every character
belongs to.
'''

import csv
import os
import requests
import re
import sys
import tempfile
import requests
import subprocess

from cStringIO import StringIO

from collections import OrderedDict, defaultdict
from itertools import islice

from lxml import etree

from operator import itemgetter

from zipfile import ZipFile

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.encoding import safe_encode, safe_decode
from geodata.file_utils import ensure_dir, download_file
from geodata.string_utils import NUM_CODEPOINTS, wide_unichr

from cldr_languages import *
from download_cldr import download_cldr
from languages import get_country_languages
from unicode_paths import UNICODE_DATA_DIR
from word_breaks import script_regex, regex_char_range

SRC_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir, 'src')

SCRIPTS_DATA_DIR = os.path.join(UNICODE_DATA_DIR, 'scripts')
LOCAL_SCRIPTS_FILE = os.path.join(SCRIPTS_DATA_DIR, 'Scripts.txt')
LOCAL_ISO_15924_FILE = os.path.join(SCRIPTS_DATA_DIR, 'iso15924.txt')

BLOCKS_DATA_DIR = os.path.join(UNICODE_DATA_DIR, 'blocks')
LOCAL_BLOCKS_FILE = os.path.join(BLOCKS_DATA_DIR, 'Blocks.txt')

PROPS_DATA_DIR = os.path.join(UNICODE_DATA_DIR, 'props')
LOCAL_PROPS_FILE = os.path.join(PROPS_DATA_DIR, 'PropList.txt')
LOCAL_PROP_ALIASES_FILE = os.path.join(PROPS_DATA_DIR, 'PropertyAliases.txt')
LOCAL_PROP_VALUE_ALIASES_FILE = os.path.join(PROPS_DATA_DIR, 'PropertyValueAliases.txt')
LOCAL_DERIVED_CORE_PROPS_FILE = os.path.join(PROPS_DATA_DIR, 'DerivedCoreProperties.txt')

WORD_BREAKS_DIR = os.path.join(UNICODE_DATA_DIR, 'word_breaks')
LOCAL_WORD_BREAKS_FILE = os.path.join(WORD_BREAKS_DIR, 'WordBreakProperty.txt')

SCRIPTS_HEADER = 'unicode_script_types.h'
SCRIPTS_DATA_FILENAME = 'unicode_scripts_data.c'

SCRIPTS_URL = 'http://unicode.org/Public/UNIDATA/Scripts.txt'
BLOCKS_URL = 'http://unicode.org/Public/UNIDATA/Blocks.txt'
PROPS_URL = 'http://unicode.org/Public/UNIDATA/PropList.txt'
PROP_ALIASES_URL = 'http://unicode.org/Public/UNIDATA/PropertyAliases.txt'
PROP_VALUE_ALIASES_URL = 'http://unicode.org/Public/UNIDATA/PropertyValueAliases.txt'
DERIVED_CORE_PROPS_URL = 'http://unicode.org/Public/UNIDATA/DerivedCoreProperties.txt'
WORD_BREAKS_URL = 'http://unicode.org/Public/UNIDATA/auxiliary/WordBreakProperty.txt'

ISO_15924_URL = 'http://unicode.org/iso15924/iso15924.txt.zip'

scripts_header_template = u'''#ifndef UNICODE_SCRIPT_TYPES_H
#define UNICODE_SCRIPT_TYPES_H

#include <stdlib.h>

#define NUM_CODEPOINTS {num_codepoints}
#define MAX_LANGS {max_langs}

typedef enum {{
    {script_enum}
    NUM_SCRIPTS
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

script_languages_t script_languages[] = {{
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
COMMON_SCRIPT = 'Common'


def parse_char_range(r):
    return [unicode_to_integer(u) for u in r.split('..')]


def get_chars_by_script():
    scripts_file = open(LOCAL_SCRIPTS_FILE)
    scripts = [None] * NUM_CODEPOINTS

    # Lines look like:
    # 0041..005A    ; Latin # L&  [26] LATIN CAPITAL LETTER A..LATIN CAPITAL LETTER Z
    for char_range, script, char_class in script_regex.findall(scripts_file.read()):
        script_range = parse_char_range(char_range)
        if len(script_range) == 2:
            for i in xrange(script_range[0], script_range[1] + 1):
                scripts[i] = script
        elif script_range:
            scripts[script_range[0]] = script

    return scripts


COMMENT_CHAR = '#'
DELIMITER_CHAR = ';'


def parse_file(f):
    for line in f:
        line = line.split(COMMENT_CHAR)[0].strip()
        if not line:
            continue
        tokens = line.split(DELIMITER_CHAR)
        if tokens:
            yield [t.strip() for t in tokens]


def get_property_aliases():
    prop_aliases_file = open(LOCAL_PROP_ALIASES_FILE)

    aliases = {}

    for line in parse_file(prop_aliases_file):
        prop = line[1]
        prop_aliases = [line[0]] + line[2:]

        for alias in prop_aliases:
            aliases[alias.lower()] = prop.lower()

    return aliases


def get_property_value_aliases():
    prop_value_aliases_file = open(LOCAL_PROP_VALUE_ALIASES_FILE)

    value_aliases = defaultdict(dict)

    for line in parse_file(prop_value_aliases_file):
        prop = line[0]
        if prop not in ('ccc', 'gc'):
            value = line[2]
            aliases = [line[1]] + line[3:]
        else:
            value = line[1]
            aliases = line[2:]

        for alias in aliases:
            value_aliases[prop.lower()][alias] = value

    return dict(value_aliases)


def get_unicode_blocks():
    blocks_file = open(LOCAL_BLOCKS_FILE)

    blocks = defaultdict(list)

    for line in parse_file(blocks_file):
        char_range, block = line
        char_range = parse_char_range(char_range)

        if len(char_range) == 2:
            for i in xrange(char_range[0], char_range[1] + 1):
                blocks[block.lower()].append(wide_unichr(i))
        elif char_range:
            blocks[block.lower()].append(wide_unichr(char_range[0]))

    return dict(blocks)


def get_unicode_properties():
    props_file = open(LOCAL_PROPS_FILE)

    props = defaultdict(list)

    for line in parse_file(props_file):
        char_range, prop = line

        char_range = parse_char_range(char_range)

        if len(char_range) == 2:
            for i in xrange(char_range[0], char_range[1] + 1):
                props[prop.lower()].append(wide_unichr(i))
        elif char_range:
            props[prop.lower()].append(wide_unichr(char_range[0]))

    derived_props_file = open(LOCAL_DERIVED_CORE_PROPS_FILE)
    for line in parse_file(derived_props_file):
        char_range, prop = line
        char_range = parse_char_range(char_range)

        if len(char_range) == 2:
            for i in xrange(char_range[0], char_range[1] + 1):
                props[prop.lower()].append(wide_unichr(i))
        elif char_range:
            props[prop.lower()].append(wide_unichr(char_range[0]))

    return dict(props)


def get_word_break_properties():
    props_file = open(LOCAL_WORD_BREAKS_FILE)

    props = defaultdict(list)

    for line in parse_file(props_file):
        char_range, prop = line

        char_range = parse_char_range(char_range)

        if len(char_range) == 2:
            for i in xrange(char_range[0], char_range[1] + 1):
                props[prop].append(wide_unichr(i))
        elif char_range:
            props[prop].append(wide_unichr(char_range[0]))

    return dict(props)


def build_master_scripts_list(chars):
    all_scripts = OrderedDict.fromkeys(filter(bool, chars))

    for i, script in enumerate(all_scripts.keys()):
        all_scripts[script] = i + 1

    # Unknown script for all characters not covered
    all_scripts[UNKNOWN_SCRIPT] = 0

    return all_scripts


SCRIPT_ALIASES_SUPPLEMENTAL = {
    'Hant': 'Han',
    'Hans': 'Han'
}


def get_script_codes(all_scripts):

    if not os.path.exists(LOCAL_ISO_15924_FILE):
        temp_dir = tempfile.gettempdir()

        script_codes_filename = os.path.join(temp_dir, ISO_15924_URL.rsplit('/')[-1])

        # This comes as a .zip
        script_codes_response = requests.get(ISO_15924_URL)
        zf = ZipFile(StringIO(script_codes_response.content))
        iso15924_filename = [name for name in zf.namelist() if name.startswith('iso15924')][0]

        # Strip out the comments, etc.
        temp_iso15924_file = u'\n'.join([line.rstrip() for line in safe_decode(zf.read(iso15924_filename)).split('\n')
                                        if line.strip() and not line.strip().startswith('#')])

        f = open(LOCAL_ISO_15924_FILE, 'w')
        f.write(safe_encode(temp_iso15924_file))
        f.close()

    script_codes_file = open(LOCAL_ISO_15924_FILE)

    script_codes = {}
    seen_scripts = set()

    # Scripts in the CLDR repos use 4-letter ISO-15924 codes, so map those
    for code, _, name, _, _, _ in csv.reader(script_codes_file, delimiter=';'):
        if name in all_scripts:
            script_codes[code] = name
            seen_scripts.add(name)
        else:
            normalized_name = name.split('(')[0].strip()
            if normalized_name in all_scripts and normalized_name not in seen_scripts:
                script_codes[code] = normalized_name
                seen_scripts.add(normalized_name)

    value_aliases = get_property_value_aliases()
    script_aliases = value_aliases['sc']

    for code, script in script_aliases.iteritems():
        if code not in script_codes and script in all_scripts:
            script_codes[code] = script

    script_codes.update(SCRIPT_ALIASES_SUPPLEMENTAL)

    return script_codes


SCRIPT_CODE_ALIASES = {
    'Jpan': ['Hani', 'Hira', 'Kana'],
    'Kore': ['Hang', 'Han']
}


def extract_language_scripts(xml):
    language_scripts = defaultdict(list)

    for lang in xml.xpath('//languageData/language'):
        language_code = lang.attrib['type'].lower()
        scripts = lang.get('scripts')
        if not scripts:
            continue
        for script in scripts.split():
            script_aliases = SCRIPT_CODE_ALIASES.get(script)
            if not script_aliases:
                language_scripts[language_code].append(script)
            else:
                language_scripts[language_code].extend(script_aliases)

    return language_scripts


def batch_iter(iterable, batch_size):
    source_iter = iter(iterable)
    while True:
        batch = list(islice(source_iter, batch_size))
        if len(batch) > 0:
            yield batch
        else:
            return


def get_script_languages():
    # For some languages (Greek, Thai, etc.), use of an unambiguous script is sufficient
    # to identify the language. We keep track of those single language scripts to inform
    # the language classifier

    chars = get_chars_by_script()
    all_scripts = build_master_scripts_list(chars)
    script_codes = get_script_codes(all_scripts)

    cldr_supplemental_data = open(CLDR_SUPPLEMENTAL_DATA)
    cldr_xml = etree.parse(cldr_supplemental_data)
    language_scripts = extract_language_scripts(cldr_xml)

    country_languages_path = os.path.join(DEFAULT_LANGUAGES_DIR, COUNTRY_LANGUAGES_FILENAME)
    if not os.path.exists(country_languages_path):
        fetch_cldr_languages(DEFAULT_LANGUAGES_DIR)

    country_language_file = open(country_languages_path)
    country_language_reader = csv.reader(country_language_file, delimiter='\t')

    countries = set([country for country, lang, script, pct, is_official
                     in country_language_reader])

    spoken_languages = set.union(*(set(get_country_languages(country)) for country in countries))

    script_code_languages = defaultdict(list)
    for language, scripts in language_scripts.iteritems():
        if language not in spoken_languages:
            continue
        for script in scripts:
            script_code_languages[script].append(language)

    script_languages = defaultdict(list)

    for script_code, script_name in script_codes.iteritems():
        langs = script_code_languages.get(script_code, [])
        script_languages[script_name].extend(langs)

    for name in all_scripts.iterkeys():
        script_languages.setdefault(name, [])

    return script_languages


def main(out_dir=SRC_DIR):
    # Output is a C header and data file, see templates
    out_file = open(os.path.join(out_dir, SCRIPTS_DATA_FILENAME), 'w')
    out_header = open(os.path.join(out_dir, SCRIPTS_HEADER), 'w')

    download_file(SCRIPTS_URL, LOCAL_SCRIPTS_FILE)
    download_file(BLOCKS_URL, LOCAL_BLOCKS_FILE)
    download_file(PROPS_URL, LOCAL_PROPS_FILE)
    download_file(PROP_ALIASES_URL, LOCAL_PROP_ALIASES_FILE)
    download_file(PROP_VALUE_ALIASES_URL, LOCAL_PROP_VALUE_ALIASES_FILE)
    download_file(DERIVED_CORE_PROPS_URL, LOCAL_DERIVED_CORE_PROPS_FILE)
    download_file(WORD_BREAKS_URL, LOCAL_WORD_BREAKS_FILE)

    if not os.path.exists(CLDR_SUPPLEMENTAL_DATA):
        download_cldr()

    chars = get_chars_by_script()
    all_scripts = build_master_scripts_list(chars)
    script_codes = get_script_codes(all_scripts)

    script_languages = get_script_languages()

    max_langs = 0

    for script, langs in script_languages.iteritems():
        num_langs = len(langs)
        if num_langs > max_langs:
            max_langs = num_langs

    # Generate C header and constants

    script_enum = u'''
    '''.join(['SCRIPT_{} = {},'.format(s.upper(), i) for s, i in sorted(all_scripts.iteritems(), key=itemgetter(1))])

    out_header.write(scripts_header_template.format(num_codepoints=NUM_CODEPOINTS,
                     max_langs=max_langs,
                     script_enum=script_enum))
    out_header.close()

    # Generate C data file

    char_scripts_data = u''',
    '''.join([', '.join([str(all_scripts[sc or UNKNOWN_SCRIPT]) for sc in batch]) for batch in batch_iter(chars, 25)])

    script_codes_data = u''',
    '''.join([script_code_template.format(name=name.upper(), code=code) for code, name in script_codes.iteritems()])

    sorted_lang_scripts = [script_languages[s] for s, i in sorted(all_scripts.iteritems(), key=itemgetter(1))]

    script_language_data = u''',
    '''.join([script_language_template.format(num_langs=len(langs),
              languages='{{{}}}'.format(', '.join(['"{}"'.format(l) for l in langs]) if langs else 'NULL'))
              for langs in sorted_lang_scripts])

    out_file.write(scripts_c_data_template.format(header_name=SCRIPTS_HEADER,
                   char_scripts=char_scripts_data,
                   script_codes=script_codes_data,
                   script_languages=script_language_data))
    out_file.close()


if __name__ == '__main__':
    main(*sys.argv[1:])
