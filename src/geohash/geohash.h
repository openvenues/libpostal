/*** MIT License
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef GEOHASH_H
#define GEOHASH_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

 

enum {
    GEOHASH_OK,
    GEOHASH_NOTSUPPORTED,
    GEOHASH_INVALIDCODE,
    GEOHASH_INVALIDARGUMENT,
    GEOHASH_INTERNALERROR,
    GEOHASH_NOMEMORY
};

int geohash_encode(double latitude, double longitude, char* r, size_t capacity);
int geohash_decode(char* r, size_t length, double *latitude, double *longitude);
int geohash_neighbors(char *hashcode, char* dst, size_t dst_length, int *string_count);

 

#endif