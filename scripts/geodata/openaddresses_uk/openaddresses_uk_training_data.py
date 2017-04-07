# -*- coding: utf-8 -*-

import argparse
import logging
import csv
import os
import random
import six
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.address_expansions.abbreviations import abbreviate
from geodata.address_expansions.gazetteers import street_types_gazetteer, toponym_abbreviations_gazetteer
from geodata.address_formatting.formatter import AddressFormatter

from geodata.addresses.components import AddressComponents
from geodata.countries.constants import Countries
from geodata.openaddresses.formatter import *

from geodata.encoding import safe_decode
from geodata.i18n.languages import get_country_languages

from geodata.math.sampling import cdf, weighted_choice

from geodata.places.config import place_config
from geodata.text.utils import is_numeric, is_numeric_strict

from geodata.csv_utils import unicode_csv_reader

OPENADDRESSES_UK_FORMAT_DATA_TAGGED_FILENAME = 'uk_openaddresses_formatted_addresses_tagged.tsv'
OPENADDRESSES_UK_FORMAT_DATA_FILENAME = 'uk_openaddresses_formatted_addresses.tsv'


class OpenAddressesUKFormatter(object):
    field_map = {
        'pao': AddressFormatter.HOUSE_NUMBER,
        'street.name': AddressFormatter.ROAD,
        'town.name': AddressFormatter.CITY,
        'postcode.name': AddressFormatter.POSTCODE
    }

    def __init__(self):
        self.formatter = AddressFormatter()

    component_validators = {
        AddressFormatter.HOUSE_NUMBER: OpenAddressesFormatter.validators.validate_house_number,
        AddressFormatter.ROAD: OpenAddressesFormatter.validators.validate_street,
        AddressFormatter.POSTCODE: OpenAddressesFormatter.validators.validate_postcode,
    }

    cldr_country_probability = 0.3
    address_only_probability = 0.4
    drop_address_probability = 0.6
    drop_address_and_postcode_probability = 0.1

    @classmethod
    def cleanup_number(cls, num, strip_commas=False):
        num = num.strip()
        if strip_commas:
            num = num.replace(six.u(','), six.u(''))
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

    def fix_component_encodings(self, components):
        return {k: ftfy.fix_encoding(safe_decode(v)) for k, v in six.iteritems(components)}

    def formatted_addresses(self, path, tag_components=True):
        country = Countries.UNITED_KINGDOM
        candidate_languages = get_country_languages(country).items()

        f = open(path)
        reader = unicode_csv_reader(f)
        headers = reader.next()

        header_indices = {i: self.field_map[k] for i, k in enumerate(headers) if k in self.field_map}

        for row in reader:
            components = {}

            for i, key in six.iteritems(header_indices):
                value = row[i].strip()
                if not value:
                    continue

                if not_applicable_regex.match(value) or null_regex.match(value) or unknown_regex.match(value):
                    continue

                value = value.strip(', -')

                validator = self.component_validators.get(key, None)

                if validator is not None and not validator(value):
                    continue

                if value:
                    components[key] = value

            if components:
                components = self.fix_component_encodings(components)

                language = AddressComponents.address_language(components, candidate_languages)

                street = components.get(AddressFormatter.ROAD, None)
                if street is not None:
                    street = street.strip()
                    street = AddressComponents.cleaned_name(street)
                    if AddressComponents.street_name_is_valid(street):

                        street = abbreviate(street_types_gazetteer, street, language)
                        components[AddressFormatter.ROAD] = street
                    else:
                        components.pop(AddressFormatter.ROAD)
                        street = None

                house_number = components.get(AddressFormatter.HOUSE_NUMBER, None)
                if house_number:
                    house_number = self.cleanup_number(house_number, strip_commas=True)

                    if house_number is not None:
                        components[AddressFormatter.HOUSE_NUMBER] = house_number

                postcode = components.get(AddressFormatter.POSTCODE, None)

                # If there's a postcode, we can still use just the city/state/postcode, otherwise discard
                if not street or (street and house_number and (street.lower() == house_number.lower())):
                    if not postcode:
                        continue
                    components = AddressComponents.drop_address(components)

                country_name = AddressComponents.cldr_country_name(country, language)
                if country_name:
                    components[AddressFormatter.COUNTRY] = country_name

                for component_key in AddressFormatter.BOUNDARY_COMPONENTS:
                    component = components.get(component_key, None)
                    if component is not None:
                        component = abbreviate(toponym_abbreviations_gazetteer, component, language)
                        component = AddressComponents.name_hyphens(component)
                        components[component_key] = component

                AddressComponents.replace_names(components)

                AddressComponents.prune_duplicate_names(components)

                AddressComponents.remove_numeric_boundary_names(components)
                AddressComponents.add_house_number_phrase(components, language, country=country)

                # Component dropout
                components = place_config.dropout_components(components, country=country)

                formatted = self.formatter.format_address(components, country, language=language,
                                                          minimal_only=False, tag_components=tag_components)
                yield (language, country, formatted)

                if random.random() < self.address_only_probability and street:
                    address_only_components = AddressComponents.drop_places(components)
                    address_only_components = AddressComponents.drop_postcode(address_only_components)
                    formatted = self.formatter.format_address(address_only_components, country, language=language,
                                                              minimal_only=False, tag_components=tag_components)
                    yield (language, country, formatted)

                rand_val = random.random()

                if street and house_number and rand_val < self.drop_address_probability:
                    components = AddressComponents.drop_address(components)

                    if rand_val < self.drop_address_and_postcode_probability:
                        components = AddressComponents.drop_postcode(components)

                    if components and (len(components) > 1):
                        formatted = self.formatter.format_address(components, country, language=language,
                                                                  minimal_only=False, tag_components=tag_components)
                        yield (language, country, formatted)

    def build_training_data(self, infile, out_dir, tag_components=True):
        if tag_components:
            formatted_tagged_file = open(os.path.join(out_dir, OPENADDRESSES_UK_FORMAT_DATA_TAGGED_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')
        else:
            formatted_tagged_file = open(os.path.join(out_dir, OPENADDRESSES_UK_FORMAT_DATA_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')

        i = 0

        for language, country, formatted_address in self.formatted_addresses(infile, tag_components=tag_components):
            if tag_components:
                row = (language, country, formatted_address)
            else:
                row = (formatted_address,)

            writer.writerow(row)
            i += 1
            if i % 1000 == 0 and i > 0:
                print('did {} formatted addresses'.format(i))


if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('sources', nargs='*')

    parser.add_argument('-i', '--openaddresses-uk-file',
                        help='Path to OpenAddresses UK addresses.csv file')

    parser.add_argument('-f', '--format',
                        action='store_true',
                        default=False,
                        help='Save formatted addresses (slow)')

    parser.add_argument('-u', '--untagged',
                        action='store_true',
                        default=False,
                        help='Save untagged formatted addresses (slow)')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    args = parser.parse_args()

    if args.openaddresses_uk_file and args.format:
        oa_formatter = OpenAddressesUKFormatter()
        oa_formatter.build_training_data(args.openaddresses_uk_file, args.out_dir, tag_components=not args.untagged)
