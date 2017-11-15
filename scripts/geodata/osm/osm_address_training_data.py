# -*- coding: utf-8 -*-
'''
osm_address_training_data.py
----------------------------

This script generates several training sets from OpenStreetMap addresses,
streets, venues and toponyms.

Note: the combined size of all the files created by this script exceeds 100GB
so if training these models, it is wise to use a server-grade machine with
plenty of disk space. The following commands can be used in parallel to create
all the training sets:

Ways:
python osm_address_training_data.py -s $(OSM_DIR)/planet-ways.osm --country-rtree-dir=$(RTREE_DIR) -o $(OUT_DIR)

Venues:
python osm_address_training_data.py -v $(OSM_DIR)/planet-venues.osm --country-rtree-dir=$(RTREE_DIR) -o $(OUT_DIR)

Limited formatted addresses:
python osm_address_training_data.py -a -l $(OSM_DIR)/planet-addresses.osm --country-rtree-dir=$(COUNTRY_RTREE_DIR) --rtree-dir=$(RTREE_DIR) --neighborhoods-rtree-dir=$(NEIGHBORHOODS_RTREE_DIR)  -o $(OUT_DIR)

Formatted addresses (tagged):
python osm_address_training_data.py -a $(OSM_DIR)/planet-addresses.osm -f --country-rtree-dir=$(COUNTRY_RTREE_DIR) --neighborhoods-rtree-dir=$(NEIGHBORHOODS_RTREE_DIR) --rtree-dir=$(RTREE_DIR) -o $(OUT_DIR)

Formatted addresses (untagged):
python osm_address_training_data.py -a $(OSM_DIR)/planet-addresses.osm  -f -u --country-rtree-dir=$(COUNTRY_RTREE_DIR) --neighborhoods-rtree-dir=$(NEIGHBORHOODS_RTREE_DIR)  --rtree-dir=$(RTREE_DIR) -o $(OUT_DIR)

Intersections (after running intersections.py to create the JSON file):
python osm_address_training_data -x $(OSM_DIR)/intersections.json -f --country-rtree-dir=$(COUNTRY_RTREE_DIR) --neighborhoods-rtree-dir=$(NEIGHBORHOODS_RTREE_DIR)  --rtree-dir=$(RTREE_DIR) -o $(OUT_DIR)

Toponyms:
python osm_address_training_data.py -b $(OSM_DIR)/planet-borders.osm --country-rtree-dir=$(COUNTRY_RTREE_DIR) -o $(OUT_DIR)
'''

import argparse
import csv
import logging
import os
import operator
import random
import re
import sys
import tempfile
import urllib
import ujson as json
import HTMLParser

from collections import defaultdict, OrderedDict
from lxml import etree
from itertools import ifilter, chain, combinations

from shapely.geos import LOG as shapely_geos_logger
shapely_geos_logger.setLevel(logging.CRITICAL)

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.address_expansions.abbreviations import abbreviate
from geodata.address_expansions.gazetteers import *
from geodata.addresses.components import AddressComponents
from geodata.coordinates.conversion import *
from geodata.language_id.disambiguation import *
from geodata.language_id.sample import sample_random_language
from geodata.i18n.languages import *
from geodata.metro_stations.reverse_geocode import MetroStationReverseGeocoder
from geodata.neighborhoods.reverse_geocode import NeighborhoodReverseGeocoder
from geodata.osm.extract import *
from geodata.osm.formatter import OSMAddressFormatter
from geodata.places.reverse_geocode import PlaceReverseGeocoder
from geodata.polygons.language_polys import *
from geodata.polygons.reverse_geocode import *
from geodata.i18n.unicode_paths import DATA_DIR

from geodata.csv_utils import *
from geodata.file_utils import *

# Input files
PLANET_ADDRESSES_INPUT_FILE = 'planet-addresses.osm'
PLANET_WAYS_INPUT_FILE = 'planet-ways.osm'
PLANET_VENUES_INPUT_FILE = 'planet-venues.osm'
PLANET_BORDERS_INPUT_FILE = 'planet-borders.osm'

# Output files
WAYS_LANGUAGE_DATA_FILENAME = 'streets_by_language.tsv'
ADDRESS_LANGUAGE_DATA_FILENAME = 'address_streets_by_language.tsv'
TOPONYM_LANGUAGE_DATA_FILENAME = 'toponyms_by_language.tsv'


