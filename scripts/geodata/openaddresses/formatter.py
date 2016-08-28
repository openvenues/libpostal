# -*- coding: utf-8 -*-

import csv
import itertools
import os
import random
import re
import six
import yaml

from geodata.addresses.units import Unit
from geodata.address_expansions.abbreviations import abbreviate
from geodata.address_expansions.address_dictionaries import address_phrase_dictionaries
from geodata.address_expansions.gazetteers import street_types_gazetteer, unit_types_gazetteer
from geodata.address_formatting.formatter import AddressFormatter
from geodata.addresses.components import AddressComponents
from geodata.countries.names import country_names
from geodata.encoding import safe_decode, safe_encode
from geodata.language_id.disambiguation import UNKNOWN_LANGUAGE
from geodata.math.sampling import cdf, weighted_choice
from geodata.text.utils import is_numeric, is_numeric_strict

from geodata.csv_utils import tsv_string, unicode_csv_reader

this_dir = os.path.realpath(os.path.dirname(__file__))

OPENADDRESSES_PARSER_DATA_CONFIG = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                                'resources', 'parser', 'data_sets', 'openaddresses.yaml')

OPENADDRESS_FORMAT_DATA_TAGGED_FILENAME = 'openaddresses_formatted_addresses_tagged.tsv'
OPENADDRESS_FORMAT_DATA_FILENAME = 'openaddresses_formatted_addresses.tsv'

null_regex = re.compile('^\s*(?:null|none)\s*$', re.I)
unknown_regex = re.compile('^\s*(?:unknown)\s*$', re.I)
not_applicable_regex = re.compile('^\s*n\.?\s*/?\s*a\.?\s*$', re.I)
sin_numero_regex = re.compile('^\s*s\s\s*/\s*n\s*$')

SPANISH = 'es'


