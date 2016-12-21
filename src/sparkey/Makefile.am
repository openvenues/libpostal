CFLAGS_CONF = @CFLAGS@
CFLAGS = -I/usr/local/include -O2 -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Werror -pedantic $(CFLAGS_CONF)

noinst_LTLIBRARIES = libsparkey.la
libsparkey_la_SOURCES = endiantools.h hashheader.h logheader.h \
MurmurHash3.h buf.c hashalgorithms.c hashiter.c hashwriter.c \
logreader.c returncodes.c util.c buf.h hashalgorithms.h hashiter.h \
sparkey.h util.h endiantools.c \
hashheader.c hashreader.c logheader.c logwriter.c MurmurHash3.c \
sparkey-internal.h
libsparkey_la_LDFLAGS = -L/usr/local/lib