def normalize_osm_name_tag(tag, script=False):
    norm = tag.rsplit(':', 1)[-1]
    if not script:
        return norm
    return norm.split('_', 1)[0]


def get_language_names(country_rtree, key, value, tag_prefix='name'):
    if not ('lat' in value and 'lon' in value):
        return None, None

    has_colon = ':' in tag_prefix
    tag_first_component = tag_prefix.split(':')[0]
    tag_last_component = tag_prefix.split(':')[-1]

    try:
        latitude, longitude = latlon_to_decimal(value['lat'], value['lon'])
    except Exception:
        return None, None

    osm_country_components = country_rtree.point_in_poly(latitude, longitude, return_all=True)
    country, candidate_languages = country_rtree.country_and_languages_from_components(osm_country_components)
    if not (country and candidate_languages):
        return None, None

    num_langs = len(candidate_languages)
    default_langs = set([l for l, d in candidate_languages if d])
    num_defaults = len(default_langs)
    name_language = defaultdict(list)

    alternate_langs = []

    equivalent_alternatives = defaultdict(list)
    for k, v in value.iteritems():
        if k.startswith(tag_prefix + ':') and normalize_osm_name_tag(k, script=True) in languages:
            lang = k.rsplit(':', 1)[-1]
            alternate_langs.append((lang, v))
            equivalent_alternatives[v].append(lang)

    has_alternate_names = len(alternate_langs)
    # Some countries like Lebanon list things like name:en == name:fr == "Rue Abdel Hamid Karame"
    # Those addresses should be disambiguated rather than taken for granted
    ambiguous_alternatives = set([k for k, v in equivalent_alternatives.iteritems() if len(v) > 1])

    regional_defaults = 0
    country_defaults = 0
    regional_langs = set()
    country_langs = set()
    for c in osm_country_components:
        _, langs = country_rtree.country_and_languages_from_components([c])
        if 'ISO3166-1:alpha2' not in c:
            regional_defaults += sum((1 for l, d in langs if d))
            regional_langs |= set([l for l, d in langs])
        else:
            country_defaults += sum((1 for l, d in langs if d))
            country_langs |= set([l for l, d in langs])

    ambiguous_already_seen = set()

    for k, v in value.iteritems():
        if k.startswith(tag_prefix + ':'):
            if v not in ambiguous_alternatives:
                norm = normalize_osm_name_tag(k)
                norm_sans_script = normalize_osm_name_tag(k, script=True)
                if norm in languages or norm_sans_script in languages:
                    name_language[norm].append(v)
            elif v not in ambiguous_already_seen:
                langs = [(lang, lang in default_langs) for lang in equivalent_alternatives[v]]
                lang = disambiguate_language(v, langs)

                if lang != AMBIGUOUS_LANGUAGE and lang != UNKNOWN_LANGUAGE:
                    name_language[lang].append(v)

                ambiguous_already_seen.add(v)
        elif not has_alternate_names and k.startswith(tag_first_component) and (has_colon or ':' not in k) and normalize_osm_name_tag(k, script=True) == tag_last_component:
            if num_langs == 1:
                name_language[candidate_languages[0][0]].append(v)
            else:
                lang = disambiguate_language(v, candidate_languages)
                default_lang = candidate_languages[0][0]

                if lang == AMBIGUOUS_LANGUAGE:
                    return None, None
                elif lang == UNKNOWN_LANGUAGE and num_defaults == 1:
                    name_language[default_lang].append(v)
                elif lang != UNKNOWN_LANGUAGE:
                    if lang != default_lang and lang in country_langs and country_defaults > 1 and regional_defaults > 0 and lang in WELL_REPRESENTED_LANGUAGES:
                        return None, None
                    name_language[lang].append(v)
                else:
                    return None, None

    return country, name_language


