#include "transliterate.h"
#include "file_utils.h"

#define TRANSLITERATION_TABLE_SIGNATURE 0xAAAAAAAA

static transliteration_table_t *trans_table = NULL;

transliteration_table_t *get_transliteration_table(void) {
    return trans_table;
}

transliterator_t *transliterator_new(char *name, uint8_t internal, uint32_t steps_index, size_t steps_length) {
    transliterator_t *trans = malloc(sizeof(transliterator_t));

    if (trans == NULL) {
        return NULL;
    }

    trans->name = strdup(name);
    trans->internal = internal;
    trans->steps_index = steps_index;
    trans->steps_length = steps_length;

    return trans;
}

void transliterator_destroy(transliterator_t *self) {
    if (self == NULL) return;
    if (self->name) {
        free(self->name);
    }
    free(self);
}


transliterator_t *get_transliterator(char *name) {
    transliteration_table_t *trans_table = get_transliteration_table();
    if (trans_table == NULL) {
        return NULL;
    }

    khiter_t k;
    k = kh_get(str_transliterator, trans_table->transliterators, name);
    return (k == kh_end(trans_table->transliterators)) ? kh_value(trans_table->transliterators, k) : NULL;
}


// N.B. stub
char *transliterate(char *trans_name, char *str) {
    return str;
}

void transliteration_table_destroy(void) {
    transliteration_table_t *trans_table = get_transliteration_table();
    if (trans_table == NULL) return;
    if (trans_table->trie) {
        trie_destroy(trans_table->trie);
    }

    if (trans_table->transliterators) {
        transliterator_t *trans;
        kh_foreach_value(trans_table->transliterators, trans, {
            transliterator_destroy(trans);
        })

        kh_destroy(str_transliterator, trans_table->transliterators);
    }

    if (trans_table->steps) {
        step_array_destroy(trans_table->steps);
    }

    if (trans_table->replacements) {
        transliteration_replacement_array_destroy(trans_table->replacements);
    }

    if (trans_table->replacement_strings) {
        cstring_array_destroy(trans_table->replacement_strings);
    }

    free(trans_table);
}

transliteration_table_t *transliteration_table_init(void) {
    transliteration_table_t *trans_table = get_transliteration_table();

    if (trans_table == NULL) {
        trans_table = malloc(sizeof(transliteration_table_t));

        trans_table->trie = trie_new();
        if (trans_table->trie == NULL) {
            goto exit_trans_table_created;
        }

        trans_table->transliterators = kh_init(str_transliterator);
        if (trans_table->transliterators == NULL) {
            goto exit_trans_table_created;
        }

        trans_table->steps = step_array_new();
        if (trans_table->steps == NULL) {
            goto exit_trans_table_created;
        }

        trans_table->replacements = transliteration_replacement_array_new();
        if (trans_table->replacements == NULL) {
            goto exit_trans_table_created;
        }

        trans_table->replacement_strings = cstring_array_new();
        if (trans_table->replacement_strings == NULL) {
            goto exit_trans_table_created;
        }
    }

    return trans_table;

exit_trans_table_created:
   transliteration_table_destroy();
   exit(1);
}

transliteration_step_t *transliteration_step_new(char *name, step_type_t type) {
    transliteration_step_t *self = malloc(sizeof(transliteration_step_t));

    if (self == NULL) {
        return NULL;
    }

    self->name = strdup(name);
    if (self->name == NULL) {
        transliteration_step_destroy(self);
    }

    self->type = type;
    return self;
}


void transliteration_step_destroy(transliteration_step_t *self) {
    if (self == NULL) {
        return;
    }

    if (self->name != NULL) {
        free(self->name);
    }

    free(self);
}


transliteration_replacement_t *transliteration_replacement_new(uint32_t string_index, int32_t move, group_capture_array *groups) {
    transliteration_replacement_t *replacement = malloc(sizeof(transliteration_replacement_t)); 

    if (replacement == NULL) {
        return NULL;
    }

    replacement->num_groups = groups == NULL ? 0 : groups->n;
    replacement->groups = groups;

    replacement->string_index = string_index;
    replacement->move = move;
    return replacement;

exit_replacement_created:
    transliteration_replacement_destroy(replacement);
    return NULL;

}

