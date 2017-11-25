#include "trie.h"
#include <math.h>

/* 
* Maps the 256 characters (suitable for UTF-8 strings) to array indices
* ordered by frequency of usage in Wikipedia titles.
* In practice the order of the chars shouldn't matter for larger key sets
* but may save space for a small number of keys
*/
uint8_t DEFAULT_ALPHABET[] = {
32, 97, 101, 105, 111, 110, 114, 0, 116, 108, 115, 117, 104, 99, 100, 109,
103, 121, 83, 112, 67, 98, 107, 77, 65, 102, 118, 66, 80, 84, 41, 40,
119, 82, 72, 68, 76, 71, 70, 87, 49, 44, 78, 75, 69, 74, 73, 48,
195, 122, 45, 50, 57, 79, 86, 46, 120, 85, 106, 39, 56, 51, 52, 89,
128, 226, 147, 55, 53, 54, 197, 113, 196, 90, 169, 161, 81, 179, 58, 88,
173, 188, 141, 182, 153, 177, 38, 130, 135, 164, 159, 47, 168, 33, 186, 167,
129, 200, 131, 162, 155, 184, 163, 171, 160, 137, 132, 190, 133, 34, 225, 187,
165, 189, 176, 63, 201, 140, 154, 180, 151, 170, 145, 175, 43, 152, 150, 166,
158, 194, 198, 178, 144, 181, 148, 134, 136, 42, 185, 174, 156, 143, 172, 191,
142, 96, 59, 202, 139, 183, 64, 206, 157, 61, 146, 36, 37, 199, 149, 126,
229, 230, 204, 233, 231, 207, 138, 208, 232, 92, 227, 228, 209, 94, 224, 239,
217, 205, 221, 218, 211, 4, 8, 12, 16, 20, 24, 28, 60, 203, 215, 219,
223, 235, 243, 247, 251, 124, 254, 3, 7, 11, 15, 19, 23, 27, 31, 35,
192, 212, 216, 91, 220, 95, 236, 240, 244, 248, 123, 252, 127, 2, 6, 10,
14, 18, 22, 26, 30, 62, 193, 213, 237, 241, 245, 249, 253, 1, 5, 9,
13, 17, 21, 25, 29, 210, 214, 93, 222, 234, 238, 242, 246, 250, 125, 255
};


/*
Constructors
*/

static trie_t *trie_new_empty(uint8_t *alphabet, uint32_t alphabet_size) {
    trie_t *self = calloc(1, sizeof(trie_t));
    if (!self)
        goto exit_no_malloc;

    self->nodes = trie_node_array_new_size(DEFAULT_NODE_ARRAY_SIZE);
    if (!self->nodes)
        goto exit_trie_created;

    self->null_node = NULL_NODE;

    self->tail = uchar_array_new_size(1);
    if (!self->tail)
        goto exit_node_array_created;

    self->alphabet = malloc(alphabet_size);
    if (!self->alphabet)
        goto exit_tail_created;
    memcpy(self->alphabet, alphabet, alphabet_size);

    self->alphabet_size = alphabet_size;

    self->num_keys = 0;

    for (int i = 0; i < self->alphabet_size; i++) {
        self->alpha_map[alphabet[i]] = i;
        log_debug("setting alpha_map[%d] = %d\n", alphabet[i], i);
    }

    self->data = trie_data_array_new_size(1);
    if (!self->data)
        goto exit_alphabet_created;

    return self;

exit_alphabet_created:
    free(self->alphabet);
exit_tail_created:
    uchar_array_destroy(self->tail);
exit_node_array_created:
    trie_node_array_destroy(self->nodes);
exit_trie_created:
    free(self);
exit_no_malloc:
    return NULL;
}

trie_t *trie_new_alphabet(uint8_t *alphabet, uint32_t alphabet_size) {
    trie_t *self = trie_new_empty(alphabet, alphabet_size);
    if (!self)
        return NULL;

    trie_node_array_push(self->nodes, (trie_node_t){0, 0});
    // Circular reference  point for first and last free nodes in the linked list
    trie_node_array_push(self->nodes, (trie_node_t){-1, -1});
    // Root node
    trie_node_array_push(self->nodes, (trie_node_t){TRIE_POOL_BEGIN, 0});

    uchar_array_push(self->tail, '\0');
    // Since data indexes are negative integers, index 0 is not valid, so pad it
    trie_data_array_push(self->data, (trie_data_node_t){0, 0});

    return self;
}

trie_t *trie_new(void) {
    return trie_new_alphabet(DEFAULT_ALPHABET, sizeof(DEFAULT_ALPHABET));
}

inline bool trie_node_is_free(trie_node_t node) {
    return node.check < 0;
}

