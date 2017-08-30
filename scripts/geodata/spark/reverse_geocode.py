from shapely.geometry import shape

from geodata.addresses.components import AddressComponents
from geodata.encoding import safe_decode

from geodata.polygons.area import polygon_area
from geodata.spark.polygon_index import PolygonIndexSpark
from geodata.spark.point_index import PointIndexSpark


class OSMIndexSpark(object):
    @classmethod
    def geojson_ids(cls, geojson):
        geojson_ids = geojson.map(lambda rec: ((rec['properties']['type'], safe_decode(rec['properties']['id'])), rec))
        return geojson_ids


class OSMPolygonIndexSpark(OSMIndexSpark, PolygonIndexSpark):
    pass


class OSMPointIndexSpark(OSMIndexSpark, PointIndexSpark):
    pass


class OSMAdminPolygonIndexSpark(OSMPolygonIndexSpark):
    ADMIN_LEVEL_KEY = 'admin_level'

    large_polygons = True
    sort_reverse = True

    @classmethod
    def sort_key(cls, properties):
        admin_level_key = cls.ADMIN_LEVEL_KEY
        admin_level = properties.get(admin_level_key, 0)
        try:
            admin_level = int(admin_level)
        except ValueError:
            admin_level = 0
        return admin_level


class OSMPlaceIndexSpark(OSMPointIndexSpark):
    GEOHASH_PRECISION = 5


class OSMMetroStationIndexSpark(OSMPointIndexSpark):
    GEOHASH_PRECISION = 6


class NeighborhoodsIndexSpark(OSMPolygonIndexSpark):
    source_priorities = {
        'osm': 0,            # Best names/polygons, same coordinate system
        'osm_cth': 1,        # Prefer the OSM names if possible
        'clickthathood': 2,  # Better names/polygons than Quattroshapes
        'osm_quattro': 3,    # Prefer OSM names matched with Quattroshapes polygon
        'quattroshapes': 4,  # Good results in some countries/areas
    }

    level_priorities = {
        'neighborhood': 0,
        'local_admin': 1,
    }

    SOURCE_KEY = 'source'
    POLYGON_TYPE_KEY = 'polygon_type'

    @classmethod
    def sort_key(cls, properties):
        source = properties[cls.SOURCE_KEY]
        polygon_type = properties[cls.POLYGON_TYPE_KEY]
        return (cls.source_priorities[source], cls.level_priorities[polygon_type])


class OSMAreaPolygonIndexSpark(OSMPolygonIndexSpark):
    AREA_KEY = '__area__'

    sort_reverse = True

    @classmethod
    def preprocess_geojson(cls, rec):
        poly_area = polygon_area(shape(rec['geometry']))
        poly_area_km2 = poly_area * 1e-6
        area_key = cls.AREA_KEY
        rec['properties'][area_key] = poly_area_km2
        return rec

    @classmethod
    def sort_key(cls, props):
        area_key = cls.AREA_KEY
        return props[area_key]

    @classmethod
    def postprocess_polygon_properties(cls, props):
        props.pop(cls.AREA_KEY, None)
        return props


class OSMSubdivisionPolygonIndexSpark(OSMAreaPolygonIndexSpark):
    pass


class OSMBuildingPolygonIndexSpark(OSMAreaPolygonIndexSpark):
    pass


class OSMCountryPolygonIndexSpark(OSMPolygonIndexSpark):
    buffer_levels = (0.0, 10e-6, 0.001, 0.01, 0.1, 0.2, 0.3, 0.4, 0.5, 1.0, 2.0, 3.0)
    buffered_simplify_tolerance = 0.001

    large_polygons = True

    COUNTRY = 'country'
    CANDIDATE_LANGUAGES = 'candidate_languages'

    @classmethod
    def preprocess_geojson(cls, rec):
        rec['properties'] = AddressComponents.country_polygon_minimal_properties(rec['properties'])
        return rec

    @classmethod
    def country_and_candidate_languages(cls, polys):
        country, candidate_languages = AddressComponents.osm_country_and_languages(polys)

        return {
            cls.COUNTRY: country,
            cls.CANDIDATE_LANGUAGES: candidate_languages
        }

    @classmethod
    def reverse_geocode(cls, sc, point_ids, polygon_ids):
        points_with_polygons = cls.points_with_polygons(sc, point_ids, polygon_ids) \
                                  .mapValues(lambda polys: cls.country_and_candidate_languages(polys))

        points_without_polygons = point_ids.subtractByKey(points_with_polygons)

        country_polygons = polygon_ids.filter(lambda (poly_id, rec): 'ISO3166-1:alpha2' in rec['properties'])

        points_with_polygons_buffered = cls.points_with_polygons(sc, points_without_polygons,
                                                                 country_polygons,
                                                                 buffer_levels=cls.buffer_levels,
                                                                 buffered_simplify_tolerance=cls.buffered_simplify_tolerance) \
                                           .mapValues(lambda polys: cls.country_and_candidate_languages(polys))

        combined_points_with_polygons = points_with_polygons.union(points_with_polygons_buffered)

        all_points = point_ids.leftOuterJoin(combined_points_with_polygons) \
                              .map(lambda (point_id, (point, polys)): (point, polys or []))

        return all_points
