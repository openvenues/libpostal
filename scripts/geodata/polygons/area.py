import pyproj

from functools import partial
from shapely.ops import transform
from shapely.geometry import Polygon


def polygon_area(poly):
    return transform(
        partial(pyproj.transform,
                pyproj.Proj(init='EPSG:4326'),
                pyproj.Proj(proj='aea',
                            lat1=poly.bounds[1],
                            lat2=poly.bounds[2],
                            )
                ),
        poly
    ).area


def polygon_bounding_box_area(poly):
    bbox = poly.bounds
    p = Polygon([(bbox[0], bbox[3]), (bbox[0], bbox[1]),
                 (bbox[2], bbox[1]), (bbox[2], bbox[3]),
                 ])
    return polygon_area(p)
