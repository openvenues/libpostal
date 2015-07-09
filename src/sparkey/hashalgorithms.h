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
#ifndef SPARKEY_HASHALGORITHM_H_INCLUDED
#define SPARKEY_HASHALGORITHM_H_INCLUDED

#include "endiantools.h"

typedef struct {
  uint64_t (*hash)(const uint8_t *data, uint64_t len, uint32_t seed);
  uint64_t (*read_hash)(const uint8_t *data, uint64_t pos);
  void (*write_hash)(uint8_t *data, uint64_t hash);
} sparkey_hash_algorithm;

sparkey_hash_algorithm sparkey_get_hash_algorithm(uint32_t hash_size);

#endif
