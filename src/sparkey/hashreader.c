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
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "hashheader.h"
#include "hashiter.h"
#include "util.h"
#include "endiantools.h"
#include "sparkey.h"
#include "sparkey-internal.h"

#define MAGIC_VALUE_HASHREADER (0x75103df9)

sparkey_returncode sparkey_hash_open(sparkey_hashreader **reader_ref, const char *hash_filename, const char *log_filename) {
  RETHROW(correct_endian_platform());

  sparkey_returncode returncode;

  sparkey_hashreader *reader = malloc(sizeof(sparkey_hashreader));
  if (reader == NULL) {
    return SPARKEY_INTERNAL_ERROR;
  }

  TRY(sparkey_load_hashheader(&reader->header, hash_filename), free_reader);
  TRY(sparkey_logreader_open_noalloc(&reader->log, log_filename), free_reader);
  if (reader->header.file_identifier != reader->log.header.file_identifier) {
    returncode = SPARKEY_FILE_IDENTIFIER_MISMATCH;
    goto close_reader;
  }
  if (reader->header.data_end > reader->log.header.data_end) {
    returncode = SPARKEY_HASH_HEADER_CORRUPT;
    goto close_reader;
  }
  if (reader->header.max_key_len > reader->log.header.max_key_len) {
    returncode = SPARKEY_HASH_HEADER_CORRUPT;
    goto close_reader;
  }
  if (reader->header.max_value_len > reader->log.header.max_value_len) {
    returncode = SPARKEY_HASH_HEADER_CORRUPT;
    goto close_reader;
  }

  reader->fd = open(hash_filename, O_RDONLY);
  if (reader->fd < 0) {
    int e = errno;
    returncode = sparkey_open_returncode(e);
    goto close_reader;
  }

  reader->data_len = reader->header.header_size + reader->header.hash_capacity * (reader->header.hash_size + reader->header.address_size);

  struct stat s;
  stat(hash_filename, &s);
  if (reader->data_len > (uint64_t) s.st_size) {
    returncode = SPARKEY_HASH_TOO_SMALL;
    goto close_reader;
  }

  reader->data = mmap(NULL, reader->data_len, PROT_READ, MAP_SHARED, reader->fd, 0);
  if (reader->data == MAP_FAILED) {
    returncode = SPARKEY_MMAP_FAILED;
    goto close_reader;
  }

  *reader_ref = reader;
  reader->open_status = MAGIC_VALUE_HASHREADER;
  return SPARKEY_SUCCESS;

close_reader:
  sparkey_hash_close(&reader);
  return returncode;

free_reader:
  free(reader);
  return returncode;
}

void sparkey_hash_close(sparkey_hashreader **reader_ref) {
  if (reader_ref == NULL) {
    return;
  }
  sparkey_hashreader *reader = *reader_ref;
  if (reader == NULL) {
    return;
  }

  if (reader->open_status != MAGIC_VALUE_HASHREADER) {
    return;
  }
  sparkey_logreader_close_nodealloc(&reader->log);

  reader->open_status = 0;
  if (reader->data != NULL) {
    munmap(reader->data, reader->data_len);
    reader->data = NULL;
  }
  close(reader->fd);
  reader->fd = -1;
  free(reader);
  *reader_ref = NULL;
}

