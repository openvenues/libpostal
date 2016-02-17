CFLAGS_BASE = -Wfloat-equal -Wpointer-arith -std=gnu99 -DLIBPOSTAL_DATA_DIR='"$(datadir)/libpostal"'
CFLAGS_O0 = $(CFLAGS_BASE) -O0
CFLAGS_O1 = $(CFLAGS_BASE) -O1
CFLAGS_O2 = $(CFLAGS_BASE) -O2
CFLAGS_O3 = $(CFLAGS_BASE) -O3
DEFAULT_INCLUDES = -I..

CFLAGS = $(CFLAGS_BASE)

TESTS = test_libpostal
noinst_PROGRAMS = test_libpostal
test_libpostal_SOURCES = test.c test_expand.c test_parser.c test_transliterate.c test_numex.c test_trie.c
test_libpostal_LDADD = ../src/libpostal.la
test_libpostal_CFLAGS = $(CFLAGS_O3)
