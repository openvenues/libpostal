import six

from collections import *
from marisa_trie import BytesTrie
from geodata.encoding import safe_encode, safe_decode

SENTINEL = None


class PhraseFilter(object):
    def __init__(self, phrases):
        if hasattr(phrases, 'items'):
            phrases = six.iteritems(phrases)
        vals = [(safe_decode(key), self.serialize(val)) for key, val in phrases]
        self.trie = BytesTrie(vals)

    serialize = staticmethod(safe_encode)
    deserialize = staticmethod(safe_decode)

    def filter(self, tokens):
        def return_item(item):
            return False, item, []

        if not tokens:
            return

        ent = []
        ent_tokens = []

        queue = deque(tokens + [(SENTINEL,) * 2])
        skip_until = 0

        trie = self.trie

        while queue:
            item = queue.popleft()
            t, c = item

            if t is not SENTINEL and trie.has_keys_with_prefix(u' '.join(ent_tokens + [t])):
                ent.append(item)
                ent_tokens.append(item[0])
            elif ent_tokens:
                res = trie.get(u' '.join(ent_tokens)) or None
                if res is not None:
                    yield (True, ent, map(self.deserialize, res))
                    queue.appendleft(item)
                    ent = []
                    ent_tokens = []
                elif len(ent_tokens) == 1:
                    yield return_item(ent[0])
                    ent = []
                    ent_tokens = []
                    queue.appendleft(item)
                else:
                    have_phrase = False

                    for i in xrange(len(ent) - 1, 0, -1):
                        remainder = ent[i:]
                        res = trie.get(u' '.join([e[0] for e in ent[:i]])) or None
                        if res is not None:
                            yield (True, ent[:i], map(self.deserialize, res))
                            have_phrase = True
                            break

                    if not have_phrase:
                        yield return_item(ent[0])

                    todos = list(remainder)
                    todos.append(item)
                    queue.extendleft(reversed(todos))

                    ent = []
                    ent_tokens = []
            elif t is not SENTINEL:
                yield return_item(item)
