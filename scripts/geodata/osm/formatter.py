# -*- coding: utf-8 -*-
import itertools
import os
import random
import re
import six
import sys
import ftfy
import yaml

from collections import defaultdict, OrderedDict, Counter
from shapely.geometry import Point
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
from geodata.countries.constants import Countries
from geodata.addresses.conscription_numbers import ConscriptionNumber
from geodata.addresses.house_numbers import HouseNumber
from geodata.boundaries.names import boundary_names
from geodata.categories.config import category_config
from geodata.categories.query import Category, NULL_CATEGORY_QUERY
from geodata.chains.query import Chain, NULL_CHAIN_QUERY
from geodata.coordinates.conversion import *
from geodata.configs.utils import nested_get
from geodata.countries.country_names import *
from geodata.language_id.disambiguation import *
from geodata.language_id.sample import INTERNET_LANGUAGE_DISTRIBUTION
from geodata.i18n.languages import *
from geodata.intersections.query import Intersection, IntersectionQuery
from geodata.address_formatting.formatter import AddressFormatter
from geodata.osm.components import osm_address_components
from geodata.osm.definitions import osm_definitions
from geodata.osm.extract import *
from geodata.osm.intersections import OSMIntersectionReader
from geodata.places.config import place_config
from geodata.polygons.language_polys import *
from geodata.polygons.reverse_geocode import *
from geodata.postal_codes.phrases import PostalCodes
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
WAYS_TAGGED_FILENAME = 'formatted_ways_tagged.tsv'
WAYS_FILENAME = 'formatted_ways.tsv'

