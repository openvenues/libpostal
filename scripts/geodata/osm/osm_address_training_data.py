# -*- coding: utf-8 -*-
import argparse
import csv
import os
import operator
import pystache
import re
import subprocess
import sys
import tempfile
import ujson as json
import yaml

from collections import defaultdict, OrderedDict
from lxml import etree
from itertools import ifilter, chain

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir, os.pardir, 'python')))

from address_normalizer.text.tokenize import *
from address_normalizer.text.normalize import PhraseFilter
from geodata.i18n.languages import *
from geodata.polygons.language_polys import *
from geodata.i18n.unicode_paths import DATA_DIR

from marisa_trie import BytesTrie

from geodata.csv_utils import *
from geodata.file_utils import *

this_dir = os.path.realpath(os.path.dirname(__file__))

FORMATTER_GIT_REPO = 'https://github.com/OpenCageData/address-formatting'

WAY_OFFSET = 10 ** 15
RELATION_OFFSET = 2 * 10 ** 15

PLANET_ADDRESSES_INPUT_FILE = 'planet-addresses.osm'
PLANET_ADDRESSES_OUTPUT_FILE = 'planet-addresses.tsv'

PLANET_WAYS_INPUT_FILE = 'planet-ways.osm'
PLANET_WAYS_OUTPUT_FILE = 'planet-ways.tsv'

PLANET_VENUES_INPUT_FILE = 'planet-venues.osm'
PLANET_VENUES_OUTPUT_FILE = 'planet-venues.tsv'

DICTIONARIES_DIR = os.path.join(DATA_DIR, 'dictionaries')

ALL_OSM_TAGS = set(['node', 'way', 'relation'])
WAYS_RELATIONS = set(['way', 'relation'])


class OSMField(object):
    def __init__(self, name, c_constant, alternates=None):
        self.name = name
        self.c_constant = c_constant
        self.alternates = alternates

osm_fields = [
    # Field if alternate_names present, default field name if not, C header constant
    OSMField('addr:housename', 'OSM_HOUSE_NAME'),
    OSMField('addr:housenumber', 'OSM_HOUSE_NUMBER'),
    OSMField('addr:block', 'OSM_BLOCK'),
    OSMField('addr:street', 'OSM_STREET_ADDRESS'),
    OSMField('addr:place', 'OSM_PLACE'),
    OSMField('addr:city', 'OSM_CITY', alternates=['addr:locality', 'addr:municipality', 'addr:hamlet']),
    OSMField('addr:suburb', 'OSM_SUBURB'),
    OSMField('addr:neighborhood', 'OSM_NEIGHBORHOOD', alternates=['addr:neighbourhood']),
    OSMField('addr:district', 'OSM_DISTRICT'),
    OSMField('addr:subdistrict', 'OSM_SUBDISTRICT'),
    OSMField('addr:ward', 'OSM_WARD'),
    OSMField('addr:state', 'OSM_STATE'),
    OSMField('addr:province', 'OSM_PROVINCE'),
    OSMField('addr:postcode', 'OSM_POSTAL_CODE', alternates=['addr:postal_code']),
    OSMField('addr:country', 'OSM_COUNTRY'),
]


PREFIX_KEY = u'\x02'
SUFFIX_KEY = u'\x03'

POSSIBLE_ROMAN_NUMERALS = set(['i', 'ii', 'iii', 'iv', 'v', 'vi', 'vii', 'viii', 'ix',
                               'x', 'xi', 'xii', 'xiii', 'xiv', 'xv', 'xvi', 'xvii', 'xviii', 'xix',
                               'xx', 'xxx', 'xl', 'l', 'lx', 'lxx', 'lxxx', 'xc',
                               'c', 'cc', 'ccc', 'cd', 'd', 'dc', 'dcc', 'dccc', 'cm',
                               'm', 'mm', 'mmm', 'mmmm'])


