#include "json-scanner.h"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static JsonToken checkToken(JsonScanner *scanner, const JsonToken last_token, JsonTokenType e_type, const uint8_t *e_str) {
    JsonToken t = json_scanner_next(scanner, last_token);
    if (t.type != e_type) {
        fprintf(stderr, "Expected type %d got %d\n", e_type, t.type);
        abort();
    }
    if (strncmp(e_str, t.start, t.len) != 0) {
        fprintf(stderr, "Expected val %s got %s\n", e_str, t.start);
        abort();
    }

    return t;
}

static void checkTokens(JsonScanner *scanner, const uint8_t *str, const uint8_t *expected, ...) {
    va_list ap;
    va_start(ap, expected);

    JsonToken tok = json_scanner_init(scanner, str, strlen(str));
    if (tok.type != JST_BEGIN) {
        fprintf(stderr, "Expected 'begin' token, got %d\n", tok.type);
        abort();
    }

    while (expected) {
        tok = json_scanner_next(scanner, tok);
        if (tok.len == 0) {
            fprintf(stderr, "Token length was zero: %s\n", tok.start);
            abort();
        }

        if (tok.type == JST_EOF || tok.type == JST_ERR_INCOMPLETE) {
            fprintf(stderr, "Expected more tokens\n");
            abort();
        }

        if (strncmp(expected, tok.start, tok.len) != 0) {
            fprintf(stderr, "Expected '%s' got '%s'\n", expected, tok.start);
            abort();
        }

        expected = va_arg(ap, const uint8_t *);
    }

    tok = json_scanner_next(scanner, tok);
    if (tok.type != JST_EOF) {
        fprintf(stderr, "Expected 'eof' token, got %d\n", tok.type);
        abort();
    }

    va_end(ap);
}

static void testComposites(void) {
    JsonScanner sc;
    checkTokens(&sc, "[\"hello\", \"world\" ]",
                "[", "hello", ",", "world", "]", NULL);
    checkTokens(&sc, "{ \"hello\": 123, \"x\": [ true ] }",
                "{", "hello", ":", "123", ",", "x", ":", "[", "true", "]", "}");

    {
        const uint8_t *data = "[]{},:";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ARRAY_OPEN, "[");
        t = checkToken(&sc, t, JST_ARRAY_CLOSE, "]");
        t = checkToken(&sc, t, JST_OBJECT_OPEN, "{");
        t = checkToken(&sc, t, JST_OBJECT_CLOSE, "}");
        t = checkToken(&sc, t, JST_COMMA, ",");
        t = checkToken(&sc, t, JST_COLON, ":");

    }

    fprintf(stdout, "[ OK ] Composites\n");
}

static void testStrings(void) {
    JsonScanner sc;
    {
        const uint8_t *data = "\"\\\\\"";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_STRING, "\\\\");
    }
    {
        const uint8_t *data = "\"\\\"\"";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_STRING, "\\\"");
    }
    {
        const uint8_t *data = "\"\\\\/\"";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_STRING, "\\\\/");
    }
    {
        const uint8_t *data = "\"\\b\\f\\n\\r\\t\"";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_STRING, "\\b\\f\\n\\r\\t");
    }
    {
        const uint8_t *data = "\"\\uabcd\"";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_STRING, "\\uabcd");
    }

    // evil
    {
        const uint8_t *data = "\"\\uabxd\"";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ERR_BAD_ESCAPE, "\\uabxd");
    }
    {
        const uint8_t *data = "\"\\z\"";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ERR_BAD_ESCAPE, "\\z");
    }
}

static void testNumbers(void) {
    JsonScanner sc;
    {
        const uint8_t *data = "0, 213 0.2]-0}0.1 -3.14e1 0E5 4e+2 3E-4";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));

        t = checkToken(&sc, t, JST_NUMBER, "0");
        t = checkToken(&sc, t, JST_COMMA, ",");
        t = checkToken(&sc, t, JST_NUMBER, "213");
        t = checkToken(&sc, t, JST_NUMBER, "0.2");
        t = checkToken(&sc, t, JST_ARRAY_CLOSE, "]");
        t = checkToken(&sc, t, JST_NUMBER, "-0");
        t = checkToken(&sc, t, JST_OBJECT_CLOSE, "}");
        t = checkToken(&sc, t, JST_NUMBER, "0.1");
        t = checkToken(&sc, t, JST_NUMBER, "-3.14e1");
        t = checkToken(&sc, t, JST_NUMBER, "0E5");
        t = checkToken(&sc, t, JST_NUMBER, "4e+2");
        t = checkToken(&sc, t, JST_NUMBER, "3E-4");
        t = checkToken(&sc, t, JST_EOF, "");
    }

    // evil
    {
        const uint8_t *data = "0..2";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ERR_UNEXPECTED, ".");
    }

    {
        const uint8_t *data = "0.";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ERR_INCOMPLETE, "0.");
    }

    {
        const uint8_t *data = "01";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ERR_UNEXPECTED, "1");
    }

    {
        const uint8_t *data = "1.";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ERR_INCOMPLETE, "1.");
    }

    {
        const uint8_t *data = "e01";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ERR_UNEXPECTED, "e");
    }

    {
        const uint8_t *data = "-e10";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ERR_UNEXPECTED, "e");
    }

    {
        const uint8_t *data = "+2";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ERR_UNEXPECTED, "+");
    }

    {
        const uint8_t *data = "1e";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ERR_INCOMPLETE, "1e");
    }

    {
        const uint8_t *data = "1.0e+e0";
        JsonToken t = json_scanner_init(&sc, data, strlen(data));
        t = checkToken(&sc, t, JST_ERR_UNEXPECTED, "e");
    }

    fprintf(stdout, "[ OK ] Numbers\n");
}

static void testSymbols(void) {
    JsonScanner sc;
    const uint8_t *data = "  true,false]  null}null";
    JsonToken t = json_scanner_init(&sc, data, strlen(data));

    t = checkToken(&sc, t, JST_TRUE, "true");
    t = checkToken(&sc, t, JST_COMMA, ",");
    t = checkToken(&sc, t, JST_FALSE, "false");
    t = checkToken(&sc, t, JST_ARRAY_CLOSE, "]");
    t = checkToken(&sc, t, JST_NULL, "null");
    t = checkToken(&sc, t, JST_OBJECT_CLOSE, "}");
    t = checkToken(&sc, t, JST_NULL, "null");

    fprintf(stdout, "[ OK ] Symbols\n");
}

int main(int argc, const char *argv[]) {
    testComposites();
    testStrings();
    testNumbers();
    testSymbols();

    return 0;
}
