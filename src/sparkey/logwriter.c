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
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <snappy-c.h>

#include "util.h"
#include "sparkey.h"
#include "logheader.h"
#include "endiantools.h"
#include "buf.h"
#include "sparkey-internal.h"

#define MAGIC_VALUE_LOGWRITER (0x2866211b)


static inline int write_vlq(uint8_t *buf, uint64_t value) {
  int count = 1;
  while (value >= 1 << 7) {
    *buf = (value & 0x7f) | 0x80;
    value >>= 7;
    count++;
    buf++;
  }
  *buf = value;
  return count;
}

static sparkey_returncode assert_writer_open(sparkey_logwriter *log) {
  if (log->open_status != MAGIC_VALUE_LOGWRITER) {
    return SPARKEY_LOG_CLOSED;
  }
  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_logwriter_create(sparkey_logwriter **log_ref, const char *filename, sparkey_compression_type compression_type, int compression_block_size) {
  sparkey_returncode returncode;
  int fd = 0;
  sparkey_logwriter *l = malloc(sizeof(sparkey_logwriter));
  if (l == NULL) {
    TRY(SPARKEY_INTERNAL_ERROR, error);
  }
  switch (compression_type) {
  case SPARKEY_COMPRESSION_NONE:
    compression_block_size = 0;
    l->compressed = NULL;
    break;
  case SPARKEY_COMPRESSION_SNAPPY:
    if (compression_block_size < 10) {
      TRY(SPARKEY_INVALID_COMPRESSION_BLOCK_SIZE, error);
    }
    l->max_compressed_size = snappy_max_compressed_length(compression_block_size);
    l->compressed = malloc(l->max_compressed_size);
    if (l->compressed == NULL) {
      TRY(SPARKEY_INTERNAL_ERROR, error);
    }
    break;
  default:
    TRY(SPARKEY_INVALID_COMPRESSION_TYPE, error);
  }

  // Try removing it first, to avoid overwriting existing files that readers may be using.
  if (remove(filename) < 0) {
    int e = errno;
    if (e != ENOENT) {
      TRY(sparkey_remove_returncode(e), error);
    }
  }

  fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 00644);
  if (fd == -1) {
    TRY(sparkey_create_returncode(errno), error);
  }
  l->fd = fd;

  l->header.compression_block_size = compression_block_size;
  l->header.compression_type = compression_type;

  TRY(rand32(&(l->header.file_identifier)), error);
  l->header.data_end = LOG_HEADER_SIZE;
  l->header.major_version = LOG_MAJOR_VERSION;
  l->header.minor_version = LOG_MINOR_VERSION;
  l->header.put_size = 0;
  l->header.delete_size = 0;
  l->header.num_puts = 0;
  l->header.num_deletes = 0;
  l->header.max_entries_per_block = 0;
  l->header.max_key_len = 0;
  l->header.max_value_len = 0;

  TRY(write_logheader(fd, &l->header), error);
  off_t pos = lseek(fd, 0, SEEK_CUR);
  if (pos != LOG_HEADER_SIZE) {
    TRY(SPARKEY_INTERNAL_ERROR, error);
  }

  TRY(buf_init(&l->file_buf, 1024*1024), error);
  TRY(buf_init(&l->block_buf, compression_block_size), error);

  l->entry_count = 0;

  l->open_status = MAGIC_VALUE_LOGWRITER;
  *log_ref = l;
  return SPARKEY_SUCCESS;
error:
  free(l);
  if (fd > 0) close(fd);
  return returncode;
}

