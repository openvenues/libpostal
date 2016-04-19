import random
import six

from geodata.addresses.config import address_config
from geodata.addresses.numbering import NumericPhrase
from geodata.addresses.sampling import weighted_choice
from geodata.encoding import safe_decode


class RelativeDirection(NumericPhrase):
    key = 'directions'


class CardinalDirection(NumericPhrase):
    key = 'cardinal_directions'
