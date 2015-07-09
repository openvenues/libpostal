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
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "endiantools.h"
#include "buf.h"

sparkey_returncode buf_init(sparkey_buf *buf, ptrdiff_t size) {
  buf->start = malloc(size);
  if (buf->start == NULL) {
    return SPARKEY_INTERNAL_ERROR;
  }
  buf->cur = buf->start;
  buf->end = buf->start + size;
  return SPARKEY_SUCCESS;
}

void buf_close(sparkey_buf *buf) {
  free(buf->start);
  buf->start = NULL;
  buf->cur = NULL;
  buf->end = NULL;
}

uint64_t buf_size(sparkey_buf *buf) {
  return buf->end - buf->start;
}

uint64_t buf_remaining(sparkey_buf *buf) {
  return buf->end - buf->cur;
}

uint64_t buf_used(sparkey_buf *buf) {
  return buf->cur - buf->start;
}

sparkey_returncode buf_flushfile(sparkey_buf *buf, int fd) {
  RETHROW(write_full(fd, buf->start, buf_used(buf)));
  buf->cur = buf->start;
  return SPARKEY_SUCCESS;
}

sparkey_returncode buf_add(sparkey_buf *buf, int fd, const uint8_t *data, ptrdiff_t len) {
  while (1) {
    ptrdiff_t remaining = buf_remaining(buf);
    if (remaining >= len) {
      memcpy(buf->cur, data, len);
      buf->cur += len;
      return SPARKEY_SUCCESS;
    } else {
      memcpy(buf->cur, data, remaining);
      buf->cur += remaining;
      data += remaining;
      len -= remaining;
      RETHROW(buf_flushfile(buf, fd));
    }
  }
  return SPARKEY_SUCCESS;
}

