import re
import os
import sys

this_dir = os.path.realpath(os.path.dirname(__file__))
sys.path.append(os.path.realpath(os.path.join(os.pardir, os.pardir)))

from geodata.encoding import safe_decode

class Scanner(object):
    '''
    Simple scanner implementation in Python using regular expression groups.
    Used to create dynamic lexicons for parsing various CLDR files
    without compiling a C scanner. Only C scanners are used at runtime
    '''

    def __init__(self, lexicon, flags=re.VERBOSE | re.I | re.UNICODE):
        self.lexicon = lexicon

        regexes, responses = zip(*lexicon)

        self.regex = re.compile(u'|'.join([u'({})'.format(safe_decode(r)) for r in regexes]), flags)
        self.responses = responses

    def scan(self, s):

        for match in self.regex.finditer(safe_decode(s)):
            i = match.lastindex
            response = self.responses[i - 1]
            token = match.group(i)
            if not callable(response):
                yield (token, response)
            else:
                responses = response(match, token)
                if responses is not None:
                    for response, token in responses:
                        yield (token, response)
