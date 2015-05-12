# -*- coding: utf-8 -*-
'''
transliteration.py

Automatically builds rules for transforming other scripts (e.g. Cyrillic, Greek,
Han, Katakana, Devanagari, etc.) into Latin characters.

Uses XML transforms from the CLDR repository.

'''

import argparse
import codecs
import csv
import htmlentitydefs
import itertools
import os
import re
import requests
import sys
import time
import urlparse
import unicodedata

from collections import defaultdict, deque

from lxml import etree

from scanner import Scanner
from unicode_properties import *
from unicode_paths import CLDR_DIR
from geodata.encoding import safe_decode, safe_encode

CLDR_TRANSFORMS_DIR = os.path.join(CLDR_DIR, 'common', 'transforms')

PRE_TRANSFORM = 1
FORWARD_TRANSFORM = 2
BACKWARD_TRANSFORM = 3
BIDIRECTIONAL_TRANSFORM = 4

PRE_TRANSFORM_OP = '::'
BACKWARD_TRANSFORM_OPS = set([u'←', u'<'])
FORWARD_TRANSFORM_OPS = set([u'→', u'>'])
BIDIRECTIONAL_TRANSFORM_OPS = set([u'↔', u'<>'])

ASSIGNMENT_OP = '='

PRE_CONTEXT_INDICATOR = '{'
POST_CONTEXT_INDICATOR = '}'

REVISIT_INDICATOR = '|'

WORD_BOUNDARY_VAR_NAME = 'wordBoundary'
WORD_BOUNDARY_VAR = '${}'.format(WORD_BOUNDARY_VAR_NAME)

word_boundary_var_regex = re.compile(WORD_BOUNDARY_VAR.replace('$', '\$'))

WORD_BOUNDARY_CHAR = u'\u0001'
EMPTY_TRANSITION = u'\u0004'

NAMESPACE_SEPARATOR_CHAR = u"|"

WORD_BOUNDARY_CHAR = u"\x01"
PRE_CONTEXT_CHAR = u"\x02"
POST_CONTEXT_CHAR = u"\x03"
EMPTY_TRANSITION_CHAR = u"\x04"
REPEAT_ZERO_CHAR = u"\x05"
REPEAT_ONE_CHAR = u"\x06"
BEGIN_SET_CHAR = u"\x07"
END_SET_CHAR = u"\x08"
GROUP_INDICATOR_CHAR = u"\x09"

EXCLUDE_TRANSLITERATORS = set([
    'hangul-latin',
    'interindic-latin',
    'jamo-latin',
    # Don't care about spaced Han because 
    'han-spacedhan',
])

NFD = 'NFD'
NFKD = 'NFKD'
NFC = 'NFC'
NFKC = 'NFKC'

LOWER = 'lower'
UPPER = 'upper'
TITLE = 'title'

UNICODE_NORMALIZATION_TRANSFORMS = set([
    NFD,
    NFKD,
    NFC,
    NFKC,
])

unicode_category_aliases = {
    'letter': 'L',
    'lower': 'Ll',
    'lowercase': 'Ll',
    'lowercaseletter': 'Ll',
    'upper': 'Lu',
    'uppercase': 'Lu',
    'uppercaseletter': 'Lu',
    'title': 'Lt',
    'nonspacing mark': 'Mn',
    'mark': 'M',
}

unicode_categories = defaultdict(list)
unicode_blocks = defaultdict(list)
unicode_combining_classes = defaultdict(list)
unicode_general_categories = defaultdict(list)
unicode_scripts = defaultdict(list)
unicode_properties = {}

unicode_blocks = {}
unicode_category_aliases = {}
unicode_property_aliases = {}
unicode_property_value_aliases = {}
unicode_word_breaks = {}

COMBINING_CLASS_PROP = 'canonical_combining_class'
BLOCK_PROP = 'block'
GENERAL_CATEGORY_PROP = 'general_category'
SCRIPT_PROP = 'script'
WORD_BREAK_PROP = 'word_break'


class TransliterationParseError(Exception):
    pass


def init_unicode_categories():
    global unicode_categories, unicode_general_categories, unicode_scripts, unicode_category_aliases
    global unicode_blocks, unicode_combining_classes, unicode_properties, unicode_property_aliases
    global unicode_property_value_aliases, unicode_scripts, unicode_word_breaks

    for i in xrange(65536):
        unicode_categories[unicodedata.category(unichr(i))].append(unichr(i))
        unicode_combining_classes[str(unicodedata.combining(unichr(i)))].append(unichr(i))

    unicode_categories = dict(unicode_categories)
    unicode_combining_classes = dict(unicode_combining_classes)

    for key in unicode_categories.keys():
        unicode_general_categories[key[0]].extend(unicode_categories[key])

    unicode_general_categories = dict(unicode_general_categories)

    script_chars = get_chars_by_script()
    for i, script in enumerate(script_chars):
        if script:
            unicode_scripts[script.lower()].append(unichr(i))

    unicode_scripts = dict(unicode_scripts)

    unicode_blocks.update(get_unicode_blocks())
    unicode_properties.update(get_unicode_properties())
    unicode_property_aliases.update(get_property_aliases())

    unicode_word_breaks.update(get_word_break_properties())

    for key, value in get_property_value_aliases().iteritems():
        key = unicode_property_aliases.get(key, key)
        if key == GENERAL_CATEGORY_PROP:
            for k, v in value.iteritems():
                k = k.lower()
                unicode_category_aliases[k] = v
                if '_' in k:
                    unicode_category_aliases[k.replace('_', '')] = v

        unicode_property_value_aliases[key] = value


