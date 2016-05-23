# -*- coding: utf-8 -*-
import os
import random
import six
import sys
import yaml

from collections import OrderedDict

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
from geodata.i18n.languages import *
from geodata.address_formatting.formatter import AddressFormatter
from geodata.osm.extract import *
from geodata.polygons.language_polys import *
from geodata.polygons.reverse_geocode import *
from geodata.i18n.unicode_paths import DATA_DIR

from geodata.csv_utils import *
from geodata.file_utils import *

OSM_PARSER_DATA_DEFAULT_CONFIG = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                              'resources', 'parser', 'data_sets', 'osm.yaml')

ADDRESS_FORMAT_DATA_TAGGED_FILENAME = 'formatted_addresses_tagged.tsv'
ADDRESS_FORMAT_DATA_FILENAME = 'formatted_addresses.tsv'
ADDRESS_FORMAT_DATA_LANGUAGE_FILENAME = 'formatted_addresses_by_language.tsv'


class OSMAddressFormatter(object):
    aliases = Aliases(
        OrderedDict([
            ('name', AddressFormatter.HOUSE),
            ('addr:housename', AddressFormatter.HOUSE),
            ('addr:housenumber', AddressFormatter.HOUSE_NUMBER),
            ('addr:house_number', AddressFormatter.HOUSE_NUMBER),
            ('addr:street', AddressFormatter.ROAD),
            ('addr:place', AddressFormatter.ROAD),
            ('level', AddressFormatter.LEVEL),
            ('addr:floor', AddressFormatter.LEVEL),
            ('addr:unit', AddressFormatter.UNIT),
            ('addr:flats', AddressFormatter.UNIT),
            ('addr:door', AddressFormatter.UNIT),
            ('addr:suite', AddressFormatter.UNIT),
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

    def __init__(self, components):
        # Instance of AddressComponents, contains structures for reverse geocoding, etc.
        self.components = components
        self.language_rtree = components.language_rtree

        self.config = yaml.load(open(OSM_PARSER_DATA_DEFAULT_CONFIG))
        self.formatter = AddressFormatter()

    def pick_language(self, osm_tags, candidate_languages):
        language = None

        pick_namespaced_language_prob = float(nested_get(self.config, ('languages', 'pick_namespaced_language_probability'), default=0.0))

        if len(candidate_languages) == 1:
            language = candidate_languages[0]['lang']
        else:
            street = osm_tags.get('addr:street', None)

            namespaced = [l['lang'] for l in candidate_languages if 'addr:street:{}'.format(l['lang']) in osm_tags]

            if street is not None and not namespaced:
                language = disambiguate_language(street, [(l['lang'], l['default']) for l in candidate_languages])
            elif namespaced and random.random() < pick_namespaced_language_prob:
                language = random.choice(namespaced)
                lang_suffix = ':{}'.format(language)
                for k in osm_tags:
                    if k.startswith('addr:') and k.endswith(lang_suffix):
                        osm_tags[k.rstrip(lang_suffix)] = osm_tags[k]
            else:
                language = UNKNOWN_LANGUAGE

        return language

    def normalize_address_components(self, tags):
        address_components = {k: v for k, v in six.iteritems(tags) if self.aliases.get(k)}
        self.aliases.replace(address_components)
        address_components = {k: v for k, v in six.iteritems(address_components) if k in AddressFormatter.address_formatter_fields}
        return address_components

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
        abbreviate_prob = float(nested_get(self.config, ('street', 'abbreviate_probability'), default=0.0))
        separate_prob = float(nested_get(self.config, ('street', 'separate_probability'), default=0.0))

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
        abbreviate_prob = float(nested_get(self.config, ('venue', 'abbreviate_probability'), default=0.0))
        separate_prob = float(nested_get(self.config, ('venue', 'separate_probability'), default=0.0))

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
            props['addr:street'] = six.u(', ').join(street, safe_decode(parent_street))
            return True
        return False

    def venue_names(self, props):
        '''
        Venue names
        -----------

        Some venues have multiple names listed in OSM, grab them all
        With a certain probability, add None to the list so we drop the name
        '''

        venue_names = []
        for key in ('name', 'alt_name', 'loc_name', 'int_name', 'old_name'):
            venue_name = props.get(key)
            if venue_name:
                venue_names.append(venue_name)
        return venue_names

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

        venue_names = self.venue_names(tags) or []

        try:
            latitude, longitude = latlon_to_decimal(tags['lat'], tags['lon'])
        except Exception:
            return None, None, None

        combined_street = self.combine_street_name(tags)

        revised_tags = self.normalize_address_components(tags)

        address_components, country, language = self.components.expanded(revised_tags, latitude, longitude)

        if not address_components:
            return None, None, None

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

        revised_tags = self.normalize_address_components(tags)

        admin_dropout_prob = float(nested_get(self.config, ('limited', 'admin_dropout_prob'), default=0.0))

        address_components, country, language = self.components.limited(revised_tags, latitude, longitude)

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
