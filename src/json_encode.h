#ifndef JSON_ENCODE_H
#define JSON_ENCODE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collections.h"
#include "string_utils.h"
#include "utf8proc/utf8proc.h"

char *json_encode_string(char *str);

#endif