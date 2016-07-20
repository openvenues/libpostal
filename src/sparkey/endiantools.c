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
#if defined(__linux)
#   include <byteswap.h>
#elif defined(__APPLE__)
#   include <libkern/OSByteOrder.h>
#   define bswap_32 OSSwapInt32
#   define bswap_64 OSSwapInt64
#elif defined(__OpenBSD__)
#   include <endian.h>
#   define bswap_32 swap32
#   define bswap_64 swap64
#else
#   error "no byteswap.h or libkern/OSByteOrder.h"
#endif

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include "util.h"
#include "endiantools.h"
#include "sparkey.h"

static sparkey_returncode _write_full(int fd, uint8_t *buf, size_t count) {
  while (count > 0) {
    ssize_t actual = write(fd, buf, count);
    if (actual < 0) {
      switch (errno) {
      case EINTR:
      case EAGAIN: continue;
      case ENOSPC: return SPARKEY_OUT_OF_DISK;
      case EFBIG: return SPARKEY_FILE_SIZE_EXCEEDED;
      case EBADF: return SPARKEY_FILE_CLOSED;
      default:
        fprintf(stderr, "_write_full():%d bug: actual_written = %"PRIu64", wanted = %"PRIu64", errno = %d\n", __LINE__, (uint64_t)actual, (uint64_t)count, errno);
        return SPARKEY_INTERNAL_ERROR;
      }
    }
    count -= actual;
    buf += actual;
  }
  return SPARKEY_SUCCESS;
}

sparkey_returncode write_full(int fd, uint8_t *buf, size_t count) {
  const size_t block_size = 256*1024*1024;
  size_t fullruns = count / block_size;
  while (fullruns > 0) {
    RETHROW(_write_full(fd, buf, block_size));
    buf += block_size;
    fullruns--;
  }
  return _write_full(fd, buf, count % block_size);
}

void write_little_endian32(uint8_t *buf, uint32_t value) {
  buf[0] = (value >> 0) & 0xFF;
  buf[1] = (value >> 8) & 0xFF;
  buf[2] = (value >> 16) & 0xFF;
  buf[3] = (value >> 24) & 0xFF;
}

sparkey_returncode fwrite_little_endian32(int fd, uint32_t value) {
  uint8_t buf[4];
  write_little_endian32(buf, value);
  return write_full(fd, buf, 4);
}

void write_little_endian64(uint8_t *buf, uint64_t value) {
  buf[0] = (value >> 0) & 0xFF;
  buf[1] = (value >> 8) & 0xFF;
  buf[2] = (value >> 16) & 0xFF;
  buf[3] = (value >> 24) & 0xFF;
  buf[4] = (value >> 32) & 0xFF;
  buf[5] = (value >> 40) & 0xFF;
  buf[6] = (value >> 48) & 0xFF;
  buf[7] = (value >> 56) & 0xFF;
}

sparkey_returncode fwrite_little_endian64(int fd, uint64_t value) {
  uint8_t buf[8];
  write_little_endian64(buf, value);
  return write_full(fd, buf, 8);
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || defined(__LITTLE_ENDIAN) || defined(__LITTLE_ENDIAN__)
uint32_t read_little_endian32(const uint8_t * array, uint64_t pos) {
  return *((uint32_t*)(array + pos));
}

uint64_t read_little_endian64(const uint8_t * array, uint64_t pos) {
  return *((uint64_t*)(array + pos));
}
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ || defined(__BIG_ENDIAN) || defined(__BIG_ENDIAN__)
uint32_t read_little_endian32(const uint8_t * array, uint64_t pos) {
  return bswap_32(*((uint32_t*)(array + pos)));
}

uint64_t read_little_endian64(const uint8_t * array, uint64_t pos) {
  return bswap_64(*((uint64_t*)(array + pos)));
}
#else
#error "none of __LITTLE_ENDIAN, __LITTLE_ENDIAN__, __BIG_ENDIAN, __BIG_ENDIAN__ is defined"
#endif


sparkey_returncode correct_endian_platform() {
	return SPARKEY_SUCCESS;
}

sparkey_returncode fread_little_endian32(FILE *fp, uint32_t *res) {
  uint8_t data[4];
  int count = fread(data, 4, 1, fp);
  if (count < 1) {
    return SPARKEY_UNEXPECTED_EOF;
  }
  *res = read_little_endian32(data, 0);
  return SPARKEY_SUCCESS;
}

sparkey_returncode fread_little_endian64(FILE *fp, uint64_t *res) {
  uint8_t data[8];
  int count = fread(data, 8, 1, fp);
  if (count < 1) {
    return SPARKEY_UNEXPECTED_EOF;
  }
  *res = read_little_endian64(data, 0);
  return SPARKEY_SUCCESS;
}

