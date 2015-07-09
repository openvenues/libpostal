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
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "sparkey.h"
#include "sparkey-internal.h"

#include "logheader.h"
#include "endiantools.h"
#include "util.h"
#include "hashheader.h"
#include "hashiter.h"

static uint32_t int_log2(uint32_t x) {
  uint32_t count = 0;
  while (x > 0) {
    x >>= 1;
    count++;
  }
  return count;
}

static int unsigned_vlq_size(uint64_t value) {
  if (value < 1ULL << 7ULL) {
    return 1;
  }
  if (value < 1ULL << 14ULL) {
    return 2;
  }
  if (value < 1ULL << 21ULL) {
    return 3;
  }
  if (value < 1ULL << 28ULL) {
    return 4;
  }
  if (value < 1ULL << 35ULL) {
    return 5;
  }
  if (value < 1ULL << 42ULL) {
    return 6;
  }
  if (value < 1ULL << 49ULL) {
    return 7;
  }
  if (value < 1ULL << 56ULL) {
    return 8;
  }
  if (value < 1ULL << 63ULL) {
    return 9;
  }
  return 10;
}


static void added_entry(sparkey_hashheader *hash_header) {
  hash_header->num_entries++;
}

static void replaced_entry(sparkey_hashheader *hash_header, uint64_t keylen, uint64_t valuelen) {
  hash_header->garbage_size += keylen + valuelen + unsigned_vlq_size(keylen + 1) + unsigned_vlq_size(valuelen);
}

static void deleted_entry(sparkey_hashheader *hash_header, uint64_t keylen, uint64_t valuelen) {
  hash_header->garbage_size += keylen + valuelen + unsigned_vlq_size(keylen + 1) + unsigned_vlq_size(valuelen);
  hash_header->num_entries--;
}

static sparkey_returncode hash_delete(uint64_t wanted_slot, uint64_t hash, uint8_t *hashtable, sparkey_hashheader *hash_header, sparkey_logiter *iter, sparkey_logiter *ra_iter, sparkey_logreader *log) {
  int slot_size = hash_header->address_size + hash_header->hash_size;
  uint64_t pos = wanted_slot * slot_size;

  uint64_t displacement = 0;
  uint64_t slot = wanted_slot;

  while (1) {
    uint64_t hash2 = hash_header->hash_algorithm.read_hash(hashtable, pos);
    uint64_t position2 = read_addr(hashtable, pos + hash_header->hash_size, hash_header->address_size);
    if (position2 == 0) {
        return SPARKEY_SUCCESS;
    }
    int entry_index2 = (int) (position2) & hash_header->entry_block_bitmask;
    position2 >>= hash_header->entry_block_bits;
    if (position2 < log->header.header_size || position2 >= log->header.data_end ) {
      fprintf(stderr, "hash_delete():%d bug: found pointer outside of range %"PRIu64"\n", __LINE__, position2);
      return SPARKEY_INTERNAL_ERROR;
    }
    if (hash == hash2) {
      RETHROW(sparkey_logiter_seek(ra_iter, log, position2));
      RETHROW(sparkey_logiter_skip(ra_iter, log, entry_index2));
      RETHROW(sparkey_logiter_next(ra_iter, log));
      uint64_t keylen2 = ra_iter->keylen;
      uint64_t valuelen2 = ra_iter->valuelen;
      if (ra_iter->type != SPARKEY_ENTRY_PUT) {
        fprintf(stderr, "hash_delete():%d bug: expected a put entry but found %d\n", __LINE__, ra_iter->type);
        return SPARKEY_INTERNAL_ERROR;
      }
      if (iter->keylen == keylen2) {
        RETHROW(sparkey_logiter_reset(iter, log));
        int cmp;
        RETHROW(sparkey_logiter_keycmp(iter, ra_iter, log, &cmp));
        if (cmp == 0) {
          // TODO: possibly optimize this to read and write stuff to move in chunks instead of one by one, to decrease number of seeks.
          while (1) {
            uint64_t next_slot = (slot + 1) % hash_header->hash_capacity;
            uint64_t next_pos = next_slot * slot_size;

            uint64_t hash3 = hash_header->hash_algorithm.read_hash(hashtable, next_pos);
            uint64_t position3 = read_addr(hashtable, next_pos + hash_header->hash_size, hash_header->address_size);
            if (position3 == 0) {
                break;
            }
            if ((hash3 % hash_header->hash_capacity) == next_slot) {
                break;
            }

            uint64_t pos3 = slot * slot_size;
            hash_header->hash_algorithm.write_hash(&hashtable[pos3], hash3);
            write_addr(&hashtable[pos3 + hash_header->hash_size], position3, hash_header->address_size);

            slot = next_slot;
          }

          uint64_t pos3 = slot * slot_size;
          hash_header->hash_algorithm.write_hash(&hashtable[pos3], 0);
          write_addr(&hashtable[pos3 + hash_header->hash_size], 0, hash_header->address_size);
          deleted_entry(hash_header, keylen2, valuelen2);

          return SPARKEY_SUCCESS;

        }
      }
    }
    uint64_t other_displacement = get_displacement(hash_header->hash_capacity, slot, hash2);
    if (displacement > other_displacement) {
      return SPARKEY_SUCCESS;
    }
    pos += slot_size;
    displacement++;
    slot++;
    if (slot >= hash_header->hash_capacity) {
      pos = 0;
      slot = 0;
    }
  }
  fprintf(stderr, "hash_put():%d bug: unreachable statement\n", __LINE__);
  return SPARKEY_INTERNAL_ERROR;
}