inline trie_node_t trie_get_node(trie_t *self, uint32_t index) {
    if ((index >= self->nodes->n) || index < ROOT_NODE_ID) return self->null_node;
    return self->nodes->a[index];
}

inline void trie_set_base(trie_t *self, uint32_t index, int32_t base) {
    log_debug("Setting base at %d to %d\n", index, base);
    self->nodes->a[index].base = base;
}

inline void trie_set_check(trie_t *self, uint32_t index, int32_t check) {
    log_debug("Setting check at %d to %d\n", index, check);
    self->nodes->a[index].check = check;
}


inline trie_node_t trie_get_root(trie_t *self) {
    return self->nodes->a[ROOT_NODE_ID];
}

inline trie_node_t trie_get_free_list(trie_t *self) {
    return self->nodes->a[FREE_LIST_ID];
}


/* 
* Private implementation
*/



static bool trie_extend(trie_t *self, uint32_t to_index) {
    uint32_t new_begin, i, free_tail;

    if (to_index <= 0 || TRIE_MAX_INDEX <= to_index)
        return false;

    if (to_index < self->nodes->n)
        return true;

    new_begin = (uint32_t)self->nodes->n;

    for (i = new_begin; i < to_index + 1; i++) {
        trie_node_array_push(self->nodes, (trie_node_t){-(i-1), -(i+1)});
    }

    trie_node_t free_list_node = trie_get_free_list(self);
    free_tail = -free_list_node.base;
    trie_set_check(self, free_tail, -new_begin);
    trie_set_base(self, new_begin, -free_tail);
    trie_set_check(self, to_index, -FREE_LIST_ID);
    trie_set_base(self, FREE_LIST_ID, -to_index);

    return true;
}

void trie_make_room_for(trie_t *self, uint32_t next_id) {
    if (next_id+self->alphabet_size >= self->nodes->n) {
        trie_extend(self, next_id+self->alphabet_size);
        log_debug("extended to %zu\n", self->nodes->n);
    }
}

static inline void trie_set_node(trie_t *self, uint32_t index, trie_node_t node) {
    log_debug("setting node, index=%d, node=(%d,%d)\n", index, node.base, node.check);
    self->nodes->a[index] = node;
}

static void trie_init_node(trie_t *self, uint32_t index) {
    int32_t prev, next;

    trie_node_t node = trie_get_node(self, index);
    prev = -node.base;
    next = -node.check;

    trie_set_check(self, prev, -next);
    trie_set_base(self, next, -prev);

}

static void trie_free_node(trie_t *self, uint32_t index) {
    int32_t i, prev;

    trie_node_t free_list_node = trie_get_free_list(self);
    trie_node_t node;
    i = -free_list_node.check;
    while (i != FREE_LIST_ID && i < index) {
        node = trie_get_node(self, i);
        i = -node.check;
    }

    node = trie_get_node(self, i);
    prev = -node.base;

    trie_set_node(self, index, (trie_node_t){-prev, -i});

    trie_set_check(self, prev, -index);
    trie_set_base(self, i, -index);
}


static bool trie_node_has_children(trie_t *self, uint32_t node_id) {
    uint32_t index;
    if (node_id > self->nodes->n)
        return false;
    trie_node_t node = trie_get_node(self, node_id);
    if (node.base < 0)
        return false;
    for (int i = 0; i < self->alphabet_size; i++) {
        unsigned char c = self->alphabet[i];
        index = trie_get_transition_index(self, node, c);
        if (index < self->nodes->n && (uint32_t)trie_get_node(self, index).check == node_id)
            return true;
    }
    return false;
}

static void trie_prune_up_to(trie_t *self, uint32_t p, uint32_t s) {
    log_debug("Pruning from %d to %d\n", s, p);
    log_debug("%d has_children=%d\n", s, trie_node_has_children(self, s));
    while (p != s && !trie_node_has_children(self, s)) {
        uint32_t parent = trie_get_node(self, s).check;
        trie_free_node(self, s);
        s = parent;
    }
}

static void trie_prune(trie_t *self, uint32_t s) {
    trie_prune_up_to(self, ROOT_NODE_ID, s);
}

static void trie_get_transition_chars(trie_t *self, uint32_t node_id, unsigned char *transitions, uint32_t *num_transitions) {
    uint32_t index;
    uint32_t j = 0;
    trie_node_t node = trie_get_node(self, node_id);
    for (int i = 0; i < self->alphabet_size; i++) {
        unsigned char c = self->alphabet[i];
        index = trie_get_transition_index(self, node, c);
        if (index < self->nodes->n && trie_get_node(self, index).check == node_id) {
            log_debug("adding transition char %c to index %d\n", c, j);
            transitions[j++] = c;
        }
    }

    *num_transitions = j;
}


