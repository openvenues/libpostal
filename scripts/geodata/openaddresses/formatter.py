# -*- coding: utf-8 -*-

import csv
import ftfy
import itertools
import os
import random
import re
import six
import yaml

from geodata.addresses.units import Unit
from geodata.address_expansions.abbreviations import abbreviate
from geodata.address_expansions.address_dictionaries import address_phrase_dictionaries
from geodata.address_expansions.gazetteers import street_types_gazetteer, unit_types_gazetteer, toponym_abbreviations_gazetteer
from geodata.address_formatting.formatter import AddressFormatter
from geodata.addresses.components import AddressComponents
from geodata.countries.constants import Countries
from geodata.countries.names import country_names
from geodata.encoding import safe_decode, safe_encode
from geodata.i18n.languages import get_country_languages
from geodata.i18n.word_breaks import ideographic_scripts
from geodata.language_id.disambiguation import UNKNOWN_LANGUAGE, get_string_script
from geodata.math.sampling import cdf, weighted_choice
from geodata.openaddresses.config import openaddresses_config
from geodata.places.config import place_config
from geodata.postal_codes.phrases import PostalCodes
from geodata.text.tokenize import tokenize
from geodata.text.token_types import token_types
from geodata.text.utils import is_numeric, is_numeric_strict

from geodata.csv_utils import tsv_string, unicode_csv_reader

OPENADDRESSES_FORMAT_DATA_TAGGED_FILENAME = 'openaddresses_formatted_addresses_tagged.tsv'
OPENADDRESSES_FORMAT_DATA_FILENAME = 'openaddresses_formatted_addresses.tsv'

null_regex = re.compile('^\s*(?:null|none)\s*$', re.I)
unknown_regex = re.compile('\bunknown\b', re.I)
not_applicable_regex = re.compile('^\s*n\.?\s*/?\s*a\.?\s*$', re.I)
sin_numero_regex = re.compile('^\s*s\s*/\s*n\s*$', re.I)

russian_number_regex_str = safe_decode(r'(?:№\s*)?(?:(?:[\d]+\w?(?:[\-/](?:(?:[\d]+\w?)|\w))*)|(?:[\d]+\s*\w?)|(?:\b\w\b))')
dom_korpus_stroyeniye_regex = re.compile(safe_decode('(?:(?:дом(?=\s)|д\.?)\s*)?{}(?:(?:\s*,|\s+)\s*(?:(?:корпус(?=\s)|к\.?)\s*){})?(?:(?:\s*,|\s+)\s*(?:(?:строение(?=\s)|с\.?)\s*){})?\s*$').format(russian_number_regex_str, russian_number_regex_str, russian_number_regex_str), re.I | re.U)
uchastok_regex = re.compile(safe_decode('{}\s*(?:,?\s*участок\s+{}\s*)?$').format(russian_number_regex_str, russian_number_regex_str), re.I | re.U)
bea_nomera_regex = re.compile(safe_decode('^\s*б\s*/\s*н\s*$'), re.I)
fraction_regex = re.compile('^\s*[\d]+[\s]*/[\s]*(?:[\d]+|[a-z]|[\d]+[a-z]|[a-z][\d]+)[\s]*$', re.I)
number_space_letter_regex = re.compile('^[\d]+\s+[a-z]$', re.I)
number_slash_number_regex = re.compile('^(?:[\d]+|[a-z]|[\d]+[a-z]|[a-z][\d]+)[\s]*/[\s]*(?:[\d]+|[a-z]|[\d]+[a-z]|[a-z][\d]+)$', re.I)
number_fraction_regex = re.compile('^(?:[\d]+\s+)?(?:1[\s]*/[\s]*[234]|2[\s]*/[\s]*3)$')

colombian_standard_house_number_regex = re.compile('^(\d+[\s]*[a-z]?)\s+([a-z]?[\d]+[\s]*[a-z]?)?', re.I)

dutch_house_number_regex = re.compile('([\d]+)( [a-z])?( [\d]+)?', re.I)

