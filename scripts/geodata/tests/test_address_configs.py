import re
import six
import unittest

from geodata.addresses.entrances import *
from geodata.addresses.floors import *
from geodata.intersections.query import *
from geodata.addresses.po_boxes import *
from geodata.addresses.postcodes import *
from geodata.addresses.staircases import *
from geodata.addresses.units import *
from geodata.categories.query import *


invalid_phrase_re = re.compile(r'\b(None|False|True)\b')


class TestAddressConfigs(unittest.TestCase):
    def valid_phrase(self, phrase):
        return phrase is None or not invalid_phrase_re.search(phrase)

    def check_entrance_phrases(self, language, country=None):
        for i in xrange(1000):
            phrase = Entrance.phrase(Entrance.random(language, country=country), language, country=country)
            self.assertTrue(self.valid_phrase(phrase), six.u('phrase was: {}').format(phrase))

    def check_staircase_phrases(self, language, country=None):
        for i in xrange(1000):
            phrase = Entrance.phrase(Entrance.random(language, country=country), language, country=country)
            self.assertTrue(self.valid_phrase(phrase), six.u('phrase was: {}').format(phrase))

    def check_floor_phrases(self, language, country=None):
        for i in xrange(10000):
            phrase = Floor.phrase(Floor.random(language, country=country), language, country=country)
            self.assertTrue(self.valid_phrase(phrase), six.u('phrase was: {}').format(phrase))

    def check_unit_phrases(self, language, country=None):
        for i in xrange(10000):
            phrase = Unit.phrase(Unit.random(language, country=country), language, country=country)
            self.assertTrue(self.valid_phrase(phrase), six.u('phrase was: {}').format(phrase))

    def check_po_boxes(self, language, country=None):
        for i in xrange(1000):
            phrase = POBox.phrase(POBox.random(language, country=country), language, country=country)
            if phrase is None:
                break
            self.assertTrue(self.valid_phrase(phrase), six.u('phrase was: {}').format(phrase))

    def check_postcodes(self, language, country=None):
        for i in xrange(1000):
            phrase = PostCode.phrase('12345', language, country=country)
            if phrase is None:
                break
            self.assertTrue(self.valid_phrase(phrase), six.u('phrase was: {}').format(phrase))

    def check_intersection_phrases(self, language, country=None):
        for i in xrange(1000):
            phrase = Intersection.phrase(language, country=country)
            if phrase is None:
                break
            self.assertTrue(self.valid_phrase(phrase), six.u('phrase was: {}').format(phrase))

    def check_category_phrases(self, language, country=None):
        for i in xrange(1000):
            phrase = Category.phrase(language, 'amenity', 'restaurant', country=country)
            if phrase.category is None:
                break

    def check_config(self, language, country=None):
        print('Doing lang={}, country={}'.format(language, country))
        print('Checking entrances')
        self.check_entrance_phrases(language, country=country)
        print('Checking staircases')
        self.check_staircase_phrases(language, country=country)
        print('Checking floors')
        self.check_floor_phrases(language, country=country)
        print('Checking units')
        self.check_unit_phrases(language, country=country)
        print('Checking intersections')
        self.check_intersection_phrases(language, country=country)
        print('Checking categories')
        self.check_category_phrases(language, country=country)
        print('Checking PO boxes')
        self.check_po_boxes(language, country=country)
        print('Checking postcodes')
        self.check_postcodes(language, country=country)

    def test_configs(self):
        for lang, value in six.iteritems(address_config.address_configs):
            self.check_config(lang)
            for country in value.get('countries', []):
                self.check_config(lang, country)

if __name__ == '__main__':
    unittest.main()
