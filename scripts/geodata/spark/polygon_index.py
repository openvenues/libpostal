import geohash

from six.moves import xrange
from shapely.geometry import shape, Point
from shapely.geometry.base import BaseGeometry
from shapely.prepared import prep
from geodata.polygons.geohash_polygon import GeohashPolygon
from geodata.spark.geo_index import GeoIndexSpark


class PolygonIndexSpark(GeoIndexSpark):
    sort_reverse = False
    large_polygons = False

    MAX_PER_SHARD = 500000

    @classmethod
    def polygon_geohashes(cls, geojson_ids):
        return geojson_ids.flatMap(lambda (key, rec): [(h, key) for h in GeohashPolygon.cover_polygon_find_precision(shape(rec['geometry']))])

    @classmethod
    def point_geohashes(cls, geojson_ids):
        geohash_ids = cls.geohash_points(geojson_ids)
        return geohash_ids.flatMap(lambda (key, (gh, lat, lon)): [(gh[:i], (key, lat, lon)) for i in range(GeohashPolygon.GEOHASH_MIN_PRECISION, GeohashPolygon.GEOHASH_MAX_PRECISION + 1)])

    @classmethod
    def preprocess_geojson(cls, rec):
        return rec

    @classmethod
    def postprocess_geojson(cls, rec):
        return rec

    @classmethod
    def geojson_ids(cls, geojson):
        geojson_ids = geojson.zipWithUniqueId() \
                             .map(lambda (rec, uid): (uid, rec))
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
    def build_polygons(cls, geometry, buffer_levels=(), buffered_simplify_tolerance=0.0):
        poly = shape(geometry)
        if not buffer_levels:
            return poly

        polys = [(None, poly)]
        for level in buffer_levels:
            buffered = poly.buffer(level)
            if level > 0.0:
                simplify_level = buffered_simplify_tolerance if level >= buffered_simplify_tolerance else level
                buffered = buffered.simplify(simplify_level)
            polys.append((level, buffered))
        return polys

    @classmethod
    def prep_polygons(cls, polys):
        if isinstance(polys, BaseGeometry):
            poly = polys
            return prep(poly)
        else:
            return [(level, prep(poly)) for level, poly in polys]

    @classmethod
    def polygon_num_shards(cls, count, per_shard, remainder_split_threshold=0.4):
        if count < per_shard:
            return 1
        else:
            shards = count / per_shard
            if count % per_shard > (per_shard * remainder_split_threshold):
                shards += 1
            return shards

    @classmethod
    def large_polygon_shards(cls, sc, candidate_points, max_per_shard=MAX_PER_SHARD, poly_id_key=0):
        return sc.broadcast(candidate_points.map(lambda x: (x[poly_id_key], 1)) \
                                            .reduceByKey(lambda x, y: x + y) \
                                            .mapValues(lambda count: cls.polygon_num_shards(count, per_shard=max_per_shard)) \
                                            .filter(lambda (key, count): count > 1) \
                                            .collectAsMap()
                            )

    @classmethod
    def candidate_points(cls, point_ids, polygon_ids):
        polygon_geohashes = cls.polygon_geohashes(polygon_ids)
        point_geohashes = cls.point_geohashes(point_ids)

        return polygon_geohashes.join(point_geohashes) \
                                .values() \
                                .filter(lambda (poly_id, (point_id, lat, lon)): poly_id != point_id)

    @classmethod
    def polygon_properties(cls, polygon_ids):
        return polygon_ids.mapValues(lambda rec: rec['properties'])

    @classmethod
    def polygon_geometries(cls, polygon_ids):
        return polygon_ids.mapValues(lambda rec: rec['geometry'])

    @classmethod
    def points_in_polygons_large(cls, candidate_points, point_ids, polygon_ids, large_poly_shards, buffer_levels=(), buffered_simplify_tolerance=0.0):
        poly_points = candidate_points.zipWithUniqueId() \
                                      .map(lambda ((poly_id, (point_id, lat, lon)), uid): ((poly_id, uid % large_poly_shards.value.get(poly_id, 1)), (point_id, lat, lon)))

        poly_geometries = cls.polygon_geometries(polygon_ids) \
                             .flatMap(lambda (poly_id, geometry): (((poly_id, i), geometry) for i in xrange(large_poly_shards.value.get(poly_id, 1))))

        poly_groups = poly_points.aggregateByKey([], cls.append_to_list, cls.extend_list) \
                                 .join(poly_geometries) \
                                 .mapValues(lambda (points, geometry): (cls.build_polygons(geometry, buffer_levels=buffer_levels, buffered_simplify_tolerance=buffered_simplify_tolerance), points))

        points_in_polygons = poly_groups.mapValues(lambda (poly, points): (cls.prep_polygons(poly), points)) \
                                        .flatMap(lambda ((poly_id, shard), (poly, points)): ((point_id, poly_id, level) for point_id, poly_id, (level, contained) in ((point_id, poly_id, cls.polygon_contains(poly, lat, lon)) for (point_id, lat, lon) in points) if contained))

        return points_in_polygons

    @classmethod
    def points_in_polygons(cls, candidate_points, point_ids, polygon_ids, buffer_levels=(), buffered_simplify_tolerance=0.0):
        poly_geometries = cls.polygon_geometries(polygon_ids)

        poly_groups = candidate_points.aggregateByKey([], cls.append_to_list, cls.extend_list) \
                                      .join(poly_geometries) \
                                      .map(lambda (poly_id, (points, geometry)): ((poly_id, (cls.build_polygons(geometry, buffer_levels=buffer_levels, buffered_simplify_tolerance=buffered_simplify_tolerance), points))))

        points_in_polygons = poly_groups.mapValues(lambda (poly, points): (cls.prep_polygons(poly), points)) \
                                        .flatMap(lambda (poly_id, (poly, points)): ((point_id, poly_id, level) for point_id, poly_id, (level, contained) in ((point_id, poly_id, cls.polygon_contains(poly, lat, lon)) for (point_id, lat, lon) in points) if contained))

        return points_in_polygons

    @classmethod
    def sort_key(cls, props):
        return None

    @classmethod
    def sort_key_tuple(cls, (props, level)):
        return cls.sort_level(level), cls.sort_key(props)

    @classmethod
    def sort_level(cls, level):
        return level if level is not None else -float('inf') if not cls.sort_reverse else float('inf')

    @classmethod
    def preprocess_polygons(cls, polygon_ids):
        return polygon_ids.mapValues(lambda rec: cls.preprocess_geojson(rec))

    @classmethod
    def append_to_list(cls, l, val):
        l.append(val)
        return l

    @classmethod
    def extend_list(cls, l1, l2):
        l1.extend(l2)
        return l1

    @classmethod
    def join_polys_large(cls, points_in_polygons, polygon_ids, large_poly_shards, with_buffer_levels=False):
        polygon_props = cls.polygon_properties(polygon_ids) \
                           .flatMap(lambda (poly_id, props): (((poly_id, i), props) for i in xrange(large_poly_shards.value.get(poly_id, 1))))

        points_with_polys = points_in_polygons.zipWithUniqueId() \
                                              .map(lambda ((point_id, poly_id, level), uid): ((poly_id, uid % large_poly_shards.value.get(poly_id, 1)), (point_id, level))) \
                                              .join(polygon_props) \
                                              .values() \
                                              .map(lambda ((point_id, level), poly_props): (point_id, (poly_props, level))) \
                                              .aggregateByKey([], cls.append_to_list, cls.extend_list) \
                                              .mapValues(lambda polys: sorted(polys, key=cls.sort_key_tuple, reverse=cls.sort_reverse))

        if not with_buffer_levels:
            return points_with_polys.mapValues(lambda polys: [cls.postprocess_geojson(p) for p, level in polys])
        else:
            return points_with_polys.mapValues(lambda polys: [cls.postprocess_geojson(p) for p in polys])

    @classmethod
    def join_polys(cls, points_in_polygons, polygon_ids, with_buffer_levels=False):
        polygon_props = cls.polygon_properties(polygon_ids)

        points_with_polys = points_in_polygons.map(lambda (point_id, polygon_id, level): (polygon_id, (point_id, level))) \
                                              .join(polygon_props) \
                                              .values() \
                                              .map(lambda ((point_id, level), poly_props): (point_id, (poly_props, level))) \
                                              .aggregateByKey([], cls.append_to_list, cls.extend_list) \
                                              .mapValues(lambda polys: sorted(polys, key=cls.sort_key_tuple, reverse=cls.sort_reverse))

        if not with_buffer_levels:
            return points_with_polys.mapValues(lambda polys: [cls.postprocess_geojson(p) for p, level in polys])
        else:
            return points_with_polys.mapValues(lambda polys: [cls.postprocess_geojson(p) for p in polys])

    @classmethod
    def points_with_polygons(cls, sc, point_ids, polygon_ids, buffer_levels=(), buffered_simplify_tolerance=0.0, with_buffer_levels=False):
        polygon_ids = cls.preprocess_polygons(polygon_ids)
        candidate_points = cls.candidate_points(point_ids, polygon_ids)

        if not cls.large_polygons:
            points_in_polygons = cls.points_in_polygons(candidate_points, point_ids, polygon_ids, buffer_levels=buffer_levels, buffered_simplify_tolerance=buffered_simplify_tolerance)
            return cls.join_polys(points_in_polygons, polygon_ids, with_buffer_levels=with_buffer_levels)
        else:
            large_poly_shards = cls.large_polygon_shards(sc, candidate_points, max_per_shard=cls.MAX_PER_SHARD)
            points_in_polygons = cls.points_in_polygons_large(candidate_points, point_ids, polygon_ids, large_poly_shards, buffer_levels=buffer_levels, buffered_simplify_tolerance=buffered_simplify_tolerance)

            return cls.join_polys_large(points_in_polygons, polygon_ids, large_poly_shards, with_buffer_levels=with_buffer_levels)

    @classmethod
    def reverse_geocode(cls, sc, point_ids, polygon_ids):
        points_with_polygons = cls.points_with_polygons(sc, point_ids, polygon_ids)

        all_points = point_ids.leftOuterJoin(points_with_polygons) \
                              .map(lambda (point_id, (point, polys)): (point, polys or []))

        return all_points
