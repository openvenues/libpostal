#include <stdio.h>

#include "greatest.h"

#include "../src/features.h"
#include "../src/scanner.h"
#include "../src/string_utils.h"

SUITE(libpostal_string_utils_tests);

TEST test_utf8_reverse(void) {
    char *s = "Bünderstraße";
    char *rev = utf8_reversed_string(s);
    if (rev == NULL) {
        FAIL();
    }

    ASSERT_STR_EQ(rev, "eßartsrednüB");
    free(rev);

    PASS();
}

TEST test_utf8proc_iterate_reversed(void) {
    char *s = "\xce\xa9\xcc\x93\xcd\x85";

    int32_t ch;
    ssize_t char_len;
    size_t idx = strlen(s);
    char_len = utf8proc_iterate_reversed((uint8_t *)s, idx, &ch);
    ASSERT_EQ(char_len, 2);
    ASSERT_EQ(ch, 837);
    idx -= char_len;

    char_len = utf8proc_iterate_reversed((uint8_t *)s, idx, &ch);
    ASSERT_EQ(char_len, 2);
    ASSERT_EQ(ch, 787);
    idx -= char_len;

    char_len = utf8proc_iterate_reversed((uint8_t *)s, idx, &ch);
    ASSERT_EQ(char_len, 2);
    ASSERT_EQ(ch, 937);
    idx -= char_len;

    char_len = utf8proc_iterate_reversed((uint8_t *)s, idx, &ch);
    ASSERT_EQ(char_len, 0);
    ASSERT_EQ(ch, -1);

    PASS();
}

TEST test_utf8_compare_ignore_separators(void) {
    char *str1 = "Bünderstraße";
    char *str2 = "Bünder-straße";

    size_t prefix = utf8_common_prefix_ignore_separators(str1, str2);

    ASSERT_EQ(prefix, 14);

    PASS();
}

TEST test_utf8_equal_ignore_separators(void) {
    char *str1 = "Bünderstraße  ";
    char *str2 = "Bünder-straße";

    bool equal = utf8_common_prefix_ignore_separators(str1, str2);
    ASSERT(equal);

    str1 = " Bünder-straße ";
    str2 = "Bünder straße";
    equal = utf8_common_prefix_ignore_separators(str1, str2);
    ASSERT(equal);

    str1 = "Bünder-straße-a";
    str2 = "Bünder straße aa";
    equal = utf8_common_prefix_ignore_separators(str1, str2);
    ASSERT_FALSE(equal);

    PASS();
}

TEST test_feature_array_add(void) {
    cstring_array *features = cstring_array_new();
    if (features == NULL) {
        FAIL();
    }
    feature_array_add(features, 3, "a", "foo", "blee");
    feature_array_add(features, 1, "b");

    ASSERT_EQ(cstring_array_num_strings(features), 2);

    char *feature = cstring_array_get_string(features, 0);
    size_t len = cstring_array_token_length(features, 0);

    if (feature == NULL) {
        cstring_array_destroy(features);
        FAIL();
    }

    ASSERT_STR_EQ(feature, "a|foo|blee");
    ASSERT_EQ(len, strlen(feature));

    feature = cstring_array_get_string(features, 1);
    len = cstring_array_token_length(features, 1);

    if (feature == NULL) {
        cstring_array_destroy(features);
        FAIL();
    }

    ASSERT_STR_EQ(feature, "b");
    ASSERT_EQ(len, strlen(feature));

    char **strings = cstring_array_to_strings(features);
    if (strings == NULL) {
        FAIL();
    }

    ASSERT_STR_EQ(strings[0], "a|foo|blee");
    free(strings[0]);
    ASSERT_STR_EQ(strings[1], "b");
    free(strings[1]);

    free(strings);

    PASS();
}

TEST test_char_array(void) {
    char_array *str = char_array_new();
    if (str == NULL) {
        FAIL();
    }
    char_array_cat(str, "Bürgermeister");
    char_array_cat(str, "|");
    char_array_cat_reversed(str, "straße");

    ASSERT_STR_EQ(str->a, "Bürgermeister|eßarts");

    char_array_cat_printf(str, " %d %s %.2f \t ", 1234, "onetwothreefour", 12.34);

    char *expected_output = "Bürgermeister|eßarts 1234 onetwothreefour 12.34 \t ";
    ASSERT_STR_EQ(str->a, expected_output);

    char *a = char_array_to_string(str);
    ASSERT_STR_EQ(a, expected_output);

    char *b = string_trim(a);
    ASSERT_STR_EQ(b, "Bürgermeister|eßarts 1234 onetwothreefour 12.34");

    free(a);
    free(b);

    str = char_array_new();
    #define SEPARATOR "|*|*|*|"

    char_array_add_joined(str, SEPARATOR, true, 3, "dictionaries" SEPARATOR, "foo", "bar");

    a = char_array_get_string(str);

    ASSERT_STR_EQ(a, "dictionaries|*|*|*|foo|*|*|*|bar");

    char_array_destroy(str);

    PASS();
}

