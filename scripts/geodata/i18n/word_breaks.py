'''
word_breaks.py

This script is used to automatically build ranges of unicode characters
from the unicode spec's word break properties. These ranges help us
build a tokenizer that does the right thing in every language with regard
to word segmentation. The lines outputted by this script can be pasted
into scanner.re before compliation.
'''

import requests
from collections import defaultdict
import re

# Operate on WordBreakProperty.txt file
hebrew_letter_regex = re.compile('^([^\s]+)[\s]+; Hebrew_Letter ')
format_regex = re.compile('^([^\s]+)[\s]+; Format ')
extend_regex = re.compile('^([^\s]+)[\s]+; Extend ')
katakana_regex = re.compile('^([^\s]+)[\s]+; Katakana ')
other_alpha_letter_regex = re.compile('^([^\s]+)[\s]+; ALetter # Lo (?!.*(?:HANGUL|TIBETAN|JAVANESE|BALINESE|YI) )')
mid_letter_regex = re.compile('^([^\s]+)[\s]+; MidLetter')
mid_number_regex = re.compile('^([^\s]+)[\s]+; MidNum ')
mid_num_letter_regex = re.compile('^([^\s]+)[\s]+; MidNumLet ')
numeric_regex = re.compile('^([^\s]+)[\s]+; Numeric ')
extend_num_letter_regex = re.compile('^([^\s]+)[\s]+; ExtendNumLet ')

# Operate on Scripts.txt file
other_number_regex = re.compile('^([^\s]+)[\s]+; ExtendNumLet ')

script_regex = re.compile('([^\s]+)[\s]+;[\s]*([^\s]+)[\s]*#[\s]*([^\s]+)')

WORD_BREAK_PROPERTIES_URL = 'http://www.unicode.org/Public/UCD/latest/ucd/auxiliary/WordBreakProperty.txt'
HANGUL_SYLLABLE_TYPES_URL = 'http://www.unicode.org/Public/UCD/latest/ucd/HangulSyllableType.txt'
SCRIPTS_URL = 'http://unicode.org/Public/UNIDATA/Scripts.txt'

ideographic_scripts = set([
    'han',
    'hiragana',
    'hangul',
    'tibetan',
    'thai',
    'lao',
    'javanese',
    'balinese',
    'yi',
])


def regex_char_range(match):
    r = match.split('..')
    # Wide version
    return u'-'.join([('\u{}'.format(c.lower()) if len(c) < 5 else '\U{}'.format(c.lower().rjust(8, '0'))) for c in r])


def get_letter_range(text, *regexes):
    char_ranges = []
    for line in text.split('\n'):
        for regex in regexes:
            m = regex.match(line)
            if m:
                char_ranges.append(regex_char_range(m.group(1)))
    return char_ranges


def get_letter_ranges_for_scripts(text, scripts, char_class_regex):
    char_ranges = []
    for char_range, script, char_class in script_regex.findall(text):
        if script.lower() in scripts and char_class_regex.match(char_class):
            char_ranges.append(regex_char_range(char_range))
    return char_ranges


def get_char_class(text, char_class_regex):
    char_ranges = []
    for char_range, script, char_class in script_regex.findall(text):
        if char_class_regex.match(char_class):
            char_ranges.append(regex_char_range(char_range))
    return char_ranges


hangul_syllable_type_regex = re.compile('^([^\s]+)[\s]+; ([A-Z]+)')


def get_hangul_syllable_ranges(text):
    char_ranges = defaultdict(list)
    for line in text.split('\n'):
        m = hangul_syllable_type_regex.match(line)
        if m:
            char_ranges[m.group(2)].append(regex_char_range(m.group(1)))
    return dict(char_ranges)


name_funcs = [
    ('hebrew_letter_chars', hebrew_letter_regex),
    ('format_chars', format_regex),
    ('extend_chars', extend_regex),
    ('katakana_chars', katakana_regex),
    ('letter_other_alpha_chars', other_alpha_letter_regex),
    ('mid_letter_chars', mid_letter_regex),
    ('mid_number_chars', mid_number_regex),
    ('mid_num_letter_chars', mid_num_letter_regex),
    ('numeric_chars', numeric_regex),
    ('extend_num_letter_chars', extend_num_letter_regex),
]

IDEOGRAPHIC_CHARS = 'ideographic_chars'
IDEOGRAPHIC_NUMERIC_CHARS = 'ideographic_numeric_chars'

numbers_regex = re.compile('N[ol]', re.I)
letters_regex = re.compile('L*', re.I)


def main():
    ''' Insert these lines into scanner.re '''
    response = requests.get(WORD_BREAK_PROPERTIES_URL)

    if response.ok:
        for name, reg in name_funcs:
            s = get_letter_range(response.content, reg)
            print '{} = [{}];'.format(name, ''.join(s))

    response = requests.get(HANGUL_SYLLABLE_TYPES_URL)

    if response.ok:
        syllable_ranges = get_hangul_syllable_ranges(response.content)
        for name, ranges in syllable_ranges.iteritems():
            print 'hangul_syllable_class_{} = [{}];'.format(name, u''.join(ranges))

    response = requests.get(SCRIPTS_URL)
    if response.ok:
        s = ''.join(get_char_class(response.content, numbers_regex))

        print '{} = [{}];'.format(IDEOGRAPHIC_NUMERIC_CHARS, ''.join(s))

        s = ''.join(get_letter_ranges_for_scripts(response.content, ideographic_scripts, letters_regex))
        print '{} = [{}];'.format(IDEOGRAPHIC_CHARS, ''.join(s))


if __name__ == '__main__':
    main()
