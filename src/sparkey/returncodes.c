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
#include "sparkey.h"

const char * sparkey_errstring(sparkey_returncode code) {
  switch (code) {
  case SPARKEY_SUCCESS: return "Success";
  case SPARKEY_INTERNAL_ERROR: return "Internal error";
  case SPARKEY_FILE_NOT_FOUND: return "File not found";
  case SPARKEY_PERMISSION_DENIED: return "Permission denied";
  case SPARKEY_TOO_MANY_OPEN_FILES: return "Too many open files";
  case SPARKEY_FILE_TOO_LARGE: return "File is too large";
  case SPARKEY_FILE_ALREADY_EXISTS: return "File already exists";
  case SPARKEY_FILE_BUSY: return "File is busy";
  case SPARKEY_FILE_IS_DIRECTORY: return "File is a directory";
  case SPARKEY_FILE_SIZE_EXCEEDED: return "Maximum file size exceeded";
  case SPARKEY_FILE_CLOSED: return "File is closed";
  case SPARKEY_OUT_OF_DISK: return "Out of free disk space";
  case SPARKEY_UNEXPECTED_EOF: return "Encountered unexpected end of file";
  case SPARKEY_MMAP_FAILED: return "mmap failed - running on 32 bit system?";

  case SPARKEY_WRONG_LOG_MAGIC_NUMBER: return "Wrong magic number of log file";
  case SPARKEY_WRONG_LOG_MAJOR_VERSION: return "Wrong major version of log file";
  case SPARKEY_UNSUPPORTED_LOG_MINOR_VERSION: return "Unsupported minor version of log file";
  case SPARKEY_LOG_TOO_SMALL: return "Corrupt log file - smaller than the header indicates";
  case SPARKEY_LOG_CLOSED: return "Log file is closed";
  case SPARKEY_LOG_ITERATOR_INACTIVE: return "Log iterator is inactive";
  case SPARKEY_LOG_ITERATOR_MISMATCH: return "The iterator is not associated with the log";
  case SPARKEY_LOG_ITERATOR_CLOSED: return "Log iterator is closed";
  case SPARKEY_LOG_HEADER_CORRUPT: return "Log header is corrupt";

  case SPARKEY_INVALID_COMPRESSION_BLOCK_SIZE: return "Invalid compression block size";
  case SPARKEY_INVALID_COMPRESSION_TYPE: return "Invalid compression type";

  case SPARKEY_WRONG_HASH_MAGIC_NUMBER: return "Wrong magic number of hash file";
  case SPARKEY_WRONG_HASH_MAJOR_VERSION: return "Wrong major version of hash file";
  case SPARKEY_UNSUPPORTED_HASH_MINOR_VERSION: return "Unsupported minor version of hash file";
  case SPARKEY_HASH_TOO_SMALL: return "Corrupt hash file - smaller than the header indicates";
  case SPARKEY_HASH_CLOSED: return "Hash file is closed";
  case SPARKEY_FILE_IDENTIFIER_MISMATCH: return "File identifier differs between hash file and log file";
  case SPARKEY_HASH_HEADER_CORRUPT: return "Hash header is corrupt";
  case SPARKEY_HASH_SIZE_INVALID: return "Hash size is invalid";

  default: return "Unknown error";
  }
}

