import random
import six

from geodata.addresses.config import address_config
from geodata.addresses.numbering import NumberedComponent, sample_alphabet, latin_alphabet
from geodata.addresses.sampling import weighted_choice, zipfian_distribution, cdf
from geodata.encoding import safe_decode


class Floor(NumberedComponent):
    # When we don't know the number of floors, use a Zipfian distribution
    # to choose randomly between 1 and max_floors with 1 being much more
    # likely than 2, etc.
    max_floors = 10
    max_basements = 2
    numbered_floors = range(max_floors + 1) + range(-1, -max_basements - 1, -1)
    floor_probs = zipfian_distribution(len(numbered_floors), 2.0)
    floor_probs_cdf = cdf(floor_probs)

    # For use with numbers e.g. A0 is probably not as common
    positive_floors = range(1, max_floors + 1) + [0]
    positive_floors_cdf = cdf(positive_floors)

    @classmethod
    def sample_floors(cls, num_floors, num_basements=0):
        num_floors = int(num_floors)
        return random.randint(-num_basements, num_floors - 1)

    @classmethod
    def random_floor(cls, language, country=None, num_floors=None, num_basements=None):
        level_props = address_config.get_property('levels.alphanumeric', language, country=country)
        numbering_starts_at = int(address_config.get_property('levels.numbering_starts_at', language, country=country, default=0))

        values = []
        probs = []

        for num_type in ('numeric', 'alpha', 'alpha_plus_numeric', 'numeric_plus_alpha'):
            key = '{}_probability'.format(num_type)
            prob = level_props.get(key)
            if prob is not None:
                values.append(num_type)
                probs.append(prob)

        probs = cdf(probs)
        num_type = weighted_choice(values, probs)

        if num_floors is not None:
            number = cls.sample_floors(num_floors, num_basements or 0)
        else:
            number = weighted_choice(cls.numbered_floors, cls.floor_probs_cdf)

        if number >= 0:
            number += numbering_starts_at

        if num_type == 'numeric':
            return safe_decode(number)
        else:
            alphabet = address_config.get_property('alphabet', language, country=country, default=latin_alphabet)
            letter = sample_alphabet(alphabet)
            if num_type == 'alpha':
                return letter
            else:
                if number < numbering_starts_at:
                    number = random.randint(numbering_starts_at, num_floors or cls.max_floors)

                if num_type == 'alpha_plus_numeric':
                    return six.u('{}{}').format(letter, number)
                elif num_type == 'numeric_plus_alpha':
                    return six.u('{}{}').format(number, letter)

    @classmethod
    def phrase(cls, floor, language, country=None, is_top=False):
        floor = safe_decode(floor)
        try:
            floor = int(floor)
            integer_floor = True
        except ValueError:
            try:
                floor = float(floor)
                integer_floor = int(floor) == floor
            except ValueError:
                return cls.numeric_phrase('levels.alphanumeric', safe_decode(floor), language,
                                          dictionaries=['level_types_numbered'], country=country)

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
                return cls.numeric_phrase('{}.{}'.format(alias_prefix, alias), safe_decode(floor), language,
                                          dictionaries=['level_types_basement',
                                                        'level_types_mezzanine',
                                                        'level_types_numbered',
                                                        'level_types_standalone',
                                                        'level_types_sub_basement'],
                                          country=country)

        return cls.numeric_phrase('levels.alphanumeric', safe_decode(floor), language,
                                  dictionaries=['level_types_numbered'], country=country)