static bool trie_can_fit_transitions(trie_t *self, uint32_t node_id, unsigned char *transitions, uint32_t num_transitions) {
    uint32_t i;
    uint32_t char_index, index;

    for (i = 0; i < num_transitions; i++) {
        unsigned char c = transitions[i];
        char_index = trie_get_char_index(self, c);
        index = node_id + char_index;
        trie_node_t node = trie_get_node(self, index);
        if (node_id > TRIE_MAX_INDEX - char_index || !trie_node_is_free(node)) {
            return false;
        }

    }
    return true;

}

static uint32_t trie_find_new_base(trie_t *self, unsigned char *transitions, uint32_t num_transitions) {
    uint32_t first_char_index = trie_get_char_index(self, transitions[0]);

    trie_node_t node = trie_get_free_list(self);
    uint32_t index = -node.check;

    while (index != FREE_LIST_ID && index < first_char_index + TRIE_POOL_BEGIN) {
        node = trie_get_node(self, index);
        index = -node.check;
    }  


    if (index == FREE_LIST_ID) {
        for (index = first_char_index + TRIE_POOL_BEGIN; ; index++) {
            if (!trie_extend(self, index)) {
                log_error("Trie index error extending to %d\n", index);
                return TRIE_INDEX_ERROR;
            }
            node = trie_get_node(self, index);
            if (node.check < 0) 
                break;
        }
    }

    // search for next free cell that fits the transitions
    while (!trie_can_fit_transitions(self, index - first_char_index, transitions, num_transitions)) {
        trie_node_t node = trie_get_node(self, index);
        if (-node.check == FREE_LIST_ID) {
            if (!trie_extend(self, (uint32_t) self->nodes->n + self->alphabet_size)) {
                log_error("Trie index error extending to %d\n", index);
                return TRIE_INDEX_ERROR;
            }
            node = trie_get_node(self, index);
        }

        index = -node.check;

    }

    return index - first_char_index;

}

static size_t trie_required_size(trie_t *self, uint32_t index) {
    size_t array_size = (size_t)self->nodes->m;
    // Make sure we have enough space in the array
    while (array_size < (TRIE_POOL_BEGIN+index)) {
        array_size *= 2;
    }
    return array_size;
}

static void trie_relocate_base(trie_t *self, uint32_t current_index, int32_t new_base) {
    log_debug("Relocating base at %d\n", current_index);
    uint32_t i;

    trie_make_room_for(self, new_base);

    trie_node_t old_node = trie_get_node(self, current_index);

    uint32_t num_transitions = 0;
    unsigned char transitions[self->alphabet_size];
    trie_get_transition_chars(self, current_index, transitions, &num_transitions);

    for (i = 0; i < num_transitions; i++) {
        unsigned char c = transitions[i];

        uint32_t char_index = trie_get_char_index(self, c);

        uint32_t old_index = old_node.base + char_index;
        uint32_t new_index = new_base + char_index;

        log_debug("old_index=%d\n", old_index);
        trie_node_t old_transition = trie_get_node(self, old_index);

        trie_init_node(self, new_index);
        trie_set_node(self, new_index, (trie_node_t){old_transition.base, current_index});

        /*
        *  All transitions out of old_index are now owned by new_index
        *  set check values appropriately
        */
        if (old_transition.base > 0) {  // do nothing in the case of a tail pointer
            for (uint32_t j = 0; j < self->alphabet_size; j++) {
                unsigned char c = self->alphabet[j];
                uint32_t index = trie_get_transition_index(self, old_transition, c);
                if (index < self->nodes->n && trie_get_node(self, index).check == old_index) {
                    trie_set_check(self, index, new_index);
                }
            }
        }

        // Free the node at old_index
        log_debug("freeing node at %d\n", old_index);
        trie_free_node(self, old_index);

    }

    trie_set_base(self, current_index, new_base);
}



/*
* Public methods
*/

inline uint32_t trie_get_char_index(trie_t *self, unsigned char c) {
    return self->alpha_map[(uint8_t)c] + 1;
}

inline uint32_t trie_get_transition_index(trie_t *self, trie_node_t node, unsigned char c) {
    uint32_t char_index = trie_get_char_index(self, c);
    return node.base + char_index;
}

inline trie_node_t trie_get_transition(trie_t *self, trie_node_t node, unsigned char c) {
   uint32_t index = trie_get_transition_index(self, node, c);

    if (index >= self->nodes->n) {
        return self->null_node;
    } else {
        return self->nodes->a[index];
    }

}

void trie_add_tail(trie_t *self, unsigned char *tail) {
    log_debug("Adding tail: %s\n", tail);
    for (; *tail; tail++) {
        uchar_array_push(self->tail, *tail);
    }

    uchar_array_push(self->tail, '\0');
}

