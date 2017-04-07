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
import six
import sys
import time
import urlparse
import unicodedata

from collections import defaultdict, deque

from lxml import etree

from scanner import Scanner
from unicode_data import *
from unicode_properties import *
from unicode_paths import CLDR_DIR
from geodata.encoding import safe_decode, safe_encode
from geodata.string_utils import NUM_CODEPOINTS, wide_unichr, wide_ord

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

START_OF_HAN_VAR_NAME = 'startOfHanMarker'
START_OF_HAN_VAR = '${}'.format(START_OF_HAN_VAR_NAME)

start_of_han_regex = re.compile(START_OF_HAN_VAR.replace('$', '\$'))

word_boundary_var_regex = re.compile(WORD_BOUNDARY_VAR.replace('$', '\$'))

WORD_BOUNDARY_CHAR = u'\u0001'
EMPTY_TRANSITION = u'\u0004'

NAMESPACE_SEPARATOR_CHAR = u"|"

WORD_BOUNDARY_CHAR = u"\x01"
PRE_CONTEXT_CHAR = u"\x86"
POST_CONTEXT_CHAR = u"\x87"
EMPTY_TRANSITION_CHAR = u"\x04"
REPEAT_CHAR = u"\x05"
GROUP_INDICATOR_CHAR = u"\x1d"
BEGIN_SET_CHAR = u"\x0f"
END_SET_CHAR = u"\x0e"

BIDIRECTIONAL_TRANSLITERATORS = {
    'fullwidth-halfwidth': 'halfwidth-fullwidth'
}

REVERSE_TRANSLITERATORS = {
    'latin-katakana': 'katakana-latin',
    'latin-conjoiningjamo': 'conjoiningjamo-latin',
}

EXCLUDE_TRANSLITERATORS = set([
    # Don't care about spaced Han because our tokenizer does it already
    'han-spacedhan',
    # Doesn't appear to be used in ICU
    'korean-latin-bgn',
])

TRANSLITERATOR_ALIASES = {
    'greek_latin_ungegn': 'greek-latin-ungegn'
}

NFD = 'NFD'
NFKD = 'NFKD'
NFC = 'NFC'
NFKC = 'NFKC'
STRIP_MARK = 'STRIP_MARK'

LOWER = 'lower'
UPPER = 'upper'
TITLE = 'title'

UNICODE_NORMALIZATION_TRANSFORMS = set([
    NFD,
    NFKD,
    NFC,
    NFKC,
    STRIP_MARK,
])


class TransliterationParseError(Exception):
    pass

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
pre_transform_regex = re.compile('[\s]*([^\s\(\)]*)[\s]*(?:\((.*)\)[\s]*)?', re.UNICODE)
assignment_regex = re.compile(u"(?:[\s]*(\$[^\s\=]+)[\s]*\=[\s]*(?!=[\s])(.*)(?<![\s])[\s]*)", re.UNICODE)
transform_regex = re.compile(u"(?:[\s]*(?!=[\s])(.*?)(?<![\s])[\s]*)((?:<>)|[←<→>↔])(?:[\s]*(?!=[\s])(.*)(?<![\s])[\s]*)", re.UNICODE)

newline_regex = re.compile('[\n]+')

quoted_string_regex = re.compile(r'\'.*?\'', re.UNICODE)

COMMENT_CHAR = '#'
END_CHAR = ';'


def unescape_unicode_char(m):
    return m.group(0).decode('unicode-escape')

escaped_unicode_regex = re.compile(r'\\u[0-9A-Fa-f]{4}')
escaped_wide_unicode_regex = re.compile(r'\\U[0-9A-Fa-f]{8}')

literal_space_regex = re.compile(r'(?:\\u0020|\\U00000020)')

# These are a few unicode property types that were needed by the transforms
unicode_property_regexes = [
    ('ideographic', '[〆〇〡-〩〸-〺㐀-䶵一-鿌豈-舘並-龎 𠀀-𪛖𪜀-𫜴𫝀-𫠝丽-𪘀]'),
    ('logical_order_exception', '[เ-ไ ເ-ໄ ꪵ ꪶ ꪹ ꪻ ꪼ]'),
]

rule_map = {
    u'[:Latin:] { [:Mn:]+ → ;': ':: {}'.format(STRIP_MARK),
    u':: [[[:Greek:][:Mn:][:Me:]] [\:-;?·;·]] ;': u':: [[[:Greek:][́̀᾿᾿˜̑῀¨ͺ´`῀᾿῎῍῏῾῞῝῟΅῭῁ˉ˘]] [\'\:-;?·;·]]',

}

unicode_properties = {}


def replace_literal_space(m):
    return "' '"

regex_char_set_greedy = re.compile(r'\[(.*)\]', re.UNICODE)
regex_char_set = re.compile(r'\[(.*?)(?<!\\)\]', re.UNICODE)

char_class_regex_str = '\[(?:[^\[\]]*\[[^\[\]]*\][^\[\]]*)*[^\[\]]*\]'

nested_char_class_regex = re.compile('\[(?:[^\[\]]*\[[^\[\]]*\][^\[\]]*)+[^\[\]]*\]', re.UNICODE)

range_regex = re.compile(r'[\\]?([^\\])\-[\\]?([^\\])', re.UNICODE)
var_regex = re.compile('[\s]*\$([A-Za-z_\-]+[A-Za-z_0-9\-]*)[\s]*')

context_regex = re.compile(u'(?:[\s]*(?!=[\s])(.*?)(?<![\s])[\s]*{)?(?:[\s]*([^}{]*)[\s]*)(?:}[\s]*(?!=[\s])(.*)(?<![\s])[\s]*)?', re.UNICODE)

paren_regex = re.compile(r'\(.*\)', re.UNICODE)

group_ref_regex_str = '\$[0-9]+'
group_ref_regex = re.compile(group_ref_regex_str)

comment_regex = re.compile('(?<!\')#(?!=\')')


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
WIDE_CHARACTER = 'WIDE_CHARACTER'
REVISIT = 'REVISIT'
REPEAT = 'REPEAT'
REPEAT_ONE = 'REPEAT_ONE'
LPAREN = 'LPAREN'
RPAREN = 'RPAREN'
WHITESPACE = 'WHITESPACE'
QUOTED_STRING = 'QUOTED_STRING'
SINGLE_QUOTE = 'SINGLE_QUOTE'
HTML_ENTITY = 'HTML_ENTITY'
SINGLE_QUOTE = 'SINGLE_QUOTE'
UNICODE_CHARACTER = 'UNICODE_CHARACTER'
UNICODE_WIDE_CHARACTER = 'UNICODE_WIDE_CHARACTER'
ESCAPED_CHARACTER = 'ESCAPED_CHARACTER'
END = 'END'
COMMENT = 'COMMENT'

BEFORE_CONTEXT = 'BEFORE_CONTEXT'
AFTER_CONTEXT = 'AFTER_CONTEXT'

PLUS = 'PLUS'
STAR = 'STAR'

rule_scanner = Scanner([
    (r'[\\].', ESCAPED_CHARACTER),
    (r'\'\'', SINGLE_QUOTE),
    (r'\'.*?\'', QUOTED_STRING),
    ('\[', OPEN_SET),
    ('\]', CLOSE_SET),
    ('\(', OPEN_GROUP),
    ('\)', CLOSE_GROUP),
    ('\{', BEFORE_CONTEXT),
    ('\}', AFTER_CONTEXT),
    ('[\s]+', WHITESPACE),
    (';', END),
    (r'[^\s]', CHARACTER),
])


# Scanner for the lvalue or rvalue of a transform rule

transform_scanner = Scanner([
    (r'\\u[0-9A-Fa-f]{4}', UNICODE_CHARACTER),
    (r'\\U[0-9A-Fa-f]{8}', UNICODE_WIDE_CHARACTER),
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
    (r'(?<![\\])\+', REPEAT_ONE),
    ('(?<=[^\s])\?', OPTIONAL),
    ('\(', LPAREN),
    ('\)', RPAREN),
    ('\|', REVISIT),
    ('[\s]+', WHITESPACE),
    (ur'[\ud800-\udbff][\udc00-\udfff]', WIDE_CHARACTER),
    (r'[^\s]', CHARACTER),
], re.UNICODE)

