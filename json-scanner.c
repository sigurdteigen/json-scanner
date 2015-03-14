/*
    Copyright (c) 2015, Sigurd Teigen
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of  nor the names of its contributors may be used to
       endorse or promote products derived from this software without specific
       prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
 */

#include "json-scanner.h"

#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

JsonToken json_scanner_init(JsonScanner *scanner, const uint8_t *s, size_t len) {
    scanner->start = s;
    scanner->len = len;
    return (JsonToken) { JST_BEGIN, s, 0 };
}

static JsonToken scan_string(JsonScanner *scanner, const uint8_t *s) {
    const uint8_t *start = s;
    size_t remaining = scanner->len - (s - scanner->start);

    bool escaped = false;
    for (size_t i = 0; i < remaining; i++) {
        if (escaped) {
            switch (s[i]) {
            case '"': case '\\': case '/':
            case 'b': case 'f': case 'n':
            case 'r': case 't':
                break;

            case 'u':
                if (i + 4 > remaining   ||
                    !isxdigit(s[i + 1]) ||
                    !isxdigit(s[i + 2]) ||
                    !isxdigit(s[i + 3]) ||
                    !isxdigit(s[i + 4])) {
                    return (JsonToken) { JST_ERR_BAD_ESCAPE, start, i };
                } else {
                    i += 4;
                }
                break;

            default:
                return (JsonToken) { JST_ERR_BAD_ESCAPE, start, i };
            }

            escaped = false;
        } else {
            switch (s[i]) {
            case '"':
                return (JsonToken) { JST_STRING, start, i };
            case '\\':
                escaped = true;
                break;

            default:
                break;
            }
        }
    }

    return (JsonToken) { JST_ERR_INCOMPLETE, start, remaining };
}

static inline bool is_whitespace(uint8_t c) {
    return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D;
}

static inline bool is_number_delim(uint8_t c) {
    return is_whitespace(c) || c == ',' || c == '}' || c == ']';
}


static JsonToken scan_number(JsonScanner *scanner, const uint8_t *s) {
    const uint8_t *start = s;
    size_t remaining = scanner->len - (s - scanner->start);

    typedef enum {
        STATE_START,
        STATE_SIGN_1,
        STATE_SIGN_2,
        STATE_ZERO,
        STATE_DIGIT_1,
        STATE_DIGIT_2,
        STATE_DIGIT_3,
        STATE_DOT,
        STATE_EXP,
    } State;

    State state = STATE_START;

    for (size_t i = 0; i < remaining; i++) {
        switch (state) {
        case STATE_START:
            if (s[i] == '-') {
                state = STATE_SIGN_1;
            } else if (s[i] == '0') {
                state = STATE_ZERO;
            } else if (isdigit(s[i])) {
                state = STATE_DIGIT_1;
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, start + i, 1 };
            }
            break;

        case STATE_SIGN_1:
            if (s[i] == '0') {
                state = STATE_ZERO;
            } else if (isdigit(s[i])) {
                state = STATE_DIGIT_1;
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, start + i, 1 };
            }
            break;

        case STATE_SIGN_2:
            if (isdigit(s[i])) {
                state = STATE_DIGIT_3;
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, start + i, 1 };
            }
            break;

        case STATE_ZERO:
            if (s[i] == '.') {
                state = STATE_DOT;
            } else if (s[i] == 'e' || s[i] == 'E') {
                state = STATE_EXP;
            } else if (is_number_delim(s[i])) {
                return (JsonToken) { JST_NUMBER, start, i };
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, start + i, 1 };
            }
            break;

        case STATE_DIGIT_1:
            if (s[i] == '.') {
                state = STATE_DOT;
            } else if (s[i] == 'e' || s[i] == 'E') {
                state = STATE_EXP;
            } else if (isdigit(s[i])) {
                state = STATE_DIGIT_1;
            } else if (is_number_delim(s[i])) {
                return (JsonToken) { JST_NUMBER, start, i };
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, start + i, 1 };
            }
            break;

        case STATE_DIGIT_2:
            if (s[i] == 'e' || s[i] == 'E') {
                state = STATE_EXP;
            } else if (isdigit(s[i])) {
                state = STATE_DIGIT_2;
            } else if (is_number_delim(s[i])) {
                return (JsonToken) { JST_NUMBER, start, i };
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, start + i, 1 };
            }
            break;

        case STATE_DIGIT_3:
            if (isdigit(s[i])) {
                state = STATE_DIGIT_3;
            } else if (is_number_delim(s[i])) {
                return (JsonToken) { JST_NUMBER, start, i };
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, start + i, 1 };
            }
            break;

        case STATE_DOT:
            if (isdigit(s[i])) {
                state = STATE_DIGIT_2;
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, start + i, 1 };
            }
            break;

        case STATE_EXP:
            if (s[i] == '+' || s[i] == '-') {
                state = STATE_SIGN_2;
            } else if (isdigit(s[i])) {
                state = STATE_DIGIT_3;
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, start + i, 1 };
            }
            break;

        default:
            assert(0 && "Never reach");
            break;
        }
    }

    switch (state) {
    case STATE_ZERO:
    case STATE_DIGIT_1: case STATE_DIGIT_2: case STATE_DIGIT_3:
        return (JsonToken) { JST_NUMBER, start, remaining };

    default:
        return (JsonToken) { JST_ERR_INCOMPLETE, start, remaining };
    }
}

