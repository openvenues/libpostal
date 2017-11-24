import os
import sys

import yaml

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(this_dir, os.pardir, os.pardir)))

from geodata.encoding import safe_encode
from geodata.i18n.unicode_paths import DATA_DIR


class InvalidNumexRuleException(Exception):
    pass

NUMEX_DATA_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                              'resources', 'numex')

NUMEX_RULES_FILE = os.path.join(this_dir, os.pardir, os.pardir, os.pardir, 'src', 'numex_data.c')

GENDER_MASCULINE = 'GENDER_MASCULINE'
GENDER_FEMININE = 'GENDER_FEMININE'
GENDER_NEUTER = 'GENDER_NEUTER'
GENDER_NONE = 'GENDER_NONE'

gender_map = {
    'm': GENDER_MASCULINE,
    'f': GENDER_FEMININE,
    'n': GENDER_NEUTER,
    None: GENDER_NONE,
}


CATEGORY_PLURAL = 'CATEGORY_PLURAL'
CATEGORY_DEFAULT = 'CATEGORY_DEFAULT'

valid_numex_keys = set(['name', 'value', 'type', 'left', 'right', 'gender', 'category', 'radix',
                        'multiply_gte', 'exact_multiple_only', 'left_separator', 'right_separator'])

valid_ordinal_keys = set(['suffixes', 'gender', 'category'])


category_map = {
    'plural': CATEGORY_PLURAL,
    None: CATEGORY_DEFAULT
}

LEFT_CONTEXT_MULTIPLY = 'NUMEX_LEFT_CONTEXT_MULTIPLY'
LEFT_CONTEXT_ADD = 'NUMEX_LEFT_CONTEXT_ADD'
LEFT_CONTEXT_CONCAT_ONLY_IF_NUMBER = 'NUMEX_LEFT_CONTEXT_CONCAT_ONLY_IF_NUMBER'
LEFT_CONTEXT_NONE = 'NUMEX_LEFT_CONTEXT_NONE'

left_context_map = {
    'add': LEFT_CONTEXT_ADD,
    'multiply': LEFT_CONTEXT_MULTIPLY,
    'concat_only_if_number': LEFT_CONTEXT_CONCAT_ONLY_IF_NUMBER,
    None: LEFT_CONTEXT_NONE,
}

RIGHT_CONTEXT_MULTIPLY = 'NUMEX_RIGHT_CONTEXT_MULTIPLY'
RIGHT_CONTEXT_ADD = 'NUMEX_RIGHT_CONTEXT_ADD'
RIGHT_CONTEXT_NONE = 'NUMEX_RIGHT_CONTEXT_NONE'

right_context_map = {
    'add': RIGHT_CONTEXT_ADD,
    'multiply': RIGHT_CONTEXT_MULTIPLY,
    None: RIGHT_CONTEXT_NONE,
}

CARDINAL = 'NUMEX_CARDINAL_RULE'
ORDINAL = 'NUMEX_ORDINAL_RULE'
ORDINAL_INDICATOR = 'NUMEX_ORDINAL_INDICATOR_RULE'

rule_type_map = {
    'cardinal': CARDINAL,
    'ordinal': ORDINAL,
    'ordinal_indicator': ORDINAL_INDICATOR,
}

numex_key_template = u'"{key}"'
numex_rule_template = u'{{{left_context_type}, {right_context_type}, {rule_type}, {gender}, {category}, {radix}, {value}LL}}'

stopword_rule = u'NUMEX_STOPWORD_RULE'

ordinal_indicator_template = u'{{"{key}", {gender}, {category}, "{value}"}}'

stopwords_template = u'"{word}"'

language_template = u'{{"{language}", {whole_words_only}, {rule_index}, {num_rules}, {ordinal_indicator_index}, {num_ordinal_indicators}}}'

