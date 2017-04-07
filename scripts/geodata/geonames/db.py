import sqlite3
from collections import defaultdict


class GeoNamesDB(object):
    names_query = '''
    select iso_language, alternate_name,
    is_preferred_name, is_short_name
    from alternate_names
    where geonames_id = ?
    and is_historic != '1'
    and is_colloquial != '1'
    and iso_language != 'post'
    order by iso_language, cast(is_preferred_name as integer) desc, cast(is_short_name as integer)
    '''

    def __init__(self, filename):
        self.db = sqlite3.connect(filename)

    def query(self, query, *params):
        return self.db.execute(self.names_query, params)

    def get_alternate_names(self, geonames_id):
        cursor = self.query(self.names_query, geonames_id)
        language_names = defaultdict(list)
        for language, name, is_preferred, is_short in cursor:
            language_names[language].append((name,
                                             int(is_preferred or 0),
                                             int(is_short or 0)))
        return dict(language_names)