JsonToken json_scanner_next(JsonScanner *scanner, const JsonToken last_token) {
    if (last_token.type == JST_EOF) {
        return last_token;
    }

    const uint8_t *s = scanner->start;
    if (last_token.type != JST_BEGIN) {
        s = last_token.start + last_token.len;
        if (last_token.type == JST_STRING) {
            s++; // skip end "
        }
    }

    const size_t remaining = scanner->len - (s - scanner->start);

    for (size_t i = 0; i < remaining; i++) {
        if (is_whitespace(s[i])) {
            continue;
        }

        switch (s[i]) {
        case ',':
            return (JsonToken) { JST_COMMA, s + i, 1 };
        case ':':
            return (JsonToken) { JST_COLON, s + i, 1 };
        case '{':
            return (JsonToken) { JST_OBJECT_OPEN, s + i, 1 };
        case '}':
            return (JsonToken) { JST_OBJECT_CLOSE, s + i, 1 };
        case '[':
            return (JsonToken) { JST_ARRAY_OPEN, s + i, 1 };
        case ']':
            return (JsonToken) { JST_ARRAY_CLOSE, s + i, 1 };

        case '"':
            return scan_string(scanner, s + i + 1);
        case '-':
            return scan_number(scanner, s + i);

        case 't':
            if (strncmp("true", s + i, MIN(4, remaining)) == 0) {
                return (JsonToken) { JST_TRUE, s + i, 4 };
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, s + i, remaining };
            }

        case 'f':
            if (strncmp("false", s + i, MIN(5, remaining)) == 0) {
                return (JsonToken) { JST_FALSE, s + i, 5 };
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, s + i, remaining };
            }

        case 'n':
            if (strncmp("null", s + i, MIN(4, remaining)) == 0) {
                return (JsonToken) { JST_NULL, s + i, 4 };
            } else {
                return (JsonToken) { JST_ERR_UNEXPECTED, s + i, remaining };
            }


        default:
            if (isdigit(s[i])) {
                return scan_number(scanner, s + i);
            }

            return (JsonToken) { JST_ERR_UNEXPECTED, s + i, 1 };
        }
    }

    return (JsonToken) { JST_EOF, s, remaining };
}


int json_scanner_error(const JsonToken last_token) {
    switch (last_token.type) {
    case JST_ERR_BAD_ESCAPE:
    case JST_ERR_INCOMPLETE:
    case JST_ERR_UNEXPECTED:
        return last_token.type;

    default:
        return 0;
    }
}


void json_scanner_strerror(const JsonToken token, char *errbuf, size_t errbuf_len) {
    int c = 0;

    switch (token.type) {
    case JST_ERR_BAD_ESCAPE:
        snprintf(errbuf, errbuf_len, "Bad escape sequence in string: ");
        break;

    case JST_ERR_INCOMPLETE:
        snprintf(errbuf, errbuf_len, "Syntax is ok but was unexpectedly terminated: ");
        break;

    case JST_ERR_UNEXPECTED:
        snprintf(errbuf, errbuf_len, "Unexpected character: ");
        break;

    default:
        snprintf(errbuf, errbuf_len, "No error");
        return;
    }

    size_t rem = errbuf_len - c - 1;
    if (token.len < rem) {
        rem = token.len;
    }

    if (c < rem - 1) {
        strncpy(errbuf + c, token.start, rem);
    }
}
