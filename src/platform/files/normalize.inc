/* Intentionally included twice with type/function macros. One canonical
 * lexical body, preserved from the qualified GameDirectory implementation. */
static size_t MC_FILE_NORMALIZE(const MC_FILE_UNIT *input, size_t length, MC_FILE_UNIT *path) {
    size_t read_at = 0, used = 0;
#ifdef _WIN32
    size_t leading = 0;
    while (leading < length && path_separator(MC_FILE_CODE_UNIT(input[leading])))
        ++leading;
    if (length - leading >= 2u && drive_letter(MC_FILE_CODE_UNIT(input[leading])) && input[leading + 1u] == ':') {
        path[used++] = input[leading];
        path[used++] = ':';
        read_at = leading + 2u;
    } else if (leading >= 2u) {
        path[used++] = '\\';
        path[used++] = '\\';
        read_at = leading;
    }
    const MC_FILE_UNIT separator = '\\';
#else
    const MC_FILE_UNIT separator = '/';
#endif
    while (read_at < length) {
        if (!path_separator(MC_FILE_CODE_UNIT(input[read_at]))) {
            path[used++] = input[read_at++];
            continue;
        }
        while (read_at < length && path_separator(MC_FILE_CODE_UNIT(input[read_at])))
            ++read_at;
        if (read_at != length || used == 0u
#ifdef _WIN32
            || (used == 2u && path[1] == ':')
#endif
        )
            path[used++] = separator;
    }
    path[used] = 0;
    return used;
}
