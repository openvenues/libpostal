from geodata.text.tokenize import tokenize
from geodata.text.token_types import token_types


def is_numeric(s):
    tokens = tokenize(s)
    return sum((1 for t, c in tokens if c in token_types.NUMERIC_TOKEN_TYPES)) == len(tokens)


def is_numeric_strict(s):
    tokens = tokenize(s)
    return sum((1 for t, c in tokens if c == token_types.NUMERIC)) == len(tokens)
