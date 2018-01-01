#include "acronyms.h"
#include "address_parser.h"
#include "dedupe.h"
#include "expand.h"
#include "float_utils.h"
#include "jaccard.h"
#include "place.h"
#include "scanner.h"
#include "soft_tfidf.h"
#include "token_types.h"

bool expansions_intersect(cstring_array *expansions1, cstring_array *expansions2) {
    size_t n1 = cstring_array_num_strings(expansions1);
    size_t n2 = cstring_array_num_strings(expansions2);

    bool intersect = false;

    for (size_t i = 0; i < n1; i++) {
        char *e1 = cstring_array_get_string(expansions1, i);
        for (size_t j = 0; j < n2; j++) {
            char *e2 = cstring_array_get_string(expansions2, j);
            if (string_equals(e1, e2)) {
                intersect = true;
                break;
            }
        }
        if (intersect) break;
    }
    return intersect;
}


bool address_component_equals_root_option(char *s1, char *s2, libpostal_normalize_options_t options, bool root) {
    size_t n1, n2;
    cstring_array *expansions1 = NULL;
    cstring_array *expansions2 = NULL;
    if (!root) {
        expansions1 = expand_address(s1, options, &n1);
    } else {
        expansions1 = expand_address_root(s1, options, &n1);
    }

    if (expansions1 == NULL) return false;

    if (!root) {
        expansions2 = expand_address(s2, options, &n2);
    } else {
        expansions2 = expand_address_root(s2, options, &n2);
    }

    if (expansions2 == NULL) {
        cstring_array_destroy(expansions1);
        return false;
    }

    bool intersect = expansions_intersect(expansions1, expansions2);

    cstring_array_destroy(expansions1);
    cstring_array_destroy(expansions2);

    return intersect;
}

static inline bool address_component_equals(char *s1, char *s2, libpostal_normalize_options_t options) {
    return address_component_equals_root_option(s1, s2, options, false);
}

static inline bool address_component_equals_root(char *s1, char *s2, libpostal_normalize_options_t options) {
    return address_component_equals_root_option(s1, s2, options, true);
}


static inline bool address_component_equals_root_fallback(char *s1, char *s2, libpostal_normalize_options_t options) {
    return address_component_equals_root(s1, s2, options) || address_component_equals(s1, s2, options);
}

libpostal_duplicate_status_t is_duplicate(char *value1, char *value2, libpostal_normalize_options_t normalize_options, libpostal_duplicate_options_t options, bool root_comparison_first, libpostal_duplicate_status_t root_comparison_status) {
    if (value1 == NULL || value2 == NULL) {
        return LIBPOSTAL_NULL_DUPLICATE_STATUS;
    }

    normalize_options.num_languages = options.num_languages;
    normalize_options.languages = options.languages;

    if (root_comparison_first) {
        if (address_component_equals_root(value1, value2, normalize_options)) {
            return root_comparison_status;
        } else if (address_component_equals(value1, value2, normalize_options)) {
            return LIBPOSTAL_EXACT_DUPLICATE;
        }
    } else {
        if (address_component_equals(value1, value2, normalize_options)) {
            return LIBPOSTAL_EXACT_DUPLICATE;
        } else if (address_component_equals_root(value1, value2, normalize_options)) {
            return root_comparison_status;
        }
    }
    return LIBPOSTAL_NON_DUPLICATE;
}

libpostal_duplicate_status_t is_name_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options) {
    libpostal_normalize_options_t normalize_options = libpostal_get_default_options();
    normalize_options.address_components = LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_ANY;
    bool root_comparison_first = false;
    libpostal_duplicate_status_t root_comparison_status = LIBPOSTAL_POSSIBLE_DUPLICATE_NEEDS_REVIEW;
    return is_duplicate(value1, value2, normalize_options, options, root_comparison_first, root_comparison_status);
}

libpostal_duplicate_status_t is_street_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options) {
    libpostal_normalize_options_t normalize_options = libpostal_get_default_options();
    normalize_options.address_components = LIBPOSTAL_ADDRESS_STREET | LIBPOSTAL_ADDRESS_ANY;
    bool root_comparison_first = false;
    libpostal_duplicate_status_t root_comparison_status = LIBPOSTAL_POSSIBLE_DUPLICATE_NEEDS_REVIEW;
    return is_duplicate(value1, value2, normalize_options, options, root_comparison_first, root_comparison_status);
}

