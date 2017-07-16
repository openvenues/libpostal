# -*- coding: utf-8 -*-
import random
import re
import six

from collections import defaultdict

from geodata.addresses.config import address_config
from geodata.address_expansions.address_dictionaries import address_phrase_dictionaries
from geodata.encoding import safe_decode
from geodata.math.sampling import weighted_choice, zipfian_distribution, cdf
from geodata.math.floats import isclose
from geodata.numbers.ordinals import ordinal_expressions
from geodata.numbers.spellout import numeric_expressions
from geodata.text.tokenize import tokenize, token_types

alphabets = {}


def sample_alphabet(alphabet, b=1.5):
    '''
    Sample an "alphabet" using a Zipfian distribution (frequent items are very
    frequent, long tail of infrequent items). If we look at something like
    unit numbers, "Unit A" or "Unit B" are much more likely than "Unit X" or
    "Unit Z" simply because most dwellings only have a few units. Sampling
    letters from a Zipfian distribution rather than uniformly means that instead
    of every letter having the same likelihood (1/26), letters toward the beginning
    of the alphabet are much more likely to be selected. Letters toward the end can
    still be selected sometimes, but are not very likely.

    Note letters don't necessarily need to be sorted alphabetically, just in order
    of frequency.
    '''
    global alphabets
    alphabet = tuple(alphabet)
    if alphabet not in alphabets:
        probs = zipfian_distribution(len(alphabet), b)
        probs_cdf = cdf(probs)

        alphabets[alphabet] = probs_cdf

    probs_cdf = alphabets[alphabet]
    return weighted_choice(alphabet, probs_cdf)

latin_alphabet = [chr(i) for i in range(ord('A'), ord('Z') + 1)]


class Digits(object):
    ASCII = 'ascii'
    SPELLOUT = 'spellout'
    UNICODE_FULL_WIDTH = 'unicode_full_width'
    ROMAN_NUMERAL = 'roman_numeral'

    CARDINAL = 'cardinal'
    ORDINAL = 'ordinal'

    unicode_full_width_map = {
        '0': safe_decode('０'),
        '1': safe_decode('１'),
        '2': safe_decode('２'),
        '3': safe_decode('３'),
        '4': safe_decode('４'),
        '5': safe_decode('５'),
        '6': safe_decode('６'),
        '7': safe_decode('７'),
        '8': safe_decode('８'),
        '9': safe_decode('９'),
    }

    full_width_digit_map = {
        v: k for k, v in six.iteritems(unicode_full_width_map)
    }

    @classmethod
    def rewrite_full_width(cls, s):
        return six.u('').join([cls.unicode_full_width_map.get(c, c) for c in s])

    @classmethod
    def rewrite_standard_width(cls, s):
        return six.u('').join([cls.full_width_digit_map.get(c, c) for c in s])

    @classmethod
    def rewrite_roman_numeral(cls, s):
        roman_numeral = None
        if s.isdigit():
            roman_numeral = numeric_expressions.roman_numeral(s)

        if roman_numeral:
            return roman_numeral
        else:
            return s

    @classmethod
    def rewrite_spellout(cls, s, lang, num_type, props):
        if s.isdigit():
            num = int(s)
            spellout = None
            gender = props.get('gender')
            category = props.get('category')

            if num_type == cls.CARDINAL:
                spellout = numeric_expressions.spellout_cardinal(num, lang, gender=gender, category=category)
            elif num_type == cls.ORDINAL:
                spellout = numeric_expressions.spellout_ordinal(num, lang, gender=gender, category=category)

            if spellout:
                return spellout.title()
            return s
        else:
            return s

    @classmethod
    def rewrite(cls, d, lang, props, num_type=CARDINAL):
        if not props:
            return d

        d = safe_decode(d)

        values = []
        probs = []

        for digit_type in (cls.SPELLOUT, cls.UNICODE_FULL_WIDTH, cls.ROMAN_NUMERAL):
            key = '{}_probability'.format(digit_type)
            if key in props:
                values.append(digit_type)
                probs.append(props[key])

        if not isclose(sum(probs), 1.0):
            values.append(cls.ASCII)
            probs.append(1.0 - sum(probs))

        probs = cdf(probs)
        digit_type = weighted_choice(values, probs)

        if digit_type == cls.ASCII:
            return d
        elif digit_type == cls.SPELLOUT:
            return cls.rewrite_spellout(d, lang, num_type, props)
        elif digit_type == cls.ROMAN_NUMERAL:
            roman_numeral = cls.rewrite_roman_numeral(d)
            if random.random() < props.get('ordinal_suffix_probability', 0.0):
                ordinal_suffix = ordinal_expressions.get_suffix(d, lang, gender=props.get('gender', None))
                if ordinal_suffix:
                    roman_numeral = six.u('{}{}').format(roman_numeral, ordinal_suffix)
            return roman_numeral
        elif digit_type == cls.UNICODE_FULL_WIDTH:
            return cls.rewrite_full_width(d)
        else:
            return d


