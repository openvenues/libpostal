import random
import six

from geodata.addresses.config import address_config

from geodata.addresses.numbering import NumberedComponent, sample_alphabet, latin_alphabet
from geodata.encoding import safe_decode
from geodata.math.sampling import weighted_choice, zipfian_distribution, cdf
from geodata.numbers.spellout import numeric_expressions


class Floor(NumberedComponent):
    # When we don't know the number of floors, use a Zipfian distribution
    # to choose randomly between 1 and max_floors with 1 being much more
    # likely than 2, etc.
    max_floors = 10
    max_basements = 2
    numbered_floors = range(max_floors + 1) + range(-1, -max_basements - 1, -1)
    floor_probs = zipfian_distribution(len(numbered_floors), 0.75)
    floor_probs_cdf = cdf(floor_probs)

    # For use with letters e.g. A0 is probably not as common
    floors_letters = range(1, max_floors + 1) + [0]
    floors_letters_probs = zipfian_distribution(len(floors_letters), 2.0)
    floors_letters_cdf = cdf(floors_letters_probs)

    @classmethod
    def sample_floors(cls, num_floors, num_basements=0):
        num_floors = int(num_floors)
        return random.randint(-num_basements, (num_floors - 1) if num_floors > 0 else 0)

    @classmethod
    def sample_floors_range(cls, min_floor, max_floor):
        return random.randint(min_floor, (max_floor - 1) if max_floor > min_floor else min_floor)

    @classmethod
    def random_int(cls, language, country=None, num_floors=None, num_basements=None):
        number = None
        if num_floors is not None:
            try:
                num_floors = int(num_floors)
            except (ValueError, TypeError):
                return weighted_choice(cls.numbered_floors, cls.floor_probs_cdf)

            if num_floors <= cls.max_floors:
                number = cls.sample_floors(num_floors, num_basements=num_basements or 0)
            else:
                number = cls.sample_floors_range(cls.max_floors + 1, num_floors)

        else:
            number = weighted_choice(cls.numbered_floors, cls.floor_probs_cdf)

        return number

    @classmethod
    def random_from_int(cls, number, language, country=None):
        num_type, num_type_props = cls.choose_alphanumeric_type('levels.alphanumeric', language, country=country)
        if num_type is None:
            return None

        numbering_starts_at = int(address_config.get_property('levels.numbering_starts_at', language, country=country, default=0))

        if number >= 0:
            number += numbering_starts_at

        if num_type == cls.NUMERIC:
            return safe_decode(number)
        elif num_type == cls.ROMAN_NUMERAL:
            roman_numeral = numeric_expressions.roman_numeral(number)
            if roman_numeral is not None:
                return roman_numeral
            else:
                return safe_decode(number)
        elif num_type == cls.HYPHENATED_NUMBER:
            number2 = number + sample_floors_range(1, cls.max_floors)
            return u'{}-{}'.format(number, number2)
        else:
            alphabet = address_config.get_property('alphabet', language, country=country, default=latin_alphabet)
            alphabet_probability = address_config.get_property('alphabet_probability', language, country=country, default=None)
            if alphabet_probability is not None and random.random() >= alphabet_probability:
                alphabet = latin_alphabet
            letter = sample_alphabet(alphabet)
            if num_type == cls.ALPHA:
                return letter
            else:
                number = weighted_choice(cls.floors_letters, cls.floors_letters_cdf)

                if num_type == cls.ALPHA_PLUS_NUMERIC:
                    return six.u('{}{}').format(letter, number)
                elif num_type == cls.NUMERIC_PLUS_ALPHA:
                    return six.u('{}{}').format(number, letter)

        return None

    @classmethod
    def random(cls, language, country=None, num_floors=None, num_basements=None):
        number = cls.random_int(language, country=country, num_floors=num_floors, num_basements=num_basements)
        return cls.random_from_int(number, language, country=country)

    @classmethod
    def phrase(cls, floor, language, country=None, num_floors=None):
        if floor is None:
            return None

        integer_floor = False
        floor = safe_decode(floor)
        try:
            floor = int(floor)
            integer_floor = True
        except (ValueError, TypeError):
            try:
                floor = float(floor)
                integer_floor = int(floor) == floor
            except (ValueError, TypeError):
                return cls.numeric_phrase('levels.alphanumeric', floor, language,
                                          dictionaries=['level_types_numbered'], country=country)

        numbering_starts_at = int(address_config.get_property('levels.numbering_starts_at', language, country=country, default=0))
        try:
            num_floors = int(num_floors)
            top_floor = num_floors if numbering_starts_at == 1 else num_floors - 1
            is_top = num_floors and floor == top_floor
        except (ValueError, TypeError):
            is_top = False

        alias_prefix = 'levels.aliases'
        aliases = address_config.get_property(alias_prefix, language, country=country)
        if aliases:
            alias = None

            if not integer_floor and floor >= 0 and 'half_floors' in aliases:
                floor = int(floor)
                alias = 'half_floors'
            elif not integer_floor and floor < 0 and 'half_floors_negative' in aliases:
                floor = int(floor)
                alias = 'half_floors_negative'
            elif floor < -1 and '<-1' in aliases:
                alias = '<-1'
            elif is_top and 'top' in aliases:
                alias = 'top'
            elif safe_decode(floor) in aliases:
                alias = safe_decode(floor)

            floor = safe_decode(floor)

            if alias:
                alias_props = aliases.get(alias)

                # Aliases upon aliases, e.g. for something like "Upper Mezzanine"
                # where it's an alias for "1" under the half_floors key
                if safe_decode(floor) in alias_props.get('aliases', {}):
                    alias_prefix = '{}.{}.aliases'.format(alias_prefix, alias)
                    alias = safe_decode(floor)

            if alias:
                return cls.numeric_phrase('{}.{}'.format(alias_prefix, alias), floor, language,
                                          dictionaries=['level_types_basement',
                                                        'level_types_mezzanine',
                                                        'level_types_numbered',
                                                        'level_types_standalone',
                                                        'level_types_sub_basement'],
                                          country=country)

        return cls.numeric_phrase('levels.alphanumeric', floor, language,
                              dictionaries=['level_types_numbered'], country=country)