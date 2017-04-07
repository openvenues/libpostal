import bisect
import math
import os
import operator
import random
import six
import sys
import yaml

from collections import defaultdict
from marisa_trie import BytesTrie

from geodata.text.phrases import PhraseFilter
from geodata.encoding import safe_encode, safe_decode
from geodata.i18n.unicode_paths import DATA_DIR

from geodata.numbers.numex import NUMEX_DATA_DIR


class OrdinalSuffixTrie(PhraseFilter):
    def __init__(self, ordinal_rules):
        self.trie = BytesTrie([(safe_decode(k)[::-1], safe_decode('|').join(v).encode('utf-8')) for k, v in six.iteritems(ordinal_rules)])
        self.configured = True

    def search_substring(self, s):
        if len(s) == 0:
            return None, 0

        for i in xrange(len(s) + 1):
            if not self.trie.has_keys_with_prefix(s[:i]):
                i -= 1
                break
        if i > 0:
            return (self.trie.get(s[:i]), i)
        else:
            return None, 0

    def search_suffix(self, token):
        suffix_search, suffix_len = self.search_substring(safe_decode(token[::-1]))
        if suffix_search:
            return suffix_search[0].split('|')
        else:
            return None


class OrdinalExpressions(object):
    def __init__(self, base_dir=NUMEX_DATA_DIR):
        self.cardinal_rules = {}
        self.cardinal_rules_ones = {}

        self.ordinal_rules = {}
        self.ordinal_suffix_rules = {}

        for filename in os.listdir(base_dir):
            if filename.endswith('.yaml'):
                lang = filename.split('.yaml')[0]
                f = open(os.path.join(base_dir, filename))
                data = yaml.load(f)

                rules = data.get('rules')
                if rules is not None and hasattr(rules, '__getslice__'):
                    cardinals = []
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
                            ordinals[(value, gender, category)].append(name)
                        else:
                            cardinals.append(rule)
                            if value == 1:
                                self.cardinal_rules_ones[(lang, gender, category)] = name

                    self.cardinal_rules[lang] = cardinals
                    self.ordinal_rules[lang] = ordinals

                ordinal_indicators = data.get('ordinal_indicators')
                if ordinal_indicators is not None and hasattr(ordinal_indicators, '__getslice__'):
                    for rule_set in ordinal_indicators:
                        gender = rule_set.get('gender', None)
                        category = rule_set.get('category', None)
                        self.ordinal_suffix_rules[(lang, gender, category)] = OrdinalSuffixTrie(rule_set['suffixes'])

    def get_suffixes(self, num, lang, gender=None, category=None):
        trie = self.ordinal_suffix_rules.get((lang, gender, category))
        if not trie:
            return None

        return trie.search_suffix(str(num))

    def get_suffix(self, num, lang, gender=None, category=None):
        suffixes = self.get_suffixes(num, lang, gender=gender, category=category)
        if not suffixes:
            return None
        return random.choice(suffixes)

    def suffixed_number(self, num, lang, gender=None, category=None):
        suffix = self.get_suffix(num, lang, gender=gender, category=category)
        if not suffix:
            return None
        return six.u('{}{}').format(safe_decode(num), safe_decode(suffix))

ordinal_expressions = OrdinalExpressions()
