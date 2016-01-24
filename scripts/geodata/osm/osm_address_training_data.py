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

Limited formatted addresses:
python osm_address_training_data.py -a -l $(OSM_DIR)/planet-addresses.osm --language-rtree-dir=$(LANG_RTREE_DIR) --rtree-dir=$(RTREE_DIR) --neighborhoods-rtree-dir=$(NEIGHBORHOODS_RTREE_DIR)  -o $(OUT_DIR)

Formatted addresses (tagged):
python osm_address_training_data.py -a $(OSM_DIR)/planet-addresses.osm -f --language-rtree-dir=$(LANG_RTREE_DIR) --neighborhoods-rtree-dir=$(NEIGHBORHOODS_RTREE_DIR) --rtree-dir=$(RTREE_DIR) --quattroshapes-rtree-dir=$(QS_TREE_DIR) --geonames-db=$(GEONAMES_DB_PATH) -o $(OUT_DIR)

Formatted addresses (untagged):
python osm_address_training_data.py -a $(OSM_DIR)/planet-addresses.osm  -f -u --language-rtree-dir=$(LANG_RTREE_DIR) --neighborhoods-rtree-dir=$(NEIGHBORHOODS_RTREE_DIR)  --rtree-dir=$(RTREE_DIR) --quattroshapes-rtree-dir=$(QS_TREE_DIR) --geonames-db=$(GEONAMES_DB_PATH) -o $(OUT_DIR)

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

from geodata.address_expansions.gazetteers import *
from geodata.coordinates.conversion import *
from geodata.countries.country_names import *
from geodata.geonames.db import GeoNamesDB
from geodata.language_id.disambiguation import *
from geodata.language_id.sample import sample_random_language
from geodata.states.state_abbreviations import STATE_ABBREVIATIONS, STATE_EXPANSIONS
from geodata.language_id.polygon_lookup import country_and_languages
from geodata.i18n.languages import *
from geodata.address_formatting.formatter import AddressFormatter
from geodata.names.normalization import replace_name_prefixes, replace_name_suffixes
from geodata.osm.extract import *
from geodata.polygons.language_polys import *
from geodata.polygons.reverse_geocode import *
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
    AddressComponent(AddressFormatter.CITY_DISTRICT, dependencies=(AddressFormatter.CITY,)),
    AddressComponent(AddressFormatter.CITY),
    AddressComponent(AddressFormatter.STATE_DISTRICT, dependencies=(AddressFormatter.STATE, AddressFormatter.POSTCODE)),
    AddressComponent(AddressFormatter.STATE, dependencies=(AddressFormatter.SUBURB, AddressFormatter.CITY,
                                                           AddressFormatter.POSTCODE, AddressFormatter.COUNTRY)),
    AddressComponent(AddressFormatter.POSTCODE),
    AddressComponent(AddressFormatter.COUNTRY),
])


def num_deps(c):
    return len(c.dependencies)


RANDOM_VALUE_REPLACEMENTS = {
    # Key: address component
    AddressFormatter.COUNTRY: {
        # value: (replacement, probability)
        'GB': ('UK', 0.3),
        'United Kingdom': ('UK', 0.3),
    }
}


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


BOUNDARY_COMPONENTS = (
    AddressFormatter.SUBURB,
    AddressFormatter.CITY_DISTRICT,
    AddressFormatter.CITY,
    AddressFormatter.STATE_DISTRICT,
    AddressFormatter.STATE
)


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


ALL_LANGUAGES = 'all'

LOWER, UPPER, TITLE, MIXED = range(4)


def token_capitalization(s):
    if s.istitle():
        return TITLE
    elif s.islower():
        return LOWER
    elif s.isupper():
        return UPPER
    else:
        return MIXED


def recase_abbreviation(expansion, tokens):
    expansion_tokens = expansion.split()
    if len(tokens) > len(expansion_tokens) and all((token_capitalization(t) != LOWER for t, c in tokens)):
        return expansion.upper()
    elif len(tokens) == len(expansion_tokens):
        strings = []
        for (t, c), e in zip(tokens, expansion_tokens):
            cap = token_capitalization(t)
            if cap == LOWER:
                strings.append(e.lower())
            elif cap == UPPER:
                strings.append(e.upper())
            elif cap == TITLE:
                strings.append(e.title())
            elif t.lower() == e.lower():
                strings.append(t)
            else:
                strings.append(e.title())
        return u' '.join(strings)
    else:
        return u' '.join([t.title() for t in expansion_tokens])


