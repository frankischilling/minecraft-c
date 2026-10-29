#ifndef MC_TEST_CZ_FILES_H
#define MC_TEST_CZ_FILES_H
#include "mc/world/storage/java_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
static inline unsigned long test_pid(void) {
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static inline bool test_name(const char *text, mc_world_info_name **out) {
    size_t n = strlen(text);
    uint16_t *units = malloc((n == 0 ? 1u : n) * sizeof(*units));
    if (units == NULL)
        return false;
    for (size_t i = 0; i < n; ++i) {
        if ((unsigned char)text[i] > 127u) {
            free(units);
            return false;
        }
        units[i] = (unsigned char)text[i];
    }
    mc_world_info_status s = mc_world_info_name_copy((mc_nbt_string){units, n}, SIZE_MAX, out);
    free(units);
    return s == MC_WORLD_INFO_OK;
}

static inline bool test_child(mc_java_files *files, mc_java_file *parent, const char *text,
                              mc_java_file **out) {
    mc_world_info_name *name = NULL;
    mc_java_file_error e = {0};
    if (!test_name(text, &name))
        return false;
    mc_java_file *f = NULL;
    bool ok = mc_java_file_allocate(files, &f, &e) == MC_JAVA_FILE_OK &&
              mc_java_file_construct_child(f, parent, name, &e) == MC_JAVA_FILE_OK;
    mc_world_info_name_release(name);
    if (ok)
        *out = f;
    else
        (void)mc_java_file_release(&f, &e);
    return ok;
}

static inline bool test_directory(const char *parent, mc_java_files **files, mc_java_file **root) {
    mc_java_file_error e = {0};
    mc_world_info_name *path = NULL;
    if (mc_java_files_create(NULL, files, &e) != MC_JAVA_FILE_OK || !test_name(parent, &path))
        return false;
    mc_java_file *base = NULL;
    bool ok = mc_java_file_allocate(*files, &base, &e) == MC_JAVA_FILE_OK &&
              mc_java_file_construct_string(base, path, &e) == MC_JAVA_FILE_OK;
    mc_world_info_name_release(path);
    char suffix[64];
    int n = snprintf(suffix, sizeof(suffix), "cz-%lu", test_pid());
    if (ok && n > 0 && (size_t)n < sizeof(suffix))
        ok = test_child(*files, base, suffix, root);
    else
        ok = false;
    (void)mc_java_file_release(&base, &e);
    if (!ok)
        return false;
    bool exists = false, made = false;
    return mc_java_file_exists(*root, &exists, &e) == MC_JAVA_FILE_OK && !exists &&
           mc_java_file_mkdirs(*root, &made, &e) == MC_JAVA_FILE_OK && made;
}

static inline bool test_write(mc_java_files *files, mc_java_file *file, const uint8_t *bytes,
                              size_t length) {
    mc_java_file_output *out = NULL;
    mc_java_output_error e = {0};
    bool ok = mc_java_file_output_allocate(files, NULL, &out, &e) == MC_JAVA_OUTPUT_OK &&
              mc_java_file_output_construct(out, file, &e) == MC_JAVA_OUTPUT_OK &&
              mc_java_file_output_write_span(out, bytes, length, &e) == MC_JAVA_OUTPUT_OK &&
              mc_java_file_output_source_close(out, &e) == MC_JAVA_OUTPUT_OK;
    return mc_java_file_output_release(&out, &e) == MC_JAVA_OUTPUT_OK && ok;
}
#endif
