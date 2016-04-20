import random
import six

from geodata.addresses.numbering import NumberedComponent, sample_alphabet, latin_alphabet
from geodata.encoding import safe_decode


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
    def random_letter(cls, alphabet=latin_alphabet):
        return random.choice(alphabet)

    @classmethod
    def phrase(cls, box_number, language, country=None):
        return cls.numeric_phrase('po_boxes.alphanumeric', safe_decode(box_number), language,
                                  dictionaries=['post_office'], country=country)
