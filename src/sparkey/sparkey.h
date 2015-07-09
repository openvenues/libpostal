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
#ifndef SPOTIFY_SPARKEY_H_INCLUDED
#define SPOTIFY_SPARKEY_H_INCLUDED

/**
 * \mainpage Sparkey C API
 * \section intro_sec Getting started
 *
 * For a complete listing of available functions, see sparkey.h .
 *
 * \section logwriter Writing to a log file
 *
 * This section contains all functions relevant for writing entries to a log.
 * Writing to the same log file is not thread safe. Only use the writer objects
 * from one thread at a time, and make sure to only write to a file from one process.
 * The library will not do any form of locking or checking for other writers, so be
 * careful.
 *
 * Basic workflow:
 * - Create and initialize the logwriter:
 * \code
 * sparkey_logwriter *mywriter;
 * sparkey_returncode returncode = sparkey_logwriter_create(&mywriter, "mylog.spl", SPARKEY_COMPRESSION_NONE, 0);
 * // TODO: check the returncode
 * \endcode
 * - Write to the log:
 * \code
 * const char *mykey = "mykey";
 * const char *myvalue = "this is my value";
 * sparkey_returncode returncode = sparkey_logwriter_put(mywriter, strlen(mykey), (uint8_t*)mykey, strlen(myvalue), (uint8_t*)myvalue);
 * // TODO: check the returncode
 * \endcode
 * - Close it when you're done:
 * \code
 * sparkey_returncode returncode = sparkey_logwriter_close(&mywriter);
 * // TODO: check the returncode
 * \endcode
 *
 * \section logreader Reading from a log file
 *
 * This section contains all functions relevant for reading entries from a log.
 * A sparkey_logreader may be shared between multiple threads, but \ref sparkey_logreader_open
 * and \ref sparkey_logreader_close are not thread safe.
 *
 * The logreader is not useful by itself. You also need a sparkey_logiter to iterate through the entries.
 * This is a highly mutable struct and should not be shared between threads. It is not threadsafe.
 *
 * Here is a basic workflow for iterating through all entries in a logfile:
 * - Create a logreader
 * \code
 * sparkey_logreader *myreader;
 * sparkey_returncode returncode = sparkey_logreader_open(&myreader, "mylog.spl");
 * \endcode
 * - Create a logiter
 * \code
 * sparkey_logiter *myiter;
 * sparkey_returncode returncode = sparkey_logiter_create(&myiter, myreader);
 * \endcode
 * - Perform the iteration:
 * \code
 * while (1) {
 *   sparkey_returncode returncode = sparkey_logiter_next(myiter, myreader);
 *   // TODO: check the returncode
 *   if (sparkey_logiter_state(myiter) != SPARKEY_ITER_ACTIVE) {
 *     break;
 *   }
 *   uint64_t wanted_keylen = sparkey_logiter_keylen(myiter);
 *   uint8_t *keybuf = malloc(wanted_keylen);
 *   uint64_t actual_keylen;
 *   returncode = sparkey_logiter_fill_key(myiter, myreader, wanted_keylen, keybuf, &actual_keylen);
 *   // TODO: check the returncode
 *   // TODO: assert actual_keylen == wanted_keylen
 *   uint64_t wanted_valuelen = sparkey_logiter_valuelen(myiter);
 *   uint8_t *valuebuf = malloc(wanted_valuelen);
 *   uint64_t actual_valuelen;
 *   returncode = sparkey_logiter_fill_value(myiter, myreader, wanted_valuelen, valuebuf, &actual_valuelen);
 *   // TODO: check the returncode
 *   // TODO: assert actual_valuelen == wanted_valuelen
 *   // Do stuff with key and value
 *   free(keybuf);
 *   free(valuebuf);
 * }
 * \endcode
 * Note that you have to allocate memory for the key and value manually - Sparkey does not allocate memory except for when
 * creating readers, writers and iterators.
 *
 * - Alternatively, you can preallocate the buffers by using
 * max_key_len and max_value_len provided by the log header:
 * \code
 * uint8_t *keybuf = malloc(sparkey_logreader_maxkeylen(sparkey_hash_getreader(myreader)));
 * uint8_t *valuebuf = malloc(sparkey_logreader_maxvaluelen(sparkey_hash_getreader(myreader)));
 * while (1) {
 *   sparkey_returncode returncode = sparkey_logiter_next(&myiter, &myreader);
 *   // TODO: check the returncode
 *   if (sparkey_logiter_state(myiter) != SPARKEY_ITER_ACTIVE) {
 *     break;
 *   }
 *   uint64_t wanted_keylen = sparkey_logiter_keylen(myiter);
 *   uint64_t actual_keylen;
 *   returncode = sparkey_logiter_fill_key(&myiter, &myreader, wanted_keylen, keybuf, &actual_keylen);
 *   // TODO: check the returncode
 *   // TODO: assert actual_keylen == wanted_keylen
 *   uint64_t wanted_valuelen = sparkey_logiter_valuelen(myiter);
 *   uint64_t actual_valuelen;
 *   returncode = sparkey_logiter_fill_value(&myiter, &myreader, wanted_valuelen, valuebuf, &actual_valuelen);
 *   // TODO: check the returncode
 *   // TODO: assert actual_valuelen == wanted_valuelen
 *   // Do stuff with key and value
 * }
 * free(keybuf);
 * free(valuebuf);
 * \endcode
 * - You can also skip allocating at all, if you can process the key and/or value in chunks. Here's an example for processing a key in chunks,
 * but the same can be applied for values:
 * \code
 * uint64_t total_len = sparkey_logiter_keylen(myiter);
 * while (total_len > 0) {
 *   uint8_t *buf;
 *   uint64_t len;
 *   sparkey_returncode returncode = sparkey_logiter_keychunk(&myiter, &myreader, total_len, &buf, &len);
 *   // TODO: check the returncode
 *   // Example: use the chunks to write to standard out
 *   fwrite(buf, 1, len, stdout);
 *   total_len -= len;
 * }
 * \endcode
 * - Close everything when you're done:
 * \code
 * sparkey_logreader_close(&myreader);
 * sparkey_logiter_close(&myiter);
 * \endcode
 *
 * \section hashwriter Creating hash files from log files
 *
 * This header only contains the function sparkey_hash_write which creates a hash file.
 *
 * This is all you need to do to create a hash file based on an existing log file:
 * \code
 * sparkey_returncode returncode = sparkey_hash_write("mylog.spi", "mylog.spl", 0);
 * // TODO: check the returncode
 * \endcode
 *
 * \section hashreader Reading from a hash-file/log-file pair
 *
 * This header contains all functions relevant for reading live key/value pairs from a log and hash file.
 * The documentation is very similar to the one for reading from a log file, because this api is an extension.
 * Random lookups is the only feature that's added, and iteration simply skips dead entries.
 *
 * A sparkey_hashreader may be shared between multiple threads, but \ref sparkey_hash_open
 * and \ref sparkey_hash_close are not thread safe.
 *
 * The hashreader is not useful by itself. You also need a sparkey_logiter to do random lookups and
 * iterate through the entries.
 * This is a highly mutable struct and should not be shared between threads. It is not threadsafe.
 *
 * Here is a basic workflow for iterating through all live entries in a log and hash file:
 * - Create a hashreader
 * \code
 * sparkey_hashreader *myreader;
 * sparkey_returncode returncode = sparkey_hash_open(&myreader, "mylog.spi", "mylog.spl");
 * // TODO: check the returncode
 * \endcode
 * \code
 * sparkey_logiter *myiter;
 * sparkey_returncode returncode = sparkey_logiter_create(&myiter, sparkey_hash_getreader(myreader));
 * // TODO: check the returncode
 * \endcode
 * - Iteration is exactly as described previously, but uses \ref sparkey_logiter_hashnext instead of
 * \ref sparkey_logiter_next.
 * - Random lookup
 * \code
 * sparkey_returncode returncode = sparkey_hash_get(myreader, (uint8_t*)"mykey", 5, myiter);
 * if (sparkey_logiter_state(myiter) != SPARKEY_ITER_ACTIVE) {
 *   // Entry not found;
 * } else {
 *   // Extracting value is done the same as when iterating.
 *   uint64_t wanted_valuelen = sparkey_logiter_valuelen(myiter);
 *   uint8_t *valuebuf = malloc(wanted_valuelen);
 *   uint64_t actual_valuelen;
 *   returncode = sparkey_logiter_fill_value(myiter, sparkey_hash_getreader(myreader), wanted_valuelen, valuebuf, &actual_valuelen);
 * }
 * \endcode
 * Note that this API allows you to do a random seek and then iterate through the following entries. This may be
 * useful when you insert groups of entries in order and quickly want to access all of them.
 * - Close everything when you're done:
 * \code
 * sparkey_hash_close(&myreader);
 * sparkey_logiter_close(&myiter);
 * \endcode
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SPARKEY_SUCCESS = 0,
  SPARKEY_INTERNAL_ERROR = -1,

  SPARKEY_FILE_NOT_FOUND = -100,
  SPARKEY_PERMISSION_DENIED = -101,
  SPARKEY_TOO_MANY_OPEN_FILES = -102,
  SPARKEY_FILE_TOO_LARGE = -103,
  SPARKEY_FILE_ALREADY_EXISTS = -104,
  SPARKEY_FILE_BUSY = -105,
  SPARKEY_FILE_IS_DIRECTORY = -106,
  SPARKEY_FILE_SIZE_EXCEEDED = -107,
  SPARKEY_FILE_CLOSED = -108,
  SPARKEY_OUT_OF_DISK = -109,
  SPARKEY_UNEXPECTED_EOF = -110,
  SPARKEY_MMAP_FAILED = -111,

  SPARKEY_WRONG_LOG_MAGIC_NUMBER = -200,
  SPARKEY_WRONG_LOG_MAJOR_VERSION = -201,
  SPARKEY_UNSUPPORTED_LOG_MINOR_VERSION = -202,
  SPARKEY_LOG_TOO_SMALL = -203,
  SPARKEY_LOG_CLOSED = -204,
  SPARKEY_LOG_ITERATOR_INACTIVE = -205,
  SPARKEY_LOG_ITERATOR_MISMATCH = -206,
  SPARKEY_LOG_ITERATOR_CLOSED = -207,
  SPARKEY_LOG_HEADER_CORRUPT = -208,
  SPARKEY_INVALID_COMPRESSION_BLOCK_SIZE = -209,
  SPARKEY_INVALID_COMPRESSION_TYPE = -210,

  SPARKEY_WRONG_HASH_MAGIC_NUMBER = -300,
  SPARKEY_WRONG_HASH_MAJOR_VERSION = -301,
  SPARKEY_UNSUPPORTED_HASH_MINOR_VERSION = -302,
  SPARKEY_HASH_TOO_SMALL = -303,
  SPARKEY_HASH_CLOSED = -304,
  SPARKEY_FILE_IDENTIFIER_MISMATCH = -305,
  SPARKEY_HASH_HEADER_CORRUPT = -306,
  SPARKEY_HASH_SIZE_INVALID = -307,

} sparkey_returncode;

/**
 * Get a human readable string from a return code.
 * @param code a return code
 * @returns a string representing the return code.
 */