CHAR_RANGE = 'CHAR_RANGE'
CHAR_CLASS_PCRE = 'CHAR_CLASS_PCRE'
WORD_BOUNDARY = 'WORD_BOUNDARY'
NEGATION = 'NEGATION'
INTERSECTION = 'INTERSECTION'
DIFFERENCE = 'DIFFERENCE'
BRACKETED_CHARACTER = 'BRACKETED_CHARACTER'

# Scanner for a character set (yes, a regex regex)

char_set_scanner = Scanner([
    ('^\^', NEGATION),
    (r'\\p\{[^\{\}]+\}', CHAR_CLASS_PCRE),
    (r'[\\]?[^\\\s]\-[\\]?[^\s]', CHAR_RANGE),
    (r'\\u[0-9A-Fa-f]{4}', UNICODE_CHARACTER),
    (r'\\U[0-9A-Fa-f]{8}', UNICODE_WIDE_CHARACTER),
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
    ('-', DIFFERENCE),
    ('\$', WORD_BOUNDARY),
    (ur'[\ud800-\udbff][\udc00-\udfff]', WIDE_CHARACTER),
    (r'\{[^\s]+\}', BRACKETED_CHARACTER),
    (r'[^\s]', CHARACTER),
])

NUM_CODEPOINTS_16 = 65536

all_chars = set([unichr(i) for i in six.moves.xrange(NUM_CODEPOINTS_16)])

control_chars = set([c for c in all_chars if unicodedata.category(c) in ('Cc', 'Cn', 'Cs')])


def get_transforms(d=CLDR_TRANSFORMS_DIR):
    return [f for f in os.listdir(d) if f.endswith('.xml')]


def parse_transform_file(filename, d=CLDR_TRANSFORMS_DIR):
    f = open(os.path.join(d, filename))
    xml = etree.parse(f)
    return xml


def parse_transforms(d=CLDR_TRANSFORMS_DIR):
    for filename in get_transforms(d=d):
        name = filename.split('.xml')[0].lower()
        xml = parse_transform_file(filename)
        yield filename, name, xml


def replace_html_entity(ent):
    name = ent.strip('&;')
    return wide_unichr(htmlentitydefs.name2codepoint[name])


def parse_regex_char_range(regex):
    prev_char = None
    ranges = range_regex.findall(regex)
    regex = range_regex.sub('', regex)
    chars = [wide_ord(c) for c in regex]

    for start, end in ranges:
        start_ord = wide_ord(start)
        end_ord = wide_ord(end)

        if end_ord > start_ord:
            # Ranges are inclusive
            chars.extend([wide_unichr(c) for c in range(start_ord, end_ord + 1)])

    return chars

chars = get_chars_by_script()
all_scripts = build_master_scripts_list(chars)
script_codes = {k.lower(): v.lower() for k, v in six.iteritems(get_script_codes(all_scripts))}


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
            if value.lower() in unicode_scripts:
                chars = unicode_scripts[value.lower()]
            elif value.lower() in script_codes:
                chars = unicode_scripts[script_codes[value.lower()]]
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
        elif c.lower() in script_codes:
            chars = unicode_scripts[script_codes[c.lower()]]
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

    s = s[1:-1]
    is_negation = False
    this_group = set()
    is_intersection = False
    is_difference = False
    is_word_boundary = False

    real_chars = set()

    for token, token_class in parse_balanced_sets(s):
        if token_class == CHAR_RANGE:
            this_char_set = set(parse_regex_char_range(token))
            this_group |= this_char_set
        elif token_class == ESCAPED_CHARACTER:
            token = token.strip('\\')
            this_group.add(token)
            real_chars.add(token)
        elif token_class == SINGLE_QUOTE:
            t = "'"
            this_group.add(t)
            real_chars.add(t)
        elif token_class == QUOTED_STRING:
            t = token.strip("'")
            this_group.add(t)
            real_chars.add(t)
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
        elif token_class == CHARACTER and token not in control_chars:
            this_group.add(token)
            real_chars.add(token)
        elif token_class in (UNICODE_CHARACTER, UNICODE_WIDE_CHARACTER):
            token = token.decode('unicode-escape')
            if token not in control_chars:
                this_group.add(token)
                real_chars.add(token)
        elif token_class == WIDE_CHARACTER:
            if token not in control_chars:
                this_group.add(token)
                real_chars.add(token)
        elif token_class == BRACKETED_CHARACTER:
            if token.strip('{{}}') not in control_chars:
                this_group.add(token)
                real_chars.add(token)
        elif token_class == WORD_BOUNDARY:
            is_word_boundary = True

    if is_negation:
        this_group = current_filter - this_group

    return sorted((this_group & (current_filter | real_chars)) - control_chars) + ([WORD_BOUNDARY_CHAR] if is_word_boundary else [])


for name, regex_range in unicode_property_regexes:
    unicode_properties[name] = parse_regex_char_set(regex_range)

init_unicode_categories()


hangul_jamo_latin_filter = set(parse_regex_char_set("[['ᄀ-하-ᅵᆨ-ᇂ가-힣ㄱ-ㄿㅁ-ㅃㅅ-ㅣ㈀-㈜㉠-㉻가-힣＇ﾡ-ﾯﾱ-ﾳﾵ-ﾾￂ-ￇￊ-ￏￒ-ￗￚ-][:Latin:]]"))

custom_filters = {
    'conjoiningjamo-latin': hangul_jamo_latin_filter,
}


def get_source_and_target(name):
    name = TRANSLITERATOR_ALIASES.get(name.lower(), name.lower())
    components = name.split('-')[:2]
    if len(components) < 2:
        raise Exception(name)
    return components

def is_internal(xml):
    return xml.xpath('//transform/@visibility="internal"')


def split_rule(rule):
    splits = []
    current_token = []

    in_set = False
    in_group = False
    open_brackets = 0

    for token, token_type in rule_scanner.scan(rule):
        if token_type == ESCAPED_CHARACTER:
            current_token.append(token)
        elif token_type == OPEN_SET:
            in_set = True
            open_brackets += 1
            current_token.append(token)
        elif token_type == CLOSE_SET:
            open_brackets -= 1
            current_token.append(token)
            if open_brackets == 0:
                in_set = False
        elif token_type == END and not in_set:
            current_token.append(token)
            splits.append(u''.join(current_token).strip())
            current_token = []
        else:
            current_token.append(token)
    return splits


def get_raw_rules_and_variables(xml, reverse=False):
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

    nodes = xml.xpath('*//tRule')

    lines = [l for n in nodes for l in (newline_regex.split(n.text) if n.text else [])]
    if reverse:
        lines = reversed(lines)
    queue = deque(lines)

    while queue:
        rule = queue.popleft()

        rule = safe_decode(comment_regex.split(rule)[0].strip())

        splits = split_rule(rule)
        if len(splits) > 1:
            for r in splits[1:]:
                queue.appendleft(r)

        if rule.strip() not in rule_map:
            rule = literal_space_regex.sub(replace_literal_space, rule)
            rule = rule.rstrip(END_CHAR).strip()
        else:
            rule = rule_map[rule.strip()]

        if rule.strip().endswith('\\'):
            compound_rule.append(rule.rstrip('\\'))
            in_compound_rule = True
            continue
        elif in_compound_rule:
            compound_rule.append(rule)
            rule = u''.join(compound_rule)
            in_compound_rule = False
            compound_rule = []

        assignment = assignment_regex.match(rule)
        transform = transform_regex.match(rule)
        pre_transform = pre_transform_full_regex.match(rule)

        if pre_transform:
            rules.append((PRE_TRANSFORM, pre_transform.group(1)))
        elif assignment:
            lvalue, rvalue = assignment.groups()
            var_name = lvalue.strip().lstrip('$')
            rvalue = rvalue.strip()
            variables[var_name] = rvalue
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

    return rules, variables

CHAR_CLASSES = set([
    ESCAPED_CHARACTER,
    CHAR_CLASS,
    QUOTED_STRING,
    CHARACTER,
    GROUP_REF,
])


