# -*- coding: utf-8 -*-
'''
geodata.coordinates.conversion
------------------------------

Geographic coordinates typically come in two flavors: decimal and
DMS (degree-minute-second). This module parses a coordinate string
in just about any format. This was originally created for parsing
lat/lons found on the web.

Usage:
    >>> latlon_to_decimal('40°42′46″N', '74°00′21″W') # returns (40.71277777777778, 74.00583333333333)
    >>> latlon_to_decimal('40,74 N', '74,001 W') # returns (40.74, -74.001)
'''

import re

from geodata.encoding import safe_decode

beginning_re = re.compile('^[^0-9\-]+', re.UNICODE)
end_re = re.compile('[^0-9]+$', re.UNICODE)

latitude_dms_regex = re.compile(ur'^(-?[0-9]{1,2})[ ]*[ :°ºd][ ]*([0-5]?[0-9])?[ ]*[:\'\u2032m]?[ ]*([0-5]?[0-9](?:\.\d+)?)?[ ]*[:\?\"\u2033s]?[ ]*(N|n|S|s)?$', re.I | re.UNICODE)
longitude_dms_regex = re.compile(ur'^(-?1[0-8][0-9]|0?[0-9]{1,2})[ ]*[ :°ºd][ ]*([0-5]?[0-9])?[ ]*[:\'\u2032m]?[ ]*([0-5]?[0-9](?:\.\d+)?)?[ ]*[:\?\"\u2033s]?[ ]*(E|e|W|w)?$', re.I | re.UNICODE)

latitude_decimal_with_direction_regex = re.compile('^(-?[0-9][0-9](?:\.[0-9]+))[ ]*[ :°ºd]?[ ]*(N|n|S|s)$', re.I)
longitude_decimal_with_direction_regex = re.compile('^(-?1[0-8][0-9]|0?[0-9][0-9](?:\.[0-9]+))[ ]*[ :°ºd]?[ ]*(E|e|W|w)$', re.I)

direction_sign_map = {'n': 1, 's': -1, 'e': 1, 'w': -1}


def direction_sign(d):
    if d is None:
        return 1
    d = d.lower().strip()
    if d in direction_sign_map:
        return direction_sign_map[d]
    else:
        raise ValueError('Invalid direction: {}'.format(d))


def int_or_float(d):
    try:
        return int(d)
    except ValueError:
        return float(d)


def degrees_to_decimal(degrees, minutes, seconds):
    degrees = int_or_float(degrees)
    minutes = int_or_float(minutes)
    seconds = int_or_float(seconds)

    return degrees + (minutes / 60.0) + (seconds / 3600.0)


def latlon_to_decimal(latitude, longitude):
    have_lat = False
    have_lon = False

    latitude = safe_decode(latitude).strip(u' ,;|')
    longitude = safe_decode(longitude).strip(u' ,;|')

    latitude = latitude.replace(u',', u'.')
    longitude = longitude.replace(u',', u'.')

    lat_dms = latitude_dms_regex.match(latitude)
    lat_dir = latitude_decimal_with_direction_regex.match(latitude)

    if lat_dms:
        d, m, s, c = lat_dms.groups()
        sign = direction_sign(c)
        latitude = degrees_to_decimal(d or 0, m or 0, s or 0)
        have_lat = True
    elif lat_dir:
        d, c = lat_dir.groups()
        sign = direction_sign(c)
        latitude = float(d) * sign
        have_lat = True
    else:
        latitude = re.sub(beginning_re, u'', latitude)
        latitude = re.sub(end_re, u'', latitude)

    lon_dms = longitude_dms_regex.match(longitude)
    lon_dir = longitude_decimal_with_direction_regex.match(longitude)

    if lon_dms:
        d, m, s, c = lon_dms.groups()
        sign = direction_sign(c)
        longitude = degrees_to_decimal(d or 0, m or 0, s or 0)
        have_lon = True
    elif lon_dir:
        d, c = lon_dir.groups()
        sign = direction_sign(c)
        longitude = float(d) * sign
        have_lon = True
    else:
        longitude = re.sub(beginning_re, u'', longitude)
        longitude = re.sub(end_re, u'', longitude)

    return float(latitude), float(longitude)