RULE = 'RULE'
TRANSFORM = 'TRANSFORM'
FILTER = 'FILTER'

UTF8PROC_TRANSFORMS = {
    'Any-NFC': NFC,
    'Any-NFD': NFD,
    'Any-NFKD': NFKD,
    'Any-NFKC': NFKC,
    'Any-Lower': LOWER,
    'Any-Upper': UPPER,
    'Any-Title': TITLE,
}


CONTEXT_TYPE_NONE = 'CONTEXT_TYPE_NONE'
CONTEXT_TYPE_STRING = 'CONTEXT_TYPE_STRING'
CONTEXT_TYPE_WORD_BOUNDARY = 'CONTEXT_TYPE_WORD_BOUNDARY'
CONTEXT_TYPE_REGEX = 'CONTEXT_TYPE_REGEX'

all_transforms = set()

pre_transform_full_regex = re.compile('::[\s]*(.*)[\s]*', re.UNICODE)
pre_transform_regex = re.compile('[\s]*([^\s\(\)]*)[\s]*(?:\(.*\)[\s]*)?', re.UNICODE)
transform_regex = re.compile(u"(?:[\s]*(?!=[\s])(.*)(?<![\s])[\s]*)((?:<>)|[←<→>↔=])(?:[\s]*(?!=[\s])(.*)(?<![\s])[\s]*)", re.UNICODE)

quoted_string_regex = re.compile(r'\'.*?\'', re.UNICODE)

COMMENT_CHAR = '#'
END_CHAR = ';'


def unescape_unicode_char(m):
    return m.group(0).decode('unicode-escape')

escaped_unicode_regex = re.compile(r'(?:\\u[0-9A-Fa-f]{4}|\\U[0-9A-Fa-f]{8})')

literal_space_regex = re.compile(r'(?:\\u0020|\\U00000020)')

# These are a few unicode property types that were needed by the transforms
unicode_property_regexes = [
    ('ideographic', '[〆〇〡-〩〸-〺㐀-䶵一-鿌豈-舘並-龎 𠀀-𪛖𪜀-𫜴𫝀-𫠝丽-𪘀]'),
    ('logical_order_exception', '[เ-ไ ເ-ໄ ꪵ ꪶ ꪹ ꪻ ꪼ]'),
]

