import geohash
import ujson as json
from shapely.geometry import shape, Point
from shapely.prepared import prep
from geodata.polygons.geohash_polygon import GeohashPolygon


class PolygonIndexSpark(object):
    @classmethod
    def polygon_geohashes(cls, geojson_ids):
        return geojson_ids.flatMap(lambda (key, rec): [(h, key) for h in GeohashPolygon.cover_polygon_find_precision(shape(rec['geometry']))])

    @classmethod
    def point_geohashes(cls, geojson_ids):
        geohash_ids = geojson_ids.mapValues(lambda rec: geohash.encode(rec['geometry']['coordinates'][1], rec['geometry']['coordinates'][0]))
        return geohash_ids.flatMap(lambda (key, gh): [(gh[:i], key) for i in range(GeohashPolygon.GEOHASH_MIN_PRECISION, GeohashPolygon.GEOHASH_MAX_PRECISION + 1)])

    @classmethod
    def points_in_polygons(cls, point_ids, polygon_ids):
        polygon_geohashes = cls.polygon_geohashes(polygon_ids)
        point_geohashes = cls.point_geohashes(point_ids)

        point_coords = point_ids.mapValues(lambda rec: (rec['geometry']['coordinates'][1], rec['geometry']['coordinates'][0]))

        poly_points = polygon_geohashes.join(point_geohashes) \
                                       .map(lambda (gh, (poly_id, point_id)): (point_id, (poly_id, gh if len(gh) <= 4 else None))) \
                                       .join(point_coords) \
                                       .map(lambda (point_id, ((poly_id, gh), (lat, lon))): ((poly_id, gh), (point_id, lat, lon))) \
                                       .cache()

        num_partitions = poly_points.getNumPartitions()

        poly_groups = poly_points.groupByKey() \
                                 .map(lambda ((poly_id, gh), points): (poly_id, (gh, points))) \
                                 .join(polygon_ids) \
                                 .map(lambda (poly_id, ((gh, points), rec)): ((poly_id, gh), (rec, points))) \
                                 .partitionBy(num_partitions)  # repartition the keys so theyre (poly_id, geohash) instead of just poly_id

        points_in_polygons = poly_groups.mapValues(lambda (rec, points): (prep(shape(rec['geometry'])), points)) \
                                        .flatMap(lambda ((poly_id, gh), (poly, points)): ((point_id, poly_id) for (point_id, lat, lon) in points if poly.contains(Point(lon, lat))))

        points_with_polygons = points_in_polygons.map(lambda (point_id, polygon_id): (polygon_id, point_id)) \
                                                 .join(polygon_ids) \
                                                 .values() \
                                                 .groupByKey()

        all_points = point_ids.leftOuterJoin(points_with_polygons) \
                              .values() \
                              .mapValues(lambda (point, polys): (point, list(polys or [])))

        return all_points


class OSMPolygonIndexSpark(PolygonIndexSpark):
    @classmethod
    def geojson_ids(cls, lines):
        geojson = lines.map(lambda line: json.loads(line.rstrip()))
        geojson_ids = geojson.map(lambda rec: ((rec['properties']['type'], rec['properties']['id']), rec))
        return geojson_ids