sparkey_returncode sparkey_logwriter_append(sparkey_logwriter **log_ref, const char *filename) {
  sparkey_returncode returncode;
  int fd = 0;
  sparkey_logwriter *log = malloc(sizeof(sparkey_logwriter));
  if (log == NULL) {
    TRY(SPARKEY_INTERNAL_ERROR, error);
  }
  TRY(sparkey_load_logheader(&log->header, filename), error);

  if (log->header.major_version != LOG_MAJOR_VERSION) {
    TRY(SPARKEY_WRONG_LOG_MAJOR_VERSION, error);
  }
  if (log->header.minor_version != LOG_MINOR_VERSION) {
    TRY(SPARKEY_UNSUPPORTED_LOG_MINOR_VERSION, error);
  }

  switch (log->header.compression_type) {
  case SPARKEY_COMPRESSION_NONE:
    log->header.compression_block_size = 0;
    log->compressed = NULL;
    break;
  case SPARKEY_COMPRESSION_SNAPPY:
    if (log->header.compression_block_size < 10) {
      TRY(SPARKEY_INVALID_COMPRESSION_BLOCK_SIZE, error);
    }
    log->max_compressed_size = snappy_max_compressed_length(log->header.compression_block_size);
    log->compressed = malloc(log->max_compressed_size);
    break;
  default:
    TRY(SPARKEY_INVALID_COMPRESSION_TYPE, error);
  }

  fd = open(filename, O_WRONLY, 00644);
  if (fd == -1) {
    int e = errno;
    TRY(sparkey_create_returncode(e), error);
  }
  log->fd = fd;

  lseek(fd, log->header.data_end, SEEK_SET);

  TRY(buf_init(&log->file_buf, 1024*1024), error);
  TRY(buf_init(&log->block_buf, log->header.compression_block_size), error);

  log->entry_count = 0;

  log->open_status = MAGIC_VALUE_LOGWRITER;
  *log_ref = log;
  return SPARKEY_SUCCESS;
error:
  free(log);
  if (fd > 0) close(fd);
  return returncode;
}

static sparkey_returncode flush_snappy(sparkey_logwriter *log) {
  log->flushed = 1;
  if (log->entry_count > (int) log->header.max_entries_per_block) {
    log->header.max_entries_per_block = log->entry_count;
  }
  log->entry_count = 0;
  sparkey_buf *block_buf = &log->block_buf;
  uint8_t *compressed = log->compressed;
  uint32_t max_compressed_size = log->max_compressed_size;
  sparkey_buf *file_buf = &log->file_buf;
  int fd = log->fd;

  size_t compressed_size = max_compressed_size;
  snappy_status status = snappy_compress((char *) block_buf->start, buf_used(block_buf), (char *) compressed, &compressed_size);
  switch (status) {
  case SNAPPY_OK: break;
  case SNAPPY_INVALID_INPUT:
  case SNAPPY_BUFFER_TOO_SMALL:
  default:
    return SPARKEY_INTERNAL_ERROR;
  }
  uint8_t buf1[10];
  ptrdiff_t written1 = write_vlq(buf1, compressed_size);
  RETHROW(buf_add(file_buf, fd, buf1, written1));
  RETHROW(buf_add(file_buf, fd, compressed, compressed_size));
  block_buf->cur = block_buf->start;
  return SPARKEY_SUCCESS;
}