static sparkey_returncode assert_reader_open(sparkey_hashreader *reader) {
  if (reader->open_status != MAGIC_VALUE_HASHREADER) {
    return SPARKEY_HASH_CLOSED;
  }
  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_hash_get(sparkey_hashreader *reader, const uint8_t *key, uint64_t keylen, sparkey_logiter *iter) {
  RETHROW(assert_reader_open(reader));
  uint64_t hash = reader->header.hash_algorithm.hash(key, keylen, reader->header.hash_seed);
  uint64_t wanted_slot = hash % reader->header.hash_capacity;

  int slot_size = reader->header.address_size + reader->header.hash_size;
  uint64_t pos = wanted_slot * slot_size;

  uint64_t displacement = 0;
  uint64_t slot = wanted_slot;

  uint8_t *hashtable = reader->data + reader->header.header_size;

  while (1) {
    uint64_t hash2 = reader->header.hash_algorithm.read_hash(hashtable, pos);
    uint64_t position2 = read_addr(hashtable, pos + reader->header.hash_size, reader->header.address_size);
    if (position2 == 0) {
      iter->state = SPARKEY_ITER_INVALID;
      return SPARKEY_SUCCESS;
    }
    int entry_index2 = (int) (position2) & reader->header.entry_block_bitmask;
    position2 >>= reader->header.entry_block_bits;
    if (hash == hash2) {
      RETHROW(sparkey_logiter_seek(iter, &reader->log, position2));
      RETHROW(sparkey_logiter_skip(iter, &reader->log, entry_index2));
      RETHROW(sparkey_logiter_next(iter, &reader->log));
      uint64_t keylen2 = iter->keylen;
      if (iter->type != SPARKEY_ENTRY_PUT) {
        iter->state = SPARKEY_ITER_INVALID;
        return SPARKEY_INTERNAL_ERROR;
      }
      if (keylen == keylen2) {
        uint64_t pos2 = 0;
        int equals = 1;
        while (pos2 < keylen) {
          uint8_t *buf2;
          uint64_t len2;
          RETHROW(sparkey_logiter_keychunk(iter, &reader->log, keylen, &buf2, &len2));
          if (memcmp(&key[pos2], buf2, len2) != 0) {
            equals = 0;
            break;
          }
          pos2 += len2;
        }
        if (equals) {
          return SPARKEY_SUCCESS;
        }
      }
    }
    uint64_t other_displacement = get_displacement(reader->header.hash_capacity, slot, hash2);
    if (displacement > other_displacement) {
      iter->state = SPARKEY_ITER_INVALID;
      return SPARKEY_SUCCESS;
    }
    pos += slot_size;
    displacement++;
    slot++;
    if (slot >= reader->header.hash_capacity) {
      pos = 0;
      slot = 0;
    }
  }
  iter->state = SPARKEY_ITER_INVALID;
  return SPARKEY_INTERNAL_ERROR;
}

sparkey_returncode sparkey_logiter_hashnext(sparkey_logiter *iter, sparkey_hashreader *reader) {
  RETHROW(assert_reader_open(reader));

  uint8_t *hashtable = reader->data + reader->header.header_size;
  int slot_size = reader->header.address_size + reader->header.hash_size;

  while (1) {
    RETHROW(sparkey_logiter_next(iter, &reader->log));
    if (iter->state != SPARKEY_ITER_ACTIVE) {
      return SPARKEY_SUCCESS;
    }
    if (iter->type != SPARKEY_ENTRY_PUT) {
      continue;
    }
    uint64_t position = (iter->entry_block_position << reader->header.entry_block_bits) | iter->entry_count;

    uint64_t key_hash = sparkey_iter_hash(&reader->header, iter, &reader->log);
    uint64_t wanted_slot = key_hash % reader->header.hash_capacity;

    uint64_t pos = wanted_slot * slot_size;

    uint64_t displacement = 0;
    uint64_t slot = wanted_slot;

    while (1) {
      uint64_t hash2 = reader->header.hash_algorithm.read_hash(hashtable, pos);
      uint64_t position2 = read_addr(hashtable, pos + reader->header.hash_size, reader->header.address_size);
      if (position2 == 0) {
        break;
      }
      if (position == position2) {
        // Found a match! Just reset the iterator
        RETHROW(sparkey_logiter_reset(iter, &reader->log));
        return SPARKEY_SUCCESS;
      }
      uint64_t other_displacement = get_displacement(reader->header.hash_capacity, slot, hash2);
      if (displacement > other_displacement) {
        break;
      }
      pos += slot_size;
      displacement++;
      slot++;
      if (slot >= reader->header.hash_capacity) {
        pos = 0;
        slot = 0;
      }
    }
  }
}

sparkey_logreader * sparkey_hash_getreader(sparkey_hashreader *reader) {
  return &reader->log;
}

uint64_t sparkey_hash_numentries(sparkey_hashreader *reader) {
  return reader->header.num_entries;
}

uint64_t sparkey_hash_numcollisions(sparkey_hashreader *reader) {
  return reader->header.hash_collisions;
}

