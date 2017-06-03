import random

from geodata.addresses.config import address_config
from geodata.addresses.numbering import NumberedComponent
from geodata.encoding import safe_decode


class ConscriptionNumber(NumberedComponent):
    key = 'conscription_numbers'
    dictionaries = ['house_numbers']

    @classmethod
    def phrase(cls, number, language, country=None):
        if number is None:
            return number

        key = 'conscription_numbers.alphanumeric'
        default = safe_decode(number)

        return cls.numeric_phrase(key, safe_decode(number), language,
                                  dictionaries=cls.dictionaries, country=country)
