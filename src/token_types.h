#ifndef TOKEN_TYPES_H
#define TOKEN_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

// Doing these as #defines so we can duplicate the values exactly in Python

#define END 0               // Null byte

// Word types
#define WORD 1              // Any letter-only word (includes all unicode letters)
#define ABBREVIATION 2      // Loose abbreviations (ending in ".")
#define IDEOGRAM 3          // For languages that don't separate on whitespace (e.g. Chinese, Japanese, Korean), separate by character
#define PHRASE 4            // Not part of the first stage tokenizer, but may be used after phrase parsing

// Numbers and numeric types
#define NUMBER 50           // All digits
#define NUMERIC 51          // Any sequence containing a digit
#define ORDINAL 52          // 1st, 2nd, etc.
#define NUMERIC_RANGE 53    // 2-3, Queens addresses, US ZIP+4 codes
#define ORDINAL_RANGE 54    // 1-2nd, 1st-2nd
#define ROMAN_NUMERAL 55    // II, III, VI, etc.
#define US_PHONE 56         // US phone number (with or without country code)
#define INTL_PHONE 57       // A non-US phone number (must have country code)

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
#define LPAREN 114
#define RPAREN 115
#define LBSQUARE 116
#define RBSQUARE 117
#define DOUBLE_QUOTE 118
#define SINGLE_QUOTE 119
#define LEFT_DOUBLE_QUOTE 120
#define RIGHT_DOUBLE_QUOTE 121
#define LEFT_SINGLE_QUOTE 122
#define RIGHT_SINGLE_QUOTE 123
#define SLASH 124
#define BACKSLASH 125
#define GREATER_THAN 126
#define LESS_THAN 127

// Non-letters and whitespace
#define OTHER 200
#define WHITESPACE 300
#define NEWLINE 301 


#ifdef __cplusplus
}
#endif

#endif