def char_permutations(s, current_filter=all_chars, reverse=False):
    '''
    char_permutations

    Parses the lvalue or rvalue of a transform rule into
    a list of character permutations, in addition to keeping
    track of revisits and regex groups
    '''

    if not s:
        return deque([EMPTY_TRANSITION_CHAR]), deque([]), []

    char_types = deque()

    add_char_type = deque.append if not reverse else deque.appendleft
    last_index = -1 if not reverse else 0

    revisit_char_types = deque()
    in_revisit = False

    in_group = False
    last_token_group_start = False

    start_group = 0
    end_group = 0

    open_brackets = 0
    current_set = []

    current_chars = char_types

    groups = []

    for token, token_type in transform_scanner.scan(s):
        if open_brackets > 0 and token_type not in (OPEN_SET, CLOSE_SET):
            current_set.append(token)
            continue

        if token_type == ESCAPED_CHARACTER:
            add_char_type(current_chars, [token.strip('\\')])
        elif token_type == OPEN_GROUP:
            in_group = True
            last_token_group_start = True
        elif token_type == CLOSE_GROUP:
            in_group = False
            end_group = len([c for c in current_chars if c[0] != REPEAT_CHAR])
            groups.append((start_group, end_group))
        elif token_type == OPEN_SET:
            open_brackets += 1
            current_set.append(token)
        elif token_type == CLOSE_SET:
            open_brackets -= 1
            current_set.append(token)
            if open_brackets == 0:
                char_set = parse_regex_char_set(u''.join(current_set), current_filter=current_filter)
                if char_set:
                    add_char_type(current_chars, char_set)
                current_set = []
        elif token_type == QUOTED_STRING:
            token = token.strip("'")
            for c in token:
                add_char_type(current_chars, [c])
        elif token_type == GROUP_REF:
            add_char_type(current_chars, [token.replace('$', GROUP_INDICATOR_CHAR)])
        elif token_type == REVISIT:
            in_revisit = True
            current_chars = revisit_char_types
        elif token_type == REPEAT:
            current_chars[last_index].append(EMPTY_TRANSITION_CHAR)
            if not reverse:
                add_char_type(current_chars, [REPEAT_CHAR])
            else:
                prev = current_chars.popleft()
                add_char_type(current_chars, [REPEAT_CHAR])
                add_char_type(current_chars, prev)
        elif token_type == REPEAT_ONE:
            if not reverse:
                add_char_type(current_chars, [REPEAT_CHAR])
            else:
                prev = current_chars.popleft()
                add_char_type(current_chars, [REPEAT_CHAR])
                add_char_type(current_chars, prev)
        elif token_type == OPTIONAL:
            current_chars[last_index].append(EMPTY_TRANSITION_CHAR)
        elif token_type == HTML_ENTITY:
            add_char_type(current_chars, [replace_html_entity(token)])
        elif token_type == CHARACTER:
            add_char_type(current_chars, [token])
        elif token_type == SINGLE_QUOTE:
            add_char_type(current_chars, ["'"])
        elif token_type in (UNICODE_CHARACTER, UNICODE_WIDE_CHARACTER):
            token = token.decode('unicode-escape')
            add_char_type(current_chars, [token])
        elif token_type == WIDE_CHARACTER:
            add_char_type(current_chars, [token])

        if in_group and last_token_group_start:
            start_group = len(current_chars)
            last_token_group_start = False

    return char_types, revisit_char_types, groups

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


def char_types_string(char_types, escape=True):
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
            if escape:
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
        group_regex.append(char_types_string(char_types[start:end]))
        group_regex.append(u')')
        last_end = end
    group_regex.append(char_types_string(char_types[last_end:]))
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
    revisit = rule[9]
    if not revisit:
        revisit = 'NULL'
        revisit_len = 0
    else:
        revisit = safe_encode(revisit)
        revisit_len = len(revisit)
        revisit = quote_string(escape_string(revisit))

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
        revisit,
        str(revisit_len),
        groups,
        str(groups_len),
    )

    return output_rule


def parse_transform_rules(name, xml, reverse=False, transforms_only=False):
    '''
    parse_transform_rules takes a parsed xml document as input
    and generates rules suitable for use in the C code.

    Since we're only concerned with transforming into Latin/ASCII,
    we don't care about backward transforms or two-way contexts.
    Only the lvalue's context needs to be used.
    '''

    rules, variables = get_raw_rules_and_variables(xml, reverse=reverse)

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
    variables[START_OF_HAN_VAR_NAME] = START_OF_HAN_VAR

    current_filter = custom_filters.get(name.lower(), all_chars)

    for rule_type, rule in rules:
        if transforms_only and rule_type != PRE_TRANSFORM:
            continue

        if not reverse and rule_type in (BIDIRECTIONAL_TRANSFORM, FORWARD_TRANSFORM):
            left, right = rule
        elif reverse and rule_type in (BIDIRECTIONAL_TRANSFORM, FORWARD_TRANSFORM, BACKWARD_TRANSFORM):
            right, left = rule
            if rule_type == BACKWARD_TRANSFORM:
                rule_type = FORWARD_TRANSFORM
            elif rule_type == FORWARD_TRANSFORM:
                rule_type = BACKWARD_TRANSFORM

        if rule_type in (BIDIRECTIONAL_TRANSFORM, FORWARD_TRANSFORM):
            left = var_regex.sub(get_var, left)
            right = var_regex.sub(get_var, right)

            left_pre_context = None
            left_post_context = None
            have_post_context = False
            current_token = []

            in_set = False
            in_group = False
            open_brackets = 0

            for token, token_type in rule_scanner.scan(left):
                if token_type == ESCAPED_CHARACTER:
                    current_token.append(token)
                elif token_type == OPEN_SET:
                    in_set = True
                    open_brackets += 1
                    current_token.append(token)
                elif token_type == CLOSE_SET:
                    open_brackets -= 1
                    current_token.append(token)
                    if open_brackets == 0:
                        in_set = False
                elif token_type == BEFORE_CONTEXT and not in_set:
                    left_pre_context = u''.join(current_token)

                    current_token = []
                elif token_type == AFTER_CONTEXT and not in_set:
                    have_post_context = True
                    left = u''.join(current_token)
                    current_token = []
                else:
                    current_token.append(token)

            if have_post_context:
                left_post_context = u''.join(current_token)
            else:
                left = u''.join(current_token).strip()

            right_pre_context = None
            right_post_context = None
            have_post_context = False
            current_token = []

            in_set = False
            in_group = False
            open_brackets = 0

            for token, token_type in rule_scanner.scan(right):
                if token_type == OPEN_SET:
                    in_set = True
                    open_brackets += 1
                    current_token.append(token)
                elif token_type == CLOSE_SET:
                    open_brackets -= 1
                    current_token.append(token)
                    if open_brackets == 0:
                        in_set = False
                elif token_type == BEFORE_CONTEXT and not in_set:
                    right_pre_context = u''.join(current_token)
                    current_token = []
                elif token_type == AFTER_CONTEXT and not in_set:
                    have_post_context = True
                    right = u''.join(current_token)
                    current_token = []
                else:
                    current_token.append(token)

            if have_post_context:
                right_post_context = u''.join(current_token)
            else:
                right = u''.join(current_token)

            if start_of_han_regex.search(left) or start_of_han_regex.search(right):
                continue

            left_pre_context_max_len = 0
            left_post_context_max_len = 0

            left_pre_context_type = CONTEXT_TYPE_NONE
            left_post_context_type = CONTEXT_TYPE_NONE

            left_groups = []
            right_groups = []

            revisit = None

            if left_pre_context:
                if left_pre_context.strip() == WORD_BOUNDARY_VAR:
                    left_pre_context = None
                    left_pre_context_type = CONTEXT_TYPE_WORD_BOUNDARY
                elif left_pre_context.strip() == START_OF_HAN_VAR:
                    left_pre_context = None
                    left_pre_context_type = CONTEXT_TYPE_NONE
                elif left_pre_context.strip():
                    left_pre_context, _, _ = char_permutations(left_pre_context.strip(), current_filter=current_filter, reverse=True)
                    if left_pre_context:
                        left_pre_context_max_len = len(left_pre_context or [])
                        left_pre_context = list(left_pre_context)
                        left_pre_context = char_types_string(left_pre_context)

                        if charset_regex.search(left_pre_context):
                            left_pre_context_type = CONTEXT_TYPE_REGEX
                        else:
                            left_pre_context_type = CONTEXT_TYPE_STRING
                    else:
                        left_pre_context = None
                        left_pre_context_type = CONTEXT_TYPE_NONE
            else:
                left_pre_context = None
                left_pre_context_type = CONTEXT_TYPE_NONE

            if left is not None:
                left_chars, _, left_groups = char_permutations(left.strip(), current_filter=current_filter)
                if not left_chars and (left.strip() or not (left_pre_context and left_post_context)):
                    print 'ignoring', rule
                    continue
                left_chars = list(left_chars)

                if left_groups:
                    left_groups = format_groups(left_chars, left_groups)
                else:
                    left_groups = None
                left = char_types_string(left_chars)

            if left_post_context:
                if left_post_context.strip() == WORD_BOUNDARY_VAR:
                    left_post_context = None
                    left_post_context_type = CONTEXT_TYPE_WORD_BOUNDARY
                elif left_post_context.strip() == START_OF_HAN_VAR:
                    left_pre_context_type = None
                    left_pre_context_type = CONTEXT_TYPE_NONE
                elif left_post_context.strip():
                    left_post_context, _, _ = char_permutations(left_post_context.strip(), current_filter=current_filter)
                    if left_post_context:
                        left_post_context_max_len = len(left_post_context or [])
                        left_post_context = list(left_post_context)
                        left_post_context = char_types_string(left_post_context)
                        if charset_regex.search(left_post_context):
                            left_post_context_type = CONTEXT_TYPE_REGEX
                        elif left_post_context:
                            left_post_context_type = CONTEXT_TYPE_STRING
                    else:
                        left_post_context = None
                        left_post_context_type = CONTEXT_TYPE_NONE
            else:
                left_post_context = None
                left_post_context_type = CONTEXT_TYPE_NONE

            if right:
                right, revisit, right_groups = char_permutations(right.strip(), current_filter=current_filter)
                right = char_types_string(right, escape=False)
                if revisit:
                    revisit = list(revisit)
                    revisit = char_types_string(revisit)
                else:
                    revisit = None

            yield RULE, (left, left_pre_context_type, left_pre_context, left_pre_context_max_len,
                         left_post_context_type, left_post_context, left_post_context_max_len, left_groups, right, revisit)
        elif rule_type == PRE_TRANSFORM and not reverse and rule.strip(': ').startswith('('):
            continue
        elif not reverse and rule_type == PRE_TRANSFORM and '[' in rule and ']' in rule:
            filter_rule = regex_char_set_greedy.search(rule)
            current_filter = set(parse_regex_char_set(filter_rule.group(0)))
        elif reverse and rule_type == PRE_TRANSFORM and '(' in rule and '[' in rule and ']' in rule and ')' in rule:
            rule = rule.strip(': ()')
            filter_rule = regex_char_set_greedy.search(rule)
            rule = regex_char_set_greedy.sub('', rule).strip()
            if rule:
                yield TRANSFORM, rule
            else:
                current_filter = set(parse_regex_char_set(filter_rule.group(0)))
        elif rule_type == PRE_TRANSFORM and not reverse:
            pre_transform = pre_transform_regex.search(rule)
            if pre_transform and pre_transform.group(1):
                yield TRANSFORM, pre_transform.group(1)
        elif rule_type == PRE_TRANSFORM and reverse:
            pre_transform = pre_transform_regex.search(rule)
            if pre_transform and pre_transform.group(2):
                yield TRANSFORM, pre_transform.group(2)


