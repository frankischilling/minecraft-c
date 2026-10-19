#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "locale.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wchar.h>
#include <windows.h>
#else
#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#endif

mc_text_locale_status mc_text_number_locale_query(mc_text_number_locale *out)
{
    if (out == NULL)
        return MC_TEXT_LOCALE_QUERY_FAILED;
    mc_text_number_locale selected = MC_TEXT_NUMBER_LOCALE_UNKNOWN;
#ifdef _WIN32
    const LCID locale = GetUserDefaultLCID();
    WCHAR language[9], country[4];
    if (GetLocaleInfoW(locale, LOCALE_SISO639LANGNAME, language, 9) == 0 ||
        GetLocaleInfoW(locale, LOCALE_SISO3166CTRYNAME, country, 4) == 0)
        return MC_TEXT_LOCALE_QUERY_FAILED;
    if (wcscmp(language, L"en") == 0) {
        if (wcscmp(country, L"US") == 0)
            selected = MC_TEXT_NUMBER_LOCALE_EN_US;
        else if (country[0] == L'\0')
            selected = MC_TEXT_NUMBER_LOCALE_EN;
    }
#else
    errno = 0;
    locale_t locale = newlocale(LC_ALL_MASK, "", (locale_t)0);
    if (locale == (locale_t)0 && errno != ENOMEM) {
        /* A fresh standalone JVM ignores failed setlocale(LC_ALL, "") and
         * retains its startup C locale. Model that fallback privately; an
         * embedding host's preexisting global locale is outside this query.
         * glibc can cache a failed lookup without setting errno on repeats. */
        errno = 0;
        locale = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    }
    if (locale == (locale_t)0)
        return errno == ENOMEM ? MC_TEXT_LOCALE_ALLOCATION_FAILED : MC_TEXT_LOCALE_QUERY_FAILED;
    const char *name = nl_langinfo_l(_NL_LOCALE_NAME(LC_CTYPE), locale);
    if (name == NULL) {
        freelocale(locale);
        return MC_TEXT_LOCALE_QUERY_FAILED;
    }
    /* OpenJDK's Linux ParseLocale maps bare C/POSIX to en_US before splitting.
     * C.UTF-8 instead reaches language_names[C]=en with no country. Bare en
     * is the locale_aliases entry for en_US. Encoding does not change symbols. */
    if (strcmp(name, "C") == 0 || strcmp(name, "POSIX") == 0)
        selected = MC_TEXT_NUMBER_LOCALE_EN_US;
    else {
        const size_t base = strcspn(name, ".@");
        const bool variant = strchr(name, '@') != NULL;
        if (!variant && ((base == 5 && memcmp(name, "en_US", 5) == 0) ||
                         (base == 2 && memcmp(name, "en", 2) == 0)))
            selected = MC_TEXT_NUMBER_LOCALE_EN_US;
        else if (!variant &&
                 ((base == 1 && name[0] == 'C') || (base == 5 && memcmp(name, "POSIX", 5) == 0)))
            selected = MC_TEXT_NUMBER_LOCALE_EN;
    }
    freelocale(locale);
#endif
    *out = selected;
    return MC_TEXT_LOCALE_OK;
}