const char * sparkey_errstring(sparkey_returncode code);

/* logwriter */

/**
 * A structure holding all the data necessary to add entries to a log file.
 */
struct sparkey_logwriter;
typedef struct sparkey_logwriter sparkey_logwriter;

typedef enum {
  SPARKEY_COMPRESSION_NONE,
  SPARKEY_COMPRESSION_SNAPPY
} sparkey_compression_type;

typedef enum {
  SPARKEY_ENTRY_PUT,
  SPARKEY_ENTRY_DELETE
} sparkey_entry_type;

typedef enum {
  SPARKEY_ITER_NEW,
  SPARKEY_ITER_ACTIVE,
  SPARKEY_ITER_CLOSED,
  SPARKEY_ITER_INVALID
} sparkey_iter_state;

struct sparkey_logreader;
typedef struct sparkey_logreader sparkey_logreader;

struct sparkey_logiter;
typedef struct sparkey_logiter sparkey_logiter;

struct sparkey_hashreader;
typedef struct sparkey_hashreader sparkey_hashreader;


/**
 * Creates a new Sparkey log file, possibly overwriting an already existing.
 * @param log a double reference to a sparkey_logwriter structure that gets allocated and initialized by this call.
 * @param filename the file to create.
 * @param compression_type NONE or SNAPPY, specifies if block compression should be used or not.
 * @param compression_block_size is only relevant if compression type is not NONE.
 * It represents the maximum number of bytes of an uncompressed block.
 * @return SPARKEY_SUCCESS if all goes well.
 */
