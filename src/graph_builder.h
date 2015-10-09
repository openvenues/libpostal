/*
graph_builder.h
---------------

For graphs it's sometimes impractical to assume that the vertices
will arrive in sorted order, which is required for constructing a
compressed sparse row (CSR) matrix. This is simply a coordinate matrix
(COO) style constructor where the rows/columns do not need to be
ordered or unique.

*/

#ifndef GRAPH_BUILDER_H
#define GRAPH_BUILDER_H

#include "collections.h"
#include "file_utils.h"
#include "graph.h"


typedef struct graph_edge {
    uint32_t v1;
    uint32_t v2;
} graph_edge_t;

VECTOR_INIT(graph_edge_array, graph_edge_t)


typedef struct graph_builder {
    graph_type_t type;
    size_t m;
    size_t n;
    bool fixed_rows;
    graph_edge_array *edges;
} graph_builder_t;

graph_builder_t *graph_builder_new(graph_type_t type, bool fixed_rows);
void graph_builder_destroy(graph_builder_t *self);

/*
Destroy the builder and return a graph.

Note: remove_duplicates=true requires sorting the indices. Can only preserve
edge ordering if we can guarantee there are no duplicates.
*/
graph_t *graph_builder_finalize(graph_builder_t *self, bool sort_edges, bool remove_duplicates);

/*
Add an edge. Order 

Reflexive edges not allowed.
*/
void graph_builder_add_edge(graph_builder_t *self, uint32_t v1, uint32_t v2);

#endif