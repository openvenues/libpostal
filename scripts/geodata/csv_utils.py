import csv

csv.register_dialect('tsv_no_quote', delimiter='\t', quoting=csv.QUOTE_NONE, quotechar='')


def unicode_csv_reader(filename, **kw):
    for line in csv.reader(filename, **kw):
        yield [unicode(c, 'utf-8') for c in line]
