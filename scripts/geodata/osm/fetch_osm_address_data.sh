#!/usr/bin/env bash

: '
fetch_osm_address_data.sh
-------------------------

Shell script to download OSM planet and derive inputs
for language detection and address parser training set
construction.

Usage: ./fetch_osm_address_data.sh out_dir
'

if [ "$#" -ge 1 ]; then
    OUT_DIR=$1
else
    OUT_DIR=`pwd`
fi

# Check for osmfilter and osmconvert
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

# Download planet as PBF
# TODO: currently uses single mirror, randomly choose one instead
echo "Started OSM download: `date`"
wget http://ftp5.gwdg.de/pub/misc/openstreetmap/planet.openstreetmap.org/pbf/planet-latest.osm.pbf

echo "Converting to o5m: `date`"
PLANET_PBF="planet-latest.osm.pbf"
PLANET_O5M="planet-latest.o5m"

IS_AIRPORT="aeroway=aerodrome"
VALID_AMENITIES="amenity="
VALID_TOURISM_KEYS="tourism=hotel or tourism=attraction or tourism=guest_house or tourism=museum or tourism=chalet or tourism=motel or tourism=hostel or tourism=alpine_hut or tourism=theme_park or tourism=zoo or tourism=apartment or tourism=wilderness_hut or tourism=gallery or tourism=bed_and_breakfast or tourism=hanami or tourism=wine_cellar or tourism=resort or tourism=aquarium or tourism=apartments or tourism=cabin or tourism=winery or tourism=hut"
VALID_LEISURE_KEYS="leisure=adult_gaming_centre or leisure=amusement_arcade or leisure=arena or leisure=bandstand or leisure=beach_resort or leisure=bbq or leisure=bird_hide or leisure=bowling_alley or leisure=casino or leisure=common or leisure=club or leisure=dance or leisure=dancing or leisure=disc_golf_course or leisure=dog_park or leisure=fishing or leisure=fitness_centre or leisure=gambling or leisure=garden or leisure=golf_course or leisure=hackerspace or leisure=horse_riding or leisure=hospital or leisure=hot_spring or leisure=ice_rink leisure=landscape_reserve or leisure=marina or leisure=maze or leisure=miniature_golf or leisure=nature_reserve or leisure=padding_pool or leisure=park or leisure=pitch or leisure=playground or leisure=recreation_ground or leisure=resort or leisure=sailing_club or leisure=sauna or leisure=social_club or leisure=sports_centre or leisure=stadium or leisure=summer_camp or leisure=swimming_pool or leisure=tanning_salon or leisure=track or leisure=trampoline_park or leisure=turkish_bath or leisure=video_arcade or leisure=water_park or leisure=wildlife_hide"

VALID_VENUES="( ( $IS_AIRPORT ) or ( $VALID_AMENITIES ) or ( $VALID_TOURISM_KEYS ) or ( $VALID_LEISURE_KEYS ) )"

# Needs to be in O5M for some of the subsequent steps to work whereas PBF is smaller for download
osmconvert $PLANET_PBF -o=$PLANET_O5M
rm $PLANET_PBF

# Address data set for use in parser, language detection
echo "Filtering for records with address tags: `date`"
PLANET_ADDRESSES_O5M="planet-addresses.o5m"
osmfilter $PLANET_O5M --keep="( ( name= and $VALID_VENUES ) ) or ( addr:street= and ( name= or building= or building:levels= or addr:housename= or addr:housenumber= ) )" --drop-author --drop-version -o=$PLANET_ADDRESSES_O5M
PLANET_ADDRESSES_LATLONS="planet-addresses-latlons.o5m"
osmconvert $PLANET_ADDRESSES_O5M --max-objects=1000000000 --all-to-nodes -o=$PLANET_ADDRESSES_LATLONS
rm $PLANET_ADDRESSES_O5M
PLANET_ADDRESSES="planet-addresses.osm"
osmfilter $PLANET_ADDRESSES_LATLONS --keep="( ( name= and $VALID_VENUES ) ) or ( addr:street= and ( name= or building= or building:levels= or addr:housename= or addr:housenumber= ) )" -o=$PLANET_ADDRESSES
rm $PLANET_ADDRESSES_LATLONS