sparkey_returncode sparkey_logwriter_flush(sparkey_logwriter *log) {
  RETHROW(assert_writer_open(log));
  if (buf_used(&log->block_buf) > 0) {
    RETHROW(flush_snappy(log));
  }
  if (buf_used(&log->file_buf) > 0) {
    RETHROW(buf_flushfile(&log->file_buf, log->fd));
  }
  off_t pos = lseek(log->fd, 0, SEEK_CUR);
  log->header.data_end = pos;
  lseek(log->fd, 0, SEEK_SET);
  RETHROW(write_logheader(log->fd, &log->header));
  lseek(log->fd, pos, SEEK_SET);

  /* Can't build fsync support on lenny */
  /* fsync(log->fd); */
  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_logwriter_close(sparkey_logwriter **log) {
  sparkey_logwriter *l = *log;
  if (l->open_status != MAGIC_VALUE_LOGWRITER) {
    return SPARKEY_SUCCESS;
  }

  RETHROW(sparkey_logwriter_flush(l));
  close(l->fd);
  buf_close(&l->file_buf);
  buf_close(&l->block_buf);
  if (l->compressed != NULL) {
    free(l->compressed);
  }

  l->open_status = 0;
  free(l);
  *log = NULL;
  return SPARKEY_SUCCESS;
}

static sparkey_returncode snappy_add(sparkey_logwriter *log, const uint8_t *data, ptrdiff_t len) {
  sparkey_buf *block_buf = &log->block_buf;

  while (1) {
    ptrdiff_t remaining = buf_remaining(block_buf);
    if (remaining >= len) {
      memcpy(block_buf->cur, data, len);
      block_buf->cur += len;
      return SPARKEY_SUCCESS;
    } else {
      memcpy(block_buf->cur, data, remaining);
      block_buf->cur += remaining;
      data += remaining;
      len -= remaining;
      RETHROW(flush_snappy(log));
    }
  }
  return SPARKEY_SUCCESS;
}


static sparkey_returncode log_add(sparkey_logwriter *log, uint64_t num1, uint64_t num2, uint64_t len1, const uint8_t *data1, uint64_t len2, const uint8_t *data2, ptrdiff_t *datasize) {
  uint8_t buf1[10];
  uint8_t buf2[10];

  uint64_t written1 = write_vlq(buf1, num1);
  uint64_t written2 = write_vlq(buf2, num2);

  *datasize = written1 + written2 + len1 + len2;
  uint64_t remaining;
  switch (log->header.compression_type) {
  case SPARKEY_COMPRESSION_NONE:
    RETHROW(buf_add(&log->file_buf, log->fd, buf1, written1));
    RETHROW(buf_add(&log->file_buf, log->fd, buf2, written2));
    RETHROW(buf_add(&log->file_buf, log->fd, data1, len1));
    RETHROW(buf_add(&log->file_buf, log->fd, data2, len2));
    break;
  case SPARKEY_COMPRESSION_SNAPPY:
    remaining = buf_remaining(&log->block_buf);
    // todo: make it smarter by checking if it's better to flush directly
    uint64_t fits_in_one = written1 + written2 + len1 + len2 <= buf_size(&log->block_buf);
    uint64_t doesnt_fit_this = written1 + written2 + len1 + len2 > buf_remaining(&log->block_buf);
    if ((remaining < written1 + written2) || (fits_in_one && doesnt_fit_this)) {
      RETHROW(flush_snappy(log));
    }
    log->entry_count++;
    log->flushed = 0;
    RETHROW(snappy_add(log, buf1, written1));
    RETHROW(snappy_add(log, buf2, written2));
    RETHROW(snappy_add(log, data1, len1));
    RETHROW(snappy_add(log, data2, len2));
    if (log->flushed && buf_used(&log->block_buf) > 0) {
      RETHROW(flush_snappy(log));
    }
    break;
  default:
    return SPARKEY_INTERNAL_ERROR;
  }
  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_logwriter_put(sparkey_logwriter *log, uint64_t keylen, const uint8_t *key, uint64_t valuelen, const uint8_t *value) {
  RETHROW(assert_writer_open(log));
  ptrdiff_t datasize;
  RETHROW(log_add(log, keylen + 1, valuelen, keylen, key, valuelen, value, &datasize));

  log->header.num_puts++;
  log->header.put_size += datasize;
  if (keylen > log->header.max_key_len) {
    log->header.max_key_len = keylen;
  }
  if (valuelen > log->header.max_value_len) {
    log->header.max_value_len = valuelen;
  }
  return SPARKEY_SUCCESS;
}

sparkey_returncode sparkey_logwriter_delete(sparkey_logwriter *log, uint64_t keylen, const uint8_t *key) {
  RETHROW(assert_writer_open(log));
  ptrdiff_t datasize;
  RETHROW(log_add(log, 0, keylen, 0, NULL, keylen, key, &datasize));

  log->header.num_deletes++;
  log->header.delete_size += datasize;
  return SPARKEY_SUCCESS;
}

