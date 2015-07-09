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
#ifndef SPARKEY_INTERNAL_H
#define SPARKEY_INTERNAL_H
#include <stdint.h>

#include "sparkey.h"

#include "logheader.h"
#include "hashheader.h"
#include "buf.h"

struct sparkey_logreader {
  uint32_t open_status;
  sparkey_logheader header;
  int fd;

  uint64_t data_len;
  uint8_t *data;
};

struct sparkey_logiter {
  uint32_t open_status;
  uint32_t file_identifier;

  // position in reader
  uint64_t block_position;
  uint64_t next_block_position;
  uint64_t block_offset;
  uint64_t block_len;
  int entry_count;

  // compression buffer
  int compression_buf_allocated;
  uint8_t *compression_buf;

  // current entry
  uint64_t entry_block_position;
  uint64_t entry_block_offset;
  sparkey_entry_type type;
  sparkey_iter_state state;
  uint64_t keylen;
  uint64_t valuelen;
  uint64_t key_remaining;
  uint64_t value_remaining;
};

struct sparkey_logwriter {
  uint32_t open_status;
  sparkey_logheader header;
  int fd;

  sparkey_buf block_buf;
  uint32_t max_compressed_size;
  uint8_t *compressed;
  sparkey_buf file_buf;
  int flushed;

  int entry_count;
};

struct sparkey_hashreader {
  uint32_t open_status;
  sparkey_hashheader header;
  sparkey_logreader log;

  int fd;

  uint64_t data_len;
  uint8_t *data;

};

sparkey_returncode sparkey_logreader_open_noalloc(sparkey_logreader *log, const char *filename);
void sparkey_logreader_close_nodealloc(sparkey_logreader *log);

#endif
