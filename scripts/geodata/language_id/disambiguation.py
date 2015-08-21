import os
import sys

from collections import defaultdict

from marisa_trie import BytesTrie

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir, os.pardir, 'python')))

from geodata.encoding import safe_decode
from geodata.i18n.unicode_paths import DATA_DIR
from address_normalizer.text.normalize import PhraseFilter
from address_normalizer.text.tokenize import *


DICTIONARIES_DIR = os.path.join(DATA_DIR, 'dictionaries')

PREFIX_KEY = u'\x02'
SUFFIX_KEY = u'\x03'

POSSIBLE_ROMAN_NUMERALS = set(['i', 'ii', 'iii', 'iv', 'v', 'vi', 'vii', 'viii', 'ix',
                               'x', 'xi', 'xii', 'xiii', 'xiv', 'xv', 'xvi', 'xvii', 'xviii', 'xix',
                               'xx', 'xxx', 'xl', 'l', 'lx', 'lxx', 'lxxx', 'xc',
                               'c', 'cc', 'ccc', 'cd', 'd', 'dc', 'dcc', 'dccc', 'cm',
                               'm', 'mm', 'mmm', 'mmmm'])


class StreetTypesGazetteer(PhraseFilter):
    def serialize(self, s):
        return s

    def deserialize(self, s):
        return s

    def configure(self, base_dir=DICTIONARIES_DIR):
        kvs = defaultdict(OrderedDict)
        for lang in os.listdir(DICTIONARIES_DIR):
            for filename in ('street_types.txt', 'directionals.txt'):
                path = os.path.join(DICTIONARIES_DIR, lang, filename)
                if not os.path.exists(path):
                    continue

                for line in open(path):
                    line = line.strip()
                    if not line:
                        continue
                    for phrase in safe_decode(line).split(u'|'):
                        if phrase in POSSIBLE_ROMAN_NUMERALS:
                            continue
                        kvs[phrase][lang] = None
            for filename in ('concatenated_suffixes_separable.txt', 'concatenated_suffixes_inseparable.txt', 'concatenated_prefixes_separable.txt'):
                path = os.path.join(DICTIONARIES_DIR, lang, filename)
                if not os.path.exists(path):
                    continue

                for line in open(path):
                    line = line.strip()
                    if not line:
                        continue

                    for phrase in safe_decode(line).split(u'|'):
                        if 'suffixes' in filename:
                            phrase = SUFFIX_KEY + phrase[::-1]
                        else:
                            phrase = PREFIX_KEY + phrase

                        kvs[phrase][lang] = None

        kvs = [(k, v) for k, vals in kvs.iteritems() for v in vals.keys()]

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

    def basic_filter(self, tokens):
        return super(StreetTypesGazetteer, self).filter(tokens)

    def filter(self, tokens):
        for c, t, data in self.basic_filter(tokens):
            if c != token_types.PHRASE:
                token = t[1]
                token_len = len(token)
                suffix_search, suffix_len = self.search_substring(SUFFIX_KEY + token[::-1])

                if suffix_search and self.trie.get(token[token_len - (suffix_len - len(SUFFIX_KEY)):]):
                    yield (token_types.PHRASE, [(c,) + t], suffix_search)
                    continue
                prefix_search, prefix_len = self.search_substring(PREFIX_KEY + token)
                print 'prefix = ', prefix_search, token[:(prefix_len - len(PREFIX_KEY))]
                if prefix_search and self.trie.get(token[:(prefix_len - len(PREFIX_KEY))]):
                    yield (token_types.PHRASE, [(c,) + t], prefix_search)
                    continue
            yield c, t, data

street_types_gazetteer = StreetTypesGazetteer()


UNKNOWN_LANGUAGE = 'unk'
AMBIGUOUS_LANGUAGE = 'xxx'


def disambiguate_language(text, languages):
    valid_languages = OrderedDict([(l['lang'], l['default']) for l in languages])
    tokens = tokenize(safe_decode(text).replace(u'-', u' ').lower())

    current_lang = None

    for c, t, data in street_types_gazetteer.filter(tokens):
        if c == token_types.PHRASE:
            valid = [lang for lang in data if lang in valid_languages]
            if len(valid) != 1:
                continue

            phrase_lang = valid[0]
            if phrase_lang != current_lang and current_lang is not None:
                return AMBIGUOUS_LANGUAGE
            current_lang = phrase_lang

    if current_lang is not None:
        return current_lang
    return UNKNOWN_LANGUAGE
