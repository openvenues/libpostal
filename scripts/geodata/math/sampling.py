import bisect
import random
import sys

from geodata.math.floats import isclose, FLOAT_EPSILON


def weighted_choice(values, cdf):
    """Pick one of n values given a discrete cumulative distribution"""
    assert values and cdf, 'values and probabilities cannot be empty/None'
    assert len(values) == len(cdf), 'len(values) != len(probs)'
    assert all(p >= 0.0 and p <= (1.0 + FLOAT_EPSILON) for p in cdf), 'Probabilities not valid: {}'.format(cdf)

    x = random.random()
    i = bisect.bisect(cdf, x)
    return values[i]


def check_probability_distribution(probs):
    cumulative = 0.0
    for p in probs:
        assert p >= 0.0, 'Probabilities cannot be negative'
        assert p <= 1.0, 'Probabilities cannot be > 1.0'
        cumulative += p
    assert isclose(cumulative, 1.0), 'Probabilities must sum to 1: probs={}, cumulative={}'.format(probs, cumulative)


def cdf(probs):
    total = 0.0
    cumulative = [0.0] * len(probs)
    for i, p in enumerate(probs):
        total += p
        cumulative[i] = total

    return cumulative


def zipfian_distribution(n, b=1.0):
    """Distribution where the ith item's frequency is proportional to its rank"""
    frequencies = [1. / (i ** b) for i in xrange(1, n + 1)]
    total = sum(frequencies)
    return [f / total for f in frequencies]
