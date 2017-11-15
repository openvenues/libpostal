from collections import namedtuple

from geodata.addresses.config import address_config
from geodata.math.sampling import weighted_choice

IntersectionQuery = namedtuple('IntersectionQuery', 'road1, intersection_phrase, road2')

NULL_INTERSECTION_QUERY = IntersectionQuery(None, None, None)


class Intersection(object):
    @classmethod
    def phrase(cls, language, country=None):
        values, probs = address_config.alternative_probabilities('cross_streets.intersection', language, country=country)
        if not values:
            return None
        phrase, props = weighted_choice(values, probs)
        return phrase