def build_ways_training_data(country_rtree, infile, out_dir, abbreviate_streets=True):
    '''
    Creates a training set for language classification using most OSM ways
    (streets) under a fairly lengthy osmfilter definition which attempts to
    identify all roads/ways designated for motor vehicle traffic, which
    is more-or-less what we'd expect to see in addresses.

    The fields are {language, country, street name}. Example:

    ar      ma      ﺵﺍﺮﻋ ﻑﺎﻟ ﻮﻟﺩ ﻊﻤﻳﺭ
    '''
    i = 0
    f = open(os.path.join(out_dir, WAYS_LANGUAGE_DATA_FILENAME), 'w')
    writer = csv.writer(f, 'tsv_no_quote')

    for key, value, deps in parse_osm(infile, allowed_types=WAYS_RELATIONS):
        country, name_language = get_language_names(country_rtree, key, value, tag_prefix='name')
        if not name_language:
            continue

        for lang, val in name_language.iteritems():
            for v in val:
                for s in v.split(';'):
                    if lang in languages:
                        writer.writerow((lang, country, tsv_string(s)))
                        if not abbreviate_streets:
                            continue
                        abbrev = abbreviate(street_and_synonyms_gazetteer, s, lang)
                        if abbrev != s:
                            writer.writerow((lang, country, tsv_string(abbrev)))
            if i % 1000 == 0 and i > 0:
                print('did {} ways'.format(i))
            i += 1
    f.close()


NAME_KEYS = (
    'name',
    'addr:housename',
)

HOUSE_NUMBER_KEYS = (
    'addr:house_number',
    'addr:housenumber',
    'house_number'
)

COUNTRY_KEYS = (
    'country',
    'country_name',
    'addr:country',
    'is_in:country',
    'addr:country_code',
    'country_code',
    'is_in:country_code'
)

POSTAL_KEYS = (
    'postcode',
    'postal_code',
    'addr:postcode',
    'addr:postal_code',
)


def build_toponym_training_data(country_rtree, infile, out_dir):
    '''
    Data set of toponyms by language and country which should assist
    in language classification. OSM tends to use the native language
    by default (e.g. Москва instead of Moscow). Toponyms get messy
    due to factors like colonialism, historical names, name borrowing
    and the shortness of the names generally. In these cases
    we're more strict as to what constitutes a valid language for a
    given country.

    Example:
    ja      jp      東京都
    '''
    i = 0
    f = open(os.path.join(out_dir, TOPONYM_LANGUAGE_DATA_FILENAME), 'w')
    writer = csv.writer(f, 'tsv_no_quote')

    for key, value, deps in parse_osm(infile):
        if not any((k.startswith('name') for k, v in value.iteritems())):
            continue

        try:
            latitude, longitude = latlon_to_decimal(value['lat'], value['lon'])
        except Exception:
            continue


        osm_country_components = country_rtree.point_in_poly(latitude, longitude, return_all=True)
        country, candidate_languages = country_rtree.country_and_languages_from_components(osm_country_components)
        if not (country and candidate_languages):
            continue

        name_language = defaultdict(list)

        official = official_languages[country]

        default_langs = set([l for l, default in official.iteritems() if default])

        _, regional_langs = country_rtree.country_and_languages_from_components([c for c in osm_country_components if 'ISO3166-1:alpha2' not in c])

        top_lang = None
        if len(official) > 0:
            top_lang = official.iterkeys().next()

        # E.g. Hindi in India, Urdu in Pakistan
        if top_lang is not None and top_lang not in WELL_REPRESENTED_LANGUAGES and len(default_langs) > 1:
            default_langs -= WELL_REPRESENTED_LANGUAGES

        valid_languages = set([l for l, d in candidate_languages])

        '''
        WELL_REPRESENTED_LANGUAGES are languages like English, French, etc. for which we have a lot of data
        WELL_REPRESENTED_LANGUAGE_COUNTRIES are more-or-less the "origin" countries for said languages where
        we can take the place names as examples of the language itself (e.g. place names in France are examples
        of French, whereas place names in much of Francophone Africa tend to get their names from languages
        other than French, even though French is the official language.
        '''
        valid_languages -= set([lang for lang in valid_languages if lang in WELL_REPRESENTED_LANGUAGES and country not in WELL_REPRESENTED_LANGUAGE_COUNTRIES[lang]])

        valid_languages |= default_langs

        if not valid_languages:
            continue

        have_qualified_names = False

        for k, v in value.iteritems():
            if not k.startswith('name:'):
                continue

            norm = normalize_osm_name_tag(k)
            norm_sans_script = normalize_osm_name_tag(k, script=True)

            if norm in languages:
                lang = norm
            elif norm_sans_script in languages:
                lang = norm_sans_script
            else:
                continue

            if lang in valid_languages:
                have_qualified_names = True
                name_language[lang].append(v)

        if not have_qualified_names and len(regional_langs) <= 1 and 'name' in value and len(valid_languages) == 1:
            name_language[top_lang].append(value['name'])

        for k, v in name_language.iteritems():
            for s in v:
                s = s.strip()
                if not s:
                    continue
                writer.writerow((k, country, tsv_string(s)))
            if i % 1000 == 0 and i > 0:
                print('did {} toponyms'.format(i))
            i += 1

    f.close()


