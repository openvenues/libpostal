import operator

from geodata.language_id.disambiguation import disambiguate_language, UNKNOWN_LANGUAGE, WELL_REPRESENTED_LANGUAGES


def country_and_languages(language_rtree, latitude, longitude):
    props = language_rtree.point_in_poly(latitude, longitude, return_all=True)
    if not props:
        return None, None, None

    country = props[0]['qs_iso_cc'].lower()
    languages = []
    language_set = set()

    have_regional = False

    for p in props:
        for l in p['languages']:
            lang = l['lang']
            if lang not in language_set:
                language_set.add(lang)
                if p['admin_level'] > 0 and l['default']:
                    have_regional = True
                elif have_regional:
                    l = {'lang': l['lang'], 'default': 0}
                languages.append(l)

    # Python's builtin sort is stable, so if there are two defaults, the first remains first
    # Since polygons are returned from the index ordered from smallest admin level to largest,
    # it means the default language of the region overrides the country default
    default_languages = sorted(languages, key=operator.itemgetter('default'), reverse=True)
    return country, default_languages, props


def best_country_and_language(language_rtree, latitude, longitude, name):
    country, candidate_languages, language_props = country_and_languages(language_rtree, latitude, longitude)
    if not (country and candidate_languages):
        return None, None

    num_langs = len(candidate_languages)
    default_langs = set([l['lang'] for l in candidate_languages if l.get('default')])
    num_defaults = len(default_langs)

    regional_defaults = 0
    country_defaults = 0
    regional_langs = set()
    country_langs = set()
    for p in language_props:
        if p['admin_level'] > 0:
            regional_defaults += sum((1 for lang in p['languages'] if lang.get('default')))
            regional_langs |= set([l['lang'] for l in p['languages']])
        else:
            country_defaults += sum((1 for lang in p['languages'] if lang.get('default')))
            country_langs |= set([l['lang'] for l in p['languages']])

    if num_langs == 1:
        return country, candidate_languages[0]['lang']
    else:
        lang = disambiguate_language(name, [(l['lang'], l['default']) for l in candidate_languages])
        default_lang = candidate_languages[0]['lang']

        if lang == UNKNOWN_LANGUAGE and num_defaults == 1:
            return country, default_lang
        elif lang != UNKNOWN_LANGUAGE:
            if lang != default_lang and lang in country_langs and country_defaults > 1 and regional_defaults > 0 and lang in WELL_REPRESENTED_LANGUAGES:
                return country, UNKNOWN_LANGUAGE
            return country, lang
        else:
            return None, None