sparkey_returncode sparkey_logwriter_create(sparkey_logwriter **log, const char *filename, sparkey_compression_type compression_type, int compression_block_size);

/**
 * Append to an existing Sparkey log file.
 * @param log a double reference to a sparkey_logwriter structure that gets allocated and initialized by this call.
 * @param filename the file to open for appending.
 * It represents the maximum number of bytes of an uncompressed block.
 * @return SPARKEY_SUCCESS if all goes well.
 */
sparkey_returncode sparkey_logwriter_append(sparkey_logwriter **log, const char *filename);

/**
 * Append a key/value pair to the log file
 * @param log a reference to an open log writer.
 * @param keylen the number of bytes of the key data block
 * @param key a pointer to a block of continuous data where the key can be found.
 * Does not need to be NUL-terminated.
 * @param valuelen the number of bytes of the value data block
 * @param value a pointer to a block of continuous data where the value can be found.
 * Does not need to be NUL-terminated.
 * @return SPARKEY_SUCCESS if all goes well.
 */
sparkey_returncode sparkey_logwriter_put(sparkey_logwriter *log, uint64_t keylen, const uint8_t *key, uint64_t valuelen, const uint8_t *value);

/**
 * Append a delete operation for a key to the log file
 * @param log a reference to an open log writer.
 * @param keylen the number of bytes of the key data block
 * @param key a pointer to a block of continuous data where the key can be found.
 * Does not need to be NUL-terminated.
 * @return SPARKEY_SUCCESS if all goes well.
 */
