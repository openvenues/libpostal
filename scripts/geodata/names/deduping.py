from geodata.text.normalize import *
from geodata.names.similarity import soft_tfidf_similarity, jaccard_similarity

from collections import Counter


class NameDeduper(object):
    '''
    Base class for deduping geographic entity names e.g. for matching names
    from different databases (concordances).

    By default uses Soft TFIDF similarity (see geodata.names.similarity)
    for non-ideographic names and Jaccard similarity with word frequencies
    for ideographic names.

    See class attributes for options.
    '''

    stopwords = set()
    '''Set of words which should not be considered in similarity'''

    discriminative_words = set()
    '''Set of words which break similarity e.g. North, Heights'''

    discriminative_categories = token_types.NUMERIC_TOKEN_TYPES
    '''Set of categories which, if not contained in both sets, break similarity'''

    content_categories = token_types.WORD_TOKEN_TYPES | token_types.NUMERIC_TOKEN_TYPES
    '''Set of categories representing content tokens (default setting ignores punctuation)'''

    replacements = {}
    '''Dictionary of lowercased token replacements e.g. {u'saint': u'st'}'''

    dupe_threshold = 0.9
    '''Similarity threshold above which entities are considered dupes'''

    ignore_parentheticals = True
    '''Whether to ignore parenthetical phrases e.g. "Kangaroo Point (NSW)"'''

    @classmethod
    def tokenize(cls, s):
        return normalized_tokens(s)

    @classmethod
    def content_tokens(cls, s):
        tokens = cls.tokenize(s)
        if cls.ignore_parentheticals:
            tokens = remove_parens(tokens)
        return [(cls.replacements.get(t, t), c)
                for t, c in tokens
                if c in cls.content_categories and
                t not in cls.stopwords]

    @classmethod
    def possible_match(cls, tokens1, tokens2):
        if not cls.discriminative_categories and not cls.discriminative_words:
            return True

        intersection = set([t for t, c in tokens1]) & set([t for t, c in tokens2])
        invalid = any((True for t, c in tokens1 + tokens2
                      if t not in intersection and
                      (c in cls.discriminative_categories or t in cls.discriminative_words)
                       ))
        return not invalid

    @classmethod
    def compare_ideographs(cls, s1, s2):
        tokens1 = cls.content_tokens(s1)
        tokens2 = cls.content_tokens(s2)

        if not cls.possible_match(tokens1, tokens2):
            return 0.0

        tokens1_only = [t for t, c in tokens1]
        tokens2_only = [t for t, c in tokens2]

        if u''.join(tokens1_only) == u''.join(tokens2_only):
            return 1.0
        else:
            # Many Han/Hangul characters are common, shouldn't use IDF
            return jaccard_similarity(tokens1_only, tokens2_only)

    @classmethod
    def compare(cls, s1, s2, idf):
        tokens1 = cls.content_tokens(s1)
        tokens2 = cls.content_tokens(s2)

        if not cls.possible_match(tokens1, tokens2):
            return 0.0

        tokens1_only = [t for t, c in tokens1]
        tokens2_only = [t for t, c in tokens2]

        # Test exact equality, also handles things like Cabbage Town == Cabbagetown
        if u''.join(tokens1_only) == u''.join(tokens2_only):
            return 1.0
        else:
            return soft_tfidf_similarity(tokens1_only, tokens2_only, idf)

    @classmethod
    def is_dupe(cls, sim):
        return sim >= cls.dupe_threshold
