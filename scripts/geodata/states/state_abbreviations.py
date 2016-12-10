import os
import random
import six
import sys
import yaml

from collections import defaultdict

this_dir = os.path.realpath(os.path.dirname(__file__))

from geodata.configs.utils import nested_get, DoesNotExist
from geodata.encoding import safe_decode


STATE_DIR = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                         'resources', 'states')


class StateAbbreviations(object):
    def __init__(self, base_dir=STATE_DIR):
        self.full_names = {}
        self.abbreviations = {}

        for filename in os.listdir(base_dir):
            country = filename.split('.yaml')[0]
            country_config = yaml.load(open(os.path.join(base_dir, filename)))

            country_abbreviations = defaultdict(list)
            country_full_names = defaultdict(dict)

            for abbreviation, vals in six.iteritems(country_config):
                for language, full_name in six.iteritems(vals):
                    full_name = safe_decode(full_name)
                    abbreviation = safe_decode(abbreviation)
                    country_abbreviations[(full_name.lower(), language)].append(abbreviation)
                    country_full_names[abbreviation.lower()][language] = full_name

            self.abbreviations[country] = dict(country_abbreviations)
            self.full_names[country] = dict(country_full_names)

    def get_all_abbreviations(self, country, language, state, default=None):
        values = nested_get(self.abbreviations, (country.lower(), (state.lower(), language.lower())))
        if values is DoesNotExist:
            return default
        return values

    def get_abbreviation(self, country, language, state, default=None):
        values = self.get_all_abbreviations(country, language, state, default=default)
        if values == default:
            return default
        elif len(values) == 1:
            return values[0]
        return random.choice(values)

    def get_full_name(self, country, language, state, default=None):
        value = nested_get(self.full_names, (country.lower(), state.lower(), language.lower()))
        if value is DoesNotExist:
            return default
        return value


state_abbreviations = StateAbbreviations()