char_set_map = {
    '[^[:ccc=Not_Reordered:][:ccc=Above:]]': u'[֑ ֖ ֚ ֛ ֢-֧ ֪ ֭ ֮ ֽ ׅ ؘ-ؚ ۣ ۪ ۭ ݄ ݈ ࣭-࣯ ॒ ༘ ༙ ༵ ༷ ࿆ ᩿ ᭬ ᳔-᳙ ᳜-᳟ ᳢-᳨ ⵿ ︨ ︪-︭ 𝅥𐋠-𝅩𝅭-𝅲 𝅻-𝆂 𝆊 𝆋 𞣐-𞣖 ̲ ̸ ̧ ̨ ̕ ̚ ͝ ͞ ᷍ ᷎ ̖-̙ ̜-̠ ̩-̬ ̯ ̳ ̺-̼ ͇-͉ ͍ ͎ ͓-͖ ͙ ͚ ͜ ͟ ͢ ݂ ݆ ࡙-࡛ ᪵-᪺ ᪽ ᷂ ᷏ ᷐ ᷼ ᷽ ᷿ ⃬-⃯ ︧ 𐨍 𐫦 ̶ ̷ ⃘-⃚ ⃥ ⃪ ⃫ 𛲞 ゙ ゚ ̵ ̛ ̡-̦ ̭ ̮ ̰ ̱ ̴ ̹ ͅ ͘ ͠ ︩ ͡ ְ-ָ ׇ ֹ-ֻ ׂ ׁ ּ ֿ ﬞ ً ࣰ ٌ ࣱ ٍ ࣲ ࣩ َ-ِ ࣦ ࣶ ّ ْ ٕ ٟ ٖ ٜ ࣹ ࣺ ٰ ܑ ܱ ܴ ܷ-ܹ ܻ ܼ ܾ ߲ 𖫰-𖫴 ़ ় ਼ ઼ ଼ ಼ ᬴ ᯦ ᰷ ꦳ 𑂺 𑅳 𑈶 𑋩 𑌼 𑓃 𑗀 𑚷 ᳭ 𐨹 𐨺 ่-๋ ່-໋ ༹ ꤫-꤭ ့ ᤹ ᤻ 〪-〯 ⃒ ⃓ ⃦ ⃨ 𐇽 ᷊ ् ্ ੍ ્ ୍ ் ్ ౕ ౖ ್ ് ් ꯭ ꫶ ꠆ ꣄ 𑂹 𑇀 𑈵 𑋪 𑍍 𑓂 𑖿 𑘿 𑚶 ᮪ ᮫ 𑁆 𑁿 𐨿 ุ-ฺ ຸ ູ ꪴ ཱ ི ྀ ུ ེ-ཽ ྄ ᜔ ᜴ ᨘ ᯲ ᯳ ꥓ ္ ် ႍ 𑄳 𑄴 ្ ᩠ ᭄ ꧀ ᢩ]',
    '[[:^ccc=0:] & [:^ccc=230:]]': u'[֑ ֖ ֚ ֛ ֢-֧ ֪ ֭ ֮ ֽ ׅ ؘ-ؚ ۣ ۪ ۭ ݄ ݈ ࣭-࣯ ॒ ༘ ༙ ༵ ༷ ࿆ ᩿ ᭬ ᳔-᳙ ᳜-᳟ ᳢-᳨ ⵿ ︨ ︪-︭ 𝅥𐋠-𝅩𝅭-𝅲 𝅻-𝆂 𝆊 𝆋 𞣐-𞣖 ̲ ̸ ̧ ̨ ̕ ̚ ͝ ͞ ᷍ ᷎ ̖-̙ ̜-̠ ̩-̬ ̯ ̳ ̺-̼ ͇-͉ ͍ ͎ ͓-͖ ͙ ͚ ͜ ͟ ͢ ݂ ݆ ࡙-࡛ ᪵-᪺ ᪽ ᷂ ᷏ ᷐ ᷼ ᷽ ᷿ ⃬-⃯ ︧ 𐨍 𐫦 ̶ ̷ ⃘-⃚ ⃥ ⃪ ⃫ 𛲞 ゙ ゚ ̵ ̛ ̡-̦ ̭ ̮ ̰ ̱ ̴ ̹ ͅ ͘ ͠ ︩ ͡ ְ-ָ ׇ ֹ-ֻ ׂ ׁ ּ ֿ ﬞ ً ࣰ ٌ ࣱ ٍ ࣲ ࣩ َ-ِ ࣦ ࣶ ّ ْ ٕ ٟ ٖ ٜ ࣹ ࣺ ٰ ܑ ܱ ܴ ܷ-ܹ ܻ ܼ ܾ ߲ 𖫰-𖫴 ़ ় ਼ ઼ ଼ ಼ ᬴ ᯦ ᰷ ꦳ 𑂺 𑅳 𑈶 𑋩 𑌼 𑓃 𑗀 𑚷 ᳭ 𐨹 𐨺 ่-๋ ່-໋ ༹ ꤫-꤭ ့ ᤹ ᤻ 〪-〯 ⃒ ⃓ ⃦ ⃨ 𐇽 ᷊ ् ্ ੍ ્ ୍ ் ్ ౕ ౖ ್ ് ් ꯭ ꫶ ꠆ ꣄ 𑂹 𑇀 𑈵 𑋪 𑍍 𑓂 𑖿 𑘿 𑚶 ᮪ ᮫ 𑁆 𑁿 𐨿 ุ-ฺ ຸ ູ ꪴ ཱ ི ྀ ུ ེ-ཽ ྄ ᜔ ᜴ ᨘ ᯲ ᯳ ꥓ ္ ် ႍ 𑄳 𑄴 ្ ᩠ ᭄ ꧀ ᢩ]',
    '[^\p{ccc=0}\p{ccc=above}]': u'[֑ ֖ ֚ ֛ ֢-֧ ֪ ֭ ֮ ֽ ׅ ؘ-ؚ ۣ ۪ ۭ ݄ ݈ ࣭-࣯ ॒ ༘ ༙ ༵ ༷ ࿆ ᩿ ᭬ ᳔-᳙ ᳜-᳟ ᳢-᳨ ⵿ ︨ ︪-︭ 𝅥𐋠-𝅩𝅭-𝅲 𝅻-𝆂 𝆊 𝆋 𞣐-𞣖 ̲ ̸ ̧ ̨ ̕ ̚ ͝ ͞ ᷍ ᷎ ̖-̙ ̜-̠ ̩-̬ ̯ ̳ ̺-̼ ͇-͉ ͍ ͎ ͓-͖ ͙ ͚ ͜ ͟ ͢ ݂ ݆ ࡙-࡛ ᪵-᪺ ᪽ ᷂ ᷏ ᷐ ᷼ ᷽ ᷿ ⃬-⃯ ︧ 𐨍 𐫦 ̶ ̷ ⃘-⃚ ⃥ ⃪ ⃫ 𛲞 ゙ ゚ ̵ ̛ ̡-̦ ̭ ̮ ̰ ̱ ̴ ̹ ͅ ͘ ͠ ︩ ͡ ְ-ָ ׇ ֹ-ֻ ׂ ׁ ּ ֿ ﬞ ً ࣰ ٌ ࣱ ٍ ࣲ ࣩ َ-ِ ࣦ ࣶ ّ ْ ٕ ٟ ٖ ٜ ࣹ ࣺ ٰ ܑ ܱ ܴ ܷ-ܹ ܻ ܼ ܾ ߲ 𖫰-𖫴 ़ ় ਼ ઼ ଼ ಼ ᬴ ᯦ ᰷ ꦳ 𑂺 𑅳 𑈶 𑋩 𑌼 𑓃 𑗀 𑚷 ᳭ 𐨹 𐨺 ่-๋ ່-໋ ༹ ꤫-꤭ ့ ᤹ ᤻ 〪-〯 ⃒ ⃓ ⃦ ⃨ 𐇽 ᷊ ् ্ ੍ ્ ୍ ் ్ ౕ ౖ ್ ് ් ꯭ ꫶ ꠆ ꣄ 𑂹 𑇀 𑈵 𑋪 𑍍 𑓂 𑖿 𑘿 𑚶 ᮪ ᮫ 𑁆 𑁿 𐨿 ุ-ฺ ຸ ູ ꪴ ཱ ི ྀ ུ ེ-ཽ ྄ ᜔ ᜴ ᨘ ᯲ ᯳ ꥓ ္ ် ႍ 𑄳 𑄴 ្ ᩠ ᭄ ꧀ ᢩ]',
}

unicode_properties = {}


def replace_literal_space(m):
    return "' '"

regex_char_set_greedy = re.compile(r'\[(.*)\]', re.UNICODE)
regex_char_set = re.compile(r'\[(.*?)(?<!\\)\]', re.UNICODE)

