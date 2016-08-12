'''
create_geonames_tsv.py
----------------------

This script formats the open GeoNames database (as well as
its accompanying postal codes data set) into a schema'd
tab-separated value file.

It generates a C header which uses an enum for the field names.
This way if new fields are added or there's a typo, etc. the
error will show up at compile-time.

The relevant C modules which operate on this data are:
    geodb_builder.c
    geonames.c

As well as the generated headers:
    geonames_fields.h
    postal_fields.h
'''

import argparse
import csv
import logging
import operator
import os
import re
import sqlite3
import subprocess
import sys

import pycountry

import unicodedata

import urllib
import urlparse

from collections import defaultdict, OrderedDict
from lxml import etree

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.csv_utils import *
from geodata.file_utils import *
from geodata.countries.country_names import *
from geodata.encoding import safe_encode, safe_decode
from geodata.geonames.paths import DEFAULT_GEONAMES_DB_PATH
from geodata.i18n.languages import *
from geodata.i18n.unicode_paths import CLDR_DIR
from geodata.log import log_to_file

multispace_regex = re.compile('[\s]+')


def encode_field(value):
    return multispace_regex.sub(' ', safe_encode((value if value is not None else '')))

log_to_file(sys.stderr)

DEFAULT_DATA_DIR = os.path.join(this_dir, os.path.pardir, os.path.pardir,
                                os.path.pardir, 'data', 'geonames')

COUNTRY_FEATURE_CODES = ('PCL', 'PCLI', 'PCLIX', 'PCLD', 'PCLF', 'PCLS')
CONTINENT_FEATURE_CODES = ('CONT',)

ADMIN_1_FEATURE_CODES = ('ADM1',)
ADMIN_2_FEATURE_CODES = ('ADM2',)
ADMIN_3_FEATURE_CODES = ('ADM3',)
ADMIN_4_FEATURE_CODES = ('ADM4',)
OTHER_ADMIN_FEATURE_CODES = ('ADM5',)
ADMIN_OTHER_FEATURE_CODES = ('ADMD', )

POPULATED_PLACE_FEATURE_CODES = ('PPL', 'PPLA', 'PPLA2', 'PPLA3', 'PPLA4',
                                 'PPLC', 'PPLCH', 'PPLF', 'PPLG', 'PPLL',
                                 'PPLR', 'PPLS', 'STLMT')
NEIGHBORHOOD_FEATURE_CODES = ('PPLX', )


class boundary_types:
    COUNTRY = 0
    ADMIN1 = 1
    ADMIN2 = 2
    ADMIN3 = 3
    ADMIN4 = 4
    ADMIN_OTHER = 5
    LOCALITY = 6
    NEIGHBORHOOD = 7

geonames_admin_dictionaries = OrderedDict([
    (boundary_types.COUNTRY, COUNTRY_FEATURE_CODES),
    (boundary_types.ADMIN1, ADMIN_1_FEATURE_CODES),
    (boundary_types.ADMIN2, ADMIN_2_FEATURE_CODES),
    (boundary_types.ADMIN3, ADMIN_3_FEATURE_CODES),
    (boundary_types.ADMIN4, ADMIN_4_FEATURE_CODES),
    (boundary_types.ADMIN_OTHER, ADMIN_OTHER_FEATURE_CODES),
    (boundary_types.LOCALITY, POPULATED_PLACE_FEATURE_CODES),
    (boundary_types.NEIGHBORHOOD, NEIGHBORHOOD_FEATURE_CODES),
])

# Inserted post-query
DUMMY_BOUNDARY_TYPE = '-1 as type'
DUMMY_HAS_WIKIPEDIA_ENTRY = '0 as has_wikipedia_entry'
DUMMY_LANGUAGE_PRIORITY = '0 as language_priority'


class GeonamesField(object):
    def __init__(self, name, c_constant, default=None, is_dummy=False):
        self.name = name
        self.c_constant = c_constant
        self.default = default
        self.is_dummy = is_dummy

