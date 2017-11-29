#ifndef STRNDUP_H
#define STRNDUP_H

#include <config.h>

#ifndef HAVE_STRNDUP

char *strndup(const char *s, size_t n);

#endif /* HAVE_STRNDUP */
#endif /* STRNDUP_H */
