# -*- coding: utf-8 -*-
import argparse
import logging
import operator
import os
import re
import six
import subprocess
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.coordinates.conversion import latlon_to_decimal
from geodata.encoding import safe_decode
from geodata.file_utils import ensure_dir, download_file
from geodata.i18n.unicode_properties import get_chars_by_script
from geodata.i18n.word_breaks import ideographic_scripts
from geodata.names.deduping import NameDeduper
from geodata.osm.definitions import osm_definitions
from geodata.osm.extract import parse_osm, osm_type_and_id, NODE, WAY, RELATION, OSM_NAME_TAGS
from geodata.polygons.index import *
from geodata.polygons.reverse_geocode import QuattroshapesReverseGeocoder
from geodata.statistics.tf_idf import IDFIndex


class NeighborhoodDeduper(NameDeduper):
    # Lossless conversions only
    replacements = {
        u'saint': u'st',
        u'and': u'&',
        u'〇': u'0',
        u'一': u'1',
        u'二': u'2',
        u'三': u'3',
        u'四': u'4',
        u'五': u'5',
        u'六': u'6',
        u'七': u'7',
        u'八': u'8',
        u'九': u'9',
        u'十': u'10',
    }

    discriminative_words = set([
        # Han numbers
        u'〇', u'一',
        u'二', u'三',
        u'四', u'五',
        u'六', u'七',
        u'八', u'九',
        u'十', u'百',
        u'千', u'万',
        u'億', u'兆',
        u'京', u'第',

        # Roman numerals
        u'i', u'ii',
        u'iii', u'iv',
        u'v', u'vi',
        u'vii', u'viii',
        u'ix', u'x',
        u'xi', u'xii',
        u'xiii', u'xiv',
        u'xv', u'xvi',
        u'xvii', u'xviii',
        u'xix', u'xx',

        # English directionals
        u'north', u'south',
        u'east', u'west',
        u'northeast', u'northwest',
        u'southeast', u'southwest',

        # Spanish, Portguese and Italian directionals
        u'norte', u'nord', u'sur', u'sul', u'sud',
        u'est', u'este', u'leste', u'oeste', u'ovest',

        # New in various languages
        u'new',
        u'nova',
        u'novo',
        u'nuevo',
        u'nueva',
        u'nuovo',
        u'nuova',

        # Qualifiers
        u'heights',
        u'hills',

        u'upper', u'lower',
        u'little', u'great',

        u'park',
        u'parque',

        u'village',

    ])

    stopwords = set([
        u'cp',
        u'de',
        u'la',
        u'urbanizacion',
        u'do',
        u'da',
        u'dos',
        u'del',
        u'community',
        u'bairro',
        u'barrio',
        u'le',
        u'el',
        u'mah',
        u'раион',
        u'vila',
        u'villa',
        u'kampung',
        u'ahupua`a',

    ])


class ZetashapesReverseGeocoder(GeohashPolygonIndex):
    simplify_tolerance = 0.00001
    preserve_topology = True
    persistent_polygons = False
    cache_size = 0

    SCRATCH_DIR = '/tmp'

    # Contains accurate boundaries for neighborhoods sans weird GeoPlanet names like "Adelphi" or "Crown Heights South"
    NEIGHBORHOODS_REPO = 'https://github.com/blackmad/neighborhoods'

    @classmethod
    def clone_repo(cls, path):
        subprocess.check_call(['rm', '-rf', path])
        subprocess.check_call(['git', 'clone', cls.NEIGHBORHOODS_REPO, path])

    @classmethod
    def create_neighborhoods_index(cls):
        scratch_dir = cls.SCRATCH_DIR
        repo_path = os.path.join(scratch_dir, 'neighborhoods')
        cls.clone_repo(repo_path)

        neighborhoods_dir = os.path.join(scratch_dir, 'neighborhoods', 'index')
        ensure_dir(neighborhoods_dir)

        index = cls(save_dir=neighborhoods_dir)

        have_geonames = set()
        is_neighborhood = set()

        for filename in os.listdir(repo_path):
            path = os.path.join(repo_path, filename)
            base_name = filename.split('.')[0].split('gn-')[-1]
            if filename.endswith('.geojson') and filename.startswith('gn-'):
                have_geonames.add(base_name)
            elif filename.endswith('metadata.json'):
                data = json.load(open(os.path.join(repo_path, filename)))
                if data.get('neighborhoodNoun', [None])[0] in (None, 'rione'):
                    is_neighborhood.add(base_name)

        for filename in os.listdir(repo_path):
            if not filename.endswith('.geojson'):
                continue
            base_name = filename.rsplit('.geojson')[0]
            if base_name in have_geonames:
                f = open(os.path.join(repo_path, 'gn-{}'.format(filename)))
            elif base_name in is_neighborhood:
                f = open(os.path.join(repo_path, filename))
            else:
                continue
            try:
                index.add_geojson_like_file(json.load(f)['features'])
            except ValueError:
                continue

        return index