void transliteration_replacement_destroy(transliteration_replacement_t *self) {
    if (self == NULL) return;

    if (self->groups != NULL) {
        group_capture_array_destroy(self->groups);
    }

    free(self);
}

bool transliteration_table_add_transliterator(transliterator_t *trans) {
    if (trans_table == NULL) {
        return false;
    }

    int ret;
    khiter_t k = kh_put(str_transliterator, trans_table->transliterators, trans->name, &ret);
    kh_value(trans_table->transliterators, k) = trans;

    return true;
}

char *transliterator_replace_strings(trie_t *trie, cstring_array *replacements, char *input) {
    phrase_array *phrases;
    char_array *str;
    char *current = input;
    bool is_original = true;
    
    size_t len = strlen(input);

    // We may go through several rounds of replacements
    while (1) {
        phrases = trie_search(trie, current);
        if (!phrases) {
            break;
        } else {
            str = char_array_new_size(len);
            phrase_t phrase;
            int start = 0;
            int end = 0;
            for (int i = 0; i < phrases->n; i++) {
                phrase = phrases->a[i];
                end = phrase.start;
                char_array_append_len(str, input + start, end - start);
                char_array_append(str, cstring_array_get_token(replacements, phrase.data));
                start = phrase.start + phrase.len;
            }

            char_array_append_len(str, input + end, len - end);
            char_array_terminate(str);

            if (!is_original) {
                free(current);
            }

            // Destroys the char array itself, but not the string it holds
            current = char_array_to_string(str);
            is_original = false;
        }
    }

    return current;
}

transliterator_t *transliterator_read(FILE *f) {
    size_t trans_name_len;


    if (!file_read_int64(f, (int64_t *) &trans_name_len)) {
        return false;
    }

    char name[trans_name_len];

    if (!file_read_chars(f, name, trans_name_len)) {
        return false;
    }

    bool internal;
    if (!file_read_int8(f, (int8_t *)&internal)) {
        return false;
    }

    uint32_t steps_index;

    if (!file_read_int32(f, (int32_t *)&steps_index)) {
        return false;
    }


    uint32_t steps_length;

    if (!file_read_int32(f, (int32_t *)&steps_length)) {
        return false;
    }

    transliterator_t *trans =  transliterator_new(name, internal, steps_index, steps_length);
    return trans;
}

bool transliterator_write(transliterator_t *trans, FILE *f) {
    size_t trans_name_len = strlen(trans->name) + 1;
    if (!file_write_int64(f, trans_name_len) || 
        !file_write_chars(f, trans->name, trans_name_len)) {
        return false;
    }

    if (!file_write_int8(f, (int8_t)trans->internal)) {
        return false;
    }

    if (!file_write_int32(f, trans->steps_index)) {
        return false;
    }

    if (!file_write_int32(f, trans->steps_length)) {
        return false;
    }

    return true;
}

transliteration_step_t *transliteration_step_read(FILE *f) {
    size_t step_name_len;

    log_info("reading step\n");;

    transliteration_step_t *step = malloc(sizeof(transliteration_step_t));
    if (step == NULL) {
        return NULL;
    }

    if (!file_read_int32(f, (int32_t *) &step->type)) {
        goto exit_step_destroy;
    }
    if (!file_read_int64(f, (int64_t *) &step_name_len)) {
        goto exit_step_destroy;
    }

    char *name = malloc(step_name_len);
    if (name == NULL) {
        goto exit_step_destroy;
    }

    if (!file_read_chars(f, name, step_name_len)) {
        free(name);
        goto exit_step_destroy;
    }
    step->name = name;

    return step;

exit_step_destroy:
    free(step);
}

bool transliteration_step_write(transliteration_step_t *step, FILE *f) {
    if (!file_write_int32(f, step->type)) {
        return false;
    }

    // Include the NUL byte
    size_t step_name_len = strlen(step->name) + 1;

    if (!file_write_int64(f, step_name_len) || 
        !file_write_chars(f, step->name, step_name_len)) {
        return false;
    }

    return true;
}

bool group_capture_read(FILE *f, group_capture_t *group) {
    if (!file_read_int32(f, (int32_t *)&group->start)) {
        return false;
    }

    if (!file_read_int32(f, (int32_t *)&group->len)) {
        return false;
    }

    return true;
}