geonames_fields = [
    # Field if alternate_names present, default field name if not, C header constant
    GeonamesField('alternate_name', 'GEONAMES_NAME', default='gn.name'),
    GeonamesField('gn.geonames_id as geonames_id', 'GEONAMES_ID'),
    GeonamesField('gn.name as canonical', 'GEONAMES_CANONICAL'),
    GeonamesField(DUMMY_BOUNDARY_TYPE, 'GEONAMES_BOUNDARY_TYPE', is_dummy=True),
    GeonamesField(DUMMY_HAS_WIKIPEDIA_ENTRY, 'GEONAMES_HAS_WIKIPEDIA_ENTRY', is_dummy=True),
    GeonamesField('iso_language', 'GEONAMES_ISO_LANGUAGE', default="''"),
    GeonamesField(DUMMY_LANGUAGE_PRIORITY, 'GEONAMES_LANGUAGE_PRIORITY', is_dummy=True),
    GeonamesField('is_preferred_name', 'GEONAMES_IS_PREFERRED_NAME', default='0'),
    GeonamesField('is_short_name', 'GEONAMES_IS_SHORT_NAME', default='0'),
    GeonamesField('is_colloquial', 'GEONAMES_IS_COLLOQUIAL', default='0'),
    GeonamesField('is_historic', 'GEONAMES_IS_HISTORICAL', default='0'),
    GeonamesField('gn.population', 'GEONAMES_POPULATION'),
    GeonamesField('gn.latitude', 'GEONAMES_LATITUDE'),
    GeonamesField('gn.longitude', 'GEONAMES_LONGITUDE'),
    GeonamesField('gn.feature_code', 'GEONAMES_FEATURE_CODE'),
    GeonamesField('gn.country_code as country_code', 'GEONAMES_COUNTRY_CODE'),
    GeonamesField('c.geonames_id as country_gn_id', 'GEONAMES_COUNTRY_ID'),
    GeonamesField('gn.admin1_code as admin1_code', 'GEONAMES_ADMIN1_CODE'),
    GeonamesField('a1.geonames_id as a1_gn_id', 'GEONAMES_ADMIN1_ID'),
    GeonamesField('gn.admin2_code as admin2_code', 'GEONAMES_ADMIN2_CODE'),
    GeonamesField('a2.geonames_id as a2_gn_id', 'GEONAMES_ADMIN2_ID'),
    GeonamesField('gn.admin3_code as admin3_code', 'GEONAMES_ADMIN3_CODE'),
    GeonamesField('a3.geonames_id as a3_gn_id', 'GEONAMES_ADMIN3_ID'),
    GeonamesField('gn.admin4_code as admin4_code', 'GEONAMES_ADMIN4_CODE'),
    GeonamesField('a4.geonames_id as a4_gn_id', 'GEONAMES_ADMIN4_ID'),
]

def geonames_field_index(s):
    for i, f in enumerate(geonames_fields):
        if f.c_constant == s:
            return i
    return None


DUMMY_BOUNDARY_TYPE_INDEX = geonames_field_index('GEONAMES_BOUNDARY_TYPE')
DUMMY_HAS_WIKIPEDIA_ENTRY_INDEX = geonames_field_index('GEONAMES_HAS_WIKIPEDIA_ENTRY')

GEONAMES_ID_INDEX = geonames_field_index('GEONAMES_ID')
LANGUAGE_INDEX = geonames_field_index('GEONAMES_ISO_LANGUAGE')

DUMMY_LANGUAGE_PRIORITY_INDEX = geonames_field_index('GEONAMES_LANGUAGE_PRIORITY')

CANONICAL_NAME_INDEX = geonames_field_index('GEONAMES_CANONICAL')

NAME_INDEX = geonames_field_index('GEONAMES_NAME')

COUNTRY_CODE_INDEX = geonames_field_index('GEONAMES_COUNTRY_CODE')

POPULATION_INDEX = geonames_field_index('GEONAMES_POPULATION')

PREFERRED_INDEX = geonames_field_index('GEONAMES_IS_PREFERRED_NAME')

HISTORICAL_INDEX = geonames_field_index('GEONAMES_IS_HISTORICAL')


geonames_admin_joins = '''
left join admin1_codes a1
    on a1.code = gn.admin1_code
    and a1.country_code = gn.country_code
left join admin2_codes a2
    on a2.code = gn.admin2_code
    and a2.admin1_code = gn.admin1_code
    and a2.country_code = gn.country_code
left join admin3_codes a3
    on a3.code = gn.admin3_code
    and a3.admin1_code = gn.admin1_code
    and a3.admin2_code = gn.admin2_code
    and a3.country_code = gn.country_code
left join admin4_codes a4
    on a4.code = gn.admin4_code
    and a4.admin1_code = gn.admin1_code
    and a4.admin2_code = gn.admin2_code
    and a4.admin3_code = gn.admin3_code
    and a4.country_code = gn.country_code
'''