char_class_regex_str = '\[(?:[^\[\]]*\[[^\[\]]*\][^\[\]]*)*[^\[\]]*\]'

nested_char_class_regex = re.compile('\[(?:[^\[\]]*\[[^\[\]]*\][^\[\]]*)+[^\[\]]*\]', re.UNICODE)

range_regex = re.compile(r'[\\]?([^\\])\-[\\]?([^\\])', re.UNICODE)
var_regex = re.compile('\$([A-Za-z_\-]+)')

context_regex = re.compile(u'(?:[\s]*(?!=[\s])(.*?)(?<![\s])[\s]*{)?(?:[\s]*([^}{]*)[\s]*)(?:}[\s]*(?!=[\s])(.*)(?<![\s])[\s]*)?', re.UNICODE)

paren_regex = re.compile(r'\(.*\)', re.UNICODE)

group_ref_regex_str = '\$[0-9]+'
group_ref_regex = re.compile(group_ref_regex_str)

# Limited subset of regular expressions used in transforms

OPEN_SET = 'OPEN_SET'
CLOSE_SET = 'CLOSE_SET'
OPEN_GROUP = 'OPEN_GROUP'
CLOSE_GROUP = 'CLOSE_GROUP'
GROUP_REF = 'GROUP_REF'
CHAR_SET = 'CHAR_SET'
CHAR_MULTI_SET = 'CHAR_MULTI_SET'
CHAR_CLASS = 'CHAR_CLASS'
OPTIONAL = 'OPTIONAL'
CHARACTER = 'CHARACTER'
REVISIT = 'REVISIT'
REPEAT = 'REPEAT'
LPAREN = 'LPAREN'
RPAREN = 'RPAREN'
WHITESPACE = 'WHITESPACE'
QUOTED_STRING = 'QUOTED_STRING'
SINGLE_QUOTE = 'SINGLE_QUOTE'
HTML_ENTITY = 'HTML_ENTITY'
SINGLE_QUOTE = 'SINGLE_QUOTE'
ESCAPED_CHARACTER = 'ESCAPED_CHARACTER'

BEFORE_CONTEXT = '{'
AFTER_CONTEXT = '}'

PLUS = 'PLUS'
STAR = 'STAR'

# Scanner for the lvalue or rvalue of a transform rule

transform_scanner = Scanner([
    (r'[\\].', ESCAPED_CHARACTER),
    (r'\'\'', SINGLE_QUOTE),
    (r'\'.*?\'', QUOTED_STRING),
    # Char classes only appear to go two levels deep in LDML
    ('\[', OPEN_SET),
    ('\]', CLOSE_SET),
    ('\(', OPEN_GROUP),
    ('\)', CLOSE_GROUP),
    (group_ref_regex_str, GROUP_REF),
    (r'\|', REVISIT),
    (r'&.*?;', HTML_ENTITY),
    (r'(?<![\\])\*', REPEAT),
    (r'(?<![\\])\+', PLUS),
    ('\?', OPTIONAL),
    ('\(', LPAREN),
    ('\)', RPAREN),
    ('\|', REVISIT),
    ('[\s]+', WHITESPACE),
    (r'[\\]?[^\s]', CHARACTER),
], re.UNICODE)

CHAR_RANGE = 'CHAR_RANGE'
CHAR_CLASS_PCRE = 'CHAR_CLASS_PCRE'
WORD_BOUNDARY = 'WORD_BOUNDARY'
NEGATION = 'NEGATION'
INTERSECTION = 'INTERSECTION'
DIFFERENCE = 'DIFFERENCE'

# Scanner for a character set (yes, a regex regex)

char_set_scanner = Scanner([
    ('^\^', NEGATION),
    (r'\\p\{[^\{\}]+\}', CHAR_CLASS_PCRE),
    (r'[\\]?[^\\\s]\-[\\]?[^\s]', CHAR_RANGE),
    (r'[\\].', ESCAPED_CHARACTER),
    (r'\'\'', SINGLE_QUOTE),
    (r'\'.*?\'', QUOTED_STRING),
    (':[^:]+:', CHAR_CLASS),
    # Char set
    ('\[[^\[\]]+\]', CHAR_SET),
    ('\[.*\]', CHAR_MULTI_SET),
    ('\[', OPEN_SET),
    ('\]', CLOSE_SET),
    ('&', INTERSECTION),
    ('(?<=[\s])-(?=[\s])', DIFFERENCE),
    ('\$', WORD_BOUNDARY),
    (r'[^\s]', CHARACTER),
])

NUM_CHARS = 65536

all_chars = set([unichr(i) for i in xrange(NUM_CHARS)])

control_chars = set([c for c in all_chars if unicodedata.category(c) == 'Cc'])


def get_transforms():
    return [f for f in os.listdir(CLDR_TRANSFORMS_DIR) if f.endswith('.xml')]


def replace_html_entity(ent):
    name = ent.strip('&;')
    return unichr(htmlentitydefs.name2codepoint[name])


def parse_regex_char_range(regex):
    prev_char = None
    ranges = range_regex.findall(regex)
    regex = range_regex.sub('', regex)
    chars = [ord(c) for c in regex]

    for start, end in ranges:

        if ord(end) > ord(start):
            # Ranges are inclusive
            chars.extend([unichr(c) for c in range(ord(start), ord(end) + 1)])

    return chars


