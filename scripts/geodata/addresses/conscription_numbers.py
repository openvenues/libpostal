import random

from geodata.addresses.config import address_config
from geodata.addresses.numbering import NumberedComponent
from geodata.encoding import safe_decode


class ConscriptionNumber(NumberedComponent):
    @classmethod
    def phrase(cls, number, language, country=None):
        if number is None:
            return number

        key = 'conscription_numbers.alphanumeric'
        dictionaries = ['house_numbers']
        default = safe_decode(number)

        return cls.numeric_phrase(key, safe_decode(number), language,
                                  dictionaries=dictionaries, country=country)
