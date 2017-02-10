import argparse
import csv
import itertools
import os
import six
import sqlite3
import sys

from collections import defaultdict

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.address_expansions.abbreviations import abbreviate
from geodata.address_expansions.equivalence import equivalent
from geodata.address_expansions.gazetteers import *

from geodata.address_formatting.formatter import AddressFormatter

from geodata.countries.names import country_names
from geodata.postal_codes.validation import postcode_regexes
from geodata.names.normalization import name_affixes
from geodata.places.config import place_config

from geodata.csv_utils import tsv_string, unicode_csv_reader

GEOPLANET_DB_FILE = 'geoplanet.db'
GEOPLANET_FORMAT_DATA_TAGGED_FILENAME = 'geoplanet_formatted_addresses_tagged.tsv'
GEOPLANET_FORMAT_DATA_FILENAME = 'geoplanet_formatted_addresses.tsv'


class GeoPlanetFormatter(object):
    # Map of GeoPlanet language codes to ISO-639 alpha2 language codes
    language_codes = {
        'ENG': 'en',
        'JPN': 'ja',
        'GER': 'de',
        'SPA': 'es',
        'FRE': 'fr',
        'UNK': 'unk',
        'ITA': 'it',
        'POR': 'pt',
        'POL': 'pl',
        'ARA': 'ar',
        'CZE': 'cs',
        'SWE': 'sv',
        'CHI': 'zh',
        'RUM': 'ro',
        'FIN': 'fi',
        'DUT': 'nl',
        'NOR': 'nb',
        'DAN': 'da',
        'HUN': 'hu',
        'KOR': 'kr',
    }

    non_latin_script_languages = {
        'JPN',  # Japanese
        'ARA',  # Arabic
        'CHI',  # Chinese
        'KOR',  # Korean
    }

    ALIAS_PREFERRED = 'P'
    ALIAS_PREFERRED_FOREIGN = 'Q'
    ALIAS_VARIANT = 'V'
    ALIAS_ABBREVIATED = 'A'
    ALIAS_COLLOQUIAL = 'S'

    # Map of GeoPlanet place types to address formatter types
    place_types = {
        'Continent': AddressFormatter.WORLD_REGION,
        'Country': AddressFormatter.COUNTRY,
        'CountryRegion': AddressFormatter.COUNTRY_REGION,
        'State': AddressFormatter.STATE,
        'County': AddressFormatter.STATE_DISTRICT,
        'Island': AddressFormatter.ISLAND,
        'Town': AddressFormatter.CITY,
        # Note: if we do general place queris from GeoPlanet, this
        # may have to be mapped more carefully
        'LocalAdmin': AddressFormatter.CITY_DISTRICT,
        'Suburb': AddressFormatter.SUBURB,
    }

    def __init__(self, geoplanet_db):
        self.db = sqlite3.connect(geoplanet_db)

        # These aren't too large and it's easier to have them in memory
        self.places = {row[0]: row[1:] for row in self.db.execute('select * from places')}
        self.aliases = defaultdict(list)

        self.coterminous_admins = {}
        self.admins_with_ambiguous_city = set()

        print('Doing admin ambiguities')
        for row in self.db.execute('''select p.id,
                                             (select count(*) from places where parent_id = p.id) as num_places,
                                             (select count(*) from places where parent_id = p.id and place_type = "Town") as num_towns,
                                             p2.id
                                      from places p
                                      join places p2
                                          on p2.parent_id = p.id
                                          and p.name = p2.name
                                          and p.place_type != "Town"
                                          and p2.place_type = "Town"
                                      group by p.id'''):
            place_id, num_places, num_towns, coterminous_town_id = row
            num_places = int(num_places)
            num_towns = int(num_towns)

            if num_places == 1 and num_towns == 1:
                self.coterminous_admins[place_id] = coterminous_town_id
            self.admins_with_ambiguous_city.add(place_id)

        print('num coterminous: {}'.format(len(self.coterminous_admins)))
        print('num ambiguous: {}'.format(len(self.admins_with_ambiguous_city)))

        print('Doing aliases')
        for row in self.db.execute('''select a.* from aliases a
                                      left join places p
                                          on a.id = p.id
                                          and p.place_type in ("State", "County")
                                          and a.language != p.language
                                      where name_type != "S" -- no colloquial aliases like "The Big Apple"
                                      and name_type != "V" -- variants can often be demonyms like "Welsh" or "English" for UK
                                      and p.id is NULL -- exclude foreign-language states/county names
                                      order by id, language,
                                      case name_type
                                          when "P" then 1
                                          when "Q" then 2
                                          when "V" then 3
                                          when "A" then 4
                                          when "S" then 5
                                          else 6
                                      end'''):
            place = self.places.get(row[0])
            if not place:
                continue

            self.aliases[row[0]].append(row[1:])

        print('Doing variant aliases')
        variant_aliases = 0
        for i, row in enumerate(self.db.execute('''select a.*, p.name, p.country_code from aliases a
                                                   join places p using(id)
                                                   where a.name_type = "V"
                                                   and a.language = p.language''')):
            place_name, country_code = row[-2:]
            country = country_code.lower()

            row = row[:-2]
            place_id, alias, name_type, language = row

            language = self.language_codes[language]
            if language != 'unk':
                alias_sans_affixes = name_affixes.replace_affixes(alias, language, country=country)
                if alias_sans_affixes:
                    alias = alias_sans_affixes

                place_name_sans_affixes = name_affixes.replace_affixes(place_name, language, country=country)
                if place_name_sans_affixes:
                    place_name = place_name_sans_affixes
            else:
                language = None

            if equivalent(place_name, alias, toponym_abbreviations_gazetteer, language):
                self.aliases[row[0]].append(row[1:])
                variant_aliases += 1

            if i % 10000 == 0 and i > 0:
                print('tested {} variant aliases with {} positives'.format(i, variant_aliases))

        self.aliases = dict(self.aliases)

        self.formatter = AddressFormatter()

    def get_place_hierarchy(self, place_id):
        all_places = []
        original_place_id = place_id
        place = self.places[place_id]
        all_places.append((place_id, ) + place)
        place_id = place[-1]
        while place_id != 1 and place_id != original_place_id:
            place = self.places[place_id]
            all_places.append((place_id,) + place)
            place_id = place[-1]
        return all_places

    def get_aliases(self, place_id):
        return self.aliases.get(place_id, [])

    def cleanup_name(self, name):
        return name.strip(' ,-')

    def format_postal_codes(self, tag_components=True):
        all_postal_codes = self.db.execute('select * from postal_codes')
        for postal_code_id, country, postal_code, language, place_type, parent_id in all_postal_codes:
            country = country.lower()
            postcode_language = language

            language = self.language_codes[language]

            if len(postal_code) <= 3:
                postcode_regex = postcode_regexes.get(country)

                valid_postcode = False
                if postcode_regex:
                    match = postcode_regex.match(postal_code)
                    if match and match.end() == len(postal_code):
                        valid_postcode = True

                if not valid_postcode:
                    continue

            # If the county/state is coterminous with a city and contains only one place,
            # set the parent_id to the city instead
            if parent_id in self.coterminous_admins:
                parent_id = self.coterminous_admins[parent_id]

            place_hierarchy = self.get_place_hierarchy(parent_id)

            containing_places = defaultdict(set)

            language_places = {None: containing_places}

            original_language = language

            have_default_language = False

            if place_hierarchy:
                base_place_id, _, _, _, base_place_type, _ = place_hierarchy[0]
                base_place_type = self.place_types[base_place_type]
            else:
                base_place_id = None
                base_place_type = None

            place_types_seen = set()

            for place_id, country, name, lang, place_type, parent in place_hierarchy:
                country = country.lower()

                # First language
                if not have_default_language and lang != postcode_language:
                    language = self.language_codes[lang]
                    have_default_language = True

                place_type = self.place_types[place_type]
                if AddressFormatter.CITY not in place_types_seen and place_id in self.admins_with_ambiguous_city:
                    continue

                name = self.cleanup_name(name)
                containing_places[place_type].add(name)

                aliases = self.get_aliases(place_id)
                for name, name_type, alias_lang in aliases:
                    if not alias_lang:
                        alias_lang = 'UNK'
                    if alias_lang == lang and lang != 'UNK':
                        alias_language = None
                    else:
                        alias_language = self.language_codes[alias_lang]

                    language_places.setdefault(alias_language, defaultdict(set))
                    lang_places = language_places[alias_language]

                    name = self.cleanup_name(name)

                    lang_places[place_type].add(name)

                place_types_seen.add(place_type)

            default_city_names = set([name.lower() for name in language_places.get(None, {}).get(AddressFormatter.CITY, [])])

            for language, containing_places in six.iteritems(language_places):
                if language is None:
                    language = original_language

                country_localized_name = country_names.localized_name(country, language)
                if country_localized_name:
                    containing_places[AddressFormatter.COUNTRY].add(country_localized_name)
                country_alpha3_code = country_names.alpha3_code(country)
                if country_alpha3_code and language in (None, 'ENG'):
                    containing_places[AddressFormatter.COUNTRY].add(country_alpha3_code)

                keys = containing_places.keys()
                all_values = containing_places.values()

                keys_set = set(keys)

                for i, values in enumerate(itertools.product(*all_values)):
                    components = {
                        AddressFormatter.POSTCODE: postal_code
                    }

                    if not default_city_names:
                        components.update(zip(keys, values))
                    else:
                        for k, v in zip(keys, values):
                            if k == AddressFormatter.CITY or AddressFormatter.CITY in keys_set or v.lower() not in default_city_names:
                                components[k] = v

                    format_language = language if self.formatter.template_language_matters(country, language) else None
                    formatted = self.formatter.format_address(components, country, language=format_language,
                                                              minimal_only=False, tag_components=tag_components)

                    yield (language, country, formatted)

                    component_keys = set(components)
                    components = place_config.dropout_components(components, (), country=country, population=0)

                    if len(components) > 1 and set(components) ^ component_keys:
                        formatted = self.formatter.format_address(components, country, language=format_language,
                                                                  minimal_only=False, tag_components=tag_components)
                        yield (language, country, formatted)

    def build_training_data(self, out_dir, tag_components=True):
        if tag_components:
            formatted_tagged_file = open(os.path.join(out_dir, GEOPLANET_FORMAT_DATA_TAGGED_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')
        else:
            formatted_tagged_file = open(os.path.join(out_dir, GEOPLANET_FORMAT_DATA_FILENAME), 'w')
            writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')

        i = 0

        for language, country, formatted_address in self.format_postal_codes(tag_components=tag_components):
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


if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit('Usage: python geoplanet_training_data.py geoplanet_db_path out_dir')

    geoplanet_db_path = sys.argv[1]
    out_dir = sys.argv[2]

    geoplanet = GeoPlanetFormatter(geoplanet_db_path)
    geoplanet.build_training_data(out_dir)
