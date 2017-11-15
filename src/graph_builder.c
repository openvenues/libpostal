#include "graph_builder.h"


#define ks_lt_graph_edge(a, b) ((a).v1 < (b).v1)
KSORT_INIT(graph_edge_array, graph_edge_t, ks_lt_graph_edge)

#define ks_lt_graph_edge_sort_vertices(a, b) (((a).v1 < (b).v1) || ((a).v1 == (b).v1 && (a).v2 < (b).v2))
KSORT_INIT(graph_edge_array_sort_vertices, graph_edge_t, ks_lt_graph_edge_sort_vertices)


void graph_builder_destroy(graph_builder_t *self) {
    if (self == NULL) return;

    if (self->edges != NULL) {
        graph_edge_array_destroy(self->edges);
    }

    free(self);
}

graph_builder_t *graph_builder_new(graph_type_t type, bool fixed_rows) {
    graph_builder_t *builder = malloc(sizeof(graph_builder_t));

    builder->type = type;
    builder->m = 0;
    builder->n = 0;
    builder->fixed_rows = fixed_rows;

    builder->edges = graph_edge_array_new();
    if (builder->edges == NULL) {
        graph_builder_destroy(builder);
        return NULL;
    }

    return builder;
}


static graph_t *graph_builder_build_edges(graph_builder_t *self, bool remove_duplicates) {
    graph_t *graph = graph_new(self->type);
    if (graph == NULL) return NULL;

    uint32_t last_vertex = 0;
    uint32_t last_edge = 0;

    for (int i = 0; i < self->edges->n; i++) {
        graph_edge_t edge = self->edges->a[i];
        if (edge.v1 > last_vertex) {
            for (uint32_t row = last_vertex; row < edge.v1; row++) {
                // Sorting is done prior to this
                graph_finalize_vertex_no_sort(graph);
            }
        }

        if (!remove_duplicates || i == 0 || edge.v1 != last_vertex || edge.v2 != last_edge) {
            graph_append_edge(graph, edge.v2);
        }
        last_vertex = edge.v1;
        last_edge = edge.v2;
    }

    graph_finalize_vertex_no_sort(graph);

    return graph;
}

graph_t *graph_builder_finalize(graph_builder_t *self, bool sort_edges, bool remove_duplicates) {
    graph_t *graph;
    if (remove_duplicates && !sort_edges) {
        sort_edges = true;
    }

    if (!sort_edges) {
        ks_introsort(graph_edge_array, self->edges->n, self->edges->a);
        graph = graph_builder_build_edges(self, remove_duplicates);
    } else {
        ks_introsort(graph_edge_array_sort_vertices, self->edges->n, self->edges->a);
        graph = graph_builder_build_edges(self, remove_duplicates);
    }

    graph->fixed_rows = self->fixed_rows;
    graph_set_size(graph);

    graph_builder_destroy(self);
    return graph;
}

void graph_builder_add_edge(graph_builder_t *self, uint32_t v1, uint32_t v2) {
    if (v1 == v2) return;
    graph_edge_t edge;

    if (self->type != GRAPH_UNDIRECTED || v2 > v1) {
        edge = (graph_edge_t) {v1, v2};
    } else {
        edge = (graph_edge_t) {v2, v1};
    }

    graph_edge_array_push(self->edges, edge);

    if (v1 >= self->m) {
        self->m = v1 + 1;
    }

    if (v2 >= self->n) {
        self->n = v2 + 1;
    }
}
