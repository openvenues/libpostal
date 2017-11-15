import os
import six

from collections import defaultdict, OrderedDict

from geodata.address_expansions.address_dictionaries import address_phrase_dictionaries
from geodata.encoding import safe_decode, safe_encode
from geodata.i18n.unicode_paths import DATA_DIR
from geodata.text.normalize import normalized_tokens, normalize_string
from geodata.text.tokenize import tokenize, token_types
from geodata.text.phrases import PhraseFilter
from geodata.enum import EnumValue

from marisa_trie import BytesTrie


DICTIONARIES_DIR = os.path.join(DATA_DIR, 'dictionaries')

PREFIX_KEY = u'\x02'
SUFFIX_KEY = u'\x03'

POSSIBLE_ROMAN_NUMERALS = set(['i', 'ii', 'iii', 'iv', 'v', 'vi', 'vii', 'viii', 'ix',
                               'x', 'xi', 'xii', 'xiii', 'xiv', 'xv', 'xvi', 'xvii', 'xviii', 'xix',
                               'xx', 'xxx', 'xl', 'l', 'lx', 'lxx', 'lxxx', 'xc',
                               'c', 'cc', 'ccc', 'cd', 'd', 'dc', 'dcc', 'dccc', 'cm',
                               'm', 'mm', 'mmm', 'mmmm'])


class DictionaryPhraseFilter(PhraseFilter):
    serialize = safe_encode
    deserialize = safe_decode

    def __init__(self, *dictionaries):
        self.dictionaries = dictionaries
        self.canonicals = {}

        kvs = defaultdict(OrderedDict)

        for language in address_phrase_dictionaries.languages:
            for dictionary_name in self.dictionaries:
                is_suffix_dictionary = 'suffixes' in dictionary_name
                is_prefix_dictionary = 'prefixes' in dictionary_name

                for phrases in address_phrase_dictionaries.phrases.get((language, dictionary_name), []):
                    canonical = phrases[0]
                    canonical_normalized = normalize_string(canonical)

                    self.canonicals[(canonical, language, dictionary_name)] = phrases[1:]

                    for i, phrase in enumerate(phrases):

                        if phrase in POSSIBLE_ROMAN_NUMERALS:
                            continue

                        is_canonical = normalize_string(phrase) == canonical_normalized

                        if is_suffix_dictionary:
                            phrase = SUFFIX_KEY + phrase[::-1]
                        elif is_prefix_dictionary:
                            phrase = PREFIX_KEY + phrase

                        kvs[phrase][(language, dictionary_name, canonical)] = is_canonical

        kvs = [(k, '|'.join([l, d, str(int(i)), safe_encode(c)])) for k, vals in kvs.iteritems() for (l, d, c), i in vals.iteritems()]

        self.trie = BytesTrie(kvs)

    def serialize(self, s):
        return s

    def deserialize(self, s):
        return s

    def search_substring(self, s):
        if len(s) == 0:
            return None, 0

        for i in xrange(len(s) + 1):
            if not self.trie.has_keys_with_prefix(s[:i]):
                i -= 1
                break
        if i > 0:
            return (self.trie.get(s[:i]), i)
        else:
            return None, 0

    def search_suffix(self, token):
        suffix_search, suffix_len = self.search_substring(SUFFIX_KEY + token[::-1])
        if suffix_len > 0:
            suffix_len -= len(SUFFIX_KEY)
        return suffix_search, suffix_len

    def search_prefix(self, token):
        prefix_search, prefix_len = self.search_substring(PREFIX_KEY + token)
        if prefix_len > 0:
            prefix_len -= len(PREFIX_KEY)
        return prefix_search, prefix_len

    def basic_filter(self, tokens):
        return super(DictionaryPhraseFilter, self).filter(tokens)

    def filter(self, tokens):
        for p, t, data in self.basic_filter(tokens):
            if not p:
                t, c = t
                token = t
                token_len = len(token)

                suffix_search, suffix_len = self.search_suffix(token)
                if suffix_search and self.trie.get(token[(token_len - suffix_len):].rstrip('.')):
                    yield ([(t, c)], token_types.PHRASE, suffix_len, map(safe_decode, suffix_search))
                    continue
                prefix_search, prefix_len = self.search_prefix(token)
                if prefix_search and self.trie.get(token[:prefix_len]):
                    yield ([(t, c)], token_types.PHRASE, prefix_len, map(safe_decode, prefix_search))
                    continue
            else:
                c = token_types.PHRASE
            yield t, c, len(t), map(safe_decode, data)

    def gen_phrases(self, s, canonical_only=False, languages=None):
        tokens = tokenize(s)
        norm_tokens = [(t.lower() if c in token_types.WORD_TOKEN_TYPES else t, c) for t, c in tokens]

        if not languages:
            languages = None
        elif not hasattr(languages, '__iter__'):
            languages = [languages]

        if not hasattr(languages, '__contains__'):
            languages = set(languages)

        for t, c, length, data in self.filter(norm_tokens):
            if c == token_types.PHRASE:
                if not canonical_only and languages is None:
                    yield six.u(' ').join([t_i for t_i, c_i in t])
                else:
                    phrase = None
                    for d in data:
                        lang, dictionary, is_canonical, canonical = d.split(six.b('|'))

                        if (bool(int(is_canonical)) or not canonical_only) and (languages is None or lang in languages or lang == 'all'):
                            phrase = phrase if phrase is not None else six.u(' ').join([t_i for t_i, c_i in t])
                            yield phrase

    def string_contains_phrases(self, s, canonical_only=False, languages=None):
        phrases = self.gen_phrases(s, canonical_only=canonical_only, languages=languages)
        try:
            phrases.next()
            return True
        except StopIteration:
            return False

    def extract_phrases(self, s, canonical_only=False, languages=None):
        return set(self.gen_phrases(s, canonical_only=canonical_only, languages=languages))


