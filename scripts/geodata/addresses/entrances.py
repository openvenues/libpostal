import random
import six

from geodata.addresses.config import address_config
from geodata.addresses.numbering import NumberedComponent
from geodata.encoding import safe_decode

from geodata.configs.utils import nested_get
from geodata.addresses.directions import RelativeDirection
from geodata.addresses.floors import Floor
from geodata.addresses.numbering import NumberedComponent, Digits, sample_alphabet, latin_alphabet
from geodata.encoding import safe_decode
from geodata.math.sampling import weighted_choice, zipfian_distribution, cdf


class Entrance(NumberedComponent):
    max_entrances = 10

    entrance_range = range(1, max_entrances + 1)
    entrance_range_probs = zipfian_distribution(len(entrance_range), 2.0)
    entrance_range_cdf = cdf(entrance_range_probs)

    @classmethod
    def random(cls, language, country=None):
        num_type, num_type_props = cls.choose_alphanumeric_type('entrances.alphanumeric', language, country=country)
        if num_type is None:
            return None

        if num_type == cls.NUMERIC:
            number = weighted_choice(cls.entrance_range, cls.entrance_range_cdf)
            return safe_decode(number)
        elif num_type == cls.HYPHENATED_NUMBER:
            number = weighted_choice(cls.entrance_range, cls.entrance_range_cdf)
            number2 = number + weighted_choice(cls.entrance_range, cls.entrance_range_cdf)
            return u'{}-{}'.format(number, number2)
        else:
            alphabet = address_config.get_property('alphabet', language, country=country, default=latin_alphabet)
            alphabet_probability = address_config.get_property('alphabet_probability', language, country=country, default=None)
            if alphabet_probability is not None and random.random() >= alphabet_probability:
                alphabet = latin_alphabet
            letter = sample_alphabet(alphabet, 2.0)
            if num_type == cls.ALPHA:
                return safe_decode(letter)
            else:
                number = weighted_choice(cls.entrance_range, cls.entrance_range_cdf)

                whitespace_probability = float(num_type_props.get('whitespace_probability', 0.0))
                hyphen_probability = float(num_type_props.get('hyphen_probability', 0.0))
                whitespace_phrase = u''
                r = random.random()
                if r < whitespace_probability:
                    whitespace_phrase = u' '
                elif r < (whitespace_probability + hyphen_probability):
                    whitespace_phrase = u'-'

                if num_type == cls.ALPHA_PLUS_NUMERIC:
                    return six.u('{}{}{}').format(letter, whitespace_phrase, number)
                elif num_type == cls.NUMERIC_PLUS_ALPHA:
                    return six.u('{}{}{}').format(number, whitespace_phrase, letter)

    @classmethod
    def phrase(cls, entrance, language, country=None):
        if entrance is None:
            return None
        return cls.numeric_phrase('entrances.alphanumeric', entrance, language,
                                  dictionaries=['entrances'], country=country)
