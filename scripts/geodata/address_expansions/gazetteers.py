import os
import sys

from collections import defaultdict, OrderedDict

from geodata.encoding import safe_decode, safe_encode
from geodata.i18n.unicode_paths import DATA_DIR
from geodata.text.normalize import normalized_tokens, normalize_string
from geodata.text.tokenize import tokenize, token_types
from geodata.text.phrases import PhraseFilter

from marisa_trie import BytesTrie


DICTIONARIES_DIR = os.path.join(DATA_DIR, 'dictionaries')

PREFIX_KEY = u'\x02'
SUFFIX_KEY = u'\x03'

POSSIBLE_ROMAN_NUMERALS = set(['i', 'ii', 'iii', 'iv', 'v', 'vi', 'vii', 'viii', 'ix',
                               'x', 'xi', 'xii', 'xiii', 'xiv', 'xv', 'xvi', 'xvii', 'xviii', 'xix',
                               'xx', 'xxx', 'xl', 'l', 'lx', 'lxx', 'lxxx', 'xc',
                               'c', 'cc', 'ccc', 'cd', 'd', 'dc', 'dcc', 'dccc', 'cm',
                               'm', 'mm', 'mmm', 'mmmm'])

PHRASE = 'PHRASE'


class DictionaryPhraseFilter(PhraseFilter):

    def __init__(self, *dictionaries):
        self.dictionaries = dictionaries
        self.canonicals = {}

    def serialize(self, s):
        return s

    def deserialize(self, s):
        return s

    def configure(self, base_dir=DICTIONARIES_DIR):
        kvs = defaultdict(OrderedDict)
        for lang in os.listdir(DICTIONARIES_DIR):
            for filename in self.dictionaries:
                is_suffix_dictionary = 'suffixes' in filename
                is_prefix_dictionary = 'prefixes' in filename

                dictionary_name = filename.split('.', 1)[0]

                path = os.path.join(DICTIONARIES_DIR, lang, filename)
                if not os.path.exists(path):
                    continue

                for line in open(path):
                    line = line.strip()
                    if not line:
                        continue

                    phrases = safe_decode(line).split(u'|')
                    if not phrases:
                        continue

                    canonical = phrases[0]
                    canonical_normalized = normalize_string(canonical)

                    self.canonicals[(canonical, lang, dictionary_name)] = phrases[1:]

                    for i, phrase in enumerate(phrases):

                        if phrase in POSSIBLE_ROMAN_NUMERALS:
                            continue

                        is_canonical = normalize_string(phrase) == canonical_normalized

                        if is_suffix_dictionary:
                            phrase = SUFFIX_KEY + phrase[::-1]
                        elif is_prefix_dictionary:
                            phrase = PREFIX_KEY + phrase

                        kvs[phrase][(lang, dictionary_name, canonical)] = is_canonical

        kvs = [(k, '|'.join([l, d, str(int(i)), safe_encode(c)])) for k, vals in kvs.iteritems() for (l, d, c), i in vals.iteritems()]

        self.trie = BytesTrie(kvs)
        self.configured = True

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
                    yield ([(t, c)], PHRASE, suffix_len, map(safe_decode, suffix_search))
                    continue
                prefix_search, prefix_len = self.search_prefix(token)
                if prefix_search and self.trie.get(token[:prefix_len]):
                    yield ([(t, c)], PHRASE, prefix_len, map(safe_decode, prefix_search))
                    continue
            else:
                c = PHRASE
            yield t, c, len(t), map(safe_decode, data)

STREET_TYPES_DICTIONARIES = ('street_types.txt',
                             'directionals.txt',
                             'concatenated_suffixes_separable.txt',
                             'concatenated_suffixes_inseparable.txt',
                             'concatenated_prefixes_separable.txt',
                             'organizations.txt',
                             'people.txt',
                             'personal_suffixes.txt',
                             'personal_titles.txt',
                             'qualifiers.txt',
                             'stopwords.txt',)

GIVEN_NAME_DICTIONARY = 'given_names.txt'
SURNAME_DICTIONARY = 'surnames.txt'

NAME_DICTIONARIES = (GIVEN_NAME_DICTIONARY,
                     SURNAME_DICTIONARY,)



NAME_ABBREVIATION_DICTIONARIES = STREET_TYPES_DICTIONARIES + ('academic_degrees.txt',
                                                              'building_types.txt',
                                                              'company_types.txt',
                                                              'place_names.txt',
                                                              'qualifiers.txt',
                                                              'synonyms.txt',
                                                              'toponyms.txt',
                                                              )


UNIT_ABBREVIATION_DICTIONARIES = ('level_types.txt',
                                  'post_office.txt',
                                  'unit_types.txt',
                                  )


ALL_ABBREVIATION_DICTIONARIES = STREET_TYPES_DICTIONARIES + \
    NAME_ABBREVIATION_DICTIONARIES + \
    UNIT_ABBREVIATION_DICTIONARIES + \
    ('no_number.txt', 'nulls.txt',)


_gazetteers = []


def create_gazetteer(*dictionaries):
    g = DictionaryPhraseFilter(*dictionaries)
    _gazetteers.append(g)
    return g


street_types_gazetteer = create_gazetteer(*STREET_TYPES_DICTIONARIES)
names_gazetteer = create_gazetteer(*NAME_ABBREVIATION_DICTIONARIES)
unit_types_gazetteer = create_gazetteer(*UNIT_ABBREVIATION_DICTIONARIES)
street_and_unit_types_gazetteer = create_gazetteer(*(STREET_TYPES_DICTIONARIES + UNIT_ABBREVIATION_DICTIONARIES))
abbreviations_gazetteer = create_gazetteer(*ALL_ABBREVIATION_DICTIONARIES)
given_name_gazetteer = create_gazetteer(GIVEN_NAME_DICTIONARY)


def init_gazetteers():
    for g in _gazetteers:
        g.configure()
