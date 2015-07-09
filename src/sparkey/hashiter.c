/*
* Copyright (c) 2012-2013 Spotify AB
*
* Licensed under the Apache License, Version 2.0 (the "License"); you may not
* use this file except in compliance with the License. You may obtain a copy of
* the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations under
* the License.
*/
#include <stdlib.h>
#include <string.h>

#include "sparkey.h"
#include "sparkey-internal.h"
#include "hashiter.h"

uint64_t sparkey_iter_hash(sparkey_hashheader *hash_header, sparkey_logiter *iter, sparkey_logreader *log) {
  uint8_t *buf;
  uint64_t len;
  sparkey_returncode returncode = sparkey_logiter_keychunk(iter, log, 1 << 31, &buf, &len);
  if (returncode != SPARKEY_SUCCESS) {
    return 0;
  }
  if (len == iter->keylen) {
    return hash_header->hash_algorithm.hash(buf, len, hash_header->hash_seed);
  } else {
    uint8_t *keybuf = malloc(iter->keylen);
    memcpy(keybuf, buf, len);
    uint64_t len2;
    returncode = sparkey_logiter_fill_key(iter, log, 1 << 31, keybuf + len, &len2);
    if (len + len2 != iter->keylen) {
      free(keybuf);
      return 0;
    }
    uint64_t hash = hash_header->hash_algorithm.hash(keybuf, iter->keylen, hash_header->hash_seed);
    free(keybuf);
    return hash;
  }
}

