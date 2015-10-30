import Levenshtein
from collections import OrderedDict


def soft_tfidf_similarity(tokens1, tokens2, idf,
                          sim_func=Levenshtein.jaro_winkler, theta=0.9,
                          common_word_threshold=100):
    '''
    Soft TFIDF is a hybrid distance function using both global statistics
    (inverse document frequency) and local similarity (Jaro-Winkler).

    For each token t1 in the first string, find the token t2 which is most
    similar to t1 in terms of the local distance function.

    The SoftTFIDF similarity is the dot product of the max token similarities
    and the cosine similarity of the TF-IDF vectors for all tokens where
    the max similarity is >= a given threshold theta.

    sim_func should return a number in the range (0, 1) inclusive and theta
    should be in the same range i.e. this would _not_ work for a metric like
    basic Levenshtein or Damerau-Levenshtein distance where we'd want the
    value to be below the threshold. Those metrics can be transformed into
    a (0, 1) measure.

    @param tokens1: normalized tokens of string 1 (list of strings only)
    @param tokens2: normalized tokens of string 2 (list of strings only)

    @param idf: IDFIndex from geodata.statistics.tf_idf
    @param sim_func: similarity function which takes 2 strings and returns
                     a number between 0 and 1
    @param theta: token-level threshold on sim_func's return value at
                  which point two tokens are considered "close"

    Reference:
    https://www.cs.cmu.edu/~pradeepr/papers/ijcai03.pdf
    '''

    token1_counts = OrderedDict()
    for k in tokens1:
        token1_counts[k] = token1_counts.get(k, 0) + 1

    token2_counts = OrderedDict()
    for k in tokens2:
        token2_counts[k] = token2_counts.get(k, 0) + 1

    tfidf1 = idf.tfidf_vector(token1_counts)
    tfidf2 = idf.tfidf_vector(token2_counts)

    total_sim = 0.0

    t1_len = len(token1_counts)
    t2_len = len(token2_counts)

    if t2_len < t1_len:
        token1_counts, token2_counts = token2_counts, token1_counts
        tfidf1, tfidf2 = tfidf2, tfidf1

    for i, t1 in enumerate(token1_counts):
        sim, j = max([(sim_func(t1, t2), j) for j, t2 in enumerate(token2_counts)])
        if sim >= theta:
            total_sim += sim * tfidf1[i] * tfidf2[j]

    return total_sim
