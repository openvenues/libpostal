import os
import pycountry
import random
import six
import yaml

from collections import defaultdict

from geodata.address_formatting.formatter import AddressFormatter
from geodata.address_formatting.aliases import Aliases

from geodata.addresses.floors import Floor
from geodata.addresses.units import Unit
from geodata.configs.utils import nested_get
from geodata.coordinates.conversion import latlon_to_decimal
from geodata.countries.country_names import *
from geodata.language_id.disambiguation import *
from geodata.language_id.sample import sample_random_language
from geodata.math.sampling import cdf, weighted_choice
from geodata.names.normalization import name_affixes
from geodata.boundaries.names import boundary_names
from geodata.osm.components import osm_address_components
from geodata.states.state_abbreviations import state_abbreviations

this_dir = os.path.realpath(os.path.dirname(__file__))

PARSER_DEFAULT_CONFIG = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                     'resources', 'parser', 'default.yaml')


class AddressExpander(object):
    '''
    This class, while it has a few dependencies, exposes a simple method
    for transforming geocoded input addresses (usually a lat/lon with either
    a name or house number + street name) into the sorts of examples used by
    libpostal's address parser. The dictionaries produced here can be fed
    directly to AddressFormatter.format_address to produce training examples.

    There are several steps in expanding an address including reverse geocoding
    to polygons, disambiguating which language the address uses, stripping standard
    prefixes like "London Borough of", pruning duplicates like "Antwerpen, Antwerpen, Antwerpen".

    Usage:
    >>> expander = AddressExpander(osm_admin_rtree, language_rtree, neighborhoods_rtree, buildings_rtree, subdivisions_rtree, quattroshapes_rtree, geonames)
    >>> expander.expanded_address_components({'name': 'Hackney Empire'}, 51.54559, -0.05567)

    Returns (results vary because of randomness):

    ({'city': u'London',
      'city_district': u'London Borough of Hackney',
      'country': 'UK',
      'name': 'Hackney Empire',
      'state': u'England',
      'state_district': u'Greater London'},
     u'gb',
     u'en')

    '''
    iso_alpha2_codes = set([c.alpha2.lower() for c in pycountry.countries])
    iso_alpha3_codes = set([c.alpha3.lower() for c in pycountry.countries])

    rare_components = {
        AddressFormatter.SUBURB,
        AddressFormatter.CITY_DISTRICT,
        AddressFormatter.ISLAND,
        AddressFormatter.STATE_DISTRICT,
        AddressFormatter.STATE,
    }

    BOUNDARY_COMPONENTS = (
        AddressFormatter.SUBURB,
        AddressFormatter.CITY_DISTRICT,
        AddressFormatter.CITY,
        AddressFormatter.ISLAND,
        AddressFormatter.STATE_DISTRICT,
        AddressFormatter.STATE
    )

    ALL_OSM_NAME_KEYS = set(['name', 'name:simple',
                             'ISO3166-1:alpha2', 'ISO3166-1:alpha3',
                             'short_name', 'alt_name', 'official_name'])

    def __init__(self, osm_admin_rtree, language_rtree, neighborhoods_rtree, buildings_rtree, subdivisions_rtree, quattroshapes_rtree, geonames):
        self.config = yaml.load(open(PARSER_DEFAULT_CONFIG))

        self.osm_admin_rtree = osm_admin_rtree
        self.language_rtree = language_rtree
        self.neighborhoods_rtree = neighborhoods_rtree
        self.subdivisions_rtree = subdivisions_rtree
        self.buildings_rtree = buildings_rtree
        self.quattroshapes_rtree = quattroshapes_rtree
        self.geonames = geonames

    def strip_keys(self, value, ignore_keys):
        for key in ignore_keys:
            value.pop(key, None)

    def osm_reverse_geocoded_components(self, latitude, longitude):
        return self.osm_admin_rtree.point_in_poly(latitude, longitude, return_all=True)

    def categorized_osm_components(self, country, osm_components):
        components = defaultdict(list)
        for props in osm_components:
            name = props.get('name')
            if not name:
                continue

            for k, v in props.iteritems():
                normalized_key = osm_address_components.get_component(country, k, v)
                if normalized_key:
                    components[normalized_key].append(props)
        return components

    def address_language(self, components, candidate_languages):
        '''
        Language
        --------

        If there's only one candidate language for a given country or region,
        return that language.

        In countries that speak multiple languages (Belgium, Hong Kong, Wales, the US
        in Spanish-speaking regions, etc.), we need at least a road name for disambiguation.

        If we can't identify a language, the address will be labeled "unk". If the street name
        itself contains phrases from > 1 language, the address will be labeled ambiguous.
        '''
        language = None

        if len(candidate_languages) == 1:
            language = candidate_languages[0]['lang']
        else:
            street = components.get(AddressFormatter.ROAD, None)
            if street is not None:
                language = disambiguate_language(street, [(l['lang'], l['default']) for l in candidate_languages])
            else:
                language = UNKNOWN_LANGUAGE

        return language

    def pick_random_name_key(self, props, component, suffix=''):
        '''
        Random name
        -----------

        Pick a name key from OSM
        '''
        name_key = boundary_names.name_key(props, component)
        return name_key, ''.join((name_key, suffix)) if ':' not in name_key else name_key

    def all_names(self, props, languages=None):
        names = set()
        for k, v in six.iteritems(props):
            if k in self.ALL_OSM_NAME_KEYS:
                names.add(v)
            elif ':' in k:
                k, qual = k.split(':', 1)
                if k in self.ALL_OSM_NAME_KEYS and qual.split('_', 1)[0] in languages:
                    names.add(v)
        return names

    def normalized_place_name(self, name, tag, osm_components, country=None, languages=None):
        '''
        Multiple place names
        --------------------

        This is to help with things like  addr:city="New York NY"
        '''

        names = set()

        components = defaultdict(set)
        for props in osm_components:
            component_names = self.all_names(props, languages=languages)
            names |= component_names

            is_state = False
            for k, v in six.iteritems(props):
                normalized_key = osm_address_components.get_component(country, k, v)
                if not normalized_key:
                    continue
                for cn in component_names:
                    components[cn.lower()].add(normalized_key)

                if normalized_key == AddressFormatter.STATE and not is_state:
                    is_state = True

            if is_state:
                for state in component_names:
                    for language in languages:
                        state_code = state_abbreviations.get_abbreviation(country, language, state, default=None)
                        if state_code:
                            names.add(state_code.upper())

        phrase_filter = PhraseFilter([(n.lower(), '') for n in names])

        tokens = tokenize(name)
        tokens_lower = [(t.lower(), c) for t, c in tokens]
        phrases = list(phrase_filter.filter(tokens_lower))

        num_phrases = 0
        total_tokens = 0
        current_phrase_start = 0
        current_phrase_len = 0
        current_phrase = []

        for is_phrase, phrase_tokens, value in phrases:
            if is_phrase:
                whitespace = not any((c in (token_types.IDEOGRAPHIC_CHAR, token_types.IDEOGRAPHIC_NUMBER) for t, c in phrase_tokens))
                join_phrase = six.u(' ') if whitespace else six.u('')

                if num_phrases > 0:
                    # Return phrase with original capitalization
                    return join_phrase.join([t for t, c in tokens[:total_tokens]])
                elif num_phrases == 0 and total_tokens > 0:
                    phrase = join_phrase.join([t for t, c in phrase_tokens])
                    if tag not in components.get(phrase, set()):
                        return None
                elif num_phrases == 0:
                    current_phrase_tokens = tokens_lower[current_phrase_start:current_phrase_start + current_phrase_len]
                    current_phrase = join_phrase.join([t for t, c in current_phrase_tokens])

                current_phrase_start = total_tokens
                current_phrase_len = len(phrase_tokens)

                current_phrase_tokens = tokens_lower[current_phrase_start:current_phrase_start + current_phrase_len]

                current_phrase = join_phrase.join([t for t, c in current_phrase_tokens])
                # Handles cases like addr:city="Harlem" when Harlem is a neighborhood
                tags = components.get(current_phrase, set())
                if tags and tag not in tags:
                    return None

                total_tokens += len(phrase_tokens)
                num_phrases += 1
            else:
                total_tokens += 1

        # If the name contains a comma, stop and only use the phrase before the comma
        if ',' in name:
            return name.split(',')[0].strip()

        return name

    def normalize_place_names(self, address_components, osm_components, country=None, languages=None):
        for key in list(address_components):
            name = address_components[key]
            if key in set(self.BOUNDARY_COMPONENTS):
                name = self.normalized_place_name(name, key, osm_components,
                                                  country=country, languages=languages)

                if name is not None:
                    address_components[key] = name
                else:
                    address_components.pop(key)

    def normalize_address_components(self, components):
        address_components = {k: v for k, v in components.iteritems()
                              if k in self.formatter.aliases}
        self.formatter.aliases.replace(address_components)
        return address_components

    def cldr_country_name(self, country_code, language):
        '''
        Country names
        -------------

        In OSM, addr:country is almost always an ISO-3166 alpha-2 country code.
        However, we'd like to expand these to include natural language forms
        of the country names we might be likely to encounter in a geocoder or
        handwritten address.

        These splits are somewhat arbitrary but could potentially be fit to data
        from OpenVenues or other sources on the usage of country name forms.

        If the address includes a country, the selection procedure proceeds as follows:

        1. With probability a, select the country name in the language of the address
           (determined above), or with the localized country name if the language is
           undtermined or ambiguous.

        2. With probability b(1-a), sample a language from the distribution of
           languages on the Internet and use the country's name in that language.

        3. This is implicit, but with probability (1-b)(1-a), keep the country code
        '''

        cldr_config = nested_get(self.config, ('country', 'cldr'))

        alpha_2_iso_code_prob = float(cldr_config['iso_alpha_2_code_probability'])
        localized_name_prob = float(cldr_config['localized_name_probability'])
        alpha_3_iso_code_prob = float(cldr_config['iso_alpha_3_code_probability'])

        values = ('localized', 'alpha3', 'alpha2')
        probs = cdf([localized_name_prob, alpha_3_iso_code_prob, alpha_2_iso_code_prob])
        value = weighted_choice(values, probs)

        country_name = country_code.upper()

        if language in (AMBIGUOUS_LANGUAGE, UNKNOWN_LANGUAGE):
            language = None

        if value == 'localized':
            country_name = country_names.localized_name(country_code, language) or country_names.localized_name(country_code) or country_name
        elif value == 'alpha3':
            country_name = country_names.alpha3_code(country_code) or country_name

        return country_name

    def is_country_iso_code(self, country):
        country = country.lower()
        return country in self.iso_alpha2_codes or country in self.iso_alpha3_codes

    def replace_country_name(self, address_components, country, language):
        address_country = address_components.get(AddressFormatter.COUNTRY)

        cldr_country_prob = float(nested_get(self.config, ('country', 'cldr_country_probability')))
        replace_with_cldr_country_prob = float(nested_get(self.config, ('country', 'replace_with_cldr_country_probability')))
        remove_iso_code_prob = float(nested_get(self.config, ('country', 'remove_iso_code_probability')))

        is_iso_code = address_country and self.is_country_iso_code(address_country)

        if (is_iso_code and random.random() < replace_with_cldr_country_prob) or random.random() < cldr_country_prob:
            address_country = self.cldr_country_name(country, language)
            if address_country:
                address_components[AddressFormatter.COUNTRY] = address_country
        elif is_iso_code and random.random() < remove_iso_code_prob:
            address_components.pop(AddressFormatter.COUNTRY)

    def non_local_language(self):
        non_local_language_prob = float(nested_get(self.config, ('languages', 'non_local_language_probability')))
        if random.random() < non_local_language_prob:
            return sample_random_language()
        return None

    def state_name(self, address_components, country, language, non_local_language=None, state_full_name_prob=0.4):
        '''
        States
        ------

        Primarily for the US, Canada and Australia, OSM tends to use the abbreviated state name
        whereas we'd like to include both forms, so wtih some probability, replace the abbreviated
        name with the unabbreviated one e.g. CA => California
        '''
        address_state = address_components.get(AddressFormatter.STATE)

        if address_state and country and not non_local_language:
            state_full_name = state_abbreviations.get_full_name(country, language, address_state)

            if state_full_name and random.random() < state_full_name_prob:
                address_state = state_full_name
        elif address_state and non_local_language:
            _ = address_components.pop(AddressFormatter.STATE, None)
            address_state = None
        return address_state

    def tag_suffix(self, language, non_local_language, more_than_one_official_language=False):
        if non_local_language is not None:
            osm_suffix = ':{}'.format(non_local_language)
        elif more_than_one_official_language and language not in (AMBIGUOUS_LANGUAGE, UNKNOWN_LANGUAGE):
            osm_suffix = ':{}'.format(language)
        else:
            osm_suffix = ''
        return osm_suffix

    def add_admin_boundaries(self, address_components,
                             osm_components,
                             country, language,
                             osm_suffix='',
                             non_local_language=None,
                             random_key=True,
                             ):
        '''
        OSM boundaries
        --------------

        For many addresses, the city, district, region, etc. are all implicitly
        generated by the reverse geocoder e.g. we do not need an addr:city tag
        to identify that 40.74, -74.00 is in New York City as well as its parent
        geographies (New York county, New York state, etc.)

        Where possible we augment the addr:* tags with some of the reverse-geocoded
        relations from OSM.

        Since addresses found on the web may have the same properties, we
        include these qualifiers in the training data.
        '''

        name_key = ''.join((boundary_names.DEFAULT_NAME_KEY, osm_suffix))
        raw_name_key = boundary_names.DEFAULT_NAME_KEY
        simple_name_key = 'name:simple'
        international_name_key = 'int_name'

        if osm_components:
            osm_components = self.categorized_osm_components(country, osm_components)
            poly_components = defaultdict(list)

            existing_city_name = address_components.get(AddressFormatter.CITY)

            for component, components_values in osm_components.iteritems():
                seen = set()

                for component_value in components_values:
                    if random_key:
                        key, raw_key = self.pick_random_name_key(component_value, component, suffix=osm_suffix)
                    else:
                        key, raw_key = name_key, raw_name_key

                    name = component_value.get(key, component_value.get(raw_key))

                    if not name or (component != AddressFormatter.CITY and name == existing_city_name):
                        name = component_value.get(name_key, component_value.get(raw_name_key))

                    if not name or (component != AddressFormatter.CITY and name == existing_city_name):
                        continue

                    if (component, name) not in seen:
                        poly_components[component].append(name)
                        seen.add((component, name))

            abbreviate_state_prob = float(nested_get(self.config, ('state', 'abbreviated_probability')))
            join_state_district_prob = float(nested_get(self.config, ('state_district', 'join_probability')))
            replace_with_non_local_prob = float(nested_get(self.config, ('languages', 'replace_non_local_probability')))

            for component, vals in poly_components.iteritems():
                if component not in address_components or (non_local_language and random.random() < replace_with_non_local_prob):
                    if component == AddressFormatter.STATE_DISTRICT and random.random() < join_state_district_prob:
                        num = random.randrange(1, len(vals) + 1)
                        val = six.u(', ').join(vals[:num])
                    else:
                        val = random.choice(vals)

                    if component == AddressFormatter.STATE and random.random() < abbreviate_state_prob:
                        val = state_abbreviations.get_abbreviation(country, language,  val, default=val)

                    address_components[component] = val

    def quattroshapes_city(self, address_components,
                           latitude, longitude,
                           language, non_local_language=None,
                           abbreviated_name_prob=0.1):
        '''
        Quattroshapes/GeoNames cities
        -----------------------------

        Quattroshapes isn't great for everything, but it has decent city boundaries
        in places where OSM sometimes does not (or at least in places where we aren't
        currently able to create valid polygons). While Quattroshapes itself doesn't
        reliably use local names, which we'll want for consistency, Quattroshapes cities
        are linked with GeoNames, which has per-language localized names for most places.
        '''

        city = None

        qs_add_city_prob = float(nested_get(self.config, ('city', 'quattroshapes_geonames_backup_city_probability')))
        abbreviated_name_prob = float(nested_get(self.config, ('city', 'quattroshapes_geonames_abbreviated_probability')))

        if non_local_language or (AddressFormatter.CITY not in address_components and random.random() < qs_add_city_prob):
            lang = non_local_language or language
            quattroshapes_cities = self.quattroshapes_rtree.point_in_poly(latitude, longitude, return_all=True)
            for result in quattroshapes_cities:
                if result.get(self.quattroshapes_rtree.LEVEL) == self.quattroshapes_rtree.LOCALITY and self.quattroshapes_rtree.GEONAMES_ID in result:
                    geonames_id = int(result[self.quattroshapes_rtree.GEONAMES_ID].split(',')[0])
                    names = self.geonames.get_alternate_names(geonames_id)

                    if not names or lang not in names:
                        continue

                    city = None
                    if 'abbr' not in names or non_local_language:
                        # Use the common city name in the target language
                        city = names[lang][0][0]
                    elif random.random() < abbreviated_name_prob:
                        # Use an abbreviation: NYC, BK, SF, etc.
                        city = random.choice(names['abbr'])[0]

                    if not city or not city.strip():
                        continue
                    return city
                    break
            else:
                if non_local_language and AddressFormatter.CITY in address_components and (
                        AddressFormatter.CITY_DISTRICT in address_components or
                        AddressFormatter.SUBURB in address_components):
                    address_components.pop(AddressFormatter.CITY)

        return city

    def neighborhood_components(self, latitude, longitude):
        return self.neighborhoods_rtree.point_in_poly(latitude, longitude, return_all=True)

    def add_neighborhoods(self, address_components,
                          neighborhoods,
                          osm_suffix=''):
        '''
        Neighborhoods
        -------------

        In some cities, neighborhoods may be included in a free-text address.

        OSM includes many neighborhoods but only as points, rather than the polygons
        needed to perform reverse-geocoding. We use a hybrid index containing
        Quattroshapes/Zetashapes polygons matched fuzzily with OSM names (which are
        on the whole of better quality).
        '''

        neighborhood_levels = defaultdict(list)

        add_prefix_prob = float(nested_get(self.config, ('neighborhood', 'add_prefix_probability')))
        add_neighborhood_prob = float(nested_get(self.config, ('neighborhood', 'add_neighborhood_probability')))

        name_key = ''.join((boundary_names.DEFAULT_NAME_KEY, osm_suffix))
        raw_name_key = boundary_names.DEFAULT_NAME_KEY

        for neighborhood in neighborhoods:
            place_type = neighborhood.get('place')
            polygon_type = neighborhood.get('polygon_type')

            neighborhood_level = AddressFormatter.SUBURB

            if place_type == 'borough' or polygon_type == 'local_admin':
                neighborhood_level = AddressFormatter.CITY_DISTRICT

                # Optimization so we don't use e.g. Brooklyn multiple times
                city_name = address_components.get(AddressFormatter.CITY)
                if name == city_name:
                    name = neighborhood.get(name_key, neighborhood.get(raw_name_key))
                    if not name or name == city_name:
                        continue

            key, raw_key = self.pick_random_name_key(neighborhood, neighborhood_level, suffix=osm_suffix)
            name = neighborhood.get(key, neighborhood.get(raw_key))

            if not name:
                name = neighborhood.get(name_key, neighborhood.get(raw_name_key))

                name_prefix = neighborhood.get('name:prefix')

                if name and name_prefix and random.random() < add_prefix_prob:
                    name = six.u(' ').join([name_prefix, name])

            if not name:
                continue

            neighborhood_levels[neighborhood_level].append(name)

        for component, neighborhoods in neighborhood_levels.iteritems():
            if component not in address_components and random.random() < add_neighborhood_prob:
                address_components[component] = neighborhoods[0]

    def replace_name_affixes(self, address_components, language, replacement_prob=0.6):
        '''
        Name normalization
        ------------------

        Probabilistically strip standard prefixes/suffixes e.g. "London Borough of"
        '''

        replacement_prob = float(nested_get(self.config, ('names', 'replace_affix_probability')))

        for component in list(address_components):
            if component not in self.BOUNDARY_COMPONENTS:
                continue
            name = address_components[component]
            if not name:
                continue
            replacement = name_affixes.replace_suffixes(name, language)
            replacement = name_affixes.replace_prefixes(replacement, language)
            if replacement != name and random.random() < replacement_prob:
                address_components[component] = replacement

    def replace_names(self, address_components):
        '''
        Name replacements
        -----------------

        Make a few special replacements (like UK instead of GB)
        '''

        for component, value in address_components.iteritems():
            replacement = nested_get(self.config, ('value_replacements', component, value), default=None)
            if replacement is not None:
                new_value = repl['replacement']
                prob = repl['probability']
                if random.random() < prob:
                    address_components[component] = new_value

    def prune_duplicate_names(self, address_components):
        '''
        Name deduping
        -------------

        For some cases like "Antwerpen, Antwerpen, Antwerpen"
        that are very unlikely to occur in real life.

        Note: prefer the city name in these cases
        '''

        name_components = defaultdict(list)

        for component in (AddressFormatter.CITY, AddressFormatter.STATE_DISTRICT,
                          AddressFormatter.CITY_DISTRICT, AddressFormatter.SUBURB):
            name = address_components.get(component)
            if name:
                name_components[name].append(component)

        for name, components in name_components.iteritems():
            if len(components) > 1:
                for component in components[1:]:
                    address_components.pop(component, None)

    def cleanup_house_number(self, address_components):
        '''
        House number cleanup
        --------------------

        This method was originally used for OSM nodes because in some places,
        particularly Uruguay, we see house numbers that are actually a comma-separated
        list. It seemed prudent to retain this cleanup in the generalized version
        in case we see similar issues with other data sets.

        If there's one comma in the house number, allow it as it might
        be legitimate, but if there are 2 or more, just take the first one.
        '''

        house_number = address_components.get(AddressFormatter.HOUSE_NUMBER)
        if not house_number:
            return
        if ';' in house_number:
            house_number = house_number.replace(';', ',')
            address_components[AddressFormatter.HOUSE_NUMBER] = house_number
        if house_number and house_number.count(',') >= 2:
            house_numbers = house_number.split(',')
            random.shuffle(house_numbers)
            for num in house_numbers:
                num = num.strip()
                if num:
                    address_components[AddressFormatter.HOUSE_NUMBER] = num
                    break
            else:
                address_components.pop(AddressFormatter.HOUSE_NUMBER, None)

    def expanded_address_components(self, address_components, latitude, longitude):
        try:
            latitude, longitude = latlon_to_decimal(latitude, longitude)
        except Exception:
            return None, None, None

        country, candidate_languages, language_props = self.language_rtree.country_and_languages(latitude, longitude)
        if not (country and candidate_languages):
            return None, None, None

        language = None

        more_than_one_official_language = len(candidate_languages) > 1

        language = self.address_language(address_components, candidate_languages)

        non_local_language = self.non_local_language()
        # If a country already was specified
        self.replace_country_name(address_components, country, non_local_language or language)

        address_state = self.state_name(address_components, country, language, non_local_language=non_local_language)
        if address_state:
            address_components[AddressFormatter.STATE] = address_state

        osm_suffix = self.tag_suffix(language, non_local_language, more_than_one_official_language)

        osm_components = self.osm_reverse_geocoded_components(latitude, longitude)
        neighborhoods = self.neighborhood_components(latitude, longitude)

        all_languages = set([l['lang'] for l in candidate_languages])

        all_osm_components = osm_components + neighborhoods
        self.normalize_place_names(address_components, all_osm_components, country=country, languages=all_languages)

        self.add_admin_boundaries(address_components, osm_components, country, language,
                                  non_local_language=non_local_language,
                                  osm_suffix=osm_suffix)

        city = self.quattroshapes_city(address_components, latitude, longitude, language, non_local_language=non_local_language)
        if city:
            address_components[AddressFormatter.CITY] = city

        self.add_neighborhoods(address_components, neighborhoods,
                               osm_suffix=osm_suffix)

        street = address_components.get(AddressFormatter.ROAD)

        self.replace_name_affixes(address_components, non_local_language or language)

        self.replace_names(address_components)

        self.prune_duplicate_names(address_components)

        self.cleanup_house_number(address_components)

        return address_components, country, language

    def limited_address_components(self, address_components, latitude, longitude):
        try:
            latitude, longitude = latlon_to_decimal(latitude, longitude)
        except Exception:
            return None, None, None

        country, candidate_languages, language_props = self.language_rtree.country_and_languages(latitude, longitude)
        if not (country and candidate_languages):
            return None, None, None

        remove_keys = NAME_KEYS + HOUSE_NUMBER_KEYS + POSTAL_KEYS + OSM_IGNORE_KEYS

        for key in remove_keys:
            _ = value.pop(key, None)

        language = None

        more_than_one_official_language = len(candidate_languages) > 1

        language = self.address_language(value, candidate_languages)

        address_components = self.normalize_address_components(value)

        non_local_language = self.non_local_language()
        self.replace_country_name(address_components, country, non_local_language or language)

        address_state = self.state_name(address_components, country, language, non_local_language=non_local_language, state_full_name_prob=1.0)
        if address_state:
            address_components[AddressFormatter.STATE] = address_state

        street = address_components.get(AddressFormatter.ROAD)

        osm_suffix = self.tag_suffix(language, non_local_language, more_than_one_official_language)

        osm_components = self.osm_reverse_geocoded_components(latitude, longitude)
        neighborhoods = self.neighborhood_components(latitude, longitude)

        all_languages = set([l['lang'] for l in candidate_languages])

        all_osm_components = osm_components + neighborhoods
        self.normalize_place_names(address_components, all_osm_components, country=country, languages=all_languages)

        self.add_admin_boundaries(address_components, osm_components, country, language,
                                  osm_suffix=osm_suffix,
                                  non_local_language=non_local_language,
                                  random_key=False,
                                  alpha_3_iso_code_prob=0.0,
                                  alpha_2_iso_code_prob=0.0,
                                  replace_with_non_local_prob=0.0,
                                  abbreviate_state_prob=0.0)

        city = self.quattroshapes_city(address_components, latitude, longitude, language, non_local_language=non_local_language)

        if city:
            address_components[AddressFormatter.CITY] = city

        neighborhoods = self.neighborhood_components(latitude, longitude)

        self.add_neighborhoods(address_components, neighborhoods,
                               osm_suffix=osm_suffix)

        self.replace_name_affixes(address_components, non_local_language or language)

        self.replace_names(address_components)

        self.prune_duplicate_names(address_components)

        return address_components, country, language
