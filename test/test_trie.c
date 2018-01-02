#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"
#include "../src/scanner.h"
#include "../src/trie.h"
#include "../src/trie_search.h"

SUITE(libpostal_trie_tests);

static greatest_test_res test_trie_add_get(trie_t *trie, char *key, uint32_t data) {
    bool added = trie_add(trie, key, data);
    ASSERT(added);

    uint32_t trie_data;
    bool fetched = trie_get_data(trie, key, &trie_data);
    ASSERT(fetched);
    ASSERT_EQ(data, trie_data);

    PASS();
}

static greatest_test_res test_trie_setup(trie_t *trie) {
    CHECK_CALL(test_trie_add_get(trie, "st", 1));
    CHECK_CALL(test_trie_add_get(trie, "street", 2));
    CHECK_CALL(test_trie_add_get(trie, "st rt", 3));
    CHECK_CALL(test_trie_add_get(trie, "st rd", 3));
    CHECK_CALL(test_trie_add_get(trie, "state route", 4));
    CHECK_CALL(test_trie_add_get(trie, "maine", 5));

    PASS();
}


TEST test_trie(void) {
    trie_t *trie = trie_new();
    ASSERT(trie != NULL);
    CHECK_CALL(test_trie_setup(trie));

    char *input = "main st r 20";
    token_array *tokens = tokenize_keep_whitespace(input);
    phrase_array *phrases = trie_search_tokens(trie, input, tokens);

    ASSERT(phrases != NULL);
    ASSERT(phrases->n == 1);
    phrase_t phrase = phrases->a[0];
    ASSERT(phrase.start == 2);
    ASSERT(phrase.len == 1);

    phrase_array_destroy(phrases);
    token_array_destroy(tokens);
    trie_destroy(trie);

    PASS();
}

GREATEST_SUITE(libpostal_trie_tests) {
    RUN_TEST(test_trie);
}
