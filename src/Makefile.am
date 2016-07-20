SUBDIRS = sparkey

CFLAGS_BASE = -Wfloat-equal -Wpointer-arith -DLIBPOSTAL_DATA_DIR='"$(datadir)/libpostal"'
CFLAGS_O0 = $(CFLAGS_BASE) -O0
CFLAGS_O1 = $(CFLAGS_BASE) -O1
CFLAGS_O2 = $(CFLAGS_BASE) -O2
CFLAGS_O3 = $(CFLAGS_BASE) -O3
DEFAULT_INCLUDES = -I.. -I/usr/local/include

CFLAGS = $(CFLAGS_BASE)

lib_LTLIBRARIES = libpostal.la
libpostal_la_SOURCES = libpostal.c address_dictionary.c transliterate.c tokens.c trie.c trie_search.c trie_utils.c string_utils.c file_utils.c numex.c utf8proc/utf8proc.c cmp/cmp.c geodb.c geo_disambiguation.c normalize.c bloom.c features.c geonames.c geohash/geohash.c unicode_scripts.c msgpack_utils.c address_parser.c address_parser_io.c averaged_perceptron.c sparse_matrix.c averaged_perceptron_tagger.c graph.c graph_builder.c language_classifier.c language_features.c logistic_regression.c logistic.c matrix.c minibatch.c float_utils.c
libpostal_la_LIBADD = libscanner.la sparkey/libsparkey.la
libpostal_la_CFLAGS = $(CFLAGS_O2)

dist_bin_SCRIPTS = libpostal_data

# Scanner can take a very long time to compile with higher optimization levels, so always use -O0, scanner is fast enough
noinst_LTLIBRARIES = libscanner.la
libscanner_la_SOURCES = scanner.c
libscanner_la_CFLAGS = $(CFLAGS_O0)

noinst_PROGRAMS = libpostal bench build_address_dictionary build_geodb build_numex_table build_trans_table address_parser_train address_parser_test address_parser language_classifier_train language_classifier language_classifier_test
libpostal_SOURCES = main.c json_encode.c
libpostal_LDADD = libpostal.la
libpostal_CFLAGS = $(CFLAGS_O3)
bench_SOURCES = bench.c
bench_LDADD = libpostal.la libscanner.la
bench_CFLAGS = $(CFLAGS_O3)
build_address_dictionary_SOURCES = address_dictionary_builder.c address_dictionary.c file_utils.c string_utils.c trie.c trie_search.c utf8proc/utf8proc.c
build_address_dictionary_CFLAGS = $(CFLAGS_O3)
build_geodb_SOURCES = geodb_builder.c geodb.c geo_disambiguation.c graph.c graph_builder.c normalize.c features.c geonames.c geohash/geohash.c unicode_scripts.c transliterate.c trie.c trie_search.c string_utils.c msgpack_utils.c file_utils.c utf8proc/utf8proc.c cmp/cmp.c
build_geodb_LDADD = sparkey/libsparkey.la
build_geodb_CFLAGS = $(CFLAGS_O3)
build_numex_table_SOURCES = numex_table_builder.c numex.c file_utils.c string_utils.c tokens.c trie.c trie_search.c utf8proc/utf8proc.c
build_numex_table_CFLAGS = $(CFLAGS_O3)
build_trans_table_SOURCES = transliteration_table_builder.c transliterate.c trie.c trie_search.c file_utils.c string_utils.c utf8proc/utf8proc.c
build_trans_table_CFLAGS = $(CFLAGS_O3)
address_parser_train_SOURCES = address_parser_train.c address_parser.c address_parser_io.c averaged_perceptron.c sparse_matrix.c matrix.c float_utils.c averaged_perceptron_trainer.c averaged_perceptron_tagger.c address_dictionary.c geodb.c geo_disambiguation.c graph.c graph_builder.c normalize.c features.c geonames.c geohash/geohash.c unicode_scripts.c transliterate.c trie.c trie_search.c trie_utils.c string_utils.c tokens.c msgpack_utils.c file_utils.c shuffle.c utf8proc/utf8proc.c cmp/cmp.c
address_parser_train_LDADD = sparkey/libsparkey.la libscanner.la
address_parser_train_CFLAGS = $(CFLAGS_O3)
address_parser_test_SOURCES = address_parser_test.c address_parser.c address_parser_io.c averaged_perceptron.c sparse_matrix.c matrix.c float_utils.c averaged_perceptron_trainer.c averaged_perceptron_tagger.c address_dictionary.c geodb.c geo_disambiguation.c graph.c graph_builder.c normalize.c features.c geonames.c geohash/geohash.c unicode_scripts.c transliterate.c trie.c trie_search.c trie_utils.c string_utils.c tokens.c msgpack_utils.c file_utils.c utf8proc/utf8proc.c cmp/cmp.c
address_parser_test_LDADD = sparkey/libsparkey.la libscanner.la
address_parser_test_CFLAGS = $(CFLAGS_O3)
address_parser_SOURCES = address_parser_cli.c json_encode.c linenoise/linenoise.c
address_parser_LDADD = sparkey/libsparkey.la libscanner.la libpostal.la
address_parser_CFLAGS = $(CFLAGS_O3)
language_classifier_train_SOURCES = language_classifier_train.c language_classifier.c language_features.c language_classifier_io.c logistic_regression_trainer.c logistic_regression.c logistic.c matrix.c sparse_matrix.c sparse_matrix_utils.c features.c minibatch.c float_utils.c stochastic_gradient_descent.c normalize.c transliterate.c trie.c trie_search.c trie_utils.c address_dictionary.c string_utils.c file_utils.c utf8proc/utf8proc.c unicode_scripts.c shuffle.c
language_classifier_train_LDADD = libscanner.la
language_classifier_train_CFLAGS = $(CFLAGS_O3)
language_classifier_SOURCES = language_classifier_cli.c language_classifier.c language_features.c logistic_regression.c logistic.c matrix.c sparse_matrix.c features.c minibatch.c float_utils.c normalize.c transliterate.c trie.c trie_search.c trie_utils.c address_dictionary.c string_utils.c file_utils.c utf8proc/utf8proc.c unicode_scripts.c
language_classifier_LDADD = libscanner.la
language_classifier_CFLAGS = $(CFLAGS_O3)
language_classifier_test_SOURCES = language_classifier_test.c language_classifier.c language_classifier_io.c language_features.c logistic_regression.c logistic.c matrix.c sparse_matrix.c features.c minibatch.c float_utils.c normalize.c transliterate.c trie.c trie_search.c trie_utils.c address_dictionary.c string_utils.c file_utils.c utf8proc/utf8proc.c unicode_scripts.c
language_classifier_test_LDADD = libscanner.la
language_classifier_test_CFLAGS = $(CFLAGS_O3)


pkginclude_HEADERS = libpostal.h

all-local:
	${srcdir}/libpostal_data download all $(datadir)/libpostal

lexer: scanner.re
	re2c -F -s -b -8 -o scanner.c scanner.re

.PHONY: lexer
