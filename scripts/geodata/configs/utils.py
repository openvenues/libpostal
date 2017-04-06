import six
from collections import Mapping


def recursive_merge(a, b):
    for k, v in six.iteritems(b):
        if isinstance(v, Mapping) and v:
            existing = a.get(k, v)
            merged = recursive_merge(existing, v)
            a[k] = merged
        else:
            a[k] = b[k]
    return a


class DoesNotExist:
    pass


def nested_get(obj, keys, default=DoesNotExist):
    if len(keys) == 0:
        return obj
    try:
        for key in keys[:-1]:
            obj = obj.get(key, {})
            if not hasattr(obj, 'items'):
                return default
        key = keys[-1]
        return obj.get(key, default)
    except AttributeError:
        return default


def alternative_probabilities(properties):
    if properties is None:
        return None, None

    probs = []
    alternatives = []

    if 'probability' in properties:
        prob = properties['probability']
        props = properties['default']
        probs.append(prob)
        alternatives.append(props)
    elif 'alternatives' not in properties and 'default' in properties:
        prob = 1.0
        props = properties['default']
        probs.append(prob)
        alternatives.append(props)
    elif 'alternatives' not in properties and 'default' not in properties:
        return None, None

    alts = properties.get('alternatives', [])
    for alt in alts:
        prob = alt.get('probability', 1.0 / len(alts))
        props = alt['alternative']
        probs.append(prob)
        alternatives.append(props)

    return alternatives, probs
