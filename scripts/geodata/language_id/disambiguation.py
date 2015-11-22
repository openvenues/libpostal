import os
import sys

from collections import defaultdict, OrderedDict

from marisa_trie import BytesTrie

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir, os.pardir, 'python')))

from address_normalizer.text.normalize import PhraseFilter
from geodata.encoding import safe_decode
from geodata.string_utils import wide_iter, wide_ord
from geodata.i18n.unicode_paths import DATA_DIR
from geodata.i18n.normalize import strip_accents
from geodata.i18n.unicode_properties import get_chars_by_script, get_script_languages
from postal.text.tokenize import *

WELL_REPRESENTED_LANGUAGES = set(['en', 'fr', 'it', 'de', 'nl', 'es', 'pt'])

# For toponyms, we want to limit the countries we consider to those where
# we the place names can themselves be considered training examples of the language
WELL_REPRESENTED_LANGUAGE_COUNTRIES = {
    'en': set(['gb', 'us', 'ca', 'au', 'nz', 'ie']),
    'fr': set(['fr']),
    'it': set(['it']),
    'de': set(['de', 'at']),
    'nl': set(['nl']),
    'es': set(['es', 'ar', 'mx', 'cl', 'co', 'pe', 'ec', 'pr', 'uy',
               've', 'cu', 'do', 'bo', 'gt', 'cr', 'py', 'sv', 'pa',
               'ni', 'hn']),
    'pt': set(['pt', 'br']),
}

DICTIONARIES_DIR = os.path.join(DATA_DIR, 'dictionaries')

PREFIX_KEY = u'\x02'
SUFFIX_KEY = u'\x03'

POSSIBLE_ROMAN_NUMERALS = set(['i', 'ii', 'iii', 'iv', 'v', 'vi', 'vii', 'viii', 'ix',
                               'x', 'xi', 'xii', 'xiii', 'xiv', 'xv', 'xvi', 'xvii', 'xviii', 'xix',
                               'xx', 'xxx', 'xl', 'l', 'lx', 'lxx', 'lxxx', 'xc',
                               'c', 'cc', 'ccc', 'cd', 'd', 'dc', 'dcc', 'dccc', 'cm',
                               'm', 'mm', 'mmm', 'mmmm'])


class DictionaryPhraseFilter(PhraseFilter):
    def __init__(self, *dictionaries):
        self.dictionaries = dictionaries

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
                is_street_types_dictionary = 'street_types' in filename
                is_stopword_dictionary = 'stopwords' in filename

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
                    canonical = strip_accents(phrases[0])

                    for phrase in phrases:

                        if phrase in POSSIBLE_ROMAN_NUMERALS:
                            continue

                        is_canonical = strip_accents(phrase) == canonical

                        if is_suffix_dictionary:
                            phrase = SUFFIX_KEY + phrase[::-1]
                        elif is_prefix_dictionary:
                            phrase = PREFIX_KEY + phrase

                        if is_canonical or is_street_types_dictionary or is_prefix_dictionary or is_suffix_dictionary:
                            kvs[phrase][lang] = (is_canonical, is_stopword_dictionary)

        kvs = [(k, '|'.join([v, str(int(c)), str(int(s))])) for k, vals in kvs.iteritems() for v, (c, s) in vals.iteritems()]

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
        for c, t, data in self.basic_filter(tokens):
            if c != token_types.PHRASE:
                token = t[1]
                token_len = len(token)

                suffix_search, suffix_len = self.search_suffix(token)
                if suffix_search and self.trie.get(token[(token_len - suffix_len):].rstrip('.')):
                    yield (token_types.PHRASE, [(c,) + t], suffix_search)
                    continue
                prefix_search, prefix_len = self.search_prefix(token)
                if prefix_search and self.trie.get(token[:prefix_len]):
                    yield (token_types.PHRASE, [(c,) + t], prefix_search)
                    continue
            yield c, t, data

street_types_gazetteer = DictionaryPhraseFilter('street_types.txt',
                                                'directionals.txt',
                                                'concatenated_suffixes_separable.txt',
                                                'concatenated_suffixes_inseparable.txt',
                                                'concatenated_prefixes_separable.txt',
                                                'stopwords.txt',)

