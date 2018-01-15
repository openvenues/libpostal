// This program is for running multithreaded tests.
//
// Prerequisites: json-c (Ubuntu package libjson-c-dev)

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <json_object.h>
#include <json_tokener.h>
#include "libpostal.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct Args {
    char *input_file;
    int thread_count;
    int iterations;
    bool verbose;
};

struct Address {
    char *address;
    char *country;
};

static struct Args *get_args(int, char **);
static void args_destroy(struct Args *const);
static void print_usage(char const *const);
static struct Address **addresses_load(
        bool const,
        char const *const);
static char *read_file(char const *const);
static void addresses_destroy(struct Address **const);
static bool start_threaded_lookups(
    int const,
    int const,
    bool const,
    struct Address **const);
static void *thread(void *);
static bool run_lookups(bool const, struct Address **const);

int main(int argc, char **argv)
{
    struct Args *const args = get_args(argc, argv);
    if (!args) {
        return 1;
    }

    if (args->verbose) {
        printf("reading JSON...\n");
    }

    struct Address **const addresses = addresses_load(args->verbose,
            args->input_file);
    if (!addresses) {
        fprintf(stderr, "error loading addresses\n");
        args_destroy(args);
        return 1;
    }

    if (args->verbose) {
        printf("done reading JSON\n");
    }

    if (args->verbose) {
        printf("setting up libpostal...\n");
    }

    if (!libpostal_setup() || !libpostal_setup_parser() ||
            !libpostal_setup_language_classifier()) {
        fprintf(stderr, "libpostal setup failed\n");
        args_destroy(args);
        addresses_destroy(addresses);
        return 1;
    }

    if (args->verbose) {
        printf("done setting up libpostal\n");
    }

    if (!start_threaded_lookups(args->thread_count, args->iterations,
                args->verbose, addresses)) {
        fprintf(stderr, "error running lookups\n");
        args_destroy(args);
        addresses_destroy(addresses);
        libpostal_teardown();
        libpostal_teardown_parser();
        libpostal_teardown_language_classifier();
        return 1;
    }

    args_destroy(args);
    addresses_destroy(addresses);
    libpostal_teardown();
    libpostal_teardown_parser();
    libpostal_teardown_language_classifier();
    return 0;
}

static struct Args *get_args(int argc, char **argv)
{
    struct Args *const args = calloc(1, sizeof(struct Args));
    if (!args) {
        fprintf(stderr, "%s: error allocating argument memory: %s\n", __func__,
                strerror(errno));
        return NULL;
    }

    while (1) {
        int const opt = getopt(argc, argv, "f:t:i:vh\n");
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 'f':
            args->input_file = strdup(optarg);
            if (!args->input_file) {
                fprintf(stderr, "%s: error allocating memory: %s\n", __func__,
                        strerror(errno));
                args_destroy(args);
                return NULL;
            }
            break;
        case 't':
            args->thread_count = atoi(optarg);
            break;
        case 'i':
            args->iterations = atoi(optarg);
            break;
        case 'v':
            args->verbose = true;
            break;
        case 'h':
            print_usage(argv[0]);
            return NULL;
            break;
        default:
            print_usage(argv[0]);
            return NULL;
            break;
        }
    }

    if (!args->input_file) {
        fprintf(stderr, "you must provide an input file\n");
        args_destroy(args);
        return NULL;
    }

    if (args->thread_count <= 0) {
        fprintf(stderr, "thread count must be at least 1\n");
        args_destroy(args);
        return NULL;
    }

    if (args->iterations <= 0) {
        fprintf(stderr, "iterations must be at least 1\n");
        args_destroy(args);
        return NULL;
    }

    return args;
}

static void args_destroy(struct Args *const args)
{
    if (!args) {
        return;
    }

    if (args->input_file) {
        free(args->input_file);
    }
}