static sparkey_returncode hash_put(uint64_t wanted_slot, uint64_t hash, uint8_t *hashtable, sparkey_hashheader *hash_header, sparkey_logiter *iter, sparkey_logiter *ra_iter, sparkey_logreader *log, uint64_t position) {
  int slot_size = hash_header->address_size + hash_header->hash_size;
  uint64_t pos = wanted_slot * slot_size;

  uint64_t displacement = 0;
  uint64_t slot = wanted_slot;

  int might_be_collision = iter != NULL && ra_iter != NULL && log != NULL;
  while (1) {
    uint64_t hash2 = hash_header->hash_algorithm.read_hash(hashtable, pos);
    uint64_t position2 = read_addr(hashtable, pos + hash_header->hash_size, hash_header->address_size);
    if (position2 == 0) {
      hash_header->hash_algorithm.write_hash(&hashtable[pos], hash);
      write_addr(&hashtable[pos + hash_header->hash_size], position, hash_header->address_size);
      added_entry(hash_header);
      return SPARKEY_SUCCESS;
    }

    int entry_index2 = (int) (position2) & hash_header->entry_block_bitmask;
    uint64_t position3 = position2 >> hash_header->entry_block_bits;

    if (might_be_collision && hash == hash2) {
      RETHROW(sparkey_logiter_seek(ra_iter, log, position3));
      RETHROW(sparkey_logiter_skip(ra_iter, log, entry_index2));
      RETHROW(sparkey_logiter_next(ra_iter, log));
      uint64_t keylen2 = ra_iter->keylen;
      uint64_t valuelen2 = ra_iter->valuelen;
      if (ra_iter->type != SPARKEY_ENTRY_PUT) {
        fprintf(stderr, "hash_put():%d bug: expected a put entry but found %d\n", __LINE__, ra_iter->type);
        return SPARKEY_INTERNAL_ERROR;
      }
      if (iter->keylen == keylen2) {
        RETHROW(sparkey_logiter_reset(iter, log));
        int cmp;
        RETHROW(sparkey_logiter_keycmp(iter, ra_iter, log, &cmp));
        if (cmp == 0) {
          hash_header->hash_algorithm.write_hash(&hashtable[pos], hash);
          write_addr(&hashtable[pos + hash_header->hash_size], position, hash_header->address_size);
          replaced_entry(hash_header, keylen2, valuelen2);
          return SPARKEY_SUCCESS;
        }
      }
    }

    uint64_t other_displacement = get_displacement(hash_header->hash_capacity, slot, hash2);
    if (displacement > other_displacement) {
      // Steal the slot, and move the other one
      hash_header->hash_algorithm.write_hash(&hashtable[pos], hash);
      write_addr(&hashtable[pos + hash_header->hash_size], position, hash_header->address_size);
      position = position2;
      displacement = other_displacement;
      hash = hash2;
      might_be_collision = 0;
    }
    pos += slot_size;
    displacement++;
    slot++;
    if (slot >= hash_header->hash_capacity) {
      pos = 0;
      slot = 0;
    }
  }
  fprintf(stderr, "hash_put():%d bug: unreachable statement\n", __LINE__);
  return SPARKEY_INTERNAL_ERROR;
}

