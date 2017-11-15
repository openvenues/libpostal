FLOAT_EPSILON = 1e-09


def isclose(a, b, rel_tol=FLOAT_EPSILON, abs_tol=0.0):
    return abs(a - b) <= max(rel_tol * max(abs(a), abs(b)), abs_tol)
