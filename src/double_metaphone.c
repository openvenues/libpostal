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

static inline bool substring_equals(char *str, size_t len, ssize_t index, size_t substr_len, size_t nargs, ...) {
    char *string_at_index = get_string_at(str, len, index);
    if (string_at_index == NULL) return false;

    va_list args;
    char *sub;

    va_start(args, nargs);

    bool matched = false;

    for (size_t i = 0; i < nargs; i++) {
        sub = va_arg(args, char *);
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

    if (substring_equals(str, len, current, 2, 1, "ʻ")) {
        str += 2;
    } else if (get_char_at(str, len, current) == '\'') {
        str++;
    }

    if (substring_equals(str, len, current, 2, 5, "GN", "KN", "PN", "WR", "PS")) {
        current++;
    } else if (get_char_at(str, len, current) == 'X') {
        char_array_append(primary, "S");
        char_array_append(secondary, "S");
        current++;
    }

    while (true) {
        char c = *(str + current);
        if (c == '\x00') break;

        if (is_vowel(c) && current == 0) {
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
        // C with cedilla (denormalized)
        } else if (substring_equals(str, len, current, 3, 2, "C\xcc\xa7", "Ç")) { 
            char_array_append(primary, "S");
            char_array_append(secondary, "S");
            current += 2;
        } else if (c == 'C') {
            // various germanic
            if ((current > 1)
                && !is_vowel(get_char_at(str, len, current - 2))
                && substring_equals(str, len, current - 1, 3, 1, "ACH")
                && ((get_char_at(str, len, current + 2) != 'I')
                    && ((get_char_at(str, len, current + 2) != 'E')
                        || substring_equals(str, len, current - 2, 6, 2, "BACHER", "MACHER"))
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
                && substring_equals(str, len, current, 6, 1, "CAESAR"))
            {
                char_array_append(primary, "S");
                char_array_append(secondary, "K");
                current += 2;
                continue;
            }

            // Italian e.g. "chianti"
            if (substring_equals(str, len, current, 4, 1, "CHIA")) {
                char_array_append(primary, "K");
                char_array_append(secondary, "K");
                current += 2;
                continue;
            }

            if (substring_equals(str, len, current, 2, 1, "CH")) {
                // "michael"
                if ((current > 0)
                    && substring_equals(str, len, current, 4, 1, "CHAE"))
                {
                    char_array_append(primary, "K");
                    char_array_append(secondary, "K");
                    current += 2;
                    continue;
                }

                // Greek roots e.g. "chemistry", "chorus"
                if ((current == 0)
                    && (substring_equals(str, len, current + 1, 5, 3, "HARAC", "HARIS", "HOREO")
                     || substring_equals(str, len, current + 1, 4, 3, "HIRO", "HAOS", "HAOT")
                     || substring_equals(str, len, current + 1, 3, 5, "HOR", "HYM", "HIA", "HEM", "HIM"))
                    )
                {
                    char_array_append(primary, "K");
                    char_array_append(secondary, "K");
                    current += 2;
                    continue;
                }

                // Germanic, Greek, or otherwise "ch" for "kh" sound
                if (
                      (substring_equals(str, len, 0, 4, 2, "VAN ", "VON ")
                       || substring_equals(str, len, current - 5, 5, 2, " VAN ", " VON ")
                       || substring_equals(str, len, 0, 3, 1, "SCH"))
                      // "ochestra", "orchid", "architect" but not "arch"
                      || substring_equals(str, len, current - 2, 6, 1, "ORCHES", "ARCHIT", "ORCHID")
                      || substring_equals(str, len, current + 2, 1, 2, "T", "S")
                      || (
                            ((current == 0) || substring_equals(str, len, current - 1, 1, 4, "A", "O", "U", "E"))
                            // e.g. "wachtler", "wechsler", but not "tichner"
                            && substring_equals(str, len, current + 2, 1, 10, "L", "R", "N", "M", "B", "H", "F", "V", "W", " ")
                         )
                   )
                {
                    char_array_append(primary, "K");
                    char_array_append(secondary, "K");
                    current += 2;
                    continue;
                } else {
                    if (current > 0) {
                        if (substring_equals(str, len, 0, 2, 1, "MC")) {
                            char_array_append(primary, "K");
                            char_array_append(secondary, "K");
                        } else {
                            char_array_append(primary, "X");
                            char_array_append(secondary, "K");
                        }
                    } else {
                        char_array_append(primary, "X");
                        char_array_append(secondary, "K");
                    }
                }
                current += 2;
                continue;
            }

            // e.g, "czerny"
            if (substring_equals(str, len, current, 2, 1, "CZ")
                && !substring_equals(str, len, current - 2, 4, 1, "WICZ"))
            {
                char_array_append(primary, "S");
                char_array_append(secondary, "X");
                current += 2;
                continue;
            }

            // e.g. "focaccia"
            if (substring_equals(str, len, current + 1, 3, 1, "CIA")) {
                char_array_append(primary, "X");
                char_array_append(secondary, "X");
                current += 3;
                continue;              
            }

            // double 'C' but not if e.g. "McClellan"
            if (substring_equals(str, len, current, 2, 1, "CC")
                && !((current == 1) && get_char_at(str, len, 0) == 'M'))
            {
                // "bellocchio" but not "bacchus"
                if (substring_equals(str, len, current + 2, 1, 3, "I", "E", "H")
                    && !substring_equals(str, len, current + 2, 2, 1, "HU"))
                {
                    // "accident", "accede", "succeed"
                    if (((current == 1)
                         && (get_char_at(str, len, current - 1) == 'A'))
                        || substring_equals(str, len, current - 1, 5, 2, "UCCEE", "UCCES"))
                    {
                        char_array_append(primary, "KS");
                        char_array_append(secondary, "KS");
                    } else {
                        char_array_append(primary, "X");
                        char_array_append(secondary, "X");
                    }
                    current += 3;
                    continue;
                }
            } else {
                char_array_append(primary, "K");
                char_array_append(secondary, "K");
                current += 2;
                continue;
            }

            if (substring_equals(str, len, current, 2, 3, "CK", "CG", "CQ")) {
                char_array_append(primary, "K");
                char_array_append(secondary, "K");
                current += 2;
                continue;
            }

            if (substring_equals(str, len, current, 2, 3, "CI", "CE", "CY")) {
                if (substring_equals(str, len, current, 3, 3, "CIO", "CIE", "CIA")) {
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

            if (substring_equals(str, len, current + 1, 2, 3, " C", " Q", " G")) {
                current += 3;
            } else if (substring_equals(str, len, current + 1, 1, 3, "C", "K", "Q")
                   && !substring_equals(str, len, current + 1, 2, 2, "CE", "CI"))
            {
                current += 2;
            } else {
                current++;
            }

            continue;
        } else if (substring_equals(str, len, current, 2, 1, "Đ")) {
            char_array_append(primary, "T");
            char_array_append(secondary, "T");
            current += 2;
            continue;
        } else if (c == 'D') {
            if (substring_equals(str, len, current, 2, 1, "DG")) {
                if (substring_equals(str, len, current + 2, 1, 3, "I", "E", "Y")) {
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

            if (substring_equals(str, len, current, 2, 2, "DT", "DD")) {
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

                if (current < 3) {
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
                     && substring_equals(str, len, current - 2, 1, 3, "B", "H", "D"))
                    // e.g. "bough"
                    || ((current > 2)
                        && substring_equals(str, len, current - 3, 1, 3, "B", "H", "D"))
                    // e.g. "broughton"
                    || ((current > 3)
                        && substring_equals(str, len, current - 4, 1, 2, "B", "H"))
                    )
                {
                    current += 2;
                    continue;
                } else {
                    // e.g. "laugh", "McLaughlin", "cough", "gough", "rough", "tough"
                    if ((current > 2)
                        && (get_char_at(str, len, current - 1) == 'U')
                        && substring_equals(str, len, current - 3, 1, 5, "C", "G", "L", "R", "T"))
                    {
                        char_array_append(primary, "F");
                        char_array_append(secondary, "F");
                    } else if ((current > 0)
                               && get_char_at(str, len, current - 1) == 'I')
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
                    && slavo_germanic)
                {
                    char_array_append(primary, "KN");
                    char_array_append(secondary, "N");
                // not e.g. "cagney"
                } else if (!substring_equals(str, len, current + 2, 2, 1, "EY")
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
            if (substring_equals(str, len, current + 1, 2, 1, "LI")
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
                    || substring_equals(str, len, current + 1, 2, 13, "ES", "EP",
                                        "EB", "EL", "EY", "IB", "IL", "IN", "IE",
                                        "EI", "ER")))
            {
                char_array_append(primary, "K");
                char_array_append(secondary, "J");
                current += 2;
                continue;
            }

            // -ger-, -gy-
            if (
                (substring_equals(str, len, current + 1, 2, 1, "ER")
                 || (get_char_at(str, len, current + 1) == 'Y'))
                && !substring_equals(str, len, 0, 6, 3, "DANGER", "RANGER", "MANGER")
                && !substring_equals(str, len, current - 1, 1, 2, "E", "I")
                && !substring_equals(str, len, current - 1, 3, 2, "RGY", "OGY")
                )
            {
                char_array_append(primary, "K");
                char_array_append(secondary, "J");
                current += 2;
                continue;
            }

            // italian e.g. "viaggi"
            if (substring_equals(str, len, current + 1, 1, 3, "E", "I", "Y")
                || substring_equals(str, len, current - 1, 4, 2, "AGGI", "OGGI"))
            {
                // obvious germanic
                if (
                    (substring_equals(str, len, 0, 4, 2, "VAN ", "VON ")
                     || substring_equals(str, len, current - 5, 5, 2, " VAN ", " VON ")
                     || substring_equals(str, len, 0, 3, 1, "SCH"))
                    || substring_equals(str, len, current + 1, 2, 1, "ET"))
                {
                    char_array_append(primary, "K");
                    char_array_append(secondary, "K");
                } else {
                    if (substring_equals(str, len, current + 1, 4, 1, "IER ")
                        || ((current == len - 3) && substring_equals(str, len, current + 1, 3, 1, "IER"))) 
                    {
                        char_array_append(primary, "J");
                        char_array_append(secondary, "J");
                    } else {
                        char_array_append(primary, "J");
                        char_array_append(secondary, "K");
                    }
                    current += 2;
                    continue;
                }
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
            if (substring_equals(str, len, current, 4, 1, "JOSE")
                || substring_equals(str, len, current, 5, 1, "JOSÉ")
                || substring_equals(str, len, 0, 4, 1, "SAN "))
            {
                if (((current == 0)
                     && (get_char_at(str, len, current + 4) == ' '))
                    || substring_equals(str, len, 0, 4, 1, "SAN "))
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
                && !substring_equals(str, len, current, 4, 1, "JOSE")
                && !substring_equals(str, len, current, 5, 1, "JOSÉ"))
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
                    if (current == last) {
                        char_array_append(primary, "J");
                    } else {
                        if (!substring_equals(str, len, current + 1, 1, 8, "L", "T",
                                              "K", "S", "N", "M", "B", "Z")
                            && !substring_equals(str, len, current - 1, 1, 3, "S", "K", "L"))
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
        } else if (substring_equals(str, len, current, 2, 1, "Ł")) {
            current += 2;
            char_array_append(primary, "L");
            char_array_append(secondary, "L");
            continue;
        } else if (c == 'L') {
            if (get_char_at(str, len, current + 1) == 'L') {
                // Spanish e.g. "Cabrillo", "Gallegos"
                if (((current == (len - 3))
                     && substring_equals(str, len, current - 1, 4, 3, "ILLO", "ILLA", "ALLE"))
                    || ((substring_equals(str, len, last - 1, 2, 2, "AS", "OS")
                      || substring_equals(str, len, last, 1, 2, "A", "O"))
                      && substring_equals(str, len, current - 1, 4, 1, "ALLE")
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
            if ((substring_equals(str, len, current - 1, 3, 1, "UMB")
                && (((current + 1) == last)
                    || substring_equals(str, len, current + 2, 2, 1, "ER")))
                || (get_char_at(str, len, current + 1) == 'M'))
            {
                current += 2;
            } else {
                current++;
            }
            char_array_append(primary, "M");
            char_array_append(secondary, "M");
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
        } else if (substring_equals(str, len, current, 2, 1, "Ñ")) {
            current += 2;
            char_array_append(primary, "N");
            char_array_append(secondary, "N");
            continue;
        } else if (c == 'P') {
            if (get_char_at(str, len, current + 1) == 'H') {
                char_array_append(primary, "F");
                char_array_append(secondary, "F");
                current += 2;
                continue;
            }

            // also account for "Campbell", "raspberry"
            if (substring_equals(str, len, current + 1, 1, 2, "P", "B")) {
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
                && substring_equals(str, len, current - 2, 2, 1, "IE")
                && !substring_equals(str, len, current - 4, 2, 2, "ME", "MA"))
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
            if (substring_equals(str, len, current - 1, 3, 2, "ISL", "YSL")) {
                current++;
                continue;
            }

            // special case "sugar-"
            if ((current == 0)
                && substring_equals(str, len, current, 5, 1, "SUGAR"))
            {
                char_array_append(primary, "X");
                char_array_append(secondary, "S");
                current++;
                continue;
            }

            if (substring_equals(str, len, current, 2, 1, "SH")) {
                // Germanic
                if (substring_equals(str, len, current + 1, 4, 4, "HEIM", "HOEK", "HOLM", "HOLZ")) {
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
            if (substring_equals(str, len, current, 3, 2, "SIO", "SIA")
                || substring_equals(str, len, current, 4, 1, "SIAN"))
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
                 && substring_equals(str, len, current + 1, 1, 4, "M", "N", "L", "W"))
                || substring_equals(str, len, current + 1, 1, 1, "Z"))
            {
                char_array_append(primary, "S");
                char_array_append(secondary, "X");
                if (substring_equals(str, len, current + 1, 1, 1, "Z")) {
                    current += 2;
                } else {
                    current++;
                }
                continue;
            }


            if (substring_equals(str, len, current, 2, 1, "SC")) {
                // Schlesinger's rule
                if (get_char_at(str, len, current + 2) == 'H') {
                    // Dutch origin e.g. "school", "schooner"
                    if (substring_equals(str, len, current + 3, 2, 6, "OO", "ER", "EN",
                                         "UY", "ED", "EM"))
                    {
                        // "Schermerhorn", "Schenker"
                        if (substring_equals(str, len, current + 3, 2, 2, "ER", "EN")) {
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

                    if (substring_equals(str, len, current + 2, 1, 3, "I", "E", "Y")) {
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
                && substring_equals(str, len, current - 2, 2, 2, "AI", "OI"))
            {
                char_array_append(secondary, "S");
            } else {
                char_array_append(primary, "S");
                char_array_append(secondary, "S");
            }

            if (substring_equals(str, len, current - 1, 1, 2, "S", "Z")) {
                current += 2;
            } else {
                current++;
            }
            continue;
        } else if (c == 'T') {

            if (substring_equals(str, len, current, 4, 1, "TION")) {
                char_array_append(primary, "X");
                char_array_append(secondary, "X");
                current += 3;
                continue;
            }

            if (substring_equals(str, len, current, 3, 2, "TIA", "TCH")) {
                char_array_append(primary, "X");
                char_array_append(secondary, "X");
                current += 3;
                continue;
            }

            if (substring_equals(str, len, current, 2, 1, "TH")
                || substring_equals(str, len, current, 3, 1, "TTH")) 
            {
                // special case "Thomas", "Thames", or Germanic
                if (substring_equals(str, len, current + 2, 2, 2, "OM", "AM")
                 || substring_equals(str, len, 0, 4, 2, "VAN ", "VON ")
                 || substring_equals(str, len, current - 5, 5, 2, " VAN ", " VON ")
                 || substring_equals(str, len, 0, 3, 1, "SCH"))
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

            if (substring_equals(str, len, current + 1, 1, 2, "T", "D")) {
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
            if (substring_equals(str, len, current, 2, 1, "WR")) {
                char_array_append(primary, "R");
                char_array_append(secondary, "R");
                current += 2;
                continue;
            }

            if ((current == 0)
                && (is_vowel(get_char_at(str, len, current + 1))
                || substring_equals(str, len, current, 2, 1, "WH")))
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
                || substring_equals(str, len, current - 1, 5, 4, "EWSKI", "EWSKY",
                                    "OWSKI", "OWSKY")
                || substring_equals(str, len, 0, 3, 1, "SCH"))
            {
                char_array_append(secondary, "F");
                current++;
                continue;
            }

            // Polish e.g. "Filipowicz"
            if (substring_equals(str, len, current, 4, 2, "WICZ", "WITZ")) {
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
                  && (substring_equals(str, len, current - 3, 3, 2, "IAU", "EAU")
                   || substring_equals(str, len, current - 2, 2, 2, "AU", "OU"))))
            {
                char_array_append(primary, "KS");
                char_array_append(secondary, "KS");
            }

            if (substring_equals(str, len, current + 1, 1, 2, "C", "X")) {
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
            } else if (substring_equals(str, len, current + 1, 2, 3, "ZO", "ZI", "ZA")
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