void trie_set_tail(trie_t *self, unsigned char *tail, uint32_t tail_pos) {
    log_debug("Setting tail: %s at pos %d\n", tail, tail_pos);
    size_t tail_len = strlen((char *)tail);
    ssize_t num_appends = (ssize_t)(tail_pos + tail_len) - self->tail->n;
    int i = 0;

    // Pad with 0s if we're short
    if (num_appends > 0) {
        for (i = 0; i < num_appends; i++) {
            uchar_array_push(self->tail, '\0');
        }
    }

    for (i = tail_pos; *tail && i < self->tail->n; i++, tail++) {
        self->tail->a[i] = *tail;
    }
    self->tail->a[i] = '\0';
}


uint32_t trie_add_transition(trie_t *self, uint32_t node_id, unsigned char c) {
    uint32_t next_id;
    trie_node_t node, next;
    uint32_t new_base;


    node = trie_get_node(self, node_id);
    uint32_t char_index = trie_get_char_index(self, c);

    log_debug("adding transition %c to node_id %d + char_index %d, base=%d, check=%d\n", c, node_id, char_index, node.base, node.check);


    if (node.base > 0) {
        log_debug("node.base > 0\n");
        next_id = node.base + char_index;
        log_debug("next_id=%d\n", next_id);
        trie_make_room_for(self, next_id);

        next = trie_get_node(self, next_id);

        if (next.check == node_id) {
            return next_id;
        }

        log_debug("next.base=%d, next.check=%d\n", next.base, next.check);

        if (node.base > TRIE_MAX_INDEX - char_index || !trie_node_is_free(next)) {
            log_debug("node.base > TRIE_MAX_INDEX\n");
            uint32_t num_transitions;
            unsigned char transitions[self->alphabet_size];
            trie_get_transition_chars(self, node_id, transitions, &num_transitions);

            transitions[num_transitions++] = c;
            new_base = trie_find_new_base(self, transitions, num_transitions);

            trie_relocate_base(self, node_id, new_base);
            next_id = new_base + char_index;
        }

    } else {
        unsigned char transitions[] = {c};
        new_base = trie_find_new_base(self, transitions, 1);
        log_debug("Found base for transition char %c, base=%d\n", c, new_base);

        trie_set_base(self, node_id, new_base);
        next_id = new_base + char_index;
    }
    log_debug("init_node\n");
    trie_init_node(self, next_id);
    log_debug("setting check\n");
    trie_set_check(self, next_id, node_id);

    return next_id;
}

int32_t trie_separate_tail(trie_t *self, uint32_t from_index, unsigned char *tail, uint32_t data) {
    unsigned char c = *tail;
    int32_t index = trie_add_transition(self, from_index, c);

    if (*tail != '\0') tail++;

    log_debug("Separating node at index %d into char %c with tail %s\n", from_index, c, tail);
    trie_set_base(self, index, -1 * (int32_t)self->data->n);

    trie_data_array_push(self->data, (trie_data_node_t){(uint32_t)self->tail->n, data});
    trie_add_tail(self, tail);

    return index;
}

void trie_tail_merge(trie_t *self, uint32_t old_node_id, unsigned char *suffix, uint32_t data) {
    unsigned char c;
    uint32_t next_id;

    trie_node_t old_node = trie_get_node(self, old_node_id);
    int32_t old_data_index = -1*old_node.base;
    trie_data_node_t old_data_node = self->data->a[old_data_index];
    uint32_t old_tail_pos = old_data_node.tail;

    unsigned char *original_tail = self->tail->a + old_tail_pos;
    unsigned char *old_tail = original_tail;
    log_debug("Merging existing tail %s with new tail %s, node_id=%d\n", original_tail, suffix, old_node_id);

    size_t common_prefix = string_common_prefix((char *)old_tail, (char *)suffix);
    size_t old_tail_len = strlen((char *)old_tail);
    size_t suffix_len = strlen((char *)suffix);
    if (common_prefix == old_tail_len && old_tail_len == suffix_len) {
        log_debug("Key already exists, setting value to %d\n", data);
        self->data->a[old_data_index] = (trie_data_node_t) {old_tail_pos, data};
        return;
    }

    uint32_t node_id = old_node_id;
    log_debug("common_prefix=%zu\n", common_prefix);

    for (size_t i = 0; i < common_prefix; i++) {
        c = old_tail[i];
        log_debug("merge tail, c=%c, node_id=%d\n", c, node_id);
        next_id = trie_add_transition(self, node_id, c);
        if (next_id == TRIE_INDEX_ERROR) {
            goto exit_prune;
        }
        node_id = next_id;
    }

    uint32_t old_tail_index = trie_add_transition(self, node_id, *(old_tail+common_prefix));
    log_debug("old_tail_index=%d\n", old_tail_index);
    if (old_tail_index == TRIE_INDEX_ERROR) {
        goto exit_prune;
    }

    old_tail += common_prefix;
    if (*old_tail != '\0') {
        old_tail++;
    }

    trie_set_base(self, old_tail_index, -1 * old_data_index);
    trie_set_tail(self, old_tail, old_tail_pos);

    trie_separate_tail(self, node_id, suffix+common_prefix, data);
    return;

exit_prune:
    trie_prune_up_to(self, old_node_id, node_id);
    trie_set_tail(self, original_tail, old_tail_pos);
    return;
}