STEP_RULESET = 'STEP_RULESET'
STEP_TRANSFORM = 'STEP_TRANSFORM'
STEP_UNICODE_NORMALIZATION = 'STEP_UNICODE_NORMALIZATION'


NEW_STEP = 'NEW_STEP'
EXISTING_STEP = 'EXISTING_STEP'
PREPEND_STEP = 'PREPEND_STEP'


html_escapes = {'&{};'.format(name): safe_encode(wide_unichr(value))
                for name, value in six.iteritems(htmlentitydefs.name2codepoint)
                }

html_escapes.update({'&#{};'.format(i): safe_encode(wide_unichr(i))
                     for i in six.moves.xrange(NUM_CODEPOINTS_16)
                     })

# [[:Latin] & [:Ll:]]
latin_lower_set = 'abcdefghijklmnopqrstuvwxyz\\xc2\\xaa\\xc2\\xba\\xc3\\x9f\\xc3\\xa0\\xc3\\xa1\\xc3\\xa2\\xc3\\xa3\\xc3\\xa4\\xc3\\xa5\\xc3\\xa6\\xc3\\xa7\\xc3\\xa8\\xc3\\xa9\\xc3\\xaa\\xc3\\xab\\xc3\\xac\\xc3\\xad\\xc3\\xae\\xc3\\xaf\\xc3\\xb0\\xc3\\xb1\\xc3\\xb2\\xc3\\xb3\\xc3\\xb4\\xc3\\xb5\\xc3\\xb6\\xc3\\xb8\\xc3\\xb9\\xc3\\xba\\xc3\\xbb\\xc3\\xbc\\xc3\\xbd\\xc3\\xbe\\xc3\\xbf\\xc4\\x81\\xc4\\x83\\xc4\\x85\\xc4\\x87\\xc4\\x89\\xc4\\x8b\\xc4\\x8d\\xc4\\x8f\\xc4\\x91\\xc4\\x93\\xc4\\x95\\xc4\\x97\\xc4\\x99\\xc4\\x9b\\xc4\\x9d\\xc4\\x9f\\xc4\\xa1\\xc4\\xa3\\xc4\\xa5\\xc4\\xa7\\xc4\\xa9\\xc4\\xab\\xc4\\xad\\xc4\\xaf\\xc4\\xb1\\xc4\\xb3\\xc4\\xb5\\xc4\\xb7\\xc4\\xb8\\xc4\\xba\\xc4\\xbc\\xc4\\xbe\\xc5\\x80\\xc5\\x82\\xc5\\x84\\xc5\\x86\\xc5\\x88\\xc5\\x89\\xc5\\x8b\\xc5\\x8d\\xc5\\x8f\\xc5\\x91\\xc5\\x93\\xc5\\x95\\xc5\\x97\\xc5\\x99\\xc5\\x9b\\xc5\\x9d\\xc5\\x9f\\xc5\\xa1\\xc5\\xa3\\xc5\\xa5\\xc5\\xa7\\xc5\\xa9\\xc5\\xab\\xc5\\xad\\xc5\\xaf\\xc5\\xb1\\xc5\\xb3\\xc5\\xb5\\xc5\\xb7\\xc5\\xba\\xc5\\xbc\\xc5\\xbe\\xc5\\xbf\\xc6\\x80\\xc6\\x83\\xc6\\x85\\xc6\\x88\\xc6\\x8c\\xc6\\x8d\\xc6\\x92\\xc6\\x95\\xc6\\x99\\xc6\\x9a\\xc6\\x9b\\xc6\\x9e\\xc6\\xa1\\xc6\\xa3\\xc6\\xa5\\xc6\\xa8\\xc6\\xaa\\xc6\\xab\\xc6\\xad\\xc6\\xb0\\xc6\\xb4\\xc6\\xb6\\xc6\\xb9\\xc6\\xba\\xc6\\xbd\\xc6\\xbe\\xc6\\xbf\\xc7\\x86\\xc7\\x89\\xc7\\x8c\\xc7\\x8e\\xc7\\x90\\xc7\\x92\\xc7\\x94\\xc7\\x96\\xc7\\x98\\xc7\\x9a\\xc7\\x9c\\xc7\\x9d\\xc7\\x9f\\xc7\\xa1\\xc7\\xa3\\xc7\\xa5\\xc7\\xa7\\xc7\\xa9\\xc7\\xab\\xc7\\xad\\xc7\\xaf\\xc7\\xb0\\xc7\\xb3\\xc7\\xb5\\xc7\\xb9\\xc7\\xbb\\xc7\\xbd\\xc7\\xbf\\xc8\\x81\\xc8\\x83\\xc8\\x85\\xc8\\x87\\xc8\\x89\\xc8\\x8b\\xc8\\x8d\\xc8\\x8f\\xc8\\x91\\xc8\\x93\\xc8\\x95\\xc8\\x97\\xc8\\x99\\xc8\\x9b\\xc8\\x9d\\xc8\\x9f\\xc8\\xa1\\xc8\\xa3\\xc8\\xa5\\xc8\\xa7\\xc8\\xa9\\xc8\\xab\\xc8\\xad\\xc8\\xaf\\xc8\\xb1\\xc8\\xb3\\xc8\\xb4\\xc8\\xb5\\xc8\\xb6\\xc8\\xb7\\xc8\\xb8\\xc8\\xb9\\xc8\\xbc\\xc8\\xbf\\xc9\\x80\\xc9\\x82\\xc9\\x87\\xc9\\x89\\xc9\\x8b\\xc9\\x8d\\xc9\\x8f\\xc9\\x90\\xc9\\x91\\xc9\\x92\\xc9\\x93\\xc9\\x94\\xc9\\x95\\xc9\\x96\\xc9\\x97\\xc9\\x98\\xc9\\x99\\xc9\\x9a\\xc9\\x9b\\xc9\\x9c\\xc9\\x9d\\xc9\\x9e\\xc9\\x9f\\xc9\\xa0\\xc9\\xa1\\xc9\\xa2\\xc9\\xa3\\xc9\\xa4\\xc9\\xa5\\xc9\\xa6\\xc9\\xa7\\xc9\\xa8\\xc9\\xa9\\xc9\\xaa\\xc9\\xab\\xc9\\xac\\xc9\\xad\\xc9\\xae\\xc9\\xaf\\xc9\\xb0\\xc9\\xb1\\xc9\\xb2\\xc9\\xb3\\xc9\\xb4\\xc9\\xb5\\xc9\\xb6\\xc9\\xb7\\xc9\\xb8\\xc9\\xb9\\xc9\\xba\\xc9\\xbb\\xc9\\xbc\\xc9\\xbd\\xc9\\xbe\\xc9\\xbf\\xca\\x80\\xca\\x81\\xca\\x82\\xca\\x83\\xca\\x84\\xca\\x85\\xca\\x86\\xca\\x87\\xca\\x88\\xca\\x89\\xca\\x8a\\xca\\x8b\\xca\\x8c\\xca\\x8d\\xca\\x8e\\xca\\x8f\\xca\\x90\\xca\\x91\\xca\\x92\\xca\\x93\\xca\\x95\\xca\\x96\\xca\\x97\\xca\\x98\\xca\\x99\\xca\\x9a\\xca\\x9b\\xca\\x9c\\xca\\x9d\\xca\\x9e\\xca\\x9f\\xca\\xa0\\xca\\xa1\\xca\\xa2\\xca\\xa3\\xca\\xa4\\xca\\xa5\\xca\\xa6\\xca\\xa7\\xca\\xa8\\xca\\xa9\\xca\\xaa\\xca\\xab\\xca\\xac\\xca\\xad\\xca\\xae\\xca\\xaf\\xe1\\xb4\\x80\\xe1\\xb4\\x81\\xe1\\xb4\\x82\\xe1\\xb4\\x83\\xe1\\xb4\\x84\\xe1\\xb4\\x85\\xe1\\xb4\\x86\\xe1\\xb4\\x87\\xe1\\xb4\\x88\\xe1\\xb4\\x89\\xe1\\xb4\\x8a\\xe1\\xb4\\x8b\\xe1\\xb4\\x8c\\xe1\\xb4\\x8d\\xe1\\xb4\\x8e\\xe1\\xb4\\x8f\\xe1\\xb4\\x90\\xe1\\xb4\\x91\\xe1\\xb4\\x92\\xe1\\xb4\\x93\\xe1\\xb4\\x94\\xe1\\xb4\\x95\\xe1\\xb4\\x96\\xe1\\xb4\\x97\\xe1\\xb4\\x98\\xe1\\xb4\\x99\\xe1\\xb4\\x9a\\xe1\\xb4\\x9b\\xe1\\xb4\\x9c\\xe1\\xb4\\x9d\\xe1\\xb4\\x9e\\xe1\\xb4\\x9f\\xe1\\xb4\\xa0\\xe1\\xb4\\xa1\\xe1\\xb4\\xa2\\xe1\\xb4\\xa3\\xe1\\xb4\\xa4\\xe1\\xb4\\xa5\\xe1\\xb5\\xa2\\xe1\\xb5\\xa3\\xe1\\xb5\\xa4\\xe1\\xb5\\xa5\\xe1\\xb5\\xab\\xe1\\xb5\\xac\\xe1\\xb5\\xad\\xe1\\xb5\\xae\\xe1\\xb5\\xaf\\xe1\\xb5\\xb0\\xe1\\xb5\\xb1\\xe1\\xb5\\xb2\\xe1\\xb5\\xb3\\xe1\\xb5\\xb4\\xe1\\xb5\\xb5\\xe1\\xb5\\xb6\\xe1\\xb5\\xb7\\xe1\\xb5\\xb9\\xe1\\xb5\\xba\\xe1\\xb5\\xbb\\xe1\\xb5\\xbc\\xe1\\xb5\\xbd\\xe1\\xb5\\xbe\\xe1\\xb5\\xbf\\xe1\\xb6\\x80\\xe1\\xb6\\x81\\xe1\\xb6\\x82\\xe1\\xb6\\x83\\xe1\\xb6\\x84\\xe1\\xb6\\x85\\xe1\\xb6\\x86\\xe1\\xb6\\x87\\xe1\\xb6\\x88\\xe1\\xb6\\x89\\xe1\\xb6\\x8a\\xe1\\xb6\\x8b\\xe1\\xb6\\x8c\\xe1\\xb6\\x8d\\xe1\\xb6\\x8e\\xe1\\xb6\\x8f\\xe1\\xb6\\x90\\xe1\\xb6\\x91\\xe1\\xb6\\x92\\xe1\\xb6\\x93\\xe1\\xb6\\x94\\xe1\\xb6\\x95\\xe1\\xb6\\x96\\xe1\\xb6\\x97\\xe1\\xb6\\x98\\xe1\\xb6\\x99\\xe1\\xb6\\x9a\\xe1\\xb8\\x81\\xe1\\xb8\\x83\\xe1\\xb8\\x85\\xe1\\xb8\\x87\\xe1\\xb8\\x89\\xe1\\xb8\\x8b\\xe1\\xb8\\x8d\\xe1\\xb8\\x8f\\xe1\\xb8\\x91\\xe1\\xb8\\x93\\xe1\\xb8\\x95\\xe1\\xb8\\x97\\xe1\\xb8\\x99\\xe1\\xb8\\x9b\\xe1\\xb8\\x9d\\xe1\\xb8\\x9f\\xe1\\xb8\\xa1\\xe1\\xb8\\xa3\\xe1\\xb8\\xa5\\xe1\\xb8\\xa7\\xe1\\xb8\\xa9\\xe1\\xb8\\xab\\xe1\\xb8\\xad\\xe1\\xb8\\xaf\\xe1\\xb8\\xb1\\xe1\\xb8\\xb3\\xe1\\xb8\\xb5\\xe1\\xb8\\xb7\\xe1\\xb8\\xb9\\xe1\\xb8\\xbb\\xe1\\xb8\\xbd\\xe1\\xb8\\xbf\\xe1\\xb9\\x81\\xe1\\xb9\\x83\\xe1\\xb9\\x85\\xe1\\xb9\\x87\\xe1\\xb9\\x89\\xe1\\xb9\\x8b\\xe1\\xb9\\x8d\\xe1\\xb9\\x8f\\xe1\\xb9\\x91\\xe1\\xb9\\x93\\xe1\\xb9\\x95\\xe1\\xb9\\x97\\xe1\\xb9\\x99\\xe1\\xb9\\x9b\\xe1\\xb9\\x9d\\xe1\\xb9\\x9f\\xe1\\xb9\\xa1\\xe1\\xb9\\xa3\\xe1\\xb9\\xa5\\xe1\\xb9\\xa7\\xe1\\xb9\\xa9\\xe1\\xb9\\xab\\xe1\\xb9\\xad\\xe1\\xb9\\xaf\\xe1\\xb9\\xb1\\xe1\\xb9\\xb3\\xe1\\xb9\\xb5\\xe1\\xb9\\xb7\\xe1\\xb9\\xb9\\xe1\\xb9\\xbb\\xe1\\xb9\\xbd\\xe1\\xb9\\xbf\\xe1\\xba\\x81\\xe1\\xba\\x83\\xe1\\xba\\x85\\xe1\\xba\\x87\\xe1\\xba\\x89\\xe1\\xba\\x8b\\xe1\\xba\\x8d\\xe1\\xba\\x8f\\xe1\\xba\\x91\\xe1\\xba\\x93\\xe1\\xba\\x95\\xe1\\xba\\x96\\xe1\\xba\\x97\\xe1\\xba\\x98\\xe1\\xba\\x99\\xe1\\xba\\x9a\\xe1\\xba\\x9b\\xe1\\xba\\x9c\\xe1\\xba\\x9d\\xe1\\xba\\x9f\\xe1\\xba\\xa1\\xe1\\xba\\xa3\\xe1\\xba\\xa5\\xe1\\xba\\xa7\\xe1\\xba\\xa9\\xe1\\xba\\xab\\xe1\\xba\\xad\\xe1\\xba\\xaf\\xe1\\xba\\xb1\\xe1\\xba\\xb3\\xe1\\xba\\xb5\\xe1\\xba\\xb7\\xe1\\xba\\xb9\\xe1\\xba\\xbb\\xe1\\xba\\xbd\\xe1\\xba\\xbf\\xe1\\xbb\\x81\\xe1\\xbb\\x83\\xe1\\xbb\\x85\\xe1\\xbb\\x87\\xe1\\xbb\\x89\\xe1\\xbb\\x8b\\xe1\\xbb\\x8d\\xe1\\xbb\\x8f\\xe1\\xbb\\x91\\xe1\\xbb\\x93\\xe1\\xbb\\x95\\xe1\\xbb\\x97\\xe1\\xbb\\x99\\xe1\\xbb\\x9b\\xe1\\xbb\\x9d\\xe1\\xbb\\x9f\\xe1\\xbb\\xa1\\xe1\\xbb\\xa3\\xe1\\xbb\\xa5\\xe1\\xbb\\xa7\\xe1\\xbb\\xa9\\xe1\\xbb\\xab\\xe1\\xbb\\xad\\xe1\\xbb\\xaf\\xe1\\xbb\\xb1\\xe1\\xbb\\xb3\\xe1\\xbb\\xb5\\xe1\\xbb\\xb7\\xe1\\xbb\\xb9\\xe1\\xbb\\xbb\\xe1\\xbb\\xbd\\xe1\\xbb\\xbf\\xe2\\x85\\x8e\\xe2\\x86\\x84\\xe2\\xb1\\xa1\\xe2\\xb1\\xa5\\xe2\\xb1\\xa6\\xe2\\xb1\\xa8\\xe2\\xb1\\xaa\\xe2\\xb1\\xac\\xe2\\xb1\\xb1\\xe2\\xb1\\xb3\\xe2\\xb1\\xb4\\xe2\\xb1\\xb6\\xe2\\xb1\\xb7\\xe2\\xb1\\xb8\\xe2\\xb1\\xb9\\xe2\\xb1\\xba\\xe2\\xb1\\xbb\\xe2\\xb1\\xbc\\xea\\x9c\\xa3\\xea\\x9c\\xa5\\xea\\x9c\\xa7\\xea\\x9c\\xa9\\xea\\x9c\\xab\\xea\\x9c\\xad\\xea\\x9c\\xaf\\xea\\x9c\\xb0\\xea\\x9c\\xb1\\xea\\x9c\\xb3\\xea\\x9c\\xb5\\xea\\x9c\\xb7\\xea\\x9c\\xb9\\xea\\x9c\\xbb\\xea\\x9c\\xbd\\xea\\x9c\\xbf\\xea\\x9d\\x81\\xea\\x9d\\x83\\xea\\x9d\\x85\\xea\\x9d\\x87\\xea\\x9d\\x89\\xea\\x9d\\x8b\\xea\\x9d\\x8d\\xea\\x9d\\x8f\\xea\\x9d\\x91\\xea\\x9d\\x93\\xea\\x9d\\x95\\xea\\x9d\\x97\\xea\\x9d\\x99\\xea\\x9d\\x9b\\xea\\x9d\\x9d\\xea\\x9d\\x9f\\xea\\x9d\\xa1\\xea\\x9d\\xa3\\xea\\x9d\\xa5\\xea\\x9d\\xa7\\xea\\x9d\\xa9\\xea\\x9d\\xab\\xea\\x9d\\xad\\xea\\x9d\\xaf\\xea\\x9d\\xb1\\xea\\x9d\\xb2\\xea\\x9d\\xb3\\xea\\x9d\\xb4\\xea\\x9d\\xb5\\xea\\x9d\\xb6\\xea\\x9d\\xb7\\xea\\x9d\\xb8\\xea\\x9d\\xba\\xea\\x9d\\xbc\\xea\\x9d\\xbf\\xea\\x9e\\x81\\xea\\x9e\\x83\\xea\\x9e\\x85\\xea\\x9e\\x87\\xea\\x9e\\x8c\\xef\\xac\\x80\\xef\\xac\\x81\\xef\\xac\\x82\\xef\\xac\\x83\\xef\\xac\\x84\\xef\\xac\\x85\\xef\\xac\\x86\\xef\\xbd\\x81\\xef\\xbd\\x82\\xef\\xbd\\x83\\xef\\xbd\\x84\\xef\\xbd\\x85\\xef\\xbd\\x86\\xef\\xbd\\x87\\xef\\xbd\\x88\\xef\\xbd\\x89\\xef\\xbd\\x8a\\xef\\xbd\\x8b\\xef\\xbd\\x8c\\xef\\xbd\\x8d\\xef\\xbd\\x8e\\xef\\xbd\\x8f\\xef\\xbd\\x90\\xef\\xbd\\x91\\xef\\xbd\\x92\\xef\\xbd\\x93\\xef\\xbd\\x94\\xef\\xbd\\x95\\xef\\xbd\\x96\\xef\\xbd\\x97\\xef\\xbd\\x98\\xef\\xbd\\x99\\xef\\xbd\\x9a'

