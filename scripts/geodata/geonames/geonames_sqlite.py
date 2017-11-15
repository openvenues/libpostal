import os
import shutil
import sqlite3

import tempfile
import urlparse
import urllib2
import subprocess

import logging

import argparse

import csv
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.encoding import safe_decode
from geodata.geonames.paths import *

from geodata.file_utils import *
from geodata.log import *

from itertools import islice, chain

log_to_file(sys.stderr)
logger = logging.getLogger('geonames.sqlite')

BASE_URL = 'http://download.geonames.org/export/'

DUMP_URL = urlparse.urljoin(BASE_URL, 'dump/')
ALL_COUNTRIES_ZIP_FILE = 'allCountries.zip'
HIERARCHY_ZIP_FILE = 'hierarchy.zip'
ALTERNATE_NAMES_ZIP_FILE = 'alternateNames.zip'

ZIP_URL = urlparse.urljoin(BASE_URL, 'zip/')

GEONAMES_DUMP_FILES = (ALL_COUNTRIES_ZIP_FILE,
                       HIERARCHY_ZIP_FILE,
                       ALTERNATE_NAMES_ZIP_FILE)

# base_url, local_dir, is_gzipped, local_filename


GEONAMES_FILES = [(DUMP_URL, '', True, ALL_COUNTRIES_ZIP_FILE),
                  (DUMP_URL, '', True, HIERARCHY_ZIP_FILE),
                  (DUMP_URL, '', True, ALTERNATE_NAMES_ZIP_FILE),
                  (ZIP_URL, 'zip', True, ALL_COUNTRIES_ZIP_FILE),
                  ]


def download_file(url, dest):
    logger.info('Downloading file from {}'.format(url))
    subprocess.check_call(['wget', url, '-O', dest])


def admin_ddl(admin_level):
    columns = ['country_code TEXT'] + \
              ['admin{}_code TEXT'.format(i)
               for i in xrange(1, admin_level)]

    create = '''
    CREATE TABLE admin{level}_codes (
    geonames_id INT,
    code TEXT,
    name TEXT,
    {fields}
    )'''.format(level=admin_level,
                fields=''',
    '''.join(columns))

    indices = (
        '''CREATE INDEX admin{}_code_index ON
        admin{}_codes (code)'''.format(admin_level, admin_level),
        '''CREATE INDEX admin{}_gn_id_index ON
        admin{}_codes (geonames_id)'''.format(admin_level, admin_level),
    )

    return (create, ) + indices

geonames_ddl = {
    'geonames': (
        '''CREATE TABLE geonames (
        geonames_id INT PRIMARY KEY,
        name TEXT,
        ascii_name TEXT,
        alternate_names TEXT,
        latitude DOUBLE,
        longitude DOUBLE,
        feature_class TEXT,
        feature_code TEXT,
        country_code TEXT,
        cc2 TEXT,
        admin1_code TEXT,
        admin2_code TEXT,
        admin3_code TEXT,
        admin4_code TEXT,
        population LONG DEFAULT 0,
        elevation INT,
        dem INT,
        timezone TEXT,
        modification_date TEXT)''',
        '''CREATE INDEX feature_code ON
        geonames (feature_code)''',
        '''CREATE INDEX country_code ON
        geonames (country_code)''',
        '''CREATE INDEX admin_codes ON
        geonames (country_code, admin1_code, admin2_code, admin3_code, admin4_code)''',
    ),

    'alternate_names': (
        '''CREATE TABLE alternate_names (
        alternate_name_id INT PRIMARY KEY,
        geonames_id INT,
        iso_language TEXT,
        alternate_name TEXT,
        is_preferred_name BOOLEAN DEFAULT 0,
        is_short_name BOOLEAN DEFAULT 0,
        is_colloquial BOOLEAN DEFAULT 0,
        is_historic BOOLEAN DEFAULT 0)''',
        '''CREATE INDEX geonames_id_index ON
        alternate_names (geonames_id)''',
        '''CREATE INDEX geonames_id_alt_name_index ON
        alternate_names(geonames_id, alternate_name)''',
    ),

    'hierarchy': (
        '''CREATE TABLE hierarchy (
        parent_id INT,
        child_id INT,
        type TEXT
        );''',
        '''CREATE INDEX parent_child_index ON
        hierarchy (parent_id, child_id)''',
        '''CREATE INDEX child_parent_index ON
        hierarchy (child_id, parent_id)''',
    ),

    'postal_codes': (
        '''CREATE TABLE postal_codes (
        country_code TEXT,
        postal_code TEXT,
        place_name TEXT,
        admin1 TEXT,
        admin1_code TEXT,
        admin2 TEXT,
        admin2_code TEXT,
        admin3 TEXT,
        admin3_code TEXT,
        latitude DOUBLE,
        longitude DOUBLE,
        accuracy INT
        )''',
        '''CREATE INDEX post_code_index ON
        postal_codes (country_code, postal_code)''',
        '''CREATE INDEX postal_code_admins ON
        postal_codes (country_code, admin1_code, admin2_code, admin3_code)''',
    ),
    'admin1_codes': admin_ddl(1),
    'admin2_codes': admin_ddl(2),
    'admin3_codes': admin_ddl(3),
    'admin4_codes': admin_ddl(4),

}

