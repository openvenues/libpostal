#include "graph.h"
#include "klib/ksort.h"

graph_t *graph_new_dims(graph_type_t type, uint32_t m, uint32_t n, size_t nnz, bool fixed_rows) {
    graph_t *graph = calloc(1, sizeof(graph_t));
    graph->m = m;
    graph->fixed_rows = fixed_rows;
    graph->n = n;
    graph->type = type;
    graph->indptr = uint32_array_new_size(m + 1);
    if (graph->indptr == NULL) {
        graph_destroy(graph);
        return NULL;
    }

    if (!fixed_rows) {
        uint32_array_push(graph->indptr, 0);
    }

    if (nnz > 0) {
        graph->indices = uint32_array_new_size(nnz);
    } else {
        graph->indices = uint32_array_new();
    }

    if (graph->indices == NULL) {
        graph_destroy(graph);
        return NULL;
    }

    return graph;
}

graph_t *graph_new(graph_type_t type) {
    return graph_new_dims(type, 0, 0, 0, false);
}

void graph_destroy(graph_t *self) {
    if (self == NULL) return;

    if (self->indptr != NULL) {
        uint32_array_destroy(self->indptr);
    }

    if (self->indices != NULL) {
        uint32_array_destroy(self->indices);
    }

    free(self);
}

inline void graph_set_size(graph_t *self) {
    if (self->type != GRAPH_BIPARTITE) {
        uint32_t max = self->m > self->n ? self->m : self->n;
        self->m = max;
        self->n = max;
    }
}

inline void graph_clear(graph_t *self) {
    uint32_array_clear(self->indptr);
    if (!self->fixed_rows) {
        uint32_array_push(self->indptr, 0);
    }

    uint32_array_clear(self->indices);
}

inline void graph_finalize_vertex_no_sort(graph_t *self) {
    uint32_array_push(self->indptr, (uint32_t)self->indices->n);
    if (!self->fixed_rows) {
        self->m++;
        graph_set_size(self);
    }
}

void graph_finalize_vertex(graph_t *self) {
    size_t start = 0;
    if (self->indptr->n > 0) {
        start = self->indptr->a[self->indptr->n - 1];
    }

    size_t end = self->indices->n;
    size_t len = end - start;

    if (len > 1) {
        ks_introsort(uint32_t, len, self->indices->a + start);
    }
    graph_finalize_vertex_no_sort(self);
}

bool graph_has_edge(graph_t *self, uint32_t i, uint32_t j) {
    if (i > self->m || j > self->n || i >= self->indptr->n - 1) return false;

    uint32_t *indptr = self->indptr->a;
    uint32_t *indices = self->indices->a;

    uint32_t row_start = indptr[i];
    uint32_t row_end = indptr[i + 1];
    uint32_t len = row_end - row_start;
    if (len == 0) return false;

    // Simple binary search, array is sorted
    ssize_t lo = (ssize_t)row_start;
    ssize_t hi = (ssize_t)row_end - 1;

    bool found = false;

    while (lo <= hi) {
        size_t mid = (lo + hi) / 2;
        uint64_t val = indices[mid];
        if (val < j) {
            lo = mid + 1;
        } else if (val > j) {
            hi = mid - 1;
        } else {
            found = true;
            break;
        }
    }

    return found;
}


inline void graph_append_edge(graph_t *self, uint32_t col) {
    uint32_array_push(self->indices, col);
    if (col >= self->n) self->n = col + 1;
    graph_set_size(self);
}

inline void graph_append_edges(graph_t *self, uint32_t *col, size_t n) {
    for (int i = 0; i < n; i++) {
        graph_append_edge(self, col[i]);
    }
    graph_finalize_vertex(self);
}

graph_t *graph_read(FILE *f) {
    graph_t *g = malloc(sizeof(graph_t));
    if (g == NULL) return NULL;

    g->indptr = NULL;
    g->indices = NULL;

    if (!file_read_uint32(f, &g->m) ||
        !file_read_uint32(f, &g->n) ||
        !file_read_uint8(f, (uint8_t *)&g->fixed_rows)) {
        goto exit_graph_allocated;
    }

    uint64_t len_indptr;

    if (!file_read_uint64(f, &len_indptr)) {
        goto exit_graph_allocated;
    }

    uint32_array *indptr = uint32_array_new_size(len_indptr);
    if (indptr == NULL) {
        goto exit_graph_allocated;
    }

    for (int i = 0; i < len_indptr; i++) {
        if (!file_read_uint32(f, indptr->a + i)) {
            goto exit_graph_allocated;
        }
    }
    indptr->n = (size_t)len_indptr;

    g->indptr = indptr;

    uint64_t len_indices;

    if (!file_read_uint64(f, &len_indices)) {
        goto exit_graph_allocated;
    }

    uint32_array *indices = uint32_array_new_size(len_indices);
    if (indices == NULL) {
        goto exit_graph_allocated;
    }

    for (int i = 0; i < len_indices; i++) {
        if (!file_read_uint32(f, indices->a + i)) {
            goto exit_graph_allocated;
        }
    }

    indices->n = (size_t)len_indices;

    g->indices = indices;
    
    return g;

exit_graph_allocated:
    graph_destroy(g);
    return NULL;
}


graph_t *graph_load(char *path) {
    FILE *f;

    if ((f = fopen(path, "rb")) == NULL) {
        return NULL;
    }

    graph_t *graph = graph_read(f);
    fclose(f);
    return graph;
}

bool graph_write(graph_t *self, FILE *f) {
    if (self == NULL || self->indptr == NULL || self->indices == NULL) {
        return false;
    }

    if (!file_write_uint32(f, self->m) ||
        !file_write_uint32(f, self->n) ||
        !file_write_uint8(f, (uint8_t)self->fixed_rows)) {
        return false;
    }

    uint64_t len_indptr = (uint64_t)self->indptr->n;

    if (!file_write_uint64(f, len_indptr)) {
        return false;
    }

    for (int i = 0; i < len_indptr; i++) {
        if (!file_write_uint32(f, self->indptr->a[i])) {
            return false;
        }
    }

    uint64_t len_indices = (uint64_t)self->indices->n;

    if (!file_write_uint64(f, len_indices)) {
        return false;
    }

    for (int i = 0; i < len_indices; i++) {
        if (!file_write_uint32(f, self->indices->a[i])) {
            return false;
        }
    }

    return true;
}


bool graph_save(graph_t *self, char *path) {
    FILE *f;
    if ((f = fopen(path, "wb")) == NULL) {
        return false;
    }
    bool status = graph_write(self, f);
    fclose(f);
    return status;
}


