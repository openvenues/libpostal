import operator


def country_and_languages(language_rtree, latitude, longitude):
    props = language_rtree.point_in_poly(latitude, longitude, return_all=True)
    if not props:
        return None, None, None

    country = props[0]['qs_iso_cc'].lower()
    languages = []

    for p in props:
        languages.extend(p['languages'])

    # Python's builtin sort is stable, so if there are two defaults, the first remains first
    # Since polygons are returned from the index ordered from smallest admin level to largest,
    # it means the default language of the region overrides the country default
    default_languages = sorted(languages, key=operator.itemgetter('default'), reverse=True)
    return country, default_languages, props
