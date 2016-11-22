import os
import six
import sys

from collections import defaultdict, OrderedDict

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir, os.pardir, 'python')))

from geodata.address_expansions.gazetteers import *
from geodata.encoding import safe_decode, safe_encode
from geodata.string_utils import wide_iter, wide_ord
from geodata.i18n.unicode_properties import get_chars_by_script, get_script_languages
from geodata.text.normalize import normalized_tokens, normalize_string
from geodata.text.tokenize import tokenize
from geodata.text.token_types import token_types

WELL_REPRESENTED_LANGUAGES = set(['en', 'fr', 'it', 'de', 'nl', 'es', 'pt'])

# For toponyms, we want to limit the countries we consider to those where
# the place names can themselves be considered training examples of the language
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

char_scripts = get_chars_by_script()
script_languages = {script: set(langs) for script, langs in six.iteritems(get_script_languages())}
lang_scripts = defaultdict(set)

for script, langs in six.iteritems(script_languages):
    for lang in langs:
        lang_scripts[lang].add(script)

lang_scripts = dict(lang_scripts)

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


def disambiguate_language_script(text, languages):
    script_langs = {}
    read_len = 0
    while read_len < len(text):
        script, script_len, is_ascii = get_string_script(text[read_len:])
        if script != LATIN_SCRIPT:
            script_valid = [l for l, d in languages if l in script_languages.get(script, [])]
            script_langs[script] = set(script_valid)

            if script_len == len(text) and len(script_valid) == 1:
                return script_valid[0], script_langs

        read_len += script_len

    return UNKNOWN_LANGUAGE, script_langs

LATIN_TRANSLITERATED_SCRIPTS = {'Arabic', 'Cyrllic'}


def has_non_latin_script(languages):
    for lang, is_default in languages:
        scripts = lang_scripts.get(lang, set())
        if LATIN_SCRIPT not in scripts or scripts & LATIN_TRANSLITERATED_SCRIPTS:
            return True
    return False


def disambiguate_language(text, languages, scripts_only=False):
    text = safe_decode(text)
    valid_languages = OrderedDict(languages)

    language_script, script_langs = disambiguate_language_script(text, languages)
    if language_script is not UNKNOWN_LANGUAGE:
        return language_script

    num_defaults = sum((1 for lang, default in valid_languages.iteritems() if default))

    tokens = normalized_tokens(text)

    current_lang = None
    possible_lang = None

    seen_languages = set()

    for t, c, l, data in street_types_gazetteer.filter(tokens):
        if c == token_types.PHRASE:
            valid = OrderedDict()
            data = [safe_decode(d).split(u'|') for d in data]
            potentials = set([l for l, d, i, c in data if l in valid_languages])
            potential_defaults = set([l for l in potentials if valid_languages[l]])

            phrase_len = sum((len(t_i[0]) for t_i in t))
            for lang, dictionary, is_canonical, canonical in data:
                is_canonical = int(is_canonical)
                is_stopword = dictionary == 'stopword'
                if lang not in valid_languages or (is_stopword and len(potentials) > 1):
                    continue
                is_default = valid_languages[lang]

                lang_valid = is_default or not seen_languages or lang in seen_languages

                if lang_valid and phrase_len > 1 and ((is_canonical and not is_stopword) or (is_default and (len(potentials) == 1 or len(potential_defaults) == 1))):
                    valid[lang] = 1
                elif is_default and num_defaults > 1 and current_lang is not None and current_lang != lang:
                    return AMBIGUOUS_LANGUAGE
                elif is_stopword and is_canonical and not is_default and lang in seen_languages:
                    valid[lang] = 1
                elif not seen_languages and len(potentials) == 1 and phrase_len > 1:
                    possible_lang = lang if possible_lang is None or possible_lang == lang else None

            if seen_languages and valid and not any((l in seen_languages for l in valid)) and \
               (not any((valid_languages.get(l) for l in valid)) or any((valid_languages.get(l) for l in seen_languages))):
                return AMBIGUOUS_LANGUAGE

            valid = valid.keys()

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
