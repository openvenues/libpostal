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
CFLAGS =

lib_LTLIBRARIES = libpostal.la
libpostal_la_SOURCES = strndup.c libpostal.c expand.c address_dictionary.c transliterate.c tokens.c trie.c trie_search.c trie_utils.c string_utils.c file_utils.c utf8proc/utf8proc.c normalize.c numex.c features.c unicode_scripts.c address_parser.c address_parser_io.c averaged_perceptron.c crf.c crf_context.c sparse_matrix.c averaged_perceptron_tagger.c graph.c graph_builder.c language_classifier.c language_features.c logistic_regression.c logistic.c minibatch.c float_utils.c ngrams.c place.c near_dupe.c double_metaphone.c geohash/geohash.c dedupe.c string_similarity.c acronyms.c soft_tfidf.c jaccard.c
libpostal_la_LIBADD = libscanner.la $(CBLAS_LIBS)
libpostal_la_CFLAGS = $(CFLAGS_O2) -D LIBPOSTAL_EXPORTS
libpostal_la_LDFLAGS = -version-info @LIBPOSTAL_SO_VERSION@ -no-undefined

dist_bin_SCRIPTS = libpostal_data

# Scanner can take a very long time to compile with higher optimization levels, so always use -O0, scanner is fast enough
# On cross-compilation for ARM using gcc-4.7, there are "out of range" errors during compilation that can be fixed by adding
# -marm option. For that, CFLAGS_SCANNER_EXTRA is provided that can be filled during configuration stage (see ./configure --help). 
noinst_LTLIBRARIES = libscanner.la
libscanner_la_SOURCES = klib/drand48.c scanner.c
libscanner_la_CFLAGS = $(CFLAGS_O0) -D LIBPOSTAL_EXPORTS $(CFLAGS_SCANNER_EXTRA)

noinst_PROGRAMS = libpostal bench address_parser address_parser_train address_parser_test build_address_dictionary build_numex_table build_trans_table address_parser_train address_parser_test language_classifier_train language_classifier language_classifier_test near_dupe_test

libpostal_SOURCES = strndup.c main.c json_encode.c file_utils.c string_utils.c utf8proc/utf8proc.c
libpostal_LDADD = libpostal.la
libpostal_CFLAGS = $(CFLAGS_O3)
bench_SOURCES = bench.c
bench_LDADD = libpostal.la libscanner.la $(CBLAS_LIBS)
bench_CFLAGS = $(CFLAGS_O3)
address_parser_SOURCES = strndup.c address_parser_cli.c json_encode.c linenoise/linenoise.c string_utils.c utf8proc/utf8proc.c
address_parser_LDADD = libpostal.la $(CBLAS_LIBS)
address_parser_CFLAGS = $(CFLAGS_O3)

near_dupe_test_SOURCES = strndup.c near_dupe_test.c string_utils.c utf8proc/utf8proc.c
near_dupe_test_LDADD = libpostal.la
near_dupe_test_CFLAGS = $(CFLAGS_O3)


build_address_dictionary_SOURCES = strndup.c address_dictionary_builder.c address_dictionary.c file_utils.c string_utils.c trie.c trie_search.c utf8proc/utf8proc.c
build_address_dictionary_CFLAGS = $(CFLAGS_O3)
build_numex_table_SOURCES = strndup.c numex_table_builder.c numex.c file_utils.c string_utils.c tokens.c trie.c trie_search.c utf8proc/utf8proc.c
build_numex_table_CFLAGS = $(CFLAGS_O3)
build_trans_table_SOURCES = strndup.c transliteration_table_builder.c transliterate.c trie.c trie_search.c file_utils.c string_utils.c utf8proc/utf8proc.c
build_trans_table_CFLAGS = $(CFLAGS_O3)
address_parser_train_SOURCES = strndup.c address_parser_train.c address_parser.c address_parser_io.c averaged_perceptron.c crf.c crf_context.c sparse_matrix.c graph.c graph_builder.c float_utils.c averaged_perceptron_trainer.c crf_trainer.c crf_trainer_averaged_perceptron.c averaged_perceptron_tagger.c address_dictionary.c normalize.c numex.c features.c unicode_scripts.c transliterate.c trie.c trie_search.c trie_utils.c string_utils.c tokens.c file_utils.c shuffle.c utf8proc/utf8proc.c ngrams.c
address_parser_train_LDADD = libscanner.la $(CBLAS_LIBS)
address_parser_train_CFLAGS = $(CFLAGS_O3)

address_parser_test_SOURCES = strndup.c address_parser_test.c address_parser.c address_parser_io.c averaged_perceptron.c crf.c crf_context.c sparse_matrix.c graph.c graph_builder.c float_utils.c averaged_perceptron_tagger.c address_dictionary.c normalize.c numex.c features.c unicode_scripts.c transliterate.c trie.c trie_search.c trie_utils.c string_utils.c tokens.c file_utils.c utf8proc/utf8proc.c ngrams.c
address_parser_test_LDADD = libscanner.la $(CBLAS_LIBS)
address_parser_test_CFLAGS = $(CFLAGS_O3)

language_classifier_train_SOURCES = strndup.c language_classifier_train.c language_classifier.c language_features.c language_classifier_io.c logistic_regression_trainer.c logistic_regression.c logistic.c sparse_matrix.c sparse_matrix_utils.c features.c minibatch.c float_utils.c stochastic_gradient_descent.c ftrl.c regularization.c cartesian_product.c normalize.c numex.c transliterate.c trie.c trie_search.c trie_utils.c address_dictionary.c string_utils.c file_utils.c utf8proc/utf8proc.c unicode_scripts.c shuffle.c
language_classifier_train_LDADD = libscanner.la $(CBLAS_LIBS)
language_classifier_train_CFLAGS = $(CFLAGS_O3)
language_classifier_SOURCES = strndup.c language_classifier_cli.c language_classifier.c language_features.c logistic_regression.c logistic.c sparse_matrix.c features.c minibatch.c float_utils.c normalize.c numex.c transliterate.c trie.c trie_search.c trie_utils.c address_dictionary.c string_utils.c file_utils.c utf8proc/utf8proc.c unicode_scripts.c
language_classifier_LDADD = libscanner.la $(CBLAS_LIBS)
language_classifier_CFLAGS = $(CFLAGS_O3)
language_classifier_test_SOURCES = strndup.c language_classifier_test.c language_classifier.c language_classifier_io.c language_features.c logistic_regression.c logistic.c sparse_matrix.c features.c minibatch.c float_utils.c normalize.c numex.c transliterate.c trie.c trie_search.c trie_utils.c address_dictionary.c string_utils.c file_utils.c utf8proc/utf8proc.c unicode_scripts.c
language_classifier_test_LDADD = libscanner.la $(CBLAS_LIBS)
language_classifier_test_CFLAGS = $(CFLAGS_O3)


pkginclude_HEADERS = libpostal.h

if DOWNLOAD_DATA
all-local:
	${srcdir}/libpostal_data download all $(datadir)/libpostal
endif

lexer: scanner.re
	re2c -F -s -b -8 -o scanner.c scanner.re

.PHONY: lexer
