import csv
import os
import random
import six
import yaml

from geodata.addresses.unit import Unit
from geodata.address_expansions.abbreviations import abbreviate
from geodata.address_expansions.gazetteers import street_types_gazetteer, unit_types_gazetteer
from geodata.address_formatting.formatter import AddressFormatter
from geodata.addresses.components import AddressComponents
from geodata.countries.names import country_names
from geodata.encoding import safe_decode
from geodata.math.sampling import cdf, weighted_choice
from geodata.text.utils import is_numeric, is_numeric_strict

from geodata.csv_utils import tsv_string, unicode_csv_reader

this_dir = os.path.realpath(os.path.dirname(__file__))

OPENADDRESSES_PARSER_DATA_CONFIG = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                                'resources', 'parser', 'data_sets', 'openaddresses.yaml')

OPENADDRESS_FORMAT_DATA_TAGGED_FILENAME = 'openaddresses_formatted_addresses_tagged.tsv'
OPENADDRESS_FORMAT_DATA_FILENAME = 'openaddresses_formatted_addresses.tsv'


class OpenAddressesFormatter(object):
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

    component_validators = {
        AddressFormatter.HOUSE_NUMBER: validators.validate_house_number,
        AddressFormatter.ROAD: validators.validate_street,
        AddressFormatter.POSTCODE: validators.validate_postcode,
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

    def formatted_addresses(self, path, configs, tag_components=True):
        abbreviate_street_prob = float(self.get_property('abbreviate_street_probability', *configs))
        separate_street_prob = float(self.get_property('separate_street_probability', *configs) or 0.0)
        abbreviate_unit_prob = float(self.get_property('abbreviate_unit_probability', *configs))
        separate_unit_prob = float(self.get_property('separate_unit_probability', *configs) or 0.0)

        add_osm_boundaries = bool(self.get_property('add_osm_boundaries', *configs) or False)
        add_osm_neighborhoods = bool(self.get_property('add_osm_neighborhoods', *configs) or False)
        non_numeric_units = bool(self.get_property('non_numeric_units', *configs) or False)
        numeric_postcodes_only = bool(self.get_property('numeric_postcodes_only', *configs) or False)

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

                validator = self.component_validators.get(key, None)
                if validator is not None and not validator(value):
                    continue

                if key in AddressFormatter.BOUNDARY_COMPONENTS:
                    value = self.components.cleaned_name(value, first_comma_delimited_phrase=True)

                components[key] = value.strip(', ')

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
                    street = abbreviate(street_types_gazetteer, street, language,
                                        abbreviate_prob=abbreviate_street_prob,
                                        separate_prob=separate_street_prob)
                    components[AddressFormatter.ROAD] = street

                house_number = components.get(AddressFormatter.HOUSE_NUMBER, None)
                if house_number:
                    house_number = house_number.strip()

                if not (street and house_number) or street.lower() == house_number.lower():
                    continue

                unit = components.get(AddressFormatter.UNIT, None)

                if unit and street and street.lower() == unit.lower():
                    continue

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

                postcode = components.get(AddressFormatter.POSTCODE, None)
                if postcode:
                    postcode = postcode.strip()
                    if postcode and not is_numeric(postcode) and numeric_postcodes_only:
                        components.pop(AddressFormatter.POSTCODE)
                    else:
                        components[AddressFormatter.POSTCODE] = postcode

                country_name = self.cldr_country_name(country, language, configs)
                if country_name:
                    components[AddressFormatter.COUNTRY] = country_name

                if add_components:
                    for k, v in six.iteritems(add_components):
                        if k not in components:
                            components[k] = v

                address_state = self.components.state_name(components, country, language)
                if address_state:
                    components[AddressFormatter.STATE] = address_state

                if add_osm_boundaries or AddressFormatter.CITY not in components:
                    osm_components = self.components.osm_reverse_geocoded_components(latitude, longitude)
                    self.components.add_admin_boundaries(components, osm_components, country, language)

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
