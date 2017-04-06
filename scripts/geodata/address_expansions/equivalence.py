import random
import re
import six

from itertools import izip

from geodata.address_expansions.gazetteers import *
from geodata.encoding import safe_decode, safe_encode
from geodata.text.normalize import normalized_tokens
from geodata.text.tokenize import tokenize_raw, token_types
from geodata.text.utils import non_breaking_dash_regex


def canonicals_for_language(data, language):
    canonicals = set()

    for d in data:
        lang, dictionary, is_canonical, canonical = d.split(six.b('|'))
        if language is None or lang == language:
            canonicals.add(canonical)

    return canonicals

def equivalent(s1, s2, gazetteer, language):
    '''
    Address/place equivalence
    -------------------------

    OSM discourages abbreviations, but to make our training data map better
    to real-world input, we can safely replace the canonical phrase with an
    abbreviated version and retain the meaning of the words
    '''

    tokens_s1 = normalized_tokens(s1)
    tokens_s2 = normalized_tokens(s2)

    abbreviated_s1 = list(abbreviations_gazetteer.filter(tokens_s1))
    abbreviated_s2 = list(abbreviations_gazetteer.filter(tokens_s2))

    if len(abbreviated_s1) != len(abbreviated_s2):
        return False

    for ((t1, c1, l1, d1), (t2, c2, l2, d2)) in izip(abbreviated_s1, abbreviated_s2):
        if c1 != token_types.PHRASE and c2 != token_types.PHRASE:
            if t1 != t2:
                return False
        elif c2 == token_types.PHRASE and c2 == token_types.PHRASE:
            canonicals_s1 = canonicals_for_language(d1, language)
            canonicals_s2 = canonicals_for_language(d2, language)

            if not canonicals_s1 & canonicals_s2:
                return False
        else:
            return False

    return True
