# -*- coding: utf-8 -*-
import argparse
import csv
import os
import pystache
import re
import subprocess
import sys
import tempfile
import ujson as json
import yaml

from collections import defaultdict
from lxml import etree
from itertools import ifilter

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir, os.pardir, 'python')))

from address_normalizer.text.tokenize import *
from geodata.i18n.languages import *
from geodata.polygons.language_polys import *

from geodata.file_utils import *

this_dir = os.path.realpath(os.path.dirname(__file__))

FORMATTER_GIT_REPO = 'https://github.com/OpenCageData/address-formatting'

WAY_OFFSET = 10 ** 15
RELATION_OFFSET = 10 ** 15

PLANET_ADDRESSES_INPUT_FILE = 'planet-addresses.osm'
PLANET_ADDRESSES_OUTPUT_FILE = 'planet-addresses.tsv'

PLANET_WAYS_INPUT_FILE = 'planet-ways.osm'
PLANET_WAYS_OUTPUT_FILE = 'planet-ways.tsv'

PLANET_VENUES_INPUT_FILE = 'planet-venues.osm'
PLANET_VENUES_OUTPUT_FILE = 'planet-venues.tsv'

ALL_OSM_TAGS = set(['node', 'way', 'relation'])
ONLY_WAYS = set(['way'])


# Currently, all our data sets are converted to nodes with osmconvert before parsing
def parse_osm(filename, allowed_types=ALL_OSM_TAGS):
    f = open(filename)
    parser = etree.iterparse(f)

    single_type = len(allowed_types) == 1

    for (_, elem) in parser:
        elem_id = long(elem.attrib.pop('id', 0))
        item_type = elem.tag
        if elem_id >= WAY_OFFSET:
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
    writer = csv.writer(out, delimiter='\t')
    for key, attrs in parse_osm(filename):
        writer.writerow((key, json.dumps(attrs)))
    out.close()


def read_osm_json(filename):
    reader = csv.reader(open(filename), delimiter='\t')
    for key, attrs in reader:
        yield key, json.loads(attrs)


class AddressFormatter(object):
    ''' Approximate Python port of lokku's Geo::Address::Formatter '''
    MINIMAL_COMPONENT_KEYS = ('road', 'postcode')

    splitter = ', '

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

    def __init__(self, scratch_dir='/tmp', splitter=', '):
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
        return all((c in components for c in self.MINIMAL_COMPONENT_KEYS))

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

    def format_address(self, country, components, minimal_only=False, tag_components=True):
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


def country_and_languages(language_rtree, latitude, longitude):
    props = language_rtree.point_in_poly(latitude, longitude)
    if not props or not props.get('languages'):
        return None, None

    country = props['qs_iso_cc'].lower()
    default_languages = props['languages']
    return country, default_languages


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

    country, default_languages = country_and_languages(language_rtree, latitude, longitude)
    if not (country and default_languages):
        return None, None

    one_default = len(default_languages) == 1
    name_language = defaultdict(list)
    has_alternate_names = any((k.startswith(tag_prefix + ':') and normalize_osm_name_tag(k, script=True)
                               in languages for k, v in value.iteritems()))
    for k, v in value.iteritems():
        if k.startswith(tag_prefix + ':'):
            norm = normalize_osm_name_tag(k)
            norm_sans_script = normalize_osm_name_tag(k, script=True)
            if norm in languages or norm_sans_script in languages:
                name_language[norm].append(v)
        elif not has_alternate_names and k.startswith(tag_first_component) and (has_colon or ':' not in k) and normalize_osm_name_tag(k, script=True) == tag_last_component:
            name_language[default_languages[0]['lang']].append(v)

    return country, name_language


def build_ways_training_data(language_rtree, infile, out_dir):
    i = 0
    f = open(os.path.join(out_dir, WAYS_LANGUAGE_DATA_FILENAME), 'w')
    writer = csv.writer(f, delimiter='\t')

    for key, value in parse_osm(infile, allowed_types=ONLY_WAYS):
        country, name_language = get_language_names(language_rtree, key, value, tag_prefix='name')
        if not name_language:
            continue

        for k, v in name_language.iteritems():
            for s in v:
                if k in languages:
                    writer.writerow((k, country, s.encode('utf-8')))
            if i % 1000 == 0 and i > 0:
                print 'did', i, 'ways'
            i += 1
    f.close()


