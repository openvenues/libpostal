import random

from geodata.addresses.config import address_config
from geodata.address_expansions.gazetteers import chains_gazetteer
from geodata.categories.query import *
from geodata.text.normalize import normalized_tokens
from geodata.text.tokenize import tokenize, token_types


class Chain(object):
    @classmethod
    def tokenize_name(cls, name):
        if not name:
            return []
        tokens = normalized_tokens(name)
        return tokens

    @classmethod
    def possible_chain(cls, name):
        '''
        Determines if a venue name contains the name of a known chain store.

        Returns a tuple of:

        (True/False, known chain phrases, other tokens)

        Handles cases like "Hard Rock Cafe Times Square" and allows for downstream
        decision making (i.e. if the tokens have a low IDF in the local area we might
        want to consider it a chain).
        '''
        tokens = cls.tokenize_name(name)
        if not tokens:
            return False
        matches = chains_gazetteer.filter(tokens)
        other_tokens = []
        phrases = []
        for t, c, l, d in matches:
            if c == token_types.PHRASE:
                phrases.append((t, c, l, d))
            else:
                other_tokens.append((t, c))

        return len(phrases) > 0, phrases, other_tokens if len(phrases) > 0 else []

    @classmethod
    def extract(cls, name):
        '''
        Determines if an entire venue name matches a known chain store.

        Note: to avoid false positives, only return True if all of the tokens
        in the venue's name are part of a single chain store phrase. This will
        miss a few things like "Hard Rock Cafe Times Square" and the like.

        It will however handle compound chain stores like Subway/Taco Bell
        '''

        possible, phrases, other_tokens = cls.possible_chain(name)
        is_chain = possible and not any((c in token_types.WORD_TOKEN_TYPES for t, c in other_tokens))
        return is_chain, phrases if is_chain else []

    @classmethod
    def alternate_form(cls, language, dictionary, canonical):
        choices = address_config.sample_phrases.get((language, dictionary), {}).get(canonical)
        if not choices:
            return canonical
        return random.choice(choices)
