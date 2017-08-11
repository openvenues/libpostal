import geohash
import ujson as json
from shapely.geometry import shape, Point
from shapely.prepared import prep
from geodata.polygons.geohash_polygon import GeohashPolygon


class PolygonIndexSpark(object):
    sort_reverse = False

    @classmethod
    def polygon_geohashes(cls, geojson_ids):
        return geojson_ids.flatMap(lambda (key, rec): [(h, key) for h in GeohashPolygon.cover_polygon_find_precision(shape(rec['geometry']))])

    @classmethod
    def point_geohashes(cls, geojson_ids):
        geohash_ids = geojson_ids.mapValues(lambda rec: geohash.encode(rec['geometry']['coordinates'][1], rec['geometry']['coordinates'][0]))
        return geohash_ids.flatMap(lambda (key, gh): [(gh[:i], key) for i in range(GeohashPolygon.GEOHASH_MIN_PRECISION, GeohashPolygon.GEOHASH_MAX_PRECISION + 1)])

    @classmethod
    def preprocess_geojson(cls, rec):
        return rec

    @classmethod
    def geojson_ids(cls, lines):
        geojson = lines.map(lambda line: json.loads(line.rstrip()))
        geojson_ids = geojson.zipWithUniqueId() \
                             .map(lambda (rec, uid): (uid, cls.preprocess_geojson(rec)))
        return geojson_ids

    @classmethod
    def polygon_contains(cls, poly, lat, lon):
        point = Point(lon, lat)
        if not hasattr(poly, '__iter__'):
            return (None, poly.contains(point))
        else:
            for level, p in poly:
                if p.contains(point):
                    return (level, True)
        return (None, False)

    @classmethod
    def prep_polygons(cls, record, buffer_levels=(), buffered_simplify_tolerance=0.0):
        poly = shape(record['geometry'])
        if not buffer_levels:
            return prep(poly)

        polys = [(None, prep(poly))]
        for level in buffer_levels:
            buffered = poly.buffer(level)
            if level > 0.0:
                simplify_level = buffered_simplify_tolerance if level >= buffered_simplify_tolerance else level
                buffered = buffered.simplify(simplify_level)
            polys.append((level, prep(buffered)))
        return polys

    @classmethod
    def points_in_polygons(cls, point_ids, polygon_ids, buffer_levels=(), buffered_simplify_tolerance=0.0):
        polygon_geohashes = cls.polygon_geohashes(polygon_ids)
        point_geohashes = cls.point_geohashes(point_ids)

        point_coords = point_ids.mapValues(lambda rec: (rec['geometry']['coordinates'][1], rec['geometry']['coordinates'][0]))

        poly_points = polygon_geohashes.join(point_geohashes) \
                                       .map(lambda (gh, (poly_id, point_id)): (point_id, (poly_id, gh if len(gh) <= 4 else None))) \
                                       .join(point_coords) \
                                       .map(lambda (point_id, ((poly_id, gh), (lat, lon))): ((poly_id, gh), (point_id, lat, lon)))

        num_partitions = poly_points.getNumPartitions()

        poly_groups = poly_points.groupByKey() \
                                 .map(lambda ((poly_id, gh), points): (poly_id, (gh, points))) \
                                 .join(polygon_ids) \
                                 .map(lambda (poly_id, ((gh, points), rec)): ((poly_id, gh), (rec, points))) \
                                 .partitionBy(num_partitions)  # repartition the keys so theyre (poly_id, geohash) instead of just poly_id

        points_in_polygons = poly_groups.mapValues(lambda (rec, points): (cls.prep_polygons(rec, buffer_levels=buffer_levels, buffered_simplify_tolerance=buffered_simplify_tolerance), points)) \
                                        .flatMap(lambda ((poly_id, gh), (poly, points)): ((point_id, poly_id, level) for point_id, poly_id, (level, contained) in ((point_id, poly_id, cls.polygon_contains(poly, lat, lon)) for (point_id, lat, lon) in points) if contained))

        return points_in_polygons

    @classmethod
    def sort_key(cls, props):
        return None

    @classmethod
    def sort_key_tuple(cls, (props, level)):
        return cls.sort_key(props)

    @classmethod
    def sort_level(cls, (polygon, level)):
        return -float('inf') if level is None else level

    @classmethod
    def points_with_polygons(cls, point_ids, polygon_ids, buffer_levels=(), buffered_simplify_tolerance=0.0, with_buffer_levels=False):
        points_in_polygons = cls.points_in_polygons(point_ids, polygon_ids, buffer_levels=buffer_levels, buffered_simplify_tolerance=buffered_simplify_tolerance)
        polygon_props = polygon_ids.mapValues(lambda poly: poly['properties'])

        points_with_polys = points_in_polygons.map(lambda (point_id, polygon_id, level): (polygon_id, (point_id, level))) \
                                              .join(polygon_props) \
                                              .values() \
                                              .map(lambda ((point_id, level), poly_props): (point_id, [(poly_props, level)])) \
                                              .reduceByKey(lambda x, y: x + y) \
                                              .mapValues(lambda polys: sorted(polys, key=cls.sort_key_tuple, reverse=cls.sort_reverse))

        if not with_buffer_levels:
            return points_with_polys.mapValues(lambda polys: [p for p, level in sorted(polys, key=cls.sort_level)])
        else:
            return points_with_polys.mapValues(lambda polys: sorted(polys, key=cls.sort_level))

    @classmethod
    def reverse_geocode(cls, point_ids, polygon_ids):
        points_with_polygons = cls.points_with_polygons(point_ids, polygon_ids)

        all_points = point_ids.leftOuterJoin(points_with_polygons) \
                              .map(lambda (point_id, (point, polys)): (point, polys or []))

        return all_points
