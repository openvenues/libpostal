import six

text_type = six.text_type
string_types = six.string_types
binary_type = six.binary_type


def safe_decode(value, encoding='utf-8', errors='strict'):
    if isinstance(value, text_type):
        return value

    if isinstance(value, (string_types, binary_type)):
        return value.decode(encoding, errors)
    else:
        return binary_type(value).decode(encoding, errors)


def safe_encode(value, incoming=None, encoding='utf-8', errors='strict'):
    if not isinstance(value, (string_types, binary_type)):
        return binary_type(value)

    if isinstance(value, text_type):
        return value.encode(encoding, errors)
    else:
        if hasattr(incoming, 'lower'):
            incoming = incoming.lower()
        if hasattr(encoding, 'lower'):
            encoding = encoding.lower()

        if value and encoding != incoming:
            value = safe_decode(value, encoding, errors)
            return value.encode(encoding, errors)
        else:
            return value
