import os
import re
import six
import yaml

from geodata.encoding import safe_decode

this_dir = os.path.realpath(os.path.dirname(__file__))

AFFIX_CONFIG_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                'resources', 'boundaries', 'names', 'languages')


class NameAffixes(object):
    def __init__(self, config_dir=AFFIX_CONFIG_DIR):
        self.config_dir = config_dir

        self.language_prefixes = {}
        self.language_suffixes = {}

        self.language_prefix_regexes = {}
        self.language_suffix_regexes = {}

        for filename in os.listdir(config_dir):
            if not filename.endswith('.yaml'):
                continue
            lang = filename.rsplit('.yaml')[0]

            conf = yaml.load(open(os.path.join(config_dir, filename)))

            prefixes = [safe_decode(phrase).lower() for phrase in conf.get('prefixes', [])]
            prefixes_no_whitespace = [safe_decode(phrase).lower() for phrase in conf.get('prefixes_no_whitespace', [])]

            self.language_prefixes[lang] = prefixes + prefixes_no_whitespace

            suffixes = [safe_decode(phrase).lower() for phrase in conf.get('suffixes', [])]
            suffixes_no_whitespace = [safe_decode(phrase).lower() for phrase in conf.get('suffixes_no_whitespace', [])]

            self.language_suffixes[lang] = suffixes + suffixes_no_whitespace

            whitespace_phrase = six.u('[ \-]')

            all_prefixes = [six.u('{}{}').format(s, whitespace_phrase) for s in prefixes] + prefixes_no_whitespace
            all_suffixes = [six.u('{}{}').format(whitespace_phrase, s) for s in suffixes] + suffixes_no_whitespace

            prefix_regex = six.u('^(?:{})').format(six.u('|').join(all_prefixes))
            suffix_regex = six.u('(?:{})$').format(six.u('|').join(all_suffixes))

            self.language_prefix_regexes[lang] = re.compile(prefix_regex, re.I | re.UNICODE)
            self.language_suffix_regexes[lang] = re.compile(suffix_regex, re.I | re.UNICODE)

    def replace_prefixes(self, name, lang):
        name = safe_decode(name).strip()

        re = self.language_prefix_regexes.get(lang)
        if not re:
            return name

        return re.sub(six.u(''), name)

    def replace_suffixes(self, name, lang):
        name = safe_decode(name).strip()

        re = self.language_suffix_regexes.get(lang)
        if not re:
            return name

        return re.sub(six.u(''), name)

name_affixes = NameAffixes()