sparkey_returncode sparkey_logwriter_delete(sparkey_logwriter *log, uint64_t keylen, const uint8_t *key);

/**
 * Flush any open compression block to file buffer.
 * Flush any open file buffer to disk.
 * Rewrite the header on disk.
 * This enables readers to read from the log.
 * @param log a reference to an open log writer.
 * @return SPARKEY_SUCCESS if all goes well.
 */
sparkey_returncode sparkey_logwriter_flush(sparkey_logwriter *log);

/**
 * Flushes the log, then closes the file and marks the log as closed.
 * The log will be closed after this, the sparkey_logwriter struct
 * referenced will be freed and *log will be set to NULL.
 * @param log a double reference to an open log writer.
 * @return SPARKEY_SUCCESS if all goes well.
 */
sparkey_returncode sparkey_logwriter_close(sparkey_logwriter **log);

/* logreader */

/**
 * Opens a log file for reading. The logreader is threadsafe, except during opening or closing.
 * @param log a double reference to a logreader.
 * @param filename a filename of a file containing a sparkey log.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a return code indicating the error.
 */
sparkey_returncode sparkey_logreader_open(sparkey_logreader **log, const char *filename);

/**
 * Closes a logreader.
 * It's allowed to close a logreader while there are open logiterators.
 * Further operations on such logiterators will fail.
 * This is a failsafe operation.
 * @param log a double reference to a logreader
 *            This will be set to NULL after close.
 */
