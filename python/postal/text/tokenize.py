from postal.text.encoding import safe_encode, safe_decode
from postal.text import _tokenize
from postal.text.token_types import token_types


def tokenize_raw(s):
    return _tokenize.tokenize(safe_decode(s))


def tokenize(s):
    u = safe_decode(s)
    s = safe_encode(s)
    return [(safe_decode(s[start:start + length]), token_types.from_id(token_type))
            for start, length, token_type in _tokenize.tokenize(u)]
