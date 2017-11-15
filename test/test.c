#include "greatest.h"

SUITE_EXTERN(libpostal_expansion_tests);
SUITE_EXTERN(libpostal_parser_tests);
SUITE_EXTERN(libpostal_transliteration_tests);
SUITE_EXTERN(libpostal_numex_tests);
SUITE_EXTERN(libpostal_string_utils_tests);
SUITE_EXTERN(libpostal_trie_tests);
SUITE_EXTERN(libpostal_crf_context_tests);

GREATEST_MAIN_DEFS();


int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();

    RUN_SUITE(libpostal_expansion_tests);
    RUN_SUITE(libpostal_parser_tests);
    RUN_SUITE(libpostal_transliteration_tests);
    RUN_SUITE(libpostal_numex_tests);
    RUN_SUITE(libpostal_string_utils_tests);
    RUN_SUITE(libpostal_trie_tests);
    RUN_SUITE(libpostal_crf_context_tests);
    GREATEST_MAIN_END();
}