void sparkey_logreader_close(sparkey_logreader **log);

/**
 * Get the size of the largest key in the log.
 * @param log a reference to a logreader.
 * @returns
 */
uint64_t sparkey_logreader_maxkeylen(sparkey_logreader *log);

/**
 * Get the size of the largest value in the log.
 * @param log a reference to a logreader.
 * @returns
 */
uint64_t sparkey_logreader_maxvaluelen(sparkey_logreader *log);

/**
 * Get the blocksize for a reader
 * @param log a reference to a logreader.
 * @returns the blocksize
 */
int sparkey_logreader_get_compression_blocksize(sparkey_logreader *log);

/**
 * Get the compression type for a reader
 * @param log a reference to a logreader.
 * @returns the compression type
 */
sparkey_compression_type sparkey_logreader_get_compression_type(sparkey_logreader *log);

/**
 * Initializes a logiter and associates it with a logreader.
 * The logreader must be open. The logiter is not threadsafe.
 * @param iter a double reference to an uninitialized logiter. Will be set on success.
 * @param log an open logreader
 * @returns SPARKEY_SUCCESS or all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_logiter_create(sparkey_logiter **iter, sparkey_logreader *log);

/**
 * Closes a log iterator.
 * This is a failsafe operation.
 * @param iter a double reference to a log iterator.
 *             This will be set to NULL after close.
 */
void sparkey_logiter_close(sparkey_logiter **iter);