void trie_print(trie_t *self) {
    printf("Trie\n");
    printf("num_nodes=%zu, alphabet_size=%d\n\n", self->nodes->n, self->alphabet_size);
    for (size_t i = 0; i < self->nodes->n; i++) {
        int32_t base = self->nodes->a[i].base;
        int32_t check = self->nodes->a[i].check;

        int check_width = abs(check) > 9 ? (int) log10(abs(check))+1 : 1;
        int base_width = abs(base) > 9 ? (int) log10(abs(base))+1 : 1;
        if (base < 0) base_width++;
        if (check < 0) check_width++;
        int width = base_width > check_width ? base_width : check_width;
        printf("%*d ", width, base);
    }
    printf("\n");

    for (size_t i = 0; i < self->nodes->n; i++) {
        int32_t base = self->nodes->a[i].base;
        int32_t check = self->nodes->a[i].check;

        int check_width = abs(check) > 9 ? (int) log10(abs(check)) + 1 : 1;
        int base_width = abs(base) > 9 ? (int) log10(abs(base)) + 1 : 1;
        if (base < 0) base_width++;
        if (check < 0) check_width++;
        int width = base_width > check_width ? base_width : check_width;
        printf("%*d ", width, check);
    }
    printf("\n");
    for (size_t i = 0; i < self->tail->n; i++) {
        printf("%c ", self->tail->a[i]);
    }
    printf("\n");
    for (size_t i = 0; i < self->data->n; i++) {
        uint32_t tail = self->data->a[i].tail;
        uint32_t data = self->data->a[i].data;

        int tail_width = tail > 9 ? (int) log10(tail)+1 : 1;
        int data_width = data > 9 ? (int) log10(data)+1 : 1;

        int width = tail_width > data_width ? tail_width : data_width;
        printf("%*d ", width, tail);

    }
    printf("\n");
    for (size_t i = 0; i < self->data->n; i++) {
        uint32_t tail = self->data->a[i].tail;
        uint32_t data = self->data->a[i].data;

        int tail_width = tail > 9 ? (int) log10(tail)+1 : 1;
        int data_width = data > 9 ? (int) log10(data)+1 : 1;

        int width = tail_width > data_width ? tail_width : data_width;
        printf("%*d ", width, data);

    }
    printf("\n");

}

bool trie_add_at_index(trie_t *self, uint32_t node_id, char *key, size_t len, uint32_t data) {
    if (len == 2 && (key[0] == TRIE_SUFFIX_CHAR[0] || key[0] == TRIE_PREFIX_CHAR[0]) && key[1] == '\0') {
        return false;
    }

    unsigned char *ptr = (unsigned char *)key; 
    uint32_t last_node_id = node_id;
    trie_node_t last_node = trie_get_node(self, node_id);
    if (last_node.base == NULL_NODE_ID) {
        log_debug("last_node.base == NULL_NODE_ID, node_id = %d\n", node_id);
        return false;
    }
    
    trie_node_t node;

    // Walks node until prefix reached, including the trailing \0

    for (size_t i = 0; i < len; ptr++, i++, last_node_id = node_id, last_node = node) {

        log_debug("--- char=%d\n", *ptr);
        node_id = trie_get_transition_index(self, last_node, *ptr);
        log_debug("node_id=%d, last_node.base=%d, last_node.check=%d, char_index=%d\n", node_id, last_node.base, last_node.check, trie_get_char_index(self, *ptr));

        if (node_id != NULL_NODE_ID) {
            trie_make_room_for(self, node_id);
        }

        node = trie_get_node(self, node_id);
        log_debug("node.check=%d, last_node_id=%d, node.base=%d\n", node.check, last_node_id, node.base);

        if (node.check < 0 || (node.check != last_node_id)) {
            log_debug("last_node_id=%d, ptr=%s, tail_pos=%zu\n", last_node_id,  ptr, self->tail->n);
            trie_separate_tail(self, last_node_id, ptr, data);
            break;
        } else if (node.base < 0 && node.check == last_node_id) {
            log_debug("Case 3 insertion\n");
            trie_tail_merge(self, node_id, ptr + 1, data);
            break;
        }
    }

    self->num_keys++;
    return true;
}


