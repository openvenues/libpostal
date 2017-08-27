import geohash
import ujson as json

from geodata.distance.haversine import haversine_distance
from geodata.spark.geo_index import GeoIndexSpark


class PointIndexSpark(GeoIndexSpark):
    GEOHASH_PRECISION = 5
    sort_reverse = False

    @classmethod
    def point_geohashes(cls, geojson_ids, precision=None):
        if precision is None:
            precision = cls.GEOHASH_PRECISION
        geohash_ids = cls.geohash_points(geojson_ids)
        return geohash_ids.map(lambda (key, (gh, lat, lon)): (gh[:precision], (key, lat, lon)))

    @classmethod
    def indexed_point_geohashes(cls, geojson_ids, precision=None):
        if precision is None:
            precision = cls.GEOHASH_PRECISION
        geohash_ids = cls.geohash_points(geojson_ids)
        return geohash_ids.flatMap(lambda (key, (full_gh, lat, lon)): [(gh, key) for gh in geohash.expand(full_gh[:precision])])

    @classmethod
    def preprocess_geojson(cls, rec):
        return rec

    @classmethod
    def geojson_ids(cls, lines):
        geojson = lines.map(lambda line: json.loads(line.rstrip()))
        geojson_ids = geojson.zipWithUniqueId() \
                             .map(lambda (rec, uid): (uid, rec))
        return geojson_ids

    @classmethod
    def distance_sort(cls, lat, lon):
        def distance_to(other):
            other_coords = other['geometry']['coordinates']
            other_lat, other_lon = other_coords[1], other_coords[0]
            return haversine_distance(lat, lon, other_lat, other_lon)
        return distance_to

    @classmethod
    def preprocess_indexed_points(cls, indexed_point_ids):
        return indexed_point_ids.mapValues(lambda rec: cls.preprocess_geojson(rec))

    @classmethod
    def nearby_points(cls, point_ids, indexed_point_ids, precision=None):
        indexed_point_ids = cls.preprocess_indexed_points(indexed_point_ids)
        indexed_point_geohashes = cls.indexed_point_geohashes(indexed_point_ids)
        point_geohashes = cls.point_geohashes(point_ids)

        point_coords = point_geohashes.values().map(lambda (point_id, lat, lon): (point_id, (lat, lon)))

        nearby_points = indexed_point_geohashes.join(point_geohashes) \
                                               .values() \
                                               .filter(lambda (indexed_point_id, (point_id, lat, lon)): indexed_point_id != point_id) \
                                               .map(lambda (indexed_point_id, (point_id, lat, lon)): (indexed_point_id, point_id)) \
                                               .join(indexed_point_ids) \
                                               .map(lambda (indexed_point_id, (point_id, indexed_point)): (point_id, indexed_point)) \
                                               .aggregateByKey([], cls.append_to_list, cls.extend_list) \
                                               .join(point_coords)

        return nearby_points.mapValues(lambda (indexed_points, (lat, lon)): [p['properties'] for p in sorted(list(indexed_points), key=cls.distance_sort(lat, lon))])

    @classmethod
    def reverse_geocode(cls, sc, point_ids, indexed_point_ids, precision=None):
        nearby_points = cls.nearby_points(point_ids, indexed_point_ids, precision=precision)

        all_points = point_ids.leftOuterJoin(nearby_points) \
                              .map(lambda (point_id, (point, indexed)): (point, indexed or []))

        return all_points