def parse_regex_char_class(c, current_filter=all_chars):
    chars = []
    orig = c
    if c.startswith('\\p'):
        c = c.split('{')[-1].split('}')[0]

    c = c.strip(': ')
    is_negation = False
    if c.startswith('^'):
        is_negation = True
        c = c.strip('^')

    if '=' in c:
        prop, value = c.split('=')
        prop = unicode_property_aliases.get(prop.lower(), prop)

        value = unicode_property_value_aliases.get(prop.lower(), {}).get(value, value)

        if prop == COMBINING_CLASS_PROP:
            chars = unicode_combining_classes[value]
        elif prop == GENERAL_CATEGORY_PROP:
            chars = unicode_categories.get(value, unicode_general_categories[value])
        elif prop == BLOCK_PROP:
            chars = unicode_blocks[value.lower()]
        elif prop == SCRIPT_PROP:
            chars = unicode_scripts[value.lower()]
        elif prop == WORD_BREAK_PROP:
            chars = unicode_word_breaks[value]
        else:
            raise TransliterationParseError(c)
    else:
        c = c.replace('-', '_').replace(' ', '_')

        if c.lower() in unicode_property_aliases:
            c = unicode_property_aliases[c.lower()]
        elif c.lower() in unicode_category_aliases:
            c = unicode_category_aliases[c.lower()]

        if c in unicode_general_categories:
            chars = unicode_general_categories[c]
        elif c in unicode_categories:
            chars = unicode_categories[c]
        elif c.lower() in unicode_properties:
            chars = unicode_properties[c.lower()]

        elif c.lower() in unicode_scripts:
            chars = unicode_scripts[c.lower()]
        elif c.lower() in unicode_properties:
            chars = unicode_properties[c.lower()]
        else:
            raise TransliterationParseError(c)

    if is_negation:
        chars = current_filter - set(chars)

    return sorted((set(chars) & current_filter) - control_chars)


def parse_balanced_sets(s):
    open_brackets = 0
    max_nesting = 0

    skip = False

    for i, ch in enumerate(s):
        if ch == '[':
            if open_brackets == 0:
                start = i
            max_nesting
            open_brackets += 1
        elif ch == ']':
            open_brackets -= 1
            if open_brackets == 0:
                skip = False
                yield (s[start:i + 1], CHAR_MULTI_SET)
                (start, i + 1)
        elif open_brackets == 0 and not skip:
            for token, token_class in char_set_scanner.scan(s[i:]):
                if token_class not in (CHAR_SET, CHAR_MULTI_SET, OPEN_SET, CLOSE_SET):
                    yield token, token_class
                else:
                    break
            skip = True


def parse_regex_char_set(s, current_filter=all_chars):
    '''
    Given a regex character set, which may look something like:

    [[:Latin:][:Greek:] & [:Ll:]]
    [A-Za-z_]
    [ $lowerVowel $upperVowel ]

    Parse into a single, flat character set without the unicode properties,
    ranges, unions/intersections, etc.
    '''
    if s in char_set_map:
        s = char_set_map[s]

    s = s[1:-1]
    is_negation = False
    this_group = set()
    is_intersection = False
    is_difference = False
    is_word_boundary = False

    for token, token_class in parse_balanced_sets(s):
        if token_class == CHAR_RANGE:
            this_char_set = set(parse_regex_char_range(token))
            this_group |= this_char_set
        elif token_class == ESCAPED_CHARACTER:
            token = token.strip('\\')
            this_group.add(token)
        elif token_class == SINGLE_QUOTE:
            this_group.add("'")
        elif token_class == QUOTED_STRING:
            this_group.add(token.strip("'"))
        elif token_class == NEGATION:
            is_negation = True
        elif token_class in (CHAR_CLASS, CHAR_CLASS_PCRE):
            this_group |= set(parse_regex_char_class(token, current_filter=current_filter))
        elif token_class in (CHAR_SET, CHAR_MULTI_SET):
            # Recursive calls, as performance doesn't matter here and nesting is shallow
            this_char_set = set(parse_regex_char_set(token, current_filter=current_filter))
            if is_intersection:
                this_group &= this_char_set
                is_intersection = False
            elif is_difference:
                this_group -= this_char_set
                is_difference = False
            else:
                this_group |= this_char_set
        elif token_class == INTERSECTION:
            is_intersection = True
        elif token_class == DIFFERENCE:
            is_difference = True
        elif token_class == CHARACTER:
            this_group.add(token)
        elif token_class == WORD_BOUNDARY:
            is_word_boundary = True

    if is_negation:
        this_group = current_filter - this_group

    return sorted((this_group & current_filter) - control_chars) + ([WORD_BOUNDARY_CHAR] if is_word_boundary else [])


for name, regex_range in unicode_property_regexes:
    unicode_properties[name] = parse_regex_char_set(regex_range)


def get_source_and_target(xml):
    return xml.xpath('//transform/@source')[0], xml.xpath('//transform/@target')[0]


