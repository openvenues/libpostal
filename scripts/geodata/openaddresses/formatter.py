import csv
import os
import six
import yaml

from geodata.address_expansions.abbreviations import abbreviate
from geodata.address_expansions.gazetteers import street_types_gazetteer, unit_types_gazetteer
from geodata.address_formatting.formatter import AddressFormatter
from geodata.addresses.components import AddressComponents

from geodata.csv_utils import tsv_string, unicode_csv_reader

this_dir = os.path.realpath(os.path.dirname(__file__))

OPENADDRESSES_PARSER_DATA_CONFIG = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                                'resources', 'parser', 'data_sets', 'openaddresses.yaml')

OPENADDRESS_FORMAT_DATA_TAGGED_FILENAME = 'openaddresses_formatted_addresses_tagged.tsv'
OPENADDRESS_FORMAT_DATA_FILENAME = 'openaddresses_formatted_addresses.tsv'


class OpenAddressesFormatter(object):
    def __init__(self, language_rtree):
        self.language_rtree = language_rtree

        config = yaml.load(open(OPENADDRESSES_PARSER_DATA_CONFIG))
        self.config = config['global']
        self.country_configs = config['countries']

        self.formatter = AddressFormatter()

    def get_property(self, key, *configs):
        for config in configs:
            value = config.get(key, None)
            if value is not None:
                return value
        return None

    @staticmethod
    def validate_postcode(postcode):
        return not all((c == '0' for c in postcode))

    openaddresses_validators = {
        AddressFormatter.POSTCODE: validate_postcode
    }

    def formatted_addresses(self, path, configs, tag_components=True):
        abbreviate_street_prob = self.get_property('abbreviate_street_probability', *configs)
        separate_street_prob = self.get_property('separate_street_probability', *configs) or 0.0
        abbreviate_unit_prob = self.get_property('abbreviate_unit_probability', *configs)
        separate_unit_prob = self.get_property('separate_unit_probability', *configs) or 0.0

        field_map = self.get_property('field_map', *configs)
        if not field_map:
            return

        field_map = {f['field_name']: f['component'] for f in field_map}

        f = open(path)
        reader = unicode_csv_reader(f)
        headers = reader.next()

        header_indices = {i: field_map[k] for i, k in enumerate(headers) if k in field_map}
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

                validator = self.openaddresses_validators.get(key, None)
                if validator is not None and not validator(value):
                    continue

                components[key] = value

            if components:
                country, candidate_languages, language_props = self.language_rtree.country_and_languages(latitude, longitude)
                if not (country and candidate_languages):
                    continue

                if not language:
                    language = AddressComponents.address_language(components, candidate_languages)

                street = components.get(AddressFormatter.ROAD, None)
                if street is not None:
                    street = abbreviate(street_types_gazetteer, street, language,
                                        abbreviate_prob=abbreviate_street_prob,
                                        separate_prob=separate_street_prob)
                    components[AddressFormatter.ROAD] = street

                unit = components.get(AddressFormatter.UNIT, None)
                if unit is not None:
                    unit = abbreviate(unit_types_gazetteer, unit, language,
                                      abbreviate_prob=abbreviate_unit_prob,
                                      separate_prob=separate_unit_prob)

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
            for file_props in config.get('files', []):
                filename = file_props['filename']

                path = os.path.join(base_dir, country, filename)
                configs = (file_props, config, self.config)
                for language, country, formatted_address in self.formatted_addresses(path, configs, tag_components=tag_components):
                    if formatted_address and formatted_address.strip():
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
                for file_props in config.get('files', []):
                    filename = file_props['filename']

                    path = os.path.join(base_dir, country, subdir, filename)

                    configs = (file_props, subdir_config, config, self.config)
                    for language, country, formatted_address in self.formatted_addresses(path, configs, tag_components=tag_components):
                        if formatted_address and formatted_address.strip():
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
