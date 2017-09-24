# -*- coding: utf-8 -*-

import bisect
import itertools
import math
import operator
import os
import random
import re
import six
import yaml

from collections import defaultdict

from geodata.encoding import safe_decode
from geodata.numbers.numex import NUMEX_DATA_DIR


class NumericExpressions(object):
    default_separator = ' '
    LATIN = 'la'

    def __init__(self, base_dir=NUMEX_DATA_DIR):
        self.cardinal_rules = {}
        self.cardinal_rules_sorted = {}
        self.cardinal_rules_ones = defaultdict(dict)
        self.cardinal_rules_ones_sorted = {}

        self.default_separators = {}

        self.ordinal_rules = {}
        self.ordinal_suffix_rules = {}

        self.stopwords = {}

        self.cardinal_regexes = {}

        for filename in os.listdir(base_dir):
            if filename.endswith('.yaml'):
                lang = filename.split('.yaml')[0]
                f = open(os.path.join(base_dir, filename))
                data = yaml.load(f)

                default_separator = data.get('default_separator')
                if default_separator is not None:
                    self.default_separators[lang] = default_separator

                rules = data.get('rules')
                if rules is not None and hasattr(rules, '__getslice__'):
                    cardinals = defaultdict(list)
                    ordinals = defaultdict(list)
                    for rule in rules:
                        name = rule.get('name')
                        value = rule.get('value')
                        rule_type = rule.get('type')
                        if not name or type(value) not in (int, float) or rule_type not in ('cardinal', 'ordinal'):
                            continue
                        gender = rule.get('gender', None)
                        category = rule.get('category', None)
                        if rule_type == 'ordinal':
                            ordinals[(value, gender, category)].append(rule)
                        else:
                            cardinals[(value, gender, category)].append(rule)
                            if value == 1 and 'multiply_gte' in rule:
                                self.cardinal_rules_ones[lang][rule['multiply_gte']] = rule

                    self.cardinal_rules[lang] = cardinals
                    self.ordinal_rules[lang] = ordinals

                    self.cardinal_rules_sorted[lang] = sorted(set([v for v, g, c in cardinals]))
                    self.cardinal_rules_ones_sorted[lang] = sorted(self.cardinal_rules_ones[lang].keys())
                if 'stopwords' in data:
                    self.stopwords[lang] = data['stopwords']

                if lang != self.LATIN:
                    self.cardinal_regexes[lang] = self.make_cardinal_regex(lang)

        self.cardinal_rules_ones = dict(self.cardinal_rules_ones)

    def cardinal_regex(self, lang):
        return self.cardinal_regexes.get(lang)

    def make_cardinal_regex(self, lang):
        separator_pattern = u'[{}\.\-]{}'.format(self.default_separator, u'?' if lang in self.default_separators else u'')

        stopwords = self.stopwords.get(lang, [])

        if stopwords:
            separator_pattern = u'{}(?:{}{})?'.format(separator_pattern, u'|'.join(stopwords), separator_pattern)

        left_multiply = [(rule['name'], None) for rule in itertools.chain(*self.cardinal_rules.get(lang, {}).values()) if rule.get('left') == 'multiply' and 'right' not in rule]

        left_add_rules = [rule for rule in itertools.chain(*self.cardinal_rules.get(lang, {}).values()) if rule.get('left') == 'add']
        left_add = []

        for rule in left_add_rules:
            rule_name = rule['name']
            separator_rule = None
            if 'left_separator' in rule:
                left_separator = rule['left_separator'].replace(u' ', u'[\- ]')
                separator_rule = u'(?:{})?{}'.format(left_separator, rule['name'])

            left_add.append((rule_name, separator_rule))

        right_add_rules = [rule for rule in itertools.chain(*self.cardinal_rules.get(lang, {}).values()) if rule.get('right') == 'add']
        right_add = []
        for rule in right_add_rules:
            rule_name = rule['name']
            separator_rule = None
            if 'right_separator' in rule:
                right_separator = rule['right_separator'].replace(u' ', u'[\- ]')
                separator_rule = u'{}(?:{})?'.format(rule['name'], right_separator)
            right_add.append((rule_name, separator_rule))

        simple_rules = [(rule['name'], None) for rule in itertools.chain(*self.cardinal_rules.get(lang, {}).values()) if 'right' not in rule and 'left' not in rule]

        all_phrases_tuples = left_multiply + right_add + simple_rules + left_add
        all_phrases_tuples.sort(key=operator.itemgetter(0), reverse=True)
        all_phrases = []
        for rule, rule_with_separator in all_phrases_tuples:
            if rule_with_separator:
                all_phrases.append(rule_with_separator)
            all_phrases.append(rule)

        return u'(?:(?:{}){})*(?:{})'.format(u'|'.join(all_phrases), separator_pattern, u'|'.join(all_phrases))

    def spellout_cardinal(self, num, lang, gender=None, category=None, random_choice_cardinals=False):
        num = int(num)
        remainder = 0

        if lang not in self.cardinal_rules:
            return None

        rules = self.cardinal_rules.get(lang)
        cardinals = self.cardinal_rules_sorted.get(lang)
        if not rules or not cardinals:
            return None

        default_separator = self.default_separators.get(lang, self.default_separator)

        if num == 0:
            cardinal = rules.get((num, gender, category))
            if cardinal:
                if not random_choice_cardinals:
                    cardinal = cardinal[0]
                else:
                    cardinal = random.choice(cardinal)
                return cardinal['name']
            else:
                return None

        cardinal_part = []

        last_rule = {}
        left_multiply_rules = []

        while num:
            i = bisect.bisect_left(cardinals, num)
            if i > len(cardinals) - 1:
                return None
            if i > 0 and cardinals[i] > num:
                val = cardinals[i - 1]
            else:
                val = cardinals[i]

            multiple = num // val

            if val == num:
                cardinal = rules.get((num, gender, category))
            else:
                cardinal = rules.get((val, None, None), [])

            multiple_rule = None

            if multiple > 1:
                multiple_val = rules.get((multiple, None, None))
                if multiple_val:
                    if not random_choice_cardinals:
                        multiple_rule = multiple_val[0]
                    else:
                        multiple_rule = random.choice(multiple_val)
            elif multiple == 1 and lang in self.cardinal_rules_ones_sorted:
                ones_rules = self.cardinal_rules_ones_sorted[lang]
                j = bisect.bisect_right(ones_rules, val)
                if j > 0 and ones_rules[j - 1] <= num:
                    multiple_rule = self.cardinal_rules_ones[lang][ones_rules[j - 1]]

            use_multiple = multiple > 1

            is_left_multiply = False
            did_left_multiply = False

            if not use_multiple:
                rule = None
                if cardinal and not random_choice_cardinals:
                    rule = cardinal[0]
                elif cardinal:
                    rule = random.choice(cardinal)
            else:
                for rule in cardinal:
                    left_multiply = rule.get('left') == 'multiply'
                    if left_multiply:
                        if not multiple_rule:
                            left_multiply_rules.append(rule)
                            is_left_multiply = True
                            last_rule = rule
                            rule = None
                        break
                else:
                    rule = None

            if rule is not None:
                left_add = last_rule.get('left') == 'add'
                right_add = last_rule.get('right') == 'add'

                if multiple_rule:
                    if right_add and cardinal_part:
                        cardinal_part.append(last_rule.get('left_separator', default_separator))
                    cardinal_part.append(multiple_rule['name'])
                    cardinal_part.append(rule.get('left_separator', default_separator))

                if right_add:
                    if not multiple_rule and cardinal_part:
                        right_separator = last_rule.get('right_separator', default_separator)
                        cardinal_part.append(right_separator)
                    cardinal_part.append(rule['name'])
                elif left_add and cardinal_part:
                    last = cardinal_part.pop()
                    cardinal_part.append(rule['name'])
                    left_separator = last_rule.get('left_separator', default_separator)
                    cardinal_part.append(left_separator)
                    cardinal_part.append(last)
                elif not left_add and not right_add:
                    cardinal_part.append(rule['name'])

                last_rule = rule

                if left_multiply_rules and 'right' not in rule and 'left' not in rule:
                    left_multiply_rule = left_multiply_rules.pop()
                    left_separator = left_multiply_rule.get('left_separator', default_separator)
                    cardinal_part.append(left_separator)
                    cardinal_part.append(left_multiply_rule['name'])
                    did_left_multiply = True
                    last_rule = left_multiply_rule

            if not is_left_multiply and not did_left_multiply:
                num -= (multiple * val)
            elif not did_left_multiply:
                remainder = num % val
                num /= val
            else:
                num = remainder
                did_left_multiply = False

        return six.u('').join(cardinal_part)

    def roman_numeral(self, num):
        numeral = self.spellout_cardinal(num, self.LATIN)
        if numeral is None:
            return None
        return numeral.upper()

    roman_numeral_chars = {}

    simple_roman_numeral_chars = {
        'I': 1,
        'V': 5,
        'X': 10,
        'L': 50,
        'C': 100,
        'D': 500,
        'M': 1000,
    }

    roman_numeral_chars.update(simple_roman_numeral_chars)

    roman_numeral_unicode_chars = {
        u'Ⅰ': 1,
        u'Ⅱ': 2,
        u'Ⅲ': 3,
        u'Ⅳ': 4,
        u'Ⅴ': 5,
        u'Ⅵ': 6,
        u'Ⅶ': 7,
        u'Ⅷ': 8,
        u'Ⅸ': 9,
        u'Ⅹ': 10,
        u'Ⅺ': 11,
        u'Ⅻ': 12,
        u'Ⅼ': 50,
        u'Ⅽ': 100,
        u'Ⅾ': 500,
        u'Ⅿ': 1000,
        u'ⅰ': 1,
        u'ⅱ': 2,
        u'ⅲ': 3,
        u'ⅳ': 4,
        u'ⅴ': 5,
        u'ⅵ': 6,
        u'ⅶ': 7,
        u'ⅷ': 8,
        u'ⅸ': 9,
        u'ⅹ': 10,
        u'ⅺ': 11,
        u'ⅻ': 12,
        u'ⅼ': 50,
        u'ⅽ': 100,
        u'ⅾ': 500,
        u'ⅿ': 1000,
    }

    roman_numeral_chars.update(roman_numeral_unicode_chars)

    roman_numeral_ascii_pattern = u'(?=[ivxlcdm])(?:(?:m{0,4}(?:cm|cd|d?c{0,3})(?:xl|xc|l?x{0,3})(?:ix|iv|v?i{0,3})))(?<=[ivxlcdm])'
    roman_numeral_unicode_pattern = u'\\b[{}]+\\b'.format(u''.join(roman_numeral_unicode_chars.keys()))
    roman_numeral_pattern = u'\\b(?:{}|{})\\b'.format(roman_numeral_ascii_pattern, roman_numeral_unicode_pattern)

    roman_numeral_regex = re.compile(roman_numeral_pattern, re.I | re.U)

    def parse_roman_numeral(self, s):
        s = safe_decode(s).upper()

        simple_roman_numeral_chars = self.simple_roman_numeral_chars
        roman_numeral_chars = self.roman_numeral_chars
        value = 0

        nums = []
        for c in s:
            val = roman_numeral_chars.get(c)
            if val is None:
                return None
            nums.append(val)

        n = len(nums)

        for i in six.moves.xrange(n):
            num = nums[i]
            # If the next place holds a larger number, this value is negative
            if i + 1 < n and nums[i + 1] > num and s[i] in simple_roman_numeral_chars:
                value -= num
            else:
                value += num

        return value

    def sub_roman_numeral(self, match):
        value = self.parse_roman_numeral(match.group(0))
        if value is not None:
            return safe_decode(value)
        else:
            return u''

    def replace_roman_numerals(self, s):
        return self.roman_numeral_regex.sub(self.sub_roman_numeral, s)

    def spellout_ordinal(self, num, lang, gender=None, category=None,
                         random_choice_cardinals=False, random_choice_ordinals=False):
        num = int(num)
        remainder = 0

        if lang not in self.cardinal_rules:
            return None

        rules = self.ordinal_rules.get(lang)
        cardinal_rules = self.cardinal_rules.get(lang)
        cardinals = self.cardinal_rules_sorted.get(lang)
        if not rules or not cardinal_rules or not cardinals:
            return None

        default_separator = self.default_separators.get(lang, self.default_separator)

        expression = []

        last_rule = {}
        left_multiply_rules = []

        if num == 0 or (num, gender, category) in rules:
            ordinals = rules.get((num, gender, category))
            if ordinals:
                if not random_choice_ordinals:
                    ordinal = ordinals[0]
                else:
                    ordinal = random.choice(ordinals)
                return ordinal['name']
            else:
                return None

        while num:
            i = bisect.bisect_left(cardinals, num)
            if i > len(cardinals) - 1:
                return None
            if i > 0 and cardinals[i] > num:
                val = cardinals[i - 1]
            else:
                val = cardinals[i]

            if val == num and not remainder:
                if last_rule.get('right') == 'add':
                    ordinals = rules.get((num, gender, category))
                    if ordinals:
                        if not random_choice_ordinals:
                            ordinal = ordinals[0]
                        else:
                            ordinal = random.choice(ordinals)
                        right_separator = last_rule.get('right_separator', default_separator)

                        return right_separator.join([six.u('').join(expression), ordinal['name']])
                    else:
                        return None
                elif last_rule.get('left') == 'add':
                    last_num = last_rule['value']
                    ordinals = rules.get((last_num, gender, category))
                    if ordinals:
                        if not random_choice_ordinals:
                            ordinal = ordinals[0]
                        else:
                            ordinal = random.choice(ordinals)

                        last_rule = ordinal
                        expression.pop()
                        cardinals = cardinal_rules.get((num, None, None))
                        if cardinals:
                            if not random_choice_cardinals:
                                rule = cardinals[0]
                            else:
                                rule = random.choice(cardinals)
                            expression.append(rule['name'])
                        else:
                            return None
                        last = ordinal['name']
                        left_separator = last_rule.get('left_separator', default_separator)
                        return left_separator.join([six.u('').join(expression), ordinal['name']])
                    else:
                        return None
                else:
                    return None
            else:
                ordinal = rules.get((val, None, None), [])
                cardinal = cardinal_rules.get((val, None, None), [])

            multiple = num // val

            multiple_rule = None

            if multiple > 1:
                multiple_val = cardinal_rules.get((multiple, None, None))
                if multiple_val:
                    if not random_choice_cardinals:
                        multiple_rule = multiple_val[0]
                    else:
                        multiple_rule = random.choice(multiple_val)
            elif multiple == 1 and lang in self.cardinal_rules_ones_sorted:
                ones_rules = self.cardinal_rules_ones_sorted[lang]
                j = bisect.bisect_right(ones_rules, val)
                if j > 0 and ones_rules[j - 1] <= num:
                    multiple_rule = self.cardinal_rules_ones[lang][ones_rules[j - 1]]

            use_multiple = multiple > 1

            is_left_multiply = False
            did_left_multiply = False

            if not use_multiple:
                rule = None
                if ordinal and not remainder:
                    for rule in ordinal:
                        if rule.get('right') == 'add':
                            break
                    else:
                        rule = None

                if not rule and cardinal and not random_choice_cardinals:
                    rule = cardinal[0]
                elif not rule and cardinal:
                    rule = random.choice(cardinal)
            else:
                rule = None
                have_ordinal = False
                if ordinal:
                    for rule in ordinal:
                        left_multiply = rule.get('left') == 'multiply'
                        if left_multiply and rule.get('right') == 'add':
                            if not multiple_rule:
                                left_multiply_rules.append(rule)
                                is_left_multiply = True
                                last_rule = rule
                                rule = None
                                have_ordinal = True
                            break
                    else:
                        rule = None

                if not have_ordinal:
                    for rule in cardinal:
                        left_multiply = rule.get('left') == 'multiply'
                        if left_multiply:
                            if not multiple_rule:
                                left_multiply_rules.append(rule)
                                is_left_multiply = True
                                last_rule = rule
                                rule = None
                            break
                    else:
                        rule = None

            if rule is not None:
                left_add = last_rule.get('left') == 'add'
                right_add = last_rule.get('right') == 'add'

                if multiple_rule:
                    if right_add and expression:
                        expression.append(last_rule.get('left_separator', default_separator))
                    expression.append(multiple_rule['name'])
                    expression.append(rule.get('left_separator', default_separator))

                if right_add:
                    if not multiple_rule and expression:
                        right_separator = last_rule.get('right_separator', default_separator)
                        expression.append(right_separator)
                    expression.append(rule['name'])
                elif left_add and expression:
                    last = expression.pop()
                    expression.append(rule['name'])
                    left_separator = last_rule.get('left_separator', default_separator)
                    expression.append(left_separator)
                    expression.append(last)
                elif not left_add and not right_add:
                    expression.append(rule['name'])

                last_rule = rule

                if left_multiply_rules and 'right' not in rule and 'left' not in rule:
                    left_multiply_rule = left_multiply_rules.pop()
                    print 'left_multiply_rule', left_multiply_rule
                    left_separator = left_multiply_rule.get('left_separator', default_separator)
                    expression.append(left_separator)
                    expression.append(left_multiply_rule['name'])
                    did_left_multiply = True
                    last_rule = left_multiply_rule

            if not is_left_multiply and not did_left_multiply:
                num -= (multiple * val)
            elif not did_left_multiply:
                remainder = num % val
                num /= val
            else:
                num = remainder
                remainder = 0
                did_left_multiply = False

    def spellout_cardinal_hundreds(self, num, lang, gender=None, category=None, splitter=six.u(' ')):
        if num % 100 >= 10:
            first_hundred = self.spellout_cardinal(num % 100, lang, gender=gender, category=category)
        elif num % 100 == 0:
            rules = self.cardinal_rules.get(lang)
            if not rules:
                return None

            cardinals = rules.get((100, gender, category))
            if not cardinals:
                return None

            for rule in cardinals:
                if rule.get('left') == 'multiply' and not rule.get('exact_multiple_only'):
                    break
            else:
                rule = None

            if not rule:
                return None

            first_hundred = rule['name']
        else:
            rules = self.cardinal_rules.get(lang)
            if not rules:
                return None

            tens_place = num % 10
            zero_rules = rules.get((0, gender, category))
            if not zero_rules:
                return None

            tens_place_rules = rules.get((tens_place, gender, category))
            if not tens_place_rules:
                return None

            zero_rule = random.choice(zero_rules)
            tens_rule = random.choice(tens_place_rules)

            first_hundred = splitter.join([zero_rule['name'], tens_rule['name']])

        if not first_hundred:
            return None

        parts = [first_hundred]

        for i in xrange(1, int(math.ceil(math.log(num, 100)))):
            part = self.spellout_cardinal(num / 100 ** i, lang, gender=gender, category=category)
            if not part:
                return None
            parts.append(part)
        return splitter.join(reversed(parts))


numeric_expressions = NumericExpressions()