def get_raw_rules_and_variables(xml):
    '''
    Parse tRule nodes from the transform XML

    At this point we only care about lvalue, op and rvalue
    for parsing forward and two-way transforms.

    Variables are collected in a dictionary in this pass so they can be substituted later
    '''
    rules = []
    variables = {}

    in_compound_rule = False
    compound_rule = []

    for rule in xml.xpath('*//tRule'):
        if not rule.text:
            continue

        rule = safe_decode(rule.text.rsplit(COMMENT_CHAR)[0].strip())
        rule = literal_space_regex.sub(replace_literal_space, rule)
        rule = escaped_unicode_regex.sub(unescape_unicode_char, rule)
        rule = rule.rstrip(END_CHAR).strip()

        if rule.strip().endswith('\\'):
            compound_rule.append(rule.rstrip('\\'))
            in_compound_rule = True
            continue
        elif in_compound_rule:
            compound_rule.append(rule)
            rule = u''.join(compound_rule)
            in_compound_rule = False
            compound_rule = []

        transform = transform_regex.match(rule)
        pre_transform = pre_transform_full_regex.match(rule)

        if pre_transform:
            rules.append((PRE_TRANSFORM, pre_transform.group(1)))

        elif transform:
            lvalue, op, rvalue = transform.groups()
            lvalue = lvalue.strip()
            rvalue = rvalue.strip()

            if op in FORWARD_TRANSFORM_OPS:
                rules.append((FORWARD_TRANSFORM, (lvalue, rvalue)))
            elif op in BIDIRECTIONAL_TRANSFORM_OPS:
                rules.append((BIDIRECTIONAL_TRANSFORM, (lvalue, rvalue)))
            elif op in BACKWARD_TRANSFORM_OPS:
                rules.append((BACKWARD_TRANSFORM, (lvalue, rvalue)))
            elif op == ASSIGNMENT_OP:
                var_name = lvalue.lstrip('$')
                variables[var_name] = rvalue
        else:
            print 'non-rule', rule, get_source_and_target(xml)

    return rules, variables

CHAR_CLASSES = set([
    ESCAPED_CHARACTER,
    CHAR_CLASS,
    QUOTED_STRING,
    CHARACTER,
    GROUP_REF,
])


def char_permutations(s, current_filter=all_chars):
    '''
    char_permutations

    Parses the lvalue or rvalue of a transform rule into
    a list of character permutations, in addition to keeping
    track of revisits and regex groups
    '''
    char_types = []
    move = 0
    in_revisit = False

    in_group = False
    last_token_group_start = False

    start_group = 0
    end_group = 0

    open_brackets = 0
    current_set = []

    groups = []

    for token, token_type in transform_scanner.scan(s):
        if open_brackets > 0 and token_type not in (OPEN_SET, CLOSE_SET):
            current_set.append(token)
            continue

        if token_type == ESCAPED_CHARACTER:
            char_types.append([token.strip('\\')])
        elif token_type == OPEN_GROUP:
            in_group = True
            last_token_group_start = True
        elif token_type == CLOSE_GROUP:
            in_group = False
            end_group = len(char_types)
            groups.append((start_group, end_group))
        elif token_type == OPEN_SET:
            open_brackets += 1
            current_set.append(token)
        elif token_type == CLOSE_SET:
            open_brackets -= 1
            current_set.append(token)
            if open_brackets == 0:
                char_types.append(parse_regex_char_set(u''.join(current_set), current_filter=current_filter))
                current_set = []
        elif token_type == QUOTED_STRING:
            token = token.strip("'")
            for c in token:
                char_types.append([c])
        elif token_type == GROUP_REF:
            char_types.append([token.replace('$', GROUP_INDICATOR_CHAR)])
        elif token_type == REVISIT:
            in_revisit = True
        elif token_type == REPEAT:
            char_types.append([REPEAT_ZERO_CHAR])
        elif token_type == PLUS:
            char_types.append([REPEAT_ONE_CHAR])
        elif token_type == OPTIONAL:
            char_types[-1].append(EMPTY_TRANSITION_CHAR)
        elif token_type == REVISIT:
            in_revisit = True
        elif token_type == HTML_ENTITY:
            char_types.append([replace_html_entity(token)])
        elif token_type == CHARACTER:
            char_types.append([token])

        if in_group and last_token_group_start:
            start_group = len(char_types)
            last_token_group_start = False

        if in_revisit and token_type in CHAR_CLASSES:
            move += 1

    return char_types, move, groups

    return list(itertools.product(char_types)), move

string_replacements = {
    u'[': u'\[',
    u']': u'\]',
    u'(': u'\(',
    u')': u'\)',
    u'{': u'\{',
    u'}': u'\{',
    u'$': u'\$',
    u'^': u'\^',
    u'-': u'\-',
    u'\\': u'\\\\',
    u'*': u'\*',
    u'+': u'\+',
}

escape_sequence_long_regex = re.compile(r'(\\x[0-9a-f]{2})([0-9a-f])', re.I)


def replace_long_escape_sequence(s):
    def replace_match(m):
        return u'{}""{}'.format(m.group(1), m.group(2))

    return escape_sequence_long_regex.sub(replace_match, s)


def quote_string(s):
    return u'"{}"'.format(replace_long_escape_sequence(safe_decode(s).replace('"', '\\"')))


def char_types_string(char_types):
    '''
    Transforms the char_permutations output into a string
    suitable for simple parsing in C (characters and character sets only,
    no variables, unicode character properties or unions/intersections)
    '''
    ret = []

    for chars in char_types:
        template = u'{}' if len(chars) == 1 else u'[{}]'
        norm = []
        for c in chars:
            c = string_replacements.get(c, c)
            norm.append(c)

        ret.append(template.format(u''.join(norm)))

    return u''.join(ret)


def format_groups(char_types, groups):
    group_regex = []
    last_end = 0
    for start, end in groups:
        group_regex.append(char_types_string(char_types[last_end:start]))
        group_regex.append(u'(')
        group_regex.append(char_types_string(char_types[start:end + 1]))
        group_regex.append(u')')
        last_end = end
    group_regex.append(char_types_string(char_types[last_end + 1:]))
    return u''.join(group_regex)


charset_regex = re.compile(r'(?<!\\)\[')


def escape_string(s):
    return s.encode('string-escape')