def build_address_training_data(country_rtree, infile, out_dir, format=False):
    '''
    Creates training set similar to the ways data but using addr:street tags instead.
    These may be slightly closer to what we'd see in real live addresses, containing
    variations, some abbreviations (although this is discouraged in OSM), etc.

    Example record:
    eu      es      Errebal kalea
    '''
    i = 0
    f = open(os.path.join(out_dir, ADDRESS_LANGUAGE_DATA_FILENAME), 'w')
    writer = csv.writer(f, 'tsv_no_quote')

    for key, value, deps in parse_osm(infile):
        country, street_language = get_language_names(country_rtree, key, value, tag_prefix='addr:street')
        if not street_language:
            continue

        for k, v in street_language.iteritems():
            for s in v:
                s = s.strip()
                if not s:
                    continue
                if k in languages:
                    writer.writerow((k, country, tsv_string(s)))
            if i % 1000 == 0 and i > 0:
                print('did {} streets'.format(i))
            i += 1

    f.close()

VENUE_LANGUAGE_DATA_FILENAME = 'names_by_language.tsv'


def build_venue_training_data(country_rtree, infile, out_dir):
    i = 0

    f = open(os.path.join(out_dir, VENUE_LANGUAGE_DATA_FILENAME), 'w')
    writer = csv.writer(f, 'tsv_no_quote')

    for key, value, deps in parse_osm(infile):
        country, name_language = get_language_names(country_rtree, key, value, tag_prefix='name')
        if not name_language:
            continue

        venue_type = None
        for key in (u'amenity', u'building'):
            amenity = value.get(key, u'').strip()
            if amenity in ('yes', 'y'):
                continue

            if amenity:
                venue_type = u':'.join([key, amenity])
                break

        if venue_type is None:
            continue

        for k, v in name_language.iteritems():
            for s in v:
                s = s.strip()
                if k in languages:
                    writer.writerow((k, country, safe_encode(venue_type), tsv_string(s)))
            if i % 1000 == 0 and i > 0:
                print('did, {} venues'.format(i))
            i += 1

    f.close()

