import random
import re

from geodata.configs.utils import alternative_probabilities
from geodata.math.floats import isclose
from geodata.math.sampling import weighted_choice, cdf
from geodata.postal_codes.config import postal_codes_config
from geodata.postal_codes.validation import postcode_regexes


class PostalCodes(object):
    regex_cache = {}

    @classmethod
    def is_valid(cls, postal_code, country):
        regex = postcode_regexes.get(country)

        if regex:
            postal_code = postal_code.strip()
            m = regex.match(postal_code)
            if m and m.end() == len(postal_code):
                return True
            else:
                return False
        return True

    @classmethod
    def needs_validation(cls, country):
        return postal_codes_config.get_property('validate_postcode', country=country, default=False)

    @classmethod
    def should_strip_components(cls, country):
        return postal_codes_config.get_property('strip_components', country=country)

    @classmethod
    def format(cls, postal_code, country):
        postal_code = postal_code.strip()
        if not postal_codes_config.get_property('add_country_code', country=country):
            return postal_code

        regexes, regex_probs = cls.regex_cache.get(country, (None, None))
        if regexes is None:
            regex_replacements = postal_codes_config.get_property('regex_replacements', country=country, default=[])
            if regex_replacements:
                regexes = [(re.compile(r['regex'], re.I), r['replacement']) for r in regex_replacements]
                regex_probs = [r['probability'] for r in regex_replacements]
                if not isclose(sum(regex_probs), 1.0):
                    regexes.append((None, None))
                    regex_probs.append(1.0 - sum(regex_probs))
                cls.regex_cache[country] = (regexes, regex_probs)

        if regexes is not None:
            regex_probs_cdf = cdf(regex_probs)
            regex, replacement = weighted_choice(regexes, regex_probs_cdf)
            if regex is not None:
                match = regex.match(postal_code)
                postal_code = regex.sub(replacement, postal_code)

        cc_probability = postal_codes_config.get_property('country_code_probablity', country=country, default=0.0)
        if random.random() >= cc_probability or not postal_code or not postal_code[0].isdigit():
            return postal_code

        country_code_phrases = postal_codes_config.get_property('country_code_phrase', country=country, default=None)
        if country_code_phrases is None:
            country_code_phrase = country.upper()
        else:
            alternates, probs = alternative_probabilities(country_code_phrases)
            probs_cdf = cdf(probs)
            country_code_phrase = weighted_choice(alternates, probs_cdf)

        cc_hyphen_probability = postal_codes_config.get_property('country_code_hyphen_probability', country=country, default=0.0)

        separator = u''
        r = random.random()
        if r < cc_hyphen_probability:
            separator = u'-'

        return u'{}{}{}'.format(country_code_phrase, separator, postal_code)
