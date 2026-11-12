#ifndef MC_PLATFORM_TEXT_LOCALE_H
#define MC_PLATFORM_TEXT_LOCALE_H

/* Profiles whose numeric symbols are represented by the statistics formatter.
 * UNKNOWN is an actual unsupported locale, never an English fallback. */
typedef enum mc_text_number_locale {
    MC_TEXT_NUMBER_LOCALE_UNKNOWN,
    MC_TEXT_NUMBER_LOCALE_EN_US,
    MC_TEXT_NUMBER_LOCALE_EN
} mc_text_number_locale;

typedef enum mc_text_locale_status {
    MC_TEXT_LOCALE_OK,
    MC_TEXT_LOCALE_ALLOCATION_FAILED,
    MC_TEXT_LOCALE_QUERY_FAILED
} mc_text_locale_status;

/* Query the native format category without changing process-global locale.
 * Java's Linux startup derives this category from LC_CTYPE, not LC_NUMERIC.
 * A successful unknown profile still reports UNKNOWN for the later consumer
 * to reject at its reached locale-dependent operation. */
mc_text_locale_status mc_text_number_locale_query(mc_text_number_locale *out);

#endif
