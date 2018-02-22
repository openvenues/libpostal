import os
import sys

from collections import defaultdict

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(this_dir, os.pardir, os.pardir)))

from geodata.encoding import safe_encode, safe_decode

ADDRESS_EXPANSIONS_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                      'resources', 'dictionaries')

ADDRESS_HEADER_FILE = os.path.join(this_dir, os.pardir, os.pardir, os.pardir, 'src', 'address_expansion_rule.h')
ADDRESS_DATA_FILE = os.path.join(this_dir, os.pardir, os.pardir, os.pardir, 'src', 'address_expansion_data.c')

address_language_index_template = u'{{{language}, {index}, {length}}}'
address_expansion_rule_template = u'{{{phrase}, {num_dictionaries}, {{{dictionaries}}}, {canonical_index}}}'


address_expansion_rule_header_template = u'''
#ifndef ADDRESS_EXPANSION_RULE_H
#define ADDRESS_EXPANSION_RULE_H

#include <stdlib.h>
#include <stdint.h>

#include "constants.h"
#include "gazetteers.h"

#define MAX_DICTIONARY_TYPES {max_dictionary_types}

typedef struct address_expansion_rule {{
    char *phrase;
    uint32_t num_dictionaries;
    dictionary_type_t dictionaries[MAX_DICTIONARY_TYPES];
    int32_t canonical_index;
}} address_expansion_rule_t;

typedef struct address_language_index {{
    char language[MAX_LANGUAGE_LEN];
    uint32_t index;
    size_t len;
}} address_language_index_t;


#endif
'''

address_expansion_data_file_template = u'''
char *canonical_strings[] = {{
    {canonical_strings}
}};

address_expansion_rule_t expansion_rules[] = {{
    {expansion_rules}
}};

address_language_index_t expansion_languages[] = {{
    {address_languages}
}};
'''


gazetteer_types = {
    'academic_degrees': 'DICTIONARY_ACADEMIC_DEGREE',
    'ambiguous_expansions': 'DICTIONARY_AMBIGUOUS_EXPANSION',
    'building_types': 'DICTIONARY_BUILDING_TYPE',
    'categories': 'DICTIONARY_CATEGORY',
    'categories_plural': 'DICTIONARY_CATEGORY_PLURAL',
    'chains': 'DICTIONARY_CHAIN',
    'company_types': 'DICTIONARY_COMPANY_TYPE',
    'concatenated_prefixes_separable': 'DICTIONARY_CONCATENATED_PREFIX_SEPARABLE',
    'concatenated_suffixes_inseparable': 'DICTIONARY_CONCATENATED_SUFFIX_INSEPARABLE',
    'concatenated_suffixes_separable': 'DICTIONARY_CONCATENATED_SUFFIX_SEPARABLE',
    'cross_streets': 'DICTIONARY_CROSS_STREET',
    'directionals': 'DICTIONARY_DIRECTIONAL',
    'elisions': 'DICTIONARY_ELISION',
    'entrances': 'DICTIONARY_ENTRANCE',
    'given_names': 'DICTIONARY_GIVEN_NAME',
    'house_numbers': 'DICTIONARY_HOUSE_NUMBER',
    'level_types_basement': 'DICTIONARY_LEVEL_BASEMENT',
    'level_types_mezzanine': 'DICTIONARY_LEVEL_MEZZANINE',
    'level_types_numbered': 'DICTIONARY_LEVEL_NUMBERED',
    'level_types_standalone': 'DICTIONARY_LEVEL_STANDALONE',
    'level_types_sub_basement': 'DICTIONARY_LEVEL_SUB_BASEMENT',
    'near': 'DICTIONARY_NEAR',
    'no_number': 'DICTIONARY_NO_NUMBER',
    'number': 'DICTIONARY_NUMBER',
    'nulls': 'DICTIONARY_NULL',
    'organizations': 'DICTIONARY_NAMED_ORGANIZATION',
    'people': 'DICTIONARY_NAMED_PERSON',
    'personal_suffixes': 'DICTIONARY_PERSONAL_SUFFIX',
    'personal_titles': 'DICTIONARY_PERSONAL_TITLE',
    'place_names': 'DICTIONARY_PLACE_NAME',
    'post_office': 'DICTIONARY_POST_OFFICE',
    'postcodes': 'DICTIONARY_POSTAL_CODE',
    'qualifiers': 'DICTIONARY_QUALIFIER',
    'staircases': 'DICTIONARY_STAIRCASE',
    'stopwords': 'DICTIONARY_STOPWORD',
    'street_names': 'DICTIONARY_STREET_NAME',
    'street_types': 'DICTIONARY_STREET_TYPE',
    'surnames': 'DICTIONARY_SURNAME',
    'synonyms': 'DICTIONARY_SYNONYM',
    'toponyms': 'DICTIONARY_TOPONYM',
    'unit_directions': 'DICTIONARY_UNIT_DIRECTION',
    'unit_types_numbered': 'DICTIONARY_UNIT_NUMBERED',
    'unit_types_standalone': 'DICTIONARY_UNIT_STANDALONE',

}