static void calculate_max_displacement(sparkey_hashheader *hash_header, uint8_t *hashtable) {
  uint64_t capacity = hash_header->hash_capacity;
  int hash_size = hash_header->hash_size;
  int slot_size = hash_header->address_size + hash_size;

  uint64_t max_displacement = 0;
  uint64_t num_hash_collisions = 0;
  uint64_t total_displacement = 0;

  int has_first = 0;
  uint64_t first_hash = 0;

  int has_last = 0;
  uint64_t last_hash = 0;

  int has_prev = 0;
  uint64_t prev_hash = -1;
  for (uint64_t slot = 0; slot < capacity; slot++) {
    uint64_t hash = hash_header->hash_algorithm.read_hash(hashtable, slot * slot_size);
    if (has_prev && prev_hash == hash) {
      num_hash_collisions++;
    }
    uint64_t position = read_addr(hashtable, slot * slot_size + hash_size, hash_header->address_size);
    if (position != 0) {
      prev_hash = hash;
      has_prev = 1;
      uint64_t displacement = get_displacement(capacity, slot, hash);
      total_displacement += displacement;
      if (displacement > max_displacement) {
        max_displacement = displacement;
      }
      if (slot == 0) {
        first_hash = hash;
        has_first = 1;
      }
      if (slot == capacity - 1) {
        last_hash = hash;
        has_last = 1;
      }
    } else {
      has_prev = 0;
    }
  }
  if (has_first && has_last && first_hash == last_hash) {
    num_hash_collisions++;
  }
  hash_header->total_displacement = total_displacement;
  hash_header->max_displacement = max_displacement;
  hash_header->hash_collisions = num_hash_collisions;
}

static sparkey_returncode read_fully(int fd, uint8_t *buf, size_t count) {
  while (count > 0) {
    ssize_t actual_read = read(fd, buf, count);
    if (actual_read < 0) {
      fprintf(stderr, "read_fully():%d bug: actual_read = %"PRIu64", errno = %d\n", __LINE__, (uint64_t)actual_read, errno);
      return SPARKEY_INTERNAL_ERROR;
    }
    count -= actual_read;
  }
  return SPARKEY_SUCCESS;
}

static sparkey_returncode hash_copy(uint8_t *hashtable, uint8_t *buf, size_t buffer_size, sparkey_hashheader *old_header, sparkey_hashheader *new_header) {
  int slot_size = old_header->address_size + old_header->hash_size;
  for (unsigned int i = 0; i < buffer_size; i += slot_size) {
    uint64_t hash = old_header->hash_algorithm.read_hash(buf, i);
    uint64_t position = read_addr(buf, i + old_header->hash_size, old_header->address_size);

    int entry_index = (int) (position) & old_header->entry_block_bitmask;
    position >>= old_header->entry_block_bits;

    uint64_t wanted_slot = hash % new_header->hash_capacity;
    if (position != 0) {
      RETHROW(hash_put(wanted_slot, hash, hashtable, new_header, NULL, NULL, NULL, (position << new_header->entry_block_bits) | entry_index));
    }
  }
  return SPARKEY_SUCCESS;
}

