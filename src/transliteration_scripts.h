
#ifndef TRANSLITERATION_SCRIPTS_H
#define TRANSLITERATION_SCRIPTS_H

#include <stdlib.h>
#include "unicode_scripts.h"
#include "transliterate.h"

typedef struct script_transliteration_rule {
    script_type_t script;
    char *language;
    uint32_t index;
    uint32_t len;
} script_transliteration_rule_t;

script_transliteration_rule_t script_transliteration_rules[] = {
    {SCRIPT_GURMUKHI, NULL, 0, 1},
    {SCRIPT_TELUGU, NULL, 1, 1},
    {SCRIPT_CYRILLIC, "be", 2, 1},
    {SCRIPT_CYRILLIC, "bg", 3, 1},
    {SCRIPT_CYRILLIC, NULL, 4, 1},
    {SCRIPT_CYRILLIC, "ru", 5, 1},
    {SCRIPT_CYRILLIC, "uz", 6, 1},
    {SCRIPT_CYRILLIC, "kk", 7, 1},
    {SCRIPT_CYRILLIC, "sr", 8, 1},
    {SCRIPT_CYRILLIC, "mn", 9, 1},
    {SCRIPT_CYRILLIC, "mk", 10, 1},
    {SCRIPT_CYRILLIC, "uk", 11, 1},
    {SCRIPT_CYRILLIC, "ky", 12, 1},
    {SCRIPT_ORIYA, NULL, 13, 1},
    {SCRIPT_HANGUL, NULL, 14, 1},
    {SCRIPT_GUJARATI, NULL, 15, 1},
    {SCRIPT_HAN, NULL, 16, 1},
    {SCRIPT_ARMENIAN, NULL, 17, 1},
    {SCRIPT_TAMIL, NULL, 18, 1},
    {SCRIPT_BENGALI, NULL, 19, 1},
    {SCRIPT_MALAYALAM, NULL, 20, 1},
    {SCRIPT_HIRAGANA, NULL, 21, 1},
    {SCRIPT_KANNADA, NULL, 22, 1},
    {SCRIPT_LATIN, NULL, 23, 1},
    {SCRIPT_GEORGIAN, NULL, 24, 2},
    {SCRIPT_DEVANAGARI, NULL, 26, 1},
    {SCRIPT_THAI, NULL, 27, 1},
    {SCRIPT_GREEK, NULL, 28, 3},
    {SCRIPT_CANADIAN_ABORIGINAL, NULL, 31, 1},
    {SCRIPT_ARABIC, "fa", 32, 1},
    {SCRIPT_ARABIC, NULL, 33, 2},
    {SCRIPT_ARABIC, "ps", 35, 1},
    {SCRIPT_HEBREW, NULL, 36, 2},
    {SCRIPT_KATAKANA, NULL, 38, 1},
    {SCRIPT_COMMON, NULL, 39, 1}
};

char *script_transliterators[] = {
    "gurmukhi-latin",
    "telugu-latin",
    "belarusian-latin-bgn",
    "bulgarian-latin-bgn",
    "cyrillic-latin",
    "russian-latin-bgn",
    "uzbek-latin-bgn",
    "kazakh-latin-bgn",
    "serbian-latin-bgn",
    "mongolian-latin-bgn",
    "macedonian-latin-bgn",
    "ukrainian-latin-bgn",
    "kirghiz-latin-bgn",
    "oriya-latin",
    "korean-latin-bgn",
    "gujarati-latin",
    "han-latin",
    "armenian-latin-bgn",
    "tamil-latin",
    "bengali-latin",
    "malayam-latin",
    "hiragana-latin",
    "kannada-latin",
    "latin-ascii",
    "georgian-latin",
    "georgian-latin-bgn",
    "devanagari-latin",
    "thai-latin",
    "greek-latin",
    "greek-latin-bgn",
    "greek-latin-ungegn",
    "canadianaboriginal-latin",
    "persian-latin-bgn",
    "arabic-latin",
    "arabic-latin-bgn",
    "pashto-latin-bgn",
    "hebrew-latin",
    "hebrew-latin-bgn",
    "katakana-latin-bgn",
    "latin-ascii"
}

#endif
