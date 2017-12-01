#ifndef STRNDUP_H
#define STRNDUP_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef HAVE_STRNDUP

char *strndup(const char *s, size_t n);

#endif /* HAVE_STRNDUP */
#endif /* STRNDUP_H */