def osm_abbreviate(gazetteer, s, language, abbreviate_prob=0.3, separate_prob=0.2):
    '''
    Abbreviations
    -------------

    OSM discourages abbreviations, but to make our training data map better
    to real-world input, we can safely replace the canonical phrase with an
    abbreviated version and retain the meaning of the words
    '''
    raw_tokens = tokenize_raw(s)
    s_utf8 = safe_encode(s)
    tokens = [(safe_decode(s_utf8[o:o + l]), token_types.from_id(c)) for o, l, c in raw_tokens]
    norm_tokens = [(t.lower() if c in token_types.WORD_TOKEN_TYPES else t, c) for t, c in tokens]

    n = len(tokens)

    abbreviated = []

    i = 0

    for t, c, length, data in gazetteer.filter(norm_tokens):
        if c is PHRASE:
            valid = []
            data = [d.split('|') for d in data]

            added = False

            if random.random() > abbreviate_prob:
                for j, (t_i, c_i) in enumerate(t):
                    abbreviated.append(tokens[i + j][0])
                    if i + j < n - 1 and raw_tokens[i + j + 1][0] > sum(raw_tokens[i + j][:2]):
                        abbreviated.append(u' ')
                i += len(t)
                continue

            for lang, dictionary, is_canonical, canonical in data:
                if lang not in (language, 'all'):
                    continue

                is_canonical = int(is_canonical)
                is_stopword = dictionary == 'stopword'
                is_prefix = dictionary.startswith('concatenated_prefixes')
                is_suffix = dictionary.startswith('concatenated_suffixes')
                is_separable = is_prefix or is_suffix and dictionary.endswith('_separable') and len(t[0][0]) > length

                suffix = None
                prefix = None

                if not is_canonical:
                    continue

                if not is_prefix and not is_suffix:
                    abbreviations = gazetteer.canonicals.get((canonical, lang, dictionary))
                    token = random.choice(abbreviations) if abbreviations else canonical
                    token = recase_abbreviation(token, tokens[i:i + len(t)])
                    abbreviated.append(token)
                    if i + len(t) < n and raw_tokens[i + len(t)][0] > sum(raw_tokens[i + len(t) - 1][:2]):
                        abbreviated.append(u' ')
                    break
                elif is_prefix:
                    token = tokens[i][0]
                    prefix, token = token[:length], token[length:]
                    abbreviated.append(prefix)
                    if random.random() < separate_prob:
                        abbreviated.append(u' ')
                    if token.islower():
                        abbreviated.append(token.title())
                    else:
                        abbreviated.append(token)
                    abbreviated.append(u' ')
                    break
                elif is_suffix:
                    token = tokens[i][0]

                    token, suffix = token[:-length], token[-length:]

                    concatenated_abbreviations = gazetteer.canonicals.get((canonical, lang, dictionary), [])

                    separated_abbreviations = []
                    phrase = gazetteer.trie.get(suffix.rstrip('.'))
                    suffix_data = [safe_decode(d).split(u'|') for d in (phrase or [])]
                    for l, d, _, c in suffix_data:
                        if l == lang and c == canonical:
                            separated_abbreviations.extend(gazetteer.canonicals.get((canonical, lang, d)))

                    separate = random.random() < separate_prob

                    if concatenated_abbreviations and not separate:
                        abbreviation = random.choice(concatenated_abbreviations)
                    elif separated_abbreviations:
                        abbreviation = random.choice(separated_abbreviations)
                    else:
                        abbreviation = canonical

                    abbreviated.append(token)
                    if separate:
                        abbreviated.append(u' ')
                    if suffix.isupper():
                        abbreviated.append(abbreviation.upper())
                    elif separate:
                        abbreviated.append(abbreviation.title())
                    else:
                        abbreviated.append(abbreviation)
                    abbreviated.append(u' ')
                    break
            else:
                for j, (t_i, c_i) in enumerate(t):
                    abbreviated.append(tokens[i + j][0])
                    if i + j < n - 1 and raw_tokens[i + j + 1][0] > sum(raw_tokens[i + j][:2]):
                        abbreviated.append(u' ')
            i += len(t)

        else:
            abbreviated.append(tokens[i][0])
            if i < n - 1 and raw_tokens[i + 1][0] > sum(raw_tokens[i][:2]):
                abbreviated.append(u' ')
            i += 1

    return u''.join(abbreviated).strip()


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

        for lang, val in name_language.iteritems():
            for v in val:
                for s in v.split(';'):
                    if lang in languages:
                        writer.writerow((lang, country, tsv_string(s)))
                        abbrev = osm_abbreviate(street_types_gazetteer, s, lang)
                        if abbrev != s:
                            writer.writerow((lang, country, tsv_string(abbrev)))
            if i % 1000 == 0 and i > 0:
                print('did {} ways'.format(i))
            i += 1
    f.close()

OSM_IGNORE_KEYS = (
    'house',
)


def strip_keys(value, ignore_keys):
    for key in ignore_keys:
        value.pop(key, None)


def osm_reverse_geocoded_components(admin_rtree, country, latitude, longitude):
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


