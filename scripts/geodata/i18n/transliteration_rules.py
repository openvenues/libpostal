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

from collections import defaultdict

from lxml import etree

from scanner import Scanner
from unicode_scripts import get_chars_by_script
from unicode_paths import CLDR_DIR
from geodata.encoding import safe_decode, safe_encode

CLDR_TRANSFORMS_DIR = os.path.join(CLDR_DIR, 'common', 'transforms')

PRE_TRANSFORM = 1
FORWARD_TRANSFORM = 2
BACKWARD_TRANSFORM = 3
BIDIRECTIONAL_TRANSFORM = 4

PRE_TRANSFORM_OP = '::'
BACKWARD_TRANSFORM_OP = u'←'
FORWARD_TRANSFORM_OP = u'→'
BIDIRECTIONAL_TRANSFORM_OP = u'↔'

ASSIGNMENT_OP = '='

PRE_CONTEXT_INDICATOR = '{'
POST_CONTEXT_INDICATOR = '}'

REVISIT_INDICATOR = '|'

WORD_BOUNDARY_VAR_NAME = 'wordBoundary'
WORD_BOUNDARY_VAR = '${}'.format(WORD_BOUNDARY_VAR_NAME)

word_boundary_var_regex = re.compile(WORD_BOUNDARY_VAR.replace('$', '\$'))

EMPTY_TRANSITION = u'\u007f'

