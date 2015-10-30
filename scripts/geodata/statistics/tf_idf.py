import math
from collections import defaultdict


class IDFIndex(object):
    finalized = False

    def __init__(self):
        self.idf_counts = defaultdict(int)
        self.N = 0

    def update(self, doc):
        if self.finalized or not doc:
            return

        for feature, count in doc.iteritems():
            self.idf_counts[feature] += 1

        self.N += 1

    def prune(self, min_count):
        self.idf_counts = {k: count for k, count in self.idf_counts.iteritems() if count >= min_count}

    def corpus_frequency(self, key):
        return self.idf_counts.get(key, 0)

    def tfidf_score(self, key, count=1):
        if count < 0:
            return 0.0

        idf_count = self.idf_counts.get(key, None)
        if idf_count is None:
            return 0.0
        return (math.log(count + 1.0) * (math.log(float(self.N) / idf_count)))

    def tfidf_vector(self, token_counts):
        tf_idf = [self.tfidf_score(t, count=c) for t, c in token_counts.iteritems()]
        norm = math.sqrt(sum((t ** 2 for t in tf_idf)))
        return [t / norm for t in tf_idf]