SPANISH = 'es'
PORTUGUESE = 'pt'
RUSSIAN = 'ru'
CHINESE = 'zh'


class OpenAddressesFormatter(object):
    field_regex_replacements = {
        # All fields
        None: [
            (re.compile('<\s*null\s*>', re.I), u''),
            (re.compile('[\s]{2,}'), six.u(' ')),
            (re.compile('\`'), u"'"),
            (re.compile('\-?\*'), u""),
        ],
        AddressFormatter.HOUSE_NUMBER: [
            # Most of the house numbers in Montreal start with "#"
            (re.compile('^#', re.UNICODE), u''),
            # Some house numbers have multiple hyphens
            (re.compile('[\-]{2,}'), u'-'),
            # Some house number ranges are split up like "12 -14"
            (re.compile('[\s]*\-[\s]*'), u'-'),
        ]
    }

    unit_type_regexes = {}

    for (lang, dictionary_type), values in six.iteritems(address_phrase_dictionaries.phrases):
        if dictionary_type == 'unit_types_numbered':
            unit_phrases = [safe_encode(p) for p in itertools.chain(*values) if len(p) > 2]
            pattern = re.compile(r'\b(?:{})\s+(?:#?\s*)(?:[\d]+|[a-z]|[a-z]\-?[\d]+|[\d]+\-?[a-z])\s*$'.format(safe_encode('|').join(unit_phrases)),
                                 re.I | re.UNICODE)
            unit_type_regexes[lang] = pattern

    def __init__(self, components, country_rtree, debug=False):
        self.components = components
        self.country_rtree = country_rtree

        self.debug = debug

        self.formatter = AddressFormatter()

    class validators:
        @classmethod
        def validate_postcode(cls, postcode):
            '''
            Postcodes that are all zeros are improperly-formatted NULL values
            '''
            return not all((c in ('0', '-', '.', ' ', ',') for c in postcode))

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
                house_number = house_number.strip()
                return house_number and (is_numeric(house_number) or fraction_regex.match(house_number) or number_space_letter_regex.match(house_number) or
                                         number_slash_number_regex.match(house_number) or number_fraction_regex.match(house_number)) and not all((c == '0' for c in house_number if c.isdigit()))

        @classmethod
        def validate_house_number_sin_numero(cls, house_number):
            if sin_numero_regex.match(house_number):
                return True
            return cls.validate_house_number(house_number)

        @classmethod
        def validate_russian_house_number(cls, house_number):
            if dom_korpus_stroyeniye_regex.match(house_number):
                return True
            elif uchastok_regex.match(house_number):
                return True
            elif bea_nomera_regex.match(house_number):
                return True
            return cls.validate_house_number(house_number)

        @classmethod
        def validate_colombian_house_number(cls, house_number):
            return True

        @classmethod
        def validate_chinese_house_number(cls, house_number):
            if not house_number:
                return False
            tokens = tokenize(house_number)

            if all((c in token_types.NUMERIC_TOKEN_TYPES or t in (u'号', u'栋', u'附')) for t, c in tokens):
                return True
            return cls.validate_house_number(house_number)

    component_validators = {
        AddressFormatter.HOUSE_NUMBER: validators.validate_house_number,
        AddressFormatter.ROAD: validators.validate_street,
        AddressFormatter.POSTCODE: validators.validate_postcode,
    }

    language_validators = {
        SPANISH: {
            AddressFormatter.HOUSE_NUMBER: validators.validate_house_number_sin_numero,
        },
        PORTUGUESE: {
            AddressFormatter.HOUSE_NUMBER: validators.validate_house_number_sin_numero,
        },
        RUSSIAN: {
            AddressFormatter.HOUSE_NUMBER: validators.validate_russian_house_number,
        },
        CHINESE: {
            AddressFormatter.HOUSE_NUMBER: validators.validate_chinese_house_number,
        }
    }

    country_validators = {
        Countries.COLOMBIA: {
            AddressFormatter.HOUSE_NUMBER: validators.validate_colombian_house_number
        }
    }

    chinese_annex_regex = re.compile(u'([\d]+)(?![\d号栋])', re.U)

    @classmethod
    def format_chinese_house_number(cls, house_number):
        if not house_number:
            return house_number
        return cls.chinese_annex_regex.sub(u'\\1号', house_number)

    @classmethod
    def format_colombian_house_number(cls, house_number):
        house_number = house_number.strip()
        match = colombian_standard_house_number_regex.match(house_number)
        if match:
            separator = random.choice((u'-', u' - ', u' '))

            cross_street, building_number = match.groups()

            numbers = []
            if cross_street and u' ' in cross_street and random.choice((True, False)):
                cross_street = cross_street.replace(u' ', u'')

            if cross_street:
                numbers.append(cross_street)

            if building_number and u' ' in building_number and random.choice((True, False)):
                building_number = building_number.replace(u' ', u'')

            if building_number:
                numbers.append(building_number)

            if numbers:
                house_number = separator.join(numbers)
                house_number_prefixes = (u'#', u'no.', u'no', u'nº')
                if random.choice((True, False)) and not any((house_number.lower().startswith(p) for p in house_number_prefixes)):
                    house_number = u' '.join([random.choice(house_number_prefixes), house_number])

        return house_number

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
            localized, iso_3166, alpha2, alpha3 = values = range(4)
            localized_prob = float(self.get_property('localized_name_probability', *configs))
            iso_3166_prob = float(self.get_property('iso_3166_name_probability', *configs))
            alpha2_prob = float(self.get_property('iso_alpha_2_code_probability', *configs))
            alpha3_prob = float(self.get_property('iso_alpha_3_code_probability', *configs))

            probs = cdf([localized_prob, iso_3166_prob, alpha2_prob, alpha3_prob])

            country_type = weighted_choice(values, probs)

            country_name = country_code.upper()
            if country_type == localized:
                country_name = country_names.localized_name(country_code, language) or country_names.localized_name(country_code) or country_name
            elif country_type == iso_3166:
                country_name = country_names.iso3166_name(country_code)
            elif country_type == alpha3:
                country_name = country_names.alpha3_code(country_code) or country_name

        return country_name

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

    @classmethod
    def fix_component_encodings(cls, components):
        return {k: ftfy.fix_encoding(safe_decode(v)) for k, v in six.iteritems(components)}

    def formatted_addresses(self, country_dir, path, configs, tag_components=True):
        abbreviate_street_prob = float(self.get_property('abbreviate_street_probability', *configs))
        separate_street_prob = float(self.get_property('separate_street_probability', *configs) or 0.0)
        abbreviate_unit_prob = float(self.get_property('abbreviate_unit_probability', *configs))
        separate_unit_prob = float(self.get_property('separate_unit_probability', *configs) or 0.0)
        abbreviate_toponym_prob = float(self.get_property('abbreviate_toponym_probability', *configs))

        add_osm_boundaries = bool(self.get_property('add_osm_boundaries', *configs) or False)
        add_osm_neighborhoods = bool(self.get_property('add_osm_neighborhoods', *configs) or False)
        osm_neighborhood_overrides_city = self.get_property('osm_neighborhood_overrides_city', *configs)
        non_numeric_units = bool(self.get_property('non_numeric_units', *configs) or False)
        house_number_strip_commas = bool(self.get_property('house_number_strip_commas', *configs) or False)
        numeric_postcodes_only = bool(self.get_property('numeric_postcodes_only', *configs) or False)
        postcode_strip_non_digit_chars = bool(self.get_property('postcode_strip_non_digit_chars', *configs) or False)

        address_only_probability = float(self.get_property('address_only_probability', *configs))
        place_only_probability = float(self.get_property('place_only_probability', *configs))
        place_and_postcode_probability = float(self.get_property('place_and_postcode_probability', *configs))

        city_replacements = self.get_property('city_replacements', *configs)

        override_country_dir = self.get_property('override_country_dir', *configs)

        postcode_length = int(self.get_property('postcode_length', *configs) or 0)

        drop_address_probability = place_only_probability + place_and_postcode_probability

        ignore_rows_missing_fields = set(self.get_property('ignore_rows_missing_fields', *configs) or [])

        ignore_fields_containing = {field: re.compile(six.u('|').join([six.u('(?:{})').format(safe_decode(v)) for v in value]), re.I | re.UNICODE)
                                    for field, value in six.iteritems(dict(self.get_property('ignore_fields_containing', *configs) or {}))}

        alias_fields_containing = {field: [(re.compile(v['pattern'], re.I | re.UNICODE), v) for v in value]
                                   for field, value in six.iteritems(dict(self.get_property('alias_fields_containing', *configs) or {}))}

        config_language = self.get_property('language', *configs)

        add_components = self.get_property('add', *configs)

        fields = self.get_property('fields', *configs)
        if not fields:
            return

        field_map = {field_name: f['component'] for field_name, f in six.iteritems(fields)}
        mapped_values = {f['component']: f['value_map'] for f in six.itervalues(fields) if hasattr(f.get('value_map'), 'get')}

        f = open(path)
        reader = unicode_csv_reader(f)
        headers = reader.next()

        header_indices = {i: field_map[k] for i, k in enumerate(headers) if k in field_map}
        latitude_index = headers.index('LAT')
        longitude_index = headers.index('LON')

        # Clear cached polygons
        self.components.osm_admin_rtree.clear_cache()
        self.components.neighborhoods_rtree.clear_cache()

        for row in reader:
            try:
                latitude = float(row[latitude_index])
                longitude = float(row[longitude_index])
            except (ValueError, TypeError):
                continue

            language = config_language

            components = {}

            skip_record = False

            for i, key in six.iteritems(header_indices):
                value = row[i].strip()
                if not value and key in ignore_rows_missing_fields:
                    skip_record = True
                    break
                elif not value:
                    continue

                if key in mapped_values:
                    value = mapped_values[key].get(value, value)

                if key == AddressFormatter.ROAD and language == SPANISH:
                    value = self.components.spanish_street_name(value)

                if key == AddressFormatter.POSTCODE:
                    value = self.cleanup_number(value)

                    if postcode_strip_non_digit_chars:
                        value = six.u('').join((c for c in value if c.isdigit()))

                    if value and not is_numeric(value) and numeric_postcodes_only:
                        continue
                    else:
                        if postcode_length:
                            value = value.zfill(postcode_length)[:postcode_length]

                if key in AddressFormatter.BOUNDARY_COMPONENTS and key != AddressFormatter.POSTCODE:
                    if add_osm_boundaries:
                        continue
                    value = self.components.cleaned_name(value, first_comma_delimited_phrase=True)
                    if value and ((len(value) < 2 and not get_string_script(value)[0].lower() in ideographic_scripts) or is_numeric(value)):
                        continue

                if not_applicable_regex.match(value) or null_regex.match(value) or unknown_regex.match(value):
                    continue

                for exp, sub_val in self.field_regex_replacements.get(key, []):
                    value = exp.sub(sub_val, value)

                for exp, sub_val in self.field_regex_replacements.get(None, []):
                    value = exp.sub(sub_val, value)

                value = value.strip(', -')

                validator = self.country_validators.get(country_dir, {}).get(key, self.language_validators.get(language, {}).get(key, self.component_validators.get(key, None)))

                if validator is not None and not validator(value):
                    continue

                if key in ignore_fields_containing and ignore_fields_containing[key].search(value):
                    continue

                for (pattern, alias) in alias_fields_containing.get(key, []):
                    if pattern.search(value):
                        if 'component' in alias:
                            key = alias['component']

                if value:
                    components[key] = value

            if skip_record:
                continue

            if components:
                country, candidate_languages = self.country_rtree.country_and_languages(latitude, longitude)
                if not (country and candidate_languages) or (country != country_dir and not override_country_dir):
                    country = country_dir
                    candidate_languages = get_country_languages(country)
                    if not candidate_languages:
                        continue
                    candidate_languages = candidate_languages.items()

                components = self.fix_component_encodings(components)

                if language is None:
                    language = AddressComponents.address_language(components, candidate_languages)

                street = components.get(AddressFormatter.ROAD, None)
                if street is not None:
                    street = street.strip()
                    street = AddressComponents.cleaned_name(street)

                    if language == UNKNOWN_LANGUAGE:
                        strip_unit_language = candidate_languages[0][0] if candidate_languages else None
                    else:
                        strip_unit_language = language

                    street = self.components.strip_unit_phrases_for_language(street, strip_unit_language)

                    street = abbreviate(street_types_gazetteer, street, language,
                                        abbreviate_prob=abbreviate_street_prob,
                                        separate_prob=separate_street_prob)
                    components[AddressFormatter.ROAD] = street

                house_number = components.get(AddressFormatter.HOUSE_NUMBER, None)
                if house_number:
                    house_number = self.cleanup_number(house_number, strip_commas=house_number_strip_commas)

                    if language == CHINESE:
                        house_number = self.format_chinese_house_number(house_number)

                    if country_dir == Countries.COLOMBIA:
                        house_number = self.format_colombian_house_number(house_number)

                    if house_number is not None:
                        components[AddressFormatter.HOUSE_NUMBER] = house_number

                unit = components.get(AddressFormatter.UNIT, None)

                street_required = country not in (Countries.JAPAN, Countries.CZECH_REPUBLIC) and country not in Countries.FORMER_SOVIET_UNION_COUNTRIES

                postcode = components.get(AddressFormatter.POSTCODE, None)

                if postcode:
                    components[AddressFormatter.POSTCODE] = PostalCodes.add_country_code(postcode, country)

                # If there's a postcode, we can still use just the city/state/postcode, otherwise discard
                if (not street and street_required) or (street and house_number and (street.lower() == house_number.lower())) or (unit and street and street.lower() == unit.lower()):
                    if not postcode:
                        continue
                    components = self.components.drop_address(components)

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

                for component_key in AddressFormatter.BOUNDARY_COMPONENTS:
                    component = components.get(component_key, None)
                    if component is not None:
                        component = abbreviate(toponym_abbreviations_gazetteer, component, language,
                                               abbreviate_prob=abbreviate_toponym_prob)
                        component = self.components.name_hyphens(component)
                        components[component_key] = component

                # Any components specified to be added by the config (usually state)
                if add_components:
                    for k, v in six.iteritems(add_components):
                        if k not in components:
                            components[k] = v

                # Get named states occasionally, added component is usually a state code
                address_state = self.components.state_name(components, country, language)
                if address_state:
                    components[AddressFormatter.STATE] = address_state

                state = components.get(AddressFormatter.STATE)
                if state:
                    state = self.components.abbreviated_state(state, country, language)
                    if state:
                        components[AddressFormatter.STATE] = state

                # This is expensive, so only turn on for files that don't supply their own city names
                # or for which those names are flawed
                osm_components = []

                # Using population=0 instead of None means if there's no known population or
                # we don't need to add OSM components, we assume the population of the town is
                # very small and the place name shouldn't be used unqualified (i.e. needs information
                # like state name to disambiguate it)
                population = 0
                unambiguous_city = False
                if add_osm_boundaries or AddressFormatter.CITY not in components:
                    osm_components = self.components.osm_reverse_geocoded_components(latitude, longitude)
                    self.components.add_admin_boundaries(components, osm_components, country, language, latitude, longitude)
                    categorized = self.components.categorized_osm_components(country, osm_components)
                    for component, label in categorized:
                        if label == AddressFormatter.CITY:
                            unambiguous_city = self.components.unambiguous_wikipedia(component, language)
                            if 'population' in component:
                                population = component['population']
                            break

                if AddressFormatter.CITY not in components and city_replacements:
                    components.update({k: v for k, v in six.iteritems(city_replacements) if k not in components})

                # The neighborhood index is cheaper so can turn on for whole countries
                neighborhood_components = []
                if add_osm_neighborhoods:
                    neighborhood_components = self.components.neighborhood_components(latitude, longitude)
                    self.components.add_neighborhoods(components, neighborhood_components, country, language, replace_city=osm_neighborhood_overrides_city)

                self.components.cleanup_boundary_names(components)
                self.components.country_specific_cleanup(components, country)

                self.components.replace_name_affixes(components, language, country=country)

                self.components.replace_names(components)

                self.components.prune_duplicate_names(components)

                self.components.remove_numeric_boundary_names(components)
                self.components.add_house_number_phrase(components, language, country=country)
                self.components.add_postcode_phrase(components, language, country=country)

                # Component dropout
                all_osm_components = osm_components + neighborhood_components
                components = place_config.dropout_components(components, all_osm_components, country=country, population=population, unambiguous_city=unambiguous_city)

                self.components.add_genitives(components, language)

                formatted = self.formatter.format_address(components, country, language=language,
                                                          minimal_only=False, tag_components=tag_components)
                yield (language, country, formatted)

                if random.random() < address_only_probability and street:
                    address_only_components = self.components.drop_places(components)
                    address_only_components = self.components.drop_postcode(address_only_components)
                    formatted = self.formatter.format_address(address_only_components, country, language=language,
                                                              minimal_only=False, tag_components=tag_components)
                    yield (language, country, formatted)

                rand_val = random.random()

                if street and house_number and rand_val < drop_address_probability:
                    components = self.components.drop_address(components)

                    if rand_val < place_and_postcode_probability:
                        components = self.components.drop_postcode(components)

                    if components and (len(components) > 1 or add_osm_boundaries):
                        formatted = self.formatter.format_address(components, country, language=language,
                                                                  minimal_only=False, tag_components=tag_components)
                        yield (language, country, formatted)

    def build_training_data(self, base_dir, out_dir, tag_components=True, sources_only=None):
        all_sources_valid = sources_only is None
        valid_sources = set()
        if not all_sources_valid:
            for source in sources_only:
                if source.startswith(base_dir):
                    source = os.path.relpath(source, base_dir)

                parts = source.strip('/ ').split('/')
                if len(parts) > 3:
                    raise AssertionError('Sources may only have at maximum 3 parts')
                valid_sources.add(tuple(parts))

        if tag_components:
            formatted_tagged_file = open(os.path.join(out_dir, OPENADDRESSES_FORMAT_DATA_TAGGED_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')
        else:
            formatted_tagged_file = open(os.path.join(out_dir, OPENADDRESSES_FORMAT_DATA_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')

        i = 0

        for country_dir in sorted(openaddresses_config.country_configs.keys()):
            country_config = openaddresses_config.country_configs[country_dir]
            # Clear country cache for each new country
            self.country_rtree.clear_cache()

            for file_config in country_config.get('files', []):
                filename = file_config['filename']

                if not all_sources_valid and not ((country_dir, filename) in valid_sources or (country_dir,) in valid_sources):
                    continue

                print(six.u('doing {}/{}').format(country_dir, filename))

                path = os.path.join(base_dir, country_dir, filename)
                configs = (file_config, country_config, openaddresses_config.config)
                for language, country, formatted_address in self.formatted_addresses(country_dir, path, configs, tag_components=tag_components):
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
                        if self.debug:
                            break

            for subdir in sorted(country_config.get('subdirs', {}).keys()):
                subdir_config = country_config['subdirs'][subdir]
                subdir = safe_decode(subdir)
                for file_config in subdir_config.get('files', []):
                    filename = file_config['filename']

                    if not all_sources_valid and not ((country_dir, subdir, filename) in valid_sources or (country_dir, subdir) in valid_sources or (country_dir,) in valid_sources):
                        continue

                    print(six.u('doing {}/{}/{}').format(country_dir, subdir, filename))

                    path = os.path.join(base_dir, country_dir, subdir, filename)

                    configs = (file_config, subdir_config, country_config, openaddresses_config.config)
                    for language, country, formatted_address in self.formatted_addresses(country_dir, path, configs, tag_components=tag_components):
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
                            if self.debug:
                                break
