# json-scanner
This **BSD licensed**, **C library** tokenizes a **JSON** byte buffer. **No copying** is done as JsonTokens contain pointers to the underlying buffer, as well as the type of token. Drop straight into project. **C99**.

### Example
    const uint8_t *data = "[ 1, true, 3 ]";
    size_t data_len = strlen(data);
    JsonScanner sc;
    for (JsonToken t = json_scanner_init(&sc, data, data_len);
         t.type != JST_EOF && !json_scanner_error(t);
         t = json_scanner_next(&sc, t)) {

        fwrite(t.start, sizeof(uint8_t), t.len, stdout);
    }
    // no free necessary