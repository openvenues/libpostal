import csv
import os
import six
import random
import sys

from collections import defaultdict

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(this_dir, os.pardir, os.pardir)))

from geodata.encoding import safe_decode

CATEGORIES_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                              'resources', 'categories')


class CategoryConfig(object):
    def __init__(self, base_dir=CATEGORIES_DIR):
        self.language_categories_singular = {}
        self.language_categories_plural = {}

        self.language_property_names = defaultdict(set)

        if not os.path.exists(base_dir):
            raise RuntimeError('{} does not exist'.format(base_dir))

        for filename in os.listdir(base_dir):
            if not filename.endswith('.tsv'):
                continue

            lang = filename.rsplit('.tsv')[0]
            base_lang = lang.split('_')[0]

            singular_rules = self.language_categories_singular.get(base_lang, defaultdict(list))
            plural_rules = self.language_categories_plural.get(base_lang, defaultdict(list))

            reader = csv.reader(open(os.path.join(CATEGORIES_DIR, filename)), delimiter='\t')
            reader.next()  # headers

            for key, value, is_plural, phrase in reader:
                self.language_property_names[lang].add(key)
                is_plural = bool(int(is_plural))
                if is_plural:
                    plural_rules[(key, value)].append(phrase)
                else:
                    singular_rules[(key, value)].append(phrase)

            self.language_categories_singular[base_lang] = singular_rules
            self.language_categories_plural[base_lang] = plural_rules

        self.language_categories_singular = {key: dict(value) for key, value
                                             in six.iteritems(self.language_categories_singular)}

        self.language_categories_plural = {key: dict(value) for key, value
                                           in six.iteritems(self.language_categories_plural)}

    def has_keys(self, language, keys):
        prop_names = self.language_property_names.get(language, set())
        return [k for k in keys if k in prop_names]

    def get_phrase(self, language, key, value, is_plural=False):
        config = self.language_categories_singular if not is_plural else self.language_categories_plural
        if language not in config:
            return None
        language_config = config[language]
        choices = language_config.get((key, value))
        if not choices:
            return None
        return random.choice(choices)

category_config = CategoryConfig()
