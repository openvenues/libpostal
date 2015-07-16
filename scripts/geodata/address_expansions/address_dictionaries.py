import os
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.encoding import safe_encode, safe_decode

ADDRESS_EXPANSIONS_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                      'data', 'dictionaries')

ADDRESS_DATA_FILE = os.path.join(os.pardir, os.pardir, os.pardir, 'src', 'address_expansion_data.c')


address_language_index_template = u'{{{language}, {index}, {length}}}'
address_expansion_rule_template = u'{{{phrase}, {dictionary}, {canonical_index}}}'


address_expansion_data_file_template = u'''
char *canonical_strings[] = {{
    {canonical_strings}
}};

address_expansion_rule_t expansion_rules[] = {{
    {expansion_rules}
}};

address_language_index_t languages[] = {{
    {address_languages}
}};
'''


class InvalidAddressFileException(Exception):
    pass


def quote_string(s):
    return u'"{}"'.format(safe_decode(s).replace('"', '\\"'))


def create_address_expansion_rules_file(base_dir=ADDRESS_EXPANSIONS_DIR, output_file=ADDRESS_DATA_FILE):
    address_languages = []
    expansion_rules = []
    canonical_strings = []

    for language in os.listdir(base_dir):
        language_dir = os.path.join(base_dir, language)

        num_language_rules = 0
        language_index = len(expansion_rules)

        for filename in os.listdir(language_dir):
            dictionary_name = filename.rstrip('.txt')
            assert '.' not in dictionary_name

            f = open(os.path.join(language_dir, filename))
            for i, line in enumerate(f):
                line = safe_decode(line.rstrip())
                if not line.strip():
                    continue

                if u'}' in line:
                    raise InvalidAddressFileException(u'found }} in file: {}/{}, line {}'.format(language, filename, i+1))
                phrases = line.split(u'|')
                if sum((1 for p in phrases if len(p.strip()) == 0)) > 0:
                    raise InvalidAddressFileException(u'found blank synonym in: {}/{}, line {}'.format(language, filename, i+1))

                canonical = phrases[0]
                if len(phrases) > 1:
                    canonical_index = len(canonical_strings)
                    canonical_strings.append(quote_string(canonical))
                else:
                    canonical_index = -1

                for p in phrases:
                    rule_template = address_expansion_rule_template.format(phrase=quote_string(p),
                                                                           dictionary=quote_string(dictionary_name),
                                                                           canonical_index=canonical_index)
                    expansion_rules.append(rule_template)
                    num_language_rules += 1

        address_languages.append(address_language_index_template.format(language=quote_string(language),
                                                                        index=language_index,
                                                                        length=num_language_rules))

    data_file = address_expansion_data_file_template.format(
        canonical_strings=u''',
    '''.join(canonical_strings),
        expansion_rules=u''',
    '''.join(expansion_rules),
        address_languages=u''',
    '''.join(address_languages),
    )

    out = open(output_file, 'w')
    out.write(safe_encode(data_file))
    out.close()


if __name__ == '__main__':
    if len(sys.argv) > 1:
        input_dir = sys.argv[1]
    else:
        input_dir = ADDRESS_EXPANSIONS_DIR

    create_address_expansion_rules_file(base_dir=input_dir, output_file=ADDRESS_DATA_FILE)
