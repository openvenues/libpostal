#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#include "double_metaphone.h"
#include "string_utils.h"
#include "utf8proc/utf8proc.h"

static bool is_vowel(char c) {
    return (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y');
}

static char get_char_at(char *str, size_t len, ssize_t idx) {
    if (idx < 0 || idx >= len) return 0;
    return str[idx];
}

static char *get_string_at(char *str, size_t len, ssize_t idx) {
    if (idx < 0 || idx >= len) return NULL;
    return str + idx;
}

static inline bool is_slavo_germanic(char *s) {
    return strstr(s, "W")
        || strstr(s, "K")
        || strstr(s, "CZ")
        || strstr(s, "WITZ");
}

static inline bool substring_equals(char *str, size_t len, ssize_t index, size_t substr_len, ...) {
    char *string_at_index = get_string_at(str, len, index);
    if (string_at_index == NULL) return false;

    va_list args;
    va_start(args, substr_len);

    bool matched = false;

    while (true) {
        char *sub = va_arg(args, char *);
        if (sub == NULL) break;

        if (utf8_compare_len(string_at_index, sub, substr_len) == 0) {
            matched = true;
            break;
        }
    }

    va_end(args);

    return matched;

}

double_metaphone_codes_t *double_metaphone(char *input) {
    if (input == NULL) return NULL;

    char *ptr = utf8_upper(input);

    /* Note: NFD normalization will help with simple decomposable accent characters
       like "É", "Ü", etc. which effectively become "E\u0301" and "U\u0308". It does
       not handle characters like "Ł". For these, use Latin-ASCII transliteration
       prior to calling this function.

       We can still check for a specific accented character like C with cedilla (Ç),
       by comparing with its decomposed form i.e. "C\xcc\xa7"
    */

    char *normalized = (char *)utf8proc_NFD((utf8proc_uint8_t *)ptr);

    if (normalized != NULL) {
        free(ptr);
        ptr = normalized;        
    }

    if (ptr == NULL) {
        return NULL;
    }

    char *str = ptr;

    size_t len = strlen(str);
    char_array *primary = char_array_new_size(len + 1);
    char_array *secondary = char_array_new_size(len + 1);

    bool slavo_germanic = is_slavo_germanic(str);

    size_t current = 0;
    size_t last = len - 1;

    if (substring_equals(str, len, current, 2, "ʻ", NULL)) {
        str += 2;
    } else if (get_char_at(str, len, current) == '\'') {
        str++;
    }

    if (substring_equals(str, len, current, 2, "GN", "KN", "PN", "WR", "PS", NULL)) {
        current++;
    } else if (get_char_at(str, len, current) == 'X') {
        char_array_append(primary, "S");
        char_array_append(secondary, "S");
        current++;
    }

    while (true) {
        char c = *(str + current);
        if (c == '\x00') break;

        if (current == 0 && is_vowel(c)) {
            char_array_append(primary, "A");
            char_array_append(secondary, "A");
            current++;
            continue;
        } else if (c == 'B') {
            /* "-mb", e.g", "dumb", already skipped over... */
            char_array_append(primary, "P");
            char_array_append(secondary, "P");

            if (get_char_at(str, len, current + 1) == 'B') {
                current += 2;
            } else {
                current++;
            }
            continue;
        // Ç - C with cedilla (denormalized)
        } else if (substring_equals(str, len, current, 3, "C\xcc\xa7", NULL)) { 
            char_array_append(primary, "S");
            char_array_append(secondary, "S");
            current += 2;
        } else if (c == 'C') {
            // various germanic
            if ((current > 1)
                && !is_vowel(get_char_at(str, len, current - 2))
                && (substring_equals(str, len, current - 1, 3, "ACH", NULL)
                    && !substring_equals(str, len, current + 2, 1, "O", "A", "U", NULL))
                && ((get_char_at(str, len, current + 2) != 'I')
                    && ((get_char_at(str, len, current + 2) != 'E')
                        || substring_equals(str, len, current - 2, 6, "BACHER", "MACHER", NULL))
                   )
                ) 
            {
                char_array_append(primary, "K");
                char_array_append(secondary, "K");
                current += 2;
                continue;
            }

            // special case for "caesar"
            if ((current == 0)
                && substring_equals(str, len, current, 6, "CAESAR", NULL))
            {
                char_array_append(primary, "S");
                char_array_append(secondary, "K");
                current += 2;
                continue;
            }

            // Italian e.g. "chianti"
            if (substring_equals(str, len, current, 4, "CHIA", NULL)) {
                char_array_append(primary, "K");
                char_array_append(secondary, "K");
                current += 2;
                continue;
            }

            if (substring_equals(str, len, current, 2, "CH", NULL)) {
                // "michael"
                if ((current > 0)
                    && substring_equals(str, len, current, 4, "CHAE", NULL))
                {
                    char_array_append(primary, "K");
                    char_array_append(secondary, "X");
                    current += 2;
                    continue;
                }

                // Greek roots e.g. "chemistry", "chorus"
                if ((current == 0)
                    && (substring_equals(str, len, current + 1, 5, "HARAC", "HARIS", "HOREO", NULL)
                     || substring_equals(str, len, current + 1, 4, "HIRO", "HAOS", "HAOT", NULL)
                     || (substring_equals(str, len, current + 1, 3, "HOR", "HYM", "HIA", "HEM", "HIM", NULL) && !substring_equals(str, len, current + 1, 5, "HEMIN", NULL)))
                    )
                {
                    char_array_append(primary, "K");
                    char_array_append(secondary, "K");
                    current += 2;
                    continue;
                }

                // Germanic, Greek, or otherwise "ch" for "kh" sound
                if (
                      (substring_equals(str, len, 0, 4, "VAN ", "VON ", NULL)
                       || substring_equals(str, len, current - 5, 5, " VAN ", " VON ", NULL)
                       || substring_equals(str, len, 0, 3, "SCH", NULL))
                      // "ochestra", "orchid", "architect" but not "arch"
                      || substring_equals(str, len, current - 2, 6, "ORCHES", "ARCHIT", "ORCHID", NULL)
                      || substring_equals(str, len, current + 2, 1, "T", "S", NULL)
                      || (
                            (((current == 0) || substring_equals(str, len, current - 1, 1, "A", "O", "U", "E", NULL))
                             // e.g. not "breach", "broach", "pouch", "beech", etc.
                             && !substring_equals(str, len, current - 2, 2, "EA", "OU", "EE", "OA", "OO", "AU", NULL)
                             // e.g. not "lunch", "birch", "gulch"
                             && !substring_equals(str, len, current - 1, 1, "L", "R", "N", NULL))
                            // e.g. "wachtler", "wechsler", but not "tichner"
                            && ((current + 1 == last) || substring_equals(str, len, current + 2, 1, "L", "R", "N", "M", "B", "H", "F", "V", "W", " ", NULL))
                         )
                   )
                {
                    char_array_append(primary, "K");
                    char_array_append(secondary, "K");
                } else {
                    if (current > 0) {
                        if (substring_equals(str, len, 0, 2, "MC", NULL)) {
                            char_array_append(primary, "K");
                            char_array_append(secondary, "K");
                        } else {
                            char_array_append(primary, "X");
                            char_array_append(secondary, "K");
                        }
                    } else {
                        char_array_append(primary, "X");
                        char_array_append(secondary, "X");
                    }
                }
                current += 2;
                continue;
            }

            // e.g, "czerny"
            if (substring_equals(str, len, current, 2, "CZ", NULL)
                && !substring_equals(str, len, current - 2, 4, "WICZ", NULL))
            {
                char_array_append(primary, "S");
                char_array_append(secondary, "X");
                current += 2;
                continue;
            }

            // double 'C' but not if e.g. "McClellan"
            if (substring_equals(str, len, current, 2, "CC", NULL)
                && !((current == 1) && get_char_at(str, len, 0) == 'M'))
            {
                // "bellocchio" but not "bacchus"
                if (substring_equals(str, len, current + 2, 1, "I", "E", "H", NULL)
                    && !substring_equals(str, len, current + 2, 3, "HUS", "HUM", "HUN", "HAN", NULL))
                {
                    // "accident", "accede", "succeed"
                    if (((current == 1)
                         && (get_char_at(str, len, current - 1) == 'A'))
                        || substring_equals(str, len, current - 1, 5, "UCCEE", "UCCES", NULL))
                    {
                        char_array_append(primary, "KS");
                        char_array_append(secondary, "KS");
                    // "pinocchio" but not "riccio" or "picchu"
                    } else if (get_char_at(str, len, current + 2) == 'H'
                           && !substring_equals(str, len, current + 2, 2, "HU", "HA", NULL)) {
                        char_array_append(primary, "K");
                        char_array_append(secondary, "X");
                    } else {
                        char_array_append(primary, "X");
                        char_array_append(secondary, "X");
                    }
                    current += 3;
                    continue;
                } else {
                    // Pierce's rule
                    char_array_append(primary, "K");
                    char_array_append(secondary, "K");
                    current += 2;
                    continue;
                }
            }

            if (substring_equals(str, len, current, 2, "CK", "CG", "CQ", NULL)) {
                char_array_append(primary, "K");
                char_array_append(secondary, "K");
                current += 2;
                continue;
            }

            if (substring_equals(str, len, current, 2, "CI", "CJ", "CE", "CY", NULL)) {
                if (substring_equals(str, len, current, 3, "CIO", "CIE", "CIA", "CIU", NULL)) {
                    char_array_append(primary, "S");
                    char_array_append(secondary, "X");         
                } else {
                    char_array_append(primary, "S");
                    char_array_append(secondary, "S");                    
                }
                current += 2;
                continue;
            }

            // else
            char_array_append(primary, "K");
            char_array_append(secondary, "K");

            if (substring_equals(str, len, current + 1, 2, " C", " Q", " G", NULL)) {
                current += 3;
            } else if (substring_equals(str, len, current + 1, 1, "C", "K", "Q", NULL)
                   && !substring_equals(str, len, current + 1, 2, "CE", "CI", NULL))
            {
                current += 2;
            } else {
                current++;
            }

            continue;
        } else if (c == 'D') {
            if (substring_equals(str, len, current, 2, "DG", NULL)) {
                if (substring_equals(str, len, current + 2, 1, "I", "E", "Y", NULL)) {
                    // e.g. "edge"
                    char_array_append(primary, "J");
                    char_array_append(secondary, "J");
                    current += 3;
                    continue;
                } else {
                    char_array_append(primary, "TK");
                    char_array_append(secondary, "TK");
                    current += 2;
                    continue;
                }
            }

            if (substring_equals(str, len, current, 2, "DT", "DD", NULL)) {
                char_array_append(primary, "T");
                char_array_append(secondary, "T");
                current += 2;
                continue;
            }

            // else
            char_array_append(primary, "T");
            char_array_append(secondary, "T");
            current++;
            continue;
        } else if (c == 'F') {
            if (get_char_at(str, len, current + 1) == 'F') {
                current += 2;
            } else {
                current++;
            }

            char_array_append(primary, "F");
            char_array_append(secondary, "F");
            continue;
        } else if (c == 'G') {
            if (get_char_at(str, len, current + 1) == 'H') {
                if ((current > 0) && !is_vowel(get_char_at(str, len, current - 1))) {
                    char_array_append(primary, "K");
                    char_array_append(secondary, "K");
                    current += 2;
                    continue;
                }

                if (current == 0) {
                    // "ghislane", "ghiradelli"
                    if (get_char_at(str, len, current + 2) == 'I') {
                        char_array_append(primary, "J");
                        char_array_append(secondary, "J");
                    } else {
                        char_array_append(primary, "K");
                        char_array_append(secondary, "K");
                    }
                    current += 2;
                    continue;
                }

                // Parker's rule (with some further refinements) - e.g. "hugh"
                if (
                    ((current > 1)
                     && substring_equals(str, len, current - 2, 1, "B", "H", "D", NULL))
                    // e.g. "bough"
                    || ((current > 2)
                        && substring_equals(str, len, current - 3, 1, "B", "H", "D", NULL))
                    // e.g. "broughton"
                    || ((current > 3)
                        && substring_equals(str, len, current - 4, 1, "B", "H", NULL))
                    )
                {
                    current += 2;
                    continue;
                } else {
                    // e.g. "laugh", "McLaughlin", "cough", "gough", "rough", "tough"
                    if ((current > 2)
                        && (get_char_at(str, len, current - 1) == 'U')
                        && substring_equals(str, len, current - 3, 1, "C", "G", "L", "R", "T", NULL))
                    {
                        char_array_append(primary, "F");
                        char_array_append(secondary, "F");
                    } else if ((current > 0)
                               && get_char_at(str, len, current - 1) != 'I')
                    {
                        char_array_append(primary, "K");
                        char_array_append(secondary, "K");
                    }
                    current += 2;
                    continue;
                }

            }

            if (get_char_at(str, len, current + 1) == 'N') {
                if ((current == 1) && is_vowel(get_char_at(str, len, 0))
                    && !slavo_germanic)
                {
                    char_array_append(primary, "KN");
                    char_array_append(secondary, "N");
                // not e.g. "cagney"
                } else if (!substring_equals(str, len, current + 2, 2, "EY", NULL)
                        && (get_char_at(str, len, current + 1) != 'Y')
                        && !slavo_germanic)
                {
                    char_array_append(primary, "N");
                    char_array_append(secondary, "KN");
                } else {
                    char_array_append(primary, "KN");
                    char_array_append(secondary, "KN");
                }
                current += 2;
                continue;
            }

            // "tagliaro"
            if (substring_equals(str, len, current + 1, 2, "LI", NULL)
                && !slavo_germanic)
            {
                char_array_append(primary, "KL");
                char_array_append(secondary, "L");
                current += 2;
                continue;
            }

            // -ges-, -gep-, -gel-, -gie- at beginning
            if ((current == 0)
                && ((get_char_at(str, len, current + 1) == 'Y')
                    || substring_equals(str, len, current + 1, 2, "ES", "EP",
                                        "EB", "EL", "EY", "IB", "IL", "IN", "IE",
                                        "EI", "ER", NULL)))
            {
                char_array_append(primary, "K");
                char_array_append(secondary, "J");
                current += 2;
                continue;
            }

            // -ger-, -gy-
            if (
                (substring_equals(str, len, current + 1, 2, "ER", NULL)
                 || (get_char_at(str, len, current + 1) == 'Y'))
                && !substring_equals(str, len, 0, 6, "DANGER", "RANGER", "MANGER", NULL)
                && !substring_equals(str, len, current - 1, 1, "E", "I", NULL)
                && !substring_equals(str, len, current - 1, 3, "RGY", "OGY", NULL)
                )
            {
                char_array_append(primary, "K");
                char_array_append(secondary, "J");
                current += 2;
                continue;
            }

            // italian e.g. "viaggi"
            if (substring_equals(str, len, current + 1, 1, "E", "I", "Y", NULL)
                || substring_equals(str, len, current - 1, 4, "AGGI", "OGGI", NULL))
            {
                // obvious germanic
                if (
                    (substring_equals(str, len, 0, 4, "VAN ", "VON ", NULL)
                     || substring_equals(str, len, current - 5, 5, " VAN ", " VON ", NULL)
                     || substring_equals(str, len, 0, 3, "SCH", NULL))
                    || substring_equals(str, len, current + 1, 2, "ET", NULL))
                {
                    char_array_append(primary, "K");
                    char_array_append(secondary, "K");

                } else {
                    if (substring_equals(str, len, current + 1, 4, "IER ", NULL)
                        || ((current == len - 3) && substring_equals(str, len, current + 1, 3, "IER", NULL))) 
                    {
                        char_array_append(primary, "J");
                        char_array_append(secondary, "J");
                    } else {
                        char_array_append(primary, "J");
                        char_array_append(secondary, "K");
                    }
                }
                current += 2;
                continue;
            }

            if (get_char_at(str, len, current + 1) == 'G') {
                current += 2;
            } else {
                current++;
            }

            char_array_append(primary, "K");
            char_array_append(secondary, "K");
            continue;
        } else if (c == 'H') {
            // only keep if first & before vowel or between 2 vowels
            if (((current == 0) || is_vowel(get_char_at(str, len, current - 1)))
                && is_vowel(get_char_at(str, len, current + 1)))
            {
                char_array_append(primary, "H");
                char_array_append(secondary, "H");
                current += 2;
            // also takes care of "HH"
            } else {
                current++;
            }
            continue;
        } else if (c == 'J') {
            // obvious Spanish, "Jose", "San Jacinto"
            if (substring_equals(str, len, current, 4, "JOSE", NULL)
                || substring_equals(str, len, current, 5, "JOSÉ", NULL)
                || substring_equals(str, len, 0, 4, "SAN ", NULL))
            {
                if (((current == 0)
                     && (get_char_at(str, len, current + 4) == ' '))
                    || substring_equals(str, len, 0, 4, "SAN ", NULL))
                {
                    char_array_append(primary, "H");
                    char_array_append(secondary, "H");
                } else {
                    char_array_append(primary, "J");
                    char_array_append(secondary, "H");                    
                }

                current++;
                continue;
            }

            if ((current == 0)
                && !substring_equals(str, len, current, 4, "JOSE", NULL)
                && !substring_equals(str, len, current, 5, "JOSÉ", NULL))
            {
                // Yankelovich/Jankelowicz
                char_array_append(primary, "J");
                char_array_append(secondary, "A");
                current++;
                continue;
            } else {
                // Spanish pronoun of e.g. "bajador"
                if (is_vowel(get_char_at(str, len, current - 1))
                    && !slavo_germanic
                    && ((get_char_at(str, len, current + 1) == 'A')
                        || (get_char_at(str, len, current + 1) == 'O')))
                {
                    char_array_append(primary, "J");
                    char_array_append(secondary, "H");
                } else {
                    if (current == last || ((current == last - 1 || get_char_at(str, len, current + 2) == ' ') && isalpha(get_char_at(str, len, current - 1)) && substring_equals(str, len, current + 1, 1, "A", "O", NULL))) {
                        char_array_append(primary, "J");
                    } else {
                        if (!substring_equals(str, len, current + 1, 1, "L", "T",
                                              "K", "S", "N", "M", "B", "Z", NULL)
                            && !substring_equals(str, len, current - 1, 1, "S", "K", "L", NULL))
                        {
                            char_array_append(primary, "J");
                            char_array_append(secondary, "J");
                        }
                    }
                }

                // it could happen!
                if (get_char_at(str, len, current + 1) == 'J') {
                    current += 2;
                } else {
                    current++;
                }
                continue;
            }
        } else if (c == 'K') {
            if (get_char_at(str, len, current + 1) == 'K') {
                current += 2;
            } else {
                current++;
            }

            char_array_append(primary, "K");
            char_array_append(secondary, "K");
            continue;
        } else if (c == 'L') {
            if (get_char_at(str, len, current + 1) == 'L') {
                // Spanish e.g. "Cabrillo", "Gallegos"
                if (((current == (len - 3))
                     && substring_equals(str, len, current - 1, 4, "ILLO", "ILLA", "ALLE", NULL))
                    || ((substring_equals(str, len, last - 1, 2, "AS", "OS", NULL)
                      || substring_equals(str, len, last, 1, "A", "O", NULL))
                      && substring_equals(str, len, current - 1, 4, "ALLE", NULL)
                      )
                    )
                {
                    char_array_append(primary, "L");
                    current += 2;
                    continue;
                }

                current += 2;
            } else {
                current++;
            }
            char_array_append(primary, "L");
            char_array_append(secondary, "L");
            continue;
        } else if (c == 'M') {
            if ((substring_equals(str, len, current - 1, 3, "UMB", NULL)
                && (((current + 1) == last)
                    || substring_equals(str, len, current + 2, 2, "ER", NULL)))
                || (get_char_at(str, len, current + 1) == 'M'))
            {
                current += 2;
            } else {
                current++;
            }
            char_array_append(primary, "M");
            char_array_append(secondary, "M");
            continue;
        // Ñ (NFD normalized)
        } else if (substring_equals(str, len, current, 3, "N\xcc\x83", NULL)) {
            current += 3;
            char_array_append(primary, "N");
            char_array_append(secondary, "N");
            continue;
        } else if (c == 'N') {
            if (get_char_at(str, len, current + 1) == 'N')  {
                current += 2;
            } else {
                current++;
            }

            char_array_append(primary, "N");
            char_array_append(secondary, "N");
            continue;
        } else if (c == 'P') {
            if (substring_equals(str, len, current + 1, 1, "H", "F", NULL)) {
                char_array_append(primary, "F");
                char_array_append(secondary, "F");
                current += 2;
                continue;
            }

            // also account for "Campbell", "raspberry"
            if (substring_equals(str, len, current + 1, 1, "P", "B", NULL)) {
                current += 2;
            } else {
                current++;
            }

            char_array_append(primary, "P");
            char_array_append(secondary, "P");
            continue;
        } else if (c == 'Q') {
            if (get_char_at(str, len, current + 1) == 'Q') {
                current += 2;
            } else {
                current += 1;
            }

            char_array_append(primary, "K");
            char_array_append(secondary, "K");
            continue;
        } else if (c == 'R') {
            // french e.g. "rogier", but exclude "hochmeier"
            if ((current == last)
                && !slavo_germanic
                && substring_equals(str, len, current - 2, 2, "IE", NULL)
                && !substring_equals(str, len, current - 4, 2, "ME", "MA", NULL))
            {
                char_array_append(secondary, "R");
            } else {
                char_array_append(primary, "R");
                char_array_append(secondary, "R");
            }

            if (get_char_at(str, len, current + 1) == 'R') {
                current += 2;
            } else {
                current++;
            }
            continue;
        } else if (c == 'S') {
            // special cases "island", "isle", "carlisle", "carlysle"
            if (substring_equals(str, len, current - 1, 3, "ISL", "YSL", NULL)) {
                current++;
                continue;
            }

            // special case "sugar-"
            if ((current == 0)
                && substring_equals(str, len, current, 5, "SUGAR", NULL))
            {
                char_array_append(primary, "X");
                char_array_append(secondary, "S");
                current++;
                continue;
            }

            if (substring_equals(str, len, current, 2, "SH", NULL)) {
                // Germanic
                if (substring_equals(str, len, current + 1, 4, "HEIM", "HOEK", "HOLM", "HOLZ", NULL)) {
                    char_array_append(primary, "S");
                    char_array_append(secondary, "S");
                } else {
                    char_array_append(primary, "X");
                    char_array_append(secondary, "X");
                }
                current += 2;
                continue;
            }

            // Italian & Armenian
            if (substring_equals(str, len, current, 3, "SIO", "SIA", NULL)
                || substring_equals(str, len, current, 4, "SIAN", NULL))
            {
                if (!slavo_germanic) {
                    char_array_append(primary, "S");
                    char_array_append(secondary, "X");
                } else {
                    char_array_append(primary, "S");
                    char_array_append(secondary, "S");
                }
                current += 3;
                continue;
            }

            /* German & Anglicisations, e.g. "Smith" match "Schmidt", "Snider" match "Schneider"
               also, -sz- in Slavic language although in Hungarian it is pronounced 's' */
            if (((current == 0)
                 && substring_equals(str, len, current + 1, 1, "M", "N", "L", "W", NULL))
                || substring_equals(str, len, current + 1, 1, "Z", NULL))
            {
                char_array_append(primary, "S");
                char_array_append(secondary, "X");
                if (substring_equals(str, len, current + 1, 1, "Z", NULL)) {
                    current += 2;
                } else {
                    current++;
                }
                continue;
            }


            if (substring_equals(str, len, current, 2, "SC", NULL)) {
                // Schlesinger's rule
                if (get_char_at(str, len, current + 2) == 'H') {
                    // Dutch origin e.g. "school", "schooner"
                    if (substring_equals(str, len, current + 3, 2, "OO", "ER", "EN",
                                         "UY", "ED", "EM", NULL))
                    {
                        // "Schermerhorn", "Schenker"
                        if (substring_equals(str, len, current + 3, 2, "ER", "EN", NULL)) {
                            char_array_append(primary, "X");
                            char_array_append(secondary, "SK");
                        } else {
                            char_array_append(primary, "SK");
                            char_array_append(secondary, "SK");
                        }
                        current += 3;
                        continue;
                    } else {
                        if ((current == 0) && !is_vowel(get_char_at(str, len, 3))
                            && (get_char_at(str, len, 3) != 'W'))
                        {
                            char_array_append(primary, "X");
                            char_array_append(secondary, "S");
                        } else {
                            char_array_append(primary, "X");
                            char_array_append(secondary, "X");
                        }
                        current += 3;
                        continue;
                    }

                    if (substring_equals(str, len, current + 2, 1, "I", "E", "Y", NULL)) {
                        char_array_append(primary, "S");
                        char_array_append(secondary, "S");
                        current += 3;
                        continue;
                    }

                    char_array_append(primary, "SK");
                    char_array_append(secondary, "SK");
                    current += 3;
                    continue;
                }
            }

            // French e.g. "resnais", "artois"
            if ((current == last)
                && substring_equals(str, len, current - 2, 2, "AI", "OI", NULL))
            {
                char_array_append(secondary, "S");
            } else {
                char_array_append(primary, "S");
                char_array_append(secondary, "S");
            }

            if (substring_equals(str, len, current + 1, 1, "S", "Z", NULL)) {

                current += 2;
            } else {
                current++;
            }
            continue;
        } else if (c == 'T') {

            if (substring_equals(str, len, current, 4, "TION", NULL)) {
                char_array_append(primary, "X");
                char_array_append(secondary, "X");
                current += 3;
                continue;
            }

            if (substring_equals(str, len, current, 3, "TIA", "TCH", NULL)) {
                char_array_append(primary, "X");
                char_array_append(secondary, "X");
                current += 3;
                continue;
            }

            if (substring_equals(str, len, current, 2, "TH", NULL)
                || substring_equals(str, len, current, 3, "TTH", NULL)) 
            {
                // special case "Thomas", "Thames", or Germanic
                if (substring_equals(str, len, current + 2, 2, "OM", "AM", NULL)
                 || substring_equals(str, len, 0, 4, "VAN ", "VON ", NULL)
                 || substring_equals(str, len, current - 5, 5, " VAN ", " VON ", NULL)
                 || substring_equals(str, len, 0, 3, "SCH", NULL))
                {
                    char_array_append(primary, "T");
                    char_array_append(secondary, "T");
                } else {
                    // yes, zero
                    char_array_append(primary, "0");
                    char_array_append(secondary, "T");
                }

                current += 2;
                continue;
            }

            if (substring_equals(str, len, current + 1, 1, "T", "D", NULL)) {
                current += 2;
            } else {
                current++;
            }

            char_array_append(primary, "T");
            char_array_append(secondary, "T");
            continue;
        } else if (c == 'V') {
            if (get_char_at(str, len, current + 1) == 'V') {
                current += 2;
            } else {
                current++;
            }

            char_array_append(primary, "F");
            char_array_append(secondary, "F");
            continue;
        } else if (c == 'W') {
            // can also be in the middle of word
            if (substring_equals(str, len, current, 2, "WR", NULL)) {
                char_array_append(primary, "R");
                char_array_append(secondary, "R");
                current += 2;
                continue;
            }

            if ((current == 0)
                && (is_vowel(get_char_at(str, len, current + 1))
                || substring_equals(str, len, current, 2, "WH", NULL)))
            {
                // Wasserman should match Vasserman
                if (is_vowel(get_char_at(str, len, current + 1))) {
                    char_array_append(primary, "A");
                    char_array_append(secondary, "F");
                } else {
                    // need Uomo to match Womo
                    char_array_append(primary, "A");
                    char_array_append(secondary, "A");
                }
            }

            // Arnow should match Arnoff
            if (((current == last) && is_vowel(get_char_at(str, len, current - 1)))
                || substring_equals(str, len, current - 1, 5, "EWSKI", "EWSKY",
                                    "OWSKI", "OWSKY", NULL)
                || substring_equals(str, len, 0, 3, "SCH", NULL))
            {
                char_array_append(secondary, "F");
                current++;
                continue;
            }

            // Polish e.g. "Filipowicz"
            if (substring_equals(str, len, current, 4, "WICZ", "WITZ", NULL)) {
                char_array_append(primary, "TS");
                char_array_append(secondary, "FX");
                current += 4;
                continue;
            }

            // else skip it
            current++;
            continue;
        } else if (c == 'X') {
            // French e.g. "breaux"
            if (!((current == last)
                  && (substring_equals(str, len, current - 3, 3, "IAU", "EAU", NULL)
                   || substring_equals(str, len, current - 2, 2, "AU", "OU", NULL))))
            {
                char_array_append(primary, "KS");
                char_array_append(secondary, "KS");
            }

            if (substring_equals(str, len, current + 1, 1, "C", "X", NULL)) {
                current += 2;
            } else {
                current++;
            }
            continue;
        } else if (c == 'Z') {
            // Chinese Pinyin e.g. "Zhao"
            if (get_char_at(str, len, current + 1) == 'H') {
                char_array_append(primary, "J");
                char_array_append(secondary, "J");
                current += 2;
                continue;
            } else if (substring_equals(str, len, current + 1, 2, "ZO", "ZI", "ZA", NULL)
                   || (slavo_germanic
                       && ((current > 0)
                       && get_char_at(str, len, current - 1) != 'T')))
            {
                char_array_append(primary, "S");
                char_array_append(secondary, "TS");
            } else {
                char_array_append(primary, "S");
                char_array_append(secondary, "S");
            }

            if (get_char_at(str, len, current + 1) == 'Z') {
                current += 2;
            } else {
                current++;
            }
            continue;
        } else {
            current++;
        }
    }

    double_metaphone_codes_t *codes = calloc(1, sizeof(double_metaphone_codes_t));
    if (codes == NULL) {
        char_array_destroy(primary);
        char_array_destroy(secondary);
        return NULL;
    }

    codes->primary = char_array_to_string(primary);
    codes->secondary = char_array_to_string(secondary);

    free(ptr);

    return codes;
}

void double_metaphone_codes_destroy(double_metaphone_codes_t *codes) {
    if (codes != NULL) {
        if (codes->primary != NULL) {
            free(codes->primary);
        }

        if (codes->secondary != NULL) {
            free(codes->secondary);
        }

        free(codes);
    }
}