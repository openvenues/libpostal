
def topsort(graph):
    '''
    Topological sort for a dependency graph, e.g.

    Usage:

    >>> graph = {
            'a': ['b'],
            'b': ['d'],
            'c': ['d', 'a'],
            'd': [],
        }
    >>> topsort(graph)

    Returns: ['d', 'b', 'a', 'c']

    '''
    todos = set(graph.keys())
    seen = set()
    result = []
    while todos:
        for key in todos:
            deps = graph[key]
            if len([d for d in deps if d in seen]) == len(deps):
                break
        else:
            raise Exception('Cycle: {}'.format(todos))
        todos.remove(key)
        result.append(key)
        seen.add(key)
    return result
