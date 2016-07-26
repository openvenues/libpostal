import re
import requests
import six.moves.urllib_parse as urlparse
import ujson

requests.models.json = ujson


GOOGLE_I18N_API = 'http://i18napis.appspot.com'
GOOGLE_ADDRESS_DATA_API = urlparse.urljoin(GOOGLE_I18N_API, 'address/data/')


class GoogleI18N(object):
    '''
    Fetches data from e.g. http://i18napis.appspot.com/address/data/GB
    and caches it in a dictionary for each country. These requests are
    lightweight, so for a given run of a program, max 250 requests
    will be made.
    '''
    def __init__(self):
        self.responses = {}

    def get(self, country_code):
        ret = self.responses.get(country_code.lower())

        if ret is None:
            url = urlparse.urljoin(GOOGLE_ADDRESS_DATA_API, country_code.upper())
            response = requests.get(url)
            if response.ok:
                ret = response.json()
                self.responses[country_code.lower()] = ret
            else:
                self.responses[country_code.lower()] = {}
        return ret


google_i18n = GoogleI18N()


class PostcodeRegexes(object):
    def __init__(self):
        self.responses = {}
        self.postcode_regexes = {}

    def get(self, country_code):
        ret = self.postcode_regexes.get(country_code.lower())
        if ret is None:
            response = google_i18n.get(country_code)
            if response:
                postcode_expression = response.get('zip')
                if not postcode_expression:
                    self.postcode_regexes[country_code.lower()] = None
                    return None
                ret = re.compile(postcode_expression, re.I)
                self.postcode_regexes[country_code.lower()] = ret

        return ret


postcode_regexes = PostcodeRegexes()
