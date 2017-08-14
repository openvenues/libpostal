# -*- coding: utf-8 -*-
import six

from geodata.text import _normalize
from geodata.text.token_types import token_types

from geodata.encoding import safe_decode

# String options
NORMALIZE_STRING_LATIN_ASCII = _normalize.NORMALIZE_STRING_LATIN_ASCII
NORMALIZE_STRING_TRANSLITERATE = _normalize.NORMALIZE_STRING_TRANSLITERATE
NORMALIZE_STRING_STRIP_ACCENTS = _normalize.NORMALIZE_STRING_STRIP_ACCENTS
NORMALIZE_STRING_DECOMPOSE = _normalize.NORMALIZE_STRING_DECOMPOSE
NORMALIZE_STRING_LOWERCASE = _normalize.NORMALIZE_STRING_LOWERCASE
NORMALIZE_STRING_TRIM = _normalize.NORMALIZE_STRING_TRIM
NORMALIZE_STRING_REPLACE_HYPHENS = _normalize.NORMALIZE_STRING_REPLACE_HYPHENS
NORMALIZE_STRING_SIMPLE_LATIN_ASCII = _normalize.NORMALIZE_STRING_SIMPLE_LATIN_ASCII

DEFAULT_STRING_OPTIONS = _normalize.NORMALIZE_DEFAULT_STRING_OPTIONS

# Token options
NORMALIZE_TOKEN_REPLACE_HYPHENS = _normalize.NORMALIZE_TOKEN_REPLACE_HYPHENS
NORMALIZE_TOKEN_DELETE_HYPHENS = _normalize.NORMALIZE_TOKEN_DELETE_HYPHENS
NORMALIZE_TOKEN_DELETE_FINAL_PERIOD = _normalize.NORMALIZE_TOKEN_DELETE_FINAL_PERIOD
NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS = _normalize.NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS
NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES = _normalize.NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES
NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE = _normalize.NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE
NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC = _normalize.NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC
NORMALIZE_TOKEN_REPLACE_DIGITS = _normalize.NORMALIZE_TOKEN_REPLACE_DIGITS

DEFAULT_TOKEN_OPTIONS = _normalize.NORMALIZE_DEFAULT_TOKEN_OPTIONS

TOKEN_OPTIONS_DROP_PERIODS = _normalize.NORMALIZE_TOKEN_OPTIONS_DROP_PERIODS
DEFAULT_TOKEN_OPTIONS_NUMERIC = _normalize.NORMALIZE_DEFAULT_TOKEN_OPTIONS_NUMERIC


def remove_parens(tokens):
    new_tokens = []
    open_parens = 0
    for t, c in tokens:
        if c == token_types.PUNCT_OPEN:
            open_parens += 1
        elif c == token_types.PUNCT_CLOSE:
            if open_parens > 0:
                open_parens -= 1
        elif open_parens <= 0:
            new_tokens.append((t, c))
    return new_tokens


def normalize_string(s, string_options=DEFAULT_STRING_OPTIONS):
    s = safe_decode(s)
    return _normalize.normalize_string(s, string_options)


def normalized_tokens(s, string_options=DEFAULT_STRING_OPTIONS,
                      token_options=DEFAULT_TOKEN_OPTIONS,
                      strip_parentheticals=True, whitespace=False):
    '''
    Normalizes a string, tokenizes, and normalizes each token
    with string and token-level options.

    This version only uses libpostal's deterministic normalizations
    i.e. methods with a single output. The string tree version will
    return multiple normalized strings, each with tokens.

    Usage:
        normalized_tokens(u'St.-Barthélemy')
    '''
    s = safe_decode(s)
    normalized_tokens = _normalize.normalized_tokens(s, string_options, token_options, whitespace)

    if strip_parentheticals:
        normalized_tokens = remove_parens(normalized_tokens)

    return [(s, token_types.from_id(token_type)) for s, token_type in normalized_tokens]
