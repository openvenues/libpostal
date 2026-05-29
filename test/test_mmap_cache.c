#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "greatest.h"
#include "../src/file_utils.h"

SUITE(libpostal_mmap_cache_tests);

#ifndef _WIN32

#define TEST_MMAP_MAGIC   0x54455354u  /* "TEST" */
#define TEST_MMAP_VERSION 1u

typedef struct {
    uint32_t a;
    uint32_t b;
} test_type_header_t;

static greatest_test_res mmap_cache_roundtrip_at(const char *cache_path, file_mmap_src_id_t src, uint64_t off) {
    uint32_t arr0[6] = {10, 20, 30, 40, 50, 60};
    double arr1[4] = {1.5, 2.5, 3.5, 4.5};
    test_type_header_t th = { .a = 0xAABBCCDDu, .b = 7 };

    file_mmap_array_t build[2] = {
        { .data = arr0, .elem_size = sizeof(uint32_t), .count = 6 },
        { .data = arr1, .elem_size = sizeof(double),   .count = 4 },
    };
    ASSERT(file_mmap_cache_build(cache_path, TEST_MMAP_MAGIC, TEST_MMAP_VERSION, src, off, 0,
                                 &th, sizeof(th), build, 2));

    file_mmap_array_ref_t refs[2] = {
        { .elem_size = sizeof(uint32_t) },
        { .elem_size = sizeof(double) },
    };
    const void *thp = NULL;
    uint64_t disk = 999;
    size_t mlen = 0;
    void *base = file_mmap_cache_open(cache_path, TEST_MMAP_MAGIC, TEST_MMAP_VERSION, src, off,
                                      &thp, sizeof(th), refs, 2, &disk, &mlen);
    ASSERT(base != NULL);

    const test_type_header_t *thr = (const test_type_header_t *)thp;
    ASSERT_EQ(0xAABBCCDDu, thr->a);
    ASSERT_EQ(7, thr->b);
    ASSERT_EQ(6, refs[0].count);
    ASSERT_EQ(4, refs[1].count);
    ASSERT((((uintptr_t)refs[0].base) & 7u) == 0);
    ASSERT((((uintptr_t)refs[1].base) & 7u) == 0);

    uint32_t *r0 = (uint32_t *)refs[0].base;
    for (int i = 0; i < 6; i++) ASSERT_EQ(arr0[i], r0[i]);
    double *r1 = (double *)refs[1].base;
    for (int i = 0; i < 4; i++) ASSERT(memcmp(&r1[i], &arr1[i], sizeof(double)) == 0);

    file_mmap_cache_close(base, mlen);
    PASS();
}

TEST test_mmap_cache_roundtrip(void) {
    char path[256];
    snprintf(path, sizeof(path), "test_mmap_cache_%ld.trcache", (long)getpid());
    file_mmap_src_id_t src = { .size = 1000, .mtime = 123, .mtime_nsec = 456, .ino = 789 };

    CHECK_CALL(mmap_cache_roundtrip_at(path, src, 0));
    CHECK_CALL(mmap_cache_roundtrip_at(path, src, 4096));

    remove(path);
    PASS();
}

TEST test_mmap_cache_validation(void) {
    char path[256];
    snprintf(path, sizeof(path), "test_mmap_cache_val_%ld.trcache", (long)getpid());
    file_mmap_src_id_t src = { .size = 1000, .mtime = 123, .mtime_nsec = 456, .ino = 789 };

    test_type_header_t th = { .a = 1, .b = 2 };
    uint32_t arr[3] = {1, 2, 3};
    file_mmap_array_t build[1] = { { .data = arr, .elem_size = sizeof(uint32_t), .count = 3 } };
    ASSERT(file_mmap_cache_build(path, TEST_MMAP_MAGIC, TEST_MMAP_VERSION, src, 0, 0,
                                 &th, sizeof(th), build, 1));

    file_mmap_array_ref_t refs[1] = { { .elem_size = sizeof(uint32_t) } };
    const void *thp = NULL;
    size_t mlen = 0;
    void *base;

    // Wrong magic / version / size / offset / type-header length -> reject.
    base = file_mmap_cache_open(path, 0xDEADBEEFu, TEST_MMAP_VERSION, src, 0, &thp, sizeof(th), refs, 1, NULL, &mlen);
    ASSERT(base == NULL);
    base = file_mmap_cache_open(path, TEST_MMAP_MAGIC, 999, src, 0, &thp, sizeof(th), refs, 1, NULL, &mlen);
    ASSERT(base == NULL);
    file_mmap_src_id_t bad_size = src; bad_size.size = 2000;
    base = file_mmap_cache_open(path, TEST_MMAP_MAGIC, TEST_MMAP_VERSION, bad_size, 0, &thp, sizeof(th), refs, 1, NULL, &mlen);
    ASSERT(base == NULL);
    base = file_mmap_cache_open(path, TEST_MMAP_MAGIC, TEST_MMAP_VERSION, src, 64, &thp, sizeof(th), refs, 1, NULL, &mlen);
    ASSERT(base == NULL);
    base = file_mmap_cache_open(path, TEST_MMAP_MAGIC, TEST_MMAP_VERSION, src, 0, &thp, sizeof(th) + 4, refs, 1, NULL, &mlen);
    ASSERT(base == NULL);

    // Changed mtime/inode: default (no env / strict) rejects.
    file_mmap_src_id_t changed = src; changed.mtime = 999; changed.mtime_nsec = 0; changed.ino = 111;
    unsetenv("LIBPOSTAL_MMAP_CACHE");
    base = file_mmap_cache_open(path, TEST_MMAP_MAGIC, TEST_MMAP_VERSION, changed, 0, &thp, sizeof(th), refs, 1, NULL, &mlen);
    ASSERT(base == NULL);

    // LIBPOSTAL_MMAP_CACHE=trust: mtime/inode ignored, accepted (size still matches).
    setenv("LIBPOSTAL_MMAP_CACHE", "trust", 1);
    base = file_mmap_cache_open(path, TEST_MMAP_MAGIC, TEST_MMAP_VERSION, changed, 0, &thp, sizeof(th), refs, 1, NULL, &mlen);
    ASSERT(base != NULL);
    ASSERT_EQ(3, refs[0].count);
    file_mmap_cache_close(base, mlen);

    // Trust still rejects a wrong SIZE (size is always validated).
    base = file_mmap_cache_open(path, TEST_MMAP_MAGIC, TEST_MMAP_VERSION, bad_size, 0, &thp, sizeof(th), refs, 1, NULL, &mlen);
    unsetenv("LIBPOSTAL_MMAP_CACHE");
    ASSERT(base == NULL);

    remove(path);
    PASS();
}

#endif

GREATEST_SUITE(libpostal_mmap_cache_tests) {
#ifndef _WIN32
    RUN_TEST(test_mmap_cache_roundtrip);
    RUN_TEST(test_mmap_cache_validation);
#endif
}
