VISIT, VISIT_EDGE, POST_VISIT = range(3)


def strongly_connected_components(graph):
    '''
    Find strongly connected components in a graph using iterative
    depth-first search.

    Based on:
    http://code.activestate.com/recipes/578507-strongly-connected-components-of-a-directed-graph/
    '''
    identified = set()
    stack = []
    index = {}
    boundaries = []

    for v in graph:
        if v not in index:
            todo = [(VISIT, v)]
            while todo:
                op, v = todo.pop()
                if op == VISIT:
                    index[v] = len(stack)
                    stack.append(v)
                    boundaries.append(index[v])
                    todo.append((POST_VISIT, v))
                    todo.extend([(VISIT_EDGE, w) for w in graph[v]])
                elif op == VISIT_EDGE:
                    if v not in index:
                        todo.append((VISIT, v))
                    elif v not in identified:
                        while index[v] < boundaries[-1]:
                            boundaries.pop()
                else:
                    # op == POST_VISIT
                    if boundaries[-1] == index[v]:
                        boundaries.pop()
                        scc = stack[index[v]:]
                        del stack[index[v]:]
                        identified.update(scc)
                        yield scc
