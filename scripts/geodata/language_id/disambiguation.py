import os
import sys

from collections import defaultdict, OrderedDict

from marisa_trie import BytesTrie

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir, os.pardir, 'python')))

from geodata.encoding import safe_decode
from geodata.string_utils import wide_iter, wide_ord
from geodata.i18n.unicode_paths import DATA_DIR
from geodata.i18n.normalize import strip_accents
from geodata.i18n.unicode_properties import get_chars_by_script, get_script_languages
from geodata.text.normalize import normalized_tokens, normalize_string
from geodata.text.tokenize import tokenize, token_types
from geodata.text.phrases import PhraseFilter

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

                        kvs[phrase][(lang, dictionary_name)] = (is_canonical, canonical)

        kvs = [(k, '|'.join([l, d, str(int(i)), safe_encode(c)])) for k, vals in kvs.iteritems() for (l, d), (i, c) in vals.iteritems()]

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
                    yield (t, PHRASE, map(safe_decode, suffix_search))
                    continue
                prefix_search, prefix_len = self.search_prefix(token)
                if prefix_search and self.trie.get(token[:prefix_len]):
                    yield (t, PHRASE, map(safe_decode, prefix_search))
                    continue
            else:
                c = PHRASE
            yield t, c, map(safe_decode, data)

STREET_TYPES_DICTIONARIES = ('street_types.txt',
                             'directionals.txt',
                             'concatenated_suffixes_separable.txt',
                             'concatenated_suffixes_inseparable.txt',
                             'concatenated_prefixes_separable.txt',
                             'stopwords.txt',)

GIVEN_NAME_DICTIONARY = 'given_names.txt'
SURNAME_DICTIONARY = 'surnames.txt'

NAME_DICTIONARIES = (GIVEN_NAME_DICTIONARY,
                     SURNAME_DICTIONARY,)

ALL_ABBREVIATION_DICTIONARIES = STREET_TYPES_DICTIONARIES + ('academic_degrees.txt',
                                                             'building_types.txt',
                                                             'company_types.txt',
                                                             'directionals.txt',
                                                             'level_types.txt',
                                                             'no_number.txt',
                                                             'nulls.txt',
                                                             'organizations.txt',
                                                             'people.txt',
                                                             'personal_suffixes.txt',
                                                             'personal_titles.txt',
                                                             'place_names.txt',
                                                             'post_office.txt',
                                                             'qualifiers.txt',
                                                             'synonyms.txt',
                                                             'toponyms.txt',
                                                             'unit_types.txt',
                                                             )

street_types_gazetteer = DictionaryPhraseFilter(*STREET_TYPES_DICTIONARIES)
abbreviations_gazetteer = DictionaryPhraseFilter(*ALL_ABBREVIATION_DICTIONARIES)
given_name_gazetteer = DictionaryPhraseFilter(GIVEN_NAME_DICTIONARY)

char_scripts = []
script_languages = {}


def init_disambiguation():
    global char_scripts, script_languages
    char_scripts[:] = []
    char_scripts.extend(get_chars_by_script())
    script_languages.update({script: set(langs) for script, langs in get_script_languages().iteritems()})

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
    text = safe_decode(text)
    valid_languages = OrderedDict(languages)
    script_langs = {}
    read_len = 0
    while read_len < len(text):
        script, script_len, is_ascii = get_string_script(text[read_len:])
        if script != LATIN_SCRIPT:
            script_valid = [l for l, d in languages if l in script_languages.get(script, [])]
            script_langs[script] = set(script_valid)

            if script_len == len(text) and len(script_valid) == 1:
                return script_valid[0]

        read_len += script_len

    num_defaults = sum((1 for lang, default in valid_languages.iteritems() if default))

    tokens = normalized_tokens(text)

    current_lang = None
    possible_lang = None

    seen_languages = set()

    for t, c, data in street_types_gazetteer.filter(tokens):
        if c is PHRASE:
            valid = []
            data = [d.split('|') for d in data]
            potentials = [l for l, d, i, c in data if l in valid_languages]

            for lang, dictionary, is_canonical, canonical in data:
                is_canonical = int(is_canonical)
                is_stopword = dictionary == 'stopword'
                if lang not in valid_languages or (is_stopword and len(potentials) > 1):
                    continue
                is_default = valid_languages[lang]

                lang_valid = is_default or not seen_languages or lang in seen_languages

                if lang_valid and ((is_canonical and not is_stopword) or (is_default and len(potentials) == 1)):
                    valid.append(lang)
                elif is_default and num_defaults > 1 and current_lang is not None and current_lang != lang:
                    return AMBIGUOUS_LANGUAGE
                elif is_stopword and is_canonical and not is_default and lang in seen_languages:
                    valid.append(lang)
                elif not seen_languages and len(potentials) == 1 and len(t[0][0]) > 1:
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
