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
#ifndef SPARKEY_LOGHEADER_H_INCLUDED
#define SPARKEY_LOGHEADER_H_INCLUDED

#include <stdint.h>

#include "sparkey.h"

#define LOG_MAGIC_NUMBER (0x49b39c95)
#define LOG_MAJOR_VERSION (1)
#define LOG_MINOR_VERSION (0)
#define LOG_HEADER_SIZE (84)

typedef struct {
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t file_identifier;
  uint64_t num_puts;
  uint64_t num_deletes;
  uint64_t data_end;
  uint64_t max_key_len;
  uint64_t max_value_len;
  uint64_t delete_size;
  sparkey_compression_type compression_type;
  uint32_t compression_block_size;
  uint64_t put_size;
  uint32_t header_size;
  uint32_t max_entries_per_block;
} sparkey_logheader;

/**
 * fills up a logheader struct based on the contents at the beginning of the file.
 * @param header header struct to fill
 * @param filename a log file
 * @returns an error code if it could not load the file.
 */
sparkey_returncode sparkey_load_logheader(sparkey_logheader *header, const char *filename);

/**
 * Dumps a human readable representation of the header to stdout
 * @param header an initialized header struct
 */
void print_logheader(sparkey_logheader *header);

/**
 * Writes a header to the current position in the file
 * @param fd a file descripter pointing to a file open for writing
 * @param header the header to write
 * @returns an error code if it could not write to file.
 */
sparkey_returncode write_logheader(int fd, sparkey_logheader *header);

#endif