inline bool trie_add(trie_t *self, char *key, uint32_t data) {
    size_t len = strlen(key);
    if (len == 0) return false;
    return trie_add_at_index(self, ROOT_NODE_ID, key, len + 1, data);
}

inline bool trie_add_len(trie_t *self, char *key, size_t len, uint32_t data) {
    return trie_add_at_index(self, ROOT_NODE_ID, key, len, data);
}

bool trie_add_prefix_at_index(trie_t *self, char *key, uint32_t start_node_id, uint32_t data) {
    size_t len = strlen(key);
    if (start_node_id == NULL_NODE_ID || len == 0) return false;

    trie_node_t start_node = trie_get_node(self, start_node_id);

    unsigned char prefix_char = TRIE_PREFIX_CHAR[0];

    uint32_t node_id = trie_get_transition_index(self, start_node, prefix_char);
    trie_node_t node = trie_get_node(self, node_id);
    if (node.check != start_node_id) {
        node_id = trie_add_transition(self, start_node_id, prefix_char);
    }

    bool success = trie_add_at_index(self, node_id, key, len, data);

    return success;
}

inline bool trie_add_prefix(trie_t *self, char *key, uint32_t data) {
    return trie_add_prefix_at_index(self, key, ROOT_NODE_ID, data);
}

bool trie_add_suffix_at_index(trie_t *self, char *key, uint32_t start_node_id, uint32_t data) {
    size_t len = strlen(key);
    if (start_node_id == NULL_NODE_ID || len == 0) return false;

    trie_node_t start_node = trie_get_node(self, start_node_id);

    unsigned char suffix_char = TRIE_SUFFIX_CHAR[0];

    uint32_t node_id = trie_get_transition_index(self, start_node, suffix_char);
    trie_node_t node = trie_get_node(self, node_id);
    if (node.check != start_node_id) {
        node_id = trie_add_transition(self, start_node_id, suffix_char);
    }

    char *suffix = utf8_reversed_string(key);

    bool success = trie_add_at_index(self, node_id, suffix, len, data);

    free(suffix);
    return success;

}

inline bool trie_add_suffix(trie_t *self, char *key, uint32_t data) {
    return trie_add_suffix_at_index(self, key, ROOT_NODE_ID, data);
}

bool trie_compare_tail(trie_t *self, char *str, size_t len, size_t tail_index) {
    if (tail_index >= self->tail->n) return false;

    unsigned char *current_tail = self->tail->a + tail_index;
    return strncmp((char *)current_tail, str, len) == 0;
}

inline trie_data_node_t trie_get_data_node(trie_t *self, trie_node_t node) {
    if (node.base >= 0) {
        return NULL_DATA_NODE;
    }
    int32_t data_index = -1*node.base;
    trie_data_node_t data_node = self->data->a[data_index];
    return data_node;
}

inline bool trie_set_data_node(trie_t *self, uint32_t index, trie_data_node_t data_node) {
    if (self == NULL || self->data == NULL || index >= self->data->n) return false;
    self->data->a[index] = data_node;
    return true;
}

inline bool trie_get_data_at_index(trie_t *self, uint32_t index,  uint32_t *data) {
     if (index == NULL_NODE_ID) return false;

     trie_node_t node = trie_get_node(self, index);
     trie_data_node_t data_node = trie_get_data_node(self, node);
     if (data_node.tail == 0) return false;
     *data = data_node.data;

     return true;    
}

inline bool trie_get_data(trie_t *self, char *key, uint32_t *data) {
     uint32_t node_id = trie_get(self, key);
     return trie_get_data_at_index(self, node_id, data);
}

inline bool trie_set_data_at_index(trie_t *self, uint32_t index, uint32_t data) {
    if (index == NULL_NODE_ID) return false;
     trie_node_t node = trie_get_node(self, index);
     trie_data_node_t data_node = trie_get_data_node(self, node);
     data_node.data = data;
     return trie_set_data_node(self, -1*node.base, data_node);

}

inline bool trie_set_data(trie_t *self, char *key, uint32_t data) {
     uint32_t node_id = trie_get(self, key);
     if (node_id == NULL_NODE_ID) {
        return trie_add(self, key, data);
     }

     return trie_set_data_at_index(self, node_id, data);
}

