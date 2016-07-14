import argparse
import os
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.phrases.extraction import FrequentPhraseExtractor

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

    phrases = FrequentPhraseExtractor.from_file(open(args.filename),
                                                min_count=args.min_count,
                                                max_phrase_len=args.phrase_len)
    phrases.to_tsv(args.output_file, max_rows=args.max_rows)