# Canonical names are stored in the geonames table with alternates
# stored in a separate table. UNION ALL query will capture them all.

base_geonames_query = '''
select {geonames_fields}
from geonames gn
join countries c
    on gn.country_code = c.country_code
{admin_joins}
{{predicate}}
union all
select {alt_name_fields}
from geonames gn
join countries c
    on gn.country_code = c.country_code
join alternate_names an
    on an.geonames_id = gn.geonames_id
    and iso_language not in ('doi','faac','iata',
                             'icao','link','post','tcid')
    and an.alternate_name != gn.name
{admin_joins}
{{predicate}}
'''.format(
    geonames_fields=', '.join((f.name if f.default is None else
                               '{} as {}'.format(f.default, f.name)
                               for f in geonames_fields)),
    alt_name_fields=', '.join((f.name for f in geonames_fields)),
    admin_joins=geonames_admin_joins
)

IGNORE_COUNTRY_POSTAL_CODES = set([
    'AR',   # GeoNames has pre-1999 postal codes
])

postal_code_fields = [
    GeonamesField('postal_code', 'GN_POSTAL_CODE'),
    GeonamesField('p.country_code as country_code', 'GN_POSTAL_COUNTRY_CODE'),
    GeonamesField('c.geonames_id as country_geonames_id', 'GN_POSTAL_COUNTRY_GEONAMES_ID'),
    GeonamesField('c.population as country_population', 'GN_POSTAL_COUNTRY_POPULATION'),
    GeonamesField('n.geonames_id as containing_geoname_id', 'GN_POSTAL_CONTAINING_GEONAME_ID'),
    GeonamesField('group_concat(distinct a1.geonames_id) admin1_ids', 'GN_POSTAL_ADMIN1_IDS'),
    GeonamesField('group_concat(distinct a2.geonames_id) admin2_ids', 'GN_POSTAL_ADMIN2_IDS'),
    GeonamesField('group_concat(distinct a3.geonames_id) admin3_ids', 'GN_POSTAL_ADMIN3_IDS'),
]

def postal_code_field_index(s):
    for i, f in enumerate(postal_code_fields):
        if f.c_constant == s:
            return i
    return None

POSTAL_CODE_INDEX = postal_code_field_index('GN_POSTAL_CODE')
POSTAL_CODE_POP_INDEX = postal_code_field_index('GN_POSTAL_COUNTRY_POPULATION')

postal_codes_query = '''
select
{fields}
from postal_codes p
join countries c
    on p.country_code = c.country_code
left join (
    select
    gn.geonames_id,
    alternate_name,
    country_code,
    gn.name
    from alternate_names an
    join geonames gn
        on an.geonames_id = gn.geonames_id
    where iso_language = 'post'
) as n
on p.postal_code = n.alternate_name
and p.country_code = n.country_code
left join admin1_codes a1
    on a1.code = p.admin1_code
    and p.country_code = a1.country_code
left join admin2_codes a2
    on a2.code = p.admin2_code
    and a2.admin1_code = p.admin1_code
    and a2.country_code = p.country_code
left join admin3_codes a3
    on a3.code = p.admin3_code
    and a3.admin1_code = p.admin1_code
    and a3.admin2_code = p.admin2_code
    and a3.country_code = p.country_code
where p.country_code not in ({exclude_country_codes})
group by postal_code, p.country_code
'''.format(
    fields=','.join([f.name for f in postal_code_fields]),
    exclude_country_codes=','.join("'{}'".format(code) for code in IGNORE_COUNTRY_POSTAL_CODES))


wikipedia_query = '''
select alternate_name, geonames_id, is_preferred_name
from alternate_names
where iso_language = 'link'
and alternate_name like '%%en.wikipedia%%'
order by alternate_name, is_preferred_name
'''

BATCH_SIZE = 2000


wiki_paren_regex = re.compile('(.*)[\s]*\(.*?\)[\s]*')


def normalize_wikipedia_title(title):
    return safe_decode(title).replace(u'_', u' ')


def normalize_wikipedia_url(url):
    url = urllib.unquote_plus(url)

    parsed = urlparse.urlsplit(url)
    if parsed.query:
        params = urlparse.parse_qs(parsed.query)
        if 'title' in params:
            return normalize_wikipedia_title(params['title'][0])

    title = parsed.path.rsplit('/', 1)[-1]
    if title not in ('index.php', 'index.html'):
        return normalize_wikipedia_title(title)

    return None