static void print_usage(char const *const program_name)
{
    fprintf(stderr, "Usage: %s <arguments>\n", program_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  -f <file>  Input file containing JSON with addresses to look up.        \n");
    fprintf(stderr, "  -t <#>     How many threads to run.\n");
    fprintf(stderr, "  -i <#>     Number of times to perform each lookup in each thread.\n");
    fprintf(stderr, "  -v         Enable verbose output.\n");
    fprintf(stderr, "  -h         This help.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "JSON should look like this:\n");
    fprintf(stderr, "  [ {\"address\": \"123 Main St.\", \"country\": \"CA\"}, ... ]\n");
}

static struct Address **addresses_load(
        bool const verbose,
        char const *const filename)
{
    if (!filename || strlen(filename) == 0) {
        fprintf(stderr, "%s: %s\n", __func__, strerror(EINVAL));
        return NULL;
    }

    char *const contents = read_file(filename);
    if (!contents) {
        fprintf(stderr, "%s: error reading file: %s\n", __func__, filename);
        return NULL;
    }

    json_object *const top_obj = json_tokener_parse(contents);
    if (!top_obj) {
        fprintf(stderr, "%s: invalid JSON\n", __func__);
        free(contents);
        return NULL;
    }

    free(contents);

    struct Address **const addresses = calloc(
            json_object_array_length(top_obj)+1, sizeof(struct Address *));
    if (!addresses) {
        fprintf(stderr, "%s: error allocating memory for addresses: %s\n",
                __func__, strerror(errno));
        json_object_put(top_obj);
        return NULL;
    }

    for (size_t i = 0; i < json_object_array_length(top_obj); i++) {
        json_object *const ele_obj = json_object_array_get_idx(top_obj, i);

        json_object *address_obj = NULL;
        if (!json_object_object_get_ex(ele_obj, "address", &address_obj)) {
            fprintf(stderr, "%s: `address' property not found\n", __func__);
            json_object_put(top_obj);
            addresses_destroy(addresses);
            return NULL;
        }

        char const *const address = json_object_get_string(address_obj);
        if (!address) {
            fprintf(stderr, "%s: null `address' property found\n", __func__);
            json_object_put(top_obj);
            addresses_destroy(addresses);
            return NULL;
        }

        json_object *country_obj = NULL;
        if (!json_object_object_get_ex(ele_obj, "country", &country_obj)) {
            fprintf(stderr, "%s: `country' property not found\n", __func__);
            json_object_put(top_obj);
            addresses_destroy(addresses);
            return NULL;
        }

        char const *const country = json_object_get_string(country_obj);
        if (!country) {
            fprintf(stderr, "%s: null `country' property found\n", __func__);
            json_object_put(top_obj);
            addresses_destroy(addresses);
            return NULL;
        }

        addresses[i] = calloc(1, sizeof(struct Address));
        if (!addresses[i]) {
            fprintf(stderr, "%s: error allocating address memory: %s\n",
                    __func__, strerror(errno));
            json_object_put(top_obj);
            addresses_destroy(addresses);
            return NULL;
        }
        addresses[i]->address = strdup(address);
        if (!addresses[i]->address) {
            fprintf(stderr, "%s: error allocating memory for address: %s\n",
                    __func__, strerror(errno));
            json_object_put(top_obj);
            addresses_destroy(addresses);
            return NULL;
        }
        addresses[i]->country = strdup(country);
        if (!addresses[i]->country) {
            fprintf(stderr, "%s: error allocating memory for country: %s\n",
                    __func__, strerror(errno));
            json_object_put(top_obj);
            addresses_destroy(addresses);
            return NULL;
        }

        if (verbose) {
            printf("address: [%s] country: [%s]\n", addresses[i]->address,
                    addresses[i]->country);
        }
    }

    json_object_put(top_obj);

    return addresses;
}

static char *read_file(char const *const filename)
{
    if (!filename || strlen(filename) == 0) {
        fprintf(stderr, "%s: %s\n", __func__, strerror(EINVAL));
        return NULL;
    }

    FILE *const fh = fopen(filename, "r");
    if (!fh) {
        fprintf(stderr, "%s: fopen(%s): %s\n", __func__, filename,
                strerror(errno));
        return NULL;
    }

    size_t const sz = 10240000;
    char *const buf = calloc(sz, sizeof(char));
    if (!buf) {
        fprintf(stderr, "%s: %s\n", __func__, strerror(errno));
        fclose(fh);
        return NULL;
    }

    (void) fread(buf, sizeof(char), sz, fh);
    if (!feof(fh)) {
        fprintf(stderr, "%s: error fully reading file: %s\n", __func__,
                filename);
        fclose(fh);
        free(buf);
        return NULL;
    }

    if (fclose(fh) != 0) {
        fprintf(stderr, "%s: error closing file: %s: %s\n", __func__, filename,
                strerror(errno));
        free(buf);
        return NULL;
    }

    return buf;
}

static void addresses_destroy(struct Address **const addresses)
{
    if (!addresses) {
        return;
    }

    for (size_t i = 0; addresses[i]; i++) {
        struct Address *const address = addresses[i];
        if (address->address) {
            free(address->address);
        }
        if (address->country) {
            free(address->country);
        }
        free(address);
    }

    free(addresses);
}

struct thread_info {
    pthread_t id;
    int iterations;
    bool verbose;
    struct Address **addresses;
};

static bool start_threaded_lookups(
    int const thread_count,
    int const iterations,
    bool const verbose,
    struct Address **const addresses)
{
    struct thread_info *const tinfo = calloc(thread_count,
                                             sizeof(struct thread_info));
    if (!tinfo) {
        fprintf(stderr, "%s: calloc(): %s\n", __func__, strerror(errno));
        return false;
    }

    for (int i = 0; i < thread_count; i++) {
        tinfo[i].iterations = iterations;
        tinfo[i].verbose = verbose;
        tinfo[i].addresses = addresses;

        if (pthread_create(&tinfo[i].id, NULL, &thread, &tinfo[i]) != 0) {
            fprintf(stderr, "%s: pthread_create() failed\n", __func__);
            free(tinfo);
            return false;
        }
    }

    for (int i = 0; i < thread_count; i++) {
        if (pthread_join(tinfo[i].id, NULL) != 0) {
            fprintf(stderr, "%s: pthread_join() failed\n", __func__);
            free(tinfo);
            return false;
        }
    }

    free(tinfo);

    return true;
}

static void *thread(void *arg)
{
    struct thread_info const *const tinfo = arg;
    if (!tinfo) {
        fprintf(stderr, "%s: %s\n", __func__, strerror(EINVAL));
        return NULL;
    }

    for (int i = 0; i < tinfo->iterations; i++) {
        if (!run_lookups(tinfo->verbose, tinfo->addresses)) {
            fprintf(stderr, "%s: run_lookups() failed\n", __func__);
            return NULL;
        }
    }

    return NULL;
}

static bool run_lookups(bool const verbose, struct Address **const addresses)
{
    for (size_t i = 0; addresses[i]; i++) {
        struct Address const *const address = addresses[i];
        if (verbose) {
            printf("address [%s] country [%s]:\n", address->address,
                    address->country);
        }

        libpostal_address_parser_options_t options =
            libpostal_get_address_parser_default_options();
        options.country = address->country;

        libpostal_address_parser_response_t *const parsed =
            libpostal_parse_address(address->address, options);
        if (!parsed) {
            fprintf(stderr, "error parsing: address [%s] country [%s]\n",
                    address->address, address->country);
            return false;
        }

        libpostal_normalize_options_t normalize_options =
            libpostal_get_default_options();

        for (size_t j = 0; j < parsed->num_components; j++) {
            if (verbose) {
                printf("  %s: %s\n", parsed->labels[j], parsed->components[j]);
            }

            size_t num_expansions = 0;
            char **const expansions = libpostal_expand_address(
                    parsed->components[j], normalize_options, &num_expansions);
            if (verbose) {
                for (size_t k = 0; k < num_expansions; k++) {
                    printf("    -> %s\n", expansions[k]);
                }
            }
            libpostal_expansion_array_destroy(expansions, num_expansions);
        }

        libpostal_address_parser_response_destroy(parsed);
    }

    return true;
}
