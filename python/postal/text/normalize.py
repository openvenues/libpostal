from postal.text import _normalize
from postal.text import _tokenize

from postal.text.encoding import safe_decode

DEFAULT_STRING_OPTIONS = _normalize.NORMALIZE_STRING_LATIN_ASCII |  \
    _normalize.NORMALIZE_STRING_DECOMPOSE | \
    _normalize.NORMALIZE_STRING_TRIM | \
    _normalize.NORMALIZE_STRING_LOWERCASE

DEFAULT_TOKEN_OPTIONS = _normalize.NORMALIZE_TOKEN_REPLACE_HYPHENS | \
    _normalize.NORMALIZE_TOKEN_DELETE_FINAL_PERIOD | \
    _normalize.NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS | \
    _normalize.NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES | \
    _normalize.NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE | \
    _normalize.NORMALIZE_TOKEN_REPLACE_DIGITS


def normalized_tokens(s, string_options=DEFAULT_STRING_OPTIONS,
                      token_options=DEFAULT_TOKEN_OPTIONS):
    s = safe_decode(s)
    if string_options & _normalize.NORMALIZE_STRING_LATIN_ASCII:
        normalized = _normalize.normalize_string_latin(s, string_options)
    else:
        normalized = _normalize.normalize_string_utf8(s, string_options)

    # Tuples of (offset, len, type)
    tokens = _tokenize.tokenize(normalized)
    return [_normalize.normalize_token(normalized, t, token_options) for t in tokens]
