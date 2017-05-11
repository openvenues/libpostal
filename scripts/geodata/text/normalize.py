# -*- coding: utf-8 -*-
import six

from geodata.text import _normalize
from geodata.text.tokenize import tokenize_raw
from geodata.text.token_types import token_types

from geodata.encoding import safe_decode

# String options
NORMALIZE_STRING_LATIN_ASCII = _normalize.NORMALIZE_STRING_LATIN_ASCII
NORMALIZE_STRING_TRANSLITERATE = _normalize.NORMALIZE_STRING_TRANSLITERATE
NORMALIZE_STRING_STRIP_ACCENTS = _normalize.NORMALIZE_STRING_STRIP_ACCENTS
NORMALIZE_STRING_DECOMPOSE = _normalize.NORMALIZE_STRING_DECOMPOSE
NORMALIZE_STRING_COMPOSE = _normalize.NORMALIZE_STRING_COMPOSE
NORMALIZE_STRING_LOWERCASE = _normalize.NORMALIZE_STRING_LOWERCASE
NORMALIZE_STRING_TRIM = _normalize.NORMALIZE_STRING_TRIM
NORMALIZE_STRING_REPLACE_HYPHENS = _normalize.NORMALIZE_STRING_REPLACE_HYPHENS
NORMALIZE_STRING_SIMPLE_LATIN_ASCII = _normalize.NORMALIZE_STRING_SIMPLE_LATIN_ASCII

DEFAULT_STRING_OPTIONS = NORMALIZE_STRING_LATIN_ASCII |  \
    NORMALIZE_STRING_COMPOSE | \
    NORMALIZE_STRING_TRIM | \
    NORMALIZE_STRING_REPLACE_HYPHENS | \
    NORMALIZE_STRING_STRIP_ACCENTS | \
    NORMALIZE_STRING_LOWERCASE

# Token options
NORMALIZE_TOKEN_REPLACE_HYPHENS = _normalize.NORMALIZE_TOKEN_REPLACE_HYPHENS
NORMALIZE_TOKEN_DELETE_HYPHENS = _normalize.NORMALIZE_TOKEN_DELETE_HYPHENS
NORMALIZE_TOKEN_DELETE_FINAL_PERIOD = _normalize.NORMALIZE_TOKEN_DELETE_FINAL_PERIOD
NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS = _normalize.NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS
NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES = _normalize.NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES
NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE = _normalize.NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE
NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC = _normalize.NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC
NORMALIZE_TOKEN_REPLACE_DIGITS = _normalize.NORMALIZE_TOKEN_REPLACE_DIGITS

DEFAULT_TOKEN_OPTIONS = NORMALIZE_TOKEN_REPLACE_HYPHENS | \
    NORMALIZE_TOKEN_DELETE_FINAL_PERIOD | \
    NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS | \
    NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES | \
    NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE

TOKEN_OPTIONS_DROP_PERIODS = NORMALIZE_TOKEN_DELETE_FINAL_PERIOD | \
    NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS

DEFAULT_TOKEN_OPTIONS_NUMERIC = (DEFAULT_TOKEN_OPTIONS | NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC)


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
    if string_options & _normalize.NORMALIZE_STRING_LATIN_ASCII:
        normalized = _normalize.normalize_string_latin(s, string_options)
    else:
        normalized = _normalize.normalize_string_utf8(s, string_options)

    return normalized


def normalize_token(s, t, token_options=DEFAULT_TOKEN_OPTIONS):
    return _normalize.normalize_token(s, t, token_options)


def normalize_tokens_whitespace(s, raw_tokens, token_options=DEFAULT_TOKEN_OPTIONS):
    last_end = 0
    tokens = []

    for t in raw_tokens:
        t_norm = _normalize.normalize_token(s, t, token_options)
        t_class = token_types.from_id(t[-1])

        if last_end < t[0]:
            tokens.append((six.u(' '), token_types.WHITESPACE))
        last_end = sum(t[:2])

        tokens.append((t_norm, t_class))

    return tokens


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
    normalized = normalize_string(s, string_options=string_options)

    # Tuples of (offset, len, type)
    raw_tokens = tokenize_raw(normalized)
    tokens = []
    last_end = 0

    if not whitespace:
        tokens = [(_normalize.normalize_token(normalized, t, token_options),
                   token_types.from_id(t[-1])) for t in raw_tokens]
    else:
        tokens = normalize_tokens_whitespace(normalized, raw_tokens, token_options=token_options)

    if strip_parentheticals:
        return remove_parens(tokens)
    else:
        return tokens