static sparkey_returncode fill_hash(uint8_t *hashtable, const char *hash_filename, sparkey_hashheader *old_header, sparkey_hashheader *new_header) {
  int fd = open(hash_filename, O_RDONLY);
  if (fd < 0) {
    return sparkey_open_returncode(errno);
  }

  lseek(fd, old_header->header_size, SEEK_SET);

  int slot_size = old_header->address_size + old_header->hash_size;
  uint64_t buffer_size = slot_size * 1024;
  uint8_t *buf = malloc(buffer_size);
  if (buf == NULL) {
    fprintf(stderr, "fill_hash():%d bug: could not malloc %"PRIu64" bytes\n", __LINE__, buffer_size);
    return SPARKEY_INTERNAL_ERROR;
  }

  sparkey_returncode returncode = SPARKEY_SUCCESS;
  uint64_t remaining = old_header->hash_capacity * slot_size;
  while (buffer_size <= remaining) {
    TRY(read_fully(fd, buf, buffer_size), free);
    TRY(hash_copy(hashtable, buf, buffer_size, old_header, new_header), free);
    remaining -= buffer_size;
  }
  TRY(read_fully(fd, buf, remaining), free);
  TRY(hash_copy(hashtable, buf, remaining, old_header, new_header), free);

free:
  free(buf);
  if (close(fd) < 0) {
    if (returncode == SPARKEY_SUCCESS) {
      fprintf(stderr, "fill_hash():%d bug: could not close file. errno = %d\n", __LINE__, errno);
      returncode = SPARKEY_INTERNAL_ERROR;
    }
  }

  return returncode;
}

