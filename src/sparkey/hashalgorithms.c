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
#include "hashalgorithms.h"
#include "MurmurHash3.h"

static uint64_t _read_little_endian32(const uint8_t *data, uint64_t pos) {
  return read_little_endian32(data, pos);
}

static void _write_little_endian32(uint8_t *data, uint64_t hash) {
  write_little_endian32(data, hash);
}

static sparkey_hash_algorithm murmurhash32 = {
  &murmurhash32_hash,
  &_read_little_endian32,
  &_write_little_endian32
};

static sparkey_hash_algorithm murmurhash64 = {
  &murmurhash64_hash,
  &read_little_endian64,
  &write_little_endian64
};

static sparkey_hash_algorithm invalid = {
  NULL, NULL, NULL
};

sparkey_hash_algorithm sparkey_get_hash_algorithm(uint32_t hash_size) {
  switch (hash_size) {
    case 4: return murmurhash32;
    case 8: return murmurhash64;
    default: return invalid;
  }
}

