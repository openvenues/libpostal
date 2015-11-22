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
python osm_address_training_data.py -s $(OSM_DIR)/planet-ways.osm --language-rtree-dir=$(RTREE_DIR) -o $(OUT_DIR)

Venues:
python osm_address_training_data.py -v $(OSM_DIR)/planet-venues.osm --language-rtree-dir=$(RTREE_DIR) -o $(OUT_DIR)

Address streets:
python osm_address_training_data.py -a $(OSM_DIR)/planet-addresses.osm --language-rtree-dir=$(LANG_RTREE_DIR) -o $(OUT_DIR)

Limited formatted addresses:
python osm_address_training_data.py -a -l $(OSM_DIR)/planet-addresses.osm --language-rtree-dir=$(LANG_RTREE_DIR) --rtree-dir=$(RTREE_DIR) --neighborhoods-rtree-dir=$(NEIGHBORHOODS_RTREE_DIR)  -o $(OUT_DIR)

Formatted addresses (tagged):
python osm_address_training_data.py -a $(OSM_DIR)/planet-addresses.osm -f --language-rtree-dir=$(LANG_RTREE_DIR) --neighborhoods-rtree-dir=$(NEIGHBORHOODS_RTREE_DIR) --rtree-dir=$(RTREE_DIR) -o $(OUT_DIR)

Formatted addresses (untagged):
python osm_address_training_data.py -a $(OSM_DIR)/planet-addresses.osm  -f -u --language-rtree-dir=$(LANG_RTREE_DIR) --neighborhoods-rtree-dir=$(NEIGHBORHOODS_RTREE_DIR)  --rtree-dir=$(RTREE_DIR) -o $(OUT_DIR)

