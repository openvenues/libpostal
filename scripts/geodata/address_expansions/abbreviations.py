import random

from geodata.address_expansions.gazetteers import *
from geodata.encoding import safe_decode, safe_encode
from geodata.text.tokenize import tokenize_raw, token_types


LOWER, UPPER, TITLE, MIXED = range(4)


def token_capitalization(s):
    if s.istitle():
        return TITLE
    elif s.islower():
        return LOWER
    elif s.isupper():
        return UPPER
    else:
        return MIXED


def recase_abbreviation(expansion, tokens):
    expansion_tokens = expansion.split()
    if len(tokens) > len(expansion_tokens) and all((token_capitalization(t) != LOWER for t, c in tokens)):
        return expansion.upper()
    elif len(tokens) == len(expansion_tokens):
        strings = []
        for (t, c), e in zip(tokens, expansion_tokens):
            cap = token_capitalization(t)
            if cap == LOWER:
                strings.append(e.lower())
            elif cap == UPPER:
                strings.append(e.upper())
            elif cap == TITLE:
                strings.append(e.title())
            elif t.lower() == e.lower():
                strings.append(t)
            else:
                strings.append(e.title())
        return u' '.join(strings)
    else:
        return u' '.join([t.title() for t in expansion_tokens])


def abbreviate(gazetteer, s, language, abbreviate_prob=0.3, separate_prob=0.2):
    '''
    Abbreviations
    -------------

    OSM discourages abbreviations, but to make our training data map better
    to real-world input, we can safely replace the canonical phrase with an
    abbreviated version and retain the meaning of the words
    '''
    raw_tokens = tokenize_raw(s)
    s_utf8 = safe_encode(s)
    tokens = [(safe_decode(s_utf8[o:o + l]), token_types.from_id(c)) for o, l, c in raw_tokens]
    norm_tokens = [(t.lower() if c in token_types.WORD_TOKEN_TYPES else t, c) for t, c in tokens]

    n = len(tokens)

    abbreviated = []

    i = 0

    for t, c, length, data in gazetteer.filter(norm_tokens):
        if c is PHRASE:
            valid = []
            data = [d.split('|') for d in data]

            added = False

            if random.random() > abbreviate_prob:
                for j, (t_i, c_i) in enumerate(t):
                    abbreviated.append(tokens[i + j][0])
                    if i + j < n - 1 and raw_tokens[i + j + 1][0] > sum(raw_tokens[i + j][:2]):
                        abbreviated.append(u' ')
                i += len(t)
                continue

            for lang, dictionary, is_canonical, canonical in data:
                if lang not in (language, 'all'):
                    continue

                is_canonical = int(is_canonical)
                is_stopword = dictionary == 'stopword'
                is_prefix = dictionary.startswith('concatenated_prefixes')
                is_suffix = dictionary.startswith('concatenated_suffixes')
                is_separable = is_prefix or is_suffix and dictionary.endswith('_separable') and len(t[0][0]) > length

                suffix = None
                prefix = None

                if not is_canonical:
                    continue

                if not is_prefix and not is_suffix:
                    abbreviations = gazetteer.canonicals.get((canonical, lang, dictionary))
                    token = random.choice(abbreviations) if abbreviations else canonical
                    token = recase_abbreviation(token, tokens[i:i + len(t)])
                    abbreviated.append(token)
                    if i + len(t) < n and raw_tokens[i + len(t)][0] > sum(raw_tokens[i + len(t) - 1][:2]):
                        abbreviated.append(u' ')
                    break
                elif is_prefix:
                    token = tokens[i][0]
                    prefix, token = token[:length], token[length:]
                    abbreviated.append(prefix)
                    if random.random() < separate_prob:
                        abbreviated.append(u' ')
                    if token.islower():
                        abbreviated.append(token.title())
                    else:
                        abbreviated.append(token)
                    abbreviated.append(u' ')
                    break
                elif is_suffix:
                    token = tokens[i][0]

                    token, suffix = token[:-length], token[-length:]

                    concatenated_abbreviations = gazetteer.canonicals.get((canonical, lang, dictionary), [])

                    separated_abbreviations = []
                    phrase = gazetteer.trie.get(suffix.rstrip('.'))
                    suffix_data = [safe_decode(d).split(u'|') for d in (phrase or [])]
                    for l, d, _, c in suffix_data:
                        if l == lang and c == canonical:
                            separated_abbreviations.extend(gazetteer.canonicals.get((canonical, lang, d)))

                    separate = random.random() < separate_prob

                    if concatenated_abbreviations and not separate:
                        abbreviation = random.choice(concatenated_abbreviations)
                    elif separated_abbreviations:
                        abbreviation = random.choice(separated_abbreviations)
                    else:
                        abbreviation = canonical

                    abbreviated.append(token)
                    if separate:
                        abbreviated.append(u' ')
                    if suffix.isupper():
                        abbreviated.append(abbreviation.upper())
                    elif separate:
                        abbreviated.append(abbreviation.title())
                    else:
                        abbreviated.append(abbreviation)
                    abbreviated.append(u' ')
                    break
            else:
                for j, (t_i, c_i) in enumerate(t):
                    abbreviated.append(tokens[i + j][0])
                    if i + j < n - 1 and raw_tokens[i + j + 1][0] > sum(raw_tokens[i + j][:2]):
                        abbreviated.append(u' ')
            i += len(t)

        else:
            abbreviated.append(tokens[i][0])
            if i < n - 1 and raw_tokens[i + 1][0] > sum(raw_tokens[i][:2]):
                abbreviated.append(u' ')
            i += 1

    return u''.join(abbreviated).strip()