ALL_LANGUAGES = 'all'

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
            ('addr:postcode', AddressFormatter.POSTCODE),
            ('addr:postal_code', AddressFormatter.POSTCODE),
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
            'university': AddressComponents.zones.UNIVERSITY,
            'college': AddressComponents.zones.UNIVERSITY,
        },
        'amenity': {
            'university': AddressComponents.zones.UNIVERSITY,
            'college': AddressComponents.zones.UNIVERSITY,
        }
    }

    boundary_component_priorities = {k: i for i, k in enumerate(AddressFormatter.BOUNDARY_COMPONENTS_ORDERED)}

    def __init__(self, components, country_rtree, subdivisions_rtree=None, buildings_rtree=None, metro_stations_index=None):
        # Instance of AddressComponents, contains structures for reverse geocoding, etc.
        self.components = components
        self.country_rtree = country_rtree

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

            namespaced = [l for l, d in candidate_languages if 'addr:street:{}'.format(l) in tags]

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

    def fix_component_encodings(self, tags):
        return {k: ftfy.fix_encoding(safe_decode(v)) for k, v in six.iteritems(tags)}

    def normalized_street_name(self, address_components, country=None, language=None):
        street = address_components.get(AddressFormatter.ROAD)
        if street and ',' in street:
            street_parts = [part.strip() for part in street.split(',')]

            if len(street_parts) > 1 and (street_parts[-1].lower() == address_components.get(AddressFormatter.HOUSE_NUMBER, '').lower()) and self.formatter.house_number_before_road(country, language):
                street = street_parts[0]
                return street

        return None

    @classmethod
    def alphanumeric_tokens(cls, name):
        return [t for t, c in tokenize(name) if c in token_types.WORD_TOKEN_TYPES or c in token_types.NUMERIC_TOKEN_TYPES]

    @classmethod
    def valid_venue_name(cls, name, address_components, languages=(), is_generic=False, is_known_venue_type=False):
        '''
        The definition of "what is a venue" is pretty loose in libpostal, but
        there are several cases we want to avoid:

        Firstly, if the venue name is the same as the house number, usually in the
        case of a named building, it's not valid.

        Sometimes a particular street is synonymous with a tourist attraction or neighborhood
        or historic district and OSM might label it as something like a landmark,
        which passes the "named place" test but is really just a street, so if the
        name of the place is exactly equal to the street name and there's no house number
        (this still allows for bars or restaurants, etc. that might be named after the
        street they're on), call it invalid.

        Often the name or a building in OSM will just be its street address. Example:

        addr:housename="123 Park Place"
        addr:housenumber="123"
        addr:street="Park Place"

        This method will invalidate the venue name if and only if
        the set of unique tokens in the venue name is exactly equal
        to the set of unique tokens in the house number + street name.
        This does not apply to known venue typess (like a restaurant)
        where we'd want to preserve the name.
        '''

        if not name:
            return False

        name_norm = name.strip().lower()

        street = address_components.get(AddressFormatter.ROAD)
        if street:
            street = street.strip().lower()

        house_number = address_components.get(AddressFormatter.HOUSE_NUMBER)
        if house_number:
            house_number = house_number.strip().lower()

        if street and street == name_norm and not house_number:
            return False

        if not house_number:
            if not any((c in token_types.WORD_TOKEN_TYPES for t, c in tokenize(name))):
                return False

            if not street:
                for component, place_name in six.iteritems(address_components):
                    if (component in AddressFormatter.BOUNDARY_COMPONENTS or component == AddressFormatter.HOUSE_NUMBER) and place_name.strip().lower() == name_norm:
                        return False

        if street and not is_known_venue_type:
            unique_tokens_name = set([t.lower() for t in cls.alphanumeric_tokens(name)])

            unique_tokens_street_house_number = set()
            if street:
                unique_tokens_street_house_number.update([t.lower() for t in cls.alphanumeric_tokens(street)])

            if house_number:
                unique_tokens_street_house_number.update([t.lower() for t in cls.alphanumeric_tokens(house_number)])

            if len(unique_tokens_name) == len(unique_tokens_street_house_number) == len(unique_tokens_name & unique_tokens_street_house_number):
                return False

        venue_phrases = venue_names_gazetteer.extract_phrases(name, languages=languages)
        street_phrases = street_types_only_gazetteer.extract_phrases(name, languages=languages)

        if street_phrases - venue_phrases and not venue_phrases - street_phrases and not (AddressFormatter.HOUSE_NUMBER in address_components and AddressFormatter.ROAD in address_components):
            return False

        if is_generic and not venue_phrases:
            return False

        if not address_components and not venue_phrases:
            return False

        return True

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

    def remove_japanese_suburb_tags(self, tags):
        for tag in ('addr:neighbourhood', 'addr:neighbourhood', 'addr:quarter', 'addr:suburb'):
            tags.pop(tag, None)

    def conscription_number(self, tags, language, country):
        conscription_number = tags.get('addr:conscriptionnumber', None)
        if conscription_number is not None:
            phrase_probability = float(nested_get(self.config, ('conscription_numbers', 'phrase_probability'), default=0.0))
            no_phrase_probability = float(nested_get(self.config, ('conscription_numbers', 'no_phrase_probability'), default=0.0))

            if random.random() < phrase_probability:
                return ConscriptionNumber.phrase(conscription_number, language, country=country)
            elif random.random() < no_phrase_probability:
                return safe_decode(conscription_number)

        return None

    def austro_hungarian_street_number(self, tags, language, country):
        austro_hungarian_street_number = tags.get('addr:streetnumber', None)

        if austro_hungarian_street_number is not None:
            no_phrase_probability = float(nested_get(self.config, ('austro_hungarian_street_numbers', 'no_phrase_probability'), default=0.0))
            if random.random() < no_phrase_probability:
                return safe_decode(austro_hungarian_street_number)

        return None

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

        if not venue_names:
            address_components = {c: v for c, v in six.iteritems(address_components) if c != AddressFormatter.HOUSE}
            return [self.formatter.format_address(address_components, country, language=language,
                                                  tag_components=tag_components, minimal_only=minimal_only)]

        address_prob = float(nested_get(self.config, ('venues', 'address_probability'), default=0.0))
        if random.random() < address_prob and AddressFormatter.HOUSE in address_components:
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

    @classmethod
    def valid_postal_code(self, country, postal_code):
        return PostalCodes.is_valid(postal_code, country)

    def parse_valid_postal_codes(self, country, postal_code, validate=True):
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
                values = number_split_regex.split(postal_code)
                for p in values:
                    if self.valid_postal_code(country, p):
                        valid_postcode = True
                        postal_codes.append(p)
            else:
                valid_postcode = True

            if not valid_postcode:
                postal_codes = parse_osm_number_range(postal_code, parse_letter_range=False, max_range=1000)
                if validate:
                    valid_postal_codes = []
                    for pc in postal_codes:
                        if self.valid_postal_code(country, pc):
                            valid_postal_codes.append(pc)
                    postal_codes = valid_postal_codes

        return postal_codes

    def expand_postal_codes(self, postal_code, country, languages, osm_components):
        '''
        Expanded postal codes
        ---------------------

        Clean up OSM addr:postcode tag. Sometimes it will be a full address
        e.g. addr:postcode="750 Park Pl, Brooklyn, NY 11216", sometimes
        just "NY 11216", etc.
        '''
        match = number_split_regex.search(postal_code)
        valid = []

        should_strip_components = PostalCodes.should_strip_components(country)
        needs_validation = PostalCodes.needs_validation(country)

        if not match:
            if not should_strip_components and not needs_validation:
                valid.append(PostalCodes.add_country_code(postal_code, country))
                return valid

            if should_strip_components:
                postal_code = self.components.strip_components(postal_code, osm_components, country, languages)

            if not needs_validation or PostalCodes.is_valid(postal_code, country):
                valid.append(PostalCodes.add_country_code(postal_code, country))

        else:
            candidates = number_split_regex.split(postal_code)
            for candidate in candidates:
                candidate = candidate.strip()
                if should_strip_components:
                    candidate = self.components.strip_components(candidate, osm_components, country, languages)
                    if not candidate:
                        continue

                # If we're splitting, validate every delimited phrase
                if PostalCodes.is_valid(candidate, country):
                    valid.append(PostalCodes.add_country_code(candidate, country))

        return valid

    def cleanup_place_components(self, address_components, osm_components, country, language, containing_ids, population=None, keep_component=None, population_from_city=False):
        revised_address_components = self.components.dropout_places(address_components, osm_components, country, language, population=population, population_from_city=population_from_city)

        if keep_component is not None:
            revised_address_components[keep_component] = address_components[keep_component]

        self.components.cleanup_boundary_names(revised_address_components)
        self.components.country_specific_cleanup(revised_address_components, country)

        self.components.drop_invalid_components(revised_address_components, country)

        self.components.replace_name_affixes(revised_address_components, language, country=country)
        self.components.replace_names(revised_address_components)

        self.components.remove_numeric_boundary_names(revised_address_components)

        cldr_country_prob = float(nested_get(self.config, ('places', 'cldr_country_probability'), default=0.0))

        if (AddressFormatter.COUNTRY in revised_address_components or place_config.include_component(AddressFormatter.COUNTRY, containing_ids, country=country, check_population=False)) and random.random() < cldr_country_prob:
            address_country = self.components.cldr_country_name(country, language)
            if address_country:
                revised_address_components[AddressFormatter.COUNTRY] = address_country

        return revised_address_components

    def node_place_tags(self, tags, city_or_below=False):
        try:
            latitude, longitude = latlon_to_decimal(tags['lat'], tags['lon'])
        except Exception:
            return (), None

        if 'name' not in tags:
            return (), None

        osm_components = self.components.osm_reverse_geocoded_components(latitude, longitude)

        country, candidate_languages = OSMCountryReverseGeocoder.country_and_languages_from_components(osm_components)
        if not (country and candidate_languages):
            return (), None

        local_languages = candidate_languages

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

        if city_or_below and (not component_index or component_index <= self.boundary_component_priorities[AddressFormatter.CITY]):
            return (), None

        if component_index:
            revised_osm_components = []

            same_name_components = [None] * len(osm_components)

            # If this node is the admin center for one of its containing polygons, note it
            first_valid_admin_center = None

            for i, c in enumerate(osm_components):
                c_name = osm_address_components.component_from_properties(country, c, containing=containing_ids[i + 1:])

                if c.get('name', '').lower() == tags['name'].lower():
                    same_name_components[i] = c_name

                if first_valid_admin_center is None:
                    if (component_index <= self.boundary_component_priorities[AddressFormatter.CITY] and
                       tags.get('type') == 'node' and 'admin_center' in c and
                       tags.get('id') and c['admin_center']['id'] == tags['id'] and
                       c.get('name', '').lower() == tags['name'].lower()):
                            first_valid_admin_center = i

            # Check if, for instance, a node is labeled place=town, but its enclosing polygon with the same name
            # is city_district or suburb. However, sometimes there's a "suburb" version of a city indicating the
            # city proper, though there's also a city polygon encompassing the municipality. Don't want to classify
            # the unqualified city node as suburb.
            same_name_and_level_exists = any((c_name for c_name in same_name_components if c_name == component_name))
            same_name_max_below_level = None

            for i, c in enumerate(osm_components):
                c_name = osm_address_components.component_from_properties(country, c, containing=containing_ids[i + 1:])
                c_index = self.boundary_component_priorities.get(c_name, -1)

                if first_valid_admin_center == i:
                    component_name = c_name
                    component_index = c_index
                    continue

                same_name_component = same_name_components[i]
                if same_name_component is not None and not same_name_and_level_exists and c_index < component_index:
                    same_name_max_below_level = c_name

            if same_name_max_below_level is not None and not same_name_and_level_exists and first_valid_admin_center is None:
                component_name = same_name_max_below_level
                component_index = self.boundary_component_priorities.get(same_name_max_below_level, -1)

            # Add any admin components above the computed level
            for i, c in enumerate(osm_components):
                c_name = osm_address_components.component_from_properties(country, c, containing=containing_ids[i + 1:])
                c_index = self.boundary_component_priorities.get(c_name, -1)

                same_name_component = same_name_components[i]
                if c_index >= component_index and first_valid_admin_center != i and same_name_component != component_name and (c['type'], c['id']) != (tags.get('type', 'node'), tags.get('id')):
                    revised_osm_components.append(c)
                    added_component = True

            osm_components = revised_osm_components

        # Do addr:postcode, postcode, postal_code, etc.
        revised_tags = self.normalize_address_components(tags)

        place_tags = []

        postal_code = revised_tags.get(AddressFormatter.POSTCODE, None)

        postal_codes = []
        if postal_code:
            postal_codes = self.parse_valid_postal_codes(country, postal_code)

        try:
            population = int(tags.get('population', 0))
        except (ValueError, TypeError):
            population = 0

        # Calculate how many records to produce for this place given its population
        population_divisor = 10000  # Add one record for every 10k in population
        min_references = 5  # Every place gets at least 5 reference to account for variations
        if component_name == AddressFormatter.CITY:
            # Cities get a few extra references over e.g. a state_district with the same name
            # so that if the population is unknown, hopefully the city will have more references
            # and the parser will prefer that meaning
            min_references += 2
        max_references = 1000  # Cap the number of references e.g. for India and China country nodes
        num_references = min(population / population_divisor + min_references, max_references)

        component_order = AddressFormatter.component_order[component_name]
        sub_city = component_order < AddressFormatter.component_order[AddressFormatter.CITY]

        revised_tags = self.fix_component_encodings(revised_tags)

        object_type = tags.get('type')
        object_id = tags.get('id')

        for name_tag in ('name', 'alt_name', 'loc_name', 'short_name', 'int_name', 'name:simple', 'official_name'):
            if not boundary_names.valid_name(object_type, object_id, name_tag):
                continue

            if more_than_one_official_language:
                name = tags.get(name_tag)
                language_suffix = ''

                if name and name.strip():
                    if u';' in name:
                        name = random.choice(name.split(u';'))
                    elif u',' in name:
                        name = name.split(u',', 1)[0]

                    if u'|' in name:
                        name = name.replace(u'|', u'')

                    name = self.components.strip_whitespace_and_hyphens(name)

                    alt_names = self.components.alt_place_names(name, None)

                    for i in xrange(num_references if name_tag == 'name' else 1):
                        address_components = {component_name: name}

                        self.components.add_admin_boundaries(address_components, osm_components, country, UNKNOWN_LANGUAGE,
                                                             latitude, longitude,
                                                             random_key=num_references > 1,
                                                             language_suffix=language_suffix,
                                                             drop_duplicate_city_names=False,
                                                             add_city_points=sub_city)

                        place_tags.append((address_components, None, True))
                        for alt_name in alt_names:
                            address_components = address_components.copy()
                            address_components[component_name] = alt_name
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

                if six.u('|') in name:
                    name = name.replace(six.u('|'), six.u(''))

                n = min_references / 2
                if name_tag == 'name':
                    if is_default:
                        n = num_references
                    else:
                        n = num_references / 2

                name = self.components.strip_whitespace_and_hyphens(name)

                alt_names = self.components.alt_place_names(name, language)

                for i in xrange(n):
                    address_components = {component_name: name}
                    self.components.add_admin_boundaries(address_components, osm_components, country, language,
                                                         latitude, longitude,
                                                         random_key=is_default,
                                                         language_suffix=language_suffix,
                                                         drop_duplicate_city_names=False,
                                                         add_city_points=sub_city)

                    place_tags.append((address_components, language, is_default))
                    for alt_name in alt_names:
                        address_components = address_components.copy()
                        address_components[component_name] = alt_name
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

                if six.u('|') in name:
                    name = name.replace(six.u('|'), six.u(''))

                name = self.components.strip_whitespace_and_hyphens(name)

                alt_names = self.components.alt_place_names(name, language)

                # Add half as many English records as the local language, every other language gets min_referenes / 2
                for i in xrange(num_references / 2 if language == ENGLISH else min_references / 2):
                    address_components = {component_name: name}
                    self.components.add_admin_boundaries(address_components, osm_components, country, language,
                                                         latitude, longitude,
                                                         random_key=False,
                                                         non_local_language=language,
                                                         language_suffix=language_suffix,
                                                         drop_duplicate_city_names=False,
                                                         add_city_points=sub_city)

                    place_tags.append((address_components, language, False))
                    for alt_name in alt_names:
                        address_components = address_components.copy()
                        address_components[component_name] = alt_name
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
            revised_address_components = self.cleanup_place_components(address_components, osm_components, country, language, containing_ids, population=population, keep_component=component_name)

            if revised_address_components:
                revised_place_tags.append((revised_address_components, language, is_default))

        return revised_place_tags, country

    @classmethod
    def is_generic_place(cls, tags):
        return tags.get('railway', '').lower() == 'station' or tags.get('amenity', '').lower() == 'post_office'

    @classmethod
    def is_known_venue_type(cls, tags):
        for definition in (osm_definitions.AMENITY, osm_definitions.SHOP, osm_definitions.AEROWAY):
            if osm_definitions.meets_definition(tags, definition):
                return True
        return False

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

        osm_components = self.components.osm_reverse_geocoded_components(latitude, longitude)

        country, candidate_languages = self.components.osm_country_and_languages(osm_components)
        if not (country and candidate_languages):
            return None, None, None

        all_local_languages = set([l for l, d in candidate_languages])
        random_languages = set(INTERNET_LANGUAGE_DISTRIBUTION)

        combined_street = self.combine_street_name(tags)

        namespaced_language = self.namespaced_language(tags, candidate_languages)
        language = None
        japanese_variant = None

        japanese_suburb = None

        if country == Countries.JAPAN:
            japanese_variant = JAPANESE
            if random.random() < float(nested_get(self.config, ('countries', 'jp', 'romaji_probability'), default=0.0)):
                japanese_variant = JAPANESE_ROMAJI
            if self.combine_japanese_house_number(tags, japanese_variant):
                language = japanese_variant
            else:
                language = None

            self.remove_japanese_suburb_tags(tags)

        is_generic_place = self.is_generic_place(tags)
        is_known_venue_type = self.is_known_venue_type(tags)

        revised_tags = self.normalize_address_components(tags)
        if japanese_suburb is not None:
            revised_tags[AddressFormatter.SUBURB] = japanese_suburb
        sub_building_tags = self.normalize_sub_building_components(tags)
        revised_tags.update(sub_building_tags)

        # Only including nearest metro station in Japan
        if country == Countries.JAPAN:
            if random.random() < float(nested_get(self.config, ('countries', 'jp', 'add_metro_probability'), default=0.0)):
                if self.add_metro_station(revised_tags, latitude, longitude, japanese_variant, default_language=JAPANESE):
                    language = japanese_variant

        num_floors = None
        num_basements = None
        zone = None

        postal_code = revised_tags.get(AddressFormatter.POSTCODE, None)
        expanded_postal_codes = []

        if postal_code:
            expanded_postal_codes = self.expand_postal_codes(postal_code, country, all_local_languages | random_languages, osm_components)

            if len(expanded_postal_codes) == 1:
                revised_tags[AddressFormatter.POSTCODE] = expanded_postal_codes[0]
            else:
                revised_tags.pop(AddressFormatter.POSTCODE)
                postal_code = None

        building_venue_names = []

        building_components = self.building_components(latitude, longitude)

        building_is_generic_place = False
        building_is_known_venue_type = False

        if building_components:
            num_floors = self.num_floors(building_components)

            num_basements = self.num_floors(building_components, key='building:levels:underground')

            for building_tags in building_components:
                building_tags = self.normalize_address_components(building_tags)

                building_is_generic_place = building_is_generic_place or self.is_generic_place(building_tags)
                building_is_known_venue_type = building_is_known_venue_type or self.is_known_venue_type(building_tags)

                for k, v in six.iteritems(building_tags):
                    if k not in revised_tags and k in (AddressFormatter.HOUSE_NUMBER, AddressFormatter.ROAD):
                        revised_tags[k] = v
                    elif k not in revised_tags and k == AddressFormatter.POSTCODE:
                        expanded_postal_codes = self.expand_postal_codes(v, country, all_local_languages | random_languages, osm_components)

                        if len(expanded_postal_codes) == 1:
                            revised_tags[AddressFormatter.POSTCODE] = expanded_postal_codes[0]
                    elif k == AddressFormatter.HOUSE:
                        building_venue_names.append((v, building_is_generic_place, building_is_known_venue_type))

        subdivision_components = self.subdivision_components(latitude, longitude)
        if subdivision_components:
            zone = self.zone(subdivision_components)

        venue_sub_building_prob = float(nested_get(self.config, ('venues', 'sub_building_probability'), default=0.0))
        add_sub_building_components = AddressFormatter.HOUSE_NUMBER in revised_tags and (AddressFormatter.HOUSE not in revised_tags or random.random() < venue_sub_building_prob)

        revised_tags = self.fix_component_encodings(revised_tags)

        address_components, country, language = self.components.expanded(revised_tags, latitude, longitude, language=language or namespaced_language,
                                                                         num_floors=num_floors, num_basements=num_basements,
                                                                         zone=zone, add_sub_building_components=add_sub_building_components,
                                                                         population_from_city=True, check_city_wikipedia=True, osm_components=osm_components)

        languages = list(country_languages[country])
        venue_names = self.venue_names(tags, languages) or []

        conscription_number = self.conscription_number(tags, language, country)
        austro_hungarian_street_number = self.austro_hungarian_street_number(tags, language, country)

        # Abbreviate the street name with random probability
        street_name = address_components.get(AddressFormatter.ROAD)

        if street_name:
            normalized_street_name = self.normalized_street_name(address_components, country, language)
            if normalized_street_name:
                street_name = normalized_street_name
                address_components[AddressFormatter.ROAD] = street_name

            address_components[AddressFormatter.ROAD] = self.abbreviated_street(street_name, language)

        expanded_components = address_components.copy()
        for props, component in self.components.categorized_osm_components(country, osm_components):
            if 'name' in props and component not in expanded_components:
                expanded_components[component] = props['name']

        street_languages = set((language,) if language not in (UNKNOWN_LANGUAGE, AMBIGUOUS_LANGUAGE) else languages)

        venue_names = [venue_name for venue_name in venue_names if self.valid_venue_name(venue_name, expanded_components, street_languages, is_generic=is_generic_place)]
        venue_names.extend([venue_name for venue_name, building_is_generic_place, building_is_known_venue_type in building_venue_names if self.valid_venue_name(venue_name, expanded_components, street_languages, is_generic=building_is_generic_place, is_known_venue_type=building_is_known_venue_type)])

        all_venue_names = set(venue_names)

        # Ditto for venue names
        for venue_name in all_venue_names:
            abbreviated_venue = self.abbreviated_venue_name(venue_name, language)
            if abbreviated_venue != venue_name and abbreviated_venue not in all_venue_names:
                venue_names.append(abbreviated_venue)

        if not address_components and not venue_names:
            return None, None, None

        reduced_venue_names = []
        expanded_only_venue_names = []

        for venue_name in venue_names:
            if self.valid_venue_name(venue_name, address_components, street_languages):
                reduced_venue_names.append(venue_name)
            else:
                expanded_only_venue_names.append(venue_name)

        formatted_addresses = self.formatted_addresses_with_venue_names(address_components, reduced_venue_names, country, language=language,
                                                                        tag_components=tag_components, minimal_only=not tag_components)

        for alternate_house_number in (conscription_number, austro_hungarian_street_number):
            if alternate_house_number is not None:
                original_house_number = address_components.get(AddressFormatter.HOUSE_NUMBER)
                address_components[AddressFormatter.HOUSE_NUMBER] = alternate_house_number
                formatted_addresses.extend(self.formatted_addresses_with_venue_names(address_components, reduced_venue_names, country, language=language,
                                                                                     tag_components=tag_components, minimal_only=not tag_components))
                if original_house_number:
                    address_components[AddressFormatter.HOUSE_NUMBER] = original_house_number

        if len(expanded_postal_codes) > 1:
            for postal_code in expanded_postal_codes:
                address_components[AddressFormatter.POSTCODE] = postal_code
                self.components.add_postcode_phrase(address_components, language, country=country)
                formatted_addresses.extend(self.formatted_addresses_with_venue_names(address_components, reduced_venue_names, country, language=language,
                                                                                     tag_components=tag_components, minimal_only=not tag_components))

        if expanded_only_venue_names:
            formatted_addresses.extend(self.formatted_addresses_with_venue_names(expanded_components, expanded_only_venue_names, country, language=language,
                                                                                 tag_components=tag_components, minimal_only=not tag_components))

        formatted_addresses.extend(self.formatted_places(address_components, country, language))

        # In Japan an address without places is basically just house_number + metro_station (if given)
        # However, where there are streets, it's useful to have address-only queries as well
        if country != Countries.JAPAN:
            address_only_components = self.components.drop_places(address_components)
            address_only_components = self.components.drop_postcode(address_only_components)

            if address_only_components:
                address_only_venue_names = []
                if not address_only_components or (len(address_only_components) == 1 and list(address_only_components)[0] == AddressFormatter.HOUSE):
                    address_only_venue_names = [venue_name for venue_name in venue_names if self.valid_venue_name(venue_name, address_only_components, street_languages)]

                formatted_addresses.extend(self.formatted_addresses_with_venue_names(address_only_components, address_only_venue_names, country, language=language,
                                                                                     tag_components=tag_components, minimal_only=False))

        # Generate a PO Box address at random (only returns non-None values occasionally) and add it to the list
        po_box_components = self.components.po_box_address(address_components, language, country=country)
        if po_box_components:
            formatted_addresses.extend(self.formatted_addresses_with_venue_names(po_box_components, venue_names, country, language=language,
                                                                                 tag_components=tag_components, minimal_only=False))

        formatted_addresses.extend(self.category_queries(tags, address_components, language, country, tag_components=tag_components))

        venue_name = tags.get('name')

        if venue_name:
            chain_queries = self.chain_queries(venue_name, address_components, language, country, tag_components=tag_components)
            formatted_addresses.extend(chain_queries)

        if tag_components and address_components:
            # Pick a random dropout order
            dropout_order = self.components.address_level_dropout_order(address_components, country)

            for component in dropout_order:
                address_components.pop(component, None)

                dropout_venue_names = venue_names
                if not address_components or (len(address_components) == 1 and list(address_components)[0] == AddressFormatter.HOUSE):
                    dropout_venue_names = [venue_name for venue_name in venue_names if self.valid_venue_name(venue_name, address_components, street_languages)]

                formatted_addresses.extend(self.formatted_addresses_with_venue_names(address_components, dropout_venue_names, country, language=language,
                                                                                     tag_components=tag_components, minimal_only=False))

        return OrderedDict.fromkeys(formatted_addresses).keys(), country, language

    def formatted_address_limited(self, tags):
        try:
            latitude, longitude = latlon_to_decimal(tags['lat'], tags['lon'])
        except Exception:
            return None, None, None

        country, candidate_languages = self.country_rtree.country_and_languages(latitude, longitude)
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
            point = None
            if 'admin_center' in tags and 'lat' in tags['admin_center'] and 'lon' in tags['admin_center']:
                admin_center = tags['admin_center']

                latitude = admin_center['lat']
                longitude = admin_center['lon']

                try:
                    latitude, longitude = latlon_to_decimal(latitude, longitude)
                    point = Point(longitude, latitude)
                except Exception:
                    point = None

            if point is None:
                try:
                    point = poly.context.representative_point()
                except ValueError:
                    point = poly.context.centroid

            try:
                lat = point.y
                lon = point.x
            except Exception:
                continue
            tags['lat'] = lat
            tags['lon'] = lon
            place_tags, country = self.node_place_tags(tags, city_or_below=True)
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

    def way_names(self, way, candidate_languages, base_name_tag='name', all_name_tags=frozenset(OSM_NAME_TAGS), all_base_name_tags=frozenset(OSM_BASE_NAME_TAGS)):
        names = defaultdict(list)

        more_than_one_official_language = sum((1 for l, d in candidate_languages if d)) > 1

        default_language = None

        if len(candidate_languages) == 1:
            default_language = candidate_languages[0][0]
        elif not more_than_one_official_language:
            default_language = None
            name = way.get('name')
            if name:
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

            name = way[tag]
            if default_language is None and tag == 'name':
                address_language = self.components.address_language({AddressFormatter.ROAD: name}, candidate_languages)
                if address_language and address_language not in (UNKNOWN_LANGUAGE, AMBIGUOUS_LANGUAGE):
                    default_language = address_language

            names[lang].append((way[tag], False))

        if base_name_tag in way and default_language:
            names[default_language].append((way[base_name_tag], True))

        return names

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

            country, candidate_languages = self.country_rtree.country_and_languages(latitude, longitude)
            if not (country and candidate_languages):
                continue

            base_name_tag = None
            for t in all_base_name_tags:
                if any((t in way for way in ways)):
                    base_name_tag = t
                    break

            way_names = []
            namespaced_languages = set([None])
            default_language = None

            for way in ways:
                names = self.way_names(way, candidate_languages, base_name_tag=base_name_tag)

                if not names:
                    continue

                namespaced_languages |= set(names)
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

    def build_ways_training_data(self, infile, out_dir, tag_components=True):
        '''
        Simple street names and their containing boundaries.

        Example:

        en  us  34th/road Street/road New/city York/city NY/state
        '''
        i = 0

        if tag_components:
            formatted_tagged_file = open(os.path.join(out_dir, WAYS_TAGGED_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')
        else:
            formatted_file = open(os.path.join(out_dir, WAYS_FILENAME), 'w')
            writer = csv.writer(formatted_file, 'tsv_no_quote')

        all_name_tags = set(OSM_NAME_TAGS)
        all_base_name_tags = set(OSM_BASE_NAME_TAGS)

        for key, value, deps in parse_osm(infile, allowed_types=WAYS_RELATIONS):
            latitude = value['lat']
            longitude = value['lon']

            try:
                latitude, longitude = latlon_to_decimal(latitude, longitude)
            except Exception:
                continue

            country, candidate_languages = self.country_rtree.country_and_languages(latitude, longitude)
            if not (country and candidate_languages):
                continue

            names = self.way_names(value, candidate_languages)

            if not names:
                continue

            osm_components = self.components.osm_reverse_geocoded_components(latitude, longitude)

            containing_ids = [(b['type'], b['id']) for b in osm_components]

            for lang, vals in six.iteritems(names):
                way_tags = []
                for v, is_base in vals:
                    for street_name in v.split(';'):
                        street_name = street_name.strip()
                        if street_name and self.components.street_name_is_valid(street_name):
                            address_components = {AddressFormatter.ROAD: street_name}

                            self.components.add_admin_boundaries(address_components, osm_components,
                                                                 country, lang,
                                                                 latitude, longitude)

                            revised_address_components = self.cleanup_place_components(address_components, osm_components, country, lang, containing_ids, population_from_city=True)

                            way_tags.append(revised_address_components)

                            normalized = self.abbreviated_street(street_name, lang)
                            if normalized and normalized != street_name:
                                revisd_address_components = revised_address_components.copy()
                                revised_address_components[AddressFormatter.ROAD] = normalized
                                way_tags.append(revised_address_components)

                for address_components in way_tags:
                    formatted = self.formatter.format_address(address_components, country, language=lang,
                                                              tag_components=tag_components, minimal_only=False)

                    if not formatted or not formatted.strip():
                        continue
                    writer.writerow((lang, country, tsv_string(formatted)))

            if i % 1000 == 0 and i > 0:
                print('did {} ways'.format(i))
            i += 1

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
