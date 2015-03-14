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

# Inserted post-query
DUMMY_BOUNDARY_TYPE = '-1 as type'


class GeonamesField(object):
    def __init__(self, name, c_constant, default=None, is_dummy=False):
        self.name = name
        self.c_constant = c_constant
        self.default = default
        self.is_dummy = is_dummy

geonames_fields = [
    # Field if alternate_names present, default field name if not, C header constant
    GeonamesField('gn.geonames_id as geonames_id', 'GEONAMES_ID'),
    GeonamesField('gn.name as canonical', 'GEONAMES_CANONICAL'),
    GeonamesField(DUMMY_BOUNDARY_TYPE, 'GEONAMES_BOUNDARY_TYPE', is_dummy=True),
    GeonamesField('alternate_name', 'GEONAMES_NAME', default='gn.name'),
    GeonamesField('iso_language', 'GEONAMES_ISO_LANGUAGE', default="''"),
    GeonamesField('is_preferred_name', 'GEONAMES_IS_PREFERRED_NAME', default='0'),
    GeonamesField('population', 'GEONAMES_POPULATION'),
    GeonamesField('latitude', 'GEONAMES_LATITUDE'),
    GeonamesField('longitude', 'GEONAMES_LONGITUDE'),
    GeonamesField('feature_code', 'GEONAMES_FEATURE_CODE'),
    GeonamesField('gn.country_code as country_code', 'GEONAMES_COUNTRY_CODE'),
    GeonamesField('gn.admin1_code as admin1_code', 'GEONAMES_ADMIN1_CODE'),
    GeonamesField('a1.geonames_id as a1_gn_id', 'GEONAMES_ADMIN1_ID'),
    GeonamesField('gn.admin2_code as admin2_code', 'GEONAMES_ADMIN2_CODE'),
    GeonamesField('a2.geonames_id as a2_gn_id', 'GEONAMES_ADMIN2_ID'),
    GeonamesField('gn.admin3_code as admin3_code', 'GEONAMES_ADMIN3_CODE'),
    GeonamesField('a3.geonames_id as a3_gn_id', 'GEONAMES_ADMIN3_ID'),
    GeonamesField('gn.admin4_code as admin4_code', 'GEONAMES_ADMIN4_CODE'),
    GeonamesField('a4.geonames_id as a4_gn_id', 'GEONAMES_ADMIN4_ID'),
]

DUMMY_BOUNDARY_TYPE_INDEX = [i for i, f in enumerate(geonames_fields)
                             if f.is_dummy][0]

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
{admin_joins}
{{predicate}}
union all
select {alt_name_fields}
from geonames gn
join alternate_names an
    on an.geonames_id = gn.geonames_id
    and iso_language not in ('doi','faac','iata',
                             'icao','link','post','tcid')
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
    GeonamesField('n.geonames_id as containing_geoname_id', 'GN_POSTAL_CONTAINING_GEONAME_ID'),
    GeonamesField('group_concat(distinct a1.geonames_id) admin1_ids', 'GN_POSTAL_ADMIN1_IDS'),
    GeonamesField('group_concat(distinct a2.geonames_id) admin2_ids', 'GN_POSTAL_ADMIN2_IDS'),
    GeonamesField('group_concat(distinct a3.geonames_id) admin3_ids', 'GN_POSTAL_ADMIN3_IDS'),
]

postal_codes_query = '''
select
{fields}
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
where p.country_code not in ({exclude_country_codes})
group by postal_code, p.country_code
'''.format(
    fields=','.join([f.name for f in postal_code_fields]),
    exclude_country_codes=','.join("'{}'".format(code) for code in IGNORE_COUNTRY_POSTAL_CODES))

BATCH_SIZE = 2000


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
            predicate=predicate
        )

        cursor = db.execute(query)
        while True:
            batch = cursor.fetchmany(BATCH_SIZE)
            if not batch:
                break
            rows = []
            for row in batch:
                row = [safe_encode(val or '') for val in row]
                row[DUMMY_BOUNDARY_TYPE_INDEX] = boundary_type
                rows.append(row)

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

# Generates a C header telling us the order of the fields as written
GEONAMES_FIELDS_HEADER = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                      'src', 'geonames_fields.h')

GEONAMES_FIELDS_HEADER_FILE = '''enum geonames_fields {{
    {fields}
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
    {fields}
    NUM_POSTAL_FIELDS
}};
'''.format(fields=''',
    '''.join(['{}={},'.format(f.c_constant, i) for i, f in enumerate(postal_code_fields)]))


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
    create_geonames_tsv(args.db, args.out)
    create_postal_codes_tsv(args.db, args.out)
    write_geonames_fields_header()
    write_postal_fields_header()
