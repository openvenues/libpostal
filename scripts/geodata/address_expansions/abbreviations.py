import random
import re
import six

from geodata.address_expansions.gazetteers import *
from geodata.encoding import safe_decode, safe_encode
from geodata.text.tokenize import tokenize_raw, token_types
from geodata.text.utils import non_breaking_dash_regex


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


expansion_token_regex = re.compile('([^  \-\.]+)([\.\- ]+|$)')


def recase_abbreviation(expansion, tokens, space_token=six.u(' ')):
    expansion_tokens = expansion_token_regex.findall(expansion)

    if len(tokens) > len(expansion_tokens) and all((token_capitalization(t) != LOWER for t, c in tokens)):
        expansion_tokenized = tokenize(expansion)
        is_acronym = len(expansion_tokenized) == 1 and expansion_tokenized[0][1] == token_types.ACRONYM
        if len(expansion) <= 3 or is_acronym:
            return expansion.upper()
        else:
            return expansion.title()
    elif len(tokens) == len(expansion_tokens):
        strings = []
        for (t, c), (e, suf) in zip(tokens, expansion_tokens):
            cap = token_capitalization(t)
            if suf == six.u(' '):
                suf = space_token
            if cap == LOWER:
                strings.append(six.u('').join((e.lower(), suf)))
            elif cap == UPPER:
                strings.append(six.u('').join((e.upper(), suf)))
            elif cap == TITLE:
                strings.append(six.u('').join((e.title(), suf)))
            elif t.lower() == e.lower():
                strings.append(t)
            else:
                strings.append(six.u('').join((e.title(), suf)))
        return six.u('').join(strings)
    else:

        strings = []
        for e, suf in expansion_tokens:
            strings.append(e.title())
            if suf == six.u(' '):
                strings.append(space_token)
            else:
                strings.append(suf)
        return six.u('').join(strings)


def abbreviate(gazetteer, s, language, abbreviate_prob=0.3, separate_prob=0.2, add_period_hyphen_prob=0.3):
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

    def abbreviated_tokens(i, tokens, t, c, length, data, space_token=six.u(' ')):
        data = [d.split(six.b('|')) for d in data]

        # local copy
        abbreviated = []

        n = len(t)

        # Append the original tokens with whitespace if there is any
        if random.random() > abbreviate_prob or not any((int(is_canonical) and lang in (language, 'all') for lang, dictionary, is_canonical, canonical in data)):
            for j, (t_i, c_i) in enumerate(t):
                abbreviated.append(tokens[i + j][0])

                if j < n - 1:
                    abbreviated.append(space_token)
            return abbreviated

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
                # TODO: maybe make this a Zipfian choice e.g. so "St" gets chosen most often for "Street"
                # would require an audit of the dictionaries though so abbreviations are listed from
                # left-to-right by frequency of usage
                token = random.choice(abbreviations) if abbreviations else canonical
                token = recase_abbreviation(token, tokens[i:i + len(t)], space_token=space_token)
                abbreviated.append(token)
                break
            elif is_prefix:
                token = tokens[i][0]
                prefix, token = token[:length], token[length:]

                abbreviated.append(prefix)
                if random.random() < separate_prob:
                    sub_tokens = tokenize(token)
                    if sub_tokens and sub_tokens[0][1] in (token_types.HYPHEN, token_types.DASH):
                        token = six.u('').join((t for t, c in sub_tokens[1:]))

                    abbreviated.append(space_token)
                if token.islower():
                    abbreviated.append(token.title())
                else:
                    abbreviated.append(token)
                abbreviated.append(space_token)
                break
            elif is_suffix:
                token = tokens[i][0]

                token, suffix = token[:-length], token[-length:]

                concatenated_abbreviations = gazetteer.canonicals.get((canonical, lang, dictionary), [])

                separated_abbreviations = []
                phrase = gazetteer.trie.get(suffix.rstrip('.'))
                suffix_data = [safe_decode(d).split(six.u('|')) for d in (phrase or [])]
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

                if separate:
                    sub_tokens = tokenize(token)
                    if sub_tokens and sub_tokens[-1][1] in (token_types.HYPHEN, token_types.DASH):
                        token = six.u('').join((t for t, c in sub_tokens[:-1]))

                abbreviated.append(token)
                if separate:
                    abbreviated.append(space_token)
                if suffix.isupper():
                    abbreviated.append(abbreviation.upper())
                elif separate:
                    abbreviated.append(abbreviation.title())
                else:
                    abbreviated.append(abbreviation)
                break
        else:
            for j, (t_i, c_i) in enumerate(t):
                abbreviated.append(tokens[i + j][0])
                if j < n - 1:
                    abbreviated.append(space_token)
        return abbreviated

    for t, c, length, data in gazetteer.filter(norm_tokens):
        if c == token_types.PHRASE:
            abbrev_tokens = abbreviated_tokens(i, tokens, t, c, length, data)
            abbreviated.extend(abbrev_tokens)

            if i + len(t) < n and raw_tokens[i + len(t)][0] > sum(raw_tokens[i + len(t) - 1][:2]):
                abbreviated.append(six.u(' '))

            i += len(t)

        else:
            token = tokens[i][0]
            if not non_breaking_dash_regex.search(token):
                abbreviated.append(token)
            else:
                sub_tokens = tokenize(non_breaking_dash_regex.sub(six.u(' '), token))
                sub_tokens_norm = [(t.lower() if c in token_types.WORD_TOKEN_TYPES else t, c) for t, c in sub_tokens]

                sub_token_abbreviated = []
                sub_i = 0
                sub_n = len(sub_tokens)
                for t, c, length, data in gazetteer.filter(sub_tokens_norm):
                    if c == token_types.PHRASE:
                        abbrev_tokens = abbreviated_tokens(sub_i, sub_tokens, t, c, length, data, space_token=six.u('-'))
                        sub_token_abbreviated.extend(abbrev_tokens)
                        sub_i += len(t)
                        if sub_i < sub_n:
                            if abbrev_tokens and random.random() < add_period_hyphen_prob and not abbrev_tokens[-1].endswith(six.u('.')) and not abbrev_tokens[-1].lower().endswith(sub_tokens_norm[sub_i - 1][0]):
                                sub_token_abbreviated.append(six.u('.'))
                            sub_token_abbreviated.append(six.u('-'))
                    else:
                        sub_token_abbreviated.append(sub_tokens[sub_i][0])
                        sub_i += 1
                        if sub_i < sub_n:
                            sub_token_abbreviated.append(six.u('-'))

                abbreviated.append(six.u('').join(sub_token_abbreviated))

            if i < n - 1 and raw_tokens[i + 1][0] > sum(raw_tokens[i][:2]):
                abbreviated.append(six.u(' '))
            i += 1

    return six.u('').join(abbreviated).strip()
