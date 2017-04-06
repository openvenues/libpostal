import operator
import six

from geodata.graph.topsort import topsort


class ComponentDependencies(object):
    '''
    Declare an address component and its dependencies e.g.
    a house_numer cannot be used in the absence of a road name.
    '''

    component_bit_values = {}

    def __init__(self, graph):
        self.dependencies = {}

        self.all_values = long('1' * len(graph), 2)

        self.dependency_order = [c for c in topsort(graph)]

        for component, deps in six.iteritems(graph):
            self.dependencies[component] = self.component_bitset(deps) if deps else self.all_values

    def __getitem__(self, key):
        return self.dependencies.__getitem__(key)

    def __contains__(self, key):
        return self.dependencies.__contains__(key)

    @classmethod
    def get_component_bit_value(cls, name):
        val = cls.component_bit_values.get(name)
        if val is None:
            num_values = len(cls.component_bit_values)
            val = 1 << num_values
            cls.component_bit_values[name] = val
        return val

    @classmethod
    def component_bitset(cls, components):
        return reduce(operator.or_, [cls.get_component_bit_value(name) for name in components])
