# -*- coding: utf-8 -*-
import os
import random
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
from geodata.addresses.config import address_config
from geodata.addresses.components import AddressComponents
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
from geodata.osm.extract import *
from geodata.osm.intersections import OSMIntersectionReader
from geodata.polygons.language_polys import *
from geodata.polygons.reverse_geocode import *
from geodata.i18n.unicode_paths import DATA_DIR
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

    def __init__(self, components, subdivisions_rtree=None, buildings_rtree=None):
        # Instance of AddressComponents, contains structures for reverse geocoding, etc.
        self.components = components
        self.language_rtree = components.language_rtree

        self.subdivisions_rtree = subdivisions_rtree
        self.buildings_rtree = buildings_rtree

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
            return

        block = address_components.get('addr:block_number')
        if not block or not block.isdigit():
            return

        separator = six.u('-')

        combine_probability = float(nested_get(self.config, ('countries', 'jp', 'combine_block_house_number_probability'), default=0.0))
        if random.random() < combine_probability:
            if random.random() < float(nested_get(self.config, ('countries', 'jp', 'block_phrase_probability'), default=0.0)):
                block = Block.phrase(language, block_number)
                house_number = HouseNumber.phrase(house_number, language)
                if block is None or house_number is None:
                    return
                separator = six.u(' ') if language == JAPANESE_ROMAJI else six.u('')

            house_number = separator.join([block, house_number])
            address_components['addr:housenumber'] = house_number

    def venue_names(self, props, languages):
        '''
        Venue names
        -----------

        Some venues have multiple names listed in OSM, grab them all
        With a certain probability, add None to the list so we drop the name
        '''

        return self.components.all_names(props, languages, keys=('name', 'alt_name', 'loc_name', 'int_name', 'old_name'))

    def formatted_addresses_with_venue_names(self, address_components, venue_names, country, language=None, tag_components=True, minimal_only=False):
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

    def node_place_tags(self, tags):
        try:
            latitude, longitude = latlon_to_decimal(tags['lat'], tags['lon'])
        except Exception:
            return None, None, None

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
                return None, None, None

            local_languages = [(lang, bool(int(default))) for lang, default in get_country_languages(country).iteritems()]

        all_local_languages = set([l for l, d in local_languages])
        random_languages = set(INTERNET_LANGUAGE_DISTRIBUTION)

        more_than_one_official_language = len([lang for lang, default in local_languages if default]) > 1

        containing_ids = [(b['type'], b['id']) for b in osm_components]

        component_name = osm_address_components.component_from_properties(country, tags, containing=containing_ids)
        component_index = self.boundary_component_priorities.get(component_name)

        if component_index:
            osm_components = [c for i, c in enumerate(osm_components)
                              if self.boundary_component_priorities.get(osm_address_components.component_from_properties(country, c, containing=containing_ids[i + 1:]), -1) >= component_index and
                              (c['type'], c['id']) != (tags['type'], tags['id'])]

        # Do addr:postcode, postcode, postal_code, etc.
        revised_tags = self.normalize_address_components(tags)

        place_tags = []

        postal_code = revised_tags.get(AddressFormatter.POSTCODE, None)
        postal_codes = []
        if postal_code:
            postal_codes = parse_osm_number_range(postal_code, parse_letter_range=False)

        try:
            population = int(tags.get('population', 0))
        except (ValueError, TypeError):
            population = 0

        num_references = population / 10000 + 1

        for name_tag in ('name', 'alt_name', 'loc_name', 'short_name'):
            if more_than_one_official_language:
                name = tags.get(name_tag)
                language_suffix = ''

                if name and name.strip():
                    address_components = {component_name: name.strip()}
                    self.components.add_admin_boundaries(address_components, osm_components, country, language,
                                                         language_suffix=language_suffix)

                    self.components.normalize_place_names(address_components, osm_components, country=country, languages=all_local_languages)

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

                address_components = {component_name: name.strip()}
                self.components.add_admin_boundaries(address_components, osm_components, country, language,
                                                     language_suffix=language_suffix)

                self.components.normalize_place_names(address_components, osm_components, country=country, languages=all_local_languages)

                place_tags.append((address_components, language, is_default))

            for language in random_languages - all_local_languages:
                language_suffix = ':{}'.format(language)
                name = tags.get('{}{}'.format(name_tag, language_suffix))
                if not name or not name.strip():
                    continue

                address_components = {component_name: name.strip()}
                self.components.add_admin_boundaries(address_components, osm_components, country, language,
                                                     non_local_language=language,
                                                     language_suffix=language_suffix)

                self.components.normalize_place_names(address_components, osm_components, country=country, languages=set([language]))
                place_tags.append((address_components, language, False))

            if postal_codes:
                for address_components in place_tags:
                    address_components[AddressFormatter.POSTCODE] = random.choice(postal_codes)

        return place_tags, num_references, country

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

        if country == JAPAN:
            language = JAPANESE
            if random.random() < float(nested_get(self.config, ('countries', 'jp', 'romaji_probability'), default=0.0)):
                language = JAPANESE_ROMAJI
            self.combine_japanese_house_number(tags, language)

        revised_tags = self.normalize_address_components(tags)
        sub_building_tags = self.normalize_sub_building_components(tags)
        revised_tags.update(sub_building_tags)

        num_floors = None
        num_basements = None
        zone = None

        building_components = self.building_components(latitude, longitude)
        if building_components:
            num_floors = self.num_floors(building_components)
            num_basements = self.num_floors(building_components, key='building:levels:underground')

            building_tags = self.normalize_address_components(building_components)

            for k, v in six.iteritems(building_tags):
                if k not in revised_tags and k in (AddressFormatter.HOUSE_NUMBER, AddressFormatter.ROAD, AddressFormatter.HOUSE):
                    revised_tags[k] = v

        subdivision_components = self.subdivision_components(latitude, longitude)
        if subdivision_components:
            zone = self.zone(subdivision_components)

        add_sub_building_components = AddressFormatter.HOUSE_NUMBER in revised_tags

        address_components, country, language = self.components.expanded(revised_tags, latitude, longitude, language=namespaced_language,
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

        # Ditto for venue names
        for venue_name in venue_names:
            abbreviated_venue = self.abbreviated_venue_name(venue_name, language)
            if abbreviated_venue != venue_name and abbreviated_venue not in set(venue_names):
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
            dropout_order = self.components.address_level_dropout_order(address_components)

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
            place_tags, num_references, country = self.node_place_tags(tags)
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

                    for j in xrange(num_references if is_default else 1):
                        writer.writerow(row)

            i += 1
            if i % 1000 == 0 and i > 0:
                print('did {} formatted places'.format(i))

    def build_intersections_training_data(self, infile, out_dir, way_db_dir, tag_components=True):
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

        replace_with_base_name_prob = float(nested_get(self.config, ('intersections', 'replace_with_base_name_probability'), default=0.0))

        reader = OSMIntersectionReader(infile, way_db_dir)
        for node_id, latitude, longitude, ways in reader.intersections():
            if not ways or len(ways) < 2:
                continue

            tags = ways[0]
            namespaced_language = None

            language_components = {}

            base_name_tags = [t for t in all_base_name_tags if t in tags]
            if not base_name_tags:
                base_name_tag = None
            else:
                base_name_tag = base_name_tags[0]

            for tag in tags:
                if tag.rsplit(':', 1)[0] in all_name_tags:
                    way_names = [(w[tag], w.get(base_name_tag) if base_name_tag else None) for w in ways if tag in w]
                    if len(way_names) < 2:
                        continue
                    if ':' in tag:
                        namespaced_language = tag.rsplit(':')[-1]

                    if namespaced_language not in language_components:
                        address_components, country, language = self.components.expanded({}, latitude, longitude, language=namespaced_language)
                        language_components[namespaced_language] = (address_components, country, language)
                    else:
                        address_components, country, language = language_components[namespaced_language]

                    intersection_phrase = Intersection.phrase(language, country=country)
                    if not intersection_phrase:
                        continue

                    formatted_intersections = []

                    for (w1, w1_base), (w2, w2_base) in itertools.combinations(way_names, 2):
                        intersection = IntersectionQuery(road1=w1, intersection_phrase=intersection_phrase, road2=w2)
                        formatted = self.formatter.format_intersection(intersection, address_components, country, language, tag_components=tag_components)
                        formatted_intersections.append(formatted)

                        if w1_base and random.random() < replace_with_base_name_prob:
                            w1 = w1_base

                            intersection = IntersectionQuery(road1=w1, intersection_phrase=intersection_phrase, road2=w2)
                            formatted = self.formatter.format_intersection(intersection, address_components, country, language, tag_components=tag_components)
                            formatted_intersections.append(formatted)

                        if w2_base and random.random() < replace_with_base_name_prob:
                            w2 = w2_base

                            intersection = IntersectionQuery(road1=w1, intersection_phrase=intersection_phrase, road2=w2)
                            formatted = self.formatter.format_intersection(intersection, address_components, country, language, tag_components=tag_components)
                            formatted_intersections.append(formatted)

                    for formatted in formatted_intersections:
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