sparkey_returncode sparkey_hash_write(const char *hash_filename, const char *log_filename, int hash_size) {
  sparkey_logheader log_header;
  sparkey_logreader *log;
  sparkey_logiter *iter = NULL;
  sparkey_logiter *ra_iter = NULL;

  RETHROW(sparkey_load_logheader(&log_header, log_filename));

  RETHROW(sparkey_logreader_open(&log, log_filename));
  sparkey_returncode returncode = SPARKEY_SUCCESS;
  TRY(sparkey_logiter_create(&iter, log), close_reader);
  TRY(sparkey_logiter_create(&ra_iter, log), close_iter);

  sparkey_hashheader hash_header;
  sparkey_hashheader old_header;

  double cap;
  uint64_t start;
  uint32_t hash_seed;
  int copy_old;
  uint32_t old_hash_size = 0;
  returncode = sparkey_load_hashheader(&old_header, hash_filename);
  if (returncode == SPARKEY_SUCCESS &&
      old_header.file_identifier == log_header.file_identifier &&
      old_header.major_version == HASH_MAJOR_VERSION &&
      old_header.minor_version == HASH_MINOR_VERSION) {
    // Prepare to copy stuff from old header
    cap = ((log_header.num_puts - old_header.num_puts) + old_header.num_entries) * 1.3;
    start = old_header.data_end;
    hash_seed = old_header.hash_seed;
    hash_header.garbage_size = old_header.garbage_size;

    copy_old = 1;
    old_hash_size = old_header.hash_size;
  } else {
    cap = log_header.num_puts * 1.3;
    start = log_header.header_size;
    TRY(rand32(&hash_seed), close_iter);
    hash_header.garbage_size = 0;
    copy_old = 0;
    returncode = SPARKEY_SUCCESS;
  }

  hash_header.hash_capacity = 1 | (uint64_t) cap;

  hash_header.hash_seed = hash_seed;
  hash_header.max_key_len = log_header.max_key_len;
  hash_header.max_value_len = log_header.max_value_len;
  hash_header.data_end = log_header.data_end;
  hash_header.num_puts = log_header.num_puts;

  hash_header.entry_block_bits = int_log2(log_header.max_entries_per_block);
  hash_header.entry_block_bitmask = (1 << hash_header.entry_block_bits) - 1;

  if (hash_header.data_end < (1ULL << (32 - hash_header.entry_block_bits))) {
    hash_header.address_size = 4;
  } else {
    hash_header.address_size = 8;
  }
  if (old_hash_size == 8 || hash_header.hash_capacity >= (1 << 23)) {
    hash_header.hash_size = 8;
  } else {
    hash_header.hash_size = 4;
  }
  if (hash_size != 0) {
    if (hash_size == 4 || hash_size == 8) {
      hash_header.hash_size = hash_size;
    } else {
      returncode = SPARKEY_HASH_SIZE_INVALID;
      goto close_iter;
    }
  }
  if (hash_header.hash_size != old_hash_size) {
    copy_old = 0;
  }
  hash_header.hash_algorithm = sparkey_get_hash_algorithm(hash_header.hash_size);

  int slot_size = hash_header.hash_size + hash_header.address_size;
  uint64_t hashsize = slot_size * hash_header.hash_capacity;
  uint8_t *hashtable = malloc(hashsize);
  if (hashtable == NULL) {
    fprintf(stderr, "sparkey_hash_write():%d bug: could not malloc %"PRIu64" bytes\n", __LINE__, hashsize);
    returncode = SPARKEY_INTERNAL_ERROR;
    goto close_iter;
  }
  memset(hashtable, 0, hashsize);

  hash_header.max_displacement = 0;
  hash_header.total_displacement = 0;
  hash_header.num_entries = 0;
  hash_header.hash_collisions = 0;

  if (copy_old) {
    if (old_header.data_end == log->header.data_end) {
      // Nothing needs to be done - just exit
      goto close_iter;
    }
    TRY(fill_hash(hashtable, hash_filename, &old_header, &hash_header), free_hashtable);
    TRY(sparkey_logiter_seek(iter, log, start), free_hashtable);
  }

  while (1) {
    TRY(sparkey_logiter_next(iter, log), free_hashtable);
    switch (iter->state) {
    case SPARKEY_ITER_CLOSED:
      goto normal_exit;
      break;
    case SPARKEY_ITER_ACTIVE:
      break;
    default:
      fprintf(stderr, "sparkey_hash_write():%d bug: invalid iter state: %d\n", __LINE__, iter->state);
      returncode = SPARKEY_INTERNAL_ERROR;
      goto free_hashtable;
      break;
    }

    uint64_t iter_block_start = iter->block_position;
    uint64_t iter_entry_count = iter->entry_count;

    uint64_t key_hash = sparkey_iter_hash(&hash_header, iter, log);
    uint64_t wanted_slot = key_hash % hash_header.hash_capacity;

    switch (iter->type) {
    case SPARKEY_ENTRY_PUT:
      TRY(hash_put(wanted_slot, key_hash, hashtable, &hash_header, iter, ra_iter, log, (iter_block_start << hash_header.entry_block_bits) | iter_entry_count), free_hashtable);
      break;
    case SPARKEY_ENTRY_DELETE:
      hash_header.garbage_size += 1 + unsigned_vlq_size(iter->keylen) + iter->keylen;
      TRY(hash_delete(wanted_slot, key_hash, hashtable, &hash_header, iter, ra_iter, log), free_hashtable);
      break;
    }
  }
normal_exit:

  calculate_max_displacement(&hash_header, hashtable);

  // Try removing it first, to avoid overwriting existing files that readers may be using.
  if (remove(hash_filename) < 0) {
    int e = errno;
    if (e != ENOENT) {
      returncode = sparkey_remove_returncode(e);
      goto free_hashtable;
    }
  }
  int fd = creat(hash_filename, 00644);
  hash_header.major_version = HASH_MAJOR_VERSION;
  hash_header.minor_version = HASH_MINOR_VERSION;
  hash_header.file_identifier = log_header.file_identifier;
  hash_header.data_end = log_header.data_end;

  TRY(write_hashheader(fd, &hash_header), close_hash);
  TRY(write_full(fd, hashtable, hashsize), close_hash);

close_hash:
  close(fd);

free_hashtable:
  free(hashtable);

close_iter:
  sparkey_logiter_close(&iter);
  sparkey_logiter_close(&ra_iter);

close_reader:
  sparkey_logreader_close(&log);

  return returncode;
}

