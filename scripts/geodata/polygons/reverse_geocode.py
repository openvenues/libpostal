import argparse
import os
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.polygons.index import *
from geodata.encoding import safe_decode


class ReverseGeocoder(RTreePolygonIndex):
    COUNTRIES_FILENAME = 'qs_adm0.shp'
    ADMIN1_FILENAME = 'qs_adm1.shp'
    ADMIN1_REGION_FILENAME = 'qs_adm1_region.shp'
    ADMIN2_FILENAME = 'qs_adm2.shp'
    ADMIN2_REGION_FILENAME = 'qs_adm2_region.shp'
    LOCAL_ADMIN_FILENAME = 'qs_localadmin.shp'
    LOCALITIES_FILENAME = 'qs_localities.shp'
    NEIGHBORHOODS_FILENAME = 'qs_neighborhoods.shp'

    sorted_levels = ('adm0',
                     'adm1',
                     'adm1_region',
                     'adm2',
                     'adm2_region',
                     'localadmin',
                     'locality',
                     'neighborhood',
                     )

    sort_levels = {k: i for i, k in enumerate(sorted_levels)}

    polygon_properties = {
        COUNTRIES_FILENAME: {
            'qs_a0': safe_decode,
            'qs_iso_cc': safe_decode,
            'qs_level': safe_decode,
            'qs_gn_id': int,
            'qs_woe_id': int,
        },
        ADMIN1_FILENAME: {
            'qs_a1': safe_decode,
            'qs_a1_lc': safe_decode,
            'qs_level': safe_decode,
            'qs_gn_id': int,
            'qs_woe_id': int,
        },
        ADMIN1_REGION_FILENAME: {
            'qs_a1r': safe_decode,
            'qs_a1r_lc': safe_decode,
            'qs_level': safe_decode,
            'qs_gn_id': int,
            'qs_woe_id': int,
        },
        ADMIN2_FILENAME: {
            'qs_a2': safe_decode,
            'qs_a2_lc': safe_decode,
            'qs_level': safe_decode,
            'qs_gn_id': int,
            'qs_woe_id': int,
        },
        ADMIN2_REGION_FILENAME: {
            'qs_a2r': safe_decode,
            'qs_a2r_lc': safe_decode,
            'qs_level': safe_decode,
            'qs_gn_id': int,
            'qs_woe_id': int,
        },
        LOCAL_ADMIN_FILENAME: {
            'qs_la': safe_decode,
            'qs_la_lc': safe_decode,
            'qs_level': safe_decode,
            'qs_gn_id': int,
            'qs_woe_id': int,
        },
        LOCALITIES_FILENAME: {
            'qs_loc': safe_decode,
            'qs_loc_alt': safe_decode,
            'qs_level': safe_decode,
            'qs_gn_id': int,
            'qs_woe_id': int,
        },
        NEIGHBORHOODS_FILENAME: {
            'name': safe_decode,
            'name_en': safe_decode,
            'qs_level': safe_decode,
            'woe_id': int,
            'gn_id': int,
        }
    }

    @classmethod
    def create_from_shapefiles(cls,
                               input_files,
                               output_dir,
                               index_filename=DEFAULT_INDEX_FILENAME,
                               polys_filename=DEFAULT_POLYS_FILENAME):

        index = cls(save_dir=output_dir, index_filename=index_filename)

        i = 0

        for input_file in input_files:
            f = fiona.open(input_file)

            filename = os.path.split(input_file)[-1]
            include_props = cls.polygon_properties.get(filename)

            for rec in f:
                if not rec or not rec.get('geometry') or 'type' not in rec['geometry']:
                    continue

                properties = rec['properties']

                if filename == cls.NEIGHBORHOODS_FILENAME:
                    properties['qs_level'] = 'neighborhood'

                if include_props:
                    for k, func in include_props.iteritems():
                        v = properties.get(k, None)
                        if v is not None:
                            properties[k] = func(v)

                poly_type = rec['geometry']['type']
                if poly_type == 'Polygon':
                    poly = Polygon(rec['geometry']['coordinates'][0])
                    index.index_polygon(i, poly)
                    poly = index.simplify_polygon(poly)
                    index.add_polygon(poly, dict(rec['properties']), include_only_properties=include_props)
                elif poly_type == 'MultiPolygon':
                    polys = []
                    for coords in rec['geometry']['coordinates']:
                        poly = Polygon(coords[0])
                        polys.append(poly)
                        index.index_polygon(i, poly)

                    multi_poly = index.simplify_polygon(MultiPolygon(polys))
                    index.add_polygon(multi_poly, dict(rec['properties']), include_only_properties=include_props)
                else:
                    continue

                i += 1
        return index

    @classmethod
    def create_with_quattroshapes(cls, quattroshapes_dir,
                                  output_dir,
                                  index_filename=DEFAULT_INDEX_FILENAME,
                                  polys_filename=DEFAULT_POLYS_FILENAME):

        admin0_filename = os.path.join(quattroshapes_dir, cls.COUNTRIES_FILENAME)
        admin1_filename = os.path.join(quattroshapes_dir, cls.ADMIN1_FILENAME)
        admin1r_filename = os.path.join(quattroshapes_dir, cls.ADMIN1_REGION_FILENAME)
        admin2_filename = os.path.join(quattroshapes_dir, cls.ADMIN2_FILENAME)
        admin2r_filename = os.path.join(quattroshapes_dir, cls.ADMIN2_REGION_FILENAME)
        local_admin_filename = os.path.join(quattroshapes_dir, cls.LOCAL_ADMIN_FILENAME)
        localities_filename = os.path.join(quattroshapes_dir, cls.LOCALITIES_FILENAME)
        neighborhoods_filename = os.path.join(quattroshapes_dir, cls.NEIGHBORHOODS_FILENAME)

        return cls.create_from_shapefiles([admin0_filename, admin1_filename, admin1r_filename,
                                          admin2_filename, admin2r_filename, local_admin_filename,
                                          localities_filename, neighborhoods_filename],
                                          output_dir, index_filename=index_filename,
                                          polys_filename=polys_filename)

    def sort_level(self, i):
        props, p = self.polygons[i]
        return self.sort_levels.get(props['qs_level'], 0)

    def get_candidate_polygons(self, lat, lon):
        candidates = OrderedDict.fromkeys(self.index.intersection((lon, lat, lon, lat))).keys()
        return sorted(candidates, key=self.sort_level, reverse=True)


if __name__ == '__main__':
    # Handle argument parsing here
    parser = argparse.ArgumentParser()

    parser.add_argument('-q', '--quattroshapes-dir',
                        help='Path to quattroshapes dir')

    parser.add_argument('-o', '--out-dir',
                        default=os.getcwd(),
                        help='Output directory')

    args = parser.parse_args()
    index = ReverseGeocoder.create_with_quattroshapes(args.quattroshapes_dir, args.out_dir)
    index.save()
