#ifndef TOKEN_TYPES_H
#define TOKEN_TYPES_H

// Doing these as #defines so we can duplicate the values exactly in Python

#define END 0                   // Null byte

// Word types
#define WORD 1                  // Any letter-only word (includes all unicode letters)
#define ABBREVIATION 2          // Loose abbreviations (roughly anything containing a "." as we don't care about sentences in addresses)
#define IDEOGRAPHIC_CHAR 3      // For languages that don't separate on whitespace (e.g. Chinese, Japanese, Korean), separate by character
#define HANGUL_SYLLABLE 4       // Hangul syllable sequences which contain more than one codepoint
#define ACRONYM 5               // Specifically things like U.N. where we may delete internal periods

#define PHRASE 10               // Not part of the first stage tokenizer, but may be used after phrase parsing

// Special tokens
#define EMAIL 20                // Make sure emails are tokenized altogether
#define URL 21                  // Make sure urls are tokenized altogether
#define US_PHONE 22             // US phone number (with or without country code)
#define INTL_PHONE 23           // A non-US phone number (must have country code)

// Numbers and numeric types
#define NUMERIC 50              // Any sequence containing a digit
#define ORDINAL 51              // 1st, 2nd, 1er, 1 etc.
#define ROMAN_NUMERAL 52        // II, III, VI, etc.
#define IDEOGRAPHIC_NUMBER 53   // All numeric ideographic characters, includes e.g. Han numbers and chars like "²"


// Punctuation types, may separate a phrase
#define PERIOD 100
#define EXCLAMATION 101
#define QUESTION_MARK 102
#define COMMA 103
#define COLON 104
#define SEMICOLON 105
#define PLUS 106
#define AMPERSAND 107
#define AT_SIGN 108
#define POUND 109
#define ELLIPSIS 110
#define DASH 111
#define BREAKING_DASH 112
#define HYPHEN 113
#define PUNCT_OPEN 114
#define PUNCT_CLOSE 115
#define DOUBLE_QUOTE 119
#define SINGLE_QUOTE 120
#define OPEN_QUOTE 121
#define CLOSE_QUOTE 122
#define SLASH 124
#define BACKSLASH 125
#define GREATER_THAN 126
#define LESS_THAN 127

// Non-letters and whitespace
#define OTHER 200
#define WHITESPACE 300
#define NEWLINE 301

#define INVALID_CHAR 500


#define is_word_token(type) ((type) == WORD || (type) == ABBREVIATION || (type) == ACRONYM || (type) == IDEOGRAPHIC_CHAR || (type) == HANGUL_SYLLABLE)

#define is_ideographic(type) ((type) == IDEOGRAPHIC_CHAR || (type) == HANGUL_SYLLABLE || (type) == IDEOGRAPHIC_NUMBER)

#define is_numeric_token(type) ((type) == NUMERIC || (type) == IDEOGRAPHIC_NUMBER)

#define is_punctuation(type) ((type) >= PERIOD && (type) < OTHER)

#define is_special_punctuation(type) ((type) == AMPERSAND || (type) == PLUS || (type) == POUND)

#define is_special_token(type) ((type) == EMAIL || (type) == URL || (type) == US_PHONE || (type) == INTL_PHONE)

#define is_whitespace(type) ((type) == WHITESPACE)

#endif
