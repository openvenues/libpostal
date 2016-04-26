import six
from collections import Mapping


def recursive_merge(a, b):
    for k, v in six.iteritems(b):
        if isinstance(v, Mapping):
            existing = a.get(k, v)
            merged = recursive_merge(existing, v)
            a[k] = merged
        else:
            a[k] = b[k]
    return a


class DoesNotExist:
    pass


def nested_get(obj, keys):
    if len(keys) == 0:
        return obj
    try:
        for key in keys[:-1]:
            obj = obj.get(key, {})
            if not hasattr(obj, 'items'):
                return DoesNotExist
        key = keys[-1]
        return obj.get(key, DoesNotExist)
    except AttributeError:
        return DoesNotExist
