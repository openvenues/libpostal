import random
import six

from collections import namedtuple

from geodata.addresses.config import address_config
from geodata.address_expansions.gazetteers import chains_gazetteer
from geodata.categories.config import category_config
from geodata.categories.preposition import CategoryPreposition
from geodata.math.sampling import weighted_choice, cdf
from geodata.text.normalize import normalized_tokens
from geodata.text.tokenize import tokenize, token_types
from geodata.encoding import safe_decode

ChainQuery = namedtuple('ChainQuery', 'name, prep, add_place_name, add_address')

NULL_CHAIN_QUERY = ChainQuery(None, None, False, False)


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
            return False, [], []
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

    @classmethod
    def phrase(cls, chain, language, country=None):
        if not chain:
            return NULL_CHAIN_QUERY

        chain_phrase = safe_decode(chain)

        prep_phrase_type = CategoryPreposition.random(language, country=country)

        if prep_phrase_type in (None, CategoryPreposition.NULL):
            return ChainQuery(chain_phrase, prep=None, add_place_name=True, add_address=True)

        values, probs = address_config.alternative_probabilities('categories.{}'.format(prep_phrase_type), language, country=country)
        if not values:
            return ChainQuery(chain_phrase, prep=None, add_place_name=True, add_address=True)

        prep_phrase, prep_phrase_props = weighted_choice(values, probs)
        prep_phrase = safe_decode(prep_phrase)

        add_address = prep_phrase_type not in (CategoryPreposition.NEARBY, CategoryPreposition.NEAR_ME, CategoryPreposition.IN)
        add_place_name = prep_phrase_type not in (CategoryPreposition.NEARBY, CategoryPreposition.NEAR_ME)

        return ChainQuery(chain_phrase, prep=prep_phrase, add_place_name=add_place_name, add_address=add_address)
