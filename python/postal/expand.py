import _expand
from postal.text.encoding import safe_decode

DEFAULT_LANGUAGES = ('en',)


def expand_address(address, languages=DEFAULT_LANGUAGES, **kw):
    '''
    @param address: the address as either Unicode or a UTF-8 encoded string
    @param languages: a tuple or list of ISO language code strings (e.g. "en", "fr", "de", etc.)
                      to use in expansion. Default is English. Until automatic language classification
                      is ready in libpostal, this parameter is required.

    '''
    address = safe_decode(address, 'utf-8')
    return _expand.expand_address(address, languages=languages, **kw)