trie_prefix_result_t trie_get_prefix_from_index(trie_t *self, char *key, size_t len, uint32_t start_index, size_t tail_pos) {
    if (key == NULL) {
        return NULL_PREFIX_RESULT;
    }

    unsigned char *ptr = (unsigned char *)key;

    uint32_t node_id = start_index;
    trie_node_t node = trie_get_node(self, node_id);
    if (node.base == NULL_NODE_ID) {
        return NULL_PREFIX_RESULT;
    }

    uint32_t next_id = NULL_NODE_ID;

    bool original_node_no_tail = node.base >= 0;

    size_t i = 0;

    if (node.base >= 0) {
        // Include NUL-byte. It may be stored if this phrase is a prefix of a longer one
        for (i = 0; i < len; i++, ptr++, node_id = next_id) {
            next_id = trie_get_transition_index(self, node, *ptr);
            node = trie_get_node(self, next_id);

            if (node.check != node_id) {
                return NULL_PREFIX_RESULT;
            }

            if (node.base < 0) break;
        }
    } else {
        next_id = node_id;
        node = trie_get_node(self, node_id);
    }

    if (node.base < 0) {
        trie_data_node_t data_node = trie_get_data_node(self, node);

        char *query_tail = (*ptr && original_node_no_tail) ? (char *)ptr + 1 : (char *)ptr;
        size_t query_len = (*ptr && original_node_no_tail) ? len - i - 1 : len - i;

        if (data_node.tail != 0 && trie_compare_tail(self, query_tail, query_len, data_node.tail + tail_pos)) {
            return (trie_prefix_result_t){next_id, tail_pos + query_len};
        } else {
            return NULL_PREFIX_RESULT;

        }
    } else {
        return (trie_prefix_result_t){next_id, 0};
    }

    return NULL_PREFIX_RESULT;

}

trie_prefix_result_t trie_get_prefix_len(trie_t *self, char *key, size_t len) {
    return trie_get_prefix_from_index(self, key, len, ROOT_NODE_ID, 0);
}

trie_prefix_result_t trie_get_prefix(trie_t *self, char *key) {
    return trie_get_prefix_from_index(self, key, strlen(key), ROOT_NODE_ID, 0);
}

uint32_t trie_get_from_index(trie_t *self, char *word, size_t len, uint32_t i) {
    if (word == NULL) return NULL_NODE_ID;

    unsigned char *ptr = (unsigned char *)word;

    uint32_t node_id = i;
    trie_node_t node = trie_get_node(self, i);
    if (node.base == NULL_NODE_ID) return NULL_NODE_ID;

    uint32_t next_id;

    // Include NUL-byte. It may be stored if this phrase is a prefix of a longer one

    for (size_t i = 0; i < len + 1; i++, ptr++, node_id = next_id) {
        next_id = trie_get_transition_index(self, node, *ptr);
        node = trie_get_node(self, next_id);

        if (node.check != node_id) {
            return NULL_NODE_ID;
        }

        if (node.check == node_id && node.base < 0) {
            trie_data_node_t data_node = trie_get_data_node(self, node);

            char *query_tail = *ptr ? (char *) ptr + 1 : (char *) ptr;

            if (data_node.tail != 0 && trie_compare_tail(self, query_tail, strlen(query_tail) + 1, data_node.tail)) {
                return next_id;
            } else {
                return NULL_NODE_ID;
            }

        }

    }

    return next_id;

}

uint32_t trie_get_len(trie_t *self, char *word, size_t len) {
    return trie_get_from_index(self, word, len, ROOT_NODE_ID);
}

uint32_t trie_get(trie_t *self, char *word) {
    size_t word_len = strlen(word);
    return trie_get_from_index(self, word, word_len, ROOT_NODE_ID);
}


inline uint32_t trie_num_keys(trie_t *self) {
    if (self == NULL) return 0;
    return self->num_keys;
}

/*
Destructor
*/
void trie_destroy(trie_t *self) {
    if (!self)
        return;

    if (self->alphabet)
        free(self->alphabet);
    if (self->nodes)
        trie_node_array_destroy(self->nodes);
    if (self->tail)
        uchar_array_destroy(self->tail);
    if (self->data)
        trie_data_array_destroy(self->data);
    
    free(self);
}


/*
I/O methods
*/

