import random
import six

from geodata.addresses.config import address_config
from geodata.encoding import safe_decode
from geodata.math.sampling import weighted_choice, zipfian_distribution, cdf
from geodata.numbers.ordinals import ordinal_expressions
from geodata.text.tokenize import tokenize, token_types

alphabets = {}


def sample_alphabet(alphabet, b=1.5):
    '''
    Sample an "alphabet" using a Zipfian distribution (frequent items are very
    frequent, long tail of infrequent items). If we look at something like
    unit numbers, "Unit A" or "Unit B" are much more likely than "Unit X" or
    "Unit Z" simply because most dwellings only have a few units. Sampling
    letters from a Zipfian distribution rather than uniformly means that instead
    of every letter having the same likelihood (1/26), letters toward the beginning
    of the alphabet are much more likely to be selected. Letters toward the end can
    still be selected sometimes, but are not very likely.

    Note letters don't necessarily need to be sorted alphabetically, just in order
    of frequency.
    '''
    global alphabets
    alphabet = tuple(alphabet)
    if alphabet not in alphabets:
        probs = zipfian_distribution(len(alphabet), b)
        probs_cdf = cdf(probs)

        alphabets[alphabet] = probs_cdf

    probs_cdf = alphabets[alphabet]
    return weighted_choice(alphabet, probs_cdf)

latin_alphabet = [chr(i) for i in range(ord('A'), ord('Z') + 1)]


class NumericPhrase(object):
    key = None

    @classmethod
    def phrase(cls, number, language, country=None):
        values, probs = address_config.alternative_probabilities(cls.key, language, dictionaries=cls.dictionaries, country=country)
        if not values:
            return safe_decode(number) if number is not None else None

        phrase, phrase_props = weighted_choice(values, probs)

        values = []
        probs = []

        for num_type in ('numeric', 'numeric_affix'):
            key = '{}_probability'.format(num_type)
            prob = phrase_props.get(key, None)
            if prob is not None:
                values.append(num_type)
                probs.append(prob)

        probs = cdf(probs)

        if len(values) < 2:
            num_type = 'numeric'
        else:
            num_type = weighted_choice(values, probs)

        props = phrase_props[num_type]

        if num_type == 'numeric':
            # Numeric phrase the default is with whitespace e.g. "No 1"
            whitespace_default = True
        elif num_type == 'numeric_affix':
            phrase = props['affix']
            # Numeric affix default is no whitespace e.g. "#1"
            whitespace_default = False

        direction = props['direction']
        whitespace = props.get('whitespace', whitespace_default)
        if props.get('title_case', True):
            # Title case unless the config specifies otherwise
            phrase = phrase.title()

        if number is None:
            return phrase

        whitespace_phrase = six.u(' ') if whitespace else six.u('')
        # Phrase goes to the left of hte number
        if direction == 'left':
            return six.u('{}{}{}').format(phrase, whitespace_phrase, number)
        # Phrase goes to the right of the number
        elif direction == 'right':
            return six.u('{}{}{}').format(number, whitespace_phrase, phrase)
        # Need to specify a direction, otherwise return naked number
        else:
            return safe_decode(number)


class Number(NumericPhrase):
    key = 'numbers'
    dictionaries = ['number']


class NumberedComponent(object):
    @classmethod
    def numeric_phrase(cls, key, num, language, country=None, dictionaries=(), strict_numeric=False):
        is_alpha = False
        is_none = False
        if num is not None:
            try:
                num = int(num)
            except ValueError:
                try:
                    num = float(num)
                except ValueError:
                    if not all((c == token_types.NUMERIC) for t, c in tokenize(safe_decode(num))):
                        if strict_numeric:
                            return safe_decode(num)
                        is_alpha = True

        else:
            is_none = True

        # Pick a phrase given the probability distribution from the config
        values, probs = address_config.alternative_probabilities(key, language, dictionaries=dictionaries, country=country)
        if not values:
            return safe_decode(num) if not is_none else None

        phrase, phrase_props = weighted_choice(values, probs)

        values = []
        probs = []

        # Dictionaries are lowercased, so title case here
        if phrase_props.get('title_case', True):
            phrase = phrase.title()

        '''
        There are a few ways we can express the number itself

        1. Alias it as some standalone word like basement (for floor "-1")
        2. Use the number itself, so "Floor 2"
        3. Append/prepend an affix e.g. 2/F for second floor
        4. As an ordinal expression e.g. "2nd Floor"
        '''
        have_standalone = False
        have_null = False
        for num_type in ('standalone', 'null', 'numeric', 'numeric_affix', 'ordinal'):
            key = '{}_probability'.format(num_type)
            prob = phrase_props.get(key)
            if prob is not None:
                if num_type == 'standalone':
                    have_standalone = True
                elif num_type == 'null':
                    have_null = True
                values.append(num_type)
                probs.append(prob)
            elif num_type in phrase_props:
                values.append(num_type)
                probs.append(1.0)
                break

        if not probs or is_none:
            return phrase

        # If we're using something like "Floor A" or "Unit 2L", remove ordinal/affix items
        if is_alpha:
            values, probs = zip(*[(v, p) for v, p in zip(values, probs) if v in ('numeric', 'null', 'standalone')])
            total = sum(probs)
            probs = [p / total for p in probs]

        probs = cdf(probs)

        if len(values) < 2:
            if have_standalone:
                num_type = 'standalone'
            elif have_null:
                num_type = 'null'
            else:
                num_type = 'numeric'
        else:
            num_type = weighted_choice(values, probs)

        if num_type == 'standalone':
            return phrase
        elif num_type == 'null':
            return safe_decode(num)

        props = phrase_props[num_type]

        if phrase_props.get('number_abs_value', False):
            num = abs(num)

            if 'number_min_abs_value' in phrase_props and num < phrase_props['number_min_abs_value']:
                return phrase

            if phrase_props.get('number_subtract_abs_value'):
                num -= phrase_props['number_subtract_abs_value']

            num = safe_decode(num)

        # Do we add the numeric phrase e.g. Floor No 1
        add_number_phrase = props.get('add_number_phrase', False)
        if add_number_phrase and random.random() < props['add_number_phrase_probability']:
            num = Number.phrase(num, language, country=country)

        whitespace_default = True

        if num_type == 'numeric_affix':
            phrase = props['affix']
            if props.get('upper_case', True):
                phrase = phrase.upper()
            whitespace_default = False
        elif num_type == 'ordinal':
            num = ordinal_expressions.suffixed_number(num, language, gender=props.get('gender', None))

            if random.random() < props.get('null_phrase_probability', 0.0):
                return num

        direction = props['direction']
        whitespace = props.get('whitespace', whitespace_default)

        # Occasionally switch up if direction_probability is specified
        if random.random() > props.get('direction_probability', 1.0):
            if direction == 'left':
                direction = 'right'
            elif direction == 'right':
                direction = 'left'

        whitespace_phrase = six.u(' ') if whitespace else six.u('')
        # Phrase goes to the left of hte number
        if direction == 'left':
            return six.u('{}{}{}').format(phrase, whitespace_phrase, num)
        # Phrase goes to the right of the number
        elif direction == 'right':
            return six.u('{}{}{}').format(num, whitespace_phrase, phrase)
        # Need to specify a direction, otherwise return naked number
        else:
            return safe_decode(num)
