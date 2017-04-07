#include "ngrams.h"
#include "utf8proc/utf8proc.h"

bool add_ngrams(cstring_array *grams, size_t n, char *str, size_t len, bool prefix, bool suffix) {
    if (n == 0) return false;
    
    size_t lengths[n];
    size_t num_chars = 0;

    uint8_t *ptr = (uint8_t *)str;

    int32_t ch;

    size_t idx = 0;

    size_t gram_len = 0;
    size_t gram_offset = 0;
    size_t consumed = 0;

    size_t num_grams = 0;

    bool beginning = true;

    while (idx < len) {
        ssize_t char_len = utf8proc_iterate(ptr, len, &ch);
        if (char_len <= 0 || ch == 0) break;

        // Not at min characters yet
        if (num_chars < n) {
            lengths[num_chars] = (size_t)char_len;
            num_chars++;
            gram_len += char_len;
        }

        // We have a full gram of size n
        if (num_chars == n && (num_grams > 0 || idx + char_len < len)) {
            uint32_t token_index = cstring_array_start_token(grams);

            if (beginning) {
                beginning = false;
            } else {
                if (prefix) {
                    cstring_array_append_string(grams, "_");
                }
                gram_len -= lengths[0];
                gram_offset += lengths[0];
                gram_len += char_len;

                for (size_t i = 1; i < n; i++) {
                    lengths[i - 1] = lengths[i];
                }
                lengths[n - 1] = (size_t)char_len;
            }

            cstring_array_append_string_len(grams, str + gram_offset, gram_len);

            if (idx + char_len < len && suffix) {
                cstring_array_append_string(grams, "_");
            }

            cstring_array_terminate(grams);
            num_grams++;
        }

        idx += char_len;
        ptr += char_len;
        consumed += char_len;
    }

    return num_grams > 0;
}
