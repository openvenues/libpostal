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

# Needs to be in O5M for some of the subsequent steps to work whereas PBF is smaller for download
osmconvert $PLANET_PBF -o=$PLANET_O5M
rm $PLANET_PBF

IS_AIRPORT="aeroway=aerodrome"
VALID_AMENITIES="amenity=ambulance_station or amenity=animal_boarding or amenity=animal_shelter or amenity=arts_centre or amenity=baby_hatch or amenity=bank or amenity=bar or amenity=bbq or amenity=biergarten or amenity=boathouse or amenity=boat_rental or amenity=boat_sharing or amenity=boat_storage or amenity=brothel or amenity=bureau_de_change or amenity=bus_station or amenity=cafe or amenity=car_rental or amenity=car_sharing or amenity=car_wash or amenity=casino or amenity=cemetery or amenity=charging_station or amenity=cinema or amenity=childcare or amenity=clinic or amenity=club or amenity=clock or amenity=college or amenity=community_center or amenity=community_centre or amenity=community_hall or amenity=concert_hall or amenity=conference_centre or amenity=courthouse or amenity=coworking_space or amenity=crematorium or amenity=crypt or amenity=culture_center or amenity=dancing_school or amenity=dentist or amenity=dive_centre or amenity=doctors or amenity=dojo or amenity=dormitory or amenity=driving_school or amenity=embassy or amenity=emergency_service or amenity=events_venue or amenity=exhibition_centre or amenity=fast_food or amenity=ferry_terminal or amenity=festival_grounds or amenity=fire_station or amenity=food_count or amenity=fountain or amenity=fuel or amenity=gambling or amenity=game_feeding or amenity=grave_yard or amenity=greenhouse or amenity=gym or amenity=health_centre or amenity=hospice or amenity=hospital or amenity=hunting_stand or amenity=ice_cream or amenity=internet_cafe or amenity=kindergarten or amenity=kiosk or amenity=kneipp_water_cure or amenity=language_school or amenity=lavoir or amenity=library or amenity=love_hotel or amenity=market or amenity=marketplace or amenity=medical_centre or amenity=mobile_money_agent or amenity=monastery or amenity=money_transfer or amenity=mortuary or amenity=music_school or amenity=music_venue or amenity=nightclub or amenity=nursery or amenity=nursing_home or amenity=office or amenity=parish_hall or amenity=park or amenity=pharmacy or amenity=planetarium or amenity=place_of_worship or amenity=police or amenity=post_office or amenity=preschool or amenity=prison or amenity=pub or amenity=public_bath or amenity=public_bookcase or amenity=public_building or amenity=public_facility or amenity=public_hall or amenity=public_market or amenity=ranger_station or amenity=refugee_housing or amenity=register_office or amenity=research_institute or amenity=rescue_station or amenity=residential or amenity=Residential or amenity=restaurant or amenity=retirement_home or amenity=sacco or amenity=sanitary_dump_station or amenity=sanitorium or amenity=sauna or amenity=school or amenity=shelter or amenity=shop or amenity=shower or amenity=ski_rental or amenity=ski_school or amenity=social_centre or amenity=social_club or amenity=social_facility or amenity=spa or amenity=stables or amenity=stripclub or amenity=studio or amenity=swimming_pool or amenity=swingerclub or amenity=townhall or amenity=theatre or amenity=training or amenity=trolley_bay or amenity=university or amenity=vehicle_inspection or amenity=veterinary or amenity=village_hall or amenity=vivarium or amenity=waste_transfer_station or amenity=whirlpool or amenity=winery or amenity=youth_centre"
GENERIC_AMENITIES="amenity=atm or amenity=bench or amenity=bicycle_parking or amenity=bicycle_rental or amenity=bicycle_repair_station or amenity=compressed_air or amenity=drinking_water or amenity=emergency_phone or amenity=grit_bin or amenity=motorcycle_parking or amenity=parking or amenity=parking_space or amenity=recycling or amenity=taxi or amenity=ticket_validator or amenity=toilets or amenity=vending_machine or amenity=waste_basket or amenity=waste_disposal or amenity=water_point or amenity=watering_place"

VALID_OFFICE_KEYS="office=accountant or office=administrative or office=administration or office=advertising_agency or office=architect or office=association or office=camping or office=charity or office=company or office=consulting or office=educational_institution or office=employment_agency or office=estate_agent or office=financial or office=forestry or office=foundation or office=government or office=insurance or office=it or office=lawyer or office=newspaper or office=ngo or office=notary or office=parish or office=physician or office=political_party or office=publisher or office=quango or office=real_estate_agent or office=realtor or office=register or office=religion or office=research or office=tax or office=tax_advisor or office=telecommunication or office=therapist or office=travel_agent or office=water_utility"
VALID_SHOP_KEYS="shop="
VALID_TOURISM_KEYS="tourism=hotel or tourism=attraction or tourism=guest_house or tourism=museum or tourism=chalet or tourism=motel or tourism=hostel or tourism=alpine_hut or tourism=theme_park or tourism=zoo or tourism=apartment or tourism=wilderness_hut or tourism=gallery or tourism=bed_and_breakfast or tourism=hanami or tourism=wine_cellar or tourism=resort or tourism=aquarium or tourism=apartments or tourism=cabin or tourism=winery or tourism=hut"
VALID_LEISURE_KEYS="leisure=adult_gaming_centre or leisure=amusement_arcade or leisure=arena or leisure=bandstand or leisure=beach_resort or leisure=bbq or leisure=bird_hide or leisure=bowling_alley or leisure=casino or leisure=common or leisure=club or leisure=dance or leisure=dancing or leisure=disc_golf_course or leisure=dog_park or leisure=fishing or leisure=fitness_centre or leisure=gambling or leisure=garden or leisure=golf_course or leisure=hackerspace or leisure=horse_riding or leisure=hospital or leisure=hot_spring or leisure=ice_rink leisure=landscape_reserve or leisure=marina or leisure=maze or leisure=miniature_golf or leisure=nature_reserve or leisure=padding_pool or leisure=park or leisure=pitch or leisure=playground or leisure=recreation_ground or leisure=resort or leisure=sailing_club or leisure=sauna or leisure=social_club or leisure=sports_centre or leisure=stadium or leisure=summer_camp or leisure=swimming_pool or leisure=tanning_salon or leisure=track or leisure=trampoline_park or leisure=turkish_bath or leisure=video_arcade or leisure=water_park or leisure=wildlife_hide"

VALID_VENUES="( ( $IS_AIRPORT ) or ( $VALID_AMENITIES ) or ( $VALID_OFFICE_KEYS ) or ( $VALID_SHOP_KEYS ) or ( $VALID_TOURISM_KEYS ) or ( $VALID_LEISURE_KEYS ) )"

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
osmfilter $PLANET_O5M --keep="( name= and building= ) or ( $VALID_VENUES ) or ( $GENERIC_AMENITIES )" --drop-author --drop-version -o=$PLANET_VENUES_O5M
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