def format_rule(rule):
    '''
    Creates the C literal for a given transliteration rule
    '''
    key = safe_encode(rule[0])
    key_len = len(key)

    pre_context_type = rule[1]
    pre_context = rule[2]
    if pre_context is None:
        pre_context = 'NULL'
        pre_context_len = 0
    else:
        pre_context = safe_encode(pre_context)
        pre_context_len = len(pre_context)
        pre_context = quote_string(escape_string(pre_context))

    pre_context_max_len = rule[3]

    post_context_type = rule[4]
    post_context = rule[5]

    if post_context is None:
        post_context = 'NULL'
        post_context_len = 0
    else:
        post_context = safe_encode(post_context)
        post_context_len = len(post_context)
        post_context = quote_string(escape_string(post_context))

    post_context_max_len = rule[6]

    groups = rule[7]
    if not groups:
        groups = 'NULL'
        groups_len = 0
    else:
        groups = safe_encode(groups)
        groups_len = len(groups)
        groups = quote_string(escape_string(groups))

    replacement = safe_encode(rule[8])
    replacement_len = len(replacement)
    move = rule[9]

    output_rule = (
        quote_string(escape_string(key)),
        str(key_len),
        pre_context_type,
        str(pre_context_max_len),
        pre_context,
        str(pre_context_len),

        post_context_type,
        str(post_context_max_len),
        post_context,
        str(post_context_len),

        quote_string(escape_string(replacement)),
        str(replacement_len),
        str(move),
        groups,
        str(groups_len),
    )

    return output_rule


def parse_transform_rules(xml):
    '''
    parse_transform_rules takes a parsed xml document as input
    and generates rules suitable for use in the C code.

    Since we're only concerned with transforming into Latin/ASCII,
    we don't care about backward transforms or two-way contexts.
    Only the lvalue's context needs to be used.
    '''
    rules, variables = get_raw_rules_and_variables(xml)

    def get_var(m):
        return variables.get(m.group(1))

    # Replace variables within variables
    while True:
        num_found = 0
        for k, v in variables.items():
            if var_regex.search(v):
                v = var_regex.sub(get_var, v)
                variables[k] = v
                num_found += 1
        if num_found == 0:
            break

    variables[WORD_BOUNDARY_VAR_NAME] = WORD_BOUNDARY_VAR

    current_filter = all_chars

    for rule_type, rule in rules:
        if rule_type in (BIDIRECTIONAL_TRANSFORM, FORWARD_TRANSFORM):
            left, right = rule
            left = var_regex.sub(get_var, left)
            right = var_regex.sub(get_var, right)

            left_pre_context, left, left_post_context = context_regex.match(left).groups()
            right_pre_context, right, right_post_context = context_regex.match(right).groups()

            left_pre_context_max_len = 0
            left_post_context_max_len = 0

            left_pre_context_type = CONTEXT_TYPE_NONE
            left_post_context_type = CONTEXT_TYPE_NONE

            move = 0
            left_groups = []
            right_groups = []

            if left_pre_context:
                if left_pre_context.strip() == WORD_BOUNDARY_VAR:
                    left_pre_context = None
                    left_pre_context_type = CONTEXT_TYPE_WORD_BOUNDARY
                else:
                    left_pre_context, _, _ = char_permutations(left_pre_context.strip(), current_filter=current_filter)
                    left_pre_context_max_len = len(left_pre_context or [])
                    left_pre_context = char_types_string(left_pre_context)

                    if charset_regex.search(left_pre_context):
                        left_pre_context_type = CONTEXT_TYPE_REGEX
                    else:
                        left_pre_context_type = CONTEXT_TYPE_STRING

            if left:
                left, _, left_groups = char_permutations(left.strip(), current_filter=current_filter)
                if left_groups:
                    left_groups = format_groups(left, left_groups)
                else:
                    left_groups = None
                left = char_types_string(left)

            if left_post_context:
                if left_post_context.strip() == WORD_BOUNDARY_VAR:
                    left_post_context = None
                    left_post_context_type = CONTEXT_TYPE_WORD_BOUNDARY
                else:
                    left_post_context, _, _ = char_permutations(left_post_context.strip(), current_filter=current_filter)
                    left_post_context_max_len = len(left_post_context or [])
                    left_post_context = char_types_string(left_post_context)
                    if charset_regex.search(left_post_context):
                        left_post_context_type = CONTEXT_TYPE_REGEX
                    else:
                        left_post_context_type = CONTEXT_TYPE_STRING

            if right:
                right, move, right_groups = char_permutations(right.strip(), current_filter=current_filter)
                right = char_types_string(right)

            yield RULE, (left, left_pre_context_type, left_pre_context, left_pre_context_max_len,
                         left_post_context_type, left_post_context, left_post_context_max_len, left_groups, right, move)
        elif rule_type == PRE_TRANSFORM and rule.strip(': ').startswith('('):
            continue
        elif rule_type == PRE_TRANSFORM and '[' in rule and ']' in rule:
            filter_rule = regex_char_set_greedy.search(rule)
            current_filter = set(parse_regex_char_set(filter_rule.group(0)))
        elif rule_type == PRE_TRANSFORM:
            pre_transform = pre_transform_regex.search(rule)
            if pre_transform:
                yield TRANSFORM, pre_transform.group(1)


STEP_RULESET = 'STEP_RULESET'
STEP_TRANSFORM = 'STEP_TRANSFORM'
STEP_UNICODE_NORMALIZATION = 'STEP_UNICODE_NORMALIZATION'


NEW_STEP = 'NEW_STEP'
EXISTING_STEP = 'EXISTING_STEP'

