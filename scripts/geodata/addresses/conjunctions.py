import six
from geodata.addresses.config import address_config
from geodata.encoding import safe_decode
from geodata.math.sampling import weighted_choice


class Conjunction(object):
    DEFAULT_WHITESPACE_JOIN = ', '
    DEFAULT_NON_WHITESPACE_JOIN = ''
    key = 'and'

    @classmethod
    def join(cls, phrases, language, country=None):

        if not hasattr(phrases, '__iter__'):
            raise ValueError('Param phrases must be iterable')

        values, probs = address_config.alternative_probabilities(cls.key, language, country=country)
        phrase, props = weighted_choice(values, probs)

        whitespace = props.get('whitespace', True)
        whitespace_phrase = six.u(' ') if whitespace else six.u('')

        phrases = [safe_decode(p) for p in phrases]

        max_phrase_join = props.get('max_phrase_join', 2)
        if len(phrases) > max_phrase_join:
            default_join = safe_decode(props.get('default_join', cls.DEFAULT_WHITESPACE_JOIN if whitespace else cls.DEFAULT_NON_WHITESPACE_JOIN))
            prefix = default_join.join(phrases[:-max_phrase_join] + [six.u('')])
        else:
            prefix = six.u('')

        if whitespace:
            phrase = six.u('{}{}{}').format(whitespace_phrase, phrase, whitespace_phrase)
        joined_phrase = phrase.join(phrases[-max_phrase_join:])

        return six.u('').join([prefix, joined_phrase])
