#ifndef LIBPOSTAL_CONSTANTS_H
#define LIBPOSTAL_CONSTANTS_H

#include <stdlib.h>
#include <string.h>

#define NAMESPACE_SEPARATOR_CHAR "|"
#define NAMESPACE_SEPARATOR_CHAR_LEN strlen(NAMESPACE_SEPARATOR_CHAR)

#define LANGUAGE_SEPARATOR_CHAR "|"
#define LANGUAGE_SEPARATOR_CHAR_LEN strlen(LANGUAGE_SEPARATOR_CHAR)

// Supports ISO 3166 alpha 2 and alpha 3 codes
#define MAX_COUNTRY_CODE_LEN 4

// Supports ISO 639 alpha 2 and alpha 3 codes
#define MAX_LANGUAGE_LEN 4

#endif
