import operator
import os
import pycountry
import random
import six
import yaml

from collections import defaultdict, OrderedDict
from itertools import combinations

from geodata.address_formatting.formatter import AddressFormatter

from geodata.addresses.config import address_config
from geodata.addresses.floors import Floor
from geodata.addresses.entrances import Entrance
from geodata.addresses.house_numbers import HouseNumber
from geodata.addresses.po_boxes import POBox
from geodata.addresses.postcodes import PostCode
from geodata.addresses.staircases import Staircase
from geodata.addresses.units import Unit
from geodata.boundaries.names import boundary_names
from geodata.configs.utils import nested_get
from geodata.coordinates.conversion import latlon_to_decimal
from geodata.countries.names import *
from geodata.language_id.disambiguation import *
from geodata.language_id.sample import sample_random_language
from geodata.math.floats import isclose
from geodata.math.sampling import cdf, weighted_choice
from geodata.names.normalization import name_affixes
from geodata.osm.components import osm_address_components
from geodata.places.config import place_config
from geodata.states.state_abbreviations import state_abbreviations

this_dir = os.path.realpath(os.path.dirname(__file__))

PARSER_DEFAULT_CONFIG = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                     'resources', 'parser', 'default.yaml')


class ComponentDependencies(object):
    '''
    Declare an address component and its dependencies e.g.
    a house_numer cannot be used in the absence of a road name.
    '''

    def __init__(self, name, dependencies=tuple()):
        self.name = name
        self.dependencies = dependencies


