import itertools
import random
import six

from geodata.addresses.config import address_config
from geodata.addresses.directions import RelativeDirection, LateralDirection, AnteroposteriorDirection
from geodata.addresses.floors import Floor
from geodata.addresses.numbering import NumberedComponent, sample_alphabet, latin_alphabet
from geodata.configs.utils import nested_get
from geodata.encoding import safe_decode
from geodata.math.sampling import weighted_choice, zipfian_distribution, cdf
from geodata.text.utils import is_numeric_strict


class Unit(NumberedComponent):
    # When we don't know the number of units, use a Zipfian distribution
    # to choose randomly between 1 and max_units with 1 being much more
    # likely than 2, etc.
    max_units = 99
    max_basements = 2

    hundreds_numbered_units_tens = [range(101, 110) + [100],
                                    range(201, 210) + [200],
                                    range(301, 310) + [300],
                                    range(401, 410) + [400],
                                    range(501, 510) + [500],
                                    ]

    hundreds_numbered_units = [range(110, 200),
                               range(210, 300),
                               range(310, 400),
                               range(410, 500),
                               range(510, 600),
                               ]

    thousands_numbered_units = [range(1001, 1030) + [1000],
                                range(2001, 2030) + [2000],
                                range(3001, 3030) + [3000],
                                range(4001, 4030) + [4000],
                                range(5001, 5030) + [5000]
                                ]

    numbered_units = range(1, 10)
    numbered_units.extend(itertools.chain(*itertools.izip(*hundreds_numbered_units_tens)))
    numbered_units.extend(range(10, 100))
    numbered_units.extend(itertools.chain(*itertools.izip(*hundreds_numbered_units)))
    numbered_units.extend(itertools.chain(*itertools.izip(*thousands_numbered_units)))
    numbered_units.extend(range(10001, 10100) + [10000])
    numbered_units.append(0)
    numbered_units.extend(range(0, -max_basements - 1, -1))

    unit_probs = zipfian_distribution(len(numbered_units), 0.7)
    unit_probs_cdf = cdf(unit_probs)

    num_digits = [2, 3, 4]
    num_digits_probs = zipfian_distribution(len(num_digits), 4.0)
    num_digits_cdf = cdf(num_digits_probs)

    # For use with floors e.g. #301 more common than #389
    positive_units_floors = range(1, 10) + [0] + range(10, max_units + 1)
    positive_units_floors_probs = zipfian_distribution(len(positive_units_floors), 0.6)
    positive_units_floors_cdf = cdf(positive_units_floors_probs)

    # For basic positive units
    positive_units = range(1, max_units + 1)
    positive_units_probs = zipfian_distribution(len(positive_units), 0.6)
    positive_units_cdf = cdf(positive_units_probs)

    # For use with letters e.g. A0 less common
    positive_units_letters = range(1, max_units + 1) + [0]
    positive_units_letters_probs = zipfian_distribution(len(positive_units_letters), 0.6)
    positive_units_letters_cdf = cdf(positive_units_letters_probs)

    RESIDENTIAL = 'residential'
    COMMERCIAL = 'commercial'
    INDUSTRIAL = 'industrial'
    UNIVERSITY = 'university'

    @classmethod
    def sample_num_digits(cls):
        return weighted_choice(cls.num_digits, cls.num_digits_cdf)

    @classmethod
    def for_floor(cls, floor_number, num_digits=None):
        num_digits = num_digits if num_digits is not None else cls.sample_num_digits()
        unit = weighted_choice(cls.positive_units_floors, cls.positive_units_floors_cdf)
        return six.u('{}{}').format(floor_number, safe_decode(unit).zfill(num_digits))

    @classmethod
    def random(cls, language, country=None, num_floors=None, num_basements=None, floor=None):
        num_type, num_type_props = cls.choose_alphanumeric_type('units.alphanumeric', language, country=country)
        if num_type is None:
            return None

        use_floor_prob = address_config.get_property('units.alphanumeric.use_floor_probability', language, country=country, default=0.0)

        use_positive_numbers_prob = address_config.get_property('units.alphanumeric.use_positive_numbers_probability', language, country=country, default=0.0)

        if (num_floors is None and floor is None) or random.random() >= use_floor_prob:
            if random.random() >= use_positive_numbers_prob:
                number = weighted_choice(cls.numbered_units, cls.unit_probs_cdf)
            else:
                number = weighted_choice(cls.positive_units, cls.positive_units_cdf)
        else:
            if floor is None or not floor.isdigit():
                floor = Floor.random_int(language, country=country, num_floors=num_floors, num_basements=num_basements)

            floor_numbering_starts_at = address_config.get_property('levels.numbering_starts_at', language, country=country, default=0)
            ground_floor_starts_at = address_config.get_property('units.alphanumeric.use_floor_ground_starts_at', language, country=country, default=None)

            if ground_floor_starts_at is not None:
                try:
                    floor = int(floor)
                    if floor >= floor_numbering_starts_at:
                        floor -= floor_numbering_starts_at
                    floor += ground_floor_starts_at
                    floor = safe_decode(floor)
                except (TypeError, ValueError):
                    pass

            use_floor_affix_prob = address_config.get_property('units.alphanumeric.use_floor_numeric_affix_probability', language, country=country, default=0.0)
            if use_floor_affix_prob and random.random() < use_floor_affix_prob:
                floor_phrase = Floor.phrase(floor, language, country=country)
                # Only works if the floor phrase is strictly numeric e.g. "1" or "H1"
                if is_numeric_strict(floor_phrase):
                    unit = weighted_choice(cls.positive_units, cls.positive_units_cdf)

                    unit_num_digits = address_config.get_property('units.alphanumeric.use_floor_unit_num_digits', language, country=country, default=None)
                    if unit_num_digits is not None:
                        unit = safe_decode(unit).zfill(unit_num_digits)

                    return six.u('{}{}').format(floor_phrase, unit)

            floor_num_digits = address_config.get_property('units.alphanumeric.use_floor_floor_num_digits', language, country=country, default=None)
            if floor_num_digits is not None and floor.isdigit():
                floor = floor.zfill(floor_num_digits)

            number = cls.for_floor(floor)

        if num_type == cls.NUMERIC:
            return safe_decode(number)
        elif num_type == cls.HYPHENATED_NUMBER:
            number2 = weighted_choice(cls.positive_units, cls.positive_units_cdf)
            range_prob = float(address_config.get_property('units.alphanumeric.hyphenated_number.range_probability', language, country=country, default=0.5))
            direction = address_config.get_property('units.alphanumeric.hyphenated_number.direction', language, country=country, default='right')
            direction_prob = float(address_config.get_property('units.alphanumeric.hyphenated_number.direction_probability', language, country=country, default=0.0))

            if random.random() < direction_prob:
                direction = 'left' if direction == 'right' else 'right'

            direction_right = direction == 'right'

            if random.random() < range_prob:
                if direction_right:
                    number2 += number
                else:
                    number2 = max(0, number - number2)
            if direction == 'right':
                return u'{}-{}'.format(number, number2)
            else:
                return u'{}-{}'.format(number2, number)
        else:
            alphabet = address_config.get_property('alphabet', language, country=country, default=latin_alphabet)
            alphabet_probability = address_config.get_property('alphabet_probability', language, country=country, default=None)
            if alphabet_probability is not None and random.random() >= alphabet_probability:
                alphabet = latin_alphabet
            letter = sample_alphabet(alphabet)
            if num_type == cls.ALPHA:
                return safe_decode(letter)
            else:
                if num_floors is None:
                    number = weighted_choice(cls.positive_units_letters, cls.positive_units_letters_cdf)

                whitespace_probability = float(num_type_props.get('whitespace_probability', 0.0))
                hyphen_probability = float(num_type_props.get('hyphen_probability', 0.0))
                whitespace_phrase = u''
                r = random.random()
                if r < whitespace_probability:
                    whitespace_phrase = u' '
                elif r < (whitespace_probability + hyphen_probability):
                    whitespace_phrase = u'-' 

                if num_type == cls.ALPHA_PLUS_NUMERIC:
                    return six.u('{}{}{}').format(letter, whitespace_phrase, number)
                elif num_type == cls.NUMERIC_PLUS_ALPHA:
                    return six.u('{}{}{}').format(number, whitespace_phrase, letter)

    @classmethod
    def add_direction(cls, key, unit, language, country=None):
        add_direction_probability = address_config.get_property('{}.add_direction_probability'.format(key),
                                                                language, country=country, default=0.0)
        if not random.random() < add_direction_probability:
            return unit
        add_direction_numeric = address_config.get_property('{}.add_direction_numeric'.format(key),
                                                            language, country=country)
        try:
            unit = int(unit)
            integer_unit = True
        except (ValueError, TypeError):
            integer_unit = False

        if add_direction_numeric and integer_unit:
            return RelativeDirection.phrase(unit, language, country=country)
        elif not integer_unit:
            add_direction_standalone = address_config.get_property('{}.add_direction_standalone'.format(key),
                                                                   language, country=country)
            if add_direction_standalone:
                return RelativeDirection.phrase(None, language, country=country)

    @classmethod
    def add_quadrant(cls, key, unit, language, country=None):
        add_quadrant_probability = address_config.get_property('{}.add_quadrant_probability'.format(key),
                                                               language, country=country, default=0.0)
        if not random.random() < add_quadrant_probability:
            return unit
        add_quadrant_numeric = address_config.get_property('{}.add_quadrant_numeric'.format(key),
                                                           language, country=country)
        try:
            unit = int(unit)
            integer_unit = True
        except (ValueError, TypeError):
            integer_unit = False

        first_direction = address_config.get_property('{}.add_quadrant_first_direction'.format(key),
                                                      language, country=country)

        if first_direction == 'lateral':
            ordering = (LateralDirection, AnteroposteriorDirection)
        elif first_direction == 'anteroposterior':
            ordering = (AnteroposteriorDirection, LateralDirection)
        else:
            return unit

        if not integer_unit:
            add_quadrant_standalone = address_config.get_property('{}.add_quadrant_standalone'.format(key),
                                                                  language, country=country)
            if add_quadrant_standalone:
                unit = None
            else:
                return None

        last_num_type = None
        for i, c in enumerate(ordering):
            num_type, phrase, props = c.pick_phrase_and_type(unit, language, country=country)
            whitespace_default = num_type == c.NUMERIC or last_num_type == c.NUMERIC
            unit = c.combine_with_number(unit, phrase, num_type, props, whitespace_default=whitespace_default)
            last_num_type = num_type

        return unit

    @classmethod
    def phrase(cls, unit, language, country=None, zone=None):
        if unit is not None:
            key = 'units.alphanumeric' if zone is None else 'units.zones.{}'.format(zone)

            if not address_config.get_property(key, language, country=country):
                return None

            is_alpha = safe_decode(unit).isalpha()

            direction_unit = None
            add_direction = address_config.get_property('{}.add_direction'.format(key), language, country=country)
            if add_direction:
                direction_unit = cls.add_direction(key, unit, language, country=country)

            if direction_unit and direction_unit != unit:
                unit = direction_unit
                is_alpha = False
            else:
                add_quadrant = address_config.get_property('{}.add_quadrant'.format(key), language, country=country)
                if add_quadrant:
                    unit = cls.add_quadrant(key, unit, language, country=country)
                    is_alpha = False

            return cls.numeric_phrase(key, safe_decode(unit), language,
                                      dictionaries=['unit_types_numbered'], country=country, is_alpha=is_alpha)
        else:
            key = 'units.standalone'
            values, probs = address_config.alternative_probabilities(key, language,
                                                                     dictionaries=['unit_types_standalone'],
                                                                     country=country)
            if values is None:
                return None
            phrase, phrase_props = weighted_choice(values, probs)
            return phrase.title()
