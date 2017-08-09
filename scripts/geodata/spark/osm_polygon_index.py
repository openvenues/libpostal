import ujson as json

from shapely.geometry import shape

from geodata.polygons.area import polygon_area
from geodata.spark.polygon_index import PolygonIndexSpark


class OSMPolygonIndexSpark(PolygonIndexSpark):
    @classmethod
    def geojson_ids(cls, lines):
        geojson = lines.map(lambda line: json.loads(line.rstrip()))
        geojson_ids = geojson.map(lambda rec: ((rec['properties']['type'], rec['properties']['id']), cls.preprocess_geojson(rec)))
        return geojson_ids


class OSMAdminPolygonIndexSpark(OSMPolygonIndexSpark):
    ADMIN_LEVEL_KEY = 'admin_level'

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


class OSMCountryPolygonIndexSpark(OSMPolygonIndexSpark):
    buffer_levels = (0.0, 10e-6, 0.001, 0.01, 0.1, 0.2, 0.3, 0.4, 0.5, 1.0, 2.0, 3.0)
    buffered_simplify_tolerance = 0.001

    @classmethod
    def reverse_geocode(cls, point_ids, polygon_ids):
        points_with_polygons = cls.points_with_polygons(point_ids, polygon_ids)

        points_without_polygons = point_ids.subtractByKey(points_with_polygons)

        country_polygons = polygon_ids.filter(lambda (poly_id, rec): 'ISO3166-1:alpha2' in rec['properties'])

        points_with_polygons_buffered = cls.points_with_polygons(points_without_polygons,
                                                                 country_polygons,
                                                                 buffer_levels=cls.buffer_levels,
                                                                 buffered_simplify_tolerance=cls.buffered_simplify_tolerance)
        combined_points_with_polygons = points_with_polygons.union(points_with_polygons_buffered)

        all_points = point_ids.leftOuterJoin(combined_points_with_polygons) \
                              .map(lambda (point_id, (point, polys)): (point, polys or []))

        return all_points