/**
 * Skips to a specific block in the logfile.
 * The position must be a valid block start, but that will not be verified by the function.
 * If an illegal position is used, all other operations on this logiterator are undefined,
 * and may even segfault.
 * @param iter an open log iterator.
 * @param log an open logreader associated with iter.
 * @param position an offset into the logfile where a block begins.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_logiter_seek(sparkey_logiter *iter, sparkey_logreader *log, uint64_t position);

/**
 * Skip a number of entries.
 * This is equivalent to calling sparkey_logiter_next count number of times.
 * @param iter an open logiter
 * @param log an open logreader associated with iter.
 * @param count the number of entries to skip.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_logiter_skip(sparkey_logiter *iter, sparkey_logreader *log, int count);

/**
 * Prepares the logiter to start reading from the next entry.
 * iter->state will be SPARKEY_ITER_CLOSED if the last entry has been passed.
 * iter->state will be SPARKEY_ITER_INVALID if anything goes wrong.
 * iter->state will be SPARKEY_ITER_ACTIVE if it successfully reached the next entry.
 * @param iter an open logiter
 * @param log an open logreader associated with iter.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_logiter_next(sparkey_logiter *iter, sparkey_logreader *log);

/**
 * Resets the iterator to the start of the current entry. This is only valid if
 * iter->state is SPARKEY_ITER_ACTIVE.
 * @param iter an open logiter
 * @param log an open logreader associated with iter.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_logiter_reset(sparkey_logiter *iter, sparkey_logreader *log);

/**
 * Consumes and returns part of or all of the key of the current entry.
 * Usage example:
 * uint8_t *res;
 * uint64_t len;
 * sparkey_returncode code = sparkey_logiter_keychunk(iter, log, 1 << 30, &res, &len);
 *
 * @param iter an open logiter
 * @param log an open logreader associated with iter.
 * @param maxlen a limit for how much data you want to handle.
 * @param res (output parameter) reference to a read only array of data. The array is of size res, and is not NUL-terminated.
 * You can not use this as a string, and you may not modify it. The data in the array is valid until the next operation on the
 * logiter or until the log is closed.
 * @param len (output parameter) reference to a variable holding the size of res.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_logiter_keychunk(sparkey_logiter *iter, sparkey_logreader *log, uint64_t maxlen, uint8_t ** res, uint64_t *len);

/**
 * First consumes and discards any remaining key parts.
 * Then consumes and returns part of or all of the value of the current entry.
 * Usage example:
 * uint8_t *res;
 * uint64_t len;
 * sparkey_returncode code = sparkey_logiter_valuechunk(iter, log, 1 << 30, &res, &len);
 *
 * @param iter an open logiter
 * @param log an open logreader associated with iter.
 * @param maxlen a limit for how much data you want to handle.
 * @param res (output parameter) reference to a read only array of data. The array is of size res, and is not NUL-terminated.
 * You can not use this as a string, and you may not modify it. The data in the array is valid until the next operation on the
 * logiter or until the log is closed.
 * @param len (output parameter) reference to a variable holding the size of res.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_logiter_valuechunk(sparkey_logiter *iter, sparkey_logreader *log, uint64_t maxlen, uint8_t ** res, uint64_t *len);

/**
 * Convenience function around sparkey_logiter_keychunk.
 * Takes a user allocated buffer and fills it as much as possible by consuming parts of the key of the current entry.
 * No NUL will be appended after the data, so you may not use it as a string unless you add the NUL manually.
 * Usage example:
 * uint8_t *buf = malloc(iter->keylen);
 * uint64_t len;
 * sparkey_returncode code = sparkey_logiter_fill_key(iter, log, iter->keylen, buf, &len);
 *
 * @param iter an open logiter
 * @param log an open logreader associated with iter.
 * @param maxlen a limit for how much data you want to handle.
 * @param buf a writable array of data. The array must at least be of size maxlen.
 * @param len (output parameter) reference to a variable holding the amount of data written to buf.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_logiter_fill_key(sparkey_logiter *iter, sparkey_logreader *log, uint64_t maxlen, uint8_t *buf, uint64_t *len);

/**
 * Convenience function around sparkey_logiter_valuechunk.
 * Takes a user allocated buffer and fills it as much as possible by consuming parts of the key of the current entry.
 * No NUL will be appended after the data, so you may not use it as a string unless you add the NUL manually.
 * Usage example:
 * uint8_t *buf = malloc(iter->valuelen);
 * uint64_t len;
 * sparkey_returncode code = sparkey_logiter_fill_value(iter, log, iter->valuelen, buf, &len);
 *
 * @param iter an open logiter
 * @param log an open logreader associated with iter.
 * @param maxlen a limit for how much data you want to handle.
 * @param buf a writable array of data. The array must at least be of size maxlen.
 * @param len (output parameter) reference to a variable holding the amount of data written to buf.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_logiter_fill_value(sparkey_logiter *iter, sparkey_logreader *log, uint64_t maxlen, uint8_t *buf, uint64_t *len);

/**
 * Compares the keys of two iterators pointing to the same log.
 * It assumes that the iterators are both clean, i.e. nothing has been consumed from the current entry.
 *
 * @param iter1 an open logiter
 * @param iter2 an open logiter
 * @param log an open logreader associated with iter1 and iter2.
 * @param res (output parameter) reference to a variable holding the result of the comparison.
 * It will be zero if the keys are equal, negative if key1 is smaller than key2 and positive if key1 is larger than key2.
 * The behaviour is thus Like memcmp.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_logiter_keycmp(sparkey_logiter *iter1, sparkey_logiter *iter2, sparkey_logreader *log, int *res);

/**
 * Get the state for an iterator.
 * @returns iter->state
 */
sparkey_iter_state sparkey_logiter_state(sparkey_logiter *iter);

/**
 * Get the type of an iterator.
 * @returns iter->type
 */
sparkey_entry_type sparkey_logiter_type(sparkey_logiter *iter);

/**
 * Get the keylen of an iterator.
 * @returns iter->keylen
 */
uint64_t sparkey_logiter_keylen(sparkey_logiter *iter);

/**
 * Get the valuelen of an iterator.
 * @returns iter->valuelen
 */
uint64_t sparkey_logiter_valuelen(sparkey_logiter *iter);

/* hashwriter */

