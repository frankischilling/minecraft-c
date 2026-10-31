#include "cz_test_files.h"
#include "mc/core/java/ascii_operand.h"
#include "mc/core/java/integer_parse.h"
#include "mc/core/java/url.h"
#include <limits.h>
static unsigned int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "cz leaves %u line%d: %s\n", checks, __LINE__, #x);                    \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool number(const char *s, mc_java_integer_parse_status expected, int64_t value) {
    uint16_t units[64];
    size_t n = strlen(s);
    if (n > 64u)
        return false;
    for (size_t i = 0; i < n; ++i)
        units[i] = (unsigned char)s[i];
    mc_java_integer_parse_text t = {units, n};
    int64_t actual = 17;
    return mc_java_long_parse(&t, &actual) == expected &&
           actual == (expected == MC_JAVA_INTEGER_PARSE_OK ? value : 17);
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 77;
    int result = 1;
    mc_java_files *files = NULL;
    mc_java_file *root = NULL, *file = NULL, *parent = NULL, *absolute = NULL;
    mc_java_file_output *fos = NULL;
    mc_java_data_output *dos = NULL, *null_dos = NULL;
    mc_java_file_stream *in = NULL;
    mc_java_uri *uri = NULL;
    mc_java_url *url = NULL;
    mc_java_output_error oe = {0};
    mc_java_file_error fe = {0};
    mc_java_url_error ue = {0};
    CHECK(number("9223372036854775807", MC_JAVA_INTEGER_PARSE_OK, INT64_MAX));
    CHECK(number("-9223372036854775808", MC_JAVA_INTEGER_PARSE_OK, INT64_MIN));
    CHECK(number("9223372036854775808", MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT, 0));
    CHECK(number("-9223372036854775809", MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT, 0));
    CHECK(number(" 1", MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT, 0));
    CHECK(number("+", MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT, 0));
    const uint16_t arabic[] = {0x0661, 0x0662};
    int64_t parsed = 0;
    mc_java_integer_parse_text text = {arabic, 2};
    CHECK(mc_java_long_parse(&text, &parsed) == MC_JAVA_INTEGER_PARSE_OK && parsed == 12);
    const uint16_t long_s[] = {UINT16_C(0x017f), 'o', 'u', 'n', 'd'};
    CHECK(mc_java_ascii_operand_equal_ignore_case(long_s, 5, "sound"));
    const uint16_t dotted[] = {'m', 'u', 's', 0x0130, 'c'};
    CHECK(mc_java_ascii_operand_equal_ignore_case(dotted, 5, "music"));
    CHECK(test_directory(argv[1], &files, &root));
    CHECK(test_child(files, root, "a space.bin", &file));
    CHECK(mc_java_file_get_parent_file(file, &parent, &fe) == MC_JAVA_FILE_OK && parent != root);
    CHECK(mc_java_file_get_absolute_file(file, &absolute, &fe) == MC_JAVA_FILE_OK &&
          absolute != file && mc_java_file_path(absolute) == mc_java_file_path(file));
    CHECK(mc_java_file_output_allocate(files, NULL, &fos, &oe) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_file_output_construct(fos, file, &oe) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_data_output_allocate(NULL, &dos, &oe) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_data_output_construct(dos, fos, &oe) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_data_output_allocate(NULL, &null_dos, &oe) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_data_output_construct(null_dos, NULL, &oe) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_data_output_write_array(null_dos, NULL, -1, -1, &oe) ==
              MC_JAVA_OUTPUT_SOURCE_NULL &&
          oe.cause.site.pc == 7);
    const uint8_t bytes[] = {1, 2, 3, 4};
    mc_java_byte_array_view array = {bytes, 4};
    CHECK(mc_java_data_output_write_array(dos, &array, 1, 3, &oe) == MC_JAVA_OUTPUT_OK);
    mc_java_data_output_state ds;
    mc_java_file_output_state fs;
    CHECK(mc_java_data_output_inspect(dos, &ds) && ds.written == 3);
    CHECK(mc_java_data_output_write_array(dos, &array, INT32_MAX, 2, &oe) ==
          MC_JAVA_OUTPUT_SOURCE_BOUNDS);
    CHECK(mc_java_data_output_inspect(dos, &ds) && ds.written == 3);
    CHECK(mc_java_data_output_write_array(dos, NULL, 0, 0, &oe) == MC_JAVA_OUTPUT_SOURCE_NULL);
    CHECK(mc_java_data_output_source_close(dos, &oe) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_file_output_inspect(fos, &fs) && fs.source_close_succeeded && !fs.native_open &&
          fs.native_close_calls == 1);
    CHECK(mc_java_data_output_write_array(dos, &array, 4, 0, &oe) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_data_output_write_array(dos, &array, 0, 1, &oe) == MC_JAVA_OUTPUT_SOURCE_IO);
    CHECK(mc_java_data_output_inspect(dos, &ds) && ds.written == 3);
    CHECK(mc_java_file_stream_allocate(files, &in, &fe) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_stream_construct(in, file, &fe) == MC_JAVA_FILE_OK);
    uint8_t actual[4] = {0};
    mc_java_file_read intake = {0};
    CHECK(mc_java_file_stream_read(in, actual, 4, &intake, &fe) == MC_JAVA_FILE_OK &&
          intake.count == 3 && actual[0] == 2 && actual[1] == 3 && actual[2] == 4);
    CHECK(mc_java_file_stream_source_close(in, &fe) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_to_uri(file, &uri, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_java_uri_to_url(uri, &url, &ue) == MC_JAVA_URL_OK);
    mc_java_url_view view;
    CHECK(mc_java_url_inspect(url, &view) && view.file && view.native_file != NULL);
    bool escaped = false;
    for (size_t i = 0; i + 2u < view.file_url_length; ++i)
        if (view.file_url_path[i] == '%' && view.file_url_path[i + 1u] == '2' &&
            view.file_url_path[i + 2u] == '0')
            escaped = true;
    CHECK(escaped);
    result = 0;
cleanup:
    (void)mc_java_url_release(&url, &ue);
    (void)mc_java_uri_release(&uri, &ue);
    (void)mc_java_file_stream_release(&in, &fe);
    (void)mc_java_data_output_release(&null_dos, &oe);
    (void)mc_java_data_output_release(&dos, &oe);
    (void)mc_java_file_output_release(&fos, &oe);
    (void)mc_java_file_release(&absolute, &fe);
    (void)mc_java_file_release(&parent, &fe);
    (void)mc_java_file_release(&file, &fe);
    (void)mc_java_file_release(&root, &fe);
    (void)mc_java_files_release(&files, &fe);
    if (result == 0)
        printf("download leaves: %u checks\n", checks);
    return result;
}