bool group_capture_write(group_capture_t group, FILE *f) {
    if (!file_write_int32(f, group.start) ||
        !file_write_int32(f, group.len)) {
        return false;
    }

    return true;
}

transliteration_replacement_t *transliteration_replacement_read(FILE *f) {
    uint32_t string_index;

    if (!file_read_int32(f, (int32_t *)&string_index)) {
        return NULL;
    }

    int32_t move;

    if (!file_read_int32(f, &move)) {
        return NULL;
    }

    size_t num_groups;

    if (!file_read_int64(f, (int64_t *)&num_groups)) {
        return NULL;
    }

    group_capture_array *groups = group_capture_array_new_size(num_groups);
    group_capture_t group;
    for (int i = 0; i < num_groups; i++) {
        if (!group_capture_read(f, &group)) {
            group_capture_array_destroy(groups);
            return NULL;
        }
        group_capture_array_push(groups, group);
    }

    return transliteration_replacement_new(string_index, move, groups);
}

bool transliteration_replacement_write(transliteration_replacement_t *replacement, FILE *f) {
    if (!file_write_int32(f, replacement->string_index)) {
        return false;
    }

    if (!file_write_int32(f, replacement->move)) {
        return false;
    }

    if (!file_write_int64(f, replacement->num_groups)) {
        return false;
    }

    group_capture_t group;

    for (int i = 0; i < replacement->num_groups; i++) {
        group = replacement->groups->a[i];
        if (!group_capture_write(group, f)) {
            return false;
        }
    }

    return true;

}

bool transliteration_table_read(FILE *f) {
    if (f == NULL) {
        return false;
    }

    uint32_t signature;

    log_info("Reading signature\n");

    if (!file_read_int32(f, (int32_t *)&signature) || signature != TRANSLITERATION_TABLE_SIGNATURE) {
        return false;
    }

    trans_table = transliteration_table_init();

    log_info("Table initialized\n");

    size_t num_transliterators;

    if (!file_read_int64(f, (int64_t *)&num_transliterators)) {
        goto exit_trans_table_load_error;
    }


    log_info("num_transliterators = %zu\n", num_transliterators);

    int i;

    transliterator_t *trans;

    for (i = 0; i < num_transliterators; i++) {
        trans = transliterator_read(f);
        if (trans == NULL) {
            log_error("trans was NULL\n");
            goto exit_trans_table_load_error;
        } else {
            log_info("read trans with name: %s\n", trans->name);
        }
        if (!transliteration_table_add_transliterator(trans)) {
            goto exit_trans_table_load_error;
        }
    }

    log_info("Read transliterators\n");

    size_t num_steps;

    if (!file_read_int64(f, (int64_t *)&num_steps)) {
        goto exit_trans_table_load_error;
    }

    log_info("num_steps = %llu\n", num_steps);

    step_array_resize(trans_table->steps, num_steps);

    log_info("resized\n");

    transliteration_step_t *step;

    for (i = 0; i < num_steps; i++) {
        step = transliteration_step_read(f);
        if (step == NULL) {
            goto exit_trans_table_load_error;
        }
        log_info("Read step with name %s and type %d\n", step->name, step->type);
        step_array_push(trans_table->steps, step);
    }

    log_info("Done with steps\n");

    transliteration_replacement_t *replacement;

    size_t num_replacements;

    if (!file_read_int64(f, (int64_t *)&num_replacements)) {
        goto exit_trans_table_load_error;
    }

    log_info("num_replacements = %zu\n", num_replacements);

    transliteration_replacement_array_resize(trans_table->replacements, num_replacements);

    log_info("resized\n");

    for (i = 0; i < num_replacements; i++) {
        replacement = transliteration_replacement_read(f);
        if (replacement == NULL) {
            goto exit_trans_table_load_error;
        }
        transliteration_replacement_array_push(trans_table->replacements, replacement);
    }

    log_info("Done with replacements\n");

    size_t num_replacement_tokens;

    if (!file_read_int64(f, (int64_t *)&num_replacement_tokens)) {
        goto exit_trans_table_load_error;
    }

    log_info("num_replacement_tokens = %zu\n", num_replacement_tokens);

    uint32_array_resize(trans_table->replacement_strings->indices, num_replacement_tokens);

    log_info("resized\n");

    uint32_t token_index;

    for (i = 0; i < num_replacement_tokens; i++) {
        if (!file_read_int32(f, (int32_t *)&token_index)) {
            goto exit_trans_table_load_error;
        }
        uint32_array_push(trans_table->replacement_strings->indices, token_index);
    }

    log_info("Done with replacement token indices\n");

    size_t replacement_strings_len;

    if (!file_read_int64(f, (int64_t *)&replacement_strings_len)) {
        goto exit_trans_table_load_error;
    }

    log_info("replacement_strings_len = %d\n", replacement_strings_len);

    char_array_resize(trans_table->replacement_strings->str, replacement_strings_len);

    log_info("resized\n");

    if (!file_read_chars(f, trans_table->replacement_strings->str->a, replacement_strings_len)) {
        goto exit_trans_table_load_error;
    }

    log_info("Read replacement_strings\n");

    trans_table->replacement_strings->str->n = replacement_strings_len;

    // Free the default trie
    trie_destroy(trans_table->trie);

    trans_table->trie = trie_read(f);
    log_info("Read trie\n");
    if (trans_table->trie == NULL) {
        goto exit_trans_table_load_error;
    }

    return true;

exit_trans_table_load_error:
    transliteration_table_destroy();
    return false;
}