def normalize_name(name):
    name = name.replace('&', 'and')
    name = name.replace('-', ' ')
    name = name.replace(', ', ' ')
    name = name.replace(',', ' ')
    return name


saint_replacements = [
    ('st.', 'saint'),
    ('st.', 'st'),
    ('st', 'saint')
]


abbreviated_saint_regex = re.compile(r'\bSt(\.|\b)')


def normalize_display_name(name):
    return abbreviated_saint_regex.sub('Saint', name).replace('&', 'and')


def utf8_normalize(s, form='NFD'):
    return unicodedata.normalize(form, s)


def get_wikipedia_titles(db):
    d = defaultdict(dict)

    cursor = db.execute(wikipedia_query)

    while True:
        batch = cursor.fetchmany(BATCH_SIZE)
        if not batch:
            break

        for (url, geonames_id, is_preferred) in batch:
            title = normalize_wikipedia_url(safe_encode(url))
            if title is not None and title.strip():
                title = utf8_normalize(normalize_name(title))
                d[title.lower()][geonames_id] = int(is_preferred or 0)

    return d


def create_geonames_tsv(db, out_dir=DEFAULT_DATA_DIR):
    '''
    Writes geonames.tsv using the specified db to the specified data directory
    '''
    filename = os.path.join(out_dir, 'geonames.tsv')
    temp_filename = filename + '.tmp'

    f = open(temp_filename, 'w')

    writer = csv.writer(f, 'tsv_no_quote')

    init_languages()

    init_country_names()

    wiki_titles = get_wikipedia_titles(db)
    logging.info('Fetched Wikipedia titles')

    # Iterate over GeoNames boundary types from largest (country) to smallest (neighborhood)
    for boundary_type, codes in geonames_admin_dictionaries.iteritems():
        if boundary_type != boundary_types.COUNTRY:
            predicate = 'where gn.feature_code in ({codes})'.format(
                codes=','.join(['"{}"'.format(c) for c in codes])
            )
        else:
            # The query for countries in GeoNames is somewhat non-trivial
            predicate = 'where gn.geonames_id in (select geonames_id from countries)'

        query = base_geonames_query.format(
            predicate=predicate
        )

        cursor = db.execute(query)
        i = 1
        while True:
            # Fetch rows in batches to save memory
            batch = cursor.fetchmany(BATCH_SIZE)
            if not batch:
                break
            rows = []
            for row in batch:
                row = list(row)
                row[DUMMY_BOUNDARY_TYPE_INDEX] = boundary_type

                language = row[LANGUAGE_INDEX]

                country_code = row[COUNTRY_CODE_INDEX]

                is_preferred = int(row[PREFERRED_INDEX] or 0)
                is_historical = int(row[HISTORICAL_INDEX] or 0)

                lang_spoken = get_country_languages(country_code.lower(), official=False).get(language, None)
                lang_official = get_country_languages(country_code.lower()).get(language, None) == 1
                null_language = not language.strip()

                is_canonical = row[NAME_INDEX] == row[CANONICAL_NAME_INDEX]

                alpha2_code = None
                is_orig_name = False

                if boundary_type == boundary_types.COUNTRY:
                    alpha2_code = row[COUNTRY_CODE_INDEX]

                    is_orig_name = row[NAME_INDEX] == row[CANONICAL_NAME_INDEX] and row[LANGUAGE_INDEX] == ''
                    # Set the canonical for countries to the local name, see country_official_name in country_names.py
                    country_canonical = country_localized_display_name(alpha2_code.lower())
                    if not country_canonical or not country_canonical.strip():
                        raise ValueError('Could not get local canonical name for country code={}'.format(alpha2_code))
                    row[CANONICAL_NAME_INDEX] = country_canonical

                geonames_id = row[GEONAMES_ID_INDEX]

                name = utf8_normalize(safe_decode(row[NAME_INDEX]))

                # For non-postal codes, don't count
                if name.isdigit():
                    continue

                wikipedia_entries = wiki_titles.get(name.lower(), wiki_titles.get(normalize_name(name.lower()), {}))

                row[NAME_INDEX] = name

                if boundary_type == boundary_types.COUNTRY:
                    norm_name = normalize_name(name.lower())
                    for s, repl in saint_replacements:
                        if not wikipedia_entries:
                            wikipedia_entries = wiki_titles.get(norm_name.replace(s, repl), {})

                wiki_row = []

                have_wikipedia = geonames_id in wikipedia_entries
                wiki_preferred = wikipedia_entries.get(geonames_id, 0)

                '''
                The following set of heuristics assigns a numerical value to a given name
                alternative, such that in the case of ambiguous names, this value can be
                used as part of the ranking function (as indeed it will be during sort).
                The higher the value, the more likely the given entity resolution.
                '''
                if is_historical:
                    # Historical names, unlikely to be used
                    language_priority = 0
                elif not null_language and language != 'abbr' and lang_spoken is None:
                    # Name of a place in language not widely spoken e.g. Japanese name for a US toponym
                    language_priority = 1
                elif null_language and not is_preferred and not is_canonical:
                    # Null-language alternate names not marked as preferred, dubious
                    language_priority = 2
                elif language == 'abbr' and not is_preferred:
                    # Abbreviation, not preferred
                    language_priority = 3
                elif language == 'abbr' and is_preferred:
                    # Abbreviation, preferred e.g. NYC, UAE
                    language_priority = 4
                elif lang_spoken and not lang_official and not is_preferred:
                    # Non-preferred name but in a spoken (non-official) language
                    language_priority = 5
                elif lang_official == 1 and not is_preferred:
                    # Name in an official language, not preferred
                    language_priority = 6
                elif null_language and not is_preferred and is_canonical:
                    # Canonical name, may be overly official e.g. Islamic Republic of Pakistan
                    language_priority = 7
                elif is_preferred and not lang_official:
                    # Preferred names, not an official language
                    language_priority = 8
                elif is_preferred and lang_official:
                    # Official language preferred
                    language_priority = 9

                row[DUMMY_LANGUAGE_PRIORITY_INDEX] = language_priority

                if have_wikipedia:
                    wiki_row = row[:]
                    wiki_row[DUMMY_HAS_WIKIPEDIA_ENTRY_INDEX] = wiki_preferred + 1
                    rows.append(map(encode_field, wiki_row))

                canonical = utf8_normalize(safe_decode(row[CANONICAL_NAME_INDEX]))
                row[POPULATION_INDEX] = int(row[POPULATION_INDEX] or 0)

                have_normalized = False

                if is_orig_name:
                    canonical_row = wiki_row[:] if have_wikipedia else row[:]

                    canonical_row_name = normalize_display_name(name)
                    if canonical_row_name != name:
                        canonical_row[NAME_INDEX] = safe_encode(canonical_row_name)
                        have_normalized = True
                        rows.append(map(encode_field, canonical_row))

                if not have_wikipedia:
                    rows.append(map(encode_field, row))

                # Country names have more specialized logic
                if boundary_type == boundary_types.COUNTRY:
                    wikipedia_entries = wiki_titles.get(canonical.lower(), {})

                    canonical_row_name = normalize_display_name(canonical)

                    canonical_row = row[:]

                    if is_orig_name:
                        canonical = safe_decode(canonical)
                        canonical_row[NAME_INDEX] = safe_encode(canonical)

                        norm_name = normalize_name(canonical.lower())
                        for s, repl in saint_replacements:
                            if not wikipedia_entries:
                                wikipedia_entries = wiki_titles.get(norm_name.replace(s, repl), {})

                        if not wikipedia_entries:
                            norm_name = normalize_name(canonical_row_name.lower())
                            for s, repl in saint_replacements:
                                if not wikipedia_entries:
                                    wikipedia_entries = wiki_titles.get(norm_name.replace(s, repl), {})

                        have_wikipedia = geonames_id in wikipedia_entries
                        wiki_preferred = wikipedia_entries.get(geonames_id, 0)

                        if have_wikipedia:
                            canonical_row[DUMMY_HAS_WIKIPEDIA_ENTRY_INDEX] = wiki_preferred + 1

                        if (name != canonical):
                            rows.append(map(encode_field, canonical_row))

                    if canonical_row_name != canonical and canonical_row_name != name:
                        canonical_row[NAME_INDEX] = safe_encode(canonical_row_name)
                        rows.append(map(encode_field, canonical_row))

                    if alpha2_code and is_orig_name:
                        alpha2_row = row[:]
                        alpha2_row[NAME_INDEX] = alpha2_code
                        alpha2_row[DUMMY_LANGUAGE_PRIORITY_INDEX] = 10
                        rows.append(map(encode_field, alpha2_row))

                    if alpha2_code.lower() in country_alpha3_map and is_orig_name:
                        alpha3_row = row[:]
                        alpha3_row[NAME_INDEX] = country_code_alpha3_map[alpha2_code.lower()]
                        alpha3_row[DUMMY_LANGUAGE_PRIORITY_INDEX] = 10
                        rows.append(map(encode_field, alpha3_row))

            writer.writerows(rows)
            logging.info('Did {} batches'.format(i))
            i += 1

        cursor.close()
        f.flush()

    f.close()

    logging.info('Sorting...')

    env = os.environ.copy()
    env['LC_ALL'] = 'C'

    command = ['sort', '-t\t', '-u', '--ignore-case',
               '-k{0},{0}'.format(NAME_INDEX + 1),
               # If there's a Wikipedia link to this name for the given id, sort first
               '-k{0},{0}nr'.format(DUMMY_HAS_WIKIPEDIA_ENTRY_INDEX + 1),
               # Language priority rules as above
               '-k{0},{0}nr'.format(DUMMY_LANGUAGE_PRIORITY_INDEX + 1),
               # Sort descending by population (basic proxy for relevance)
               '-k{0},{0}nr'.format(POPULATION_INDEX + 1),
               # group rows for the same geonames ID together
               '-k{0},{0}'.format(GEONAMES_ID_INDEX + 1),
               # preferred names come first within that grouping
               '-k{0},{0}nr'.format(PREFERRED_INDEX + 1),
               # since uniquing is done on the sort key, add language
               '-k{0},{0}'.format(LANGUAGE_INDEX + 1),
               '-o', filename, temp_filename]

    p = subprocess.Popen(command, env=env)

    return_code = p.wait()
    if return_code != 0:
        raise subprocess.CalledProcessError(return_code, command)

    os.unlink(temp_filename)


