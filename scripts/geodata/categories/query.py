import random
import six

from collections import namedtuple

from geodata.addresses.config import address_config
from geodata.categories.config import category_config
from geodata.encoding import safe_decode
from geodata.math.sampling import weighted_choice, cdf

CategoryQuery = namedtuple('CategoryQuery', 'category, prep, add_place_name')

NULL_CATEGORY_QUERY = CategoryQuery(None, None, False)


class Category(object):
    NEAR = 'near'
    NEARBY = 'nearby'
    NEAR_ME = 'near_me'
    IN = 'in'
    NULL = 'null'

    @classmethod
    def phrase(cls, language, key, value, is_plural=False, country=None):
        category_phrase = category_config.get_phrase(language, key, value, is_plural=is_plural)
        if not category_phrase:
            return NULL_CATEGORY_QUERY

        category_phrase = safe_decode(category_phrase)

        category_props = address_config.get_property('categories', language, country=country)
        if category_props is None:
            return CategoryQuery(category_phrase, prep=None, add_place_name=True)

        values = []
        probs = []

        for prep_phrase_type in (cls.NEAR, cls.NEARBY, cls.NEAR_ME, cls.IN, cls.NULL):
            k = '{}_probability'.format(prep_phrase_type)
            prob = category_props.get(k, None)
            if prob is not None:
                values.append(prep_phrase_type)
                probs.append(prob)

        probs = cdf(probs)

        prep_phrase_type = weighted_choice(values, probs)

        if prep_phrase_type == cls.NULL:
            return CategoryQuery(category_phrase, prep=None, add_place_name=True)

        values, probs = address_config.alternative_probabilities('categories.{}'.format(prep_phrase_type), language, country=country)
        if not values:
            return CategoryQuery(category_phrase, prep=None, add_place_name=True)

        prep_phrase, prep_phrase_props = weighted_choice(values, probs)
        prep_phrase = safe_decode(prep_phrase)

        add_place_name = prep_phrase_type not in (cls.NEARBY, cls.NEAR_ME)

        return CategoryQuery(category_phrase, prep=prep_phrase, add_place_name=add_place_name)