EXCLUDE_TRANSLITERATORS = set([
    'Hangul-Latin',
    'InterIndic-Latin',
    'Jamo-Latin',
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
unicode_general_categories = defaultdict(list)
unicode_scripts = defaultdict(list)


def init_unicode_categories():
    global unicode_categories, unicode_general_categories, unicode_scripts

    for i in xrange(65536):
        unicode_categories[unicodedata.category(unichr(i))].append(unichr(i))

    for key in unicode_categories.keys():
        unicode_general_categories[key[0]].extend(unicode_categories[key])

    script_chars = get_chars_by_script()
    for i, script in enumerate(script_chars):
        if script:
            unicode_scripts[script.lower()].append(unichr(i))


RULE = 'RULE'
TRANSFORM = 'TRANSFORM'

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
transform_regex = re.compile(u"(?:[\s]*(?!=[\s])(.*)(?<![\s])[\s]*)([←→↔=])(?:[\s]*(?!=[\s])(.*)(?<![\s])[\s]*)", re.UNICODE)

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

unicode_properties = {}


def replace_literal_space(m):
    return "' '"

regex_char_set_greedy = re.compile(r'\[(.*)\]', re.UNICODE)
regex_char_set = re.compile(r'\[(.*?)(?<!\\)\]', re.UNICODE)

char_class_regex_str = '\[(?:[^\[\]]*\[[^\[\]]*\][^\[\]]*)*[^\[\]]*\]'

nested_char_class_regex = re.compile('\[(?:[^\[\]]*\[[^\[\]]*\][^\[\]]*)+[^\[\]]*\]', re.UNICODE)

range_regex = re.compile(r'([^\\])\-([^\\])', re.UNICODE)
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
WORD_BOUNDARY = 'WORD_BOUNDARY'
NEGATION = 'NEGATION'
INTERSECTION = 'INTERSECTION'

# Scanner for a character set (yes, a regex regex)

char_set_scanner = Scanner([
    ('^\^', NEGATION),
    (r'[\\]?[^\\]\-[\\]?.', CHAR_RANGE),
    (r'[\\].', ESCAPED_CHARACTER),
    (r'\'\'', SINGLE_QUOTE),
    (r'\'.*?\'', QUOTED_STRING),
    (':[^:]+:', CHAR_CLASS),
    # Char set
    ('\[[^\[\]]+\]', CHAR_SET),
    ('\[', OPEN_SET),
    ('\]', CLOSE_SET),
    ('&', INTERSECTION),
    ('\$', WORD_BOUNDARY),
    (r'[^\s]', CHARACTER),
])

NUM_CHARS = 65536

all_chars = set([unichr(i) for i in xrange(NUM_CHARS)])


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


def parse_regex_char_class(c):
    chars = []
    orig = c
    c = c.strip(':')
    is_negation = False
    if c.startswith('^'):
        is_negation = True
        c = c.strip('^')

    if '=' in c:
        cat, c = c.split('=')
        if cat.strip() in ('script', 'sc'):
            c = c.strip()

    c = unicode_category_aliases.get(c.lower(), c)

    if c in unicode_general_categories:
        chars = unicode_general_categories[c]
    elif c in unicode_categories:
        chars = unicode_categories.get(c)
    elif c.lower() in unicode_scripts:
        chars = unicode_scripts[c.lower()]
    elif c.lower() in unicode_properties:
        chars = unicode_properties[c.lower()]
    else:
        chars = []

    if is_negation:
        chars = sorted(all_chars - set(chars))

    return chars


def parse_regex_char_set(s):
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
    is_word_boundary = False

    for token, token_class in char_set_scanner.scan(s):
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
        elif token_class == CHAR_CLASS:
            this_group |= set(parse_regex_char_class(token))
        elif token_class == CHAR_SET:
            # Recursive calls, as performance doesn't matter here and nesting is shallow
            this_char_set = set(parse_regex_char_set(token))
            # Shouldn't be complex set expression logic here
            if is_intersection:
                this_group &= this_char_set
            else:
                this_group |= this_char_set
        elif token_class == INTERSECTION:
            is_intersection = True
        elif token_class == CHARACTER:
            this_group.add(token)
        elif token_class == WORD_BOUNDARY:
            is_word_boundary = True

    if is_negation:
        this_group = all_chars - this_group

    return sorted(this_group) + (['$'] if is_word_boundary else [])


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

    for rule in xml.xpath('*//tRule'):
        if not rule.text:
            continue
        rule = safe_decode(rule.text.rsplit(COMMENT_CHAR)[0].strip())
        rule = literal_space_regex.sub(replace_literal_space, rule)
        rule = escaped_unicode_regex.sub(unescape_unicode_char, rule)
        rule = rule.rstrip(END_CHAR).strip()

        transform = transform_regex.match(rule)
        if transform:
            lvalue, op, rvalue = transform.groups()
            lvalue = lvalue.strip()
            rvalue = rvalue.strip()

            if op == FORWARD_TRANSFORM_OP:
                rules.append((FORWARD_TRANSFORM, (lvalue, rvalue)))
            elif op == BIDIRECTIONAL_TRANSFORM_OP:
                rules.append((BIDIRECTIONAL_TRANSFORM, (lvalue, rvalue)))
            elif op == BACKWARD_TRANSFORM_OP:
                rules.append((BACKWARD_TRANSFORM, (lvalue, rvalue)))
            elif op == ASSIGNMENT_OP:
                var_name = lvalue.lstrip('$')
                variables[var_name] = rvalue
        else:
            pre_transform = pre_transform_full_regex.match(rule)
            if pre_transform:
                rules.append((PRE_TRANSFORM, pre_transform.group(1)))

    return rules, variables

CHAR_CLASSES = set([
    ESCAPED_CHARACTER,
    CHAR_CLASS,
    QUOTED_STRING,
    CHARACTER,
    GROUP_REF,
])


def char_permutations(s):
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
                char_types.append(parse_regex_char_set(u''.join(current_set)))
                current_set = []
        elif token_type == QUOTED_STRING:
            token = token.strip("'")
            for c in token:
                char_types.append([c])
        elif token_type == GROUP_REF:
            char_types.append([token])
        elif token_type == REVISIT:
            in_revisit = True
        elif token_type == REPEAT:
            char_types.append([STAR])
        elif token_type == PLUS:
            char_types.append([PLUS])
        elif token_type == OPTIONAL:
            char_types[-1].append('')
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
    u'': EMPTY_TRANSITION,
    u'*': u'\*',
    u'+': u'\+',
    PLUS: u'+',
    STAR: u'*',
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


def encode_string(s):
    return safe_encode(s).encode('string-escape')


def format_rule(rule):
    '''
    Creates the C literal for a given transliteration rule
    '''
    key = rule[0]

    pre_context_type = rule[1]
    pre_context = rule[2]
    if pre_context is None:
        pre_context = 'NULL'
        pre_context_len = 0
    else:
        pre_context_len = len(pre_context)
        pre_context = quote_string(encode_string(pre_context))

    pre_context_max_len = rule[3]

    post_context_type = rule[4]
    post_context = rule[5]

    if post_context is None:
        post_context = 'NULL'
        post_context_len = 0
    else:
        post_context_len = len(post_context)
        post_context = quote_string(encode_string(post_context))

    post_context_max_len = rule[6]

    groups = rule[7]
    if not groups:
        groups = 'NULL'
        groups_len = 0
    else:
        groups_len = len(groups)
        groups = quote_string(encode_string(groups))

    replacement = rule[8]
    move = rule[9]

    output_rule = (
        quote_string(encode_string(key)),
        str(len(key)),
        pre_context_type,
        str(pre_context_max_len),
        pre_context,
        str(pre_context_len),

        post_context_type,
        str(post_context_max_len),
        post_context,
        str(post_context_len),

        quote_string(encode_string(replacement)),
        str(len(replacement)),
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
                    left_pre_context, _, _ = char_permutations(left_pre_context.strip())
                    left_pre_context_max_len = len(left_pre_context or [])
                    left_pre_context = char_types_string(left_pre_context)
                    if charset_regex.search(left_pre_context):
                        left_pre_context_type = CONTEXT_TYPE_REGEX
                    else:
                        left_pre_context_type = CONTEXT_TYPE_STRING

            if left:
                left, _, left_groups = char_permutations(left.strip())
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
                    left_post_context, _, _ = char_permutations(left_post_context.strip())
                    left_post_context_max_len = len(left_post_context or [])
                    left_post_context = char_types_string(left_post_context)
                    if charset_regex.search(left_post_context):
                        left_post_context_type = CONTEXT_TYPE_REGEX
                    else:
                        left_post_context_type = CONTEXT_TYPE_STRING

            if right:
                right, move, right_groups = char_permutations(right.strip())
                right = char_types_string(right)

            yield RULE, (left, left_pre_context_type, left_pre_context, left_pre_context_max_len,
                         left_post_context_type, left_post_context, left_post_context_max_len, left_groups, right, move)

        elif rule_type == PRE_TRANSFORM and '[' in rule and ']' in rule:
            continue
        elif rule_type == PRE_TRANSFORM:
            pre_transform = pre_transform_regex.match(rule)
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

    all_transforms = set([name.strip('.xml').lower() for name in get_transforms()])

    for filename in get_transforms():
        name = filename.strip('.xml').lower()

        f = open(os.path.join(CLDR_TRANSFORMS_DIR, filename))
        xml = etree.parse(f)
        source, target = get_source_and_target(xml)

        if (target.lower() == 'latin' or name == 'latin-ascii') and name not in EXCLUDE_TRANSLITERATORS:
            to_latin.add(name)
            retain_transforms.add(name)

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
                if name in to_latin and rule.lower() in all_transforms:
                    retain_transforms.add(rule.lower())
                    steps.append((STEP_TRANSFORM, rule))

                rule = UTF8PROC_TRANSFORMS.get(rule, rule)
                if rule in UNICODE_NORMALIZATION_TRANSFORMS:
                    steps.append((STEP_UNICODE_NORMALIZATION, rule))

        if rule_set:
            steps.append((STEP_RULESET, rule_set))

        transforms[name] = steps

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
        for step_type, data in steps:
            if step_type == STEP_RULESET:
                rule_index = len(all_rules)
                num_rules = len(data)
                step = (STEP_RULESET, str(rule_index), str(num_rules), 'NULL')
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
