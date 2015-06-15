import csv


def unicode_csv_reader(filename, **kw):
    for line in csv.reader(filename, **kw):
        yield [unicode(c, 'utf-8') for c in line]