latin_lower_rule = '[{}]'.format(latin_lower_set)
latin_lower_rule_len = len(latin_lower_rule.decode('string-escape'))
latin_lower_rule = quote_string(latin_lower_rule)

html_escape_step = [(quote_string(name), str(len(name)), CONTEXT_TYPE_NONE, '0', 'NULL', '0', CONTEXT_TYPE_NONE, '0', 'NULL', '0', quote_string(escape_string(value)), str(len(value)), 'NULL', '0', 'NULL', '0')
                    for name, value in six.iteritems(html_escapes)
                    ]

HTML_ESCAPE = 'html-escape'
LATIN_ASCII = 'latin-ascii'
LATIN_ASCII_SIMPLE = 'latin-ascii-simple'
GERMAN_ASCII = 'german-ascii'
SCANDINAVIAN_ASCII = 'scandinavian-ascii'

custom_transforms = {
    GERMAN_ASCII: {
        safe_decode('ä'): safe_decode('ae'),
        safe_decode('Ä'): safe_decode('AE'),
        safe_decode('ö'): safe_decode('oe'),
        safe_decode('Ö'): safe_decode('OE'),
        safe_decode('ü'): safe_decode('ue'),
        safe_decode('Ü'): safe_decode('UE'),
    },
    SCANDINAVIAN_ASCII: {
        safe_decode('ø'): safe_decode('oe'),
        safe_decode('Ø'): safe_decode('OE'),
        safe_decode('å'): safe_decode('aa'),
        safe_decode('Å'): safe_decode('AA'),
    },
}


