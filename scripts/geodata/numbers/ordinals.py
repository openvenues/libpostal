import os
import six
import sys

import ujson as json

from marisa_trie import BytesTrie

from geodata.text.phrases import PhraseFilter
from geodata.encoding import safe_encode, safe_decode
from geodata.i18n.unicode_paths import DATA_DIR

from geodata.numbers.numex import NUMEX_DATA_DIR


class OrdinalTrie(PhraseFilter):
    def __init__(self, ordinal_rules):
        self.trie = BytesTrie([(k[::-1], safe_decode('|').join(v).encode('utf-8')) for k, v in six.iteritems(ordinal_rules)])
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

ordinal_rules = {}
ordinal_rules_configured = False


def init_ordinal_rules(d=NUMEX_DATA_DIR):
    global ordinal_rules, ordinal_rules_configured
    for filename in os.listdir(d):
        if filename.endswith('.json'):
            lang = filename.split('.json')[0]
            f = open(os.path.join(d, filename))
            data = json.load(f)
            rules = data.get('ordinal_indicators')
            if rules is not None and hasattr(rules, '__getslice__'):
                lang_rules = []
                for rule_set in rules:
                    gender = rule_set.get('gender', None)
                    category = rule_set.get('category', None)
                    ordinal_rules[(lang, gender, category)] = OrdinalTrie(rule_set['suffixes'])
    ordinal_rules_configured = True


def ordinal_suffixes(num, lang, gender=None, category=None):
    if not ordinal_rules_configured:
        raise RuntimeError('ordinal rules not configured')
    trie = ordinal_rules.get((lang, gender, category))
    if not trie:
        return None

    return trie.search_suffix(str(num))
