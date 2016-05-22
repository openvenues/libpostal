from collections import namedtuple

from geodata.addresses.config import address_config
from geodata.categories.config import category_config
from geodata.categories.preposition import CategoryPreposition
from geodata.encoding import safe_decode
from geodata.math.sampling import weighted_choice

CategoryQuery = namedtuple('CategoryQuery', 'category, prep, add_place_name, add_address')

NULL_CATEGORY_QUERY = CategoryQuery(None, None, False, False)


class Category(object):
    @classmethod
    def phrase(cls, language, key, value, is_plural=False, country=None):
        category_phrase = category_config.get_phrase(language, key, value, is_plural=is_plural)
        if not category_phrase:
            return NULL_CATEGORY_QUERY

        category_phrase = safe_decode(category_phrase)

        prep_phrase_type = CategoryPreposition.random(language, country=country)

        if prep_phrase_type in (None, CategoryPreposition.NULL):
            return CategoryQuery(category_phrase, prep=None, add_place_name=True, add_address=True)

        values, probs = address_config.alternative_probabilities('categories.{}'.format(prep_phrase_type), language, country=country)
        if not values:
            return CategoryQuery(category_phrase, prep=None, add_place_name=True, add_address=True)

        prep_phrase, prep_phrase_props = weighted_choice(values, probs)
        prep_phrase = safe_decode(prep_phrase)

        add_address = prep_phrase_type not in (CategoryPreposition.NEARBY, CategoryPreposition.NEAR_ME, CategoryPreposition.IN)
        add_place_name = prep_phrase_type not in (CategoryPreposition.NEARBY, CategoryPreposition.NEAR_ME)

        return CategoryQuery(category_phrase, prep=prep_phrase, add_place_name=add_place_name, add_address=add_address)