# Extra rules defined here
supplemental_transliterations = {
    'latin-ascii': (EXISTING_STEP, [
        # German transliterations not handled by standard NFD normalization
        (u'"\xc3\xa4"', '2', CONTEXT_TYPE_NONE, '0', 'NULL', '0', CONTEXT_TYPE_NONE, '0', 'NULL', '0', u'"ae"', '2', '0', 'NULL', '0'),       # ä => ae
        (u'"\xc3\xb6"', '2', CONTEXT_TYPE_NONE, '0', 'NULL', '0', CONTEXT_TYPE_NONE, '0', 'NULL', '0', u'"oe"', '2', '0', 'NULL', '0'),       # ö => oe
        (u'"\xc3\xbc"', '2', CONTEXT_TYPE_NONE, '0', 'NULL', '0', CONTEXT_TYPE_NONE, '0', 'NULL', '0', u'"ue"', '2', '0', 'NULL', '0'),       # ü => ue
        (u'"\xc3\x9f"', '2', CONTEXT_TYPE_NONE, '0', 'NULL', '0', CONTEXT_TYPE_NONE, '0', 'NULL', '0', u'"ss"', '2', '0', 'NULL', '0'),       # ß => ss
    ]),
}


def get_all_transform_rules():
    transforms = {}
    to_latin = set()

    retain_transforms = set()

    init_unicode_categories()

    all_transforms = set([name.split('.xml')[0].lower() for name in get_transforms()])

    dependencies = defaultdict(list)

    for filename in get_transforms():
        name = filename.split('.xml')[0].lower()

        f = open(os.path.join(CLDR_TRANSFORMS_DIR, filename))
        xml = etree.parse(f)
        source, target = get_source_and_target(xml)

        if (target.lower() == 'latin' or name == 'latin-ascii') and name not in EXCLUDE_TRANSLITERATORS:
            to_latin.add(name)
            retain_transforms.add(name)

        print 'doing', filename

        steps = []
        rule_set = []

        for rule_type, rule in parse_transform_rules(xml):
            if rule_type == RULE:
                rule = format_rule(rule)
                rule_set.append(rule)
            elif rule_type == TRANSFORM:
                if rule_set:
                    steps.append((STEP_RULESET, rule_set))
                    rule_set = []

                if rule.lower() in all_transforms and rule.lower() not in EXCLUDE_TRANSLITERATORS:
                    dependencies[name].append(rule.lower())
                    steps.append((STEP_TRANSFORM, rule.lower()))

                rule = UTF8PROC_TRANSFORMS.get(rule, rule)
                if rule in UNICODE_NORMALIZATION_TRANSFORMS:
                    steps.append((STEP_UNICODE_NORMALIZATION, rule))

        if rule_set:
            steps.append((STEP_RULESET, rule_set))

        transforms[name] = steps

    dependency_queue = deque(to_latin)
    retain_transforms |= to_latin

    seen = set()

    while dependency_queue:
        name = dependency_queue.popleft()
        for dep in dependencies.get(name, []):
            retain_transforms.add(dep)
            if dep not in seen:
                dependency_queue.append(dep)
                seen.add(dep)

    all_rules = []
    all_steps = []
    all_transforms = []

    for name, steps in transforms.iteritems():
        if name in supplemental_transliterations:
            step_type, rules = supplemental_transliterations[name]
            if step_type == EXISTING_STEP:
                steps[-1][1].extend(rules)
            else:
                steps[-1].append((STEP_RULESET, rules))
        # Only care if it's a transform to Latin/ASCII or a dependency
        # for a transform to Latin/ASCII
        elif name not in retain_transforms:
            continue
        step_index = len(all_steps)
        num_steps = len(steps)
        for i, (step_type, data) in enumerate(steps):
            if step_type == STEP_RULESET:
                rule_index = len(all_rules)
                num_rules = len(data)
                step = (STEP_RULESET, str(rule_index), str(num_rules), quote_string(str(i)))
                all_rules.extend(data)
            elif step_type == STEP_TRANSFORM:
                step = (STEP_TRANSFORM, '-1', '-1', quote_string(data))
            elif step_type == STEP_UNICODE_NORMALIZATION:
                step = (STEP_UNICODE_NORMALIZATION, '-1', '-1', quote_string(data))
            all_steps.append(step)

        internal = int(name not in to_latin)

        transliterator = (quote_string(name), str(internal), str(step_index), str(num_steps))
        all_transforms.append(transliterator)

    return all_transforms, all_steps, all_rules


transliteration_data_template = u'''#include <stdlib.h>

transliteration_rule_source_t rules_source[] = {{
    {all_rules}
}};

transliteration_step_source_t steps_source[] = {{
    {all_steps}
}};

transliterator_source_t transliterators_source[] = {{
    {all_transforms}
}};

'''


def create_transliterator(name, internal, steps):
    return transliterator_template.format(name=name, internal=int(internal), num_steps=len(steps))


TRANSLITERATION_DATA_FILENAME = 'transliteration_data.c'


def main(out_dir):
    f = open(os.path.join(out_dir, TRANSLITERATION_DATA_FILENAME), 'w')
    transforms, steps, rules = get_all_transform_rules()

    all_transforms = u''',
    '''.join([u'{{{}}}'.format(u','.join(t)) for t in transforms])

    all_steps = u''',
    '''.join([u'{{{}}}'.format(u','.join(s)) for s in steps])

    all_rules = u''',
    '''.join([u'{{{}}}'.format(u','.join(r)) for r in rules])

    template = transliteration_data_template.format(
        all_transforms=all_transforms,
        all_steps=all_steps,
        all_rules=all_rules
    )

    f.write(safe_encode(template))


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print 'Usage: python transliteration_rules.py out_dir'
        exit(1)
    main(sys.argv[1])
