import argparse
import csv
import os
import sqlite3
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.file_utils import *
from geodata.encoding import safe_encode
from geodata.geonames.geonames_sqlite import DEFAULT_GEONAMES_DB_PATH

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

geonames_admin_dictionaries = {
    boundary_types.COUNTRY: COUNTRY_FEATURE_CODES,
    boundary_types.ADMIN1: ADMIN_1_FEATURE_CODES,
    boundary_types.ADMIN2: ADMIN_2_FEATURE_CODES,
    boundary_types.ADMIN3: ADMIN_3_FEATURE_CODES,
    boundary_types.ADMIN4: ADMIN_4_FEATURE_CODES,
    boundary_types.ADMIN_OTHER: ADMIN_OTHER_FEATURE_CODES,
    boundary_types.LOCALITY: POPULATED_PLACE_FEATURE_CODES,
    boundary_types.NEIGHBORHOOD: NEIGHBORHOOD_FEATURE_CODES,
}

# Append new fields to the end for compatibility

geonames_fields = [
    'ifnull(alternate_name, gn.name) as alternate_name',
    'gn.geonames_id as geonames_id',
    'gn.name as name',
    'iso_language',
    'is_preferred_name',
    'population',
    'latitude',
    'longitude',
    'feature_code',
    'gn.country_code as country_code',
    'gn.admin1_code as admin1_code',
    'a1.geonames_id as a1_gn_id',
    'gn.admin2_code as admin2_code',
    'a2.geonames_id as a2_gn_id',
    'gn.admin3_code as admin3_code',
    'a3.geonames_id as a3_gn_id',
    'gn.admin4_code as admin4_code',
    'a4.geonames_id as a4_gn_id',
]

base_geonames_query = '''
select {fields}
from geonames gn
left join alternate_names an
    on gn.geonames_id = an.geonames_id
    and iso_language not in ('doi','faac','iata',
                             'icao','link','post','tcid')
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
{predicate}'''

IGNORE_COUNTRY_POSTAL_CODES = set([
    'AR',   # GeoNames has pre-1999 postal codes
])

postal_codes_query = '''
select
postal_code,
p.country_code as country_code,
n.geonames_id is not null as have_containing_geoname,
n.geonames_id as containing_geoname_id,
group_concat(distinct a1.geonames_id) admin1_ids,
group_concat(distinct a2.geonames_id) admin2_ids,
group_concat(distinct a3.geonames_id) admin3_ids
from postal_codes p
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
where p.country_code not in ({codes})
group by postal_code, p.country_code
'''.format(codes=','.join("'{}'".format(code) for code in IGNORE_COUNTRY_POSTAL_CODES))

BATCH_SIZE = 10000


def create_geonames_tsv(db_path, out_dir=DEFAULT_DATA_DIR):
    db = sqlite3.connect(db_path)

    filename = 'geonames.tsv'
    f = open(os.path.join(out_dir, filename), 'w')
    writer = csv.writer(f, delimiter='\t')

    for boundary_type, codes in geonames_admin_dictionaries.iteritems():
        if boundary_type != boundary_types.COUNTRY:
            predicate = 'where gn.feature_code in ({codes})'.format(
                codes=','.join(['"{}"'.format(c) for c in codes])
            )
        else:
            # The query for countries in GeoNames is somewhat non-trivial
            predicate = 'where gn.geonames_id in (select geonames_id from countries)'

        query = base_geonames_query.format(
            fields=','.join(geonames_fields),
            predicate=predicate
        )
        cursor = db.execute(query)
        while True:
            batch = cursor.fetchmany(BATCH_SIZE)
            if not batch:
                break
            rows = [
                [str(boundary_type)] + [safe_encode(val or '') for val in row]
                for row in batch
            ]
            writer.writerows(rows)
        cursor.close()
        f.flush()
    f.close()
    db.close()


def create_postal_codes_tsv(db_path, out_dir=DEFAULT_DATA_DIR):
    db = sqlite3.connect(db_path)

    filename = 'postal_codes.tsv'
    f = open(os.path.join(out_dir, filename), 'w')
    writer = csv.writer(f, delimiter='\t')

    cursor = db.execute(postal_codes_query)

    while True:
        batch = cursor.fetchmany(BATCH_SIZE)
        if not batch:
            break
        rows = [
            [safe_encode(val or '') for val in row]
            for row in batch
        ]
        writer.writerows(rows)

    cursor.close()
    f.close()
    db.close()


if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--db',
                        default=DEFAULT_GEONAMES_DB_PATH,
                        help='SQLite db file')
    parser.add_argument('-o', '--out',
                        default=DEFAULT_DATA_DIR, help='output directory')
    args = parser.parse_args()
    create_geonames_tsv(args.db, args.out)
    create_postal_codes_tsv(args.db, args.out)