Toponyms:
python osm_address_training_data.py -b $(OSM_DIR)/planet-borders.osm --language-rtree-dir=$(LANG_RTREE_DIR) -o $(OUT_DIR)
'''

import argparse
import csv
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

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir, os.pardir, 'python')))

from geodata.countries.country_names import *
from geodata.language_id.disambiguation import *
from geodata.language_id.sample import sample_random_language
from geodata.states.state_abbreviations import STATE_ABBREVIATIONS
from geodata.language_id.polygon_lookup import country_and_languages
from geodata.i18n.languages import *
from geodata.address_formatting.formatter import AddressFormatter
from geodata.osm.extract import *
from geodata.polygons.language_polys import *
from geodata.polygons.reverse_geocode import OSMReverseGeocoder
from geodata.i18n.unicode_paths import DATA_DIR

from geodata.csv_utils import *
from geodata.file_utils import *

this_dir = os.path.realpath(os.path.dirname(__file__))

# Input files
PLANET_ADDRESSES_INPUT_FILE = 'planet-addresses.osm'
PLANET_WAYS_INPUT_FILE = 'planet-ways.osm'
PLANET_VENUES_INPUT_FILE = 'planet-venues.osm'
PLANET_BORDERS_INPUT_FILE = 'planet-borders.osm'

# Output files
WAYS_LANGUAGE_DATA_FILENAME = 'streets_by_language.tsv'
ADDRESS_LANGUAGE_DATA_FILENAME = 'address_streets_by_language.tsv'
ADDRESS_FORMAT_DATA_TAGGED_FILENAME = 'formatted_addresses_tagged.tsv'
ADDRESS_FORMAT_DATA_FILENAME = 'formatted_addresses.tsv'
ADDRESS_FORMAT_DATA_LANGUAGE_FILENAME = 'formatted_addresses_by_language.tsv'
TOPONYM_LANGUAGE_DATA_FILENAME = 'toponyms_by_language.tsv'


class AddressComponent(object):
    '''
    Declare an address component and its dependencies e.g.
    a house_numer cannot be used in the absence of a road name.
    '''
    ANY = 'any'

    def __init__(self, name, dependencies=tuple(), method=ANY):
        self.name = name
        self.dependencies = dependencies

    def __hash__(self):
        return hash(self.name)

    def __cmp__(self, other):
        return cmp(self.name, other.name)


OSM_ADDRESS_COMPONENTS = OrderedDict.fromkeys([
    AddressComponent(AddressFormatter.HOUSE),
    AddressComponent(AddressFormatter.ROAD, dependencies=(AddressFormatter.HOUSE,
                                                          AddressFormatter.HOUSE_NUMBER,
                                                          AddressFormatter.SUBURB,
                                                          AddressFormatter.CITY,
                                                          AddressFormatter.POSTCODE)),
    AddressComponent(AddressFormatter.HOUSE_NUMBER, dependencies=(AddressFormatter.ROAD,)),
    AddressComponent(AddressFormatter.SUBURB, dependencies=(AddressFormatter.CITY, AddressFormatter.STATE,
                                                            AddressFormatter.POSTCODE)),
    AddressComponent(AddressFormatter.CITY),
    AddressComponent(AddressFormatter.STATE, dependencies=(AddressFormatter.SUBURB, AddressFormatter.CITY,
                                                           AddressFormatter.POSTCODE, AddressFormatter.COUNTRY)),
    AddressComponent(AddressFormatter.POSTCODE),
    AddressComponent(AddressFormatter.COUNTRY),
])


def num_deps(c):
    return len(c.dependencies)


OSM_ADDRESS_COMPONENTS_SORTED = sorted(OSM_ADDRESS_COMPONENTS, key=num_deps)

OSM_ADDRESS_COMPONENT_COMBINATIONS = []

'''
The following statements create a bitset of address components
for quickly checking testing whether or not a candidate set of
address components can be considered a full geographic string
suitable for formatting (i.e. would be a valid geocoder query).
For instance, a house number by itself is not sufficient
to be considered a valid address for this purpose unless it
has a road name as well. Using bitsets we can easily answer
questions like "Is house/house_number/road/city valid?"
'''
OSM_ADDRESS_COMPONENT_VALUES = {
    c.name: 1 << i
    for i, c in enumerate(OSM_ADDRESS_COMPONENTS.keys())
}

OSM_ADDRESS_COMPONENTS_VALID = set()


def component_bitset(components):
    return reduce(operator.or_, [OSM_ADDRESS_COMPONENT_VALUES[c] for c in components])


for i in xrange(1, len(OSM_ADDRESS_COMPONENTS.keys())):
    for perm in combinations(OSM_ADDRESS_COMPONENTS.keys(), i):
        perm_set = set([p.name for p in perm])
        valid = all((not p.dependencies or any(d in perm_set for d in p.dependencies) for p in perm))
        if valid:
            components = [c.name for c in perm]
            OSM_ADDRESS_COMPONENT_COMBINATIONS.append(tuple(components))
            OSM_ADDRESS_COMPONENTS_VALID.add(component_bitset(components))


class OSMField(object):
    def __init__(self, name, c_constant, alternates=None):
        self.name = name
        self.c_constant = c_constant
        self.alternates = alternates

osm_fields = [
    # Field if alternate_names present, default field name if not, C header constant
    OSMField('addr:housename', 'OSM_HOUSE_NAME'),
    OSMField('addr:housenumber', 'OSM_HOUSE_NUMBER'),
    OSMField('addr:block', 'OSM_BLOCK'),
    OSMField('addr:street', 'OSM_STREET_ADDRESS'),
    OSMField('addr:place', 'OSM_PLACE'),
    OSMField('addr:city', 'OSM_CITY', alternates=['addr:locality', 'addr:municipality', 'addr:hamlet']),
    OSMField('addr:suburb', 'OSM_SUBURB'),
    OSMField('addr:neighborhood', 'OSM_NEIGHBORHOOD', alternates=['addr:neighbourhood']),
    OSMField('addr:district', 'OSM_DISTRICT'),
    OSMField('addr:subdistrict', 'OSM_SUBDISTRICT'),
    OSMField('addr:ward', 'OSM_WARD'),
    OSMField('addr:state', 'OSM_STATE'),
    OSMField('addr:province', 'OSM_PROVINCE'),
    OSMField('addr:postcode', 'OSM_POSTAL_CODE', alternates=['addr:postal_code']),
    OSMField('addr:country', 'OSM_COUNTRY'),
]


def write_osm_json(filename, out_filename):
    out = open(out_filename, 'w')
    writer = csv.writer(out, 'tsv_no_quote')
    for key, attrs, deps in parse_osm(filename):
        writer.writerow((key, json.dumps(attrs)))
    out.close()


def read_osm_json(filename):
    reader = csv.reader(open(filename), delimiter='\t')
    for key, attrs in reader:
        yield key, json.loads(attrs)


def normalize_osm_name_tag(tag, script=False):
    norm = tag.rsplit(':', 1)[-1]
    if not script:
        return norm
    return norm.split('_', 1)[0]


def get_language_names(language_rtree, key, value, tag_prefix='name'):
    if not ('lat' in value and 'lon' in value):
        return None, None

    has_colon = ':' in tag_prefix
    tag_first_component = tag_prefix.split(':')[0]
    tag_last_component = tag_prefix.split(':')[-1]

    try:
        latitude, longitude = latlon_to_decimal(value['lat'], value['lon'])
    except Exception:
        return None, None

    country, candidate_languages, language_props = country_and_languages(language_rtree, latitude, longitude)
    if not (country and candidate_languages):
        return None, None

    num_langs = len(candidate_languages)
    default_langs = set([l['lang'] for l in candidate_languages if l.get('default')])
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
    for p in language_props:
        if p['admin_level'] > 0:
            regional_defaults += sum((1 for lang in p['languages'] if lang.get('default')))
            regional_langs |= set([l['lang'] for l in p['languages']])
        else:
            country_defaults += sum((1 for lang in p['languages'] if lang.get('default')))
            country_langs |= set([l['lang'] for l in p['languages']])

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
                name_language[candidate_languages[0]['lang']].append(v)
            else:
                lang = disambiguate_language(v, [(l['lang'], l['default']) for l in candidate_languages])
                default_lang = candidate_languages[0]['lang']

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


def build_ways_training_data(language_rtree, infile, out_dir):
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
        country, name_language = get_language_names(language_rtree, key, value, tag_prefix='name')
        if not name_language:
            continue

        for k, v in name_language.iteritems():
            for s in v:
                if k in languages:
                    writer.writerow((k, country, tsv_string(s)))
            if i % 1000 == 0 and i > 0:
                print 'did', i, 'ways'
            i += 1
    f.close()

OSM_IGNORE_KEYS = (
    'house',
)


def strip_keys(value, ignore_keys):
    for key in ignore_keys:
        value.pop(key, None)


def osm_reverse_geocoded_components(address_components, admin_rtree, country, latitude, longitude):
    ret = defaultdict(list)
    for props in admin_rtree.point_in_poly(latitude, longitude, return_all=True):
        name = props.get('name')
        if not name:
            continue

        for k, v in props.iteritems():
            normalized_key = osm_address_components.get_component(country, k, v)
            if normalized_key:
                ret[normalized_key].append(props)
    return ret


def build_address_format_training_data(admin_rtree, language_rtree, neighborhoods_rtree, infile, out_dir, tag_components=True):
    '''
    Creates formatted address training data for supervised sequence labeling (or potentially 
    for unsupervised learning e.g. for word vectors) using addr:* tags in OSM.

    Example:

    cs  cz  Gorkého/road ev.2459/house_number | 40004/postcode Trmice/city | CZ/country

    The field structure is similar to other training data created by this script i.e.
    {language, country, data}. The data field here is a sequence of labeled tokens similar
    to what we might see in part-of-speech tagging.


    This format uses a special character "|" to denote possible breaks in the input (comma, newline).

    Note that for the address parser, we'd like it to be robust to many different types
    of input, so we may selectively eleminate components

    This information can potentially be used downstream by the sequence model as these
    breaks may be present at prediction time.

    Example:

    sr      rs      Crkva Svetog Arhangela Mihaila | Vukov put BB | 15303 Trsic

    This may be useful in learning word representations, statistical phrases, morphology
    or other models requiring only the sequence of words.
    '''
    i = 0

    formatter = AddressFormatter()
    osm_address_components.configure()

    if tag_components:
        formatted_tagged_file = open(os.path.join(out_dir, ADDRESS_FORMAT_DATA_TAGGED_FILENAME), 'w')
        writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')
    else:
        formatted_file = open(os.path.join(out_dir, ADDRESS_FORMAT_DATA_FILENAME), 'w')
        writer = csv.writer(formatted_file, 'tsv_no_quote')

    remove_keys = OSM_IGNORE_KEYS

    for key, value, deps in parse_osm(infile):
        try:
            latitude, longitude = latlon_to_decimal(value['lat'], value['lon'])
        except Exception:
            continue

        country, candidate_languages, language_props = country_and_languages(language_rtree, latitude, longitude)
        if not (country and candidate_languages):
            continue

        for key in remove_keys:
            _ = value.pop(key, None)

        language = None
        if tag_components:
            if len(candidate_languages) == 1:
                language = candidate_languages[0]['lang']
            else:
                street = value.get('addr:street', None)
                if street is None:
                    continue
                language = disambiguate_language(street, [(l['lang'], l['default']) for l in candidate_languages])

        address_components = {k: v for k, v in value.iteritems() if k.startswith('addr:') or k == 'name'}
        formatter.replace_aliases(address_components)

        address_country = address_components.get(AddressFormatter.COUNTRY)

        '''
        Country names
        -------------

        In OSM, addr:country is almost always an ISO-3166 alpha-2 country code.
        However, we'd like to expand these to include natural language forms
        of the country names we might be likely to encounter in a geocoder or
        handwritten address.

        These splits are somewhat arbitrary but could potentially be fit to data
        from OpenVenues or other sources on the usage of country name forms.

        If the address includes a country, the selection procedure proceeds as follows:

        1. With probability a, select the country name in the language of the address
           (determined above), or with the localized country name if the language is
           undtermined or ambiguous.

        2. With probability b(1-a), sample a language from the distribution of
           languages on the Internet and use the country's name in that language.

        3. This is implicit, but with probability (1-b)(1-a), keep the country code
        '''

        non_local_language = None

        # 1. use the country name in the current language or the country's local language
        if address_country and random.random() < 0.8:
            localized = None
            if language and language not in (AMBIGUOUS_LANGUAGE, UNKNOWN_LANGUAGE):
                localized = language_country_names.get(language, {}).get(address_country.upper())

            if not localized:
                localized = country_localized_display_name(address_country.lower())

            if localized:
                address_components[AddressFormatter.COUNTRY] = localized
        # 2. country's name in a language samples from the distribution of languages on the Internet
        elif address_country and random.random() < 0.5:
            non_local_language = sample_random_language()
            lang_country = language_country_names.get(non_local_language, {}).get(address_country.upper())
            if lang_country:
                address_components[AddressFormatter.COUNTRY] = lang_country
        # 3. Implicit: the rest of the time keep the country code

        '''
        States
        ------

        Primarily for the US, Canada and Australia, OSM tends to use the abbreviated state name
        whereas we'd like to include both forms, so wtih some probability, replace the abbreviated
        name with the unabbreviated one e.g. CA => California
        '''
        address_state = address_components.get(AddressFormatter.STATE)

        if address_state:
            state_full_name = STATE_ABBREVIATIONS.get(country.upper(), {}).get(address_state.upper(), {}).get(language)

            if state_full_name and random.random() < 0.3:
                address_components[AddressFormatter.STATE] = state_full_name

        '''
        OSM boundaries
        --------------

        For many addresses, the city, district, region, etc. are all implicitly
        generated by the reverse geocoder e.g. we do not need an addr:city tag
        to identify that 40.74, -74.00 is in New York City as well as its parent
        geographies (New York county, New York state, etc.)

        Where possible we augment the addr:* tags with some of the reverse-geocoded
        relations from OSM.

        Since addresses found on the web may have the same properties, we
        include these qualifiers in the training data.
        '''

        osm_components = osm_reverse_geocoded_components(address_components, admin_rtree, country, latitude, longitude)
        if osm_components:
            if non_local_language is not None:
                suffix = ':{}'.format(non_local_language)
            else:
                suffix = ''

            name_key = ''.join(('name', suffix))
            raw_name_key = 'name'
            short_name_key = ''.join(('short_name', suffix))
            raw_short_name_key = 'short_name'
            alt_name_key = ''.join('alt_name', suffix)
            raw_alt_name_key = 'alt_name'
            official_name_key = ''.join('official_name', suffix)
            raw_official_name_key = 'official_name'

            poly_components = defaultdict(list)

            for component, values in osm_components.iteritems():
                seen = set()

                # Choose which name to use with given probabilities
                r = random.random()
                if r < 0.1:
                    # 10% of the time use the short name
                    key = short_name_key
                    raw_key = raw_short_name_key
                elif r < 0.2:
                    # 10% of the time use the official name
                    key = official_name_key
                    raw_key = raw_official_name_key
                elif r < 0.3:
                    # 10% of the time use the official name
                    key = alt_name_key
                    raw_key = raw_alt_name_key
                else:
                    # 70% of the time use the name tag
                    key = name_key
                    raw_key = raw_name_key

                for value in values:
                    name = value.get(key, value.get(raw_key))

                    if not name:
                        name = value.get(name_key, value.get(raw_name_key))

                    if not name:
                        continue

                    if (component, name) not in seen:
                        poly_components[component].append(name)
                        seen.add((component, name))

            for component, vals in poly_components.iteritems():
                if component not in address_components:
                    address_components[component] = u', '.join(vals)

        '''
        Neighborhoods
        -------------

        In some cities, neighborhoods may be included in a free-text address.

        OSM includes many neighborhoods but only as points, rather than the polygons
        needed to perform reverse-geocoding. We use a hybrid index containing
        Quattroshapes/Zetashapes polygons matched fuzzily with OSM names (which are
        on the whole of better quality).
        '''

        neighborhood = neighborhoods_rtree.point_in_poly(latitude, longitude)
        if neighborhood and AddressFormatter.SUBURB not in address_components:
            address_components[AddressFormatter.SUBURB] = neighborhood['name']

        # Version with all components
        formatted_address = formatter.format_address(country, address_components, tag_components=tag_components, minimal_only=not tag_components)

        if tag_components:
            formatted_addresses = []
            formatted_addresses.append(formatted_address)

            address_components = {k: v for k, v in address_components.iteritems() if k in OSM_ADDRESS_COMPONENT_VALUES}
            if not address_components:
                continue

            current_components = component_bitset(address_components.keys())

            for component in address_components.keys():
                if current_components ^ OSM_ADDRESS_COMPONENT_VALUES[component] in OSM_ADDRESS_COMPONENTS_VALID and random.random() < 0.5:
                    address_components.pop(component)
                    current_components ^= OSM_ADDRESS_COMPONENT_VALUES[component]
                    if not address_components:
                        break

                    formatted_address = formatter.format_address(country, address_components, tag_components=tag_components, minimal_only=False)
                    formatted_addresses.append(formatted_address)

            for formatted_address in formatted_addresses:
                if formatted_address and formatted_address.strip():
                    formatted_address = tsv_string(formatted_address)
                    if not formatted_address or not formatted_address.strip():
                        continue
                    row = (language, country, formatted_address)

                    writer.writerow(row)
        elif formatted_address and formatted_address.strip():
            formatted_address = tsv_string(formatted_address)
            writer.writerow([formatted_address])

        i += 1
        if i % 1000 == 0 and i > 0:
            print 'did', i, 'formatted addresses'

NAME_KEYS = (
    'name',
    'addr:housename',
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


def build_address_format_training_data_limited(rtree, language_rtree, infile, out_dir):
    '''
    Creates a special kind of formatted address training data from OSM's addr:* tags
    but are designed for use in language classification. These records are similar 
    to the untagged formatted records but include the language and country
    (suitable for concatenation with the rest of the language training data),
    and remove several fields like country which usually do not contain helpful
    information for classifying the language.

    Example:

    nb      no      Olaf Ryes Plass 8 | Oslo
    '''
    i = 0

    formatter = AddressFormatter()

    f = open(os.path.join(out_dir, ADDRESS_FORMAT_DATA_LANGUAGE_FILENAME), 'w')
    writer = csv.writer(f, 'tsv_no_quote')

    remove_keys = NAME_KEYS + COUNTRY_KEYS + POSTAL_KEYS + OSM_IGNORE_KEYS

    for key, value, deps in parse_osm(infile):
        try:
            latitude, longitude = latlon_to_decimal(value['lat'], value['lon'])
        except Exception:
            continue

        for k in remove_keys:
            _ = value.pop(k, None)

        if not value:
            continue

        country, name_language = get_language_names(language_rtree, key, value, tag_prefix='addr:street')
        if not name_language:
            continue

        single_language = len(name_language) == 1
        for lang, val in name_language.iteritems():
            if lang not in languages:
                continue

            address_dict = value.copy()
            for k in address_dict.keys():
                namespaced_val = u'{}:{}'.format(k, lang)
                if namespaced_val in address_dict:
                    address_dict[k] = address_dict[namespaced_val]
                elif not single_language:
                    address_dict.pop(k)

            if not address_dict:
                continue

            formatted_address_untagged = formatter.format_address(country, address_dict, tag_components=False)
            if formatted_address_untagged is not None:
                formatted_address_untagged = tsv_string(formatted_address_untagged)

                writer.writerow((lang, country, formatted_address_untagged))

        i += 1
        if i % 1000 == 0 and i > 0:
            print 'did', i, 'formatted addresses'


def build_toponym_training_data(language_rtree, infile, out_dir):
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
        if not sum((1 for k, v in value.iteritems() if k.startswith('name'))) > 0:
            continue

        try:
            latitude, longitude = latlon_to_decimal(value['lat'], value['lon'])
        except Exception:
            continue

        country, candidate_languages, language_props = country_and_languages(language_rtree, latitude, longitude)
        if not (country and candidate_languages):
            continue

        name_language = defaultdict(list)

        official = official_languages[country]

        default_langs = set([l for l, default in official.iteritems() if default])

        regional_langs = list(chain(*(p['languages'] for p in language_props if p.get('admin_level', 0) > 0)))

        top_lang = None
        if len(official) > 0:
            top_lang = official.iterkeys().next()

        # E.g. Hindi in India, Urdu in Pakistan
        if top_lang is not None and top_lang not in WELL_REPRESENTED_LANGUAGES and len(default_langs) > 1:
            default_langs -= WELL_REPRESENTED_LANGUAGES

        valid_languages = set([l['lang'] for l in candidate_languages])

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
                print 'did', i, 'toponyms'
            i += 1

    f.close()


def build_address_training_data(langauge_rtree, infile, out_dir, format=False):
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
        country, street_language = get_language_names(language_rtree, key, value, tag_prefix='addr:street')
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
                print 'did', i, 'streets'
            i += 1

    f.close()

VENUE_LANGUAGE_DATA_FILENAME = 'names_by_language.tsv'


def build_venue_training_data(language_rtree, infile, out_dir):
    i = 0

    f = open(os.path.join(out_dir, VENUE_LANGUAGE_DATA_FILENAME), 'w')
    writer = csv.writer(f, 'tsv_no_quote')

    for key, value, deps in parse_osm(infile):
        country, name_language = get_language_names(language_rtree, key, value, tag_prefix='name')
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
                print 'did', i, 'venues'
            i += 1

    f.close()

if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-s', '--streets-file',
                        help='Path to planet-ways.osm')

    parser.add_argument('-a', '--address-file',
                        help='Path to planet-addresses.osm')

    parser.add_argument('-v', '--venues-file',
                        help='Path to planet-venues.osm')

    parser.add_argument('-b', '--borders-file',
                        help='Path to planet-borders.osm')

    parser.add_argument('-f', '--format-only',
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

    parser.add_argument('-t', '--temp-dir',
                        default=tempfile.gettempdir(),
                        help='Temp directory to use')

    parser.add_argument('-g', '--language-rtree-dir',
                        required=True,
                        help='Language RTree directory')

    parser.add_argument('-r', '--osm-rtree-dir',
                        default=None,
                        help='OSM reverse geocoder RTree directory')

    parser.add_argument('-q', '--quattroshapes-rtree-dir',
                        default=None,
                        help='Quattroshapes reverse geocoder RTree directory')

    parser.add_argument('-n', '--neighborhoods-rtree-dir',
                        default=None,
                        help='Neighborhoods reverse geocoder RTree directory')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    args = parser.parse_args()

    init_country_names()
    init_languages()

    language_rtree = LanguagePolygonIndex.load(args.language_rtree_dir)
    rtree = None
    if args.osm_rtree_dir:
        osm_rtree = OSMReverseGeocoder.load(args.osm_rtree_dir)

    if args.quattroshapes_rtree_dir:
        quattroshapes_rtree = QuattroshapesReverseGeocoder.load(args.quattroshapes_rtree_dir)

    street_types_gazetteer.configure()

    # Can parallelize
    if args.streets_file:
        build_ways_training_data(language_rtree, args.streets_file, args.out_dir)
    if args.borders_file:
        build_toponym_training_data(language_rtree, args.borders_file, args.out_dir)

    if args.address_file and not args.format_only and not args.limited_addresses:
        build_address_training_data(language_rtree, args.address_file, args.out_dir)
    elif args.address_file and rtree is None:
        parser.error('--rtree-dir required for formatted addresses')

    if args.address_file and args.format_only:
        build_address_format_training_data(rtree, language_rtree, args.address_file, args.out_dir, tag_components=not args.untagged)
    if args.address_file and args.limited_addresses:
        build_address_format_training_data_limited(rtree, language_rtree, args.address_file, args.out_dir)
    if args.venues_file:
        build_venue_training_data(rtree, language_rtree, args.venues_file, args.out_dir)
