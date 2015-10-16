#!/usr/bin/env bash

if [ "$#" -eq 1 ]; then
    OUT_DIR=$1
else
    OUT_DIR=`pwd`
fi

if ! type -P osmfilter osmconvert > /dev/null; then
cat << EOF
ERROR: osmfilter and osmconvert are required

On Debian/Ubuntu:
sudo apt-get install osmctools

Or to compile:
wget -O - http://m.m.i24.cc/osmfilter.c |cc -x c - -O3 -o osmfilter
wget -O - http://m.m.i24.cc/osmconvert.c | cc -x c - -lz -O3 -o osmconvert
EOF
exit 127
fi

PREV_DIR=`pwd`

cd $OUT_DIR

echo "Started OSM download: `date`"
wget http://ftp5.gwdg.de/pub/misc/openstreetmap/planet.openstreetmap.org/pbf/planet-latest.osm.pbf

echo "Converting to o5m: `date`"
PLANET_PBF="planet-latest.osm.pbf"
PLANET_O5M="planet-latest.o5m"

osmconvert $PLANET_PBF -o=$PLANET_O5M
rm $PLANET_PBF
echo "Filtering for records with address tags: `date`"
PLANET_ADDRESSES_O5M="planet-addresses.o5m"
osmfilter $PLANET_O5M --keep="addr:street= and ( ( name= and amenity= ) or addr:housename= or addr:housenumber= )" --drop-author --drop-version -o=$PLANET_ADDRESSES_O5M
PLANET_ADDRESSES_LATLONS="planet-addresses-latlons.o5m"
osmconvert $PLANET_ADDRESSES_O5M --max-objects=1000000000 --all-to-nodes -o=$PLANET_ADDRESSES_LATLONS
rm $PLANET_ADDRESSES_O5M
PLANET_ADDRESSES="planet-addresses.osm"
osmfilter $PLANET_ADDRESSES_LATLONS --keep="addr:street= and ( ( name= and amenity= ) or addr:housename= or addr:housenumber= )" -o=$PLANET_ADDRESSES
rm $PLANET_ADDRESSES_LATLONS

echo " Filtering for borders: `date`"
PLANET_BORDERS_O5M="planet-borders.o5m"
PLANET_BORDERS="planet-borders.osm"
PLANET_ADMIN_BORDERS_OSM="planet-admin-borders.osm"
osmfilter $PLANET_O5M --keep="boundary=administrative" --drop-author --drop-version -o=$PLANET_ADMIN_BORDERS_OSM
osmfilter $PLANET_O5M --keep="boundary=administrative or place=city or place=town or place=neighbourhood or place=suburb" --drop-author --drop-version -o=$PLANET_BORDERS_O5M
PLANET_BORDERS_LATLONS="planet-borders-latlons.o5m"
osmconvert $PLANET_BORDERS_O5M --max-objects=1000000000 --all-to-nodes -o=$PLANET_BORDERS_LATLONS
rm $PLANET_BORDERS_O5M
osmfilter $PLANET_BORDERS_LATLONS --keep="boundary=administrative or place=city or place=town or place=neighbourhood or place=suburb" -o=$PLANET_BORDERS
rm $PLANET_BORDERS_LATLONS


echo "Filtering for venue records: `date`"
PLANET_VENUES_O5M="planet-venues.o5m"
osmfilter $PLANET_O5M --keep="name= and ( amenity= or building= )" --drop-author --drop-version -o=$PLANET_VENUES_O5M
PLANET_VENUES_LATLONS="planet-venues-latlons.o5m"
osmconvert $PLANET_VENUES_O5M --max-objects=1000000000 --all-to-nodes -o=$PLANET_VENUES_LATLONS
rm $PLANET_VENUES_O5M
PLANET_VENUES="planet-venues.osm"
osmfilter $PLANET_VENUES_LATLONS --keep="name= and ( amenity= or building= )" -o=$PLANET_VENUES
rm $PLANET_VENUES_LATLONS

echo "Filtering ways: `date`"
PLANET_WAYS_O5M="planet-ways.o5m"
osmfilter planet-latest.o5m --keep="name= and highway=" --drop-relations --drop-author --drop-version -o=$PLANET_WAYS_O5M
rm $PLANET_O5M

echo "Extracting ways: `date`"
PLANET_WAYS_NODES_LATLON="planet-ways-nodes-latlons.o5m"
osmconvert $PLANET_WAYS_O5M --max-objects=1000000000 --all-to-nodes -o=$PLANET_WAYS_NODES_LATLON
# 10^15 is the offset used for ways and relations with --all-to-ndoes, extracts just the ways
PLANET_WAYS_LATLONS="planet-ways.osm"
osmfilter $PLANET_WAYS_NODES_LATLON --keep="name= and ( highway=motorway or highway=motorway_link or highway=trunk or highway=trunk_link or highway=primary or highway=primary_link or highway=secondary or highway=secondary_link or highway=tertiary or highway=tertiary_link or highway=unclassified or highway=unclassified_link or highway=residential or highway=residential_link or highway=service or highway=service_link or highway=living_street or highway=pedestrian or highway=track or highway=road or ( highway=path and ( motorvehicle=yes or motorcar=yes ) ) )" -o=$PLANET_WAYS_LATLONS
rm $PLANET_WAYS_NODES_LATLON
rm $PLANET_WAYS_O5M

echo "Completed : `date`"

cd $PREV_DIR