class OpenAddressesFormatter(object):
    field_regex_replacements = {
        # All fields
        None: [
            (re.compile('<\s*null\s*>', re.I), six.u('')),
            (re.compile('[\s]{2,}'), six.u(' '))
        ],
        AddressFormatter.HOUSE_NUMBER: [
            # Most of the house numbers in Montreal start with "#"
            (re.compile('^#', re.UNICODE), six.u('')),
            # Some house number ranges are split up like "12 -14"
            (re.compile('[\s]*\-[\s]*'), six.u('-')),
        ]
    }

    unit_type_regexes = {}

    for (lang, dictionary_type), values in six.iteritems(address_phrase_dictionaries.phrases):
        if dictionary_type == 'unit_types_numbered':
            unit_phrases = [safe_encode(p) for p in itertools.chain(*values) if len(p) > 2]
            pattern = re.compile(r'\b(?:{})\s+(?:#?\s*)(?:[\d]+|[a-z]|[a-z]\-?[\d]+|[\d]+\-?[a-z])\s*$'.format(safe_encode('|').join(unit_phrases)),
                                 re.I | re.UNICODE)
            unit_type_regexes[lang] = pattern

    def __init__(self, components):
        self.components = components
        self.language_rtree = components.language_rtree

        config = yaml.load(open(OPENADDRESSES_PARSER_DATA_CONFIG))
        self.config = config['global']
        self.country_configs = config['countries']

        self.formatter = AddressFormatter()

    class validators:
        @classmethod
        def validate_postcode(cls, postcode):
            '''
            Postcodes that are all zeros are improperly-formatted NULL values
            '''
            return not all((c == '0' for c in postcode))

        @classmethod
        def validate_street(cls, street):
            '''
            Streets should not be simple numbers. If they are it's probably a
            copy/paste error and should be the house number.
            '''
            return not is_numeric(street)

        @classmethod
        def validate_house_number(cls, house_number):
            '''
            House number doesn't necessarily have to be numeric, but in some of the
            OpenAddresses data sets the house number field is equal to the capitalized
            street name, so this at least provides protection against insane values
            for house number at the cost of maybe missing a few houses numbered "A", etc.

            Also OpenAddresses primarily comes from county GIS servers, etc. which use
            a variety of database schemas and don't always handle NULLs very well. Again,
            while a single zero is a valid house number, in OpenAddresses it's more likely
            an error

            While a single zero is a valid house number, more than one zero is not, or
            at least not in OpenAddresses
            '''

            try:
                house_number = int(house_number.strip())
                return house_number > 0
            except (ValueError, TypeError):
                return house_number.strip() and is_numeric(house_number) and not all((c == '0' for c in house_number if c.isdigit()))

        @classmethod
        def validate_house_number_spanish(cls, house_number):
            if sin_numero_regex.match(house_number):
                return True
            return cls.validate_house_number(house_number)

    component_validators = {
        AddressFormatter.HOUSE_NUMBER: validators.validate_house_number,
        AddressFormatter.ROAD: validators.validate_street,
        AddressFormatter.POSTCODE: validators.validate_postcode,
    }

    language_validators = {
        SPANISH: {
            AddressFormatter.HOUSE_NUMBER: validators.validate_house_number_spanish,
        },
    }

    def get_property(self, key, *configs):
        for config in configs:
            value = config.get(key, None)
            if value is not None:
                return value
        return None

    def cldr_country_name(self, country_code, language, configs):
        cldr_country_prob = float(self.get_property('cldr_country_probability', *configs))

        country_name = None

        if random.random() < cldr_country_prob:
            localized, alpha2, alpha3 = values = range(3)
            localized_prob = float(self.get_property('localized_name_probability', *configs))
            alpha2_prob = float(self.get_property('iso_alpha_2_code_probability', *configs))
            alpha3_prob = float(self.get_property('iso_alpha_3_code_probability', *configs))

            probs = cdf([localized_prob, alpha2_prob, alpha3_prob])

            country_type = weighted_choice(values, probs)

            country_name = country_code.upper()
            if country_type == localized:
                country_name = country_names.localized_name(country_code, language) or country_names.localized_name(country_code) or country_name
            elif country_type == alpha3:
                country_name = country_names.alpha3_code(country_code) or country_name

        return country_name

    def cleanup_number(self, num):
        num = num.strip()
        try:
            num_int = int(num)
        except (ValueError, TypeError):
            try:
                num_float = float(num)
                leading_zeros = 0
                for c in num:
                    if c == six.u('0'):
                        leading_zeros += 1
                    else:
                        break
                num = safe_decode(int(num_float))
                if leading_zeros:
                    num = six.u('{}{}').format(six.u('0') * leading_zeros, num)
            except (ValueError, TypeError):
                pass
        return num

    def spanish_street_name(self, street):
        '''
        Most Spanish street names begin with Calle officially
        but since it's so common, this is often omitted entirely.
        As such, for Spanish-speaking places with numbered streets
        like Mérida in Mexico, it would be legitimate to have a
        simple number like "27" for the street name in a GIS
        data set which omits the Calle. However, we don't really
        want to train on "27/road 1/house_number" as that's not
        typically how a numeric-only street would be written. However,
        we don't want to neglect entire cities like Mérida which are
        predominantly a grid, so add Calle (may be abbreviated later).
        '''
        if is_numeric(street):
            street = six.u('Calle {}').format(street)
        return street

    def strip_unit_phrases_for_language(self, value, language):
        if language in self.unit_type_regexes:
            return self.unit_type_regexes[language].sub(six.u(''), value)
        return value

    def formatted_addresses(self, path, configs, tag_components=True):
        abbreviate_street_prob = float(self.get_property('abbreviate_street_probability', *configs))
        separate_street_prob = float(self.get_property('separate_street_probability', *configs) or 0.0)
        abbreviate_unit_prob = float(self.get_property('abbreviate_unit_probability', *configs))
        separate_unit_prob = float(self.get_property('separate_unit_probability', *configs) or 0.0)

        add_osm_boundaries = bool(self.get_property('add_osm_boundaries', *configs) or False)
        add_osm_neighborhoods = bool(self.get_property('add_osm_neighborhoods', *configs) or False)
        non_numeric_units = bool(self.get_property('non_numeric_units', *configs) or False)
        numeric_postcodes_only = bool(self.get_property('numeric_postcodes_only', *configs) or False)
        postcode_strip_non_digit_chars = bool(self.get_property('postcode_strip_non_digit_chars', *configs) or False)

        ignore_fields_containing = {field: re.compile(six.u('|').join([six.u('(?:{})').format(safe_decode(v)) for v in value]), re.I | re.UNICODE)
                                    for field, value in six.iteritems(dict(self.get_property('ignore_fields_containing', *configs) or {}))}

        language = self.get_property('language', *configs)

        add_components = self.get_property('add', *configs)

        fields = self.get_property('fields', *configs)
        if not fields:
            return

        fields = {f['field_name']: f['component'] for f in fields}

        f = open(path)
        reader = unicode_csv_reader(f)
        headers = reader.next()

        header_indices = {i: fields[k] for i, k in enumerate(headers) if k in fields}
        latitude_index = headers.index('LAT')
        longitude_index = headers.index('LON')

        for row in reader:
            try:
                latitude = float(row[latitude_index])
                longitude = float(row[longitude_index])
            except (ValueError, TypeError):
                continue

            components = {}
            for i, key in six.iteritems(header_indices):
                value = row[i].strip()
                if not value:
                    continue

                if key == AddressFormatter.ROAD and language == SPANISH:
                    value = self.spanish_street_name(value)

                if key in AddressFormatter.BOUNDARY_COMPONENTS:
                    value = self.components.cleaned_name(value, first_comma_delimited_phrase=True)
                    if value and len(value) < 2 or is_numeric(value):
                        continue

                if not_applicable_regex.match(value) or null_regex.match(value) or unknown_regex.match(value):
                    continue

                for exp, sub_val in self.field_regex_replacements.get(key, []):
                    value = exp.sub(sub_val, value)

                for exp, sub_val in self.field_regex_replacements.get(None, []):
                    value = exp.sub(sub_val, value)

                value = value.strip(', -')

                validator = self.language_validators.get(language, {}).get(key, self.component_validators.get(key, None))

                if validator is not None and not validator(value):
                    continue

                if key in ignore_fields_containing and ignore_fields_containing[key].search(value):
                    continue

                if value:
                    components[key] = value

            if components:
                country, candidate_languages, language_props = self.language_rtree.country_and_languages(latitude, longitude)
                if not (country and candidate_languages):
                    continue

                if language is None:
                    language = AddressComponents.address_language(components, candidate_languages)

                street = components.get(AddressFormatter.ROAD, None)
                if street is not None:
                    street = street.strip()
                    street = AddressComponents.cleaned_name(street)

                    if language == UNKNOWN_LANGUAGE:
                        strip_unit_language = candidate_languages[0]['lang'] if candidate_languages else None
                    else:
                        strip_unit_language = language

                    self.strip_unit_phrases_for_language(street, strip_unit_language)

                    street = abbreviate(street_types_gazetteer, street, language,
                                        abbreviate_prob=abbreviate_street_prob,
                                        separate_prob=separate_street_prob)
                    components[AddressFormatter.ROAD] = street

                house_number = components.get(AddressFormatter.HOUSE_NUMBER, None)
                if house_number:
                    house_number = self.cleanup_number(house_number)

                postcode = components.get(AddressFormatter.POSTCODE, None)
                if postcode:
                    postcode = self.cleanup_number(postcode)

                    if postcode_strip_non_digit_chars:
                        postcode = six.u('').join((c for c in postcode if c.isdigit()))

                    if postcode and not is_numeric(postcode) and numeric_postcodes_only:
                        components.pop(AddressFormatter.POSTCODE)
                        postcode = None
                    else:
                        components[AddressFormatter.POSTCODE] = postcode

                unit = components.get(AddressFormatter.UNIT, None)

                # If there's a postcode, we can still use just the city/state/postcode, otherwise discard
                if not (street and house_number) or street.lower() == house_number.lower() or (unit and street and street.lower() == unit.lower()):
                    components = self.components.drop_address(components)

                    if not postcode:
                        continue

                # Now that checks, etc. are completed, fetch unit and add phrases, abbreviate, etc.
                unit = components.get(AddressFormatter.UNIT, None)

                if unit is not None:
                    if is_numeric_strict(unit):
                        unit = Unit.phrase(unit, language, country=country)
                    elif non_numeric_units:
                        unit = abbreviate(unit_types_gazetteer, unit, language,
                                          abbreviate_prob=abbreviate_unit_prob,
                                          separate_prob=separate_unit_prob)
                    else:
                        unit = None

                    if unit is not None:
                        components[AddressFormatter.UNIT] = unit
                    else:
                        components.pop(AddressFormatter.UNIT)
                        unit = None

                # CLDR country name
                country_name = self.cldr_country_name(country, language, configs)
                if country_name:
                    components[AddressFormatter.COUNTRY] = country_name

                # Any components specified to be added by the config (usually state)
                if add_components:
                    for k, v in six.iteritems(add_components):
                        if k not in components:
                            components[k] = v

                # Get named states occasionally, added component is usually a state code
                address_state = self.components.state_name(components, country, language)
                if address_state:
                    components[AddressFormatter.STATE] = address_state

                # This is expensive, so only turn on for files that don't supply their own city names
                # or for which those names are flawed
                if add_osm_boundaries or AddressFormatter.CITY not in components:
                    osm_components = self.components.osm_reverse_geocoded_components(latitude, longitude)
                    self.components.add_admin_boundaries(components, osm_components, country, language)

                # The neighborhood index is cheaper so can turn on for whole countries
                if add_osm_neighborhoods:
                    neighborhood_components = self.components.neighborhood_components(latitude, longitude)
                    self.components.add_neighborhoods(components, neighborhood_components)

                formatted = self.formatter.format_address(components, country,
                                                          language=language, tag_components=tag_components)
                yield (language, country, formatted)

    def build_training_data(self, base_dir, out_dir, tag_components=True):
        if tag_components:
            formatted_tagged_file = open(os.path.join(out_dir, OPENADDRESS_FORMAT_DATA_TAGGED_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')
        else:
            formatted_tagged_file = open(os.path.join(out_dir, OPENADDRESS_FORMAT_DATA_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')

        i = 0

        for country, config in six.iteritems(self.country_configs):
            for file_config in config.get('files', []):
                filename = file_config['filename']

                path = os.path.join(base_dir, country, filename)
                configs = (file_config, config, self.config)
                for language, country, formatted_address in self.formatted_addresses(path, configs, tag_components=tag_components):
                    if not formatted_address or not formatted_address.strip():
                        continue

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

            for subdir, subdir_config in six.iteritems(config.get('subdirs', {})):
                for file_config in subdir_config.get('files', []):
                    filename = file_config['filename']

                    path = os.path.join(base_dir, country, subdir, filename)

                    configs = (file_config, subdir_config, config, self.config)
                    for language, country, formatted_address in self.formatted_addresses(path, configs, tag_components=tag_components):
                        if not formatted_address or not formatted_address.strip():
                            continue

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