class NumericPhrase(object):
    key = None

    NUMERIC = 'numeric'
    NUMERIC_AFFIX = 'numeric_affix'

    @classmethod
    def pick_phrase_and_type(cls, number, language, country=None):
        values, probs = address_config.alternative_probabilities(cls.key, language, dictionaries=cls.dictionaries, country=country)
        if not values:
            return None, safe_decode(number) if number is not None else None, None

        phrase, phrase_props = weighted_choice(values, probs)

        values = []
        probs = []

        for num_type in (cls.NUMERIC, cls.NUMERIC_AFFIX):
            key = '{}_probability'.format(num_type)
            prob = phrase_props.get(key, None)
            if prob is not None:
                values.append(num_type)
                probs.append(prob)

        if not probs:
            num_type = cls.NUMERIC
        else:
            probs = cdf(probs)
            num_type = weighted_choice(values, probs)

        return num_type, phrase, phrase_props[num_type]

    @classmethod
    def combine_with_number(cls, number, phrase, num_type, props, whitespace_default=False):

        if num_type == cls.NUMERIC_AFFIX:
            if 'probability' not in props:
                alt_props = props
            else:
                affixes = [props]
                affix_probs = [props['probability']]

                for a in props.get('alternatives', []):
                    affixes.append(a['alternative'])
                    affix_probs.append(a['probability'])

                affix_probs = cdf(affix_probs)
                alt_props = weighted_choice(affixes, affix_probs)

            phrase = alt_props['affix']

            if 'zero_pad' in alt_props and number.isdigit():
                number = number.rjust(alt_props['zero_pad'], alt_props.get('zero_char', '0'))

        direction = props['direction']
        whitespace = props.get('whitespace', whitespace_default)
        whitespace_probability = props.get('whitespace_probability')
        if whitespace_probability is not None:
            whitespace = random.random() < whitespace_probability

        if props.get('title_case', True):
            # Title case unless the config specifies otherwise
            phrase = phrase.title()

        if number is None:
            return phrase

        whitespace_phrase = six.u(' ') if whitespace else six.u('')
        # Phrase goes to the left of hte number
        if direction == 'left':
            return six.u('{}{}{}').format(phrase, whitespace_phrase, number)
        # Phrase goes to the right of the number
        elif direction == 'right':
            return six.u('{}{}{}').format(number, whitespace_phrase, phrase)
        # Need to specify a direction, otherwise return naked number
        else:
            return safe_decode(number)

    @classmethod
    def phrase(cls, number, language, country=None):
        num_type, phrase, props = cls.pick_phrase_and_type(number, language, country=country)
        whitespace_default = num_type == cls.NUMERIC
        return cls.combine_with_number(number, phrase, num_type, props, whitespace_default=whitespace_default)


class Number(NumericPhrase):
    key = 'numbers'
    dictionaries = ['number']


