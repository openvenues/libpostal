from geodata.addresses.numbering import NumberedComponent
from geodata.encoding import safe_decode


class PostCode(NumberedComponent):
    @classmethod
    def phrase(cls, postcode, language, country=None):
        if postcode is None:
            return None
        return cls.numeric_phrase('postcodes.alphanumeric', postcode, language,
                                  dictionaries=['postcodes'], country=country)
