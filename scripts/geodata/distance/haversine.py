# -*- coding: utf-8 -*-
import math

EARTH_RADIUS_KM = 6373


def haversine_distance(lat1, lon1, lat2, lon2, radius=EARTH_RADIUS_KM):
    """Calculate the Haversine distance between two lat/lon pairs, given by:
    a = sin²(Δφ/2) + cos φ1 ⋅ cos φ2 ⋅ sin²(Δλ/2)
    c = 2 ⋅ atan2( √a, √(1−a) )
    d = R ⋅ c

    where R is the radius of the Earth (in kilometers). By default we use 6373 km,
    a radius optimized for calculating distances at approximately 39 degrees from
    the equator i.e. Washington, DC

    :param lat1: first latitude
    :param lon1: first longitude (use negative range for longitudes West of the Prime Meridian)
    :param lat2: second latitude
    :param lon2: second longitude (use negative range for longitudes West of the Prime Meridian)
    :param radius: radius of the Earth in (miles|kilometers) depending on the desired units
    """
    lat1 = math.radians(lat1)
    lat2 = math.radians(lat2)
    lon1 = math.radians(lon1)
    lon2 = math.radians(lon2)

    dlon = lon2 - lon1
    dlat = lat2 - lat1
    a = (math.sin(dlat / 2.0)) ** 2 + math.cos(lat1) * math.cos(lat2) * (math.sin(dlon/2.0)) ** 2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    d = radius * c
    return d
