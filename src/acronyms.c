#include "acronyms.h"

phrase_array *acronym_token_alignments(const char *s1, token_array *tokens1, const char *s2, token_array *tokens2, size_t num_languages, char **languages) {
    if (s1 == NULL || tokens1 == NULL || s2 == NULL || tokens2 == NULL) {
        return NULL;
    }

    size_t len1 = tokens1->n;
    size_t len2 = tokens2->n;
    if (len1 == 0 || len2 == 0 || len1 == len2) return NULL;

    if (len1 > len2) {
        const char *tmp_s = s1;
        s1 = s2;
        s2 = tmp_s;

        token_array *tmp_t = tokens1;
        tokens1 = tokens2;
        tokens2 = tmp_t;

        size_t tmp_l = len1;
        len1 = len2;
        len2 = tmp_l;
    }

    phrase_array *alignments = NULL;

    token_t *t1 = tokens1->a;
    token_t *t2 = tokens2->a;

    uint32_array *stopwords_array = uint32_array_new_zeros(len2);

    uint32_t *stopwords = stopwords_array->a;

    for (size_t l = 0; l < num_languages; l++) {
        char *lang = languages[l];
        phrase_array *lang_phrases = search_address_dictionaries_tokens((char *)s2, tokens2, lang);

        if (lang_phrases != NULL) {
            size_t num_lang_phrases = lang_phrases->n;
            for (size_t p = 0; p < num_lang_phrases; p++) {
                phrase_t phrase = lang_phrases->a[p];

                if (address_phrase_in_dictionary(phrase, DICTIONARY_STOPWORD)) {
                    for (size_t stop_idx = phrase.start; stop_idx < phrase.start + phrase.len; stop_idx++) {
                        stopwords[stop_idx] = 1;
                    }
                }
            }
            phrase_array_destroy(lang_phrases);
        }
    }

    ssize_t acronym_start = -1;
    ssize_t acronym_token_pos = -1;

    uint8_t *ptr1 = (uint8_t *)s1;
    uint8_t *ptr2 = (uint8_t *)s2;

    int32_t c1, c2;
    ssize_t c1_len;
    ssize_t c2_len;

    size_t t2_consumed = 0;

    for (size_t i = 0; i < len1; i++) {
        token_t ti = t1[i];

        c1_len = utf8proc_iterate(ptr1 + ti.offset, ti.len, &c1);
        if (c1_len <= 0 || c1 == 0) {
            break;
        }

        // Make sure it's a non-ideographic word. Single letter abbreviations will be captured by other methods
        if (!is_word_token(ti.type) || is_ideographic(ti.type) || ti.len == c1_len) {
            acronym_token_pos = -1;
            continue;
        }

        size_t ti_pos = 0;

        for (size_t j = t2_consumed; j < len2; j++) {
            token_t tj = t2[j];
            c2_len = utf8proc_iterate(ptr2 + tj.offset, tj.len, &c2);
            if (c2_len <= 0) {
                break;
            }

            if (utf8proc_tolower(c1) == utf8proc_tolower(c2)) {
                ti_pos += c1_len;
                if (acronym_start < 0) {
                    acronym_start = j;
                    acronym_token_pos = 0;
                }
                acronym_token_pos++;
                c1_len = utf8proc_iterate(ptr1 + ti.offset + ti_pos, ti.len, &c1);
            } else if (stopwords[j] && acronym_token_pos > 0) {
                continue;
            } else if (is_punctuation(tj.type) && acronym_token_pos > 0) {
                continue;
            } else if (ti_pos < ti.len) {
                acronym_token_pos = -1;
                acronym_start = -1;
                ti_pos = 0;
                continue;
            }

            if ((utf8_is_period(c1) || utf8_is_hyphen(c1)) && ti_pos < ti.len) {
                ti_pos += c1_len;
                if (ti_pos < ti.len) {
                    c1_len = utf8proc_iterate(ptr1 + ti.offset + ti_pos, ti.len, &c1);
                    if (c1_len <= 0 || c1 == 0) {
                        break;
                    }
                }
            }

            if (ti_pos == ti.len) {
                phrase_t phrase = (phrase_t){acronym_start, j - acronym_start + 1, i};
                // got alignment
                if (alignments == NULL) {
                    alignments = phrase_array_new();
                }

                phrase_array_push(alignments, phrase);
        
                ti_pos = 0;
                acronym_token_pos = -1;
                acronym_start = -1;
            }
        }

    }

    uint32_array_destroy(stopwords_array);

    return alignments;   
}


