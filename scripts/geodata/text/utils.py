import re

from geodata.text.tokenize import tokenize
from geodata.text.token_types import token_types

non_breaking_dash_regex = re.compile(u'[\-\u058a\u05be\u1400\u1806\u2010-\u2013\u2212\u2e17\u2e1a\ufe32\ufe63\uff0d]', re.UNICODE)


def is_numeric(s):
    tokens = tokenize(s)
    return sum((1 for t, c in tokens if c in token_types.NUMERIC_TOKEN_TYPES)) == len(tokens)


def is_numeric_strict(s):
    tokens = tokenize(s)
    return sum((1 for t, c in tokens if c == token_types.NUMERIC)) == len(tokens)
