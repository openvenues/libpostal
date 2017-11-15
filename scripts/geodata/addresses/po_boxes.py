import random
import six

from geodata.addresses.config import address_config
from geodata.addresses.numbering import NumberedComponent, Digits, sample_alphabet, latin_alphabet
from geodata.encoding import safe_decode
from geodata.math.sampling import cdf, weighted_choice


class POBox(NumberedComponent):
    @classmethod
    def random_digits(cls, num_digits):
        # Note: PO Boxes can have leading zeros but not important for the parser
        # since it only cares about how many digits there are in a number
        low = 10 ** (num_digits - 1)
        high = (10 ** num_digits) - 1

        return random.randint(low, high)

    @classmethod
    def random_digits_with_prefix(cls, num_digits, prefix=six.u('')):
        return six.u('').join([prefix, safe_decode(cls.random_digits(num_digits))])

    @classmethod
    def random_digits_with_suffix(cls, num_digits, suffix=six.u('')):
        return six.u('').join([safe_decode(cls.random_digits(num_digits)), suffix])

    @classmethod
    def random_letter(cls, language, country=None):
        alphabet = address_config.get_property('alphabet', language, country=country, default=latin_alphabet)
        return sample_alphabet(alphabet)

    @classmethod
    def random(cls, language, country=None):
        num_type, num_type_props = cls.choose_alphanumeric_type('po_boxes.alphanumeric', language, country=country)
        if num_type is None:
            return None

        if num_type != cls.ALPHA:
            digit_config = address_config.get_property('po_boxes.digits', language, country=country, default=[])
            values = []
            probs = []

            for val in digit_config:
                values.append(val['length'])
                probs.append(val['probability'])

            probs = cdf(probs)

            num_digits = weighted_choice(values, probs)

            digits = cls.random_digits(num_digits)
            number = Digits.rewrite(digits, language, num_type_props)


            if num_type == cls.NUMERIC:
                return safe_decode(number)
            else:
                letter = cls.random_letter(language, country=country)

                whitespace_probability = float(num_type_props.get('whitespace_probability', 0.0))
                whitespace_phrase = six.u(' ') if whitespace_probability and random.random() < whitespace_probability else six.u('')

                if num_type == cls.ALPHA_PLUS_NUMERIC:
                    return six.u('{}{}{}').format(letter, whitespace_phrase, number)
                elif num_type == cls.NUMERIC_PLUS_ALPHA:
                    return six.u('{}{}{}').format(number, whitespace_phrase, letter)
        else:
            return cls.random_letter(language, country=country)

    @classmethod
    def phrase(cls, box_number, language, country=None):
        if box_number is None:
            return None
        return cls.numeric_phrase('po_boxes.alphanumeric', safe_decode(box_number), language,
                                  dictionaries=['post_office'], country=country)