class AddressComponents(object):
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
    >>> components = AddressComponents(osm_admin_rtree, language_rtree, neighborhoods_rtree, buildings_rtree, subdivisions_rtree, quattroshapes_rtree, geonames)
    >>> components.expand({'name': 'Hackney Empire'}, 51.54559, -0.05567)

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

    BOUNDARY_COMPONENTS = OrderedDict.fromkeys((
        AddressFormatter.SUBDIVISION,
        AddressFormatter.SUBURB,
        AddressFormatter.CITY_DISTRICT,
        AddressFormatter.CITY,
        AddressFormatter.ISLAND,
        AddressFormatter.STATE_DISTRICT,
        AddressFormatter.STATE,
        AddressFormatter.COUNTRY,
    ))

    NAME_COMPONENTS = {
        AddressFormatter.ATTENTION,
        AddressFormatter.CARE_OF,
        AddressFormatter.HOUSE,
    }

    ADDRESS_LEVEL_COMPONENTS = {
        AddressFormatter.ATTENTION,
        AddressFormatter.CARE_OF,
        AddressFormatter.HOUSE,
        AddressFormatter.HOUSE_NUMBER,
        AddressFormatter.ROAD,
        AddressFormatter.ENTRANCE,
        AddressFormatter.STAIRCASE,
        AddressFormatter.LEVEL,
        AddressFormatter.UNIT,
    }

    ALL_OSM_NAME_KEYS = set(['name', 'name:simple',
                             'ISO3166-1:alpha2', 'ISO3166-1:alpha3',
                             'short_name', 'alt_name', 'official_name'])

    NULL_PHRASE = 'null'
    ALPHANUMERIC_PHRASE = 'alphanumeric'
    STANDALONE_PHRASE = 'standalone'

    class zones:
        COMMERCIAL = 'commercial'
        RESIDENTIAL = 'residential'
        INDUSTRIAL = 'industrial'
        UNIVERSITY = 'university'

    sub_building_component_class_map = {
        AddressFormatter.ENTRANCE: Entrance,
        AddressFormatter.STAIRCASE: Staircase,
        AddressFormatter.LEVEL: Floor,
        AddressFormatter.UNIT: Unit,
    }

    def __init__(self, osm_admin_rtree, language_rtree, neighborhoods_rtree, quattroshapes_rtree, geonames):
        self.config = yaml.load(open(PARSER_DEFAULT_CONFIG))

        self.setup_component_dependencies()
        # Non-admin component dropout
        self.address_level_dropout_probabilities = {k: v['probability'] for k, v in six.iteritems(self.config['dropout'])}

        self.osm_admin_rtree = osm_admin_rtree
        self.language_rtree = language_rtree
        self.neighborhoods_rtree = neighborhoods_rtree
        self.quattroshapes_rtree = quattroshapes_rtree
        self.geonames = geonames

    def setup_component_dependencies(self):
        self.component_dependencies = {}
        self.component_bit_values = {}
        self.valid_component_bitsets = set()
        self.component_combinations = set()

        forward_deps = self.config.get('component_dependencies', {})

        for i, component in enumerate(AddressFormatter.address_formatter_fields):
            self.component_bit_values[component] = 1 << i

        all_values = self.component_bitset(forward_deps)

        for component, conf in six.iteritems(forward_deps):
            deps = conf['dependencies']
            self.component_dependencies[component] = self.component_bitset(deps) if deps else all_values

    def component_bitset(self, components):
        return reduce(operator.or_, [self.component_bit_values[c] for c in components])

    def address_level_dropout_order(self, components):
        '''
        Address component dropout
        -------------------------

        To make the parser more robust to different kinds of input (not every address is fully
        specified, especially in a geocoder, on mobile, with autocomplete, etc.), we want to
        train the parser with many types of addresses.

        This will help the parser not become too reliant on component order, e.g. it won't think
        that the first token in a string is always the venue name simply because that was the case
        in the training data.

        This method returns a dropout ordering ensuring that if the components are dropped in order,
        each set will be valid. In the parser config (resources/parser/default.yaml), the dependencies
        for each address component are specified, e.g. "house_number" depends on "road", so it would
        be invalid to have an address that was simply a house number with no other information. The
        caller of this method may decide to drop all the components at once or one at a time, creating
        N training examples from a single address.

        Some components are also more likely to be dropped than others, so in the same config there are
        dropout probabilities for each.
        '''
        if not components:
            return []

        component_bitset = self.component_bitset(components)

        candidates = [c for c in components if c in self.address_level_dropout_probabilities]
        random.shuffle(candidates)
        retained = set(candidates)

        dropout_order = []

        for component in candidates[:-1]:
            if random.random() >= self.address_level_dropout_probabilities.get(component, 0.0):
                continue
            bit_value = self.component_bit_values.get(component, 0)
            candidate_bitset = component_bitset ^ bit_value

            if all((candidate_bitset & self.component_dependencies[c] for c in retained if c != component)):
                dropout_order.append(component)
                component_bitset = candidate_bitset
                retained.remove(component)
        return dropout_order

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
                    break
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

            lang_tuples = [(l['lang'], l['default']) for l in candidate_languages]
            if street is not None:
                language = disambiguate_language(street, lang_tuples)
            else:
                if has_non_latin_script(lang_tuples):
                    for component, value in six.iteritems(components):
                        language, script_langs = disambiguate_language_script(value, lang_tuples)
                        if language is not UNKNOWN_LANGUAGE:
                            break
                    else:
                        language = UNKNOWN_LANGUAGE
                else:
                    language = UNKNOWN_LANGUAGE

        return language

    def pick_random_name_key(self, props, component, suffix=''):
        '''
        Random name
        -----------

        Pick a name key from OSM
        '''
        raw_key = boundary_names.name_key(props, component)

        key = ''.join((raw_key, suffix)) if ':' not in raw_key else raw_key
        return key, raw_key

    def all_names(self, props, languages, keys=ALL_OSM_NAME_KEYS):
        # Preserve uniqueness and order
        names = OrderedDict()
        for k, v in six.iteritems(props):
            if k in keys:
                names[v] = None
            elif ':' in k:
                k, qual = k.split(':', 1)
                if k in self.ALL_OSM_NAME_KEYS and qual.split('_', 1)[0] in languages:
                    names[v] = None
        return names.keys()

    def normalized_place_name(self, name, tag, osm_components, country=None, languages=None):
        '''
        Multiple place names
        --------------------

        This is to help with things like  addr:city="New York NY"
        '''

        names = set()

        components = defaultdict(set)
        for props in osm_components:
            component_names = set(self.all_names(props, languages or []))
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

    def combine_fields(self, address_components, language, country=None, generated_components=None):
        combo_config = address_config.get_property('components.combinations', language, country=country, default={})
        values = []
        probs = []
        generated_components = generated_components or set()

        for k, v in six.iteritems(combo_config):
            values.append(v)
            probs.append(v['probability'])

        if not isclose(sum(probs), 1.0):
            values.append(None)
            probs.append(1.0 - sum(probs))

        probs = cdf(probs)

        combo = weighted_choice(values, probs)
        if combo is not None:
            components = OrderedDict.fromkeys(combo['components']).keys()
            if not all((c in address_components and (c in generated_components or self.is_numeric(address_components[c])) for c in components)):
                return None

            values = []
            probs = []
            for s in combo['separators']:
                values.append(s['separator'])
                probs.append(s['probability'])

            probs = cdf(probs)
            separator = weighted_choice(values, probs)

            new_label = combo['label']
            new_value = separator.join([address_components.pop(c) for c in components])
            address_components[new_label] = new_value
            return new_label
        return None

    def generated_type(self, component, existing_components, language, country=None):
        component_config = address_config.get_property('components.{}'.format(component), language, country=country)
        if not component_config:
            return None

        prob_dist = component_config

        conditionals = component_config.get('conditional', [])

        if conditionals:
            for vals in conditionals:
                c = vals['component']
                if c in existing_components:
                    prob_dist = vals['probabilities']
                    break

        values = []
        probs = []
        for num_type in (self.NULL_PHRASE, self.ALPHANUMERIC_PHRASE, self.STANDALONE_PHRASE):
            key = '{}_probability'.format(num_type)
            prob = component_config.get(key)
            if prob is not None:
                values.append(num_type)
                probs.append(prob)

        probs = cdf(probs)
        num_type = weighted_choice(values, probs)

        if num_type == self.NULL_PHRASE:
            return None
        else:
            return num_type

    def is_numeric(self, component):
        tokens = tokenize(component)
        return sum((1 for t, c in tokens if c == token_types.NUMERIC or c not in token_types.WORD_TOKEN_TYPES)) == len(tokens)

    def get_component_phrase(self, cls, component, language, country=None):
        component = safe_decode(component)
        if self.is_numeric(component):
            phrase = cls.phrase(component, language, country=country)
            if phrase != component:
                return phrase
            else:
                return None
        else:
            return component

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

    def state_name(self, address_components, country, language, non_local_language=None, always_use_full_names=False):
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

            state_full_name_prob = float(nested_get(self.config, ('state', 'full_name_probability')))

            if state_full_name and (always_use_full_names or random.random() < state_full_name_prob):
                address_state = state_full_name
        elif address_state and non_local_language:
            _ = address_components.pop(AddressFormatter.STATE, None)
            address_state = None
        return address_state

    def pick_language_suffix(self, osm_components, language, non_local_language, more_than_one_official_language):
        '''
        Language suffix
        ---------------

        This captures some variations in languages written with different scripts
        e.g. language=ja_rm is for Japanese Romaji.

        Pick a language suffix with probability proportional to how often the name is used
        in the reverse geocoded components. So if only 2/5 components have name:ja_rm listed
        but 5/5 have either name:ja or just plain name, we would pick standard Japanese (Kanji) 
        with probability .7143 (5/7) and Romaji with probability .2857 (2/7).
        '''
        # This captures name variations like "ja_rm" for Japanese Romaji, etc.
        language_scripts = defaultdict(int)
        use_language = (non_local_language or language)

        for c in osm_components:
            for k, v in six.iteritems(c):
                if ':' not in k:
                    continue
                splits = k.split(':')
                if len(splits) > 0 and splits[0] == 'name' and '_' in splits[-1] and splits[-1].split('_', 1)[0] == use_language:
                    language_scripts[splits[-1]] += 1
                elif k == 'name' or (splits[0] == 'name' and splits[-1]) == use_language:
                    language_scripts[None] += 1

        language_script = None

        if len(language_scripts) > 1:
            cumulative = float(sum(language_scripts.values()))
            values = list(language_scripts.keys())
            probs = cdf([float(c) / cumulative for c in language_scripts.values()])
            language_script = weighted_choice(values, probs)

        if not language_script and not non_local_language and not more_than_one_official_language:
            return ''
        else:
            return ':{}'.format(language_script or non_local_language or language)

    def add_admin_boundaries(self, address_components,
                             osm_components,
                             country, language,
                             non_local_language=None,
                             language_suffix='',
                             random_key=True,
                             always_use_full_names=False,
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

        simple_name_key = 'name:simple'
        international_name_key = 'int_name'

        if osm_components:
            name_key = ''.join((boundary_names.DEFAULT_NAME_KEY, language_suffix))
            raw_name_key = boundary_names.DEFAULT_NAME_KEY

            osm_components = self.categorized_osm_components(country, osm_components)
            poly_components = defaultdict(list)

            existing_city_name = address_components.get(AddressFormatter.CITY)

            for component, components_values in osm_components.iteritems():
                seen = set()

                for component_value in components_values:
                    if random_key:
                        key, raw_key = self.pick_random_name_key(component_value, component, suffix=language_suffix)
                    else:
                        key, raw_key = name_key, raw_name_key

                    for k in (key, name_key, raw_key, raw_name_key):
                        name = component_value.get(k)
                        if name and not (name == existing_city_name and component != AddressFormatter.CITY):
                            break
                    # if we've checked all keys without finding a valid name, leave this component out
                    else:
                        continue

                    if (component, name) not in seen:
                        poly_components[component].append(name)
                        seen.add((component, name))

            abbreviate_state_prob = float(nested_get(self.config, ('state', 'abbreviated_probability')))
            join_state_district_prob = float(nested_get(self.config, ('state_district', 'join_probability')))
            replace_with_non_local_prob = float(nested_get(self.config, ('languages', 'replace_non_local_probability')))

            for component, vals in poly_components.iteritems():
                if component not in address_components or (non_local_language and random.random() < replace_with_non_local_prob):
                    if not always_use_full_names:
                        if component == AddressFormatter.STATE_DISTRICT and random.random() < join_state_district_prob:
                            num = random.randrange(1, len(vals) + 1)
                            val = six.u(', ').join(vals[:num])
                        elif len(vals) == 1:
                            val = vals[0]
                        else:
                            val = random.choice(vals)

                        if component == AddressFormatter.STATE and random.random() < abbreviate_state_prob:
                            val = state_abbreviations.get_abbreviation(country, language, val, default=val)

                    address_components[component] = val

    def quattroshapes_city(self, address_components,
                           latitude, longitude,
                           language, non_local_language=None,
                           always_use_full_names=False):
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

        if AddressFormatter.CITY not in address_components and random.random() < qs_add_city_prob:
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
                    elif not always_use_full_names and random.random() < abbreviated_name_prob:
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
                          neighborhoods, language_suffix=''):
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

        name_key = ''.join((boundary_names.DEFAULT_NAME_KEY, language_suffix))
        raw_name_key = boundary_names.DEFAULT_NAME_KEY

        for neighborhood in neighborhoods:
            place_type = neighborhood.get('place')
            polygon_type = neighborhood.get('polygon_type')

            neighborhood_level = AddressFormatter.SUBURB

            key, raw_key = self.pick_random_name_key(neighborhood, neighborhood_level, suffix=language_suffix)
            name = neighborhood.get(key, neighborhood.get(raw_key))

            if place_type == 'borough' or polygon_type == 'local_admin':
                neighborhood_level = AddressFormatter.CITY_DISTRICT

                # Optimization so we don't use e.g. Brooklyn multiple times
                city_name = address_components.get(AddressFormatter.CITY)
                if name == city_name:
                    name = neighborhood.get(name_key, neighborhood.get(raw_name_key))
                    if not name or name == city_name:
                        continue

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

    def generate_sub_building_component(self, component, address_components, language, country=None, **kw):
        existing = address_components.get(component, None)

        component_class = self.sub_building_component_class_map[component]

        if existing is None:
            generated_type = self.generated_type(component, address_components, language, country=country)
            if generated_type == self.ALPHANUMERIC_PHRASE:
                num = component_class.random(language, country=country, **kw)
                address_components[component] = num
                return num
            elif generated_type == self.STANDALONE_PHRASE:
                return None

        return None

    def add_sub_building_phrase(self, component, address_components, language, country, generated_components=None, **kw):
        num = address_components.get(component)
        if not num:
            return
        generated_components = generated_components or set()

        component_class = self.sub_building_component_class_map[component]
        if component in generated_components:
            phrase = component_class.phrase(num, language, country=country, **kw)

            if phrase:
                address_components[component] = phrase
        else:
            existing = address_components[component]
            phrase = self.get_component_phrase(component_class, existing, language, country=country)
            if phrase and phrase != existing:
                address_components[component] = phrase

    def add_sub_building_components(self, address_components, language, country=None, num_floors=None, num_basements=None, zone=None):
        generated_components = set()

        if self.generate_sub_building_component(AddressFormatter.ENTRANCE, address_components, language, country=country):
            generated_components.add(AddressFormatter.ENTRANCE)

        if self.generate_sub_building_component(AddressFormatter.STAIRCASE, address_components, language, country=country):
            generated_components.add(AddressFormatter.STAIRCASE)

        if self.generate_sub_building_component(AddressFormatter.LEVEL, address_components, language, country=country, num_floors=num_floors, num_basements=num_basements):
            generated_components.add(AddressFormatter.LEVEL)

        if self.generate_sub_building_component(AddressFormatter.UNIT, address_components, language, country=country, num_floors=num_floors, num_basements=num_basements):
            generated_components.add(AddressFormatter.UNIT)

        # Combine fields like unit/house_number here
        combined = self.combine_fields(address_components, language, country=country, generated_components=generated_components)
        if combined:
            generated_components -= set([combined])

        self.add_sub_building_phrase(AddressFormatter.ENTRANCE, address_components, language, country=country, generated_components=generated_components)
        self.add_sub_building_phrase(AddressFormatter.STAIRCASE, address_components, language, country=country, generated_components=generated_components)

        self.add_sub_building_phrase(AddressFormatter.LEVEL, address_components, language, country=country,
                                     generated_components=generated_components,
                                     num_floors=num_floors)

        self.add_sub_building_phrase(AddressFormatter.UNIT, address_components, language, country=country,
                                     generated_components=generated_components,
                                     zone=zone)

    def replace_name_affixes(self, address_components, language):
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

    def add_house_number_phrase(self, address_components, language, country=None):
        house_number = address_components.get(AddressFormatter.HOUSE_NUMBER, None)
        phrase = HouseNumber.phrase(house_number, language, country=country)
        if phrase and phrase != house_number:
            address_components[AddressFormatter.HOUSE_NUMBER] = phrase

    def add_postcode_phrase(self, address_components, language, country=None):
        postcode = address_components.get(AddressFormatter.POSTCODE, None)
        if postcode:
            phrase = PostCode.phrase(postcode, language, country=country)
            if phrase and phrase != postcode:
                address_components[AddressFormatter.POSTCODE] = phrase

    def drop_names(self, address_components):
        return {c: v for c, v in six.iteritems(address_components) if c not in self.NAME_COMPONENTS}

    def drop_address(self, address_components):
        return {c: v for c, v in six.iteritems(address_components) if c not in self.ADDRESS_LEVEL_COMPONENTS}

    def drop_places(self, address_components):
        return {c: v for c, v in six.iteritems(address_components) if c not in place_config.ADMIN_COMPONENTS}

    def drop_postcode(self, address_components):
        if AddressFormatter.POSTCODE not in address_components:
            return address_components
        return {c: v for c, v in six.iteritems(address_components) if c != AddressFormatter.POSTCODE}

    def drop_invalid_components(self, address_components):
        if not address_components:
            return
        component_bitset = self.component_bitset(address_components)
        for c in list(address_components):
            if c in self.component_dependencies and not component_bitset & self.component_dependencies[c]:
                address_components.pop(c)
                component_bitset ^= self.component_bit_values[c]

    def po_box_address(self, address_components, language, country=None):
        po_box_config = self.config['po_box']
        po_box_probability = float(po_box_config['probability'])
        if random.random() < po_box_probability:
            address_components = address_components.copy()
            box_number = POBox.random(language, country=country)
            if box_number is None:
                return None

            po_box = POBox.phrase(box_number, language, country=country)
            address_components[AddressFormatter.PO_BOX] = po_box

            drop_address_probability = po_box_config['drop_address_probability']
            if random.random() < drop_address_probability:
                address_components = self.drop_address(address_components)

            drop_places_probability = po_box_config['drop_places_probability']
            if random.random() < drop_places_probability:
                address_components = self.drop_places(address_components)

            drop_postcode_probability = po_box_config['drop_postcode_probability']
            if random.random() < drop_postcode_probability:
                address_components = self.drop_postcode(address_components)

            return address_components
        else:
            return None

    def dropout_address_level_component(self, address_components, component):
        probability = self.address_level_dropout_probabilities.get(component, None)
        if probability is not None and random.random() < probability:
            address_components.pop(component)
            return True
        return False

    def expanded(self, address_components, latitude, longitude,
                 dropout_places=True, add_sub_building_components=True,
                 num_floors=None, num_basements=None, zone=None):
        '''
        Expanded components
        -------------------

        Many times in geocoded address data sets, we get only a few components
        (say street name and house number) plus a lat/lon. There's a lot of information
        in a lat/lon though, so this method "fills in the blanks" as it were.

        Namely, it calls all the methods above to reverse geocode to a few of the
        R-tree + point-in-polygon indices passed in at initialization and adds things
        like admin boundaries, neighborhoods,
        '''
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
        # If a country was already specified
        self.replace_country_name(address_components, country, non_local_language or language)

        address_state = self.state_name(address_components, country, language, non_local_language=non_local_language)
        if address_state:
            address_components[AddressFormatter.STATE] = address_state

        osm_components = self.osm_reverse_geocoded_components(latitude, longitude)
        neighborhoods = self.neighborhood_components(latitude, longitude)

        all_languages = set([l['lang'] for l in candidate_languages])

        all_osm_components = osm_components + neighborhoods
        language_suffix = self.pick_language_suffix(all_osm_components, language, non_local_language, more_than_one_official_language)

        self.normalize_place_names(address_components, all_osm_components, country=country, languages=all_languages)

        self.add_admin_boundaries(address_components, osm_components, country, language,
                                  non_local_language=non_local_language,
                                  language_suffix=language_suffix)

        city = self.quattroshapes_city(address_components, latitude, longitude, language, non_local_language=non_local_language)
        if city:
            address_components[AddressFormatter.CITY] = city

        self.add_neighborhoods(address_components, neighborhoods,
                               language_suffix=language_suffix)

        street = address_components.get(AddressFormatter.ROAD)

        self.replace_name_affixes(address_components, non_local_language or language)

        self.replace_names(address_components)

        self.prune_duplicate_names(address_components)

        self.cleanup_house_number(address_components)
        self.add_house_number_phrase(address_components, language, country=country)
        self.add_postcode_phrase(address_components, language, country=country)

        if add_sub_building_components:
            self.add_sub_building_components(address_components, language, country=country,
                                             num_floors=num_floors, num_basements=num_basements, zone=zone)

        if dropout_places:
            # Perform dropout on places
            address_components = place_config.dropout_components(address_components, all_osm_components, country=country)

        self.drop_invalid_components(address_components)

        if language_suffix and not non_local_language:
            language = language_suffix.lstrip(':')

        return address_components, country, language

    def limited(self, address_components, latitude, longitude):
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

        address_state = self.state_name(address_components, country, language, non_local_language=non_local_language, always_use_full_names=True)
        if address_state:
            address_components[AddressFormatter.STATE] = address_state

        street = address_components.get(AddressFormatter.ROAD)

        osm_components = self.osm_reverse_geocoded_components(latitude, longitude)
        neighborhoods = self.neighborhood_components(latitude, longitude)

        all_languages = set([l['lang'] for l in candidate_languages])

        all_osm_components = osm_components + neighborhoods
        language_suffix = self.pick_language_suffix(all_osm_components, language, non_local_language, more_than_one_official_language)

        self.normalize_place_names(address_components, all_osm_components, country=country, languages=all_languages)

        self.add_admin_boundaries(address_components, osm_components, country, language,
                                  language_suffix=language_suffix,
                                  non_local_language=non_local_language,
                                  random_key=False,
                                  always_use_full_names=True)

        city = self.quattroshapes_city(address_components, latitude, longitude, language, non_local_language=non_local_language,
                                       always_use_full_names=True)

        if city:
            address_components[AddressFormatter.CITY] = city

        neighborhoods = self.neighborhood_components(latitude, longitude)

        self.add_neighborhoods(address_components, neighborhoods,
                               language_suffix=language_suffix)

        self.replace_name_affixes(address_components, non_local_language or language)

        self.replace_names(address_components)

        self.prune_duplicate_names(address_components)

        if language_suffix and not non_local_language:
            language = language_suffix.lstrip(':').lower()

        return address_components, country, language