def make_char_rules(transforms, char_lower, char_upper, titlecase=True):
    lower_char_encoded = safe_encode(char_lower)
    upper_char_encoded = safe_encode(char_upper)
    lower_replacement = safe_encode(transforms[char_lower])
    upper_replacement = safe_encode(transforms[char_upper])

    rules = [
        (safe_decode(quote_string(escape_string(safe_encode(lower_char_encoded)))), safe_encode(len(lower_char_encoded)), CONTEXT_TYPE_NONE, '0', 'NULL', '0', CONTEXT_TYPE_NONE, '0', 'NULL', '0', safe_decode(quote_string(escape_string(safe_encode(lower_replacement)))), safe_encode(len(lower_replacement)), 'NULL', '0', 'NULL', '0'),
    ]
    if titlecase:
        title_replacement = upper_replacement.title()
        rules.append((safe_decode(quote_string(escape_string(safe_encode(upper_char_encoded)))), safe_encode(len(upper_char_encoded)), CONTEXT_TYPE_NONE, '0', 'NULL', '0', CONTEXT_TYPE_REGEX, '1', latin_lower_rule, safe_encode(latin_lower_rule_len), safe_decode(quote_string(escape_string(safe_encode(title_replacement)))), safe_encode(len(title_replacement)), 'NULL', '0', 'NULL', '0'))

    rules.append((safe_decode(quote_string(escape_string(safe_encode(upper_char_encoded)))), safe_encode(len(upper_char_encoded)), CONTEXT_TYPE_NONE, '0', 'NULL', '0', CONTEXT_TYPE_NONE, '0', 'NULL', '0', safe_decode(quote_string(escape_string(safe_encode(upper_replacement)))), safe_encode(len(lower_replacement)), 'NULL', '0', 'NULL', '0'))
    return rules


