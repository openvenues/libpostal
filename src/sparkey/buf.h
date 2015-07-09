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
#ifndef BUF_H_INCLUDED
#define BUF_H_INCLUDED

#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "endiantools.h"

typedef struct {
  uint8_t *start;
  uint8_t *cur;
  uint8_t *end;
} sparkey_buf;

sparkey_returncode buf_init(sparkey_buf *buf, ptrdiff_t size);

void buf_close(sparkey_buf *buf);

uint64_t buf_size(sparkey_buf *buf);

uint64_t buf_remaining(sparkey_buf *buf);

uint64_t buf_used(sparkey_buf *buf);

sparkey_returncode buf_flushfile(sparkey_buf *buf, int fd);

sparkey_returncode buf_add(sparkey_buf *buf, int fd, const uint8_t *data, ptrdiff_t len);

#endif

