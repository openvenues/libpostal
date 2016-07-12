import random
import six

from geodata.addresses.config import address_config
from geodata.addresses.numbering import NumberedComponent
from geodata.encoding import safe_decode

from geodata.configs.utils import nested_get
from geodata.addresses.numbering import NumberedComponent, sample_alphabet, latin_alphabet
from geodata.encoding import safe_decode
from geodata.math.sampling import weighted_choice, zipfian_distribution, cdf


class Block(NumberedComponent):
    max_blocks = 10

    block_range = range(1, max_blocks + 1)
    block_range_probs = zipfian_distribution(len(block_range), 2.0)
    block_range_cdf = cdf(block_range_probs)

    @classmethod
    def random(cls, language, country=None):
        num_type, num_type_props = cls.choose_alphanumeric_type('blocks.alphanumeric', language, country=country)
        if num_type is None:
            return None

        if num_type == cls.NUMERIC:
            number = weighted_choice(cls.block_range, cls.block_range_cdf)
            return safe_decode(number)
        else:
            alphabet = address_config.get_property('alphabet', language, country=country, default=latin_alphabet)
            alphabet_probability = address_config.get_property('alphabet_probability', language, country=country, default=None)
            if alphabet_probability is not None and random.random() >= alphabet_probability:
                alphabet = latin_alphabet
            letter = sample_alphabet(alphabet, 2.0)
            if num_type == cls.ALPHA:
                return safe_decode(letter)
            else:
                number = weighted_choice(cls.block_range, cls.block_range_cdf)

                whitespace_probability = float(num_type_props.get('whitespace_probability', 0.0))
                whitespace_phrase = six.u(' ') if whitespace_probability and random.random() < whitespace_probability else six.u('')

                if num_type == cls.ALPHA_PLUS_NUMERIC:
                    return six.u('{}{}{}').format(letter, whitespace_phrase, number)
                elif num_type == cls.NUMERIC_PLUS_ALPHA:
                    return six.u('{}{}{}').format(number, whitespace_phrase, letter)

    @classmethod
    def phrase(cls, block, language, country=None):
        if block is None:
            return None

        phrase_prob = address_config.get_property('blocks.alphanumeric_phrase_probability', language, country=country, default=0.0)
        if random.random() < phrase_prob:
            return cls.numeric_phrase('blocks.alphanumeric', block, language,
                                      dictionaries=['qualifiers'], country=country)
        else:
            return None