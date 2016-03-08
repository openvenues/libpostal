import argparse
import csv
import os
import six
import sys

from collections import defaultdict, Counter
from itertools import izip, islice

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.text.tokenize import tokenize, token_types
from geodata.encoding import safe_encode


class FrequentPhraseExtractor(object):
    '''
    Extract common multi-word phrases from a file/iterator using the
    frequent itemsets method to keep memory usage low.
    '''
    WORD_TOKEN_TYPES = (token_types.WORD,
                        token_types.IDEOGRAPHIC_CHAR,
                        token_types.ABBREVIATION,
                        token_types.HANGUL_SYLLABLE,
                        token_types.ACRONYM)

    def __init__(self, min_count=5):
        self.min_count = min_count

        self.vocab = defaultdict(int)
        self.frequencies = defaultdict(int)
        self.train_words = 0

    def ngrams(self, words, n=2):
        for t in izip(*(islice(words, i, None) for i in xrange(n))):
            yield t

    def add_tokens(self, s):
        for t, c in tokenize(s):
            if c in self.WORD_TOKEN_TYPES:
                self.vocab[((t.lower(), c), )] += 1
                self.train_words += 1

    def create_vocab(self, f):
        for line in f:
            line = line.rstrip()
            if not line:
                continue
            self.add_tokens(line)
        self.prune_vocab()

    def prune_vocab(self):
        for k in self.vocab.keys():
            if self.vocab[k] < self.min_count:
                del self.vocab[k]

    def add_ngrams(self, s, n=2):
        sequences = []
        seq = []
        for t, c in tokenize(s):
            if c in self.WORD_TOKEN_TYPES:
                seq.append((t, c))
            elif seq:
                sequences.append(seq)
                seq = []
        if seq:
            sequences.append(seq)

        for seq in sequences:
            for gram in self.ngrams(seq, n=n):
                last_c = None

                prev_tokens = tuple([(t.lower(), c) for t, c in gram[:-1]])
                if prev_tokens in self.vocab:
                    t, c = gram[-1]
                    current_token = (t.lower(), c)

                    self.frequencies[(prev_tokens, current_token)] += 1

    def add_frequent_ngrams_to_vocab(self):
        for k, v in six.iteritems(self.frequencies):
            if v < self.min_count:
                continue
            prev, current = k
            self.vocab[prev + (current,)] = v

    def find_ngram_phrases(self, f, n=2):
        self.frequencies = defaultdict(int)
        for line in f:
            line = line.rstrip()
            if not line:
                continue
            self.add_ngrams(line, n=n)
        self.add_frequent_ngrams_to_vocab()
        self.frequencies = defaultdict(int)

    @classmethod
    def from_file(cls, filename, max_phrase_len=5, min_count=5):
        phrases = cls()

        print('Doing frequent words for {}'.format(filename))
        phrases.create_vocab(open(filename))

        for n in xrange(2, max_phrase_len + 1):
            print('Doing frequent ngrams, n={} for {}'.format(n, filename))
            phrases.find_ngram_phrases(open(filename), n=n)

        print('Done with {}'.format(filename))

        return phrases

    def to_tsv(self, filename, mode='w', max_rows=None):
        f = open(filename, mode)
        writer = csv.writer(f, delimiter='\t')
        for i, (k, v) in enumerate(Counter(self.vocab).most_common()):
            if max_rows is not None and i == max_rows:
                break

            gram = []
            for t, c in k:
                gram.append(t)
                if c != token_types.IDEOGRAPHIC_CHAR:
                    gram.append(six.text_type(' '))

            phrase = six.text_type('').join(gram)

            writer.writerow((safe_encode(phrase), safe_encode(len(k)), safe_encode(v)))

if __name__ == '__main__':
    '''
    Extract frequent words and multi-word phrases from an input file. The
    input file is expected to be a simple text file with one "sentence" per line.
    For OSM we typically use only street names and venue names.

    Phrases are considered to be sequences of n contiguous tokens given that all the
    tokens are of a "word" type according to the libpostal tokenizer, which implements
    the full Unicode TR-29 spec and will e.g. treat ideograms as individual tokens even
    though they are usually not separated by whitespace or punctuation.

    Using phrases is not only helpful for finding frequent patterns like "county road"
    or "roman catholic church" in English, but is also helpful e.g. in CJK languages for
    finding words that are longer than a single ideogram.

    Example usage:

    python extract_phrases.py en -o en.tsv --min-count=100
    find . -type f -size -10M | xargs -n1 basename | xargs -n1 --max-procs=4 -I{} python extract_phrases.py {} -o {}.tsv --min-count=5
    '''
    parser = argparse.ArgumentParser()

    parser.add_argument('filename', help='Input file')
    parser.add_argument('-o', '--output-file', required=True,
                        help='Output file')
    parser.add_argument('-p', '--phrase-len', default=5, type=int,
                        help='Maximum phrase length')
    parser.add_argument('-n', '--min-count', default=5, type=int,
                        help='Minimum count threshold')
    parser.add_argument('-m', '--max-rows', default=None, type=int)

    args = parser.parse_args()

    if args.phrase_len < 1:
        parser.error('--phrase-len must be >= 1')

    phrases = FrequentPhraseExtractor.from_file(args.filename,
                                                min_count=args.min_count,
                                                max_phrase_len=args.phrase_len)
    phrases.to_tsv(args.output_file, max_rows=args.max_rows)
