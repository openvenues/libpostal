# -*- coding: utf-8 -*-
import itertools
import os
import random
import re
import six
import sys
import yaml

from collections import OrderedDict
from six import itertools

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.address_expansions.gazetteers import *
from geodata.address_expansions.abbreviations import abbreviate
from geodata.address_formatting.aliases import Aliases
from geodata.address_formatting.formatter import AddressFormatter
from geodata.addresses.blocks import Block
from geodata.addresses.config import address_config
from geodata.addresses.components import AddressComponents
from geodata.addresses.house_numbers import HouseNumber
from geodata.categories.config import category_config
from geodata.categories.query import Category, NULL_CATEGORY_QUERY
from geodata.chains.query import Chain, NULL_CHAIN_QUERY
from geodata.coordinates.conversion import *
from geodata.configs.utils import nested_get
from geodata.countries.country_names import *
from geodata.language_id.disambiguation import *
from geodata.language_id.sample import INTERNET_LANGUAGE_DISTRIBUTION
from geodata.i18n.google import postcode_regexes
from geodata.i18n.languages import *
from geodata.intersections.query import Intersection, IntersectionQuery
from geodata.address_formatting.formatter import AddressFormatter
from geodata.osm.components import osm_address_components
from geodata.osm.extract import *
from geodata.osm.intersections import OSMIntersectionReader
from geodata.places.config import place_config
from geodata.polygons.language_polys import *
from geodata.polygons.reverse_geocode import *
from geodata.i18n.unicode_paths import DATA_DIR
from geodata.text.tokenize import tokenize, token_types
from geodata.text.utils import is_numeric

from geodata.csv_utils import *
from geodata.file_utils import *


OSM_PARSER_DATA_DEFAULT_CONFIG = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                              'resources', 'parser', 'data_sets', 'osm.yaml')

FORMATTED_ADDRESS_DATA_TAGGED_FILENAME = 'formatted_addresses_tagged.tsv'
FORMATTED_ADDRESS_DATA_FILENAME = 'formatted_addresses.tsv'
FORMATTED_ADDRESS_DATA_LANGUAGE_FILENAME = 'formatted_addresses_by_language.tsv'
FORMATTED_PLACE_DATA_TAGGED_FILENAME = 'formatted_places_tagged.tsv'
FORMATTED_PLACE_DATA_FILENAME = 'formatted_places.tsv'
INTERSECTIONS_FILENAME = 'intersections.tsv'
INTERSECTIONS_TAGGED_FILENAME = 'intersections_tagged.tsv'

ALL_LANGUAGES = 'all'

JAPAN = 'jp'
JAPANESE = 'ja'
JAPANESE_ROMAJI = 'ja_rm'

ENGLISH = 'en'

numbered_tag_regex = re.compile('_[\d]+$')


