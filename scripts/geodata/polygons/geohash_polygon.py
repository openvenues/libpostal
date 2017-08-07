import math
import geohash


def height_degrees(n):
    if n % 2 == 0:
        a = 0.0
    else:
        a = -0.5

    return 180.0 / pow(2, 2.5 * n + a)


def width_degrees(n):
    if n % 2 == 0:
        a = -1.0
    else:
        a = -0.5

    return 180.0 / pow(2, 2.5 * n + a)


class GeohashPolygon(object):
    DEFAULT_GEOHASH_PRECISION = 5

    GEOHASH_MIN_PRECISION = 3
    GEOHASH_MAX_PRECISION = 8

    MAX_HASH_LENGTH_CACHE = 8
    height_degrees_cache = {n: height_degrees(n) for n in xrange(MAX_HASH_LENGTH_CACHE)}
    width_degrees_cache = {n: width_degrees(n) for n in xrange(MAX_HASH_LENGTH_CACHE)}

    @classmethod
    def height_degrees(cls, n):
        if n <= cls.MAX_HASH_LENGTH_CACHE and n in cls.height_degrees_cache:
            return cls.height_degrees_cache[n]

        result = height_degrees(n)

        if n <= cls.MAX_HASH_LENGTH_CACHE:
            cls.height_degrees_cache[n] = result
        return result

    @classmethod
    def width_degrees(cls, n):
        if n <= cls.MAX_HASH_LENGTH_CACHE and n in cls.width_degrees_cache:
            return cls.width_degrees_cache[n]

        result = width_degrees(n)

        if n <= cls.MAX_HASH_LENGTH_CACHE:
            cls.width_degrees_cache[n] = result
        return result

    @classmethod
    def lon_to_180(cls, lon):
        if lon < 180.0 and lon > 0.0:
            return lon
        elif lon < 0.0:
            return -cls.lon_to_180(abs(lon))
        else:
            n = round(math.floor((lon + 180.0) / 360.0))
            return lon - n * 360.0

    @classmethod
    def longitude_diff(cls, a, b):
        a = cls.lon_to_180(a)
        b = cls.lon_to_180(b)

        return abs(cls.lon_to_180(a - b))

    @classmethod
    def cover_bbox(cls, top_left_lat, top_left_lon, bottom_right_lat, bottom_right_lon, precision=DEFAULT_GEOHASH_PRECISION):
        height_degrees = cls.height_degrees(precision)
        width_degrees = cls.width_degrees(precision)

        max_lon = top_left_lon + cls.longitude_diff(bottom_right_lon, top_left_lon)

        hashes = set()

        # Small optimization, prevents Python global variable lookups in the loop
        geohash_encode = geohash.encode

        lat = bottom_right_lat
        while lat <= top_left_lat:
            lon = top_left_lon
            while lon <= max_lon:
                gh = geohash_encode(lat, lon)[:precision]
                hashes.add(gh)
                lon += width_degrees
            lat += height_degrees

        # ensure that the borders are covered
        lat = bottom_right_lat
        while lat <= top_left_lat:
            gh = geohash_encode(lat, max_lon)[:precision]
            hashes.add(gh)
            lat += height_degrees

        lon = top_left_lon
        while lon <= max_lon:
            gh = geohash_encode(top_left_lat, lon)[:precision]
            hashes.add(gh)
            lon += width_degrees

        return hashes

    @classmethod
    def cover_single_polygon(cls, poly, precision=DEFAULT_GEOHASH_PRECISION):
        bbox = poly.bounds
        top_left_lon, bottom_right_lat, bottom_right_lon, top_left_lat = bbox
        return cls.cover_bbox(top_left_lat, top_left_lon, bottom_right_lat, bottom_right_lon, precision=precision)

    @classmethod
    def cover_polygon(cls, poly, precision=DEFAULT_GEOHASH_PRECISION):
        if poly.type != 'MultiPolygon':
            return cls.cover_single_polygon(poly)
        else:
            hashes = set()
            for p in poly:
                hashes |= cls.cover_single_polygon(p, precision=precision)
            return hashes

    @classmethod
    def cover_polygon_find_precision(cls, poly, min_precision=GEOHASH_MIN_PRECISION, max_precision=GEOHASH_MAX_PRECISION):
        min_hashes = 3
        if hasattr(poly, '__iter__'):
            min_hashes = len(poly) * 2

        hashes = set()

        for precision in xrange(min_precision, max_precision + 1):
            hashes = cls.cover_polygon(poly, precision=precision)
            if len(hashes) >= min_hashes:
                return hashes
        return hashes
