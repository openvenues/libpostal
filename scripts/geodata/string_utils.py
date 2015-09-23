import sys
from encoding import safe_decode

NUM_CODEPOINTS = 0x10FFFF + 1


def wide_unichr(i):
    if i <= sys.maxunicode:
        return unichr(i)
    else:
        return '\U{0:08x}'.format(i).decode('unicode-escape')


def wide_ord(c):
    if len(c) == 1:
        return ord(c)
    elif len(c) == 2:
        h, l = c
        return ((ord(h) - 0xD800) * 0x400) + (ord(l) - 0xDC00) + 0x10000

    return None


def wide_iter(s):
    skip = False
    s = safe_decode(s)
    for i, c in enumerate(s):
        if skip:
            skip = False
            continue

        if 0xD800 <= ord(c) <= 0xDBFF:
            yield s[i:i+2]
            skip = True
            continue
        yield c