extra_transforms = {
    HTML_ESCAPE: [
        (STEP_RULESET, html_escape_step)
    ],
    GERMAN_ASCII: [
        (STEP_RULESET, 
            make_char_rules(custom_transforms[GERMAN_ASCII], safe_decode('ä'), safe_decode('Ä')) + \
            make_char_rules(custom_transforms[GERMAN_ASCII], safe_decode('ö'), safe_decode('Ö')) + \
            make_char_rules(custom_transforms[GERMAN_ASCII], safe_decode('ü'), safe_decode('Ü'))
        ),
        (STEP_TRANSFORM, LATIN_ASCII),
    ],

    # Swedish/Danish/Norwegian transliterations not handled by standard NFD or Latin-ASCII
    SCANDINAVIAN_ASCII: [
        (STEP_RULESET,
            make_char_rules(custom_transforms[SCANDINAVIAN_ASCII], safe_decode('ø'), safe_decode('Ø')) + \
            make_char_rules(custom_transforms[SCANDINAVIAN_ASCII], safe_decode('å'), safe_decode('Å'))
        ),
        (STEP_TRANSFORM, LATIN_ASCII),
    ],

}


# Extra rules defined here
supplemental_transliterations = {
    LATIN_ASCII: [
        # Prepend transformations get applied in the reverse order of their appearance here
        (PREPEND_STEP, [(STEP_TRANSFORM, HTML_ESCAPE)]),
    ],
}


def simple_latin_ruleset():
    xml = parse_transform_file('Latin-ASCII.xml')

    category_chars = get_unicode_categories()
    cats = [None] * 0x10ffff
    for cat, chars in six.iteritems(category_chars):
        for c in chars:
            cats[wide_ord(c)] = cat

    ruleset = [(STEP_TRANSFORM, HTML_ESCAPE)]

    simple_rules = []

    for rule_type, rule in parse_transform_rules(LATIN_ASCII, xml):
        if rule_type == RULE:
            key = safe_decode(rule[0])
            pre_context_type = rule[1]
            post_context_type = rule[4]
            value = safe_decode(rule[8])

            if len(key) == 1 and len(value) == 1 and pre_context_type == CONTEXT_TYPE_NONE and post_context_type == CONTEXT_TYPE_NONE:
                cat = cats[wide_ord(key)]
                # Only use punctuation rules, not numeric
                if not cat.startswith('L') and not cat.startswith('N'):
                    simple_rules.append(format_rule(rule))

    ruleset.append((STEP_RULESET, simple_rules))

    return ruleset

extra_transforms[LATIN_ASCII_SIMPLE] = simple_latin_ruleset()


def get_all_transform_rules():
    transforms = {}
    to_latin = set()

    retain_transforms = set()

    all_transforms = set([name.split('.xml')[0].lower() for name in get_transforms()])

    name_aliases = {}

    dependencies = defaultdict(list)

    for filename in get_transforms():
        name = filename.split('.xml')[0].lower()

        if name in REVERSE_TRANSLITERATORS:
            all_transforms.add(REVERSE_TRANSLITERATORS[name])
        elif name in BIDIRECTIONAL_TRANSLITERATORS:
            all_transforms.add(BIDIRECTIONAL_TRANSLITERATORS[name])

    for filename, name, xml in parse_transforms():
        source, target = get_source_and_target(name)
        name_alias = '-'.join([source.lower(), target.lower()])
        if name_alias not in name_aliases and name_alias != name:
            name_aliases[name_alias] = name

    def add_dependencies(name, xml, reverse=False):
        for rule_type, rule in parse_transform_rules(name, xml, reverse=reverse, transforms_only=True):
            rule = rule.lower()
            if rule in all_transforms and rule not in EXCLUDE_TRANSLITERATORS:
                dependencies[name].append(rule)
            elif rule in name_aliases and rule not in EXCLUDE_TRANSLITERATORS:
                dependencies[name].append(name_aliases[rule])
            elif rule.split('-')[0] in all_transforms and rule.split('-')[0] not in EXCLUDE_TRANSLITERATORS:
                dependencies[name].append(rule.split('-')[0])

    for filename, name, xml in parse_transforms():
        reverse = name in REVERSE_TRANSLITERATORS

        bidirectional = name in BIDIRECTIONAL_TRANSLITERATORS

        deps = []
        if not reverse and not bidirectional:
            add_dependencies(name, xml, reverse=False)
        elif reverse:
            name = REVERSE_TRANSLITERATORS[name]
            add_dependencies(name, xml, reverse=True)
        elif bidirectional:
            add_dependencies(name, xml, reverse=False)
            name = BIDIRECTIONAL_TRANSLITERATORS[name]
            add_dependencies(name, xml, reverse=True)

    def parse_steps(name, xml, reverse=False):
        steps = []
        rule_set = []

        for rule_type, rule in parse_transform_rules(name, xml, reverse=reverse):
            if rule_type == RULE:
                rule = format_rule(rule)
                rule_set.append(rule)
            elif rule_type == TRANSFORM:
                if rule_set:
                    steps.append((STEP_RULESET, rule_set))
                    rule_set = []

                if rule.lower() in all_transforms and rule.lower() not in EXCLUDE_TRANSLITERATORS:
                    steps.append((STEP_TRANSFORM, rule.lower()))
                elif rule.lower() in name_aliases and rule.lower() not in EXCLUDE_TRANSLITERATORS:
                    dep = name_aliases[rule.lower()]
                    steps.append((STEP_TRANSFORM, dep))
                elif rule.split('-')[0].lower() in all_transforms and rule.split('-')[0].lower() not in EXCLUDE_TRANSLITERATORS:
                    steps.append((STEP_TRANSFORM, rule.split('-')[0].lower()))

                rule = UTF8PROC_TRANSFORMS.get(rule, rule)
                if rule in UNICODE_NORMALIZATION_TRANSFORMS:
                    steps.append((STEP_UNICODE_NORMALIZATION, rule))

        if rule_set:
            steps.append((STEP_RULESET, rule_set))

        return steps

    for filename, name, xml in parse_transforms():
        source, target = get_source_and_target(name)
        internal = is_internal(xml)

        if name in EXCLUDE_TRANSLITERATORS:
            continue

        reverse = name in REVERSE_TRANSLITERATORS

        bidirectional = name in BIDIRECTIONAL_TRANSLITERATORS

        if target.lower() == 'latin' or name == LATIN_ASCII and not internal:
            to_latin.add(name)
            retain_transforms.add(name)
        elif (reverse and source.lower() == 'latin') and not internal:
            to_latin.add(REVERSE_TRANSLITERATORS[name])
            retain_transforms.add(REVERSE_TRANSLITERATORS[name])
        elif (bidirectional and source.lower() == 'latin') and not internal:
            to_latin.add(BIDIRECTIONAL_TRANSLITERATORS[name])
            retain_transforms.add(name)
            retain_transforms.add(BIDIRECTIONAL_TRANSLITERATORS[name])

    dependency_queue = deque(to_latin)
    retain_transforms |= to_latin

    print retain_transforms

    seen = set()

    while dependency_queue:
        name = dependency_queue.popleft()
        for dep in dependencies.get(name, []):
            retain_transforms.add(dep)
            if dep not in seen:
                dependency_queue.append(dep)
                seen.add(dep)

    rules_data = []
    steps_data = []
    transforms_data = []

    for filename, name, xml in parse_transforms():
        if name in EXCLUDE_TRANSLITERATORS:
            continue

        reverse = name in REVERSE_TRANSLITERATORS

        bidirectional = name in BIDIRECTIONAL_TRANSLITERATORS

        normalized_name = None

        if reverse:
            normalized_name = REVERSE_TRANSLITERATORS[name]

        if bidirectional:
            normalized_name = BIDIRECTIONAL_TRANSLITERATORS[name]

        # Only care if it's a transform to Latin/ASCII or a dependency
        # for a transform to Latin/ASCII
        if name not in retain_transforms and normalized_name not in retain_transforms:
            print('skipping {}'.format(filename))
            continue

        print('doing {}'.format(filename))

        if not reverse and not bidirectional:
            steps = parse_steps(name, xml, reverse=False)
            transforms[name] = steps
        elif reverse:
            name = REVERSE_TRANSLITERATORS[name]
            steps = parse_steps(name, xml, reverse=True)
            transforms[name] = steps
        elif bidirectional:
            steps = parse_steps(name, xml, reverse=False)
            transforms[name] = steps
            name = BIDIRECTIONAL_TRANSLITERATORS[name]
            steps = parse_steps(name, xml, reverse=True)
            transforms[name] = steps

    transforms.update(extra_transforms)

    for name, steps in transforms.iteritems():
        if name in supplemental_transliterations:
            for step_type, rules in supplemental_transliterations[name]:
                if step_type == EXISTING_STEP:
                    steps[-1][1].extend(rules)
                elif step_type == PREPEND_STEP:
                    steps = rules + steps
                else:
                    steps.append((STEP_RULESET, rules))
        step_index = len(steps_data)
        num_steps = len(steps)
        for i, (step_type, data) in enumerate(steps):
            if step_type == STEP_RULESET:
                rule_index = len(rules_data)
                num_rules = len(data)
                step = (STEP_RULESET, str(rule_index), str(num_rules), quote_string(str(i)))
                rules_data.extend(data)
            elif step_type == STEP_TRANSFORM:
                step = (STEP_TRANSFORM, '-1', '-1', quote_string(data))
            elif step_type == STEP_UNICODE_NORMALIZATION:
                step = (STEP_UNICODE_NORMALIZATION, '-1', '-1', quote_string(data))
            steps_data.append(step)

        internal = int(name not in to_latin)

        transliterator = (quote_string(name.replace('_', '-')), str(internal), str(step_index), str(num_steps))
        transforms_data.append(transliterator)

    return transforms_data, steps_data, rules_data


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


