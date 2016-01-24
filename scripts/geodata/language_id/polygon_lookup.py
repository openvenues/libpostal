import operator


def country_and_languages(language_rtree, latitude, longitude):
    props = language_rtree.point_in_poly(latitude, longitude, return_all=True)
    if not props:
        return None, None, None

    country = props[0]['qs_iso_cc'].lower()
    languages = []
    language_set = set()

    for p in props:
        for l in p['languages']:
            lang = l['lang']
            if lang not in language_set:
                language_set.add(lang)
                languages.append(l)

    # Python's builtin sort is stable, so if there are two defaults, the first remains first
    # Since polygons are returned from the index ordered from smallest admin level to largest,
    # it means the default language of the region overrides the country default
    default_languages = sorted(languages, key=operator.itemgetter('default'), reverse=True)
    return country, default_languages, props
