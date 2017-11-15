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

        self.language_prefix_sim_only_regexes = {}
        self.language_suffix_sim_only_regexes = {}

        for filename in os.listdir(config_dir):
            if not filename.endswith('.yaml'):
                continue
            lang = filename.rsplit('.yaml')[0]

            conf = yaml.load(open(os.path.join(config_dir, filename)))
            self.add_affixes(lang, conf)

            for country, country_conf in six.iteritems(conf.get('countries', {})):
                country_lang = (country, lang)
                self.add_affixes(country_lang, country_conf)

    def add_affixes(self, lang, *confs):
        prefixes = [safe_decode(phrase).lower() for conf in confs for phrase in conf.get('prefixes', [])]
        prefixes_no_whitespace = [safe_decode(phrase).lower() for conf in confs for phrase in conf.get('prefixes_no_whitespace', [])]

        self.language_prefixes[lang] = prefixes + prefixes_no_whitespace

        suffixes = [safe_decode(phrase).lower() for conf in confs for phrase in conf.get('suffixes', [])]
        suffixes_no_whitespace = [safe_decode(phrase).lower() for conf in confs for phrase in conf.get('suffixes_no_whitespace', [])]

        self.language_suffixes[lang] = suffixes + suffixes_no_whitespace

        whitespace_phrase = six.u('[ \-]')

        all_prefixes = [six.u('{}{}').format(s, whitespace_phrase) for s in prefixes] + prefixes_no_whitespace
        all_suffixes = [six.u('{}{}').format(whitespace_phrase, s) for s in suffixes] + suffixes_no_whitespace

        if all_prefixes:
            prefix_regex = six.u('^(?:{})').format(six.u('|').join(all_prefixes))
            self.language_prefix_regexes[lang] = re.compile(prefix_regex, re.I | re.UNICODE)

        if all_suffixes:
            suffix_regex = six.u('(?:{})$').format(six.u('|').join(all_suffixes))
            self.language_suffix_regexes[lang] = re.compile(suffix_regex, re.I | re.UNICODE)

        sim_only_prefixes = [six.u('{}{}').format(safe_decode(phrase.lower()), whitespace_phrase) for conf in confs for phrase in conf.get('prefixes_similarity_only', [])]
        if sim_only_prefixes:
            sim_only_prefix_regex = six.u('^(?:{})').format(six.u('|').join(sim_only_prefixes + all_prefixes))
            self.language_prefix_sim_only_regexes[lang] = re.compile(sim_only_prefix_regex, re.I | re.UNICODE)

        sim_only_suffixes = [six.u('(?:{})$').format(whitespace_phrase, safe_decode(phrase.lower())) for conf in confs for phrase in conf.get('suffixes_similarity_only', [])]
        if sim_only_suffixes:
            sim_only_suffix_regex = six.u('(?:{})$').format(six.u('|').join(sim_only_suffixes + all_suffixes))

            self.language_suffix_sim_only_regexes[lang] = re.compile(sim_only_suffix_regex, re.I | re.UNICODE)

    def replace_prefixes(self, name, lang, country=None, sim_only=False):
        name = safe_decode(name).strip()

        if not sim_only or lang not in self.language_prefix_sim_only_regexes:
            d = self.language_prefix_regexes
        else:
            d = self.language_prefix_sim_only_regexes

        re = None
        if country is not None:
            re = d.get((country, lang))
            if re:
                name = re.sub(six.u(''), name)

        re = d.get(lang)

        if not re:
            return name

        return re.sub(six.u(''), name)

    def replace_suffixes(self, name, lang, country=None, sim_only=False):
        name = safe_decode(name).strip()

        if not sim_only or lang not in self.language_suffix_sim_only_regexes:
            d = self.language_suffix_regexes
        else:
            d = self.language_suffix_sim_only_regexes

        re = None
        if country is not None:
            re = d.get((country, lang))
            if re:
                name = re.sub(six.u(''), name)

        re = d.get(lang)

        if not re:
            return name

        return re.sub(six.u(''), name)

    def replace_affixes(self, name, lang, country=None, sim_only=False):
        return self.replace_prefixes(self.replace_suffixes(name, lang, country=country, sim_only=sim_only), lang, country=country, sim_only=sim_only)

name_affixes = NameAffixes()