class NumberedComponent(object):
    NUMERIC = 'numeric'
    ALPHA = 'alpha'
    ALPHA_PLUS_NUMERIC = 'alpha_plus_numeric'
    NUMERIC_PLUS_ALPHA = 'numeric_plus_alpha'
    HYPHENATED_NUMBER = 'hyphenated_number'
    DECIMAL_NUMBER = 'decimal_number'
    ROMAN_NUMERAL = 'roman_numeral'

    @classmethod
    def random_num_digits(cls, max_digits=6):
        return random.randrange(1, max_digits + 1)

    @classmethod
    def random_digits(cls, num_digits):
        return random.randrange(10 ** num_digits, 10 ** (num_digits + 1))

    numeric_pattern = u'\\b(?:[^\W\d_]{,2}[\d]+[^\W\d_]{,2}[\-\u2013\./][^\W\d_]{,2}[\d]+[^\W\d_]{,2}|[\d]*[^\W\d_][\d]*(?:[\-\u2013\./ ]|\s[\-\u2013/]\s)?[^\W\d_]?[\d]+[^\W\d_]?|[^\W\d_]?[\d]+[^\W\d_]?(?:[\-\u2013\./ ]|\s[\-\u2013/]\s)?[\d]*[^\W\d_][\d]*|[\d]+|[^\W\d_](?:(?:[\-\u2013/][^\W\d_])|(?:[\-\u2013\./][^\W\d_]?[\d]+[^\W\d_]?))?)\\b'

    @classmethod
    def numeric_regex(cls, language, country=None):
        left_phrases = []
        left_phrases_with_number = []
        left_affix_phrases = []
        left_ordinal_phrases = defaultdict(list)
        right_phrases = []
        right_phrases_with_number = []
        right_affix_phrases = []
        right_ordinal_phrases = defaultdict(list)
        standalone_phrases = []

        regexes = []

        alphanumeric_props = address_config.get_property('{}.alphanumeric'.format(cls.key), language, country=country)
        if not alphanumeric_props:
            return None

        default = alphanumeric_props.get('default')
        alternatives = alphanumeric_props.get('alternatives', [])
        config_phrases = [default] + [a['alternative'] for a in alternatives]
        sample = alphanumeric_props.get('sample')
        seen_phrases = set([c['canonical'] for c in config_phrases])

        whitespace = address_config.get_property('whitespace', language, country=country, default=True)

        canonical_phrases = defaultdict(set)
        for dictionary in cls.dictionaries:
            for p in address_phrase_dictionaries.phrases.get((language, dictionary), []):
                canonical_phrases[p[0]].update(p[1:])

        number_canonical_phrases = defaultdict(set)
        all_number_phrases = set()
        for dictionary in Number.dictionaries:
            for p in address_phrase_dictionaries.phrases.get((language, dictionary), []):
                number_canonical_phrases[p[0]].update(p[1:])
                all_number_phrases.update(p)

        zone_props = address_config.get_property('{}.zones'.format(cls.key), language, country=country)
        if zone_props:
            for zone, props in six.iteritems(zone_props):
                default = props.get('default')
                if default and default['canonical'] not in seen_phrases:
                    config_phrases.append(default)
                alternatives = props.get('alternatives', [])
                for a in alternatives:
                    alt = a['alternative']
                    if alt and alt['canonical'] not in seen_phrases:
                        config_phrases.append(alt)

        seen_phrases = set([c['canonical'] for c in config_phrases])

        alias_props = address_config.get_property('{}.aliases'.format(cls.key), language, country=country)
        if alias_props:
            for alias, props in six.iteritems(alias_props):
                default = props.get('default')
                if default and default['canonical'] not in seen_phrases:
                    config_phrases.append(default)
                alternatives = props.get('alternatives', [])
                for a in alternatives:
                    alt = a['alternative']
                    if alt and alt['canonical'] not in seen_phrases:
                        config_phrases.append(alt)

        seen_phrases = set([c['canonical'] for c in config_phrases])

        number_props = address_config.get_property(Number.key, language, country=country)
        number_default = number_props.get('default')
        number_alternatives = number_props.get('alternatives', [])
        number_phrases = [number_default] + [a['alternative'] for a in number_alternatives]

        left_number_phrases = []
        left_number_affix_phrases = []
        right_number_phrases = []
        right_number_affix_phrases = []

        for c in number_phrases:
            numeric = c.get('numeric')
            numeric_affix = c.get('numeric_affix')
            ordinal = c.get('ordinal')
            sample = c.get('sample')

            sample_exclude = set(c.get('sample_exclude', []))
            canonical = c.get('canonical')
            if canonical:
                canonical = safe_decode(canonical)
            abbreviated = c.get('abbreviated')
            if abbreviated:
                abbreviated = safe_decode(abbreviated)

            if numeric:
                direction = numeric['direction']
                if direction == 'left':
                    phrases = left_number_phrases
                else:
                    phrases = right_number_phrases

                phrases.append(canonical)
                if abbreviated:
                    phrases.append(u'{}.'.format(abbreviated))
                    phrases.append(abbreviated)
                if sample:
                    sample_phrases = list(number_canonical_phrases[canonical] - set([abbreviated]) - sample_exclude)
                    sample_phrases.extend([u'{}.'.format(p) for p in sample_phrases if len(p) < len(canonical) and not p.endswith(u'.')])
                    sample_phrases.sort(key=len, reverse=True)
                    phrases.extend(sample_phrases)

            if numeric_affix:
                direction = numeric_affix['direction']
                if direction == 'left':
                    phrases = left_number_affix_phrases
                else:
                    phrases = right_number_affix_phrases

                affix = numeric_affix['affix']

                phrases.append(affix)

        for c in config_phrases:
            numeric = c.get('numeric')
            numeric_affix = c.get('numeric_affix')
            ordinal = c.get('ordinal')
            sample = c.get('sample')
            standalone = not numeric and not numeric_affix and not ordinal

            sample_exclude = set(c.get('sample_exclude', []))
            canonical = c.get('canonical')
            if canonical:
                canonical = safe_decode(canonical)

            if canonical in all_number_phrases:
                continue
            abbreviated = c.get('abbreviated')
            if abbreviated:
                abbreviated = safe_decode(abbreviated)

            if numeric:
                direction = numeric['direction']
                add_number_phrase = numeric.get('add_number_phrase', False)
                direction_probability = numeric.get('direction_probability', None)
                if direction == 'left':
                    if not add_number_phrase:
                        all_phrases = [left_phrases]
                        if direction_probability is not None:
                            all_phrases.append(right_phrases)
                    else:
                        all_phrases = [left_phrases_with_number]
                        if direction_probability is not None:
                            all_phrases.append(right_phrases_with_number)
                else:
                    if not add_number_phrase:
                        all_phrases = [right_phrases]
                        if direction_probability is not None:
                            all_phrases.append(left_phrases)
                    else:
                        all_phrases = [right_phrases_with_number]
                        if direction_probability is not None:
                            all_phrases.append(left_phrases_with_number)

                for phrases in all_phrases:
                    phrases.append(canonical)
                    if abbreviated:
                        phrases.append(u'{}.'.format(abbreviated))
                        phrases.append(abbreviated)
                    if sample:
                        sample_phrases = list(canonical_phrases[canonical] - set([abbreviated]) - sample_exclude)
                        sample_phrases.extend([u'{}.'.format(p) for p in sample_phrases if len(p) < len(canonical) and not p.endswith(u'.')])
                        sample_phrases.sort(key=len, reverse=True)
                        phrases.extend(sample_phrases)

            if standalone:
                standalone_phrases.append(canonical)
                if abbreviated:
                    standalone_phrases.append(u'{}.'.format(abbreviated))
                    standalone_phrases.append(abbreviated)
                if sample:
                    sample_phrases = list(canonical_phrases[canonical] - set([abbreviated]) - sample_exclude)
                    sample_phrases.extend([u'{}.'.format(p) for p in sample_phrases if len(p) < len(canonical) and not p.endswith(u'.')])
                    sample_phrases.sort(key=len, reverse=True)
                    standalone_phrases.extend(sample_phrases)

            if numeric_affix:
                direction = numeric_affix['direction']
                if direction == 'left':
                    phrases = left_affix_phrases
                else:
                    phrases = right_affix_phrases

                affix = numeric_affix['affix']
                if affix in all_number_phrases:
                    continue

                phrases.append(affix)

            if ordinal:
                direction = ordinal['direction']
                gender = ordinal.get('gender')
                category = ordinal.get('category')
                if direction == 'left':
                    phrases = left_ordinal_phrases[(gender, category)]
                else:
                    phrases = right_ordinal_phrases[(gender, category)]

                phrases.append(canonical)
                if abbreviated:
                    phrases.append(u'{}.'.format(abbreviated))
                    phrases.append(abbreviated)
                if sample:
                    sample_phrases = list(canonical_phrases[canonical] - set([abbreviated]) - sample_exclude)
                    sample_phrases.extend([u'{}.'.format(p) for p in sample_phrases if len(p) < len(canonical) and not p.endswith(u'.')])
                    sample_phrases.sort(key=len, reverse=True)
                    phrases.extend(sample_phrases)

        ordinal_suffixes = defaultdict(set)
        cardinal_left_phrases = defaultdict(set)
        ordinal_phrases = defaultdict(set)

        for exprs in numeric_expressions.cardinal_rules.get(language, {}).values():
            for expr in exprs:
                gender = expr.get('gender')
                category = expr.get('category')
                cardinal_left_phrases[gender, category].add(expr['name'])

        for exprs in numeric_expressions.ordinal_rules.get(language, {}).values():
            for expr in exprs:
                gender = expr.get('gender')
                category = expr.get('category')
                ordinal_phrases[gender, category].add(expr['name'])

        for (l, g, c), suffixes in six.iteritems(ordinal_expressions.ordinal_suffixes):
            if l == language:
                for vals in suffixes.values():
                    ordinal_suffixes[(g, c)].update(vals)

        ordinal_suffix_phrase_regexes = {k: u'(?:\\b[\d]+(?:{}))'.format(u'|'.join(v)) for k, v in six.iteritems(ordinal_suffixes)}

        default_numeric_separator = numeric_expressions.default_separators.get(language, numeric_expressions.default_separator)

        numeric_separator_phrase = u'[{}\-]'.format(default_numeric_separator) if default_numeric_separator else u'[\-]?'

        ordinal_phrase_components = defaultdict(list)
        if cardinal_left_phrases and ordinal_phrases:
            for k, vals in six.iteritems(cardinal_left_phrases):
                ordinal_phrase_components[k].append(u'(?:{}{})*'.format(u'|'.join(vals), numeric_separator_phrase))

        if ordinal_phrases:
            for k, vals in six.iteritems(ordinal_phrases):
                ordinal_phrase_components[k].append(u'(?:{})'.format(u'|'.join(vals)))

        whitespace_phrase = u'[ \.]' if whitespace else u''
        whitespace_or_beginning = u'\\b' if whitespace else u''
        whitespace_lookahead = u'\\b' if whitespace else u''

        left_ordinal_phrases = {k: sorted(vals, reverse=True) for k, v in six.iteritems(left_ordinal_phrases)}
        right_ordinal_phrases = {k: sorted(vals, reverse=True) for k, v in six.iteritems(right_ordinal_phrases)}


        left_affix_phrases.sort(reverse=True)
        left_phrases_with_number.sort(reverse=True)
        left_phrases.sort(reverse=True)
        right_affix_phrases.sort(reverse=True)
        right_phrases_with_number.sort(reverse=True)
        right_phrases.sort(reverse=True)
        standalone_phrases.sort(reverse=True)

        left_number_affix_phrases.sort(reverse=True)
        left_number_phrases.sort(reverse=True)
        right_number_affix_phrases.sort(reverse=True)
        right_number_phrases.sort(reverse=True)

        if right_ordinal_phrases:
            for k, vals in six.iteritems(right_ordinal_phrases):
                ordinal_parts = [u''.join(ordinal_phrase_components[k])]
                ordinal_suffix_regex = ordinal_suffix_phrase_regexes.get(k)
                if ordinal_suffix_regex:
                    ordinal_parts.append(ordinal_suffix_regex)

                regexes.append(u'(?:{}){}(?:{})'.format(u'|'.join(ordinal_parts).replace(u'.', u'\\.'), whitespace_phrase, u'|'.join(sorted(vals, reverse=True)).replace(u'.', u'\\.')))

        if left_affix_phrases:
            regexes.append(u'(?:{})(?:{})'.format(u'|'.join(left_affix_phrases).replace(u'.', u'\\.'), cls.numeric_pattern))

        if left_phrases_with_number:
            if left_number_affix_phrases:
                regexes.append(u'(?:{}){}(?:{})?(?:{})'.format(u'|'.join(left_phrases_with_number).replace(u'.', u'\\.'), whitespace_phrase, u'|'.join(left_number_affix_phrases).replace(u'.', u'\\.'), cls.numeric_pattern))
            if left_number_phrases:
                regexes.append(u'(?:{}){}(?:(?:{}){})?(?:{})'.format(u'|'.join(left_phrases_with_number).replace(u'.', u'\\.'), whitespace_phrase, u'|'.join(left_number_phrases).replace(u'.', u'\\.'), whitespace_phrase, cls.numeric_pattern))
            if right_number_affix_phrases:
                regexes.append(u'(?:{}){}(?:{})(?:{})?'.format(u'|'.join(left_phrases_with_number).replace(u'.', u'\\.'), whitespace_phrase, cls.numeric_pattern, u'|'.join(right_number_affix_phrases).replace(u'.', u'\\.')))
            if right_number_phrases:
                regexes.append(u'(?:{}){}(?:{})(?:{}(?:{}))?'.format(u'|'.join(left_phrases_with_number).replace(u'.', u'\\.'), whitespace_phrase, cls.numeric_pattern, whitespace_phrase, u'|'.join(right_number_phrases).replace(u'.', u'\\.')))

        if left_phrases:
            regexes.append(u'(?:{}){}(?:{})'.format(u'|'.join(left_phrases).replace(u'.', u'\\.'), whitespace_phrase, cls.numeric_pattern))

        if left_ordinal_phrases:
            for k, vals in six.iteritems(left_ordinal_phrases):
                ordinal_parts = [u''.join(ordinal_phrase_components[k])]
                ordinal_suffix_regex = ordinal_suffix_phrase_regexes.get(k)
                if ordinal_suffix_regex:
                    ordinal_parts.append(ordinal_suffix_regex)

                regexes.append(u'(?:{}){}(?:{})'.format(u'|'.join(vals).replace(u'.', u'\\.'), whitespace_phrase, u'|'.join(ordinal_parts).replace(u'.', u'\\.')))

        if right_affix_phrases:
            regexes.append(u'(?:{})(?:{})'.format(cls.numeric_pattern, u'|'.join(right_affix_phrases).replace(u'.', u'\\.')))

        if right_phrases_with_number:
            if left_number_affix_phrases:
                regexes.append(u'(?:{})?(?:{}){}(?:{})'.format(u'|'.join(left_number_affix_phrases).replace(u'.', u'\\.'), cls.numeric_pattern, whitespace_phrase, u'|'.join(right_phrases_with_number).replace(u'.', u'\\.')))
            if left_number_phrases:
                regexes.append(u'(?:(?:{}){})?(?:{}){}(?:{})'.format(u'|'.join(left_number_phrases).replace(u'.', u'\\.'), whitespace_phrase, cls.numeric_pattern, whitespace_phrase, u'|'.join(right_phrases_with_number).replace(u'.', u'\\.')))
            if right_number_affix_phrases:
                regexes.append(u'(?:{})(?:{})?{}(?:{})'.format(cls.numeric_pattern, u'|'.join(right_number_affix_phrases).replace(u'.', u'\\.'), whitespace_phrase, u'|'.join(right_phrases_with_number).replace(u'.', u'\\.')))
            if right_number_phrases:
                regexes.append(u'(?:{})(?:{}(?:{}))?{}(?:{})'.format(cls.numeric_pattern, whitespace_phrase, u'|'.join(right_number_phrases).replace(u'.', u'\\.'), whitespace_phrase, u'|'.join(right_phrases_with_number).replace(u'.', u'\\.')))

        if right_phrases:
            regexes.append(u'(?:{}){}(?:{})'.format(cls.numeric_pattern, whitespace_phrase, u'|'.join(right_phrases).replace(u'.', u'\\.')))

        if standalone_phrases:
            regexes.append(u'(?:{})'.format(u'|'.join(standalone_phrases).replace(u'.', u'\\.')))

        return u'{}(?:{}){}'.format(whitespace_or_beginning, u'|'.join([u'(?:{})'.format(r) for r in regexes]), whitespace_lookahead)

    @classmethod
    def choose_alphanumeric_type(cls, key, language, country=None):
        alphanumeric_props = address_config.get_property(key, language, country=country, default=None)
        if alphanumeric_props is None:
            return None, None

        values = []
        probs = []

        for num_type in (cls.NUMERIC, cls.ALPHA, cls.ALPHA_PLUS_NUMERIC, cls.NUMERIC_PLUS_ALPHA, cls.HYPHENATED_NUMBER, cls.DECIMAL_NUMBER, cls.ROMAN_NUMERAL):
            key = '{}_probability'.format(num_type)
            prob = alphanumeric_props.get(key)
            if prob is not None:
                values.append(num_type)
                probs.append(prob)

        if not values:
            return None, None

        probs = cdf(probs)
        num_type = weighted_choice(values, probs)
        num_type_props = alphanumeric_props.get(num_type, {})

        return num_type, num_type_props

    @classmethod
    def numeric_phrase(cls, key, num, language, country=None, dictionaries=(), strict_numeric=False, is_alpha=False):
        has_alpha = False
        has_numeric = True
        is_integer = False
        is_none = False
        if num is not None:
            try:
                num_int = int(num)
                is_integer = True
            except ValueError:
                try:
                    num_float = float(num)
                except ValueError:
                    tokens = tokenize(safe_decode(num))
                    has_numeric = False
                    for t, c in tokens:
                        if c == token_types.NUMERIC:
                            has_numeric = True
                        if any((ch.isalpha() for ch in t)):
                            has_alpha = True

                    if strict_numeric and has_alpha:
                        return safe_decode(num)

        else:
            is_none = True

        values, probs = None, None

        if is_alpha:
            values, probs = address_config.alternative_probabilities('{}.alpha'.format(key), language, dictionaries=dictionaries, country=country)

        # Pick a phrase given the probability distribution from the config
        if values is None:
            values, probs = address_config.alternative_probabilities(key, language, dictionaries=dictionaries, country=country)

        if not values:
            return safe_decode(num) if not is_none else None

        phrase, phrase_props = weighted_choice(values, probs)

        values = []
        probs = []

        # Dictionaries are lowercased, so title case here
        if phrase_props.get('title_case', True):
            phrase = phrase.title()

        '''
        There are a few ways we can express the number itself

        1. Alias it as some standalone word like basement (for floor "-1")
        2. Use the number itself, so "Floor 2"
        3. Append/prepend an affix e.g. 2/F for second floor
        4. As an ordinal expression e.g. "2nd Floor"
        '''
        have_standalone = False
        have_null = False
        for num_type in ('standalone', 'null', 'numeric', 'numeric_affix', 'ordinal'):
            key = '{}_probability'.format(num_type)
            prob = phrase_props.get(key)
            if prob is not None:
                if num_type == 'standalone':
                    have_standalone = True
                elif num_type == 'null':
                    have_null = True
                values.append(num_type)
                probs.append(prob)
            elif num_type in phrase_props:
                values.append(num_type)
                probs.append(1.0)
                break

        if not probs or is_none:
            return phrase

        # If we're using something like "Floor A" or "Unit 2L", remove ordinal/affix items
        if has_alpha:
            values, probs = zip(*[(v, p) for v, p in zip(values, probs) if v in ('numeric', 'null', 'standalone')])
            total = float(sum(probs))
            if isclose(total, 0.0):
                return None

            probs = [p / total for p in probs]

        probs = cdf(probs)

        if len(values) < 2:
            if have_standalone:
                num_type = 'standalone'
            elif have_null:
                num_type = 'null'
            else:
                num_type = 'numeric'
        else:
            num_type = weighted_choice(values, probs)

        if num_type == 'standalone':
            return phrase
        elif num_type == 'null':
            return safe_decode(num)

        props = phrase_props[num_type]

        if is_integer:
            num_int = int(num)
            if phrase_props.get('number_abs_value', False):
                num_int = abs(num_int)
                num = num_int

            if 'number_min_abs_value' in phrase_props and num_int < phrase_props['number_min_abs_value']:
                return None

            if 'number_max_abs_value' in phrase_props and num_int > phrase_props['number_max_abs_value']:
                return None

            if phrase_props.get('number_subtract_abs_value'):
                num_int -= phrase_props['number_subtract_abs_value']
                num = num_int

        num = safe_decode(num)
        digits_props = props.get('digits')
        if digits_props:
            # Inherit the gender and category e.g. for ordinals
            for k in ('gender', 'category'):
                if k in props:
                    digits_props[k] = props[k]
            num = Digits.rewrite(num, language, digits_props, num_type=Digits.CARDINAL if num_type != 'ordinal' else Digits.ORDINAL)

        # Do we add the numeric phrase e.g. Floor No 1
        add_number_phrase = props.get('add_number_phrase', False)
        if add_number_phrase and random.random() < props['add_number_phrase_probability']:
            num = Number.phrase(num, language, country=country)

        whitespace_default = True

        if num_type == 'numeric_affix':
            if 'probability' not in props:
                alt_props = props
            else:
                affixes = [props]
                affix_probs = [props['probability']]
                for a in props.get('alternatives', []):
                    affixes.append(a['alternative'])
                    affix_probs.append(a['probability'])
                affix_probs = cdf(affix_probs)
                alt_props = weighted_choice(affixes, affix_probs)

            phrase = alt_props['affix']

            if alt_props.get('upper_case', True):
                phrase = phrase.upper()
            if 'zero_pad' in alt_props and num.isdigit():
                num = num.rjust(alt_props['zero_pad'], alt_props.get('zero_char', '0'))
            whitespace_default = False
        elif num_type == 'ordinal' and safe_decode(num).isdigit():
            ordinal_expression = ordinal_expressions.suffixed_number(num, language, gender=props.get('gender', None))

            if ordinal_expression is not None:
                num = ordinal_expression

        if 'null_phrase_probability' in props and (num_type == 'ordinal' or (has_alpha and (has_numeric or 'null_phrase_alpha_only' in props))):
            if random.random() < props['null_phrase_probability']:
                return num

        direction = props['direction']
        whitespace = props.get('whitespace', whitespace_default)

        whitespace_probability = props.get('whitespace_probability')
        if whitespace_probability is not None:
            whitespace = random.random() < whitespace_probability

        # Occasionally switch up if direction_probability is specified
        if random.random() > props.get('direction_probability', 1.0):
            if direction == 'left':
                direction = 'right'
            elif direction == 'right':
                direction = 'left'

        whitespace_phrase = six.u(' ') if whitespace else six.u('')
        # Phrase goes to the left of hte number
        if direction == 'left':
            return six.u('{}{}{}').format(phrase, whitespace_phrase, num)
        # Phrase goes to the right of the number
        elif direction == 'right':
            return six.u('{}{}{}').format(num, whitespace_phrase, phrase)
        # Need to specify a direction, otherwise return naked number
        else:
            return safe_decode(num)