transliterator_script_data_template = u'''
#include "unicode_scripts.h"

typedef struct script_transliteration_rule {{
    script_language_t script_language;
    transliterator_index_t index;
}} script_transliteration_rule_t;

script_transliteration_rule_t script_transliteration_rules[] = {{
    {rules}
}};

char *script_transliterators[] = {{
    {transliterators}
}};

'''

script_transliterators = {
    'arabic': {None: ['arabic-latin', 'arabic-latin-bgn'],
               'fa': ['persian-latin-bgn'],
               'ps': ['pashto-latin-bgn'],
               },
    'armenian': {None: ['armenian-latin-bgn']},
    'balinese': None,
    'bamum': None,
    'batak': None,
    'bengali': {None: ['bengali-latin']},
    'bopomofo': None,
    'braille': None,
    'buginese': None,
    'buhid': None,
    'canadian_aboriginal': {None: ['canadianaboriginal-latin']},
    'cham': None,
    'cherokee': None,
    'common': {None: [LATIN_ASCII],
               'de': [GERMAN_ASCII],
               'et': [GERMAN_ASCII],
               'da': [SCANDINAVIAN_ASCII, LATIN_ASCII],
               'nb': [SCANDINAVIAN_ASCII, LATIN_ASCII],
               'sv': [SCANDINAVIAN_ASCII, LATIN_ASCII],
               },
    'coptic': None,
    'cyrillic': {None: ['cyrillic-latin'],
                 'be': ['belarusian-latin-bgn'],
                 'ru': ['russian-latin-bgn'],
                 'bg': ['bulgarian-latin-bgn'],
                 'kk': ['kazakh-latin-bgn'],
                 'ky': ['kirghiz-latin-bgn'],
                 'mk': ['macedonian-latin-bgn'],
                 'mn': ['mongolian-latin-bgn'],
                 'sr': ['serbian-latin-bgn'],
                 'uk': ['ukrainian-latin-bgn'],
                 'uz': ['uzbek-latin-bgn'],
                 },
    'devanagari': {None: ['devanagari-latin']},
    'ethiopic': None,
    'georgian': {None: ['georgian-latin', 'georgian-latin-bgn']},
    'glagolitic': None,
    'greek': {None: ['greek-latin', 'greek-latin-bgn', 'greek-latin-ungegn']},
    'gujarati': {None: ['gujarati-latin']},
    'gurmukhi': {None: ['gurmukhi-latin']},
    'han': {None: ['han-latin']},
    'hangul': {None: ['hangul-latin']},
    'hanunoo': None,
    'hebrew': {None: ['hebrew-latin', 'hebrew-latin-bgn']},
    'hiragana': {None: ['hiragana-latin']},
    'inherited': None,
    'javanese': None,
    'kannada': {None: ['kannada-latin']},
    'katakana': {None: ['katakana-latin', 'katakana-latin-bgn']},
    'kayah_li': None,
    'khmer': None,
    'lao': None,
    'latin': {None: [LATIN_ASCII],
              'de': [GERMAN_ASCII],
              'et': [GERMAN_ASCII],
              'da': [SCANDINAVIAN_ASCII, LATIN_ASCII],
              'nb': [SCANDINAVIAN_ASCII, LATIN_ASCII],
              'sv': [SCANDINAVIAN_ASCII, LATIN_ASCII],
              },
    'lepcha': None,
    'limbu': None,
    'lisu': None,
    'malayalam': {None: ['malayalam-latin']},
    'mandaic': None,
    'meetei_mayek': None,
    'mongolian': None,
    'myanmar': None,
    'new_tai_lue': None,
    'nko': None,
    'ogham': None,
    'ol_chiki': None,
    'oriya': {None: ['oriya-latin']},
    'phags_pa': None,
    'rejang': None,
    'runic': None,
    'samaritan': None,
    'saurashtra': None,
    'sinhala': None,
    'sundanese': None,
    'syloti_nagri': None,
    'syriac': None,
    'tagalog': None,
    'tagbanwa': None,
    'tai_le': None,
    'tai_tham': None,
    'tai_viet': None,
    'tamil': {None: ['tamil-latin']},
    'telugu': {None: ['telugu-latin']},
    'thaana': {None: ['thaana-latin', 'maldivian-latin-bgn']},
    'thai': {None: ['thai-latin']},
    'tibetan': None,
    'tifinagh': None,
    'unknown': None,
    'vai': None,
    'yi': None
}


def write_transliterator_scripts_file(filename):
    transliterator_rule_template = '''{{{{{script_type}, {lang}}}, {{{start}, {length}}}}}'''
    rules = []
    all_transliterators = []
    index = 0
    for script, i in unicode_script_ids.iteritems():
        spec = script_transliterators.get(script.lower())
        if not spec:
            continue
        script_type = 'SCRIPT_{}'.format(script.upper())
        for lang, transliterators in spec.iteritems():
            lang = '""' if not lang else quote_string(lang)
            num_transliterators = len(transliterators)
            rules.append(transliterator_rule_template.format(script_type=script_type,
                         lang=lang, start=index, length=num_transliterators))
            for trans in transliterators:
                all_transliterators.append(quote_string(trans))

            index += num_transliterators

    template = transliterator_script_data_template.format(rules=''',
    '''.join(rules), transliterators=''',
    '''.join(all_transliterators))

    f = open(filename, 'w')
    f.write(safe_encode(template))


def write_transliteration_data_file(filename):
    transforms, steps, rules = get_all_transform_rules()

    all_transforms = u''',
    '''.join([u'{{{}}}'.format(u','.join(t)) for t in transforms])

    all_steps = u''',
    '''.join([u'{{{}}}'.format(u','.join(s)) for s in steps])

    for r in rules:
        try:
            r = u','.join(r)
        except Exception:
            print('Exception in rule')
            print(r)

    all_rules = u''',
    '''.join([u'{{{}}}'.format(u','.join(r)) for r in rules])

    template = transliteration_data_template.format(
        all_transforms=all_transforms,
        all_steps=all_steps,
        all_rules=all_rules,
    )

    f = open(filename, 'w')
    f.write(safe_encode(template))


SRC_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir, 'src')
TRANSLITERATION_DATA_FILENAME = 'transliteration_data.c'
TRANSLITERATION_SCRIPTS_FILENAME = 'transliteration_scripts_data.c'


def main(out_dir=SRC_DIR):
    write_transliteration_data_file(os.path.join(out_dir, TRANSLITERATION_DATA_FILENAME))
    write_transliterator_scripts_file(os.path.join(out_dir, TRANSLITERATION_SCRIPTS_FILENAME))

if __name__ == '__main__':
    if len(sys.argv) > 1:
        main(sys.argv[1])
    else:
        main()