numex_rules_data_template = u'''
char *numex_keys[] = {{
    {numex_keys}
}};

numex_rule_t numex_rules[] = {{
    {numex_rules}
}};

ordinal_indicator_t ordinal_indicator_rules[] = {{
    {ordinal_indicator_rules}
}};

numex_language_source_t numex_languages[] = {{
    {languages}
}};
'''


def parse_numex_rules(dirname=NUMEX_DATA_DIR, outfile=NUMEX_RULES_FILE):
    all_keys = []
    all_rules = []

    all_ordinal_indicators = []
    all_stopwords = []

    all_languages = []

    out = open(outfile, 'w')

    for filename in os.listdir(dirname):
        path = os.path.join(dirname, filename)
        if not os.path.isfile(path) or not filename.endswith('.yaml'):
            continue

        language = filename.split('.yaml', 1)[0]

        data = yaml.load(open(path))

        whole_words_only = data.get('whole_words_only', False)

        rules = data.get('rules', [])
        rule_index = len(all_rules)

        for rule in rules:
            invalid_keys = set(rule.keys()) - valid_numex_keys
            if invalid_keys:
                raise InvalidNumexRuleException(u'Invalid keys: ({}) for language {}, rule: {}'.format(u','.join(invalid_keys), language, rule))
            gender = gender_map[rule.get('gender')]
            rule_type = rule_type_map[rule['type']]
            key = rule['name']
            value = rule['value']
            radix = rule.get('radix', 10)
            rule_category = rule.get('category')
            category = category_map.get(rule_category)
            if category is None:
                continue
            left_context_type = left_context_map[rule.get('left')]
            right_context_type = right_context_map[rule.get('right')]
            all_keys.append(unicode(numex_key_template.format(key=key)))
            all_rules.append(unicode(numex_rule_template.format(
                language=language,
                rule_type=rule_type,
                gender=gender,
                category=category,
                left_context_type=left_context_type,
                right_context_type=right_context_type,
                value=value,
                radix=radix
            )))

        ordinal_indicator_index = len(all_ordinal_indicators)
        ordinal_indicators = data.get('ordinal_indicators', [])
        num_ordinal_indicators = 0

        for rule in ordinal_indicators:
            gender = gender_map[rule.get('gender')]
            category = category_map[rule.get('category')]
            invalid_ordinal_keys = set(rule.keys()) - valid_ordinal_keys
            if invalid_ordinal_keys:
                raise InvalidNumexRuleException(u'Invalid keys ({}) in ordinal rule for language {}, rule: {}'.format(u','.join(invalid_ordinal_keys), language, rule))

            for key, suffixes in rule['suffixes'].iteritems():
                for suffix in suffixes:
                    all_ordinal_indicators.append(unicode(ordinal_indicator_template.format(
                        key=key,
                        value=suffix,
                        gender=gender,
                        category=category
                    )))
                num_ordinal_indicators += len(suffixes)

        stopwords = data.get('stopwords', [])
        stopword_index = len(all_stopwords)
        num_stopwords = len(stopwords)

        for stopword in stopwords:
            all_keys.append(numex_key_template.format(key=unicode(stopword)))
            all_rules.append(stopword_rule)

        num_rules = len(rules) + len(stopwords)

        all_languages.append(unicode(language_template.format(
            language=language,
            whole_words_only=int(whole_words_only),
            rule_index=rule_index,
            num_rules=num_rules,
            ordinal_indicator_index=ordinal_indicator_index,
            num_ordinal_indicators=num_ordinal_indicators
        )))

    out.write(safe_encode(numex_rules_data_template.format(
        numex_keys=u''',
    '''.join(all_keys),
        numex_rules=u''',
    '''.join(all_rules),
        ordinal_indicator_rules=u''',
    '''.join(all_ordinal_indicators),
        stopwords=u''',
    '''.join(all_stopwords),
        languages=u''',
    '''.join(all_languages),
    )))

    out.close()


if __name__ == '__main__':
    parse_numex_rules(*sys.argv[1:])