class OSMAddressFormatter(object):
    aliases = Aliases(
        OrderedDict([
            ('name', AddressFormatter.HOUSE),
            ('addr:housename', AddressFormatter.HOUSE),
            ('addr:housenumber', AddressFormatter.HOUSE_NUMBER),
            ('addr:house_number', AddressFormatter.HOUSE_NUMBER),
            ('addr:street', AddressFormatter.ROAD),
            ('addr:place', AddressFormatter.ROAD),
            ('addr:suburb', AddressFormatter.SUBURB),
            ('is_in:suburb', AddressFormatter.SUBURB),
            ('addr:neighbourhood', AddressFormatter.SUBURB),
            ('is_in:neighbourhood', AddressFormatter.SUBURB),
            ('addr:neighborhood', AddressFormatter.SUBURB),
            ('is_in:neighborhood', AddressFormatter.SUBURB),
            ('addr:barangay', AddressFormatter.SUBURB),
            # Used in the UK for civil parishes, sometimes others
            ('addr:locality', AddressFormatter.SUBURB),
            # This is actually used for suburb
            ('suburb', AddressFormatter.SUBURB),
            ('addr:city', AddressFormatter.CITY),
            ('is_in:city', AddressFormatter.CITY),
            ('addr:locality', AddressFormatter.CITY),
            ('is_in:locality', AddressFormatter.CITY),
            ('addr:municipality', AddressFormatter.CITY),
            ('is_in:municipality', AddressFormatter.CITY),
            ('addr:hamlet', AddressFormatter.CITY),
            ('is_in:hamlet', AddressFormatter.CITY),
            ('addr:quarter', AddressFormatter.CITY_DISTRICT),
            ('addr:county', AddressFormatter.STATE_DISTRICT),
            ('addr:district', AddressFormatter.STATE_DISTRICT),
            ('is_in:district', AddressFormatter.STATE_DISTRICT),
            ('addr:state', AddressFormatter.STATE),
            ('is_in:state', AddressFormatter.STATE),
            ('addr:province', AddressFormatter.STATE),
            ('is_in:province', AddressFormatter.STATE),
            ('addr:region', AddressFormatter.STATE),
            ('is_in:region', AddressFormatter.STATE),
            # Used in Tunisia
            ('addr:governorate', AddressFormatter.STATE),
            ('addr:postal_code', AddressFormatter.POSTCODE),
            ('addr:postcode', AddressFormatter.POSTCODE),
            ('addr:zipcode', AddressFormatter.POSTCODE),
            ('postal_code', AddressFormatter.POSTCODE),
            ('addr:country', AddressFormatter.COUNTRY),
            ('addr:country_code', AddressFormatter.COUNTRY),
            ('country_code', AddressFormatter.COUNTRY),
            ('is_in:country_code', AddressFormatter.COUNTRY),
            ('is_in:country', AddressFormatter.COUNTRY),
        ])
    )

    sub_building_aliases = Aliases(
        OrderedDict([
            ('level', AddressFormatter.LEVEL),
            ('addr:floor', AddressFormatter.LEVEL),
            ('addr:unit', AddressFormatter.UNIT),
            ('addr:flats', AddressFormatter.UNIT),
            ('addr:door', AddressFormatter.UNIT),
            ('addr:suite', AddressFormatter.UNIT),
        ])
    )

    zones = {
        'landuse': {
            'retail': AddressComponents.zones.COMMERCIAL,
            'commercial': AddressComponents.zones.COMMERCIAL,
            'industrial': AddressComponents.zones.INDUSTRIAL,
            'residential': AddressComponents.zones.RESIDENTIAL,
        },
        'amenity': {
            'university': AddressComponents.zones.UNIVERSITY,
            'college': AddressComponents.zones.UNIVERSITY,
        }
    }

    boundary_component_priorities = {k: i for i, k in enumerate(AddressFormatter.BOUNDARY_COMPONENTS_ORDERED)}

    def __init__(self, components, subdivisions_rtree=None, buildings_rtree=None, metro_stations_index=None):
        # Instance of AddressComponents, contains structures for reverse geocoding, etc.
        self.components = components
        self.language_rtree = components.language_rtree

        self.subdivisions_rtree = subdivisions_rtree
        self.buildings_rtree = buildings_rtree

        self.metro_stations_index = metro_stations_index

        self.config = yaml.load(open(OSM_PARSER_DATA_DEFAULT_CONFIG))
        self.formatter = AddressFormatter()

    def namespaced_language(self, tags, candidate_languages):
        language = None

        pick_namespaced_language_prob = float(nested_get(self.config, ('languages', 'pick_namespaced_language_probability')))

        if len(candidate_languages) > 1:
            street = tags.get('addr:street', None)

            namespaced = [l['lang'] for l in candidate_languages if 'addr:street:{}'.format(l['lang']) in tags]

            if namespaced and random.random() < pick_namespaced_language_prob:
                language = random.choice(namespaced)
                lang_suffix = ':{}'.format(language)
                for k in tags:

                    if k.endswith(lang_suffix):
                        tags[k.rstrip(lang_suffix)] = tags[k]

        return language

    def normalize_address_components(self, tags):
        address_components = {k: v for k, v in six.iteritems(tags) if self.aliases.get(k)}
        self.aliases.replace(address_components)
        address_components = {k: v for k, v in six.iteritems(address_components) if k in AddressFormatter.address_formatter_fields}
        return address_components

    def normalize_sub_building_components(self, tags):
        sub_building_components = {k: v for k, v in six.iteritems(tags) if self.sub_building_aliases.get(k) and is_numeric(v)}
        self.aliases.replace(sub_building_components)
        sub_building_components = {k: v for k, v in six.iteritems(sub_building_components) if k in AddressFormatter.address_formatter_fields}
        return sub_building_components

    def valid_venue_name(self, name):
        if not name:
            return False
        tokens = tokenize(name)
        return any((c in token_types.WORD_TOKEN_TYPES for t, c in tokens))

    def subdivision_components(self, latitude, longitude):
        return self.subdivisions_rtree.point_in_poly(latitude, longitude, return_all=True)

    def zone(self, subdivisions):
        for subdiv in subdivisions:
            for k, v in six.iteritems(self.zones):
                zone = v.get(subdiv.get(k))
                if zone:
                    return zone
        return None

    def building_components(self, latitude, longitude):
        return self.buildings_rtree.point_in_poly(latitude, longitude, return_all=True)

    def num_floors(self, buildings, key='building:levels'):
        max_floors = None
        for b in buildings:
            num_floors = b.get(key)
            if num_floors is not None:
                try:
                    num_floors = int(num_floors)
                except (ValueError, TypeError):
                    try:
                        num_floors = int(float(num_floors))
                    except (ValueError, TypeError):
                        continue

                if max_floors is None or num_floors > max_floors:
                    max_floors = num_floors
        return max_floors

    def replace_numbered_tag(self, tag):
        '''
        If a tag is numbered like name_1, name_2, etc. replace it with name
        '''
        match = numbered_tag_regex.search(tag)
        if not match:
            return None
        return tag[:match.start()]

    def abbreviated_street(self, street, language):
        '''
        Street abbreviations
        --------------------

        Use street and unit type dictionaries to probabilistically abbreviate
        phrases. Because the abbreviation is picked at random, this should
        help bridge the gap between OSM addresses and user input, in addition
        to capturing some non-standard abbreviations/surface forms which may be
        missing or sparse in OSM.
        '''
        abbreviate_prob = float(nested_get(self.config, ('streets', 'abbreviate_probability'), default=0.0))
        separate_prob = float(nested_get(self.config, ('streets', 'separate_probability'), default=0.0))

        return abbreviate(street_and_synonyms_gazetteer, street, language,
                          abbreviate_prob=abbreviate_prob, separate_prob=separate_prob)

    def abbreviated_venue_name(self, name, language):
        '''
        Venue abbreviations
        -------------------

        Use street and unit type dictionaries to probabilistically abbreviate
        phrases. Because the abbreviation is picked at random, this should
        help bridge the gap between OSM addresses and user input, in addition
        to capturing some non-standard abbreviations/surface forms which may be
        missing or sparse in OSM.
        '''
        abbreviate_prob = float(nested_get(self.config, ('venues', 'abbreviate_probability'), default=0.0))
        separate_prob = float(nested_get(self.config, ('venues', 'separate_probability'), default=0.0))

        return abbreviate(names_gazetteer, name, language,
                          abbreviate_prob=abbreviate_prob, separate_prob=separate_prob)

    def combine_street_name(self, props):
        '''
        Combine street names
        --------------------

        In the UK sometimes streets have "parent" streets and
        both will be listed in the address.

        Example: http://www.openstreetmap.org/node/2933503941
        '''
        # In the UK there's sometimes the notion of parent and dependent streets
        if 'addr:parentstreet' not in props or 'addr:street' not in props:
            return False

        street = safe_decode(props['addr:street'])
        parent_street = props.pop('addr:parentstreet', None)
        if parent_street:
            props['addr:street'] = six.u(', ').join([street, safe_decode(parent_street)])
            return True
        return False

    def combine_japanese_house_number(self, address_components, language):
        '''
        Japanese house numbers
        ----------------------

        Addresses in Japan are pretty unique.
        There are no street names in most of the country, and so buildings
        are addressed by the following:

        1. the neighborhood (丁目 or chōme), usually numberic e.g. 4-chōme
        2. the block number (OSM uses addr:block_number for this)
        3. the house number

        Sometimes only the block number and house number are abbreviated.

        For libpostal, we want to parse:
        2丁目3-5 as {'suburb': '2丁目', 'house_number': '3-5'}

        and the abbreviated "2-3-5" as simply house_number and leave
        it up to the end user to split up that number or not.

        At this stage we're still working with the original OSM tags,
        so only combine addr_block_number with addr:housenumber

        See: https://en.wikipedia.org/wiki/Japanese_addressing_system
        '''
        house_number = address_components.get('addr:housenumber')
        if not house_number or not house_number.isdigit():
            return False

        block = address_components.get('addr:block_number')
        if not block or not block.isdigit():
            return False

        separator = six.u('-')

        combine_probability = float(nested_get(self.config, ('countries', 'jp', 'combine_block_house_number_probability'), default=0.0))
        if random.random() < combine_probability:
            if random.random() < float(nested_get(self.config, ('countries', 'jp', 'block_phrase_probability'), default=0.0)):
                block = Block.phrase(block, language)
                house_number = HouseNumber.phrase(house_number, language)
                if block is None or house_number is None:
                    return
                separator = six.u(' ') if language == JAPANESE_ROMAJI else six.u('')

            house_number = separator.join([block, house_number])
            address_components['addr:housenumber'] = house_number

            return True
        return False


    def add_metro_station(self, address_components, latitude, longitude, language=None, default_language=None):
        '''
        Metro stations
        --------------

        Particularly in Japan, where there are rarely named streets, metro stations are
        often used to help locate an address (landmarks may be used as well). Unlike in the
        rest of the world, metro stations in Japan are a semi-official component and used
        almost as frequently as street names or house number in other countries, so we would
        want libpostal's address parser to recognize Japanese train stations in both Kanji and Romaji.

        It's possible at some point to extend this to generate the sorts of natural language
        directions we sometimes see in NYC and other large cities where a subway stop might be
        included parenthetically after the address e.g. 61 Wythe Ave (L train to Bedford).
        The subway stations in OSM are in a variety of formats, so this would need some massaging
        and a slightly more sophisticated phrase generator than what we employ for numeric components
        like apartment numbers.
        '''
        if self.metro_stations_index is None:
            return False
        nearest_metro = self.metro_stations_index.nearest_point(latitude, longitude)
        if nearest_metro:
            props, lat, lon, distance = nearest_metro
            name = None
            if language is not None:
                name = props.get('name:{}'.format(language.lower()))
                if language == default_language:
                    name = props.get('name')
            else:
                name = props.get('name')

            if name:
                address_components[AddressFormatter.METRO_STATION] = name
                return True

        return False

    def venue_names(self, props, languages):
        '''
        Venue names
        -----------

        Some venues have multiple names listed in OSM, grab them all
        With a certain probability, add None to the list so we drop the name
        '''

        return self.components.all_names(props, languages, keys=('name', 'alt_name', 'loc_name', 'int_name', 'old_name'))

    def formatted_addresses_with_venue_names(self, address_components, venue_names, country, language=None,
                                             tag_components=True, minimal_only=False):
        # Since venue names are only one-per-record, this wrapper will try them all (name, alt_name, etc.)
        formatted_addresses = []

        if AddressFormatter.HOUSE not in address_components or not venue_names:
            return [self.formatter.format_address(address_components, country, language=language,
                                                  tag_components=tag_components, minimal_only=minimal_only)]

        address_prob = float(nested_get(self.config, ('venues', 'address_probability'), default=0.0))
        if random.random() < address_prob:
            address_components.pop(AddressFormatter.HOUSE)
            formatted_address = self.formatter.format_address(address_components, country, language=language,
                                                              tag_components=tag_components, minimal_only=minimal_only)
            formatted_addresses.append(formatted_address)

        for venue_name in venue_names:
            if venue_name:
                address_components[AddressFormatter.HOUSE] = venue_name
            formatted_address = self.formatter.format_address(address_components, country, language=language,
                                                              tag_components=tag_components, minimal_only=minimal_only)
            formatted_addresses.append(formatted_address)
        return formatted_addresses

    def formatted_places(self, address_components, country, language, tag_components=True):
        formatted_addresses = []

        place_components = self.components.drop_address(address_components)
        formatted_address = self.formatter.format_address(place_components, country, language=language,
                                                          tag_components=tag_components, minimal_only=False)
        formatted_addresses.append(formatted_address)

        if AddressFormatter.POSTCODE in address_components:
            drop_postcode_prob = float(nested_get(self.config, ('places', 'drop_postcode_probability'), default=0.0))
            if random.random() < drop_postcode_prob:
                place_components = self.components.drop_postcode(place_components)
                formatted_address = self.formatter.format_address(place_components, country, language=language,
                                                                  tag_components=tag_components, minimal_only=False)
                formatted_addresses.append(formatted_address)
        return formatted_addresses

    def extract_valid_postal_codes(self, country, postal_code, validate=True):
        '''
        "Valid" postal codes
        --------------------

        This is only really for place formatting at the moment so we don't
        accidentally inject too many bad postcodes into the training data.
        Sometimes for a given boundary name the postcode might be a delimited
        sequence of postcodes like "10013;10014;10015", it might be a range
        like "10013-10015", or it might be some non-postcode text. The idea
        here is to validate the text using Google's libaddressinput postcode
        regexes for the given country. These regexes are a little simplistic
        and don't account for variations like the common use of shortened
        postcodes in cities like Berlin or Oslo where the first digit is always
        known to be the same given the city. Still, at the level of places, that's
        probably fine.
        '''
        postal_codes = []
        if postal_code:
            valid_postcode = False
            if validate:
                postcode_regex = postcode_regexes.get(country)
                if postcode_regex:
                    match = postcode_regex.match(postal_code)
                    if match and match.end() == len(postal_code):
                        valid_postcode = True
                        postal_codes.append(postal_code)
            else:
                valid_postcode = True

            if not valid_postcode:
                postal_codes = parse_osm_number_range(postal_code, parse_letter_range=False, max_range=1000)

        return postal_codes

    def node_place_tags(self, tags):
        try:
            latitude, longitude = latlon_to_decimal(tags['lat'], tags['lon'])
        except Exception:
            return (), None

        osm_components = self.components.osm_reverse_geocoded_components(latitude, longitude)
        country, candidate_languages, language_props = self.language_rtree.country_and_languages(latitude, longitude)
        if country and candidate_languages:
            local_languages = [(l['lang'], bool(int(l['default']))) for l in candidate_languages]
        else:
            for c in reversed(osm_components):
                country = c.get('ISO3166-1:alpha2')
                if country:
                    country = country.lower()
                    break
            else:
                return (), None

            local_languages = [(lang, bool(int(default))) for lang, default in get_country_languages(country).iteritems()]

        all_local_languages = set([l for l, d in local_languages])
        random_languages = set(INTERNET_LANGUAGE_DISTRIBUTION)

        language_defaults = OrderedDict(local_languages)

        for tag in tags:
            if ':' in tag:
                tag, lang = tag.rsplit(':', 1)
                if lang.lower() not in all_local_languages and lang.lower().split('_', 1)[0] in all_local_languages:
                    local_languages.append((lang, language_defaults[lang.lower().split('_', 1)[0]]))
                    all_local_languages.add(lang)

        more_than_one_official_language = len([lang for lang, default in local_languages if default]) > 1

        containing_ids = [(b['type'], b['id']) for b in osm_components]

        component_name = osm_address_components.component_from_properties(country, tags, containing=containing_ids)
        if component_name is None:
            return (), None
        component_index = self.boundary_component_priorities.get(component_name)

        if component_index:
            revised_osm_components = []

            first_valid = False
            for i, c in enumerate(osm_components):
                c_name = osm_address_components.component_from_properties(country, c, containing=containing_ids[i + 1:])
                c_index = self.boundary_component_priorities.get(c_name, -1)
                if c_index >= component_index  and (c['type'], c['id']) != (tags.get('type', 'node'), tags.get('id')):
                    revised_osm_components.append(c)

                    if not first_valid:
                        if (component_index <= self.boundary_component_priorities[AddressFormatter.CITY] and
                           component_index != c_index and tags.get('type') == 'node' and 'admin_center' in c and
                           tags.get('id') and c['admin_center']['id'] == tags['id'] and c.get('name', '').lower() == tags['name'].lower()):
                            component_name = c_name
                            component_index = c_index
                            revised_osm_components.pop()
                        first_valid = True

            osm_components = revised_osm_components

        # Do addr:postcode, postcode, postal_code, etc.
        revised_tags = self.normalize_address_components(tags)

        place_tags = []

        postal_code = revised_tags.get(AddressFormatter.POSTCODE, None)

        postal_codes = []
        if postal_code:
            postal_codes = self.extract_valid_postal_codes(country, postal_code)

        try:
            population = int(tags.get('population', 0))
        except (ValueError, TypeError):
            population = 0

        # Calculate how many records to produce for this place given its population
        population_divisor = 10000  # Add one record for every 10k in population
        min_references = 5  # Every place gets at least 5 reference to account for variations
        max_references = 1000  # Cap the number of references e.g. for India and China country nodes
        num_references = min(population / population_divisor + min_references, max_references)

        cldr_country_prob = float(nested_get(self.config, ('places', 'cldr_country_probability'), default=0.0))

        for name_tag in ('name', 'alt_name', 'loc_name', 'short_name', 'int_name'):
            if more_than_one_official_language:
                name = tags.get(name_tag)
                language_suffix = ''

                if name and name.strip():
                    if six.u(';') in name:
                        name = random.choice(name.split(six.u(';')))
                    elif six.u(',') in name:
                        name = name.split(six.u(','), 1)[0]

                    for i in xrange(num_references if name_tag == 'name' else 1):
                        address_components = {component_name: name.strip()}
                        self.components.add_admin_boundaries(address_components, osm_components, country, UNKNOWN_LANGUAGE,
                                                             random_key=num_references > 1,
                                                             language_suffix=language_suffix,
                                                             drop_duplicate_city_names=False)

                        place_tags.append((address_components, None, True))

            for language, is_default in local_languages:
                if is_default and not more_than_one_official_language:
                    language_suffix = ''
                    name = tags.get(name_tag)
                else:
                    language_suffix = ':{}'.format(language)
                    name = tags.get('{}{}'.format(name_tag, language_suffix))

                if not name or not name.strip():
                    continue

                if six.u(';') in name:
                    name = random.choice(name.split(six.u(';')))
                elif six.u(',') in name:
                    name = name.split(six.u(','), 1)[0]

                n = min_references / 2
                if name_tag == 'name':
                    if is_default:
                        n = num_references
                    else:
                        n = num_references / 2

                for i in xrange(n):
                    address_components = {component_name: name.strip()}
                    self.components.add_admin_boundaries(address_components, osm_components, country, language,
                                                         random_key=is_default,
                                                         language_suffix=language_suffix,
                                                         drop_duplicate_city_names=False)

                    place_tags.append((address_components, language, is_default))

            for language in random_languages - all_local_languages:
                language_suffix = ':{}'.format(language)

                name = tags.get('{}{}'.format(name_tag, language_suffix))
                if (not name or not name.strip()) and language == ENGLISH:
                    name = tags.get(name_tag)

                if not name or not name.strip():
                    continue

                if six.u(';') in name:
                    name = random.choice(name.split(six.u(';')))
                elif six.u(',') in name:
                    name = name.split(six.u(','), 1)[0]

                # Add half as many English records as the local language, every other language gets min_referenes / 2
                for i in xrange(num_references / 2 if language == ENGLISH else min_references / 2):
                    address_components = {component_name: name.strip()}
                    self.components.add_admin_boundaries(address_components, osm_components, country, language,
                                                         random_key=False,
                                                         non_local_language=language,
                                                         language_suffix=language_suffix,
                                                         drop_duplicate_city_names=False)

                    place_tags.append((address_components, language, False))

            if postal_codes:
                extra_place_tags = []
                num_existing_place_tags = len(place_tags)
                for postal_code in postal_codes:
                    for i in xrange(min(min_references, num_existing_place_tags)):
                        if num_references == min_references:
                            # For small places, make sure we get every variation
                            address_components, language, is_default = place_tags[i]
                        else:
                            address_components, language, is_default = random.choice(place_tags)
                        address_components = address_components.copy()
                        address_components[AddressFormatter.POSTCODE] = random.choice(postal_codes)
                        extra_place_tags.append((address_components, language, is_default))

                place_tags = place_tags + extra_place_tags

        revised_place_tags = []
        for address_components, language, is_default in place_tags:
            revised_address_components = place_config.dropout_components(address_components, osm_components, country=country, population=population)
            revised_address_components[component_name] = address_components[component_name]
            self.components.drop_invalid_components(revised_address_components, country)

            self.components.replace_name_affixes(revised_address_components, language)
            self.components.replace_names(revised_address_components)

            self.components.remove_numeric_boundary_names(revised_address_components)
            self.components.cleanup_boundary_names(revised_address_components)

            if (AddressFormatter.COUNTRY in address_components or place_config.include_component(AddressFormatter.COUNTRY, containing_ids, country=country)) and random.random() < cldr_country_prob:
                address_country = self.components.cldr_country_name(country, language)
                if address_country:
                    address_components[AddressFormatter.COUNTRY] = address_country

            if revised_address_components:
                revised_place_tags.append((revised_address_components, language, is_default))

        return revised_place_tags, country

    def category_queries(self, tags, address_components, language, country=None, tag_components=True):
        formatted_addresses = []
        possible_category_keys = category_config.has_keys(language, tags)

        plural_prob = float(nested_get(self.config, ('categories', 'plural_probability'), default=0.0))
        place_only_prob = float(nested_get(self.config, ('categories', 'place_only_probability'), default=0.0))

        for key in possible_category_keys:
            value = tags[key]
            phrase = Category.phrase(language, key, value, country=country, is_plural=random.random() < plural_prob)
            if phrase is not NULL_CATEGORY_QUERY:
                if phrase.add_place_name or phrase.add_address:
                    address_components = self.components.drop_names(address_components)

                if phrase.add_place_name and random.random() < place_only_prob:
                    address_components = self.components.drop_address(address_components)

                formatted_address = self.formatter.format_category_query(phrase, address_components, country, language, tag_components=tag_components)
                if formatted_address:
                    formatted_addresses.append(formatted_address)
        return formatted_addresses

    def chain_queries(self, venue_name, address_components, language, country=None, tag_components=True):
        '''
        Chain queries
        -------------

        Generates strings like "Duane Reades in Brooklyn NY"
        '''
        is_chain, phrases = Chain.extract(venue_name)
        formatted_addresses = []

        if is_chain:
            sample_probability = float(nested_get(self.config, ('chains', 'sample_probability'), default=0.0))
            place_only_prob = float(nested_get(self.config, ('chains', 'place_only_probability'), default=0.0))

            for t, c, l, vals in phrases:
                for d in vals:
                    lang, dictionary, is_canonical, canonical = safe_decode(d).split(six.u('|'))
                    name = canonical
                    if random.random() < sample_probability:
                        names = address_config.sample_phrases.get((language, dictionary), {}).get(canonical, [])
                        if not names:
                            names = address_config.sample_phrases.get((ALL_LANGUAGES, dictionary), {}).get(canonical, [])
                        if names:
                            name = random.choice(names)
                    phrase = Chain.phrase(name, language, country)
                    if phrase is not NULL_CHAIN_QUERY:
                        if phrase.add_place_name or phrase.add_address:
                            address_components = self.components.drop_names(address_components)

                        if phrase.add_place_name and random.random() < place_only_prob:
                            address_components = self.components.drop_address(address_components)

                        formatted_address = self.formatter.format_chain_query(phrase, address_components, country, language, tag_components=tag_components)
                        if formatted_address:
                            formatted_addresses.append(formatted_address)
        return formatted_addresses

    def formatted_addresses(self, tags, tag_components=True):
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

        try:
            latitude, longitude = latlon_to_decimal(tags['lat'], tags['lon'])
        except Exception:
            return None, None, None

        country, candidate_languages, language_props = self.language_rtree.country_and_languages(latitude, longitude)
        if not (country and candidate_languages):
            return None, None, None

        combined_street = self.combine_street_name(tags)

        namespaced_language = self.namespaced_language(tags, candidate_languages)
        language = None
        japanese_variant = None

        if country == JAPAN:
            japanese_variant = JAPANESE
            if random.random() < float(nested_get(self.config, ('countries', 'jp', 'romaji_probability'), default=0.0)):
                japanese_variant = JAPANESE_ROMAJI
            if self.combine_japanese_house_number(tags, japanese_variant):
                language = japanese_variant
            else:
                language = None

        revised_tags = self.normalize_address_components(tags)
        sub_building_tags = self.normalize_sub_building_components(tags)
        revised_tags.update(sub_building_tags)

        # Only including nearest metro station in Japan
        if country == JAPAN:
            if random.random() < float(nested_get(self.config, ('countries', 'jp', 'add_metro_probability'), default=0.0)):
                if self.add_metro_station(revised_tags, latitude, longitude, japanese_variant, default_language=JAPANESE):
                    language = japanese_variant

        num_floors = None
        num_basements = None
        zone = None

        building_venue_names = []

        building_components = self.building_components(latitude, longitude)
        if building_components:
            num_floors = self.num_floors(building_components)
            num_basements = self.num_floors(building_components, key='building:levels:underground')

            for building_tags in building_components:
                building_tags = self.normalize_address_components(building_tags)

                for k, v in six.iteritems(building_tags):
                    if k not in revised_tags and k in (AddressFormatter.HOUSE_NUMBER, AddressFormatter.ROAD, AddressFormatter.POSTCODE):
                        revised_tags[k] = v
                    elif k == AddressFormatter.HOUSE:
                        building_venue_names.append(v)

        subdivision_components = self.subdivision_components(latitude, longitude)
        if subdivision_components:
            zone = self.zone(subdivision_components)

        venue_sub_building_prob = float(nested_get(self.config, ('venues', 'sub_building_probability'), default=0.0))
        add_sub_building_components = AddressFormatter.HOUSE_NUMBER in revised_tags and (AddressFormatter.HOUSE not in revised_tags or random.random() < venue_sub_building_prob)

        address_components, country, language = self.components.expanded(revised_tags, latitude, longitude, language=language or namespaced_language,
                                                                         num_floors=num_floors, num_basements=num_basements,
                                                                         zone=zone, add_sub_building_components=add_sub_building_components)

        if not address_components:
            return None, None, None

        languages = country_languages[country].keys()
        venue_names = self.venue_names(tags, languages) or []

        # Abbreviate the street name with random probability
        street_name = address_components.get(AddressFormatter.ROAD)

        if street_name:
            address_components[AddressFormatter.ROAD] = self.abbreviated_street(street_name, language)

        venue_names.extend(building_venue_names)

        venue_names = [venue_name for venue_name in venue_names if self.valid_venue_name(venue_name)]
        all_venue_names = set(venue_names)

        # Ditto for venue names
        for venue_name in all_venue_names:
            if not self.valid_venue_name(venue_name):
                continue
            abbreviated_venue = self.abbreviated_venue_name(venue_name, language)
            if abbreviated_venue != venue_name and abbreviated_venue not in all_venue_names:
                venue_names.append(abbreviated_venue)

        formatted_addresses = self.formatted_addresses_with_venue_names(address_components, venue_names, country, language=language,
                                                                        tag_components=tag_components, minimal_only=not tag_components)

        formatted_addresses.extend(self.formatted_places(address_components, country, language))

        # Generate a PO Box address at random (only returns non-None values occasionally) and add it to the list
        po_box_components = self.components.po_box_address(address_components, language, country=country)
        if po_box_components:
            formatted_addresses.extend(self.formatted_addresses_with_venue_names(po_box_components, venue_names, country, language=language,
                                                                                 tag_components=tag_components, minimal_only=False))

        formatted_addresses.extend(self.category_queries(tags, address_components, language, country, tag_components=tag_components))

        venue_name = tags.get('name')
        if venue_name:
            formatted_addresses.extend(self.chain_queries(venue_name, address_components, language, country, tag_components=tag_components))

        if tag_components:

            if not address_components:
                return []

            # Pick a random dropout order
            dropout_order = self.components.address_level_dropout_order(address_components, country)

            for component in dropout_order:
                address_components.pop(component, None)
                formatted_addresses.extend(self.formatted_addresses_with_venue_names(address_components, venue_names, country, language=language,
                                                                                     tag_components=tag_components, minimal_only=False))

        return OrderedDict.fromkeys(formatted_addresses).keys(), country, language

    def formatted_address_limited(self, tags):
        try:
            latitude, longitude = latlon_to_decimal(tags['lat'], tags['lon'])
        except Exception:
            return None, None, None

        country, candidate_languages, language_props = self.language_rtree.country_and_languages(latitude, longitude)
        if not (country and candidate_languages):
            return None, None, None

        namespaced_language = self.namespaced_language(tags, candidate_languages)

        revised_tags = self.normalize_address_components(tags)

        admin_dropout_prob = float(nested_get(self.config, ('limited', 'admin_dropout_prob'), default=0.0))

        address_components, country, language = self.components.limited(revised_tags, latitude, longitude, language=namespaced_language)

        if not address_components:
            return None, None, None

        address_components = {k: v for k, v in address_components.iteritems() if k in OSM_ADDRESS_COMPONENT_VALUES}
        if not address_components:
            return []

        for component in (AddressFormatter.COUNTRY, AddressFormatter.STATE,
                          AddressFormatter.STATE_DISTRICT, AddressFormatter.CITY,
                          AddressFormatter.CITY_DISTRICT, AddressFormatter.SUBURB):
            if random.random() < admin_dropout_prob:
                _ = address_components.pop(component, None)

        if not address_components:
            return None, None, None

        # Version with all components
        formatted_address = self.formatter.format_address(address_components, country, language, tag_components=False, minimal_only=False)

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
            formatted_tagged_file = open(os.path.join(out_dir, FORMATTED_ADDRESS_DATA_TAGGED_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')
        else:
            formatted_file = open(os.path.join(out_dir, FORMATTED_ADDRESS_DATA_FILENAME), 'w')
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
                        row = (formatted_address,)

                    writer.writerow(row)

            i += 1
            if i % 1000 == 0 and i > 0:
                print('did {} formatted addresses'.format(i))

    def build_place_training_data(self, infile, out_dir, tag_components=True):
        i = 0

        if tag_components:
            formatted_tagged_file = open(os.path.join(out_dir, FORMATTED_PLACE_DATA_TAGGED_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')
        else:
            formatted_tagged_file = open(os.path.join(out_dir, FORMATTED_PLACE_DATA_FILENAME), 'w')
            writer = csv.writer(formatted_file, 'tsv_no_quote')

        for node_id, tags, deps in parse_osm(infile):
            tags['type'], tags['id'] = node_id.split(':')
            place_tags, country = self.node_place_tags(tags)
            for address_components, language, is_default in place_tags:
                addresses = self.formatted_places(address_components, country, language)
                if language is None:
                    language = UNKNOWN_LANGUAGE

                for address in addresses:
                    if not address or not address.strip():
                        continue

                    address = tsv_string(address)
                    if tag_components:
                        row = (language, country, address)
                    else:
                        row = (address, )

                    writer.writerow(row)

            i += 1
            if i % 1000 == 0 and i > 0:
                print('did {} formatted places'.format(i))

        for tags, poly in iter(self.components.osm_admin_rtree):
            try:
                point = poly.context.representative_point()
            except ValueError:
                point = poly.context.centroid
            lat = point.y
            lon = point.x
            tags['lat'] = lat
            tags['lon'] = lon
            place_tags, country = self.node_place_tags(tags)
            for address_components, language, is_default in place_tags:
                addresses = self.formatted_places(address_components, country, language)
                if language is None:
                    language = UNKNOWN_LANGUAGE
                language = language.lower()

                for address in addresses:
                    if not address or not address.strip():
                        continue

                    address = tsv_string(address)
                    if tag_components:
                        row = (language, country, address)
                    else:
                        row = (address, )

                    writer.writerow(row)

            i += 1
            if i % 1000 == 0 and i > 0:
                print('did {} formatted places'.format(i))

    def build_intersections_training_data(self, infile, out_dir, tag_components=True):
        '''
        Intersection addresses like "4th & Main Street" are represented in OSM
        by ways that share at least one node.

        This creates formatted strings using the name of each way (sometimes the base name
        for US addresses thanks to Tiger tags).

        Example:

        en  us  34th/road Street/road &/intersection 8th/road Ave/road
        '''
        i = 0

        if tag_components:
            formatted_tagged_file = open(os.path.join(out_dir, INTERSECTIONS_TAGGED_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')
        else:
            formatted_file = open(os.path.join(out_dir, INTERSECTIONS_FILENAME), 'w')
            writer = csv.writer(formatted_file, 'tsv_no_quote')

        all_name_tags = set(OSM_NAME_TAGS)
        all_base_name_tags = set(OSM_BASE_NAME_TAGS)

        for node_id, node_props, ways in OSMIntersectionReader.read_intersections(infile):
            distinct_ways = set()
            valid_ways = []
            for way in ways:
                if way['id'] not in distinct_ways:
                    valid_ways.append(way)
                    distinct_ways.add(way['id'])
            ways = valid_ways

            if not ways or len(ways) < 2:
                continue

            latitude = node_props['lat']
            longitude = node_props['lon']

            try:
                latitude, longitude = latlon_to_decimal(latitude, longitude)
            except Exception:
                continue

            country, candidate_languages, language_props = self.language_rtree.country_and_languages(latitude, longitude)
            if not (country and candidate_languages):
                continue

            more_than_one_official_language = sum((1 for l in candidate_languages if int(l['default']))) > 1

            base_name_tag = None
            for t in all_base_name_tags:
                if any((t in way for way in ways)):
                    base_name_tag = t
                    break

            way_names = []
            namespaced_languages = set([None])
            default_language = None

            for way in ways:
                names = defaultdict(list)

                if len(candidate_languages) == 1:
                    default_language = candidate_languages[0]['lang']
                elif not more_than_one_official_language:
                    default_language = None
                    name = way['name']
                    if not name:
                        continue
                    address_language = self.components.address_language({AddressFormatter.ROAD: name}, candidate_languages)
                    if address_language and address_language not in (UNKNOWN_LANGUAGE, AMBIGUOUS_LANGUAGE):
                        default_language = address_language

                for tag in way:
                    tag = safe_decode(tag)
                    base_tag = tag.rsplit(six.u(':'), 1)[0]

                    normalized_tag = self.replace_numbered_tag(base_tag)
                    if normalized_tag:
                        base_tag = normalized_tag
                    if base_tag not in all_name_tags:
                        continue
                    lang = safe_decode(tag.rsplit(six.u(':'))[-1]) if six.u(':') in tag else None
                    if lang and lang.lower() in all_languages:
                        lang = lang.lower()
                    elif default_language:
                        lang = default_language
                    else:
                        continue

                    namespaced_languages.add(lang)

                    name = way[tag]
                    if default_language is None and tag == 'name':
                        address_language = self.components.address_language({AddressFormatter.ROAD: name}, candidate_languages)
                        if address_language and address_language not in (UNKNOWN_LANGUAGE, AMBIGUOUS_LANGUAGE):
                            default_language = address_language

                    names[lang].append((way[tag], False))

                if base_name_tag in way and default_language:
                    names[default_language].append((way[base_name_tag], True))

                if not names:
                    continue

                way_names.append(names)

            if not way_names or len(way_names) < 2:
                continue

            language_components = {}

            for namespaced_language in namespaced_languages:
                address_components, _, _ = self.components.expanded({}, latitude, longitude, language=namespaced_language or default_language)
                language_components[namespaced_language] = address_components

            for way1, way2 in itertools.combinations(way_names, 2):
                intersection_phrases = []
                for language in set(way1.keys()) & set(way2.keys()):
                    for (w1, w1_is_base), (w2, w2_is_base) in itertools.product(way1[language], way2[language]):
                        address_components = language_components[language]

                        intersection_phrase = Intersection.phrase(language, country=country)
                        if not intersection_phrase:
                            continue

                        w1 = self.components.cleaned_name(w1)
                        w2 = self.components.cleaned_name(w2)

                        if not w1_is_base:
                            w1 = self.abbreviated_street(w1, language)

                        if not w2_is_base:
                            w2 = self.abbreviated_street(w2, language)

                        intersection = IntersectionQuery(road1=w1, intersection_phrase=intersection_phrase, road2=w2)

                        formatted = self.formatter.format_intersection(intersection, address_components, country, language, tag_components=tag_components)

                        if not formatted or not formatted.strip():
                            continue

                        formatted = tsv_string(formatted)
                        if not formatted or not formatted.strip():
                            continue

                        if tag_components:
                            row = (language, country, formatted)
                        else:
                            row = (formatted,)

                        writer.writerow(row)

            i += 1
            if i % 1000 == 0 and i > 0:
                print('did {} intersections'.format(i))

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

        f = open(os.path.join(out_dir, FORMATTED_ADDRESS_DATA_LANGUAGE_FILENAME), 'w')
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