import re
from geodata.i18n.google import google_i18n
from geodata.postal_codes.config import postal_codes_config


class PostcodeRegexes(object):
    def __init__(self):
        self.responses = {}
        self.postcode_regexes = {}

    def get(self, country_code):
        ret = self.postcode_regexes.get(country_code.lower())
        if ret is None:

            override_regex = postal_codes_config.get_property('override_regex', country=country_code)
            if override_regex:
                ret = re.compile(override_regex, re.I)
                self.postcode_regexes[country_code.lower()] = ret
                return ret

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
