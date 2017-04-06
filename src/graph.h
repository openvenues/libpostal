/*
graph.h
-------

Graph stored as a compressed sparse row (CSR) matrix with no values.

This is a specialization of sparse matrices suitable for cases
where we only need to know that two nodes are connected and will
typically be iterating row-by-row (get all edges for vertex v).
By default it stores bipartite graphs

Essentially this can be viewed as a sparse matrix where all
of the non-zero values are 1.

See sparse_matrix.h for more details.

Currently we're not implementing edge types, graph traversal, etc.
*/

#ifndef GRAPH_H
#define GRAPH_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "collections.h"
#include "file_utils.h"
#include "vector.h"
#include "vector_math.h"

typedef enum {
    GRAPH_DIRECTED,
    GRAPH_UNDIRECTED,
    GRAPH_BIPARTITE
} graph_type_t;

typedef struct {
    graph_type_t type;
    uint32_t m;
    uint32_t n;
    bool fixed_rows;
    uint32_array *indptr;
    uint32_array *indices;
} graph_t;


graph_t *graph_new_dims(graph_type_t type, uint32_t m, uint32_t n, size_t nnz, bool fixed_rows);
graph_t *graph_new(graph_type_t type);
void graph_destroy(graph_t *self);

void graph_set_size(graph_t *self);

void graph_clear(graph_t *self);

void graph_append_edge(graph_t *self, uint32_t col);
void graph_append_edges(graph_t *self, uint32_t *col, size_t n);

void graph_finalize_vertex_no_sort(graph_t *self);
void graph_finalize_vertex(graph_t *self);

bool graph_has_edge(graph_t *self, uint32_t i, uint32_t j);

bool graph_write(graph_t *self, FILE *f);
bool graph_save(graph_t *self, char *path);
graph_t *graph_read(FILE *f);
graph_t *graph_load(char *path);

#define graph_foreach_row(g, row_var, index_var, length_var, code) {            \
    uint32_t _row_start = 0, _row_end = 0;                                      \
    uint32_t *_indptr = g->indptr->a;                                           \
    size_t _m = g->indptr->n - 1;                                               \
                                                                                \
    for (uint32_t _i = 0; _i < _m; _i++) {                                      \
        (row_var) = _i;                                                         \
        _row_start = _indptr[_i];                                               \
        _row_end = _indptr[_i + 1];                                             \
        (index_var) = _row_start;                                               \
        (length_var) = _row_end - _row_start;                                   \
        code;                                                                   \
    }                                                                           \
}

#define graph_foreach(g, row_var, col_var, code) {                              \
    uint32_t *_indices = g->indices->a;                                         \
    uint32_t _index, _length;                                                   \
    graph_foreach_row(g, row_var, _index, _length, {                            \
        if (_length == 0) continue;                                             \
        for (uint32_t _j = _index; _j < _index + _length; _j++) {               \
            (col_var) = _indices[_j];                                           \
            code;                                                               \
        }                                                                       \
    })                                                                          \
}   

#endif
