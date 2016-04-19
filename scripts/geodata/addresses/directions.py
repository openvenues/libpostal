from geodata.addresses.numbering import NumericPhrase


class RelativeDirection(NumericPhrase):
    key = 'directions'
    dictionaries = ['unit_directions']


class CardinalDirection(NumericPhrase):
    key = 'cardinal_directions'
    dictionaries = ['cardinal_directions']
