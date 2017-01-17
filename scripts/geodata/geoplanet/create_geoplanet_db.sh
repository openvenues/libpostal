#!/usr/bin/env bash

: '
create_geoplanet_db.sh
-------------------------

Shell script to download Geo Planet and derive inputs
for address parser training set construction.

Usage: ./create_geoplanet_db.sh out_dir
'

if [ "$#" -ge 1 ]; then
    OUT_DIR=$1
    mkdir -p $OUT_DIR
else
    OUT_DIR=$(pwd)
fi

GEOPLANET_ZIP_FILE="geoplanet_data_7.10.0.zip"
# Internet Archive URL
GEOPLANET_URL="https://archive.org/download/$GEOPLANET_ZIP_FILE/$GEOPLANET_ZIP_FILE"
GEOPLANET_ORIGINAL_PLACES_FILE="geoplanet_places_7.10.0.tsv"
GEOPLANET_ADMINS_FILE="geoplanet_admins_7.10.0.tsv"
GEOPLANET_ORIGINAL_ALIASES_FILE="geoplanet_aliases_7.10.0.tsv"

GEOPLANET_ALL_PLACES_FILE="geoplanet_all_places.tsv"
GEOPLANET_PLACES_FILE="geoplanet_places.tsv"
GEOPLANET_POSTAL_CODES_FILE="geoplanet_postal_codes.tsv"
GEOPLANET_ALIASES_FILE="geoplanet_aliases.tsv"

GEOPLANET_GEONAMES_CONCORDANCE_FILE="geonames-geoplanet-matches.csv"
GEOPLANET_GEONAMES_CONCORDANCE_URL="https://github.com/blackmad/geoplanet-concordance/raw/master/current/$GEOPLANET_GEONAMES_CONCORDANCE_FILE"

GEOPLANET_DB_FILE="geoplanet.db"

function download_file() {
    echo "Downloading $1"
    response=$(curl -sL -w "%{http_code}" $1 --retry 3 --retry-delay 5 -o $OUT_DIR/$2)
    if [ $response -ne "200" ]; then
        echo "Could not download $GEOPLANET_URL"
        exit 1
    fi    
}


if [ ! -f $OUT_DIR/$GEOPLANET_ZIP_FILE ]; then
    echo "Downloading GeoPlanet"
    download_file $GEOPLANET_URL $GEOPLANET_ZIP_FILE
fi

cd $OUT_DIR
echo "Unzipping GeoPlanet file"
unzip -o $GEOPLANET_ZIP_FILE

echo "Creating GeoPlanet postal codes file"
awk -F'\t' 'BEGIN{OFS="\t";} {if ($5 == "Zip") print $0;}' $GEOPLANET_ORIGINAL_PLACES_FILE > $GEOPLANET_POSTAL_CODES_FILE

echo "Creating GeoPlanet all places file"
tail -n+2 $GEOPLANET_ORIGINAL_PLACES_FILE > $GEOPLANET_ALL_PLACES_FILE

echo "Creating GeoPlanet places file"
awk -F'\t' 'BEGIN{OFS="\t";} {if ($5 == "Continent" || $5 == "Country" || $5 == "Nationality" || $5 == "State" || $5 == "County" ||  $5 == "Town" || $5 == "LocalAdmin" || $5 == "Island" || $5 == "Suburb") print $0;}' $GEOPLANET_ORIGINAL_PLACES_FILE > $GEOPLANET_PLACES_FILE

echo "Creating GeoPlanet aliases file"
tail -n+2 $GEOPLANET_ORIGINAL_ALIASES_FILE > $GEOPLANET_ALIASES_FILE

echo "Fetching GeoNames concordance"
download_file $GEOPLANET_GEONAMES_CONCORDANCE_URL $GEOPLANET_GEONAMES_CONCORDANCE_FILE

echo "Creating SQLite db"

echo "
DROP TABLE IF EXISTS places;
CREATE TABLE places (
    id integer primary key,
    country_code text,
    name text,
    language text,
    place_type text,
    parent_id integer
);

.separator \t
.import $OUT_DIR/$GEOPLANET_PLACES_FILE places

CREATE INDEX places_parent_id_index on places(parent_id);
CREATE INDEX places_country_code on places(country_code);

DROP TABLE IF EXISTS all_places;
CREATE TABLE all_places AS SELECT * FROM places WHERE 0;
.import $OUT_DIR/$GEOPLANET_ALL_PLACES_FILE all_places

DROP TABLE IF EXISTS postal_codes;
CREATE TABLE postal_codes (
    id integer primary key,
    country_code text,
    name text,
    language text,
    place_type text,
    parent_id integer
);

.import $OUT_DIR/$GEOPLANET_POSTAL_CODES_FILE postal_codes
CREATE INDEX postal_codes_parent_id_index on postal_codes(parent_id);
CREATE INDEX postal_codes_country_code on postal_codes(country_code);

DROP TABLE IF EXISTS admins;
CREATE TABLE admins (
    id integer primary key,
    country_code text,
    state_id integer,
    county_id integer,
    local_admin_id integer,
    country_id integer,
    continent_id integer
);

.import $OUT_DIR/$GEOPLANET_ADMINS_FILE admins

CREATE INDEX admin_country_code on admins(country_code);
CREATE INDEX admin_state_id on admins(state_id);
CREATE INDEX admin_county_id on admins(county_id);
CREATE INDEX admin_local_admin_id on admins(local_admin_id);
CREATE INDEX admin_country_id on admins(country_id);
CREATE INDEX admin_continent_id on admins(continent_id);

DROP TABLE IF EXISTS aliases;
CREATE TABLE aliases (
    id integer,
    name text,
    name_type text,
    language text
);

.import $OUT_DIR/$GEOPLANET_ALIASES_FILE aliases

CREATE INDEX alias_id on aliases(id);

DROP TABLE IF EXISTS geonames_concordance;
CREATE TABLE geonames_concordance (
    id integer primary key,
    geonames_id integer,
    name text,
    lat number,
    lon number
);

.mode csv
.import $OUT_DIR/$GEOPLANET_GEONAMES_CONCORDANCE_FILE geonames_concordance

CREATE INDEX geonames_concordance_geonames_id on geonames_concordance(geonames_id);

" | sqlite3 $OUT_DIR/$GEOPLANET_DB_FILE