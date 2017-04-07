import csv
import os
import glob
import six
import sys

from collections import defaultdict
from collections import Counter

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(this_dir, os.pardir, os.pardir)))

from geodata.address_expansions.address_dictionaries import ADDRESS_EXPANSIONS_DIR
from geodata.osm.extract import *
from geodata.encoding import safe_encode


class VenueNames(object):
    def __init__(self, venues_filename):
        self.venues_filename = venues_filename
        self.all_chains = set()
        self.chain_canonical = {}

        for filename in glob.glob(os.path.join(ADDRESS_EXPANSIONS_DIR, '**', 'chains.txt')):
            f = open(filename)
            for line in f:
                line = line.rstrip()
                phrases = safe_decode(line).split(six.u('|'))
                self.all_chains |= set(phrases)
                canonical = phrases[0]
                for p in phrases[1:]:
                    self.chain_canonical[p] = canonical

        self.names = Counter()
        self.names_lower = Counter()
        self.names_cap = defaultdict(Counter)

    def count(self):
        i = 0
        for node_id, value, deps in parse_osm(self.venues_filename):
            name = value.get('name')
            if not name:
                continue
            self.names[name] += 1
            self.names_lower[name.lower()] += 1
            self.names_cap[name.lower()][name] += 1

            if i % 1000 == 0 and i > 0:
                print 'did', i
            i += 1

    def write_to_tsv(self, out_filename, min_threshold=5):
        writer = csv.writer(open(out_filename, 'w'), delimiter='\t')
        for k, v in self.names_lower.most_common():
            if v < min_threshold:
                break
            canonical = self.chain_canonical.get(k)
            if canonical:
                canonical = self.names_cap[canonical].most_common(1)[0][0]
            else:
                canonical = ''
            most_common_cap = self.names_cap[k].most_common(1)[0][0]
            writer.writerow((safe_encode(k),
                             safe_encode(most_common_cap),
                             safe_encode(canonical),
                             safe_encode(1) if k in self.all_chains else '',
                             safe_encode(v)))

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('Usage: python chains_tsv.py infile outfile')
        sys.exit(1)
    input_file = sys.argv[1]
    output_file = sys.argv[2]

    names = VenueNames(input_file)
    names.count()
    names.write_to_tsv(output_file)