bool trie_write(trie_t *self, FILE *file) {
    if (!file_write_uint32(file, TRIE_SIGNATURE) ||
        !file_write_uint32(file, self->alphabet_size)|| 
        !file_write_chars(file, (char *)self->alphabet, (size_t)self->alphabet_size) ||
        !file_write_uint32(file, self->num_keys) ||
        !file_write_uint32(file, (uint32_t)self->nodes->n)) {
        return false;
    }

    size_t i;
    trie_node_t node;

    for (i = 0; i < self->nodes->n; i++) {
        node = self->nodes->a[i];
        if (!file_write_uint32(file, (uint32_t)node.base) ||
            !file_write_uint32(file, (uint32_t)node.check)) {
            return false;
        }
    }

    if (!file_write_uint32(file, (uint32_t)self->data->n))
        return false;

    trie_data_node_t data_node;
    for (i = 0; i < self->data->n; i++) {
        data_node = self->data->a[i];
        if (!file_write_uint32(file, data_node.tail) ||
            !file_write_uint32(file, data_node.data)) {
            return false;
        }
    }

    if (!file_write_uint32(file, (uint32_t)self->tail->n))
        return false;

    if (!file_write_chars(file, (char *)self->tail->a, self->tail->n))
        return false;

    return true;
}


bool trie_save(trie_t *self, char *path) {
    FILE *file;
    bool result = false;

    file = fopen(path, "w+");
    if (!file)
        return false;

    result = trie_write(self, file);
    fclose(file);

    return result;
}

trie_t *trie_read(FILE *file) {
    uint32_t i;

    long save_pos = ftell(file);

    uint8_t alphabet[NUM_CHARS];

    uint32_t signature;

    if (!file_read_uint32(file, &signature)) {
        goto exit_file_read;
    }

    if (signature != TRIE_SIGNATURE) {
        goto exit_file_read;
    }

    uint32_t alphabet_size;

    if (!file_read_uint32(file, &alphabet_size)) {
        goto exit_file_read;
    }

    log_debug("alphabet_size=%d\n", alphabet_size);
    if (alphabet_size > NUM_CHARS)
        goto exit_file_read;

    if (!file_read_chars(file, (char *)alphabet, alphabet_size)) {
        goto exit_file_read;
    }

    trie_t *trie = trie_new_empty(alphabet, alphabet_size);
    if (!trie) {
        goto exit_file_read;
    }

    uint32_t num_keys;
    if (!file_read_uint32(file, &num_keys)) {
        goto exit_trie_created;
    }

    trie->num_keys = num_keys;

    uint32_t num_nodes;

    if (!file_read_uint32(file, &num_nodes)) {
        goto exit_trie_created;
    }

    log_debug("num_nodes=%d\n", num_nodes);
    trie_node_array_resize(trie->nodes, num_nodes);

    int32_t base;
    int32_t check;
    trie_node_t node;

    unsigned char *buf;
    size_t buf_size = num_nodes * sizeof(uint32_t) * 2;
    buf = malloc(buf_size);
    if (buf == NULL) {
        goto exit_trie_created;
    }

    unsigned char *buf_ptr;

    if (file_read_chars(file, (char *)buf, buf_size)) {
        buf_ptr = buf;
        for (i = 0; i < num_nodes; i++) {
            node.base = (int32_t)file_deserialize_uint32(buf_ptr);
            buf_ptr += sizeof(uint32_t);
            node.check = (int32_t)file_deserialize_uint32(buf_ptr);
            buf_ptr += sizeof(uint32_t);

            trie_node_array_push(trie->nodes, node);
        }
    }

    free(buf);
    buf = NULL;

    uint32_t num_data_nodes;
    if (!file_read_uint32(file, &num_data_nodes)) {
        goto exit_trie_created;
    }

    trie_data_array_resize(trie->data, num_data_nodes);
    log_debug("num_data_nodes=%d\n", num_data_nodes);

    trie_data_node_t data_node;

    buf_size = num_data_nodes * sizeof(uint32_t) * 2;
    buf = malloc(buf_size);
    if (buf == NULL) {
        goto exit_trie_created;
    }

    if (file_read_chars(file, (char *)buf, buf_size)) {
        buf_ptr = buf;
        for (i = 0; i < num_data_nodes; i++) {
            data_node.tail = (int32_t)file_deserialize_uint32(buf_ptr);
            buf_ptr += sizeof(uint32_t);
            data_node.data = (int32_t)file_deserialize_uint32(buf_ptr);
            buf_ptr += sizeof(uint32_t);

            trie_data_array_push(trie->data, data_node);
        }
    }

    free(buf);

    uint32_t tail_len;
    if (!file_read_uint32(file, &tail_len)) {
        goto exit_trie_created;
    }

    uchar_array_resize(trie->tail, tail_len);
    trie->tail->n = tail_len;

    if (!file_read_chars(file, (char *)trie->tail->a, tail_len)) {
        goto exit_trie_created;
    }

    return trie;

exit_trie_created:
    trie_destroy(trie);
exit_file_read:
    fseek(file, save_pos, SEEK_SET);
    return NULL;
}

trie_t *trie_load(char *path) {
    FILE *file;

    file = fopen(path, "rb");
    if (!file)
        return NULL;

    trie_t *trie = trie_read(file);

    fclose(file);

    return trie;
}