class StreetTypesGazetteer(PhraseFilter):
    def serialize(self, s):
        return s

    def deserialize(self, s):
        return s

    def configure(self, base_dir=DICTIONARIES_DIR):
        kvs = defaultdict(OrderedDict)
        for lang in os.listdir(DICTIONARIES_DIR):
            for filename in ('street_types.txt', 'directionals.txt'):
                path = os.path.join(DICTIONARIES_DIR, lang, filename)
                if not os.path.exists(path):
                    continue

                for line in open(path):
                    line = line.strip()
                    if not line:
                        continue
                    canonical = safe_decode(line.split('|')[0])
                    if canonical in POSSIBLE_ROMAN_NUMERALS:
                        continue
                    kvs[canonical][lang] = None
            for filename in ('concatenated_suffixes_separable.txt', 'concatenated_suffixes_inseparable.txt', 'concatenated_prefixes_separable.txt'):
                path = os.path.join(DICTIONARIES_DIR, lang, filename)
                if not os.path.exists(path):
                    continue

                for line in open(path):
                    line = line.strip()
                    if not line:
                        continue

                    canonical = safe_decode(line.split('|')[0])
                    if 'suffixes' in filename:
                        canonical = SUFFIX_KEY + canonical[::-1]
                    else:
                        canonical = PREFIX_KEY + canonical

                    kvs[canonical][lang] = None

        kvs = [(k, v) for k, vals in kvs.iteritems() for v in vals.keys()]

        self.trie = BytesTrie(kvs)
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

    def filter(self, *args, **kw):
        for c, t, data in super(StreetTypesGazetteer, self).filter(*args):
            if c != token_types.PHRASE:
                token = t[1]
                suffix_search, suffix_len = self.search_substring(SUFFIX_KEY + token[::-1])

                if suffix_search and self.trie.get(token[len(token) - (suffix_len - len(SUFFIX_KEY)):]):
                    yield (token_types.PHRASE, [(c, t)], suffix_search)
                    continue
                prefix_search, prefix_len = self.search_substring(PREFIX_KEY + token)
                if prefix_search and self.trie.get(token[:prefix_len - len(PREFIX_KEY)]):
                    yield (token_types.PHRASE, [(c, t)], prefix_search)
                    continue
            yield c, t, data

street_types_gazetteer = StreetTypesGazetteer()


# Currently, all our data sets are converted to nodes with osmconvert before parsing
def parse_osm(filename, allowed_types=ALL_OSM_TAGS):
    f = open(filename)
    parser = etree.iterparse(f)

    single_type = len(allowed_types) == 1

    for (_, elem) in parser:
        elem_id = long(elem.attrib.pop('id', 0))
        item_type = elem.tag
        if elem_id >= WAY_OFFSET and elem_id < RELATION_OFFSET:
            elem_id -= WAY_OFFSET
            item_type = 'way'
        elif elem_id >= RELATION_OFFSET:
            elem_id -= RELATION_OFFSET
            item_type = 'relation'

        if item_type in allowed_types:
            attrs = dict(elem.attrib)
            attrs.update({e.attrib['k']: e.attrib['v']
                         for e in elem.getchildren() if e.tag == 'tag'})
            key = elem_id if single_type else '{}:{}'.format(item_type, elem_id)
            yield key, attrs

        if elem.tag != 'tag':
            elem.clear()
            while elem.getprevious() is not None:
                del elem.getparent()[0]


def write_osm_json(filename, out_filename):
    out = open(out_filename, 'w')
    writer = csv.writer(out, 'tsv_no_quote')
    for key, attrs in parse_osm(filename):
        writer.writerow((key, json.dumps(attrs)))
    out.close()


def read_osm_json(filename):
    reader = csv.reader(open(filename), delimiter='\t')
    for key, attrs in reader:
        yield key, json.loads(attrs)


