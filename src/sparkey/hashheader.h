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
#ifndef SPARKEY_HASHHEADER_H_INCLUDED
#define SPARKEY_HASHHEADER_H_INCLUDED

#include <stdint.h>
#include "endiantools.h"

#include "sparkey.h"
#include "hashalgorithms.h"

#define HASH_MAGIC_NUMBER (0x9a11318f)
#define HASH_MAJOR_VERSION (1)
#define HASH_MINOR_VERSION (1)
#define HASH_HEADER_SIZE (112)

typedef struct {
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t file_identifier;
  uint32_t hash_seed;
  uint32_t header_size;

  uint64_t data_end;
  uint64_t max_key_len;
  uint64_t max_value_len;

  uint64_t garbage_size;
  uint64_t num_entries;
  uint32_t address_size;
  uint32_t hash_size;
  uint64_t hash_capacity;
  uint64_t max_displacement;
  uint64_t num_puts;
  uint32_t entry_block_bits;
  uint32_t entry_block_bitmask;
  uint64_t hash_collisions;
  uint64_t total_displacement;
  sparkey_hash_algorithm hash_algorithm;
} sparkey_hashheader;

/**
 * fills up a hashheader struct based on the contents at the beginning of the file.
 * @param header header struct to fill
 * @param filename a hash file
 * @returns an error code if it could not load the file.
 */
sparkey_returncode sparkey_load_hashheader(sparkey_hashheader *header, const char *filename);

/**
 * Dumps a human readable representation of the header to stdout
 * @param header an initialized header struct
 */
void print_hashheader(sparkey_hashheader *header);

/**
 * Writes a header to the current position in the file
 * @param fd a file descripter pointing to a file open for writing
 * @param header the header to write
 * @returns an error code if it could not write to file.
 */
sparkey_returncode write_hashheader(int fd, sparkey_hashheader *header);

static inline uint64_t get_displacement(uint64_t capacity, uint64_t slot, uint64_t hash) {
  uint64_t wanted_slot = hash % capacity;
  return (capacity + (slot - wanted_slot)) % capacity;
}

static inline uint64_t read_addr(uint8_t *hashtable, uint64_t pos, int address_size) {
  switch (address_size) {
  case 4: return read_little_endian32(hashtable, pos);
  case 8: return read_little_endian64(hashtable, pos);
  }
  return -1;
}

static inline void write_addr(uint8_t *buf, uint64_t value, int address_size) {
  switch (address_size) {
  case 4: write_little_endian32(buf, value); return;
  case 8: write_little_endian64(buf, value); return;
  }
}


#endif

