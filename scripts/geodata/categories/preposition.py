from geodata.addresses.config import address_config
from geodata.categories.config import category_config
from geodata.math.sampling import weighted_choice, cdf


class CategoryPreposition(object):
    NEAR = 'near'
    NEARBY = 'nearby'
    NEAR_ME = 'near_me'
    IN = 'in'
    NULL = 'null'

    @classmethod
    def random(cls, language, country=None):
        category_props = address_config.get_property('categories', language, country=country)
        if category_props is None:
            return None

        values = []
        probs = []

        for prep_phrase_type in (cls.NEAR, cls.NEARBY, cls.NEAR_ME, cls.IN, cls.NULL):
            k = '{}_probability'.format(prep_phrase_type)
            prob = category_props.get(k, None)
            if prob is not None:
                values.append(prep_phrase_type)
                probs.append(prob)

        probs = cdf(probs)

        return weighted_choice(values, probs)