geonames_file_table_map = {
    ('', ALL_COUNTRIES_ZIP_FILE): 'geonames',
    ('', ALTERNATE_NAMES_ZIP_FILE): 'alternate_names',
    ('', HIERARCHY_ZIP_FILE): 'hierarchy',
    ('zip', ALL_COUNTRIES_ZIP_FILE): 'postal_codes',
}


country_codes_create_table = (
    'drop table if exists country_codes',
    '''
    create table country_codes as
    select distinct country_code from geonames
    where feature_code in ('PCL', 'PCLI', 'PCLIX', 'PCLD', 'PCLF', 'PCLS', 'TERR')
    ''',
)

proper_countries_create_table = (
    'drop table if exists proper_countries',
    '''
    create table proper_countries as
    select * from geonames
    where feature_code in ('PCL', 'PCLI', 'PCLIX', 'PCLD', 'PCLF', 'PCLS')
    and country_code in (select country_code from country_codes)
    ''',
)

territories_create_table = (
    'drop table if exists territories',
    '''
    create table territories as
    select * from geonames where feature_code = 'TERR'
    and country_code not in (select country_code from proper_countries);
    ''',
)

countries_create_table = (
    'drop table if exists countries',
    '''
    create table countries as
    select * from proper_countries
    union
    select * from territories;
    ''',
    'create index country_geonames_id on countries (geonames_id)',
    'create index conntry_country_code on countries (country_code)',
)

country_alises_create_table = (
    'drop table if exists country_aliases',
    '''
    create table country_aliases as
    select name, country_code
    from countries
    union
    select alternate_name, country_code
    from alternate_names an
    join countries c
        on c.geonames_id = an.geonames_id
    where alternate_name != ''
    and iso_language not in ('doi','faac','iata',
                             'icao','link','post','tcid')
    '''
)

country_table_create_statements = list(chain(country_codes_create_table,
                                             proper_countries_create_table,
                                             territories_create_table,
                                             countries_create_table,
                                             country_alises_create_table))


def create_table(conn, table):
    cursor = conn.cursor()
    create_statements = geonames_ddl[table]
    cursor.execute('DROP TABLE IF EXISTS {}'.format(table))
    for statement in create_statements:
        cursor.execute(statement)
    conn.commit()


def batch_iter(iterable, batch_size):
    source_iter = iter(iterable)
    while True:
        batch = list(islice(source_iter, batch_size))
        if len(batch) > 0:
            yield batch
        else:
            return


def populate_admin_table(conn, admin_level):
    logging.info('Doing admin level {}'.format(admin_level))

    columns = ['geonames_id',
               'admin{}_code'.format(admin_level),
               'name',
               'country_code']
    columns.extend(['admin{}_code'.format(i)
                    for i in xrange(1, admin_level)])

    admin_insert_statement = '''
    insert into "admin{}_codes"
    select {}
    from geonames
    where feature_code = "ADM{}"
    '''.format(admin_level, ','.join(columns), admin_level)

    conn.execute(admin_insert_statement)
    conn.commit()

    logging.info('Done with admin level {}'.format(admin_level))


def import_geonames_table(conn, table, f, batch_size=2000):
    # escape the brackets around the values format string so we can use later
    statement = 'INSERT INTO "{}" VALUES ({{}})'.format(table)
    cursor = conn.cursor()
    for i, batch in enumerate(batch_iter(f, batch_size)):
        num_cols = len(batch[0])
        cursor.executemany(statement.format(','.join(['?'] * num_cols)), batch)
        conn.commit()
        cursor = conn.cursor()
        logging.info('imported {} batches ({} records)'.format(i + 1, (i + 1) * batch_size))
    cursor.close()


def create_geonames_sqlite_db(temp_dir, db_file=DEFAULT_GEONAMES_DB_PATH):
    conn = sqlite3.connect(db_file)
    logging.info('Created database at {}'.format(db_file))
    for url, directory, is_gzipped, filename in GEONAMES_FILES:
        table = geonames_file_table_map[(directory, filename)]
        create_table(conn, table)
        full_url = urlparse.urljoin(url, filename)
        dest_dir = os.path.join(temp_dir, directory)
        ensure_dir(dest_dir)
        dest_file = os.path.join(dest_dir, filename)
        download_file(full_url, dest_file)
        if is_gzipped:
            unzip_file(dest_file, dest_dir)
            filename = dest_file.replace('.zip', '.txt')
        reader = csv.reader(open(filename), delimiter='\t', quotechar=None)
        lines = (map(safe_decode, line) for line in reader)
        import_geonames_table(conn, table, lines)
    logging.info('Creating countries tables')
    for statement in country_table_create_statements:
        conn.execute(statement)
        conn.commit()
    logging.info('Creating admin tables')
    for admin_level in xrange(1, 5):
        create_table(conn, 'admin{}_codes'.format(admin_level))
        populate_admin_table(conn, admin_level)
    conn.close()


if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--temp-dir',
                        default=tempfile.gettempdir(),
                        help='Temporary work directory')
    parser.add_argument('-o', '--out',
                        default=DEFAULT_GEONAMES_DB_PATH,
                        help='SQLite3 db filename')
    args = parser.parse_args()
    create_geonames_sqlite_db(args.temp_dir, args.out)
