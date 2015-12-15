import _parser
from postal.text.encoding import safe_decode

DEFAULT_LANGUAGES = ('en',)


def parse_address(address, language=None, country=None):
    '''
    @param address: the address as either Unicode or a UTF-8 encoded string
    @param language (optional): language code
    @param country (optional): country code
    '''
    address = safe_decode(address, 'utf-8')
    return _parser.parse_address(address, language=language, country=country)
