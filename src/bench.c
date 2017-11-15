#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include "libpostal.h"
#include "libpostal_config.h"
#include "log/log.h"
#include "scanner.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        log_error("Usage: test_libpostal string languages...\n");
        exit(EXIT_FAILURE);
    }

    char *str = argv[1];
    char *languages[argc - 2];
    for (int i = 0; i < argc - 2; i++) {
        char *arg = argv[i + 2];
        if (strlen(arg) >= LIBPOSTAL_MAX_LANGUAGE_LEN) {
            printf("arg %d was longer than a language code (%d chars). Make sure to quote the input string\n", i + 2, LIBPOSTAL_MAX_LANGUAGE_LEN - 1);
        }
        languages[i] = arg;
    }

    if (!libpostal_setup()) {
        exit(EXIT_FAILURE);
    }

    libpostal_normalize_options_t options = libpostal_get_default_options();

    options.num_languages = 1;
    options.languages = languages;

    size_t num_expansions;

    char **strings;
    char *normalized;

    int num_loops = 100000;

    token_array *tokens = tokenize(str);
    uint64_t num_tokens = tokens->n;
    token_array_destroy(tokens);

    clock_t t1 = clock();
    for (int i = 0; i < num_loops; i++) {
        strings = libpostal_expand_address(str, options, &num_expansions);
        libpostal_expansion_array_destroy(strings, num_expansions);
    }
    clock_t t2 = clock();

    double benchmark_time = (double)(t2 - t1) / CLOCKS_PER_SEC;
    printf("Benchmark time: %f\n", benchmark_time);
    double addresses_per_second = num_loops / benchmark_time;
    printf("addresses/s = %f\n", addresses_per_second);
    double tokens_per_second = (num_loops * num_tokens) / benchmark_time;
    printf("tokens/s = %f\n", tokens_per_second);
    libpostal_teardown();
}
