from __future__ import unicode_literals
import re

from geodata.encoding import safe_decode

name_prefixes = ['{} '.format(s) for s in (
    'city of',
    'township of',
    'regional municipality of',
    'municipality of',
    'borough of',
    'london borough of',
    'town of',
)]

name_suffixes = [' {}'.format(s) for s in (
    'township',
    'municipality',
)]

name_prefix_regex = re.compile('^(?:{})'.format('|'.join(name_prefixes)), re.I | re.UNICODE)
name_suffix_regex = re.compile('(?:{})$'.format('|'.join(name_suffixes)), re.I | re.UNICODE)


def replace_name_prefixes(name):
    name = safe_decode(name)
    return name_prefix_regex.sub('', name)


def replace_name_suffixes(name):
    name = safe_decode(name)
    return name_suffix_regex.sub('', name)
