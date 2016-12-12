import six
from collections import defaultdict


class Aliases(object):
    def __init__(self, aliases):
        self.aliases = aliases
        self.priorities = {k: i for i, k in enumerate(aliases)}

    def key_priority(self, key):
        return self.priorities.get(key, len(self.priorities))

    def get(self, key, default=None):
        return self.aliases.get(key, default)

    def replace(self, components):
        replacements = defaultdict(list)
        values = {}
        for k in list(components):
            new_key = self.aliases.get(k)
            if new_key and new_key not in components:
                value = components.pop(k)
                values[k] = value
                replacements[new_key].append(k)

        for key, source_keys in six.iteritems(replacements):
            source_keys.sort(key=self.key_priority)
            value = values[source_keys[0]]
            components[key] = value
