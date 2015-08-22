import csv
import re
from encoding import safe_encode, safe_decode

newline_regex = re.compile('\r\n|\r|\n')

csv.register_dialect('tsv_no_quote', delimiter='\t', quoting=csv.QUOTE_NONE, quotechar='')


def tsv_string(s):
    return safe_encode(newline_regex.sub(u', ', safe_decode(s).strip()).replace(u'\t', u' '))


def unicode_csv_reader(filename, **kw):
    for line in csv.reader(filename, **kw):
        yield [unicode(c, 'utf-8') for c in line]