bool transliteration_table_write(FILE *f) {
    if (f == NULL) {
        return false;
    }

    char *trans_name;
    transliterator_t *trans;

    if (!file_write_int32(f, TRANSLITERATION_TABLE_SIGNATURE)) {
        return false;
    }

    size_t num_transliterators = kh_size(trans_table->transliterators);

    if (!file_write_int64(f, (int64_t)num_transliterators)) {
        return false;
    }

    kh_foreach_value(trans_table->transliterators, trans, {
        if (!transliterator_write(trans, f)) {
            return false;
        }
    })

    transliteration_step_t *step;

    int i;

    size_t num_steps = trans_table->steps->n;

    if (!file_write_int64(f, (int64_t)num_steps)) {
        return false;
    }

    for (i = 0; i < num_steps; i++) {
        step = trans_table->steps->a[i];
        if (!transliteration_step_write(step, f)) {
            return false;
        }
    }

    size_t num_replacements = trans_table->replacements->n;

    if (!file_write_int64(f, (int64_t)num_replacements)) {
        return false;
    }

    transliteration_replacement_t *replacement;

    for (i = 0; i < trans_table->replacements->n; i++) {
        replacement = trans_table->replacements->a[i];
        if (!transliteration_replacement_write(replacement, f)) {
            return false;
        }
    }

    size_t replacement_tokens_len = trans_table->replacement_strings->indices->n;

    if (!file_write_int64(f, (int64_t) replacement_tokens_len)) {
        return false;
    }

    for (i = 0; i < replacement_tokens_len; i++) {
        if (!file_write_int32(f, (int32_t)trans_table->replacement_strings->indices->a[i])) {
            return false;
        }
    }

    size_t replacement_strings_len = trans_table->replacement_strings->str->n;

    if (!file_write_int64(f, (int64_t) replacement_strings_len)) {
        return false;
    }

    if (!file_write_chars(f, trans_table->replacement_strings->str->a, replacement_strings_len)) {
        return false;
    }

    if (!trie_write(trans_table->trie, f)) {
        return false;
    }

    return true;

}

bool transliteration_table_load(char *filename) {
    if (filename == NULL || trans_table != NULL) {
        return false;
    }

    FILE *f;

    if ((f = fopen(filename, "rb")) != NULL) {
        bool ret = transliteration_table_read(f);
        fclose(f);
        return ret;
    } else {
        return false;
    }
}


bool transliteration_table_save(char *filename) {
    if (trans_table == NULL || filename == NULL) {
        return false;
    }

    FILE *f;

    if ((f = fopen(filename, "wb")) != NULL) {
        bool ret = transliteration_table_write(f);
        fclose(f);
        return ret;
    } else {
        return false;
    }

}

bool transliteration_module_setup(char *filename) {
    if (filename == NULL && trans_table == NULL) {
        // Just init the table
        trans_table = transliteration_table_init();
        return true;
    } else if (trans_table == NULL) {
        return transliteration_table_load(filename);
    }

    return false;
}


void transliteration_module_teardown(void) {
    transliteration_table_destroy();
}