libpostal_duplicate_status_t is_house_number_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options) {
    libpostal_normalize_options_t normalize_options = libpostal_get_default_options();
    normalize_options.address_components = LIBPOSTAL_ADDRESS_HOUSE_NUMBER | LIBPOSTAL_ADDRESS_ANY;
    bool root_comparison_first = true;
    libpostal_duplicate_status_t root_comparison_status = LIBPOSTAL_EXACT_DUPLICATE;
    return is_duplicate(value1, value2, normalize_options, options, root_comparison_first, root_comparison_status);
}

libpostal_duplicate_status_t is_unit_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options) {
    libpostal_normalize_options_t normalize_options = libpostal_get_default_options();
    normalize_options.address_components = LIBPOSTAL_ADDRESS_UNIT | LIBPOSTAL_ADDRESS_ANY;
    bool root_comparison_first = true;
    libpostal_duplicate_status_t root_comparison_status = LIBPOSTAL_EXACT_DUPLICATE;
    return is_duplicate(value1, value2, normalize_options, options, root_comparison_first, root_comparison_status);
}

libpostal_duplicate_status_t is_floor_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options) {
    libpostal_normalize_options_t normalize_options = libpostal_get_default_options();
    normalize_options.address_components = LIBPOSTAL_ADDRESS_LEVEL | LIBPOSTAL_ADDRESS_ANY;
    bool root_comparison_first = true;
    libpostal_duplicate_status_t root_comparison_status = LIBPOSTAL_EXACT_DUPLICATE;
    return is_duplicate(value1, value2, normalize_options, options, root_comparison_first, root_comparison_status);
}

libpostal_duplicate_status_t is_po_box_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options) {
    libpostal_normalize_options_t normalize_options = libpostal_get_default_options();
    normalize_options.address_components = LIBPOSTAL_ADDRESS_PO_BOX | LIBPOSTAL_ADDRESS_ANY;
    bool root_comparison_first = true;
    libpostal_duplicate_status_t root_comparison_status = LIBPOSTAL_EXACT_DUPLICATE;
    return is_duplicate(value1, value2, normalize_options, options, root_comparison_first, root_comparison_status);
}

libpostal_duplicate_status_t is_postal_code_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options) {
    libpostal_normalize_options_t normalize_options = libpostal_get_default_options();
    normalize_options.address_components = LIBPOSTAL_ADDRESS_POSTAL_CODE | LIBPOSTAL_ADDRESS_ANY;
    bool root_comparison_first = true;
    libpostal_duplicate_status_t root_comparison_status = LIBPOSTAL_EXACT_DUPLICATE;
    return is_duplicate(value1, value2, normalize_options, options, root_comparison_first, root_comparison_status);
}

libpostal_duplicate_status_t is_toponym_duplicate(size_t num_components1, char **labels1, char **values1, size_t num_components2, char **labels2, char **values2, libpostal_duplicate_options_t options) {
    libpostal_normalize_options_t normalize_options = libpostal_get_default_options();
    normalize_options.address_components = LIBPOSTAL_ADDRESS_TOPONYM | LIBPOSTAL_ADDRESS_ANY;
    normalize_options.num_languages = options.num_languages;
    normalize_options.languages = options.languages;

    place_t *place1 = place_from_components(num_components1, labels1, values1);
    place_t *place2 = place_from_components(num_components2, labels2, values2);

    bool city_match = false;
    libpostal_duplicate_status_t dupe_status = LIBPOSTAL_NON_DUPLICATE;

    if (place1->city != NULL && place2->city != NULL) {
        city_match = address_component_equals(place1->city, place2->city, normalize_options);
        if (city_match) {
            dupe_status = LIBPOSTAL_EXACT_DUPLICATE;
        }
    }

    if (!city_match && place1->city == NULL && place1->city_district != NULL && place2->city != NULL) {
        city_match = address_component_equals(place1->city_district, place2->city, normalize_options);        
        if (city_match) {
            dupe_status = LIBPOSTAL_LIKELY_DUPLICATE;
        }
    }

    if (!city_match && place1->city == NULL && place1->suburb != NULL && place2->city != NULL) {
        city_match = address_component_equals(place1->suburb, place2->city, normalize_options);        
        if (city_match) {
            dupe_status = LIBPOSTAL_POSSIBLE_DUPLICATE_NEEDS_REVIEW;
        }
    }

    if (!city_match && place2->city == NULL && place2->city_district != NULL && place1->city != NULL) {
        city_match = address_component_equals(place1->city, place2->city_district, normalize_options);        
        if (city_match) {
            dupe_status = LIBPOSTAL_LIKELY_DUPLICATE;
        }
    }

    if (!city_match && place2->city == NULL && place2->suburb != NULL && place1->city != NULL) {
        city_match = address_component_equals(place1->suburb, place2->suburb, normalize_options);        
        if (city_match) {
            dupe_status = LIBPOSTAL_POSSIBLE_DUPLICATE_NEEDS_REVIEW;
        }
    }

    if (!city_match) {
        goto exit_destroy_places;
    }

    if (city_match && place1->state_district != NULL && place2->state_district != NULL && !address_component_equals_root(place1->state_district, place2->state_district, normalize_options)) {
        dupe_status = LIBPOSTAL_NON_DUPLICATE;
        goto exit_destroy_places;
    }

    if (city_match && place1->state != NULL && place2->state != NULL && !address_component_equals(place1->state, place2->state, normalize_options)) {
        dupe_status = LIBPOSTAL_NON_DUPLICATE;
        goto exit_destroy_places;
    }

    if (city_match && place1->country != NULL && place2->country != NULL && !address_component_equals(place1->country, place2->country, normalize_options)) {
        dupe_status = LIBPOSTAL_NON_DUPLICATE;
        goto exit_destroy_places;
    }

exit_destroy_places:
    place_destroy(place1);
    place_destroy(place2);
    return dupe_status;

}

