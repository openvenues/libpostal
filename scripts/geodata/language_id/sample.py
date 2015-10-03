import random
import bisect

from collections import OrderedDict

'''
Top languages on the Interwebs. Not a probability distribution
as it doesn't sum to 1 and websites can be in more than one
language. Reference:

https://en.wikipedia.org/wiki/Languages_used_on_the_Internet#Content_languages_for_websites
'''
INTERNET_LANGUAGE_DISTRIBUTION = OrderedDict([
    ('en', 0.555),
    ('ru', 0.059),
    ('de', 0.058),
    ('ja', 0.05),
    ('es', 0.046),
    ('fr', 0.04),
    ('zh', 0.028),
    ('pt', 0.025),
    ('it', 0.019),
    ('pl', 0.017),
    ('tr', 0.015),
    ('nl', 0.013),
    ('fa', 0.009),
    ('ar', 0.008),
    ('ko', 0.007),
])


def cdf(probs):
    total = float(sum(probs))

    result = []
    cumulative = 0.0
    for w in probs:
        cumulative += w
        result.append(cumulative / total)
    return result


MOST_COMMON_INTERNET_LANGUAGES = INTERNET_LANGUAGE_DISTRIBUTION.keys()
INTERNET_LANGUAGES_CDF = cdf(INTERNET_LANGUAGE_DISTRIBUTION.values())


def sample_random_language(keys=MOST_COMMON_INTERNET_LANGUAGES,
                           cdf=INTERNET_LANGUAGES_CDF):
    assert len(keys) == len(cdf)

    sample = random.random()
    idx = bisect.bisect(cdf, sample)
    return keys[idx]
