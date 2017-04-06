import copy
import operator
import os
import random
import six
import yaml

from collections import defaultdict

from geodata.address_expansions.address_dictionaries import address_phrase_dictionaries
from geodata.address_formatting.formatter import AddressFormatter
from geodata.configs.utils import nested_get, recursive_merge


this_dir = os.path.realpath(os.path.dirname(__file__))

POSTAL_CODES_CONFIG_FILE = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                        'resources', 'postal_codes', 'config.yaml')


class PostalCodesConfig(object):
    def __init__(self, config_file=POSTAL_CODES_CONFIG_FILE):
        self.cache = {}
        postal_codes_config = yaml.load(open(config_file))

        self.global_config = postal_codes_config['global']
        self.country_configs = {}

        countries = postal_codes_config.pop('countries', {})

        for k, v in six.iteritems(countries):
            country_config = countries[k]
            global_config_copy = copy.deepcopy(self.global_config)
            self.country_configs[k] = recursive_merge(global_config_copy, country_config)

        self.country_configs[None] = self.global_config

    def get_property(self, key, country=None, default=None):
        if isinstance(key, six.string_types):
            key = key.split(u'.')

        config = self.global_config

        if country:
            country_config = self.country_configs.get(country.lower(), {})
            if country_config:
                config = country_config

        return nested_get(config, key, default=default)

postal_codes_config = PostalCodesConfig()