class NeighborhoodReverseGeocoder(RTreePolygonIndex):
    '''
    Neighborhoods are very important in cities like NYC, SF, Chicago, London
    and many others. We want the address parser to be trained with addresses
    that sufficiently capture variations in address patterns, including
    neighborhoods. Quattroshapes neighborhood data (in the US at least)
    is not great in terms of names, mostly becasue GeoPlanet has so many
    incorrect names. The neighborhoods project, also known as Zetashapes
    has very accurate polygons with correct names, but only for a handful
    of cities. OSM usually lists neighborhoods and some other local admin
    areas like boroughs as points rather than polygons.

    This index merges all of the above data sets in prioritized order
    (Zetashapes > OSM > Quattroshapes) to provide unified point-in-polygon
    tests for neighborhoods. The properties vary by source but each has
    source has least a "name" key which in practice is what we care about.
    '''

    SCRATCH_DIR = '/tmp'

    PRIORITIES_FILENAME = 'priorities.json'

    DUPE_THRESHOLD = 0.9

    persistent_polygons = True
    cache_size = 100000

    source_priorities = {
        'zetashapes': 0,     # Best names/polygons
        'osm_zeta': 1,       # OSM names matched with Zetashapes polygon
        'osm_quattro': 2,    # OSM names matched with Quattroshapes polygon
        'quattroshapes': 3,  # Good results in some countries/areas
    }

    level_priorities = {
        'neighborhood': 0,
        'local_admin': 1,
    }

    regex_replacements = [
        # Paris arrondissements, listed like "PARIS-1ER-ARRONDISSEMENT" in Quqttroshapes
        (re.compile('^paris-(?=[\d])', re.I), ''),
    ]

    @classmethod
    def count_words(cls, s):
        doc = defaultdict(int)
        for t, c in NeighborhoodDeduper.content_tokens(s):
            doc[t] += 1
        return doc

    @classmethod
    def create_from_osm_and_quattroshapes(cls, filename, quattroshapes_dir, output_dir, scratch_dir=SCRATCH_DIR):
        '''
        Given an OSM file (planet or some other bounds) containing neighborhoods
        as points (some suburbs have boundaries)

        and their dependencies, create an R-tree index for coarse-grained
        reverse geocoding.

        Note: the input file is expected to have been created using
        osmfilter. Use fetch_osm_address_data.sh for planet or copy the
        admin borders commands if using other geometries.
        '''
        index = cls(save_dir=output_dir)

        ensure_dir(scratch_dir)

        logger = logging.getLogger('neighborhoods')

        qs_scratch_dir = os.path.join(scratch_dir, 'qs_neighborhoods')
        ensure_dir(qs_scratch_dir)
        logger.info('Creating Quattroshapes neighborhoods')

        qs = QuattroshapesNeighborhoodsReverseGeocoder.create_neighborhoods_index(quattroshapes_dir, qs_scratch_dir)
        logger.info('Creating Zetashapes neighborhoods')
        zs = ZetashapesReverseGeocoder.create_neighborhoods_index()

        logger.info('Creating IDF index')
        idf = IDFIndex()

        char_scripts = get_chars_by_script()

        for idx in (zs, qs):
            for i in xrange(idx.i):
                props = idx.get_properties(i)
                name = props.get('name')
                if name is not None:
                    doc = cls.count_words(name)
                    idf.update(doc)

        for key, attrs, deps in parse_osm(filename):
            for k, v in six.iteritems(attrs):
                if any((k.startswith(name_key) for name_key in OSM_NAME_TAGS)):
                    doc = cls.count_words(v)
                    idf.update(doc)

        qs.matched = [False] * qs.i
        zs.matched = [False] * zs.i

        logger.info('Matching OSM points to neighborhood polygons')
        # Parse OSM and match neighborhood/suburb points to Quattroshapes/Zetashapes polygons
        num_polys = 0
        for element_id, attrs, deps in parse_osm(filename):
            try:
                lat, lon = latlon_to_decimal(attrs['lat'], attrs['lon'])
            except ValueError:
                continue

            osm_name = attrs.get('name')
            if not osm_name:
                continue

            id_type, element_id = element_id.split(':')
            element_id = long(element_id)

            props['type'] = id_type
            props['id'] = element_id

            is_neighborhood = osm_definitions.meets_definition(attrs, osm_defintiions.NEIGHBORHOOD)

            ranks = []
            osm_names = []

            for key in OSM_NAME_TAGS:
                name = attrs.get(key)
                if name:
                    osm_names.append(name)

            for name_key in OSM_NAME_TAGS:
                osm_names.extend([v for k, v in six.iteritems(attrs) if k.startswith('{}:'.format(name_key))])

            for idx in (zs, qs):
                candidates = idx.get_candidate_polygons(lat, lon, return_all=True)

                if candidates:
                    max_sim = 0.0
                    arg_max = None

                    normalized_qs_names = {}

                    for osm_name in osm_names:

                        contains_ideographs = any(((char_scripts[ord(c)] or '').lower() in ideographic_scripts
                                                   for c in safe_decode(osm_name)))

                        for i in candidates:
                            props = idx.get_properties(i)
                            name = normalized_qs_names.get(i)
                            if not name:
                                name = props.get('name')
                                if not name:
                                    continue
                                for pattern, repl in cls.regex_replacements:
                                    name = pattern.sub(repl, name)
                                normalized_qs_names[i] = name

                            if is_neighborhood and idx is qs and props.get(QuattroshapesReverseGeocoder.LEVEL) != 'neighborhood':
                                continue

                            if not contains_ideographs:
                                sim = NeighborhoodDeduper.compare(osm_name, name, idf)
                            else:
                                # Many Han/Hangul characters are common, shouldn't use IDF
                                sim = NeighborhoodDeduper.compare_ideographs(osm_name, name)

                            if sim > max_sim:
                                max_sim = sim
                                poly = idx.get_polygon(i)
                                arg_max = (max_sim, props, poly.context, idx, i)

                    if arg_max:
                        ranks.append(arg_max)

            ranks.sort(key=operator.itemgetter(0), reverse=True)
            if ranks and ranks[0][0] >= cls.DUPE_THRESHOLD:
                score, props, poly, idx, i = ranks[0]

                if idx is zs:
                    attrs['polygon_type'] = 'neighborhood'
                    source = 'osm_zeta'
                else:
                    level = props.get(QuattroshapesReverseGeocoder.LEVEL, None)
                    source = 'osm_quattro'
                    if level == 'neighborhood':
                        attrs['polygon_type'] = 'neighborhood'
                    else:
                        attrs['polygon_type'] = 'local_admin'



                attrs['source'] = source
                index.index_polygon(poly)
                index.add_polygon(poly, attrs)
                idx.matched[i] = True

            num_polys += 1
            if num_polys % 1000 == 0 and num_polys > 0:
                logger.info('did {} neighborhoods'.format(num_polys))

        for idx, source in ((zs, 'zetashapes'), (qs, 'quattroshapes')):
            for i in xrange(idx.i):
                props = idx.get_properties(i)
                poly = idx.get_polygon(i)
                if idx.matched[i]:
                    continue
                props['source'] = source
                if idx is zs or props.get(QuattroshapesReverseGeocoder.LEVEL, None) == 'neighborhood':
                    props['polygon_type'] = 'neighborhood'
                else:
                    # We don't actually care about local admin polygons unless they match OSM
                    continue
                index.index_polygon(poly.context)
                index.add_polygon(poly.context, props)

        return index

    def setup(self):
        self.priorities = []

    def index_polygon_properties(self, properties):
        self.priorities.append((self.level_priorities[properties['polygon_type']], self.source_priorities[properties['source']]))

    def load_polygon_properties(self, d):
        self.priorities = json.load(open(os.path.join(d, self.PRIORITIES_FILENAME)))

    def save_polygon_properties(self, d):
        json.dump(self.priorities, open(os.path.join(d, self.PRIORITIES_FILENAME), 'w'))

    def priority(self, i):
        return self.priorities[i]

    def get_candidate_polygons(self, lat, lon):
        candidates = super(NeighborhoodReverseGeocoder, self).get_candidate_polygons(lat, lon)
        return sorted(candidates, key=self.priority)