char *joined_string_and_tokens_from_strings(char **strings, size_t num_strings, token_array *tokens) {
    if (tokens == NULL || strings == NULL || num_strings == 0) return NULL;
    token_array_clear(tokens);

    size_t full_len = 0;
    for (size_t i = 0; i < num_strings; i++) {
        full_len += strlen(strings[i]);
        if (i < num_strings - 1) full_len++;
    }

    char_array *a = char_array_new_size(full_len);
    for (size_t i = 0; i < num_strings; i++) {
        char *str = strings[i];
        size_t len = strlen(str);
        size_t offset = a->n;
        char_array_append(a, str);

        scanner_t scanner = scanner_from_string(str, len);
        uint16_t token_type = scan_token(&scanner);

        token_t token = (token_t){offset, len, token_type};
        token_array_push(tokens, token);
        if (i < num_strings - 1 && !is_ideographic(token.type)) {
            char_array_append(a, " ");
        }
    }

    char_array_terminate(a);
    return char_array_to_string(a);
}

bool have_ideographic_word_tokens(token_array *token_array) {
    if (token_array == NULL) return false;

    size_t n = token_array->n;
    token_t *tokens = token_array->a;
    for (size_t i = 0; i < n; i++) {
        token_t token = tokens[i];
        if (is_ideographic(token.type) && is_word_token(token.type)) {
            return true;   
        }
    }
    return false;
}

