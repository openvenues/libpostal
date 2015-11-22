import argparse
import os
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.polygons.index import *
from geodata.i18n.languages import *

country_language_dir = os.path.join(LANGUAGES_DIR, 'countries')
regional_language_dir = os.path.join(LANGUAGES_DIR, 'regional')


class LanguagePolygonIndex(RTreePolygonIndex):

    include_only_properties = set([
        'qs_a0',
        'qs_iso_cc',
        'qs_a1',
        'qs_a1_lc',
        'qs_a1r',
        'qs_a1r_lc',
        'qs_level',
        'languages',
        'admin_level'
    ])

    @classmethod
    def create_from_shapefiles(cls,
                               admin0_shapefile,
                               admin1_shapefile,
                               admin1_region_file,
                               output_dir,
                               index_filename=None,
                               polys_filename=DEFAULT_POLYS_FILENAME):

        init_languages()
        index = cls(save_dir=output_dir, index_filename=index_filename)

        i = 0

        '''
        Ordering of the files is important here as we want to match
        the most granular admin polygon first for regional languages. Currently
        most regional languages as they would apply to street signage are regional in
        terms of an admin 1 level (states, provinces, regions)
        '''
        for input_file in (admin0_shapefile, admin1_region_file, admin1_shapefile):
            f = fiona.open(input_file)

            for rec in f:
                if not rec or not rec.get('geometry') or 'type' not in rec['geometry']:
                    continue

                country = rec['properties']['qs_iso_cc'].lower()
                properties = rec['properties']

                admin_level = properties['qs_level']

                level_num = None
                if admin_level == 'adm1':
                    name_key = 'qs_a1'
                    code_key = 'qs_a1_lc'
                    level_num = 1
                elif admin_level == 'adm1_region':
                    name_key = 'qs_a1r'
                    code_key = 'qs_a1r_lc'
                    level_num = 1
                elif admin_level == 'adm0':
                    level_num = 0
                else:
                    continue

                assert level_num is not None

                if admin_level != 'adm0':
                    admin1 = properties.get(name_key)
                    admin1_code = properties.get(code_key)

                    regional = None

                    if name_key:
                        regional = get_regional_languages(country, name_key, admin1)

                    if code_key and not regional:
                        regional = get_regional_languages(country, code_key, admin1_code)

                    if not regional:
                        continue

                    if all((not default for lang, default in regional.iteritems())):
                        languages = get_country_languages(country)
                        languages.update(regional)
                        languages = languages.items()
                    else:
                        languages = regional.items()
                else:
                    languages = get_country_languages(country).items()

                properties['languages'] = [{'lang': lang, 'default': default}
                                           for lang, default in languages]
                properties['admin_level'] = level_num

                poly_type = rec['geometry']['type']
                if poly_type == 'Polygon':
                    poly = Polygon(rec['geometry']['coordinates'][0])
                    index.index_polygon(poly)
                    poly = index.simplify_polygon(poly)
                    index.add_polygon(poly, dict(rec['properties']))
                elif poly_type == 'MultiPolygon':
                    polys = []
                    for coords in rec['geometry']['coordinates']:
                        poly = Polygon(coords[0])
                        polys.append(poly)
                        index.index_polygon(poly)

                    multi_poly = index.simplify_polygon(MultiPolygon(polys))
                    index.add_polygon(multi_poly, dict(rec['properties']))
                else:
                    continue

                i += 1
        return index

    @classmethod
    def create_with_quattroshapes(cls, quattroshapes_dir,
                                  output_dir,
                                  index_filename=None,
                                  polys_filename=DEFAULT_POLYS_FILENAME):
        admin0_filename = os.path.join(quattroshapes_dir, 'qs_adm0.shp')
        admin1_filename = os.path.join(quattroshapes_dir, 'qs_adm1.shp')
        admin1r_filename = os.path.join(quattroshapes_dir, 'qs_adm1_region.shp')

        return cls.create_from_shapefiles(admin0_filename, admin1_filename, admin1r_filename,
                                          output_dir, index_filename=index_filename,
                                          polys_filename=polys_filename)

    def admin_level(self, i):
        props, p = self.polygons[i]
        return props['admin_level']

    def get_candidate_polygons(self, lat, lon):
        candidates = OrderedDict.fromkeys(self.index.intersection((lon, lat, lon, lat))).keys()
        return sorted(candidates, key=self.admin_level, reverse=True)


if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-q', '--quattroshapes-dir',
                        help='Path to quattroshapes dir')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    args = parser.parse_args()
    index = LanguagePolygonIndex.create_with_quattroshapes(args.quattroshapes_dir, args.out_dir)
    index.save()
