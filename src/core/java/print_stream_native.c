#include "print_stream_internal.h"

#include <limits.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

mc_java_writer_status mc_ps_native_stdout_write(void *context, const mc_text_stdout *stream,
                                                const uint8_t *bytes, size_t length,
                                                size_t *physical, bool *known,
                                                mc_java_writer_error *error) {
    (void)context;
    size_t actual = 0;
    bool io = false;
    int native_error = 0;
    const mc_text_stdout_status status =
        mc_text_stdout_write(stream, bytes, length, &actual, &io, &native_error);
    if (status != MC_TEXT_STDOUT_OK) {
        *known = false; /* The kernel deliberately preserves count on native failure. */
        *error = (mc_java_writer_error){
            .status = MC_JAVA_WRITER_INVALID_ARGUMENT,
            .operation = "stdout native control failure; physical progress unknown"};
        return error->status;
    }
    *known = true;
    *physical = actual;
    *error = (mc_java_writer_error){.status = io ? MC_JAVA_WRITER_IO_ERROR : MC_JAVA_WRITER_OK,
                                    .system_error = native_error,
                                    .operation = "FileOutputStream stdout native write"};
    return error->status;
}

#ifdef _WIN32
static bool charset_for_page(unsigned int page, mc_java_reader_charset *out) {
    switch (page) {
        case 65001:
            *out = MC_JAVA_READER_UTF8;
            return true;
        case 1252:
            *out = MC_JAVA_READER_WINDOWS1252;
            return true;
        case 28591:
            *out = MC_JAVA_READER_ISO8859_1;
            return true;
        case 20127:
            *out = MC_JAVA_READER_US_ASCII;
            return true;
        default:
            return false;
    }
}
#endif

mc_java_print_status mc_ps_native_profile(mc_java_print_profile *out, mc_java_print_error *error) {
    mc_java_print_profile p = {0};
#ifdef _WIN32
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    p.native_character_device = GetFileType(handle) == FILE_TYPE_CHAR;
    p.stdout_encoding_present = p.native_character_device;
    if (p.stdout_encoding_present) {
        p.stdout_code_page = GetConsoleCP(); /* JDK getConsoleEncoding, not OutputCP. */
        if (charset_for_page(p.stdout_code_page, &p.charset)) {
            *out = p;
            return MC_JAVA_PRINT_OK;
        }
        if (p.stdout_code_page != 0) {
            *out = p;
            return mc_ps_fail(error, MC_JAVA_PRINT_UNSUPPORTED_CHARSET,
                              "valid native sun.stdout.encoding has no implemented encoder");
        }
        /* Actual invalid cp0 name throws UnsupportedEncodingException and
         * newPrintStream's catch selects the default-charset constructor. */
    }
    p.fallback_default = true;
    WCHAR value[16] = {0};
    if (GetLocaleInfoW(GetUserDefaultLCID(), LOCALE_IDEFAULTANSICODEPAGE, value, 16) == 0) {
        *out = p;
        error->system_error = (int)GetLastError();
        return mc_ps_fail(error, MC_JAVA_PRINT_NATIVE_FAILURE, "native default locale ANSI page");
    }
    unsigned int page = 0;
    for (size_t i = 0; value[i] != 0; ++i) {
        if (value[i] < L'0' || value[i] > L'9' || page > (UINT_MAX - 9u) / 10u)
            return mc_ps_fail(error, MC_JAVA_PRINT_NATIVE_FAILURE, "invalid locale ANSI page");
        page = page * 10u + (unsigned int)(value[i] - L'0');
    }
    p.default_code_page = page;
    if (!charset_for_page(page == 0 ? 65001u : page, &p.charset)) {
        *out = p;
        return mc_ps_fail(error, MC_JAVA_PRINT_UNSUPPORTED_CHARSET,
                          "valid native defaultCharset has no implemented encoder");
    }
#else
    p.native_character_device = isatty(STDOUT_FILENO) != 0;
    p.stdout_encoding_present = p.native_character_device;
    p.fallback_default = !p.stdout_encoding_present;
    const mc_java_reader_status selected = mc_java_reader_default_charset(&p.charset);
    if (selected != MC_JAVA_READER_OK) {
        *out = p;
        return mc_ps_fail(error,
                          selected == MC_JAVA_READER_ALLOCATION_FAILED
                              ? MC_JAVA_PRINT_ALLOCATION_FAILED
                              : MC_JAVA_PRINT_UNSUPPORTED_CHARSET,
                          "native locale charset has no implemented encoder");
    }
#endif
    *out = p;
    return MC_JAVA_PRINT_OK;
}
