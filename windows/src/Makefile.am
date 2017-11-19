# this version of the makefile skips building the programs. It only builds the libraries and downloads data so you can use the API.

# Inherited from autoconf / user-specified 
CFLAGS_CONF = @CFLAGS@
CFLAGS_BASE = -Wall -Wextra -Wno-unused-function -Wformat -Werror=format-security -Winit-self -Wno-sign-compare -DLIBPOSTAL_DATA_DIR='"$(datadir)/libpostal"' -g $(CFLAGS_CONF)
CFLAGS_O0 = $(CFLAGS_BASE) -O0
CFLAGS_O1 = $(CFLAGS_BASE) -O1
CFLAGS_O2 = $(CFLAGS_BASE) -O2
CFLAGS_O3 = $(CFLAGS_BASE) -O3
DEFAULT_INCLUDES = -I.. -I/usr/local/include

# Wonky but have to be able to override the user's optimization level to compile the scanner
# as it takes an unreasonably long time to compile with the optimizer on.
#EDIT - add UTF8PROC_EXPORTS so builds on windows
CFLAGS = -D UTF8PROC_EXPORTS -D LIBPOSTAL_EXPORTS

lib_LTLIBRARIES = libpostal.la
libpostal_la_SOURCES = strndup.c libpostal.c address_dictionary.c transliterate.c tokens.c trie.c trie_search.c trie_utils.c string_utils.c file_utils.c utf8proc/utf8proc.c cmp/cmp.c normalize.c numex.c features.c unicode_scripts.c address_parser.c address_parser_io.c averaged_perceptron.c crf.c crf_context.c sparse_matrix.c averaged_perceptron_tagger.c graph.c graph_builder.c language_classifier.c language_features.c logistic_regression.c logistic.c minibatch.c float_utils.c ngrams.c
libpostal_la_LIBADD = libscanner.la $(CBLAS_LIBS)
libpostal_la_CFLAGS = $(CFLAGS_O2)
libpostal_la_LDFLAGS = -version-info @LIBPOSTAL_SO_VERSION@ -no-undefined

dist_bin_SCRIPTS = libpostal_data

# Scanner can take a very long time to compile with higher optimization levels, so always use -O0, scanner is fast enough
# On cross-compilation for ARM using gcc-4.7, there are "out of range" errors during compilation that can be fixed by adding
# -marm option. For that, CFLAGS_SCANNER_EXTRA is provided that can be filled during configuration stage (see ./configure --help). 
noinst_LTLIBRARIES = libscanner.la
libscanner_la_SOURCES = klib/drand48.c scanner.c
libscanner_la_CFLAGS = $(CFLAGS_O0) $(CFLAGS_SCANNER_EXTRA)


# program building skipped here

pkginclude_HEADERS = libpostal.h

if DOWNLOAD_DATA
all-local:
	${srcdir}/libpostal_data download all $(datadir)/libpostal
endif

lexer: scanner.re
	re2c -F -s -b -8 -o scanner.c scanner.re

.PHONY: lexer