ADDRESS_LANGUAGE_DATA_FILENAME = 'address_streets_by_language.tsv'
ADDRESS_FORMAT_DATA_TAGGED_FILENAME = 'formatted_addresses_tagged.tsv'
ADDRESS_FORMAT_DATA_FILENAME = 'formatted_addresses.tsv'


def build_address_format_training_data(language_rtree, infile, out_dir):
    i = 0

    formatter = AddressFormatter(splitter='\n')

    formatted_file = open(os.path.join(out_dir, ADDRESS_FORMAT_DATA_FILENAME), 'w')
    formatted_writer = csv.writer(formatted_file, delimiter='\t')

    formatted_tagged_file = open(os.path.join(out_dir, ADDRESS_FORMAT_DATA_TAGGED_FILENAME), 'w')
    formatted_tagged_writer = csv.writer(formatted_tagged_file, delimiter='\t')

    for key, value in parse_osm(infile):
        country, default_languages = country_and_languages(language_rtree, float(value['lat']), float(value['lon']))
        if not (country and default_languages):
            continue

        formatted_address_tagged = formatter.format_address(country, value)
        formatted_address_untagged = formatter.format_address(country, value, tag_components=False)
        if formatted_address_tagged is not None:
            formatted_address_tagged = safe_encode(formatted_address_tagged.replace('\n', '\\n'))
            formatted_tagged_writer.writerow((country, default_languages[0]['lang'], formatted_address_tagged))

        if formatted_address_untagged is not None:
            formatted_address_untagged = safe_encode(formatted_address_untagged.replace('\n', '\\n'))
            formatted_writer.writerow((country, default_languages[0]['lang'], formatted_address_untagged))

        if formatted_address_tagged is not None or formatted_address_untagged is not None:
            i += 1
            if i % 1000 == 0 and i > 0:
                print 'did', i, 'formatted addresses'


def build_address_training_data(langauge_rtree, infile, out_dir, format=False):
    i = 0
    f = open(os.path.join(out_dir, ADDRESS_LANGUAGE_DATA_FILENAME), 'w')
    writer = csv.writer(f, delimiter='\t')

    for key, value in parse_osm(infile):
        country, street_language = get_language_names(language_rtree, key, value, tag_prefix='addr:street')
        if not street_language:
            continue

        for k, v in street_language.iteritems():
            for s in v:
                if k in languages:
                    writer.writerow((k, country, safe_encode(s)))
            if i % 1000 == 0 and i > 0:
                print 'did', i, 'streets'
            i += 1

    f.close()

VENUE_LANGUAGE_DATA_FILENAME = 'names_by_language.tsv'


def build_venue_training_data(language_rtree, infile, out_dir):
    i = 0

    f = open(os.path.join(out_dir, VENUE_LANGUAGE_DATA_FILENAME), 'w')
    writer = csv.writer(f, delimiter='\t')

    for key, value in parse_osm(infile):
        country, name_language = get_language_names(language_rtree, key, value, tag_prefix='name')
        if not name_language:
            continue

        venue_type = value.get('amenity', u'').strip()
        if not venue_type.strip():
            continue
        for k, v in name_language.iteritems():
            for s in v:
                if k in languages:
                    writer.writerow((k, country, safe_encode(venue_type), safe_encode(s)))
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

    # Can parallelize
    if args.streets_file:
        build_ways_training_data(language_rtree, args.streets_file, args.out_dir)
    if args.address_file and not args.format_only:
        build_address_training_data(language_rtree, args.address_file, args.out_dir)
    if args.address_file and args.format_only:
        build_address_format_training_data(language_rtree, args.address_file, args.out_dir)
    if args.venues_file:
        build_venue_training_data(language_rtree, args.venues_file, args.out_dir)