/**
 * Creates a hash table for a specific log file.
 * It's safe and efficient to run this multiple times.
 * If the hash file already exists, it will be used to speed up the creation of the new file
 * by reusing the existing entries, and only update the new hash table based on
 * the entries in the log that are new since the last hash was built.
 * Note that the hash file is never overwritten, instead the old file is unlinked from
 * the filesystem and the new one is created. Thus, it's safe to rewrite the hash table while
 * other processes are reading from it.
 * @param hash_filename the file to create and put the sparkey hash table in.
 * @param log_filename a file that must exist and be a sparkey log file.
 * @param hash_size size of the hashes for keys.
          Valid values are 4 (32 bit murmurhash3_x86_32) and
          8 (lower 64-bit part of murmurhash3_x64_128).
          A value of zero will make it autoselect hash size, depending on number of entries.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_hash_write(const char *hash_filename, const char *log_filename, int hash_size);

/* hashreader */
/**
 * Opens a hash file and a log file for reading. The the hashreader is threadsafe, except during opening or closing.
 * @param reader a double reference to an uninitialized hashreader. Will be set on success.
 * @param hash_filename a filename of a file containing a sparkey hash table.
 * @param log_filename a filename of a file containing a sparkey log.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a return code indicating the error.
 */
sparkey_returncode sparkey_hash_open(sparkey_hashreader **reader, const char *hash_filename, const char *log_filename);

/**
 * Gets the logreader that is referenced by the hashreader
 * @param reader an open reader.
 * @returns the associated logreader
 */
sparkey_logreader * sparkey_hash_getreader(sparkey_hashreader *reader);

/**
 * Closes a hashreader.
 * It's allowed to close a hashreader while there are open logiterators associated with it.
 * Further operations on such logiterators will fail.
 * This is a failsafe operation.
 * @param reader a double reference to a hashreader
 */
void sparkey_hash_close(sparkey_hashreader **reader);

/**
 * Performs a hash table lookup of a key. If the key is found,
 * the iterator will have state SPARKEY_ITER_ACTIVE and the key chunk will be consumed.
 * Otherwise, the iterator will have state SPARKEY_ITER_INVALID.
 * @param reader an open reader.
 * @param key a buffer containing the key. It does not have be NUL terminated.
 * @param keylen the length of the key.
 * @param iter an iterator associated with the reader. Will be mutated.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a return code indicating the error.
 */
sparkey_returncode sparkey_hash_get(sparkey_hashreader *reader, const uint8_t *key, uint64_t keylen, sparkey_logiter *iter);

/**
 * Works the same as sparkey_logiter_next, except it skips entries that are not of type SPARKEY_ENTRY_PUT
 * and entries that have been overwritten or deleted. Thus it only stops at live entries.
 * iter->state will be SPARKEY_ITER_CLOSED if the last entry has been passed.
 * iter->state will be SPARKEY_ITER_INVALID if anything goes wrong.
 * iter->state will be SPARKEY_ITER_ACTIVE if it successfully reached the next entry.
 * @see sparkey_logiter_next
 * @param iter an open logiter
 * @param reader an open reader associated with iter.
 * @returns SPARKEY_SUCCESS if all goes well. Otherwise a returncode indicating the error.
 */
sparkey_returncode sparkey_logiter_hashnext(sparkey_logiter *iter, sparkey_hashreader *reader);

uint64_t sparkey_hash_numentries(sparkey_hashreader *reader);

uint64_t sparkey_hash_numcollisions(sparkey_hashreader *reader);

/* util */

/**
 * Allocates and creates a string denoting a log file from an index file.
 * This is simply a string replacement of .spi$ to .spl$
 * @param index_filename the filename representing the index file
 * @returns NULL if the index_filename does not end with ".spi"
 */
char * sparkey_create_log_filename(const char *index_filename);

/**
 * Allocates and creates a string denoting an index file from a log file.
 * This is simply a string replacement of .spl$ to .spi$
 * @param log_filename the filename representing the log file
 * @returns NULL if the log_filename does not end with ".spl"
 */
char * sparkey_create_index_filename(const char *log_filename);

#ifdef __cplusplus
}
#endif

#endif

