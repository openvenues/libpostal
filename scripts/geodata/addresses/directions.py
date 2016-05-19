from geodata.addresses.config import address_config
from geodata.addresses.numbering import NumericPhrase
from geodata.math.sampling import weighted_choice


class RelativeDirection(NumericPhrase):
    key = 'directions'
    dictionaries = ['unit_directions']


class AnteroposteriorDirection(RelativeDirection):
    key = 'directions.anteroposterior'


class LateralDirection(RelativeDirection):
    key = 'directions.lateral'


class CardinalDirection(NumericPhrase):
    key = 'cardinal_directions'
    dictionaries = ['cardinal_directions']


class Direction(object):
    CARDINAL = 'cardinal'
    RELATIVE = 'relative'

    @classmethod
    def random(cls, language, country=None, cardinal_proability=0.5):
        values = [cls.CARDINAL, cls.RELATIVE]
        probs_cdf = [cardinal_proability, 1.0]

        choice = weighted_choice(values, probs_cdf)
        if choice == cls.CARDINAL:
            return CardinalDirection.phrase(None, language, country=country)
        else:
            return RelativeDirection.phrase(None, language, country=country)