class AddressFormatter(object):
    ''' Approximate Python port of lokku's Geo::Address::Formatter '''
    MINIMAL_COMPONENT_KEYS = [
        ('road', 'house_number'),
        ('road', 'house'),
        ('road', 'postcode')
    ]

    splitter = ' | '

    aliases = OrderedDict([
        ('name', 'house'),
        ('addr:housename', 'house'),
        ('addr:housenumber', 'house_number'),
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

    def render_template(self, template, **components):
        def render_first(text):
            text = pystache.render(text, **components)
            splits = (e.strip() for e in text.split('||'))
            selected = next(ifilter(bool, splits), '')
            return selected
        output = pystache.render(template, first=render_first,
                                 **components).strip()
        output = re.sub('[\r\n]+[\s\r\n]*', self.splitter, output)

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
            components = {k: u' '.join([u'{}/{}'.format(t, k.replace(' ', '_'))
                                        for c, t in tokenize(v)])
                          for k, v in components.iteritems()}
        else:
            components = {k: u' '.join([t for c, t in tokenize(v)])
                          for k, v in components.iteritems()}

        text = self.render_template(template_text, **components)

        text = self.post_replacements(template, text)
        return text


def normalize_osm_name_tag(tag, script=False):
    norm = tag.rsplit(':', 1)[-1]
    if not script:
        return norm
    return norm.split('_', 1)[0]


WAYS_LANGUAGE_DATA_FILENAME = 'streets_by_language.tsv'

beginning_re = re.compile('^[^0-9\-]+', re.UNICODE)
end_re = re.compile('[^0-9]+$', re.UNICODE)

latitude_dms_regex = re.compile(ur'^(-?[0-9]{1,2})[ ]*[ :°ºd][ ]*([0-5]?[0-9])?[ ]*[:\'\u2032m]?[ ]*([0-5]?[0-9](?:\.\d+)?)?[ ]*[:\?\"\u2033s]?[ ]*(N|n|S|s)?$', re.I | re.UNICODE)
longitude_dms_regex = re.compile(ur'^(-?1[0-8][0-9]|0?[0-9]{1,2})[ ]*[ :°ºd][ ]*([0-5]?[0-9])?[ ]*[:\'\u2032m]?[ ]*([0-5]?[0-9](?:\.\d+)?)?[ ]*[:\?\"\u2033s]?[ ]*(E|e|W|w)?$', re.I | re.UNICODE)

latitude_decimal_with_direction_regex = re.compile('^(-?[0-9][0-9](?:\.[0-9]+))[ ]*[ :°ºd]?[ ]*(N|n|S|s)$', re.I)
longitude_decimal_with_direction_regex = re.compile('^(-?1[0-8][0-9]|0?[0-9][0-9](?:\.[0-9]+))[ ]*[ :°ºd]?[ ]*(E|e|W|w)$', re.I)


def latlon_to_floats(latitude, longitude):
    have_lat = False
    have_lon = False

    latitude = safe_decode(latitude).strip(u' ,;|')
    longitude = safe_decode(longitude).strip(u' ,;|')

    latitude = latitude.replace(u',', u'.')
    longitude = longitude.replace(u',', u'.')

    lat_dms = latitude_dms_regex.match(latitude)
    lat_dir = latitude_decimal_with_direction_regex.match(latitude)

    if lat_dms:
        d, m, s, c = lat_dms.groups()
        sign = direction_sign(c)
        latitude = degrees_to_decimal(d or 0, m or 0, s or 0)
        have_lat = True
    elif lat_dir:
        d, c = lat_dir.groups()
        sign = direction_sign(c)
        latitude = float(d) * sign
        have_lat = True
    else:
        latitude = re.sub(beginning_re, u'', latitude)
        latitude = re.sub(end_re, u'', latitude)

    lon_dms = longitude_dms_regex.match(longitude)
    lon_dir = longitude_decimal_with_direction_regex.match(longitude)

    if lon_dms:
        d, m, s, c = lon_dms.groups()
        sign = direction_sign(c)
        longitude = degrees_to_decimal(d or 0, m or 0, s or 0)
        have_lon = True
    elif lon_dir:
        d, c = lon_dir.groups()
        sign = direction_sign(c)
        longitude = float(d) * sign
        have_lon = True
    else:
        longitude = re.sub(beginning_re, u'', longitude)
        longitude = re.sub(end_re, u'', longitude)

    return float(latitude), float(longitude)


UNKNOWN_LANGUAGE = 'unk'
AMBIGUOUS_LANGUAGE = 'xxx'


def disambiguate_language(text, languages):
    valid_languages = OrderedDict([(l['lang'], l['default']) for l in languages])
    tokens = tokenize(safe_decode(text).replace(u'-', u' ').lower())

    current_lang = None

    for c, t, data in street_types_gazetteer.filter(tokens):
        if c == token_types.PHRASE:
            valid = [lang for lang in data if lang in valid_languages]
            if len(valid) != 1:
                continue

            phrase_lang = valid[0]
            if phrase_lang != current_lang and current_lang is not None:
                return AMBIGUOUS_LANGUAGE
            current_lang = phrase_lang

    if current_lang is not None:
        return current_lang
    return UNKNOWN_LANGUAGE


def country_and_languages(language_rtree, latitude, longitude):
    props = language_rtree.point_in_poly(latitude, longitude, return_all=True)
    if not props:
        return None, None, None

    country = props[0]['qs_iso_cc'].lower()
    languages = []

    for p in props:
        languages.extend(p['languages'])

    # Python's builtin sort is stable, so if there are two defaults, the first remains first
    # Since polygons are returned from the index ordered from smallest admin level to largest,
    # it means the default language of the region overrides the country default
    default_languages = sorted(languages, key=operator.itemgetter('default'), reverse=True)
    return country, default_languages, props


WELL_REPRESENTED_LANGUAGES = set(['en', 'fr', 'it', 'de', 'nl', 'es'])


def get_language_names(language_rtree, key, value, tag_prefix='name'):
    if not ('lat' in value and 'lon' in value):
        return None, None

    has_colon = ':' in tag_prefix
    tag_first_component = tag_prefix.split(':')[0]
    tag_last_component = tag_prefix.split(':')[-1]

    try:
        latitude, longitude = latlon_to_floats(value['lat'], value['lon'])
    except Exception:
        return None, None

    country, candidate_languages, language_props = country_and_languages(language_rtree, latitude, longitude)
    if not (country and candidate_languages):
        return None, None

    num_langs = len(candidate_languages)
    default_langs = set([l['lang'] for l in candidate_languages if l.get('default')])
    num_defaults = len(default_langs)
    name_language = defaultdict(list)
    has_alternate_names = any((k.startswith(tag_prefix + ':') and normalize_osm_name_tag(k, script=True)
                               in languages for k, v in value.iteritems()))

    regional_defaults = 0
    country_defaults = 0
    regional_langs = set()
    country_langs = set()
    for p in language_props:
        if p['admin_level'] > 0:
            regional_defaults += sum((1 for lang in p['languages'] if lang.get('default')))
            regional_langs |= set([l['lang'] for l in p['languages']])
        else:
            country_defaults += sum((1 for lang in p['languages'] if lang.get('default')))
            country_langs |= set([l['lang'] for l in p['languages']])

    for k, v in value.iteritems():
        if k.startswith(tag_prefix + ':'):
            norm = normalize_osm_name_tag(k)
            norm_sans_script = normalize_osm_name_tag(k, script=True)
            if norm in languages or norm_sans_script in languages:
                name_language[norm].append(v)
        elif not has_alternate_names and k.startswith(tag_first_component) and (has_colon or ':' not in k) and normalize_osm_name_tag(k, script=True) == tag_last_component:
            if num_langs == 1:
                name_language[candidate_languages[0]['lang']].append(v)
            else:
                lang = disambiguate_language(v, candidate_languages)
                default_lang = candidate_languages[0]['lang']

                if lang == AMBIGUOUS_LANGUAGE:
                    return None, None
                elif lang == UNKNOWN_LANGUAGE and num_defaults == 1:
                    name_language[default_lang].append(v)
                elif lang != UNKNOWN_LANGUAGE:
                    if lang != default_lang and lang in country_langs and country_defaults > 1 and regional_defaults > 0 and lang in WELL_REPRESENTED_LANGUAGES:
                        return None, None
                    name_language[lang].append(v)
                else:
                    return None, None

    return country, name_language

newline_regex = re.compile('\r\n|\r|\n')


def tsv_string(s):
    return safe_encode(newline_regex.sub(u', ', safe_decode(s).strip()).replace(u'\t', u' '))


def build_ways_training_data(language_rtree, infile, out_dir):
    i = 0
    f = open(os.path.join(out_dir, WAYS_LANGUAGE_DATA_FILENAME), 'w')
    writer = csv.writer(f, 'tsv_no_quote')

    for key, value in parse_osm(infile, allowed_types=WAYS_RELATIONS):
        country, name_language = get_language_names(language_rtree, key, value, tag_prefix='name')
        if not name_language:
            continue

        for k, v in name_language.iteritems():
            for s in v:
                if k in languages:
                    writer.writerow((k, country, tsv_string(s)))
            if i % 1000 == 0 and i > 0:
                print 'did', i, 'ways'
            i += 1
    f.close()


ADDRESS_LANGUAGE_DATA_FILENAME = 'address_streets_by_language.tsv'
ADDRESS_FORMAT_DATA_TAGGED_FILENAME = 'formatted_addresses_tagged.tsv'
ADDRESS_FORMAT_DATA_FILENAME = 'formatted_addresses.tsv'


def build_address_format_training_data(language_rtree, infile, out_dir):
    i = 0

    formatter = AddressFormatter()

    formatted_file = open(os.path.join(out_dir, ADDRESS_FORMAT_DATA_FILENAME), 'w')
    formatted_writer = csv.writer(formatted_file, 'tsv_no_quote')

    formatted_tagged_file = open(os.path.join(out_dir, ADDRESS_FORMAT_DATA_TAGGED_FILENAME), 'w')
    formatted_tagged_writer = csv.writer(formatted_tagged_file, 'tsv_no_quote')

    for key, value in parse_osm(infile):
        try:
            latitude, longitude = latlon_to_floats(value['lat'], value['lon'])
        except Exception:
            continue

        country, default_languages, language_props = country_and_languages(language_rtree, latitude, longitude)
        if not (country and default_languages):
            continue

        formatted_address_tagged = formatter.format_address(country, value)
        formatted_address_untagged = formatter.format_address(country, value, tag_components=False)
        if formatted_address_tagged is not None:
            formatted_address_tagged = tsv_string(formatted_address_tagged)
            formatted_tagged_writer.writerow((default_languages[0]['lang'], country, formatted_address_tagged))

        if formatted_address_untagged is not None:
            formatted_address_untagged = tsv_string(formatted_address_untagged)
            formatted_writer.writerow((default_languages[0]['lang'], country, formatted_address_untagged))

        if formatted_address_tagged is not None or formatted_address_untagged is not None:
            i += 1
            if i % 1000 == 0 and i > 0:
                print 'did', i, 'formatted addresses'


ADDRESS_FORMAT_DATA_LANGUAGE_FILENAME = 'formatted_addresses_by_language.tsv'
NAME_KEYS = (
    'name',
    'addr:housename',
)
COUNTRY_KEYS = (
    'country',
    'country_name',
    'addr:country',
)


def build_address_format_training_data_limited(language_rtree, infile, out_dir):
    i = 0

    formatter = AddressFormatter()

    f = open(os.path.join(out_dir, ADDRESS_FORMAT_DATA_LANGUAGE_FILENAME), 'w')
    writer = csv.writer(f, 'tsv_no_quote')

    for key, value in parse_osm(infile):
        try:
            latitude, longitude = latlon_to_floats(value['lat'], value['lon'])
        except Exception:
            continue

        for k in NAME_KEYS + COUNTRY_KEYS:
            _ = value.pop(k, None)

        if not value:
            continue

        country, name_language = get_language_names(language_rtree, key, value, tag_prefix='addr:street')
        if not name_language:
            continue

        formatted_address_untagged = formatter.format_address(country, value, tag_components=False)
        if formatted_address_untagged is not None:
            formatted_address_untagged = tsv_string(formatted_address_untagged)

            for k, v in name_language.iteritems():
                for s in v:
                    if k in languages:
                        writer.writerow((k, country, formatted_address_untagged))

            i += 1
            if i % 1000 == 0 and i > 0:
                print 'did', i, 'formatted addresses'


def build_address_training_data(langauge_rtree, infile, out_dir, format=False):
    i = 0
    f = open(os.path.join(out_dir, ADDRESS_LANGUAGE_DATA_FILENAME), 'w')
    writer = csv.writer(f, 'tsv_no_quote')

    for key, value in parse_osm(infile):
        country, street_language = get_language_names(language_rtree, key, value, tag_prefix='addr:street')
        if not street_language:
            continue

        for k, v in street_language.iteritems():
            for s in v:
                s = s.strip()
                if not s:
                    continue
                if k in languages:
                    writer.writerow((k, country, tsv_string(s)))
            if i % 1000 == 0 and i > 0:
                print 'did', i, 'streets'
            i += 1

    f.close()

VENUE_LANGUAGE_DATA_FILENAME = 'names_by_language.tsv'


def build_venue_training_data(language_rtree, infile, out_dir):
    i = 0

    f = open(os.path.join(out_dir, VENUE_LANGUAGE_DATA_FILENAME), 'w')
    writer = csv.writer(f, 'tsv_no_quote')

    for key, value in parse_osm(infile):
        country, name_language = get_language_names(language_rtree, key, value, tag_prefix='name')
        if not name_language:
            continue

        venue_type = None
        for key in (u'amenity', u'building'):
            amenity = value.get(key, u'').strip()
            if amenity in ('yes', 'y'):
                continue

            if amenity:
                venue_type = u':'.join([key, amenity])
                break

        if venue_type is None:
            continue

        for k, v in name_language.iteritems():
            for s in v:
                s = s.strip()
                if k in languages:
                    writer.writerow((k, country, safe_encode(venue_type), tsv_string(s)))
            if i % 1000 == 0 and i > 0:
                print 'did', i, 'venues'
            i += 1

    f.close()

if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-s', '--streets-file',
                        help='Path to planet-ways.osm')

    parser.add_argument('-a', '--address-file',
                        help='Path to planet-addresses.osm')

    parser.add_argument('-v', '--venues-file',
                        help='Path to planet-venues.osm')

    parser.add_argument('-f', '--format-only',
                        action='store_true',
                        default=False,
                        help='Save formatted addresses (slow)')

    parser.add_argument('-l', '--limited-addresses',
                        action='store_true',
                        default=False,
                        help='Save formatted addresses without house names or country (slow)')

    parser.add_argument('-t', '--temp-dir',
                        default=tempfile.gettempdir(),
                        help='Temp directory to use')

    parser.add_argument('-r', '--rtree-dir',
                        required=True,
                        help='Language RTree directory')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    args = parser.parse_args()

    init_languages()

    language_rtree = LanguagePolygonIndex.load(args.rtree_dir)

    street_types_gazetteer.configure()

    # Can parallelize
    if args.streets_file:
        build_ways_training_data(language_rtree, args.streets_file, args.out_dir)
    if args.address_file and not args.format_only and not args.limited_addresses:
        build_address_training_data(language_rtree, args.address_file, args.out_dir)
    if args.address_file and args.format_only:
        build_address_format_training_data(language_rtree, args.address_file, args.out_dir)
    if args.address_file and args.limited_addresses:
        build_address_format_training_data_limited(language_rtree, args.address_file, args.out_dir)
    if args.venues_file:
        build_venue_training_data(language_rtree, args.venues_file, args.out_dir)
