#include "mc/core/java/writer.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <share.h>
#include <wchar.h>
#include <windows.h>
#endif

static unsigned int checks;
#define CHECK(test)                                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(test)) {                                                                             \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test);                             \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static FILE *read_ascii_file(const char *path) {
    FILE *file = NULL;
#ifdef _MSC_VER
    file = _fsopen(path, "rb", _SH_DENYNO);
#else
    file = fopen(path, "rb");
#endif
    return file;
}

/* Paths belong to the separate fresh-directory harness. Unicode content and
 * filenames are read/compared independently by that harness after this actual
 * native file adapter retires. No byte-output callback is substituted here. */
static int test_body(int argc, char **argv) {
    mc_java_writer *writer = NULL;
    FILE *readback = NULL;
    int code = EXIT_FAILURE;
    CHECK(argc == 5);
    mc_java_writer_profile profile = {MC_JAVA_READER_UTF8, MC_JAVA_WRITER_PRINT,
                                      MC_JAVA_WRITER_CRLF};
    CHECK(mc_java_file_writer_open(argv[1], &profile, &writer, NULL) == MC_JAVA_WRITER_OK);
    mc_java_writer_state state = {0};
    CHECK(mc_java_writer_read_state(writer, &state) && state.native_file &&
          !state.source_close_reached);
    readback = read_ascii_file(argv[1]);
    CHECK(readback != NULL && fgetc(readback) == EOF && !ferror(readback));
    FILE *closing = readback;
    readback = NULL;
    CHECK(fclose(closing) == 0);
    const uint16_t first[] = {'s', 'k', 'i', 'n', ':', 0x20ac, 0xd83d, 0xde00, 0xd800};
    const mc_java_properties_string_view text = {first, sizeof(first) / sizeof(first[0])};
    CHECK(mc_java_writer_println(writer, &text, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_flush(writer, NULL) == MC_JAVA_WRITER_OK);
    readback = read_ascii_file(argv[1]);
    CHECK(readback != NULL && fgetc(readback) == 's' && !ferror(readback));
    closing = readback;
    readback = NULL;
    CHECK(fclose(closing) == 0);
    CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK);
    mc_java_writer_retirement retired = {0};
    CHECK(mc_java_writer_retire(&writer, &retired, NULL) == MC_JAVA_WRITER_OK &&
          retired.source_close_reached && !retired.additional_sink_close);
    profile.charset = MC_JAVA_READER_WINDOWS1252;
    profile.lines = MC_JAVA_WRITER_LF;
    CHECK(mc_java_file_writer_open(argv[2], &profile, &writer, NULL) == MC_JAVA_WRITER_OK);
    const uint16_t second[] = {'k', 'e', 'y', ':', 0xe9, 0x20ac, 0x81, 0xd83d, 0xde80};
    const mc_java_properties_string_view other = {second, sizeof(second) / sizeof(second[0])};
    CHECK(mc_java_writer_println(writer, &other, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_retire(&writer, NULL, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_file_writer_open(argv[3], &profile, &writer, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_write(writer, first, sizeof(first) / sizeof(first[0]), NULL) ==
          MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_retire(&writer, &retired, NULL) == MC_JAVA_WRITER_OK &&
          !retired.source_close_reached && retired.additional_sink_close);
    mc_java_writer_error error = {0};
    CHECK(mc_java_file_writer_open(argv[4], &profile, &writer, &error) == MC_JAVA_WRITER_IO_ERROR &&
          writer == NULL && error.status == MC_JAVA_WRITER_IO_ERROR && error.operation != NULL);
    code = EXIT_SUCCESS;
cleanup:
    if (readback != NULL && fclose(readback) != 0)
        code = EXIT_FAILURE;
    if (mc_java_writer_retire(&writer, NULL, NULL) != MC_JAVA_WRITER_OK)
        code = EXIT_FAILURE;
    if (code == EXIT_SUCCESS)
        printf("PASS: %u real FileWriter interface checks\n", checks);
    return code;
}

#ifdef _WIN32
/* A narrow CRT argv uses the active code page, not the adapter's UTF8 path
 * contract. Preserve actual native Unicode arguments through checked explicit
 * conversion; neither the process ACP nor a manifest is changed. */
int wmain(int argc, wchar_t **argv) {
    if (argc != 5)
        return test_body(argc, NULL);
    char *utf8[5] = {0};
    int code = EXIT_FAILURE;
    for (size_t i = 0; i < 5u; ++i) {
        size_t units = 0;
        while (units <= 32768u && argv[i][units] != L'\0')
            ++units;
        if (units > 32768u) {
            fputs("FileWriter test argument exceeds UTF16 bound\n", stderr);
            goto cleanup;
        }
        const int count = units == 0 ? 0
                                     : WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i],
                                                           (int)units, NULL, 0, NULL, NULL);
        if ((units != 0 && count == 0) || count < 0 || (size_t)count > 4u * 32768u) {
            fputs("FileWriter test UTF16 argument conversion failed\n", stderr);
            goto cleanup;
        }
        utf8[i] = malloc((size_t)count + 1u);
        if (utf8[i] == NULL) {
            fputs("FileWriter test UTF8 argument allocation failed\n", stderr);
            goto cleanup;
        }
        if (count != 0 && WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], (int)units,
                                              utf8[i], count, NULL, NULL) != count) {
            fputs("FileWriter test UTF8 argument copy failed\n", stderr);
            goto cleanup;
        }
        utf8[i][count] = '\0';
    }
    code = test_body(argc, utf8);
cleanup:
    for (size_t i = 0; i < 5u; ++i)
        free(utf8[i]);
    return code;
}
#else
int main(int argc, char **argv) {
    return test_body(argc, argv);
}
#endif