class OSMAddressFormatter(object):
    alpha3_codes = {c.alpha2: c.alpha3 for c in pycountry.countries}

    rare_components = {
        AddressFormatter.SUBURB,
        AddressFormatter.CITY_DISTRICT,
        AddressFormatter.STATE_DISTRICT,
        AddressFormatter.STATE,
    }

    state_important = {
        'US',
        'CA',
    }

    def __init__(self, admin_rtree, language_rtree, neighborhoods_rtree, quattroshapes_rtree, geonames, splitter=None):
        self.admin_rtree = admin_rtree
        self.language_rtree = language_rtree
        self.neighborhoods_rtree = neighborhoods_rtree
        self.quattroshapes_rtree = quattroshapes_rtree
        self.geonames = geonames
        self.formatter = AddressFormatter(splitter=splitter)
        osm_address_components.configure()

    def pick_language(self, value, candidate_languages, pick_namespaced_language_prob=0.6):
        language = None

        if len(candidate_languages) == 1:
            language = candidate_languages[0]['lang']
        else:
            street = value.get('addr:street', None)

            namespaced = [l['lang'] for l in candidate_languages if 'addr:street:{}'.format(l['lang']) in value]

            if street is not None and not namespaced:
                language = disambiguate_language(street, [(l['lang'], l['default']) for l in candidate_languages])
            elif namespaced and random.random() < pick_namespaced_language_prob:
                language = random.choice(namespaced)
                lang_suffix = ':{}'.format(language)
                for k in value:
                    if k.startswith('addr:') and k.endswith(lang_suffix):
                        value[k.rstrip(lang_suffix)] = value[k]
            else:
                language = UNKNOWN_LANGUAGE

        return language

    def pick_random_name_key(self, suffix=''):
        name_key = ''.join(('name', suffix))
        raw_name_key = 'name'
        short_name_key = ''.join(('short_name', suffix))
        raw_short_name_key = 'short_name'
        alt_name_key = ''.join(('alt_name', suffix))
        raw_alt_name_key = 'alt_name'
        official_name_key = ''.join(('official_name', suffix))
        raw_official_name_key = 'official_name'

        # Choose which name to use with given probabilities
        r = random.random()
        if r < 0.7:
            # 70% of the time use the name tag
            key = name_key
            raw_key = raw_name_key
        elif r < 0.8:
            # 10% of the time use the short name
            key = short_name_key
            raw_key = raw_short_name_key
        elif r < 0.9:
            # 10% of the time use the official name
            key = official_name_key
            raw_key = raw_official_name_key
        else:
            # 10% of the time use the official name
            key = alt_name_key
            raw_key = raw_alt_name_key

        return key, raw_key

    def normalize_address_components(self, value):
        address_components = {k: v for k, v in value.iteritems() if k in self.formatter.aliases}
        self.formatter.replace_aliases(address_components)
        return address_components

    def abbreviated_street(self, street, language, abbreviate_prob=0.3, separate_prob=0.2):
        '''
        Street abbreviations
        --------------------

        Use street and unit type dictionaries to probabilistically abbreviate
        phrases. Because the abbreviation is picked at random, this should
        help bridge the gap between OSM addresses and user input, in addition
        to capturing some non-standard abbreviations/surface forms which may be
        missing or sparse in OSM.
        '''
        return osm_abbreviate(street_and_unit_types_gazetteer, street, language,
                              abbreviate_prob=abbreviate_prob, separate_prob=separate_prob)

    def abbreviated_venue_name(self, name, language, abbreviate_prob=0.2, separate_prob=0.0):
        '''
        Venue abbreviations
        -------------------

        Use street and unit type dictionaries to probabilistically abbreviate
        phrases. Because the abbreviation is picked at random, this should
        help bridge the gap between OSM addresses and user input, in addition
        to capturing some non-standard abbreviations/surface forms which may be
        missing or sparse in OSM.
        '''
        return osm_abbreviate(names_gazetteer, name, language,
                              abbreviate_prob=abbreviate_prob, separate_prob=separate_prob)

    def country_name(self, address_components, country_code, language,
                     use_country_code_prob=0.3,
                     local_language_name_prob=0.6,
                     random_language_name_prob=0.1,
                     alpha_3_iso_code_prob=0.1,
                     ):
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

        address_country = address_components.get(AddressFormatter.COUNTRY)

        if random.random() < use_country_code_prob:
            # 30% of the time: add Quattroshapes country
            address_country = country_code.upper()

        r = random.random()

        # 1. 60% of the time: use the country name in the current language or the country's local language
        if address_country and r < local_language_name_prob:
            localized = None
            if language and language not in (AMBIGUOUS_LANGUAGE, UNKNOWN_LANGUAGE):
                localized = language_country_names.get(language, {}).get(address_country.upper())

            if not localized:
                localized = country_localized_display_name(address_country.lower())

            if localized:
                address_country = localized
        # 2. 10% of the time: country's name in a language samples from the distribution of languages on the Internet
        elif address_country and r < local_language_name_prob + random_language_name_prob:
            non_local_language = sample_random_language()
            lang_country = language_country_names.get(non_local_language, {}).get(address_country.upper())
            if lang_country:
                address_country = lang_country
        # 3. 10% of the time: use the country's alpha-3 ISO code
        elif address_country and r < local_language_name_prob + random_language_name_prob + alpha_3_iso_code_prob:
            iso_code_alpha3 = self.alpha3_codes.get(address_country)
            if iso_code_alpha3:
                address_country = iso_code_alpha3
        # 4. Implicit: the rest of the time keep the alpha-2 country code

        return address_country, non_local_language

    def venue_names(self, value):
        '''
        Venue names
        -----------

        Some venues have multiple names listed in OSM, grab them all
        With a certain probability, add None to the list so we drop the name
        '''

        venue_names = []
        for key in ('name', 'alt_name', 'loc_name', 'int_name', 'old_name'):
            venue_name = value.get(key)
            if venue_name:
                venue_names.append(venue_name)
        return venue_names

    def state_name(self, address_components, country, language, non_local_language=None, state_full_name_prob=0.4):
        '''
        States
        ------

        Primarily for the US, Canada and Australia, OSM tends to use the abbreviated state name
        whereas we'd like to include both forms, so wtih some probability, replace the abbreviated
        name with the unabbreviated one e.g. CA => California
        '''
        address_state = address_components.get(AddressFormatter.STATE)

        if address_state and country and not non_local_language:
            state_full_name = STATE_ABBREVIATIONS.get(country.upper(), {}).get(address_state.upper(), {}).get(language)

            if state_full_name and random.random() < state_full_name_prob:
                address_state = state_full_name
        elif address_state and non_local_language:
            _ = address_components.pop(AddressFormatter.STATE, None)
            address_state = None
        return address_state

    def tag_suffix(self, language, non_local_language, more_than_one_official_language=False):
        if non_local_language is not None:
            osm_suffix = ':{}'.format(non_local_language)
        elif more_than_one_official_language and language not in (AMBIGUOUS_LANGUAGE, UNKNOWN_LANGUAGE):
            osm_suffix = ':{}'.format(language)
        else:
            osm_suffix = ''
        return osm_suffix

    def add_osm_boundaries(self, address_components,
                           country, language,
                           latitude, longitude,
                           osm_suffix='',
                           non_local_language=None,
                           random_key=True,
                           alpha_3_iso_code_prob=0.1,
                           alpha_2_iso_code_prob=0.2,
                           simple_country_key_prob=0.4,
                           replace_with_non_local_prob=0.4,
                           join_state_district_prob=0.5,
                           expand_state_prob=0.7
                           ):
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

        osm_components = osm_reverse_geocoded_components(self.admin_rtree, country, latitude, longitude)

        name_key = ''.join(('name', osm_suffix))
        raw_name_key = 'name'
        simple_name_key = 'name:simple'
        international_name_key = 'int_name'

        iso_code_key = 'ISO3166-1:alpha2'
        iso_code3_key = 'ISO3166-1:alpha3'

        if osm_components:
            poly_components = defaultdict(list)

            existing_city_name = address_components.get(AddressFormatter.CITY)

            for component, components_values in osm_components.iteritems():
                seen = set()

                if random_key:
                    key, raw_key = self.pick_random_name_key(suffix=osm_suffix)
                else:
                    key, raw_key = name_key, raw_name_key

                for component_value in components_values:
                    r = random.random()
                    name = None

                    if component == AddressFormatter.COUNTRY:
                        if iso_code3_key in component_value and r < alpha_3_iso_code_prob:
                            name = component_value[iso_code3_key]
                        elif iso_code_key in component_value and r < alpha_3_iso_code_prob + alpha_2_iso_code_prob:
                            name = component_value[iso_code_key]
                        elif language == 'en' and not non_local_language and r < alpha_3_iso_code_prob + alpha_2_iso_code_prob + simple_country_key_prob:
                            # Particularly to address the US (prefer United States,
                            # not United States of America) but may capture variations
                            # in other English-speaking countries as well.
                            if simple_name_key in component_value:
                                name = component_value[simple_name_key]
                            elif international_name_key in component_value:
                                name = component_value[international_name_key]

                    if not name:
                        name = component_value.get(key, component_value.get(raw_key))

                    if not name or (component != AddressFormatter.CITY and name == existing_city_name):
                        name = component_value.get(name_key, component_value.get(raw_name_key))

                    if not name or (component != AddressFormatter.CITY and name == existing_city_name):
                        continue

                    if (component, name) not in seen:
                        poly_components[component].append(name)
                        seen.add((component, name))

            for component, vals in poly_components.iteritems():
                if component not in address_components or (non_local_language and random.random() < replace_with_non_local_prob):
                    if component == AddressFormatter.STATE_DISTRICT and random.random() < join_state_district_prob:
                        num = random.randrange(1, len(vals) + 1)
                        val = u', '.join(vals[:num])
                    else:
                        val = random.choice(vals)

                    if component == AddressFormatter.STATE and random.random() < expand_state_prob:
                        val = STATE_EXPANSIONS.get(country.upper(), {}).get(val, val)
                    address_components[component] = val

    def quattroshapes_city(self, address_components,
                           latitude, longitude,
                           language, non_local_language=None,
                           qs_add_city_prob=0.2,
                           abbreviated_name_prob=0.1):
        '''
        Quattroshapes/GeoNames cities
        -----------------------------

        Quattroshapes isn't great for everything, but it has decent city boundaries
        in places where OSM sometimes does not (or at least in places where we aren't
        currently able to create valid polygons). While Quattroshapes itself doesn't
        reliably use local names, which we'll want for consistency
        '''

        city = None

        if non_local_language or (AddressFormatter.CITY not in address_components and random.random() < qs_add_city_prob):
            lang = non_local_language or language
            quattroshapes_cities = self.quattroshapes_rtree.point_in_poly(latitude, longitude, return_all=True)
            for result in quattroshapes_cities:
                if result.get(self.quattroshapes_rtree.LEVEL) == self.quattroshapes_rtree.LOCALITY and self.quattroshapes_rtree.GEONAMES_ID in result:
                    geonames_id = int(result[self.quattroshapes_rtree.GEONAMES_ID].split(',')[0])
                    names = self.geonames.get_alternate_names(geonames_id)

                    if not names or lang not in names:
                        continue

                    city = None
                    if 'abbr' not in names or non_local_language:
                        # Use the common city name in the target language
                        city = names[lang][0][0]
                    elif random.random() < abbreviated_name_prob:
                        # Use an abbreviation: NYC, BK, SF, etc.
                        city = random.choice(names['abbr'])[0]

                    if not city or not city.strip():
                        continue
                    return city
                    break
            else:
                if non_local_language and AddressFormatter.CITY in address_components and (
                        AddressFormatter.CITY_DISTRICT in address_components or
                        AddressFormatter.SUBURB in address_components):
                    address_components.pop(AddressFormatter.CITY)

        return city

    def add_neighborhoods(self, address_components,
                          latitude, longitude,
                          osm_suffix='',
                          add_prefix_prob=0.5,
                          add_neighborhood_prob=0.5):
        '''
        Neighborhoods
        -------------

        In some cities, neighborhoods may be included in a free-text address.

        OSM includes many neighborhoods but only as points, rather than the polygons
        needed to perform reverse-geocoding. We use a hybrid index containing
        Quattroshapes/Zetashapes polygons matched fuzzily with OSM names (which are
        on the whole of better quality).
        '''

        neighborhoods = self.neighborhoods_rtree.point_in_poly(latitude, longitude, return_all=True)
        neighborhood_levels = defaultdict(list)

        name_key = ''.join(('name', osm_suffix))
        raw_name_key = 'name'

        for neighborhood in neighborhoods:
            place_type = neighborhood.get('place')
            polygon_type = neighborhood.get('polygon_type')

            key, raw_key = self.pick_random_name_key(suffix=osm_suffix)
            name = neighborhood.get(key, neighborhood.get(raw_key))

            if not name:
                name = neighborhood.get(name_key, neighborhood.get(raw_name_key))

                name_prefix = neighborhood.get('name:prefix')

                if name_prefix and random.random() < add_prefix_prob:
                    name = u' '.join([name_prefix, name])

            if not name:
                continue

            neighborhood_level = AddressFormatter.SUBURB

            if place_type == 'borough' or polygon_type == 'local_admin':
                neighborhood_level = AddressFormatter.CITY_DISTRICT

                # Optimization so we don't use e.g. Brooklyn multiple times
                city_name = address_components.get(AddressFormatter.CITY)
                if name == city_name:
                    name = neighborhood.get(name_key, neighborhood.get(raw_name_key))
                    if not name or name == city_name:
                        continue

            neighborhood_levels[neighborhood_level].append(name)

        for component, neighborhoods in neighborhood_levels.iteritems():
            if component not in address_components and random.random() < add_neighborhood_prob:
                address_components[component] = neighborhoods[0]

    def normalize_names(self, address_components, replacement_prob=0.6):
        '''
        Name normalization
        ------------------

        Probabilistically strip standard prefixes/suffixes e.g. "London Borough of"
        '''
        for component in BOUNDARY_COMPONENTS:
            name = address_components.get(component)
            if not name:
                continue
            replacement = replace_name_prefixes(replace_name_suffixes(name))
            if replacement != name and random.random() < replacement_prob:
                address_components[component] = replacement

    def replace_names(self, address_components):
        '''
        Name replacements
        -----------------

        Make a few special replacements (like UK instead of GB)
        '''
        for component, value in address_components.iteritems():
            replacement, prob = RANDOM_VALUE_REPLACEMENTS.get(component, {}).get(value, (None, 0.0))
            if replacement is not None and random.random() < prob:
                address_components[component] = replacement

    def prune_duplicate_names(self, address_components):
        '''
        Name deduping
        -------------

        For some cases like "Antwerpen, Antwerpen, Antwerpen"
        that are very unlikely to occur in real life.
        '''

        name_components = defaultdict(list)

        for component in (AddressFormatter.CITY, AddressFormatter.STATE_DISTRICT,
                          AddressFormatter.CITY_DISTRICT, AddressFormatter.SUBURB):
            name = address_components.get(component)
            if name:
                name_components[name].append(component)

        for name, components in name_components.iteritems():
            if len(components) > 1:
                for component in components[1:]:
                    address_components.pop(component, None)

    def cleanup_house_number(self, address_components):
        '''
        House number cleanup
        --------------------

        For some OSM nodes, particularly in Uruguay, we get house numbers
        that are actually a comma-separated list.

        If there's one comma in the house number, allow it as it might
        be legitimate, but if there are 2 or more, just take the first one.
        '''

        house_number = address_components.get(AddressFormatter.HOUSE_NUMBER)
        if ';' in house_number:
            house_number = house_number.replace(';', ',')
            address_components[AddressFormatter.HOUSE_NUMBER] = house_number
        if house_number and house_number.count(',') >= 2:
            house_numbers = house_number.split(',')
            random.shuffle(house_numbers)
            for num in house_numbers:
                num = num.strip()
                if num:
                    address_components[AddressFormatter.HOUSE_NUMBER] = num
                    break
            else:
                address_components.pop(AddressFormatter.HOUSE_NUMBER, None)

    def expanded_address_components(self, value):
        try:
            latitude, longitude = latlon_to_decimal(value['lat'], value['lon'])
        except Exception:
            return None, None, None

        country, candidate_languages, language_props = country_and_languages(self.language_rtree, latitude, longitude)
        if not (country and candidate_languages):
            return None, None, None

        for key in OSM_IGNORE_KEYS:
            _ = value.pop(key, None)

        language = None

        more_than_one_official_language = len(candidate_languages) > 1

        language = self.pick_language(value, candidate_languages)

        address_components = self.normalize_address_components(value)

        address_country, non_local_language = self.country_name(address_components, country, language)
        if address_country:
            address_components[AddressFormatter.COUNTRY] = address_country

        address_state = self.state_name(address_components, country, language, non_local_language=non_local_language)
        if address_state:
            address_components[AddressFormatter.STATE] = address_state

        osm_suffix = self.tag_suffix(language, non_local_language, more_than_one_official_language)

        self.add_osm_boundaries(address_components, country, language, latitude, longitude,
                                non_local_language=non_local_language,
                                osm_suffix=osm_suffix)

        city = self.quattroshapes_city(address_components, latitude, longitude, language, non_local_language=non_local_language)
        if city:
            address_components[AddressFormatter.CITY] = city

        self.add_neighborhoods(address_components, latitude, longitude,
                               osm_suffix=osm_suffix)

        street = address_components.get(AddressFormatter.ROAD)
        if street:
            address_components[AddressFormatter.ROAD] = self.abbreviated_street(street, language)

        self.normalize_names(address_components)

        self.replace_names(address_components)

        self.prune_duplicate_names(address_components)

        self.cleanup_house_number(address_components)

        return address_components, country, language

    def limited_address_components(self, value):
        try:
            latitude, longitude = latlon_to_decimal(value['lat'], value['lon'])
        except Exception:
            return None, None, None

        country, candidate_languages, language_props = country_and_languages(self.language_rtree, latitude, longitude)
        if not (country and candidate_languages):
            return None, None, None

        remove_keys = NAME_KEYS + HOUSE_NUMBER_KEYS + POSTAL_KEYS + OSM_IGNORE_KEYS

        for key in remove_keys:
            _ = value.pop(key, None)

        language = None

        more_than_one_official_language = len(candidate_languages) > 1

        language = self.pick_language(value, candidate_languages)

        address_components = self.normalize_address_components(value)

        address_country, non_local_language = self.country_name(address_components, country, language,
                                                                use_country_code_prob=0.0,
                                                                local_language_name_prob=1.0,
                                                                random_language_name_prob=0.0,
                                                                alpha_3_iso_code_prob=0.0)
        if address_country:
            address_components[AddressFormatter.COUNTRY] = address_country

        address_state = self.state_name(address_components, country, language, non_local_language=non_local_language, state_full_name_prob=1.0)
        if address_state:
            address_components[AddressFormatter.STATE] = address_state

        street = address_components.get(AddressFormatter.ROAD)
        if street:
            address_components[AddressFormatter.ROAD] = self.abbreviated_street(street, language)

        osm_suffix = self.tag_suffix(language, non_local_language, more_than_one_official_language)

        self.add_osm_boundaries(address_components, country, language, latitude, longitude,
                                osm_suffix=osm_suffix,
                                non_local_language=non_local_language,
                                random_key=False,
                                alpha_3_iso_code_prob=0.0,
                                alpha_2_iso_code_prob=0.0,
                                replace_with_non_local_prob=0.0,
                                expand_state_prob=1.0)

        city = self.quattroshapes_city(address_components, latitude, longitude, language, non_local_language=non_local_language)

        if city:
            address_components[AddressFormatter.CITY] = city

        self.add_neighborhoods(address_components, latitude, longitude,
                               osm_suffix=osm_suffix)

        self.normalize_names(address_components)

        self.prune_duplicate_names(address_components)

        return address_components, country, language

    def formatted_addresses(self, value, dropout_prob=0.5, rare_component_dropout_prob=0.6, tag_components=True):
        '''
        Formatted addresses
        -------------------

        Produces one or more formatted addresses (tagged/untagged)
        from the given dictionary of OSM tags and values.

        Here we also apply component dropout meaning we produce several
        different addresses with various components removed at random.
        That way the parser will have many examples of queries that are
        just city/state or just house_number/street. The selected
        components still have to make sense i.e. a lone house_number will
        not be used without a street name. The dependencies are listed
        above, see: OSM_ADDRESS_COMPONENTS.

        If there is more than one venue name (say name and alt_name),
        addresses using both names and the selected components are
        returned.
        '''

        venue_names = self.venue_names(value) or []

        address_components, country, language = self.expanded_address_components(value)

        if not address_components:
            return None, None, None

        for venue_name in venue_names:
            abbreviated_venue = self.abbreviated_venue_name(venue_name, language)
            if abbreviated_venue != venue_name and abbreviated_venue not in set(venue_names):
                venue_names.append(abbreviated_venue)

        # Version with all components
        formatted_address = self.formatter.format_address(country, address_components, tag_components=tag_components, minimal_only=not tag_components)

        if tag_components:
            formatted_addresses = []
            formatted_addresses.append(formatted_address)

            seen = set([formatted_address])

            address_components = {k: v for k, v in address_components.iteritems() if k in OSM_ADDRESS_COMPONENT_VALUES}
            if not address_components:
                return []

            current_components = []
            current_components_rare = []

            state_important = country.upper() in self.state_important

            current_components = [k for k in address_components.keys() if k not in self.rare_components]
            current_components_rare = [k for k in address_components.keys() if k in self.rare_components]
            random.shuffle(current_components)
            random.shuffle(current_components_rare)

            current_components = current_components_rare + current_components
            component_set = component_bitset(address_components.keys())

            for component in current_components:
                prob = rare_component_dropout_prob if component in self.rare_components else dropout_prob

                if component not in self.rare_components or (component == AddressFormatter.STATE and state_important):
                    prob = dropout_prob
                else:
                    prob = rare_component_dropout_prob

                if component_set ^ OSM_ADDRESS_COMPONENT_VALUES[component] in OSM_ADDRESS_COMPONENTS_VALID and random.random() < prob:
                    address_components.pop(component)
                    component_set ^= OSM_ADDRESS_COMPONENT_VALUES[component]
                    if not address_components:
                        return []

                    # Since venue names are 1-per-record, we must use them all
                    for venue_name in (venue_names or [None]):
                        if venue_name and AddressFormatter.HOUSE in address_components:
                            address_components[AddressFormatter.HOUSE] = venue_name
                        formatted_address = self.formatter.format_address(country, address_components, tag_components=tag_components, minimal_only=False)
                        if formatted_address and formatted_address not in seen:
                            formatted_addresses.append(formatted_address)
                            seen.add(formatted_address)

            return formatted_addresses, country, language
        else:
            formatted_addresses = []
            seen = set()
            # Since venue names are 1-per-record, we must use them all
            for venue_name in (venue_names or [None]):
                if venue_name:
                    address_components[AddressFormatter.HOUSE] = venue_name
                formatted_address = self.formatter.format_address(country, address_components, tag_components=tag_components, minimal_only=False)
                if formatted_address and formatted_address not in seen:
                    formatted_addresses.append(formatted_address)
                    seen.add(formatted_address)
            return formatted_addresses, country, language

    def formatted_address_limited(self, value, admin_dropout_prob=0.7):
        address_components, country, language = self.limited_address_components(value)

        if not address_components:
            return None, None, None

        formatted_addresses = []

        address_components = {k: v for k, v in address_components.iteritems() if k in OSM_ADDRESS_COMPONENT_VALUES}
        if not address_components:
            return []

        current_components = address_components.keys()
        random.shuffle(current_components)

        for component in (AddressFormatter.COUNTRY, AddressFormatter.STATE,
                          AddressFormatter.STATE_DISTRICT, AddressFormatter.CITY,
                          AddressFormatter.CITY_DISTRICT, AddressFormatter.SUBURB):
            if random.random() < admin_dropout_prob:
                _ = address_components.pop(component, None)

        if not address_components:
            return None, None, None

        # Version with all components
        formatted_address = self.formatter.format_address(country, address_components, tag_components=False, minimal_only=False)

        return formatted_address, country, language

    def build_training_data(self, infile, out_dir, tag_components=True):
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

        if tag_components:
            formatted_tagged_file = open(os.path.join(out_dir, ADDRESS_FORMAT_DATA_TAGGED_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')
        else:
            formatted_file = open(os.path.join(out_dir, ADDRESS_FORMAT_DATA_FILENAME), 'w')
            writer = csv.writer(formatted_file, 'tsv_no_quote')

        for node_id, value, deps in parse_osm(infile):
            formatted_addresses, country, language = self.formatted_addresses(value, tag_components=tag_components)
            if not formatted_addresses:
                continue

            for formatted_address in formatted_addresses:
                if formatted_address and formatted_address.strip():
                    formatted_address = tsv_string(formatted_address)
                    if not formatted_address or not formatted_address.strip():
                        continue

                    if tag_components:
                        row = (language, country, formatted_address)
                    else:
                        row = formatted_address

                    writer.writerow(row)

            i += 1
            if i % 1000 == 0 and i > 0:
                print('did {} formatted addresses'.format(i))

    def build_limited_training_data(self, infile, out_dir):
        '''
        Creates a special kind of formatted address training data from OSM's addr:* tags
        but are designed for use in language classification. These records are similar 
        to the untagged formatted records but include the language and country
        (suitable for concatenation with the rest of the language training data),
        and remove several fields like country which usually do not contain helpful
        information for classifying the language.

        Example:

        nb      no      Olaf Ryes Plass Oslo
        '''
        i = 0

        f = open(os.path.join(out_dir, ADDRESS_FORMAT_DATA_LANGUAGE_FILENAME), 'w')
        writer = csv.writer(f, 'tsv_no_quote')

        for node_id, value, deps in parse_osm(infile):
            formatted_address, country, language = self.formatted_address_limited(value)
            if not formatted_address:
                continue

            if formatted_address.strip():
                formatted_address = tsv_string(formatted_address.strip())
                if not formatted_address or not formatted_address.strip():
                    continue

                row = (language, country, formatted_address)
                writer.writerow(row)

            i += 1
            if i % 1000 == 0 and i > 0:
                print('did {} formatted addresses'.format(i))


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
        if not any((k.startswith('name') for k, v in value.iteritems())):
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
                print('did {} toponyms'.format(i))
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
                print('did {} streets'.format(i))
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
                print('did, {} venues'.format(i))
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

    parser.add_argument('-r', '--rtree-dir',
                        default=None,
                        help='OSM reverse geocoder RTree directory')

    parser.add_argument('-q', '--quattroshapes-rtree-dir',
                        default=None,
                        help='Quattroshapes reverse geocoder RTree directory')

    parser.add_argument('-d', '--geonames-db',
                        default=None,
                        help='GeoNames db file')

    parser.add_argument('-n', '--neighborhoods-rtree-dir',
                        default=None,
                        help='Neighborhoods reverse geocoder RTree directory')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    args = parser.parse_args()

    init_country_names()
    init_languages()
    init_disambiguation()
    init_gazetteers()

    language_rtree = LanguagePolygonIndex.load(args.language_rtree_dir)
    osm_rtree = None
    if args.rtree_dir:
        osm_rtree = OSMReverseGeocoder.load(args.rtree_dir)

    neighborhoods_rtree = None
    if args.neighborhoods_rtree_dir:
        neighborhoods_rtree = NeighborhoodReverseGeocoder.load(args.neighborhoods_rtree_dir)

    quattroshapes_rtree = None
    if args.quattroshapes_rtree_dir:
        quattroshapes_rtree = QuattroshapesReverseGeocoder.load(args.quattroshapes_rtree_dir)

    geonames = None

    if args.geonames_db:
        geonames = GeoNamesDB(args.geonames_db)

    # Can parallelize
    if args.streets_file:
        build_ways_training_data(language_rtree, args.streets_file, args.out_dir)
    if args.borders_file:
        build_toponym_training_data(language_rtree, args.borders_file, args.out_dir)

    if args.address_file:
        if osm_rtree is None:
            parser.error('--rtree-dir required for formatted addresses')
        elif neighborhoods_rtree is None:
            parser.error('--neighborhoods-rtree-dir required for formatted addresses')
        elif quattroshapes_rtree is None:
            parser.error('--quattroshapes-rtree-dir required for formatted addresses')
        elif geonames is None:
            parser.error('--geonames-db required for formatted addresses')

    if args.address_file and args.format_only:
        osm_formatter = OSMAddressFormatter(osm_rtree, language_rtree, neighborhoods_rtree, quattroshapes_rtree, geonames)
        osm_formatter.build_training_data(args.address_file, args.out_dir, tag_components=not args.untagged)
    if args.address_file and args.limited_addresses:
        osm_formatter = OSMAddressFormatter(osm_rtree, language_rtree, neighborhoods_rtree, quattroshapes_rtree, geonames, splitter=u' ')
        osm_formatter.build_limited_training_data(args.address_file, args.out_dir)
    if args.venues_file:
        build_venue_training_data(language_rtree, args.venues_file, args.out_dir)