char_scripts = get_chars_by_script()
script_languages = {script: set(langs) for script, langs in get_script_languages().iteritems()}

UNKNOWN_SCRIPT = 'Unknown'
COMMON_SCRIPT = 'Common'
MAX_ASCII = 127


def get_string_script(s):
    s = safe_decode(s)
    str_len = len(s)
    script = last_script = UNKNOWN_SCRIPT
    is_ascii = True
    script_len = 0
    for c in wide_iter(s):
        script = char_scripts[wide_ord(c)]

        if script == COMMON_SCRIPT and last_script != UNKNOWN_SCRIPT:
            script = last_script
        if last_script != script and last_script != UNKNOWN_SCRIPT and last_script != COMMON_SCRIPT:
            if (script_len < str_len):
                for c in reversed(list(wide_iter(s[:script_len]))):
                    if char_scripts[wide_ord(c)] == COMMON_SCRIPT:
                        script_len -= 1
            break
        is_ascii = is_ascii and ord(c) <= MAX_ASCII
        script_len += 1
        if script != UNKNOWN_SCRIPT:
            last_script = script
    return (last_script, script_len, is_ascii)

LATIN_SCRIPT = 'Latin'
UNKNOWN_LANGUAGE = 'unk'
AMBIGUOUS_LANGUAGE = 'xxx'


def disambiguate_language(text, languages):
    valid_languages = OrderedDict(languages)
    script_langs = {}
    read_len = 0
    while read_len < len(text):
        script, script_len, is_ascii = get_string_script(text[read_len:])
        if script != LATIN_SCRIPT:
            script_langs[script] = set([l for l, d in languages if l in script_languages.get(script, [])])
        read_len += script_len

    num_defaults = sum((1 for lang, default in valid_languages.iteritems() if default))
    tokens = [(c, t.rstrip('.')) for t, c in tokenize(safe_decode(text).replace(u'-', u' ').lower())]

    current_lang = None
    possible_lang = None

    seen_languages = set()

    for c, t, data in street_types_gazetteer.filter(tokens):
        if c == token_types.PHRASE:
            valid = []
            data = [d.split('|') for d in data]
            potentials = [l for l, c, s in data if l in valid_languages]

            for lang, canonical, stopword in data:
                canonical = int(canonical)
                stopword = int(stopword)
                if lang not in valid_languages or (stopword and len(potentials) > 1):
                    continue
                is_default = valid_languages[lang]

                lang_valid = is_default or not seen_languages or lang in seen_languages

                if lang_valid and ((canonical and not stopword) or (is_default and len(potentials) == 1)):
                    valid.append(lang)
                elif is_default and num_defaults > 1 and current_lang is not None and current_lang != lang:
                    return AMBIGUOUS_LANGUAGE
                elif stopword and canonical and not is_default and lang in seen_languages:
                    valid.append(lang)
                elif not seen_languages and len(potentials) == 1 and len(t[0][1]) > 1:
                    possible_lang = lang if possible_lang is None or possible_lang == lang else None

            if seen_languages and valid and not any((l in seen_languages for l in valid)) and \
               (not any((valid_languages.get(l) for l in valid)) or any((valid_languages.get(l) for l in seen_languages))):
                return AMBIGUOUS_LANGUAGE

            if len(valid) == 1:
                current_lang = valid[0]
            else:
                valid_default = [l for l in valid if valid_languages.get(l)]
                if len(valid_default) == 1 and current_lang is not None and valid_default[0] != current_lang:
                    return AMBIGUOUS_LANGUAGE
                elif len(valid_default) == 1:
                    current_lang = valid_default[0]

            if any((current_lang not in langs for script, langs in script_langs.iteritems())):
                return AMBIGUOUS_LANGUAGE

            seen_languages.update(valid)

    if current_lang is not None:
        return current_lang
    elif possible_lang is not None:
        if not any((possible_lang not in langs for script, langs in script_langs.iteritems())):
            return possible_lang
        else:
            return AMBIGUOUS_LANGUAGE
    return UNKNOWN_LANGUAGE
