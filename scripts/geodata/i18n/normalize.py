import unicodedata


def strip_accents(s):
    return u''.join([c for c in unicodedata.normalize('NFD', s) if unicodedata.category(c) != 'Mn'])
