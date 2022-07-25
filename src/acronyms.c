#include "acronyms.h"
#include "token_types.h"


bool existing_acronym_phrase_positions(uint32_array *existing_acronyms_array, const char *str, token_array *token_array, size_t num_languages, char **languages) {
    if (existing_acronyms_array == NULL || token_array == NULL) return false;
    size_t num_tokens = token_array->n;
    if (existing_acronyms_array->n != num_tokens) {
        uint32_array_resize_fixed(existing_acronyms_array, num_tokens);
    }

    uint32_array_zero(existing_acronyms_array->a, existing_acronyms_array->n);
    uint32_t *existing_acronyms = existing_acronyms_array->a;

    token_t *tokens = token_array->a;
    for (size_t i = 0; i < num_tokens; i++) {
        token_t token = tokens[i];
        if (token.type == ACRONYM) {
            existing_acronyms[i] = 1;
        }
    }

    for (size_t l = 0; l < num_languages; l++) {
        char *lang = languages[l];
        phrase_array *lang_phrases = search_address_dictionaries_tokens((char *)str, token_array, lang);

        if (lang_phrases != NULL) {
            size_t num_lang_phrases = lang_phrases->n;
            for (size_t p = 0; p < num_lang_phrases; p++) {
                phrase_t phrase = lang_phrases->a[p];

                address_expansion_value_t *value = address_dictionary_get_expansions(phrase.data);
                if (value == NULL) continue;

                address_expansion_array *expansions_array = value->expansions;
                if (expansions_array == NULL) continue;

                size_t num_expansions = expansions_array->n;
                address_expansion_t *expansions = expansions_array->a;

                for (size_t i = 0; i < num_expansions; i++) {
                    address_expansion_t expansion = expansions[i];
                    if (expansion.canonical_index != NULL_CANONICAL_INDEX) {
                        char *canonical = address_dictionary_get_canonical(expansion.canonical_index);
                        bool is_possible_acronym = string_contains(canonical, " ") || (phrase.len == 1 && address_expansion_in_dictionary(expansion, DICTIONARY_DIRECTIONAL));
                        if (is_possible_acronym) {
                            for (size_t j = phrase.start; j < phrase.start + phrase.len; j++) {
                                existing_acronyms[j] = 1;
                            }
                        }
                    }
                }

            }
            phrase_array_destroy(lang_phrases);
        }
    }

    return true;
}

bool stopword_positions(uint32_array *stopwords_array, const char *str, token_array *tokens, size_t num_languages, char **languages) {
    if (stopwords_array == NULL) return false;
    if (stopwords_array->n != tokens->n) {
        uint32_array_resize_fixed(stopwords_array, tokens->n);
    }

    uint32_array_zero(stopwords_array->a, stopwords_array->n);
    uint32_t *stopwords = stopwords_array->a;

    for (size_t l = 0; l < num_languages; l++) {
        char *lang = languages[l];
        phrase_array *lang_phrases = search_address_dictionaries_tokens((char *)str, tokens, lang);

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

    return true;
}


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

    uint32_array *stopwords_array = uint32_array_new_zeros(tokens2->n);
    if (stopwords_array == NULL) {
        return NULL;
    }

    stopword_positions(stopwords_array, s2, tokens2, num_languages, languages);

    uint32_t *stopwords = stopwords_array->a;

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