# Border data set for use in R-tree index/reverse geocoding, parsing, language detection
echo "Filtering for borders: `date`"
PLANET_BORDERS_O5M="planet-borders.o5m"
PLANET_BORDERS="planet-borders.osm"
PLANET_ADMIN_BORDERS_OSM="planet-admin-borders.osm"
osmfilter $PLANET_O5M --keep="boundary=administrative or boundary=town or boundary=city_limit or boundary=civil_parish or boundary=ceremonial" --drop-author --drop-version -o=$PLANET_ADMIN_BORDERS_OSM
osmfilter $PLANET_O5M --keep="boundary=administrative or place=city or place=town or place=village or place=hamlet or place=neighbourhood or place=suburb or place=quarter or place=borough" --drop-author --drop-version -o=$PLANET_BORDERS_O5M
PLANET_BORDERS_LATLONS="planet-borders-latlons.o5m"
osmconvert $PLANET_BORDERS_O5M --max-objects=1000000000 --all-to-nodes -o=$PLANET_BORDERS_LATLONS
rm $PLANET_BORDERS_O5M
osmfilter $PLANET_BORDERS_LATLONS --keep="boundary=administrative or place=city or place=town or place=village or place=hamlet or place=neighbourhood or place=suburb or place=quarter or place=borough" -o=$PLANET_BORDERS
rm $PLANET_BORDERS_LATLONS

echo "Filtering for neighborhoods"
PLANET_NEIGHBORHOODS="planet-neighborhoods.osm"
osmfilter $PLANET_O5M --keep="name= and ( place=neighbourhood or place=suburb or place=quarter or place=borough or place=locality )" --drop-relations --drop-ways --ignore-dependencies --drop-author --drop-version -o=$PLANET_NEIGHBORHOODS

# Venue data set for use in venue classification
echo "Filtering for venue records: `date`"
PLANET_VENUES_O5M="planet-venues.o5m"
osmfilter $PLANET_O5M --keep="name= and ( building= or ( $VALID_VENUES ) )" --drop-author --drop-version -o=$PLANET_VENUES_O5M
PLANET_VENUES_LATLONS="planet-venues-latlons.o5m"
osmconvert $PLANET_VENUES_O5M --max-objects=1000000000 --all-to-nodes -o=$PLANET_VENUES_LATLONS
rm $PLANET_VENUES_O5M
PLANET_VENUES="planet-venues.osm"
osmfilter $PLANET_VENUES_LATLONS --keep="name= and ( building= or ( $VALID_VENUES ) )" -o=$PLANET_VENUES
rm $PLANET_VENUES_LATLONS

# Streets data set for use in language classification 
echo "Filtering ways: `date`"
PLANET_WAYS_O5M="planet-ways.o5m"
osmfilter planet-latest.o5m --keep="name= and highway=" --drop-relations --drop-author --drop-version -o=$PLANET_WAYS_O5M
rm $PLANET_O5M
PLANET_WAYS_NODES_LATLON="planet-ways-nodes-latlons.o5m"
osmconvert $PLANET_WAYS_O5M --max-objects=1000000000 --all-to-nodes -o=$PLANET_WAYS_NODES_LATLON
# 10^15 is the offset used for ways and relations with --all-to-ndoes, extracts just the ways
PLANET_WAYS_LATLONS="planet-ways.osm"
osmfilter $PLANET_WAYS_NODES_LATLON --keep="name= and ( highway=motorway or highway=motorway_link or highway=trunk or highway=trunk_link or highway=primary or highway=primary_link or highway=secondary or highway=secondary_link or highway=tertiary or highway=tertiary_link or highway=unclassified or highway=unclassified_link or highway=residential or highway=residential_link or highway=service or highway=service_link or highway=living_street or highway=pedestrian or highway=track or highway=road or ( highway=path and ( motorvehicle=yes or motorcar=yes ) ) )" -o=$PLANET_WAYS_LATLONS
rm $PLANET_WAYS_NODES_LATLON
rm $PLANET_WAYS_O5M

echo "Completed : `date`"

cd $PREV_DIR