TEST test_cstring_array(void) {
    size_t count = 0;
    cstring_array *array = cstring_array_split_no_copy(strdup("The|Low|End|Theory"), '|', &count);
    if (array == NULL) {
        FAIL();
    }
    ASSERT_EQ(count, 4);

    char *str = NULL;

    str = cstring_array_get_string(array, 0);
    if (str == NULL) {
        FAIL();
    }
    ASSERT_STR_EQ(str, "The");

    str = cstring_array_get_string(array, 1);
    if (str == NULL) {
        FAIL();
    }
    ASSERT_STR_EQ(str, "Low");

    str = cstring_array_get_string(array, 2);
    if (str == NULL) {
        FAIL();
    }
    ASSERT_STR_EQ(str, "End");

    str = cstring_array_get_string(array, 3);
    if (str == NULL) {
        FAIL();
    }
    ASSERT_STR_EQ(str, "Theory");

    cstring_array_destroy(array);

    PASS();
}

TEST test_string_tree(void) {
    string_tree_t *tree = string_tree_new();
    if (tree == NULL) {
        FAIL();
    }
    
    string_tree_finalize_token(tree);
    string_tree_add_string(tree, "Twenty-fifth");
    string_tree_add_string(tree, "Twentyfifth");
    string_tree_finalize_token(tree);
    string_tree_add_string(tree, "Bürgermeister");
    string_tree_add_string(tree, "Buergermeister");
    string_tree_add_string(tree, "Burgermeister");
    string_tree_finalize_token(tree);
    string_tree_add_string(tree, "Straße");
    string_tree_add_string(tree, "Strasse");
    string_tree_finalize_token(tree);

    ASSERT_EQ(tree->token_indices->n - 1, 4);

    ASSERT_EQ(string_tree_num_alternatives(tree, 0), 1);
    ASSERT_EQ(string_tree_num_alternatives(tree, 1), 2);
    ASSERT_EQ(string_tree_num_alternatives(tree, 2), 3);
    ASSERT_EQ(string_tree_num_alternatives(tree, 3), 2);

    string_tree_iterator_t *iter = string_tree_iterator_new(tree);

    if (iter == NULL) {
        string_tree_destroy(tree);
        FAIL();
    }
    size_t expected_num_tokens = 4;
    ASSERT_EQ(iter->num_tokens, expected_num_tokens);
    ASSERT_EQ(iter->remaining, 12);

    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 0);
    ASSERT_EQ(iter->path[2], 0);
    ASSERT_EQ(iter->path[3], 0);

    string_tree_iterator_next(iter);
    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 0);
    ASSERT_EQ(iter->path[2], 0);
    ASSERT_EQ(iter->path[3], 1);

    string_tree_iterator_next(iter);
    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 0);
    ASSERT_EQ(iter->path[2], 1);
    ASSERT_EQ(iter->path[3], 0);

    string_tree_iterator_next(iter);
    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 0);
    ASSERT_EQ(iter->path[2], 1);
    ASSERT_EQ(iter->path[3], 1);

    string_tree_iterator_next(iter);
    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 0);
    ASSERT_EQ(iter->path[2], 2);
    ASSERT_EQ(iter->path[3], 0);

    string_tree_iterator_next(iter);
    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 0);
    ASSERT_EQ(iter->path[2], 2);
    ASSERT_EQ(iter->path[3], 1);

    string_tree_iterator_next(iter);
    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 1);
    ASSERT_EQ(iter->path[2], 0);
    ASSERT_EQ(iter->path[3], 0);

    string_tree_iterator_next(iter);
    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 1);
    ASSERT_EQ(iter->path[2], 0);
    ASSERT_EQ(iter->path[3], 1);

    string_tree_iterator_next(iter);
    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 1);
    ASSERT_EQ(iter->path[2], 1);
    ASSERT_EQ(iter->path[3], 0);

    string_tree_iterator_next(iter);
    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 1);
    ASSERT_EQ(iter->path[2], 1);
    ASSERT_EQ(iter->path[3], 1);

    string_tree_iterator_next(iter);
    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 1);
    ASSERT_EQ(iter->path[2], 2);
    ASSERT_EQ(iter->path[3], 0);

    string_tree_iterator_next(iter);
    ASSERT_FALSE(string_tree_iterator_done(iter));
    ASSERT_EQ(iter->path[0], 0);
    ASSERT_EQ(iter->path[1], 1);
    ASSERT_EQ(iter->path[2], 2);
    ASSERT_EQ(iter->path[3], 1);

    string_tree_iterator_destroy(iter);
    string_tree_destroy(tree);

    PASS();
}

SUITE(libpostal_string_utils_tests) {
    RUN_TEST(test_utf8_reverse);
    RUN_TEST(test_utf8proc_iterate_reversed);
    RUN_TEST(test_utf8_compare_ignore_separators);
    RUN_TEST(test_feature_array_add);
    RUN_TEST(test_char_array);
    RUN_TEST(test_cstring_array);
    RUN_TEST(test_string_tree);
}


