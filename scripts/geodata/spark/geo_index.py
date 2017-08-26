import geohash


class GeoIndexSpark(object):
    @classmethod
    def geohash_points(cls, geojson_ids):
        return geojson_ids.mapValues(lambda rec: (rec['geometry']['coordinates'][1], rec['geometry']['coordinates'][0])) \
                          .mapValues(lambda (lat, lon): (geohash.encode(lat, lon), lat, lon))

    @classmethod
    def point_coords(cls, point_ids):
        return point_ids.mapValues(lambda rec: (rec['geometry']['coordinates'][1], rec['geometry']['coordinates'][0]))

    @classmethod
    def append_to_list(cls, l, val):
        l.append(val)
        return l

    @classmethod
    def extend_list(cls, l1, l2):
        l1.extend(l2)
        return l1
