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

#include "hashheader.h"
#include "endiantools.h"
#include "util.h"
#include "sparkey.h"

void print_hashheader(sparkey_hashheader *header) {
  printf("Hash file version %d.%d\n", header->major_version, header->minor_version);
  printf("Identifier: %08x\n", header->file_identifier);
  printf("Max key size: %"PRIu64", Max value size: %"PRIu64"\n", header->max_key_len, header->max_value_len);
  printf("Hash size: %d bit Murmurhash3\n", 8*header->hash_size);
  printf("Num entries: %"PRIu64", Capacity: %"PRIu64"\n", header->num_entries, header->hash_capacity);
  printf("Num collisions: %"PRIu64", Max displacement: %"PRIu64", Average displacement: %.2f\n", header->hash_collisions, header->max_displacement, (double) header->total_displacement / (double) header->num_entries);
  printf("Data size: %"PRIu64", Garbage size: %"PRIu64"\n", header->data_end, header->garbage_size);
}

static sparkey_returncode hashheader_version0(sparkey_hashheader *header, FILE *fp) {
  RETHROW(fread_little_endian32(fp, &header->file_identifier));
  RETHROW(fread_little_endian32(fp, &header->hash_seed));
  RETHROW(fread_little_endian64(fp, &header->data_end));
  RETHROW(fread_little_endian64(fp, &header->max_key_len));
  RETHROW(fread_little_endian64(fp, &header->max_value_len));
  RETHROW(fread_little_endian64(fp, &header->num_puts));
  RETHROW(fread_little_endian64(fp, &header->garbage_size));
  RETHROW(fread_little_endian64(fp, &header->num_entries));

  RETHROW(fread_little_endian32(fp, &header->address_size));
  RETHROW(fread_little_endian32(fp, &header->hash_size));
  RETHROW(fread_little_endian64(fp, &header->hash_capacity));
  RETHROW(fread_little_endian64(fp, &header->max_displacement));
  RETHROW(fread_little_endian32(fp, &header->entry_block_bits));
  header->entry_block_bitmask = (1 << header->entry_block_bits) - 1;
  RETHROW(fread_little_endian64(fp, &header->hash_collisions));
  RETHROW(fread_little_endian64(fp, &header->total_displacement));
  header->header_size = HASH_HEADER_SIZE;

  header->hash_algorithm = sparkey_get_hash_algorithm(header->hash_size);
  if (header->hash_algorithm.hash == NULL) {
    return SPARKEY_HASH_HEADER_CORRUPT;
  }
  // Some basic consistency checks
  if (header->num_entries > header->num_puts) {
    return SPARKEY_HASH_HEADER_CORRUPT;
  }
  if (header->max_displacement > header->num_entries) {
    return SPARKEY_HASH_HEADER_CORRUPT;
  }
  if (header->hash_collisions > header->num_entries) {
    return SPARKEY_HASH_HEADER_CORRUPT;
  }

  return SPARKEY_SUCCESS;
}


typedef sparkey_returncode (*loader)(sparkey_hashheader *header, FILE *fp);

static loader loaders[2] = { hashheader_version0, hashheader_version0 };

sparkey_returncode sparkey_load_hashheader(sparkey_hashheader *header, const char *filename) {
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		return sparkey_open_returncode(errno);
	}

	uint32_t tmp;
	RETHROW(fread_little_endian32(fp, &tmp));
	if (tmp != HASH_MAGIC_NUMBER) {
		fclose(fp);
		return SPARKEY_WRONG_HASH_MAGIC_NUMBER;
	}
	RETHROW(fread_little_endian32(fp, &header->major_version));
	if (header->major_version != HASH_MAJOR_VERSION) {
		fclose(fp);
		return SPARKEY_WRONG_HASH_MAJOR_VERSION;
	}
	RETHROW(fread_little_endian32(fp, &header->minor_version));
	if (header->minor_version > HASH_MINOR_VERSION) {
		fclose(fp);
		return SPARKEY_UNSUPPORTED_HASH_MINOR_VERSION;
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

sparkey_returncode write_hashheader(int fd, sparkey_hashheader *header) {
  RETHROW(fwrite_little_endian32(fd, HASH_MAGIC_NUMBER));
  RETHROW(fwrite_little_endian32(fd, HASH_MAJOR_VERSION));
  RETHROW(fwrite_little_endian32(fd, HASH_MINOR_VERSION));
  RETHROW(fwrite_little_endian32(fd, header->file_identifier));
  RETHROW(fwrite_little_endian32(fd, header->hash_seed));
  RETHROW(fwrite_little_endian64(fd, header->data_end));
  RETHROW(fwrite_little_endian64(fd, header->max_key_len));
  RETHROW(fwrite_little_endian64(fd, header->max_value_len));
  RETHROW(fwrite_little_endian64(fd, header->num_puts));
  RETHROW(fwrite_little_endian64(fd, header->garbage_size));
  RETHROW(fwrite_little_endian64(fd, header->num_entries));
  RETHROW(fwrite_little_endian32(fd, header->address_size));
  RETHROW(fwrite_little_endian32(fd, header->hash_size));
  RETHROW(fwrite_little_endian64(fd, header->hash_capacity));
  RETHROW(fwrite_little_endian64(fd, header->max_displacement));
  RETHROW(fwrite_little_endian32(fd, header->entry_block_bits));
  RETHROW(fwrite_little_endian64(fd, header->hash_collisions));
  RETHROW(fwrite_little_endian64(fd, header->total_displacement));

  return SPARKEY_SUCCESS;
}