class InvalidAddressFileException(Exception):
    pass


def read_dictionary_file(path):
    for i, line in enumerate(open(path)):
        line = safe_decode(line.rstrip())
        if not line.strip():
            continue

        if u'}' in line:
            raise InvalidAddressFileException(u'Found }} in file: {}, line {}'.format(path, i+1))
        phrases = line.split(u'|')

        if sum((1 for p in phrases if len(p.strip()) == 0)) > 0:
            raise InvalidAddressFileException(u'Found blank synonym in: {}, line {}'.format(path, i+1))

        yield phrases


def quote_string(s):
    return u'"{}"'.format(safe_decode(s).replace('\\', '\\\\').replace('"', '\\"'))


class AddressPhraseDictionaries(object):
    def __init__(self, base_dir=ADDRESS_EXPANSIONS_DIR):
        self.base_dir = base_dir
        self.languages = []

        self.language_dictionaries = defaultdict(list)
        self.phrases = defaultdict(list)

        for language in os.listdir(base_dir):
            language_dir = os.path.join(base_dir, language)
            if not os.path.isdir(language_dir):
                continue

            self.languages.append(language)

            for filename in os.listdir(language_dir):
                if not filename.endswith('.txt'):
                    raise InvalidAddressFileException(u'Invalid extension for file {}/{}, must be .txt'.format(language_dir, filename))
                dictionary_name = filename.split('.')[0].lower()

                if dictionary_name not in gazetteer_types:
                    raise InvalidAddressFileException(u'Invalid filename for file {}/{}. Must be one of {{{}}}'.format(language_dir, filename, ', '.join(sorted(gazetteer_types))))
                self.language_dictionaries[language].append(dictionary_name)

                path = os.path.join(language_dir, filename)
                for i, line in enumerate(open(path)):
                    line = safe_decode(line.rstrip())
                    if not line.strip():
                        continue

                    if u'}' in line:
                        raise InvalidAddressFileException(u'Found }} in file: {}, line {}'.format(path, i+1))
                    phrases = line.split(u'|')

                    if sum((1 for p in phrases if len(p.strip()) == 0)) > 0:
                        raise InvalidAddressFileException(u'Found blank synonym in: {}, line {}'.format(path, i+1))

                    self.phrases[(language, dictionary_name)].append(phrases)

        self.language_dictionaries = dict(self.language_dictionaries)
        self.phrases = dict(self.phrases)


address_phrase_dictionaries = AddressPhraseDictionaries()


def create_address_expansion_rules_file(base_dir=ADDRESS_EXPANSIONS_DIR, output_file=ADDRESS_DATA_FILE, header_file=ADDRESS_HEADER_FILE):
    address_languages = []
    expansion_rules = []
    canonical_strings = []

    max_dictionary_types = 0

    for language in address_phrase_dictionaries.languages:
        num_language_rules = 0
        language_index = len(expansion_rules)

        language_canonical_dictionaries = defaultdict(list)
        canonical_indices = {}

        for dictionary_name in address_phrase_dictionaries.language_dictionaries[language]:
            dictionary_type = gazetteer_types[dictionary_name]

            for phrases in address_phrase_dictionaries.phrases[(language, dictionary_name)]:
                canonical = phrases[0]
                if len(phrases) > 1:
                    canonical_index = canonical_indices.get(canonical, None)
                    if canonical_index is None:
                        canonical_index = len(canonical_strings)
                        canonical_strings.append(quote_string(canonical))
                        canonical_indices[canonical] = canonical_index
                else:
                    canonical_index = -1

                for i, p in enumerate(phrases):
                    language_canonical_dictionaries[p, canonical_index if i > 0 else -1].append(dictionary_type)

        for (phrase, canonical_index), dictionary_types in language_canonical_dictionaries.iteritems():
            max_dictionary_types = max(max_dictionary_types, len(dictionary_types))
            rule_template = address_expansion_rule_template.format(phrase=quote_string(phrase),
                                                                   num_dictionaries=str(len(dictionary_types)),
                                                                   dictionaries=', '.join(dictionary_types),
                                                                   canonical_index=canonical_index)
            expansion_rules.append(rule_template)
            num_language_rules += 1

        address_languages.append(address_language_index_template.format(language=quote_string(language),
                                                                        index=language_index,
                                                                        length=num_language_rules))

    header = address_expansion_rule_header_template.format(
        max_dictionary_types=str(max_dictionary_types)
    )
    out = open(header_file, 'w')
    out.write(safe_encode(header))
    out.close()

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