if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-s', '--streets-file',
                        help='Path to planet-ways.osm')

    parser.add_argument('--unabbreviated',
                        action='store_true',
                        default=False,
                        help='Use unabbreviated street names for token counts')

    parser.add_argument('-a', '--address-file',
                        help='Path to planet-addresses.osm')

    parser.add_argument('-v', '--venues-file',
                        help='Path to planet-venues.osm')

    parser.add_argument('-b', '--borders-file',
                        help='Path to planet-borders.osm')

    parser.add_argument('-f', '--format',
                        action='store_true',
                        default=False,
                        help='Save formatted addresses (slow)')

    parser.add_argument('-u', '--untagged',
                        action='store_true',
                        default=False,
                        help='Save untagged formatted addresses (slow)')

    parser.add_argument('-l', '--limited-addresses',
                        action='store_true',
                        default=False,
                        help='Save formatted addresses without house names or country (slow)')

    parser.add_argument('-p', '--place-nodes-file',
                        help='Path to planet-admin-nodes.osm')

    parser.add_argument('-t', '--temp-dir',
                        default=tempfile.gettempdir(),
                        help='Temp directory to use')

    parser.add_argument('-x', '--intersections-file',
                        help='Path to planet-ways-latlons.osm')

    parser.add_argument('--country-rtree-dir',
                        required=True,
                        help='Country RTree directory')

    parser.add_argument('--rtree-dir',
                        default=None,
                        help='OSM reverse geocoder RTree directory')

    parser.add_argument('--places-index-dir',
                        default=None,
                        help='Places index directory')

    parser.add_argument('--metro-stations-index-dir',
                        default=None,
                        help='Metro stations reverse geocoder directory')

    parser.add_argument('--subdivisions-rtree-dir',
                        default=None,
                        help='Subdivisions reverse geocoder RTree directory')

    parser.add_argument('--buildings-rtree-dir',
                        default=None,
                        help='Buildings reverse geocoder RTree directory')

    parser.add_argument('--neighborhoods-rtree-dir',
                        default=None,
                        help='Neighborhoods reverse geocoder RTree directory')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    args = parser.parse_args()

    country_rtree = OSMCountryReverseGeocoder.load(args.country_rtree_dir)

    osm_rtree = None
    if args.rtree_dir:
        osm_rtree = OSMReverseGeocoder.load(args.rtree_dir)

    neighborhoods_rtree = None
    if args.neighborhoods_rtree_dir:
        neighborhoods_rtree = NeighborhoodReverseGeocoder.load(args.neighborhoods_rtree_dir)

    places_index = None
    if args.places_index_dir:
        places_index = PlaceReverseGeocoder.load(args.places_index_dir)

    metro_stations_index = None
    if args.metro_stations_index_dir:
        metro_stations_index = MetroStationReverseGeocoder.load(args.metro_stations_index_dir)

    subdivisions_rtree = None
    if args.subdivisions_rtree_dir:
        subdivisions_rtree = OSMSubdivisionReverseGeocoder.load(args.subdivisions_rtree_dir)

    buildings_rtree = None
    if args.buildings_rtree_dir:
        buildings_rtree = OSMBuildingReverseGeocoder.load(args.buildings_rtree_dir)

    # Can parallelize
    if args.streets_file and not args.format:
        build_ways_training_data(country_rtree, args.streets_file, args.out_dir, abbreviate_streets=not args.unabbreviated)
    if args.borders_file:
        build_toponym_training_data(country_rtree, args.borders_file, args.out_dir)
    if args.venues_file:
        build_venue_training_data(country_rtree, args.venues_file, args.out_dir)

    if args.address_file or args.intersections_file:
        if osm_rtree is None:
            parser.error('--rtree-dir required for formatted addresses')
        elif neighborhoods_rtree is None:
            parser.error('--neighborhoods-rtree-dir required for formatted addresses')
        elif places_index is None:
            parser.error('--places-index-dir required for formatted addresses')

    if args.address_file and args.format:
        components = AddressComponents(osm_rtree, neighborhoods_rtree, places_index)
        osm_formatter = OSMAddressFormatter(components, country_rtree, subdivisions_rtree, buildings_rtree, metro_stations_index)
        osm_formatter.build_training_data(args.address_file, args.out_dir, tag_components=not args.untagged)
    if args.address_file and args.limited_addresses:
        components = AddressComponents(osm_rtree, neighborhoods_rtree, places_index)
        osm_formatter = OSMAddressFormatter(components, country_rtree, subdivisions_rtree, buildings_rtree, metro_stations_index, splitter=u' ')
        osm_formatter.build_limited_training_data(args.address_file, args.out_dir)

    if args.place_nodes_file and args.format:
        components = AddressComponents(osm_rtree, neighborhoods_rtree, places_index)
        osm_formatter = OSMAddressFormatter(components, country_rtree, subdivisions_rtree, buildings_rtree, metro_stations_index)
        osm_formatter.build_place_training_data(args.place_nodes_file, args.out_dir, tag_components=not args.untagged)

    if args.intersections_file and args.format:
        components = AddressComponents(osm_rtree, neighborhoods_rtree, places_index)
        osm_formatter = OSMAddressFormatter(components, country_rtree, subdivisions_rtree, buildings_rtree, metro_stations_index)
        osm_formatter.build_intersections_training_data(args.intersections_file, args.out_dir, tag_components=not args.untagged)

    if args.streets_file and args.format:
        components = AddressComponents(osm_rtree, neighborhoods_rtree, places_index)
        osm_formatter = OSMAddressFormatter(components, country_rtree, subdivisions_rtree, buildings_rtree, metro_stations_index)
        osm_formatter.build_ways_training_data(args.streets_file, args.out_dir, tag_components=not args.untagged)