libpostal_fuzzy_duplicate_status_t is_fuzzy_duplicate(size_t num_tokens1, char **tokens1, double *token_scores1, size_t num_tokens2, char **tokens2, double *token_scores2, libpostal_fuzzy_duplicate_options_t options, libpostal_normalize_options_t normalize_options, soft_tfidf_options_t soft_tfidf_options, bool do_acronyms) {
    normalize_options.num_languages = options.num_languages;
    normalize_options.languages = options.languages;

    normalize_options.address_components |= LIBPOSTAL_ADDRESS_ANY;

    double max_sim = 0.0;

    // Default is non-duplicate;
    libpostal_duplicate_status_t dupe_status = LIBPOSTAL_NON_DUPLICATE;

    token_array *token_array1 = token_array_new_size(num_tokens1);
    char *joined1 = joined_string_and_tokens_from_strings(tokens1, num_tokens1, token_array1);

    token_array *token_array2 = token_array_new_size(num_tokens2);
    char *joined2 = joined_string_and_tokens_from_strings(tokens2, num_tokens2, token_array2);

    size_t num_languages = options.num_languages;
    char **languages = options.languages;

    phrase_array *acronym_alignments = NULL;
    
    phrase_array *phrases1 = NULL;
    phrase_array *phrases2 = NULL;

    bool is_ideographic = have_ideographic_word_tokens(token_array1) && have_ideographic_word_tokens(token_array2);

    if (!is_ideographic) {
        if (do_acronyms) {
            acronym_alignments = acronym_token_alignments(joined1, token_array1, joined2, token_array2, num_languages, languages);
        }

        if (num_languages > 0) {
            phrases1 = phrase_array_new();
            phrases2 = phrase_array_new();

            for (size_t i = 0; i < num_languages; i++) {
                char *lang = languages[i];
                phrase_array_clear(phrases1);
                phrase_array_clear(phrases2);

                search_address_dictionaries_tokens_with_phrases(joined1, token_array1, lang, &phrases1);
                search_address_dictionaries_tokens_with_phrases(joined2, token_array2, lang, &phrases2);
            
                double sim = soft_tfidf_similarity_with_phrases_and_acronyms(num_tokens1, tokens1, token_scores1, phrases1, num_tokens2, tokens2, token_scores2, phrases2, acronym_alignments, soft_tfidf_options);
                if (sim > max_sim) {
                    max_sim = sim;
                }
            }
        } else if (do_acronyms) {
            max_sim = soft_tfidf_similarity_with_phrases_and_acronyms(num_tokens1, tokens1, token_scores1, phrases1, num_tokens2, tokens2, token_scores2, phrases2, acronym_alignments, soft_tfidf_options);
        } else {
            max_sim = soft_tfidf_similarity(num_tokens1, tokens1, token_scores1, num_tokens2, tokens2, token_scores2, soft_tfidf_options);
        }
    } else {
        max_sim = jaccard_similarity_string_arrays(num_tokens1, tokens1, num_tokens2, tokens2);
        if (string_equals(joined1, joined2)) {
            dupe_status = LIBPOSTAL_EXACT_DUPLICATE;
        } else if (address_component_equals_root(joined1, joined2, normalize_options)) {
            dupe_status = LIBPOSTAL_LIKELY_DUPLICATE;
        }
    }

    if (dupe_status == LIBPOSTAL_NON_DUPLICATE) {
        if (max_sim > options.likely_dupe_threshold || double_equals(max_sim, options.likely_dupe_threshold)) {
            dupe_status = LIBPOSTAL_LIKELY_DUPLICATE;
        } else if (max_sim > options.needs_review_threshold || double_equals(max_sim, options.needs_review_threshold)) {
            dupe_status = LIBPOSTAL_POSSIBLE_DUPLICATE_NEEDS_REVIEW;
        }
    }

    if (phrases1 != NULL) {
        phrase_array_destroy(phrases1);
    }

    if (phrases2 != NULL) {
        phrase_array_destroy(phrases2);
    }

    if (acronym_alignments != NULL) {
        phrase_array_destroy(acronym_alignments);
    }

    if (token_array1 != NULL) {
        token_array_destroy(token_array1);
    }

    if (joined1 != NULL) {
        free(joined1);
    }

    if (token_array2 != NULL) {
        token_array_destroy(token_array2);
    }

    if (joined2 != NULL) {
        free(joined2);
    }

    return (libpostal_fuzzy_duplicate_status_t){dupe_status, max_sim};
}

inline libpostal_fuzzy_duplicate_status_t is_name_duplicate_fuzzy(size_t num_tokens1, char **tokens1, double *token_scores1, size_t num_tokens2, char **tokens2, double *token_scores2, libpostal_fuzzy_duplicate_options_t options) {
    libpostal_normalize_options_t normalize_options = libpostal_get_default_options();
    normalize_options.address_components = LIBPOSTAL_ADDRESS_NAME;

    bool do_acronyms = true;

    soft_tfidf_options_t soft_tfidf_options = soft_tfidf_default_options();

    return is_fuzzy_duplicate(num_tokens1, tokens1, token_scores1, num_tokens2, tokens2, token_scores2, options, normalize_options, soft_tfidf_options, do_acronyms);
}


inline libpostal_fuzzy_duplicate_status_t is_street_duplicate_fuzzy(size_t num_tokens1, char **tokens1, double *token_scores1, size_t num_tokens2, char **tokens2, double *token_scores2, libpostal_fuzzy_duplicate_options_t options) {
    libpostal_normalize_options_t normalize_options = libpostal_get_default_options();
    normalize_options.address_components = LIBPOSTAL_ADDRESS_STREET;

    // General purpose acronyms didn't make as much sense in the street name context
    // things like County Road = CR should be handled by the address dictionaries
    bool do_acronyms = false;

    soft_tfidf_options_t soft_tfidf_options = soft_tfidf_default_options();

    return is_fuzzy_duplicate(num_tokens1, tokens1, token_scores1, num_tokens2, tokens2, token_scores2, options, normalize_options, soft_tfidf_options, do_acronyms);
}

