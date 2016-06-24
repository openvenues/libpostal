import bisect
import os
import six
import yaml

from collections import defaultdict

from geodata.numbers.numex import NUMEX_DATA_DIR


class NumericExpressions(object):
    default_separator = ' '

    def __init__(self, base_dir=NUMEX_DATA_DIR):
        self.cardinal_rules = {}
        self.cardinal_rules_sorted = {}
        self.cardinal_rules_ones = defaultdict(dict)
        self.cardinal_rules_ones_sorted = {}

        self.default_separators = {}

        self.ordinal_rules = {}
        self.ordinal_suffix_rules = {}

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

        self.cardinal_rules_ones = dict(self.cardinal_rules_ones)

    def spellout_cardinal(self, num, lang, gender=None, category=None):
        num = int(num)
        remainder = 0

        if lang not in self.cardinal_rules:
            return None

        rules = self.cardinal_rules.get(lang)
        cardinals = self.cardinal_rules_sorted.get(lang)
        if not rules or not cardinals:
            return None

        default_separator = self.default_separators.get(lang, self.default_separator)

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
                    multiple_rule = multiple_val[0]
            elif multiple == 1 and lang in self.cardinal_rules_ones_sorted:
                ones_rules = self.cardinal_rules_ones_sorted[lang]
                j = bisect.bisect_right(ones_rules, val)
                if j > 0 and ones_rules[j - 1] <= num:
                    multiple_rule = self.cardinal_rules_ones[lang][ones_rules[j - 1]]

            use_multiple = multiple > 1

            is_left_multiply = False
            did_left_multiply = False

            if not use_multiple:
                rule = cardinal[0] if cardinal else None
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
        numeral = self.spellout_cardinal(num, 'la')
        if numeral is None:
            return None
        return numeral.upper()

numeric_expressions = NumericExpressions()
