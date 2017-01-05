'''
unicode_data.py
---------------

Python's unicodedata module uses an outdated spec (Unicode 5.2) and since
e.g. unicode categories are used in tokenization, we'd like to keep this
as up-to-date as possible with the latest standard.
'''
import csv
import os
import sys
from collections import defaultdict, namedtuple

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.file_utils import download_file
from geodata.string_utils import wide_unichr, wide_ord

from unicode_properties import *

from unicode_paths import UNICODE_DATA_DIR

UNIDATA_URL = 'http://unicode.org/Public/UNIDATA/UnicodeData.txt'

UNIDATA_DIR = os.path.join(UNICODE_DATA_DIR, 'unidata')
LOCAL_UNIDATA_FILE = os.path.join(UNIDATA_DIR, 'UnicodeData.txt')

unicode_categories = defaultdict(list)
unicode_blocks = defaultdict(list)
unicode_combining_classes = defaultdict(list)
unicode_general_categories = defaultdict(list)
unicode_scripts = defaultdict(list)
unicode_properties = {}

unicode_script_ids = {}

unicode_blocks = {}
unicode_category_aliases = {}
unicode_property_aliases = {}
unicode_property_value_aliases = {}
unicode_word_breaks = {}


# Ref: ftp://ftp.unicode.org/Public/3.0-Update/UnicodeData-3.0.0.html
UNIDATA_FIELDS = [
    'code',
    'name',
    'category',
    'combining',
    'bidi_category',
    'decomp_mapping',
    'decimal_value',
    'digit_value',
    'numeric_value',
    'mirrored',
    'unicode_1_name',
    'comment',
    'upper_mapping',
    'lower_mapping',
    'title_mapping',
]

UnicodeDataRow = namedtuple('UnicodeDataRow', ','.join(UNIDATA_FIELDS))


def parse_unicode_data():
    '''
    Parse UnicodeData.txt into namedtuples using UNIDATA_FIELDS
    '''
    if not os.path.exists(LOCAL_UNIDATA_FILE):
        download_file(UNIDATA_URL, LOCAL_UNIDATA_FILE)
    unidata_file = open(LOCAL_UNIDATA_FILE)

    for line in csv.reader(unidata_file, delimiter=';'):
        yield UnicodeDataRow(*line)


def iter_unicode_combining_classes():
    return unicode_combining_classes.iteritems()


def iter_unicode_categories():
    return unicode_categories.iteritems()


def get_unicode_category(cat):
    return unicode_categories[cat]


def get_unicode_combining_class(c):
    return unicode_combining_classes[c]


def get_unicode_categories():
    '''
    Build dict of unicode categories e.g.

    {
        'Lu': ['A', 'B', 'C', ...]
        'Ll': ['a', 'b', 'c', ...]
    }
    '''
    categories = defaultdict(list)
    for row in parse_unicode_data():
        categories[row.category].append(wide_unichr(unicode_to_integer(row.code)))
    return dict(categories)


def get_unicode_combining_classes():
    '''
    Build dict of unicode combining classes e.g.

    {
        '0': ['\x00', '\x01', \x02', ...]
    }
    '''
    combining_classes = defaultdict(list)
    for row in parse_unicode_data():
        combining_classes[int(row.combining)].append(wide_unichr(unicode_to_integer(row.code)))
    return dict(combining_classes)

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

COMBINING_CLASS_PROP = 'canonical_combining_class'
BLOCK_PROP = 'block'
GENERAL_CATEGORY_PROP = 'general_category'
SCRIPT_PROP = 'script'
WORD_BREAK_PROP = 'word_break'


def init_unicode_categories():
    '''
    Initialize module-level dictionaries
    '''
    global unicode_categories, unicode_general_categories, unicode_scripts, unicode_category_aliases
    global unicode_blocks, unicode_combining_classes, unicode_properties, unicode_property_aliases
    global unicode_property_value_aliases, unicode_scripts, unicode_script_ids, unicode_word_breaks

    unicode_categories.update(get_unicode_categories())
    unicode_combining_classes.update(get_unicode_combining_classes())

    for key in unicode_categories.keys():
        unicode_general_categories[key[0]].extend(unicode_categories[key])

    script_chars = get_chars_by_script()
    for i, script in enumerate(script_chars):
        if script:
            unicode_scripts[script.lower()].append(wide_unichr(i))

    unicode_scripts = dict(unicode_scripts)

    unicode_script_ids.update(build_master_scripts_list(script_chars))

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


regex_chars = re.compile('([\[\]\{\}\-\^])')


def replace_regex_chars(s):
    return regex_chars.sub(r'\\\1', s)


def format_regex_char(i):
    c = wide_unichr(i)
    return replace_regex_chars(c.encode('unicode-escape'))


def make_char_set_regex(chars):
    '''
    Build a regex character set from a list of characters
    '''
    group_start = None
    group_end = None
    last_ord = -2

    ords = map(wide_ord, chars)
    ords.sort()

    ords.append(None)

    groups = []

    for i, o in enumerate(ords):
        if o is not None and o == last_ord + 1:
            group_end = o
        elif group_start is not None and group_end is not None:
            groups.append('-'.join((format_regex_char(group_start), format_regex_char(group_end))))
            group_end = None
            group_start = o
        elif group_start is not None and group_end is None:
            groups.append(format_regex_char(group_start))
            group_start = o
        else:
            group_start = o

        last_ord = o

    return u'[{}]'.format(u''.join(groups))


name_category = [
    ('control_chars', 'Cc'),
    ('other_format_chars', 'Cf'),
    ('other_not_assigned_chars', 'Cn'),
    ('other_private_use_chars', 'Co'),
    ('other_surrogate_chars', 'Cs'),
    ('letter_lower_chars', 'Ll'),
    ('letter_modifier_chars', 'Lm'),
    ('letter_other_chars', 'Lo'),
    ('letter_title_chars', 'Lt'),
    ('letter_upper_chars', 'Lu'),
    ('mark_spacing_combining_chars', 'Mc'),
    ('mark_enclosing_chars', 'Me'),
    ('mark_nonspacing_chars', 'Mn'),
    ('number_or_digit_chars', 'Nd'),
    ('number_letter_chars', 'Nl'),
    ('number_other_chars', 'No'),
    ('punct_connector_chars', 'Pc'),
    ('punct_dash_chars', 'Pd'),
    ('punct_close_chars', 'Pe'),
    ('punct_final_quote_chars', 'Pf'),
    ('punct_initial_quote_chars', 'Pi'),
    ('punct_other_chars', 'Po'),
    ('punct_open_chars', 'Ps'),
    ('currency_symbol_chars', 'Sc'),
    ('symbol_modifier_chars', 'Sk'),
    ('symbol_math_chars', 'Sm'),
    ('symbol_other_chars', 'So'),
    ('separator_line_chars', 'Zl'),
    ('separator_paragraph_chars', 'Zp'),
    ('space', 'Zs'),
]


def main():
    init_unicode_categories()
    for name, cat in name_category:
        if cat not in unicode_categories:
            continue
        chars = unicode_categories[cat]
        print u'{} = {};'.format(name, make_char_set_regex(chars))


if __name__ == '__main__':
    main()
