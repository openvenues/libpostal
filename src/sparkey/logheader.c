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
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include "logheader.h"
#include "endiantools.h"
#include "util.h"

static char * compression_types[] = { "Uncompressed", "Snappy", NULL };

void print_logheader(sparkey_logheader *header) {
  printf("Log file version %d.%d\n", header->major_version,
      header->minor_version);
  printf("Identifier: %08x\n", header->file_identifier);
  printf("Puts: %"PRIu64", Deletes: %"PRIu64"\n", header->num_puts, header->num_deletes);
  printf("Max key size: %"PRIu64", Max value size: %"PRIu64"\n", header->max_key_len, header->max_value_len);
  printf("Compression: %s, block size: %d\n",
      compression_types[header->compression_type],
      header->compression_block_size);
}

static sparkey_returncode logheader_version0(sparkey_logheader *header, FILE *fp) {
  RETHROW(fread_little_endian32(fp, &header->file_identifier));
  RETHROW(fread_little_endian64(fp, &header->num_puts));
  RETHROW(fread_little_endian64(fp, &header->num_deletes));
  RETHROW(fread_little_endian64(fp, &header->data_end));
  RETHROW(fread_little_endian64(fp, &header->max_key_len));
  RETHROW(fread_little_endian64(fp, &header->max_value_len));
  RETHROW(fread_little_endian64(fp, &header->delete_size));
  RETHROW(fread_little_endian32(fp, &header->compression_type));
  RETHROW(fread_little_endian32(fp, &header->compression_block_size));
  RETHROW(fread_little_endian64(fp, &header->put_size));
  RETHROW(fread_little_endian32(fp, &header->max_entries_per_block));
  header->header_size = LOG_HEADER_SIZE;

  // Some basic consistency checks
  if (header->data_end < header->header_size) {
    return SPARKEY_LOG_HEADER_CORRUPT;
  }
  if (header->num_puts > header->data_end) {
    return SPARKEY_LOG_HEADER_CORRUPT;
  }
  if (header->num_deletes > header->data_end) {
    return SPARKEY_LOG_HEADER_CORRUPT;
  }
  if (header->compression_type > SPARKEY_COMPRESSION_SNAPPY) {
    return SPARKEY_LOG_HEADER_CORRUPT;
  }
  return SPARKEY_SUCCESS;
}


typedef sparkey_returncode (*loader)(sparkey_logheader *header, FILE *fp);

static loader loaders[1] = { logheader_version0 };

sparkey_returncode sparkey_load_logheader(sparkey_logheader *header, const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    return sparkey_open_returncode(errno);
  }

  uint32_t tmp;
  RETHROW(fread_little_endian32(fp, &tmp));
  if (tmp != LOG_MAGIC_NUMBER) {
    fclose(fp);
    return SPARKEY_WRONG_LOG_MAGIC_NUMBER;
  }
  RETHROW(fread_little_endian32(fp, &header->major_version));
  if (header->major_version != LOG_MAJOR_VERSION) {
    fclose(fp);
    return SPARKEY_WRONG_LOG_MAJOR_VERSION;
  }
  RETHROW(fread_little_endian32(fp, &header->minor_version));
  if (header->minor_version > LOG_MINOR_VERSION) {
    fclose(fp);
    return SPARKEY_UNSUPPORTED_LOG_MINOR_VERSION;
  }
  int version = header->minor_version;
  loader l = loaders[version];
  if (l == NULL) {
    fclose(fp);
    return SPARKEY_INTERNAL_ERROR;
  }
  sparkey_returncode x = (*l)(header, fp);
  fclose(fp);
  return x;
}

sparkey_returncode write_logheader(int fd, sparkey_logheader *header) {
  RETHROW(fwrite_little_endian32(fd, LOG_MAGIC_NUMBER));
  RETHROW(fwrite_little_endian32(fd, LOG_MAJOR_VERSION));
  RETHROW(fwrite_little_endian32(fd, LOG_MINOR_VERSION));
  RETHROW(fwrite_little_endian32(fd, header->file_identifier));
  RETHROW(fwrite_little_endian64(fd, header->num_puts));
  RETHROW(fwrite_little_endian64(fd, header->num_deletes));
  RETHROW(fwrite_little_endian64(fd, header->data_end));
  RETHROW(fwrite_little_endian64(fd, header->max_key_len));
  RETHROW(fwrite_little_endian64(fd, header->max_value_len));
  RETHROW(fwrite_little_endian64(fd, header->delete_size));
  RETHROW(fwrite_little_endian32(fd, header->compression_type));
  RETHROW(fwrite_little_endian32(fd, header->compression_block_size));
  RETHROW(fwrite_little_endian64(fd, header->put_size));
  RETHROW(fwrite_little_endian32(fd, header->max_entries_per_block));
  return SPARKEY_SUCCESS;
}