class QuattroshapesNeighborhoodsReverseGeocoder(GeohashPolygonIndex, QuattroshapesReverseGeocoder):
    persistent_polygons = False
    cache_size = None

    @classmethod
    def create_neighborhoods_index(cls, quattroshapes_dir,
                                   output_dir,
                                   index_filename=None,
                                   polys_filename=DEFAULT_POLYS_FILENAME):
        local_admin_filename = os.path.join(quattroshapes_dir, cls.LOCAL_ADMIN_FILENAME)
        neighborhoods_filename = os.path.join(quattroshapes_dir, cls.NEIGHBORHOODS_FILENAME)
        return cls.create_from_shapefiles([local_admin_filename, neighborhoods_filename],
                                          output_dir, index_filename=index_filename,
                                          polys_filename=polys_filename)


if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-q', '--quattroshapes-dir',
                        help='Path to quattroshapes dir')

    parser.add_argument('-n', '--osm-neighborhoods-file',
                        help='Path to OSM neighborhoods file (no dependencies, .osm format)')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    logging.basicConfig(level=logging.INFO)

    args = parser.parse_args()
    if args.osm_neighborhoods_file and args.quattroshapes_dir:
        index = NeighborhoodReverseGeocoder.create_from_osm_and_quattroshapes(
            args.osm_neighborhoods_file,
            args.quattroshapes_dir,
            args.out_dir
        )
    else:
        parser.error('Must specify quattroshapes dir or osm admin borders file')

    index.save()
