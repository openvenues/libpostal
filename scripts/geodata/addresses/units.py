import random
import six

from geodata.addresses.config import address_config
from geodata.addresses.directions import RelativeDirection
from geodata.addresses.floors import Floor
from geodata.addresses.numbering import NumberedComponent, sample_alphabet, latin_alphabet
from geodata.encoding import safe_decode
from geodata.math.sampling import weighted_choice, zipfian_distribution, cdf


class Unit(NumberedComponent):
    # When we don't know the number of units, use a Zipfian distribution
    # to choose randomly between 1 and max_units with 1 being much more
    # likely than 2, etc.
    max_units = 99
    max_basements = 2
    numbered_units = range(1, max_units + 1) + range(0, -max_basements - 1, -1)
    unit_probs = zipfian_distribution(len(numbered_units), 0.7)
    unit_probs_cdf = cdf(unit_probs)

    num_digits = [2, 3, 4]
    num_digits_probs = zipfian_distribution(len(num_digits), 4.0)
    num_digits_cdf = cdf(num_digits_probs)

    # For use with floors e.g. #301 more common than #389
    positive_units = range(1, 10) + [0] + range(10, max_units + 1)
    positive_units_probs = zipfian_distribution(len(positive_units), 0.6)
    positive_units_cdf = cdf(positive_units_probs)

    # For use with letters e.g. A0 less common
    positive_units_letters = range(1, max_units + 1) + [0]
    positive_units_letters_probs = zipfian_distribution(len(positive_units_letters), 0.6)
    positive_units_letters_cdf = cdf(positive_units_probs)

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
        unit = weighted_choice(cls.positive_units, cls.positive_units_cdf)
        return '{}{}'.format(floor_number, safe_decode(unit).zfill(num_digits))

    @classmethod
    def random(cls, language, country=None, num_floors=None, num_basements=None):
        unit_props = address_config.get_property('units.alphanumeric', language, country=country)

        values = []
        probs = []

        for num_type in ('numeric', 'alpha', 'alpha_plus_numeric', 'numeric_plus_alpha'):
            key = '{}_probability'.format(num_type)
            prob = unit_props.get(key)
            if prob is not None:
                values.append(num_type)
                probs.append(prob)

        probs = cdf(probs)
        num_type = weighted_choice(values, probs)
        num_type_props = unit_props.get(num_type, {})

        if num_floors is not None:
            number = cls.for_floor(Floor.sample_positive_floors(num_floors))
        else:
            number = weighted_choice(cls.numbered_units, cls.unit_probs_cdf)

        if num_type == 'numeric':
            return safe_decode(number)
        else:
            alphabet = address_config.get_property('alphabet', language, country=country, default=latin_alphabet)
            letter = sample_alphabet(alphabet)
            if num_type == 'alpha':
                return safe_decode(letter)
            else:
                number = weighted_choice(cls.positive_units_letters, cls.positive_units_letters_cdf)

                whitespace_probability = unit_props.get('{}_whitespace_probability'.format(num_type))
                whitespace_phrase = six.u(' ') if whitespace_probability and random.random() < whitespace_probability else six.u('')

                if num_type == 'alpha_plus_numeric':
                    return six.u('{}{}{}').format(letter, whitespace_phrase, number)
                elif num_type == 'numeric_plus_alpha':
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
    def phrase(cls, unit, language, country=None, zone=None):
        if unit is not None:
            key = 'units.alphanumeric' if zone is None else 'units.zone.{}'.format(zone)

            add_direction = address_config.get_property('{}.add_direction'.format(key), language, country=country)
            if add_direction:
                unit = cls.add_direction(key, unit, language, country=country)

            return cls.numeric_phrase(key, safe_decode(unit), language,
                                      dictionaries=['unit_types_numbered'], country=country)
        else:
            key = 'units.standalone'
            add_direction = address_config.get_property('{}.add_direction'.format(key), language, country=country)
            if add_direction:
                unit = cls.add_direction(key, unit, language, country=country)
                if unit is not None:
                    return unit

            values, probs = address_config.alternative_probabilities(key, language,
                                                                     dictionaries=['unit_types_standalone'],
                                                                     country=country)
            if values is None:
                return None
            phrase, phrase_props = weighted_choice(values, probs)
            return phrase.title()
