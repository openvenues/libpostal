#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef HAVE_STRNDUP

#include <stdlib.h>
#include <string.h>

char *strndup(const char *s, size_t n)
{
    char* new = malloc(n+1);
    if (new) {
        strncpy(new, s, n);
        new[n] = '\0';
    }
    return new;
}

#endif /* HAVE_STRNDUP */
