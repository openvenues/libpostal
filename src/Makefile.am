SUBDIRS = sparkey

CFLAGS_BASE = -Wfloat-equal -Wpointer-arith -Werror
CFLAGS_O0 = $(CFLAGS_BASE) -O0
CFLAGS_O1 = $(CFLAGS_BASE) -O1
CFLAGS_O2 = $(CFLAGS_BASE) -O2
CFLAGS_O3 = $(CFLAGS_BASE) -O3
DEFAULT_INCLUDES=-I..

CFLAGS = $(CFLAGS_BASE) -DLIBPOSTAL_DATA_DIR='"$(LIBPOSTAL_DATA_DIR)"'

lib_LTLIBRARIES = libpostal.la
libpostal_la_SOURCES = libpostal.c address_dictionary.c transliterate.c tokens.c trie.c trie_search.c string_utils.c file_utils.c numex.c utf8proc/utf8proc.c cmp/cmp.c geodb.c geo_disambiguation.c normalize.c bloom.c features.c geonames.c geohash/geohash.c unicode_scripts.c msgpack_utils.c
libpostal_la_LIBADD = libscanner.la sparkey/libsparkey.la
libpostal_la_CFLAGS = $(CFLAGS_O2)

# Scanner can take a very long time to compile with higher optimization levels, so always use -O0, scanner is fast enough
noinst_LTLIBRARIES = libscanner.la
libscanner_la_SOURCES = scanner.c
libscanner_la_CFLAGS =  $(CFLAGS_O0)

noinst_PROGRAMS = libpostal bench
libpostal_SOURCES = main.c
libpostal_LDADD = libpostal.la
bench_SOURCES = bench.c
bench_LDADD = libpostal.la libscanner.la

pkginclude_HEADERS = libpostal.h

LIBPOSTAL_S3_BUCKET_NAME = libpostal
LIBPOSTAL_S3_BUCKET_URL = http://$(LIBPOSTAL_S3_BUCKET_NAME).s3.amazonaws.com
LIBPOSTAL_DATA_FILE = libpostal_data.tar.gz

LIBPOSTAL_DATA_DIR = $(datadir)/libpostal

EPOCH_DATE = Jan  1 00:00:00 1970

LAST_MODIFIED_DATE = $(EPOCH_DATE)


if HAVE_DATE_STAT
USE_DATE_STAT = 1

else
if HAVE_STAT
USE_STAT = 1

else
    $(error Cannot get file modification date on this platform);
endif

endif


libpostal_data_mkdir:
	mkdir -p $(LIBPOSTAL_DATA_DIR)

libpostal_data_updated:
	if [ ! -e @LIBPOSTAL_DATA_UPDATED_PATH@ ]; then						\
		echo "$(EPOCH_DATE)" > @LIBPOSTAL_DATA_UPDATED_PATH@;			\
	fi;

libpostal_data.tar.gz: libpostal_data_mkdir libpostal_data_updated
	if [ $$(curl $(LIBPOSTAL_S3_BUCKET_URL)/$(LIBPOSTAL_DATA_FILE) -z "$$(cat @LIBPOSTAL_DATA_UPDATED_PATH@)" --silent --remote-time -o $(LIBPOSTAL_DATA_DIR)/$(LIBPOSTAL_DATA_FILE) -w %{http_code}) = "200" ]; then		\
		if [ "x$(USE_DATE_STAT)" != "x" ]; then																																												\
			echo $$(date -d "$$(date -d "@$$(date -r $(LIBPOSTAL_DATA_DIR)/$(LIBPOSTAL_DATA_FILE) +%s)") + 1 second") > @LIBPOSTAL_DATA_UPDATED_PATH@;																		\
		elif [ "x$(USE_STAT)" != "x" ]; then																																												\
			echo $$(date -r $$(stat -f %m $(LIBPOSTAL_DATA_DIR)/$(LIBPOSTAL_DATA_FILE)) -v+1S) > @LIBPOSTAL_DATA_UPDATED_PATH@;																								\
		fi;																																																					\
		tar -xvzf $(LIBPOSTAL_DATA_DIR)/$(LIBPOSTAL_DATA_FILE) -C $(LIBPOSTAL_DATA_DIR); 																																	\
	fi;

address_dictionary_builder: address_dictionary_builder.c
	$(CC) $(CFLAGS_O2) $(LDFLAGS) address_dictionary_builder.c address_dictionary.c file_utils.c string_utils.c trie.c trie_search.c utf8proc/utf8proc.c -o build_address_dictionary

geodb_builder: geodb_builder.c
	$(CC) $(CFLAGS_O2) $(LDFLAGS) geodb_builder.c geodb.c geo_disambiguation.c normalize.c bloom.c features.c geonames.c geohash/geohash.c sparkey/*.c unicode_scripts.c transliterate.c trie.c trie_search.c string_utils.c msgpack_utils.c file_utils.c utf8proc/utf8proc.c cmp/cmp.c -o build_geodb $(LIBS)

transliteration_table_builder: transliteration_table_builder.c
	$(CC) $(CFLAGS_O2) $(LDFLAGS) transliteration_table_builder.c transliterate.c trie.c trie_search.c file_utils.c string_utils.c utf8proc/utf8proc.c -o build_trans_table

numex_table_builder: numex_table_builder.c numex_data.c
	$(CC) $(CFLAGS_O2) $(LDFLAGS) numex_table_builder.c numex.c file_utils.c string_utils.c tokens.c trie.c trie_search.c utf8proc/utf8proc.c -o build_numex_table

lexer: scanner.re
	re2c -F -s -b -8 -o scanner.c scanner.re

.PHONY: address_dictionary_builder geodb_builder transliteration_table_builder numex_table_builder lexer
