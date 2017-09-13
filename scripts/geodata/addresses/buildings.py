import random
import six

from geodata.addresses.config import address_config
from geodata.addresses.numbering import NumberedComponent
from geodata.encoding import safe_decode

from geodata.configs.utils import nested_get
from geodata.addresses.numbering import NumberedComponent, sample_alphabet, latin_alphabet
from geodata.encoding import safe_decode
from geodata.math.sampling import weighted_choice, zipfian_distribution, cdf
from geodata.math.floats import isclose


class Building(NumberedComponent):
    max_buildings = 10
    key = 'buildings'
    dictionaries = ['building_types']

    building_range = range(1, max_buildings + 1)
    building_range_probs = zipfian_distribution(len(building_range), 2.0)
    building_range_cdf = cdf(building_range_probs)

    compound_building_max_digits = 6
    compound_building_digits_range = range(1, compound_building_max_digits + 1)
    compound_building_digits_range_probs = zipfian_distribution(len(compound_building_digits_range), 1.0)
    compound_building_digits_range_cdf = cdf(compound_building_digits_range_probs)

    @classmethod
    def sample_num_digits(cls):
        return weighted_choice(cls.compound_building_digits_range, cls.compound_building_digits_range_cdf)

    @classmethod
    def random(cls, language, country=None, num_type=None, num_type_props=None):
        if not num_type:
            num_type, num_type_props = cls.choose_alphanumeric_type(language, country=country)
            if num_type is None:
                return None

        if num_type == cls.NUMERIC:
            number = weighted_choice(cls.building_range, cls.building_range_cdf)
            return safe_decode(number)
        elif num_type == cls.HYPHENATED_NUMBER:
            number = weighted_choice(cls.building_range, cls.building_range_cdf)
            number2 = cls.random_digits(cls.sample_num_digits())

            range_prob = float(address_config.get_property('buildings.alphanumeric.hyphenated_number.range_probability', language, country=country, default=0.5))
            alpha_plus_numeric_prob = float(address_config.get_property('buildings.alphanumeric.hyphenated_number.alpha_plus_numeric_probability', language, country=country, default=0.1))
            alpha_plus_numeric_whitespace_prob = float(address_config.get_property('buildings.alphanumeric.hyphenated_number.alpha_plus_numeric_whitespace_probability', language, country=country, default=0.1))
            numeric_plus_alpha_prob = float(address_config.get_property('buildings.buildings.hyphenated_number.numeric_plus_alpha_probability', language, country=country, default=0.1))
            numeric_plus_alpha_whitespace_prob = float(address_config.get_property('units.alphanumeric.hyphenated_number.numeric_plus_alpha_whitespace_probability', language, country=country, default=0.1))
            direction = address_config.get_property('buildings.alphanumeric.hyphenated_number.direction', language, country=country, default='right')
            direction_prob = float(address_config.get_property('buildings.alphanumeric.hyphenated_number.direction_probability', language, country=country, default=0.0))

            if random.random() < direction_prob:
                direction = 'left' if direction == 'right' else 'right'

            direction_right = direction == 'right'

            if random.random() < range_prob:
                if direction_right:
                    number2 += number
                else:
                    number2 = max(0, number - number2)

            letter_choices = [cls.ALPHA_PLUS_NUMERIC, cls.NUMERIC_PLUS_ALPHA]
            letter_probs = [alpha_plus_numeric_prob, numeric_plus_alpha_prob]
            if not isclose(sum(letter_probs), 1.0):
                letter_choices.append(None)
                letter_probs.append(1.0 - sum(letter_probs))

            letter_probs_cdf = cdf(letter_probs)
            letter_type = weighted_choice(letter_choices, letter_probs_cdf)

            if letter_type is not None:
                alphabet = address_config.get_property('alphabet', language, country=country, default=latin_alphabet)
                alphabet_probability = address_config.get_property('alphabet_probability', language, country=country, default=None)
                if alphabet_probability is not None and random.random() >= alphabet_probability:
                    alphabet = latin_alphabet
                if num_type_props:
                    latin_alphabet_probability = num_type_props.get('latin_probability')
                    if latin_alphabet_probability and random.random() < latin_alphabet_probability:
                        alphabet = latin_alphabet
                letter = sample_alphabet(alphabet)

                if letter_type == cls.ALPHA_PLUS_NUMERIC:
                    whitespace_prob = alpha_plus_numeric_whitespace_prob
                    whitespace = u' ' if random.random() < whitespace_prob else u''
                    number = u'{}{}{}'.format(letter, whitespace, number)
                elif letter_type == cls.NUMERIC_PLUS_ALPHA:
                    whitespace_prob = numeric_plus_alpha_whitespace_prob
                    whitespace = u' ' if random.random() < whitespace_prob else u''
                    number2 = u'{}{}{}'.format(number2, whitespace, letter)

            if direction == 'right':
                return u'{}-{}'.format(number, number2)
            else:
                return u'{}-{}'.format(number2, number)
        elif num_type == cls.DECIMAL_NUMBER:
            whole_part = weighted_choice(cls.building_range, cls.building_range_cdf)
            decimal_part = cls.random_digits(cls.sample_num_digits())
            return u'{}.{}'.format(whole_part, decimal_part)
        else:
            alphabet = address_config.get_property('alphabet', language, country=country, default=latin_alphabet)
            alphabet_probability = address_config.get_property('alphabet_probability', language, country=country, default=None)
            if alphabet_probability is not None and random.random() >= alphabet_probability:
                alphabet = latin_alphabet
            if num_type_props:
                latin_alphabet_probability = num_type_props.get('latin_probability')
                if latin_alphabet_probability and random.random() < latin_alphabet_probability:
                    alphabet = latin_alphabet
            letter = sample_alphabet(alphabet, 2.0)
            if num_type == cls.ALPHA:
                return safe_decode(letter)
            else:
                number = weighted_choice(cls.building_range, cls.building_range_cdf)

                whitespace_probability = float(num_type_props.get('whitespace_probability', 0.0))
                whitespace_phrase = six.u(' ') if whitespace_probability and random.random() < whitespace_probability else six.u('')

                if num_type == cls.ALPHA_PLUS_NUMERIC:
                    return six.u('{}{}{}').format(letter, whitespace_phrase, number)
                elif num_type == cls.NUMERIC_PLUS_ALPHA:
                    return six.u('{}{}{}').format(number, whitespace_phrase, letter)

    @classmethod
    def phrase(cls, building, language, country=None, num_type=None):
        if building is None:
            return None

        return cls.numeric_phrase('buildings.alphanumeric', building, language,
                                  dictionaries=cls.dictionaries, country=country)
