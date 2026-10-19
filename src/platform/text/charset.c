#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "mc/core/java/reader.h"

#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#endif

mc_java_reader_status mc_java_reader_default_charset(mc_java_reader_charset *out)
{
    if (out == NULL)
        return MC_JAVA_READER_INVALID_ARGUMENT;
    mc_java_reader_charset selected;
#ifdef _WIN32
    const UINT codepage = GetACP();
    if (codepage == 65001u)
        selected = MC_JAVA_READER_UTF8;
    else if (codepage == 1252u)
        selected = MC_JAVA_READER_WINDOWS1252;
    else if (codepage == 20127u)
        selected = MC_JAVA_READER_US_ASCII;
    else if (codepage == 28591u)
        selected = MC_JAVA_READER_ISO8859_1;
    else
        return MC_JAVA_READER_UNSUPPORTED_CHARSET;
#else
    /* Resolve the environment's native character locale without changing the
     * process-global C locale used by other subsystems. */
    errno = 0;
    locale_t locale = newlocale(LC_CTYPE_MASK, "", (locale_t)0);
    if (locale == (locale_t)0)
        return errno == ENOMEM ? MC_JAVA_READER_ALLOCATION_FAILED
                               : MC_JAVA_READER_UNSUPPORTED_CHARSET;
    const char *name = nl_langinfo_l(CODESET, locale);
    bool supported = true;
    if (strcmp(name, "UTF-8") == 0 || strcmp(name, "UTF8") == 0 || strcmp(name, "utf8") == 0 ||
        strcmp(name, "utf-8") == 0)
        selected = MC_JAVA_READER_UTF8;
    else if (strcmp(name, "ANSI_X3.4-1968") == 0 || strcmp(name, "US-ASCII") == 0 ||
             strcmp(name, "ASCII") == 0 || strcmp(name, "646") == 0)
        selected = MC_JAVA_READER_US_ASCII;
    else if (strcmp(name, "ISO-8859-1") == 0 || strcmp(name, "ISO8859-1") == 0 ||
             strcmp(name, "ISO_8859-1") == 0)
        selected = MC_JAVA_READER_ISO8859_1;
    else if (strcmp(name, "CP1252") == 0 || strcmp(name, "WINDOWS-1252") == 0 ||
             strcmp(name, "windows-1252") == 0)
        selected = MC_JAVA_READER_WINDOWS1252;
    else
        supported = false;
    freelocale(locale);
    if (!supported)
        return MC_JAVA_READER_UNSUPPORTED_CHARSET;
#endif
    *out = selected;
    return MC_JAVA_READER_OK;
}
