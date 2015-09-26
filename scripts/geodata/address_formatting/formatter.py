# -*- coding: utf-8 -*-
import os
import pystache
import re
import subprocess
import yaml

from postal.text.tokenize import tokenize, tokenize_raw, token_types
from collections import OrderedDict
from itertools import ifilter

FORMATTER_GIT_REPO = 'https://github.com/OpenCageData/address-formatting'


class AddressFormatter(object):
    '''
    Approximate Python port of lokku's Geo::Address::Formatter

    Usage:
        address_formatter = AddressFormatter()
        components = {
            'house': u'Anticafé',
            'addr:housenumber': '2',
            'addr:street': u'Calle de la Unión',
            'addr:postcode': '28013',
            'addr:city': u'Madrid',
        }
        address_formatter.format_address('es', components)
    '''

    MINIMAL_COMPONENT_KEYS = [
        ('road', 'house_number'),
        ('road', 'house'),
        ('road', 'postcode')
    ]

    whitespace_component_regex = re.compile('[\r\n]+[\s\r\n]*')

    splitter = ' | '

    separator_tag = 'SEP'
    field_separator_tag = 'FSEP'

    aliases = OrderedDict([
        ('name', 'house'),
        ('addr:housename', 'house'),
        ('addr:housenumber', 'house_number'),
        ('addr:house_number', 'house_number'),
        ('addr:street', 'road'),
        ('addr:city', 'city'),
        ('addr:locality', 'city'),
        ('addr:municipality', 'city'),
        ('addr:hamlet', 'village'),
        ('addr:suburb', 'suburb'),
        ('addr:neighbourhood', 'suburb'),
        ('addr:neighborhood', 'suburb'),
        ('addr:district', 'suburb'),
        ('addr:state', 'state'),
        ('addr:province', 'state'),
        ('addr:region', 'state'),
        ('addr:postal_code', 'postcode'),
        ('addr:postcode', 'postcode'),
        ('addr:country', 'country'),
        ('street', 'road'),
        ('street_name', 'road'),
        ('residential', 'road'),
        ('hamlet', 'village'),
        ('neighborhood', 'suburb'),
        ('neighbourhood', 'suburb'),
        ('city_district', 'suburb'),
        ('state_code', 'state'),
        ('country_name', 'country'),
    ])

    def __init__(self, scratch_dir='/tmp', splitter=None):
        if splitter is not None:
            self.splitter = splitter

        self.formatter_repo_path = os.path.join(scratch_dir, 'address-formatting')
        self.clone_repo()
        self.load_config()

    def clone_repo(self):
        subprocess.check_call(['rm', '-rf', self.formatter_repo_path])
        subprocess.check_call(['git', 'clone', FORMATTER_GIT_REPO, self.formatter_repo_path])

    def load_config(self):
        self.config = yaml.load(open(os.path.join(self.formatter_repo_path,
                                'conf/countries/worldwide.yaml')))

    def component_aliases(self):
        self.aliases = OrderedDict()
        self.aliases.update(self.osm_aliases)
        components = yaml.load_all(open(os.path.join(self.formatter_repo_path,
                                   'conf', 'components.yaml')))
        for c in components:
            name = c['name']
            for a in c.get('aliases', []):
                self.aliases[a] = name

    def replace_aliases(self, components):
        for k in components.keys():
            new_key = self.aliases.get(k)
            if new_key and new_key not in components:
                components[new_key] = components.pop(k)

    def country_template(self, c):
        return self.config.get(c, self.config['default'])

    def render_template(self, template, components, tagged=False):
        def render_first(text):
            text = pystache.render(text, **components)
            splits = (e.strip() for e in text.split('||'))
            selected = next(ifilter(bool, splits), '')
            return selected

        output = pystache.render(template, first=render_first,
                                 **components).strip()

        values = self.whitespace_component_regex.split(output)

        splitter = self.splitter if not tagged else ' {}/{} '.format(self.splitter.strip(), self.field_separator_tag)

        output = splitter.join([
            self.strip_component(val, tagged=tagged)
            for val in values
        ])

        return output

    def minimal_components(self, components):
        for component_list in self.MINIMAL_COMPONENT_KEYS:
            if all((c in components for c in component_list)):
                return True
        return False

    def apply_replacements(self, template, components):
        if not template.get('replace'):
            return
        for key in components.keys():
            value = components[key]
            for regex, replacement in template['replace']:
                value = re.sub(regex, replacement, value)
                components[key] = value

    def post_replacements(self, template, text):
        components = []
        seen = set()
        for component in text.split(self.splitter):
            component = component.strip()
            if component not in seen:
                components.append(component)
                seen.add(component)
        text = self.splitter.join(components)
        post_format_replacements = template.get('postformat_replace')
        if post_format_replacements:
            for regex, replacement in post_format_replacements:
                text = re.sub(regex, replacement, text)
        return text

    def tag_template_separators(self, template):
        template = re.sub(r'},', '}} ,/{} '.format(self.separator_tag), template)
        template = re.sub(r'}-', '}} -/{} '.format(self.separator_tag), template)
        template = re.sub(r' - ', ' -/{} '.format(self.separator_tag), template)
        return template

    def strip_component(self, value, tagged=False):
        if not tagged:
            start = end = 0
            tokens = tokenize_raw(value)
            for token_start, token_length, token_type in tokens:
                start = token_start
                if token_type < token_types.PERIOD.value:
                    break

            for token_start, token_length, token_type in reversed(tokens):
                end = token_start + token_length
                if token_type < token_types.PERIOD.value:
                    break

            return value[start:end]
        else:
            i = j = 0
            tokens = value.split()

            separator_tag = self.separator_tag

            for i, t in enumerate(tokens):
                t, c = t.split('/')
                if c != separator_tag:
                    break

            for j, t in enumerate(reversed(tokens)):
                t, c = t.split('/')
                if c != separator_tag:
                    break

            if j == 0:
                j = None
            else:
                j = -j
            return u' '.join(tokens[i:j])

    def format_address(self, country, components, minimal_only=True, tag_components=True):
        template = self.config.get(country.upper())
        if not template:
            return None
        template_text = template['address_template']
        self.replace_aliases(components)

        if not self.minimal_components(components):
            if minimal_only:
                return None
            if 'fallback_template' in template:
                template_text = template['fallback_template']
            else:
                template_text = self.config['default']['fallback_template']

        self.apply_replacements(template, components)

        if tag_components:
            template_text = self.tag_template_separators(template_text)
            components = {k: u' '.join([u'{}/{}'.format(t, k.replace(' ', '_'))
                                        for t, c in tokenize(v)])
                          for k, v in components.iteritems()}

        text = self.render_template(template_text, components, tagged=tag_components)

        text = self.post_replacements(template, text)
        return text