def create_postal_codes_tsv(db, out_dir=DEFAULT_DATA_DIR):
    filename = os.path.join(out_dir, 'postal_codes.tsv')
    temp_filename = filename + '.tmp'
    f = open(temp_filename, 'w')

    writer = csv.writer(f, 'tsv_no_quote')

    cursor = db.execute(postal_codes_query)

    i = 1
    while True:
        batch = cursor.fetchmany(BATCH_SIZE)
        if not batch:
            break
        rows = [
            map(encode_field, row)
            for row in batch
        ]
        writer.writerows(rows)
        logging.info('Did {} batches'.format(i))
        i += 1

    cursor.close()
    f.close()

    logging.info('Sorting...')

    subprocess.check_call([
        'sort', '-t\t', '--ignore-case',
        '-k{0},{0}'.format(POSTAL_CODE_INDEX + 1),
        '-k{0},{0}nr'.format(POSTAL_CODE_POP_INDEX + 1),
        '-o', filename,
        temp_filename
    ])
    os.unlink(temp_filename)

# Generates a C header telling us the order of the fields as written
GEONAMES_FIELDS_HEADER = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                      'src', 'geonames_fields.h')

GEONAMES_FIELDS_HEADER_FILE = '''enum geonames_fields {{
    {fields},
    NUM_GEONAMES_FIELDS
}};
'''.format(fields=''',
    '''.join(['{}={}'.format(f.c_constant, i) for i, f in enumerate(geonames_fields)]))


def write_geonames_fields_header(filename=GEONAMES_FIELDS_HEADER):
    with open(filename, 'w') as f:
        f.write(GEONAMES_FIELDS_HEADER_FILE)

POSTAL_FIELDS_HEADER = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                    'src', 'postal_fields.h')

POSTAL_FIELDS_HEADER_FILE = '''enum gn_postal_fields {{
    {fields},
    NUM_POSTAL_FIELDS
}};
'''.format(fields=''',
    '''.join(['{}={}'.format(f.c_constant, i) for i, f in enumerate(postal_code_fields)]))


def write_postal_fields_header(filename=POSTAL_FIELDS_HEADER):
    with open(filename, 'w') as f:
        f.write(POSTAL_FIELDS_HEADER_FILE)


if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--db',
                        default=DEFAULT_GEONAMES_DB_PATH,
                        help='SQLite db file')
    parser.add_argument('-o', '--out',
                        default=DEFAULT_DATA_DIR, help='output directory')
    args = parser.parse_args()
    db = sqlite3.connect(args.db)

    create_geonames_tsv(db, args.out)
    create_postal_codes_tsv(db, args.out)
    write_geonames_fields_header()
    write_postal_fields_header()
    db.close()