STREET_TYPES_ONLY_DICTIONARIES = ('street_types',
                                  'directionals',
                                  'concatenated_suffixes_separable',
                                  'concatenated_suffixes_inseparable',
                                  'people',
                                  'personal_suffixes',
                                  'personal_titles',
                                  )

STREET_TYPES_DICTIONARIES = STREET_TYPES_ONLY_DICTIONARIES + ('concatenated_prefixes_separable',
                                                              'organizations',
                                                              'qualifiers',
                                                              'stopwords',
                                                              )

GIVEN_NAME_DICTIONARY = 'given_names'
SURNAME_DICTIONARY = 'surnames'

CHAIN_DICTIONARY = 'chains'

SYNONYM_DICTIONARY = 'synonyms'

PERSONAL_NAME_DICTIONARIES = (GIVEN_NAME_DICTIONARY,
                              SURNAME_DICTIONARY,)


NAME_DICTIONARIES = STREET_TYPES_DICTIONARIES + ('academic_degrees',
                                                 'building_types',
                                                 'company_types',
                                                 'place_names',
                                                 'qualifiers',
                                                 'synonyms',
                                                 'toponyms',
                                                 )

QUALIFIERS_DICTIONARY = 'qualifiers'

HOUSE_NUMBER_DICTIONARIES = ('house_number', 'no_number')

POSTCODE_DICTIONARIES = ('postcode',)

TOPONYMS_DICTIONARY = 'toponyms'

TOPONYM_ABBREVIATION_DICTIONARIES = ('qualifiers',
                                     'directionals',
                                     'personal_titles',
                                     'synonyms',
                                     )


UNIT_ABBREVIATION_DICTIONARIES = ('level_types_basement',
                                  'level_types_mezzanine',
                                  'level_types_numbered',
                                  'level_types_standalone',
                                  'level_types_sub_basement',
                                  'number',
                                  'post_office',
                                  'unit_types_numbered',
                                  'unit_types_standalone',
                                  )

VENUE_NAME_DICTIONARIES = ('academic_degrees',
                           'building_types',
                           'chains',
                           'company_types',
                           'directionals',
                           'given_names',
                           'organizations',
                           'people',
                           'personal_suffixes',
                           'personal_titles',
                           'place_names',
                           'stopwords',
                           'surnames',
                           )

ALL_ABBREVIATION_DICTIONARIES = STREET_TYPES_DICTIONARIES + \
    NAME_DICTIONARIES + \
    UNIT_ABBREVIATION_DICTIONARIES + \
    ('no_number', 'nulls',)


_gazetteers = []


def create_gazetteer(*dictionaries):
    g = DictionaryPhraseFilter(*dictionaries)
    _gazetteers.append(g)
    return g


street_types_gazetteer = create_gazetteer(*STREET_TYPES_DICTIONARIES)
street_types_only_gazetteer = create_gazetteer(*STREET_TYPES_ONLY_DICTIONARIES)
qualifiers_gazetteer = create_gazetteer(QUALIFIERS_DICTIONARY)
names_gazetteer = create_gazetteer(*NAME_DICTIONARIES)
chains_gazetteer = create_gazetteer(CHAIN_DICTIONARY)
unit_types_gazetteer = create_gazetteer(*UNIT_ABBREVIATION_DICTIONARIES)
street_and_synonyms_gazetteer = create_gazetteer(*(STREET_TYPES_DICTIONARIES + (SYNONYM_DICTIONARY, )))
abbreviations_gazetteer = create_gazetteer(*ALL_ABBREVIATION_DICTIONARIES)
toponym_abbreviations_gazetteer = create_gazetteer(*TOPONYM_ABBREVIATION_DICTIONARIES)
toponym_gazetteer = create_gazetteer(TOPONYMS_DICTIONARY)
given_name_gazetteer = create_gazetteer(GIVEN_NAME_DICTIONARY)
venue_names_gazetteer = create_gazetteer(*VENUE_NAME_DICTIONARIES)
