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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <snappy-c.h>

#include "sparkey.h"
#include "sparkey-internal.h"
#include "logheader.h"
#include "endiantools.h"
#include "util.h"

#define MAGIC_VALUE_LOGITER (0xd765c8cc)
#define MAGIC_VALUE_LOGREADER (0xe93356c4)

static inline uint64_t min64(uint64_t a, uint64_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

static inline uint64_t read_vlq(uint8_t * array, uint64_t *position) {
  uint64_t res = 0;
  uint64_t shift = 0;
  uint64_t tmp, tmp2;
  while (1) {
    tmp = array[(*position)++];
    tmp2 = tmp & 0x7f;
    if (tmp == tmp2) {
      return res | tmp << shift;
    }
    res |= tmp2 << shift;
    shift += 7;
  }
  return res;
}

sparkey_returncode sparkey_logreader_open_noalloc(sparkey_logreader *log, const char *filename) {
  int fd = 0;
  sparkey_returncode returncode;
  TRY(sparkey_load_logheader(&log->header, filename), cleanup);
  log->data_len = log->header.data_end;

  struct stat s;
  stat(filename, &s);
  if (log->data_len > (uint64_t) s.st_size) {
    returncode = SPARKEY_LOG_TOO_SMALL;
    goto cleanup;
  }

  fd = open(filename, O_RDONLY);
  if (fd < 0) {
    returncode = sparkey_open_returncode(errno);
    goto cleanup;
  }
  log->fd = fd;

  log->data = mmap(NULL, log->data_len, PROT_READ, MAP_SHARED, fd, 0);
  if (log->data == MAP_FAILED) {
    returncode = SPARKEY_MMAP_FAILED;
    goto cleanup;
  }

  log->open_status = MAGIC_VALUE_LOGREADER;
  return SPARKEY_SUCCESS;

cleanup:
  if (fd > 0) close(fd);
  return returncode;
}

sparkey_returncode sparkey_logreader_open(sparkey_logreader **log_ref, const char *filename) {
  RETHROW(correct_endian_platform());

  sparkey_logreader *log = malloc(sizeof(sparkey_logreader));
  if (log == NULL) {
    return SPARKEY_INTERNAL_ERROR;
  }

  sparkey_returncode returncode;
  TRY(sparkey_logreader_open_noalloc(log, filename), cleanup);

  *log_ref = log;
  return SPARKEY_SUCCESS;

cleanup:
  free(log);
  return returncode;
}

void sparkey_logreader_close_nodealloc(sparkey_logreader *log) {
  if (log == NULL) {
    return;
  }
  if (log->open_status != MAGIC_VALUE_LOGREADER) {
    return;
  }
  log->open_status = 0;
  if (log->data != NULL) {
    munmap(log->data, log->data_len);
    log->data = NULL;
  }
  close(log->fd);
  log->fd = -1;
}

void sparkey_logreader_close(sparkey_logreader **log_ref) {
  if (log_ref == NULL) {
    return;
  }
  sparkey_logreader *log = *log_ref;
  sparkey_logreader_close_nodealloc(log);
  free(log);
  *log_ref = NULL;
}

static sparkey_returncode assert_log_open(sparkey_logreader *log) {
  if (log->open_status != MAGIC_VALUE_LOGREADER) {
    return SPARKEY_LOG_CLOSED;
  }
  return SPARKEY_SUCCESS;
}

static sparkey_returncode assert_iter_open(sparkey_logiter *iter, sparkey_logreader *log) {
  RETHROW(assert_log_open(log));
  if (iter->open_status != MAGIC_VALUE_LOGITER) {
    return SPARKEY_LOG_ITERATOR_CLOSED;
  }
  if (iter->file_identifier != log->header.file_identifier) {
    return SPARKEY_LOG_ITERATOR_MISMATCH;
  }
  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_logiter_create(sparkey_logiter **iter_ref, sparkey_logreader *log) {
  RETHROW(assert_log_open(log));

  sparkey_logiter *iter = malloc(sizeof(sparkey_logiter));
  if (iter == NULL) {
    return SPARKEY_INTERNAL_ERROR;
  }

  iter->open_status = MAGIC_VALUE_LOGITER;
  iter->file_identifier = log->header.file_identifier;
  iter->block_position = 0;
  iter->next_block_position = log->header.header_size;
  iter->block_offset = 0;
  iter->block_len = 0;
  iter->state = SPARKEY_ITER_NEW;

  switch (log->header.compression_type) {
  case SPARKEY_COMPRESSION_NONE:
    iter->compression_buf_allocated = 0;
    break;
  case SPARKEY_COMPRESSION_SNAPPY:
    iter->compression_buf_allocated = 1;
    iter->compression_buf = malloc(log->header.compression_block_size);
    if (iter->compression_buf == NULL) {
      free(iter);
      return SPARKEY_INTERNAL_ERROR;
    }
    break;
  default:
    free(iter);
    return SPARKEY_INTERNAL_ERROR;
  }

  *iter_ref = iter;
  return SPARKEY_SUCCESS;
}

void sparkey_logiter_close(sparkey_logiter **iter_ref) {
  if (iter_ref == NULL) {
    return;
  }
  sparkey_logiter *iter = *iter_ref;
  if (iter == NULL) {
    return;
  }
  if (iter->open_status != MAGIC_VALUE_LOGITER) {
    return;
  }
  iter->open_status = 0;

  if (iter->compression_buf_allocated) {
    free(iter->compression_buf);
  }
  free(iter);
  *iter_ref = NULL;
}

static sparkey_returncode seekblock(sparkey_logiter *iter, sparkey_logreader *log, uint64_t position) {
  iter->block_offset = 0;
  if (iter->block_position == position) {
    return SPARKEY_SUCCESS;
  }
  if (log->header.compression_type == SPARKEY_COMPRESSION_NONE) {
    iter->compression_buf = &log->data[position];
    iter->block_position = position;
    iter->next_block_position = log->header.data_end;
    iter->block_len = log->data_len - position;
    return SPARKEY_SUCCESS;
  }
  if (log->header.compression_type == SPARKEY_COMPRESSION_SNAPPY) {
    uint64_t pos = position;
    // TODO: assert that size_t >= uint64_t
    size_t compressed_size = read_vlq(log->data, &pos);
    uint64_t next_pos = pos + compressed_size;
    const char *input = (char *) &log->data[pos];

    size_t uncompressed_size = log->header.compression_block_size;
    snappy_status status = snappy_uncompress(input, compressed_size, (char *) iter->compression_buf, &uncompressed_size);
    switch (status) {
    case SNAPPY_OK: break;
    case SNAPPY_INVALID_INPUT:
      return SPARKEY_INTERNAL_ERROR;
    case SNAPPY_BUFFER_TOO_SMALL:
      return SPARKEY_INTERNAL_ERROR;
    default:
      return SPARKEY_INTERNAL_ERROR;
    }
    iter->block_position = position;
    iter->next_block_position = next_pos;
    iter->block_len = uncompressed_size;
    return SPARKEY_SUCCESS;
  }

  return SPARKEY_INTERNAL_ERROR;
}

sparkey_returncode sparkey_logiter_seek(sparkey_logiter *iter, sparkey_logreader *log, uint64_t position) {
  RETHROW(assert_iter_open(iter, log));
  if (position == log->header.data_end) {
    iter->state = SPARKEY_ITER_CLOSED;
    return SPARKEY_SUCCESS;
  }
  RETHROW(seekblock(iter, log, position));
  iter->entry_count = -1;
  iter->state = SPARKEY_ITER_NEW;
  return SPARKEY_SUCCESS;
}

static sparkey_returncode ensure_available(sparkey_logiter *iter, sparkey_logreader *log) {
  if (iter->block_offset < iter->block_len) {
    return SPARKEY_SUCCESS;
  }

  if (iter->next_block_position >= log->header.data_end) {
    iter->block_position = 0;
    iter->block_offset = 0;
    iter->block_len = 0;
    return SPARKEY_SUCCESS;
  }
  RETHROW(seekblock(iter, log, iter->next_block_position));
  iter->entry_count = -1;

  return SPARKEY_SUCCESS;
}

static sparkey_returncode skip(sparkey_logiter *iter, sparkey_logreader *log, uint64_t len) {
  while (len > 0) {
    RETHROW(ensure_available(iter, log));
    uint64_t m = min64(len, iter->block_len - iter->block_offset);
    len -= m;
    iter->block_offset += m;
  }
  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_logiter_next(sparkey_logiter *iter, sparkey_logreader *log) {
  if (iter->state == SPARKEY_ITER_CLOSED) {
    return SPARKEY_SUCCESS;
  }
  uint64_t key_remaining = 0;
  uint64_t value_remaining = 0;
  if (iter->state == SPARKEY_ITER_ACTIVE) {
    key_remaining = iter->key_remaining;
    value_remaining = iter->value_remaining;
  }

  iter->state = SPARKEY_ITER_INVALID;
  iter->key_remaining = 0;
  iter->value_remaining = 0;
  iter->keylen = 0;
  iter->valuelen = 0;

  RETHROW(assert_iter_open(iter, log));
  RETHROW(skip(iter, log, key_remaining));
  RETHROW(skip(iter, log, value_remaining));

  RETHROW(ensure_available(iter, log));
  if (iter->block_len - iter->block_offset == 0) {
    // Reached end of data
    iter->state = SPARKEY_ITER_CLOSED;
    return SPARKEY_SUCCESS;
  }

  if (log->header.compression_type == SPARKEY_COMPRESSION_NONE) {
  	iter->block_position += iter->block_offset;
  	iter->block_len -= iter->block_offset;
  	iter->block_offset = 0;
    iter->compression_buf = &log->data[iter->block_position];
    iter->entry_count = -1;
  }

  iter->entry_count++;

  uint64_t a = read_vlq(iter->compression_buf, &iter->block_offset);
  uint64_t b = read_vlq(iter->compression_buf, &iter->block_offset);
  if (a == 0) {
    iter->keylen = iter->key_remaining = b;
    iter->valuelen = iter->value_remaining = 0;
    iter->type = SPARKEY_ENTRY_DELETE;
  } else {
    iter->keylen = iter->key_remaining = a - 1;
    iter->valuelen = iter->value_remaining = b;
    iter->type = SPARKEY_ENTRY_PUT;
  }

  iter->entry_block_position = iter->block_position;
  iter->entry_block_offset = iter->block_offset;

  iter->state = SPARKEY_ITER_ACTIVE;

  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_logiter_reset(sparkey_logiter *iter, sparkey_logreader *log) {
  if (iter->state != SPARKEY_ITER_ACTIVE) {
    return SPARKEY_LOG_ITERATOR_INACTIVE;
  }
  RETHROW(seekblock(iter, log, iter->entry_block_position));

  iter->key_remaining = iter->keylen;
  iter->value_remaining = iter->valuelen;
  iter->block_offset = iter->entry_block_offset;
  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_logiter_skip(sparkey_logiter *iter, sparkey_logreader *log, int count) {
  while (count > 0) {
    count--;
    RETHROW(sparkey_logiter_next(iter, log));
  }
  return SPARKEY_SUCCESS;
}



static sparkey_returncode sparkey_logiter_chunk(sparkey_logiter *iter, sparkey_logreader *log, uint64_t maxlen, uint64_t *len, uint8_t ** res, uint64_t *var) {
  RETHROW(assert_iter_open(iter, log));

  if (iter->state != SPARKEY_ITER_ACTIVE) {
    return SPARKEY_LOG_ITERATOR_INACTIVE;
  }

  if (*var > 0) {
    RETHROW(ensure_available(iter, log));
    uint64_t m = min64(*var, iter->block_len - iter->block_offset);
    m = min64(maxlen, m);
    *len = m;
    *res = &iter->compression_buf[iter->block_offset];
    iter->block_offset += m;
    *var -= m;
    return SPARKEY_SUCCESS;
  }
  *len = 0;
  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_logiter_keychunk(sparkey_logiter *iter, sparkey_logreader *log, uint64_t maxlen, uint8_t ** res, uint64_t *len) {
  return sparkey_logiter_chunk(iter, log, maxlen, len, res, &iter->key_remaining);
}

sparkey_returncode sparkey_logiter_valuechunk(sparkey_logiter *iter, sparkey_logreader *log, uint64_t maxlen, uint8_t ** res, uint64_t *len) {
  RETHROW(skip(iter, log, iter->key_remaining));
  iter->key_remaining = 0;
  return sparkey_logiter_chunk(iter, log, maxlen, len, res, &iter->value_remaining);
}

sparkey_returncode sparkey_logiter_fill_key(sparkey_logiter *iter, sparkey_logreader *log, uint64_t maxlen, uint8_t *buf, uint64_t *len) {
  *len = 0;
  while (maxlen > 0) {
    uint8_t *buf2;
    uint64_t len2;
    RETHROW(sparkey_logiter_keychunk(iter, log, maxlen, &buf2, &len2));
    if (len2 == 0) {
      return SPARKEY_SUCCESS;
    }
    memcpy(buf, buf2, len2);
    buf += len2;
    *len += len2;
    maxlen -= len2;
  }
  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_logiter_fill_value(sparkey_logiter *iter, sparkey_logreader *log, uint64_t maxlen, uint8_t *buf, uint64_t *len) {
  *len = 0;
  while (maxlen > 0) {
    uint8_t *buf2;
    uint64_t len2;
    RETHROW(sparkey_logiter_valuechunk(iter, log, maxlen, &buf2, &len2));
    if (len2 == 0) {
      return SPARKEY_SUCCESS;
    }
    memcpy(buf, buf2, len2);
    buf += len2;
    *len += len2;
    maxlen -= len2;
  }
  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_logiter_keycmp(sparkey_logiter *iter1, sparkey_logiter *iter2, sparkey_logreader *log, int *res) {
  uint8_t *first;
  uint64_t first_len;
  uint8_t *second;
  uint64_t second_len;

  RETHROW(sparkey_logiter_keychunk(iter1, log, 1 << 30, &first, &first_len));
  RETHROW(sparkey_logiter_keychunk(iter2, log, 1 << 30, &second, &second_len));

  while (1) {
    if (first_len == 0 && second_len == 0) {
      break;
    }
    if (first_len == 0) {
      *res = -1;
      return SPARKEY_SUCCESS;
    }
    if (second_len == 0) {
      *res = 1;
      return SPARKEY_SUCCESS;
    }

    uint64_t cmp_len = min64(first_len, second_len);
    int v = memcmp(first, second, cmp_len);
    if (v) {
      *res = v;
      return SPARKEY_SUCCESS;
    }
    first += cmp_len;
    first_len -= cmp_len;
    second += cmp_len;
    second_len -= cmp_len;

    if (first_len == 0) {
      RETHROW(sparkey_logiter_keychunk(iter1, log, 1 << 30, &first, &first_len));
    }
    if (second_len == 0) {
      RETHROW(sparkey_logiter_keychunk(iter2, log, 1 << 30, &second, &second_len));
    }
  }
  *res = 0;
  return SPARKEY_SUCCESS;
}


uint64_t sparkey_logreader_maxkeylen(sparkey_logreader *log) {
  return log->header.max_key_len;
}

uint64_t sparkey_logreader_maxvaluelen(sparkey_logreader *log) {
  return log->header.max_value_len;
}

int sparkey_logreader_get_compression_blocksize(sparkey_logreader *log) {
  return log->header.compression_block_size;
}

sparkey_compression_type sparkey_logreader_get_compression_type(sparkey_logreader *log) {
  return log->header.compression_type;
}

sparkey_iter_state sparkey_logiter_state(sparkey_logiter *iter) {
  return iter->state;
}

sparkey_entry_type sparkey_logiter_type(sparkey_logiter *iter) {
  return iter->type;
}

uint64_t sparkey_logiter_keylen(sparkey_logiter *iter) {
  return iter->keylen;
}

uint64_t sparkey_logiter_valuelen(sparkey_logiter *iter) {
  return iter->valuelen;
}

