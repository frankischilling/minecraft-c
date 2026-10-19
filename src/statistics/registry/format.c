#include "../../block/registry/registry_internal.h"
#include "../../platform/text/locale.h"
#include "registry_internal.h"

#include <stdint.h>

static mc_registry_status infinity_string(mc_registry_classes *classes,
                                          const mc_registry_string **out)
{
    mc_registry_string *string = mc_registry_allocate(classes, sizeof(*string));
    if (string == NULL)
        return classes->last_error.status;
    uint16_t *unit = mc_registry_allocate(classes, sizeof(*unit));
    if (unit == NULL)
        return classes->last_error.status;
    *unit = UINT16_C(0x221e);
    *string = (mc_registry_string){.units = unit, .length = 1};
    *out = string;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_number_format_create(mc_registry_classes *classes, bool integer_us,
                                                    mc_statistic_number_format **out)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (out == NULL || classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Number formatter requires admitted initialization", NULL);

    mc_statistic_number_locale selected = MC_STATISTIC_NUMBER_LOCALE_EN_US;
    if (!integer_us) {
        mc_text_number_locale locale = MC_TEXT_NUMBER_LOCALE_UNKNOWN;
        const mc_text_locale_status status = mc_text_number_locale_query(&locale);
        if (status == MC_TEXT_LOCALE_ALLOCATION_FAILED)
            return mc_registry_fail(classes, MC_REGISTRY_ALLOCATION_FAILED,
                                    "native format locale allocation", NULL);
        if (status != MC_TEXT_LOCALE_OK)
            return mc_registry_fail(classes, MC_REGISTRY_IO_FAILED, "native format locale query",
                                    NULL);
        if (locale == MC_TEXT_NUMBER_LOCALE_EN_US)
            selected = MC_STATISTIC_NUMBER_LOCALE_EN_US;
        else if (locale == MC_TEXT_NUMBER_LOCALE_EN)
            selected = MC_STATISTIC_NUMBER_LOCALE_EN;
        else
            return mc_registry_fail(classes, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                                    "Number formatter locale profile unfinished", NULL);
    }

    mc_statistic_number_format *format = mc_registry_allocate(classes, sizeof(*format));
    if (format == NULL)
        return classes->last_error.status;
    format->classes = classes;
    mc_statistic_number_format_fields *fields = &format->fields;
    /* These are the reached getIntegerInstance(Locale.US) and
     * DecimalFormat("########0.00") configurations. Native ownership does not
     * reproduce JDK reflection, caches, or temporary getter-result identities.
     * Runtime number conversion is a separate, unfinished operation. */
    *fields = (mc_statistic_number_format_fields){.locale = selected,
                                                  .grouping_used = integer_us,
                                                  .parse_integer_only = integer_us,
                                                  .maximum_integer_digits = INT32_MAX,
                                                  .minimum_integer_digits = 1,
                                                  .maximum_fraction_digits = integer_us ? 0 : 2,
                                                  .minimum_fraction_digits = integer_us ? 0 : 2,
                                                  .grouping_size = integer_us ? 3 : 0,
                                                  .multiplier = 1,
                                                  .half_even = true,
                                                  .decimal_separator_always_shown = false,
                                                  .parse_big_decimal = false,
                                                  .zero_digit = UINT16_C(0x0030),
                                                  .decimal_separator = UINT16_C(0x002e),
                                                  .grouping_separator = UINT16_C(0x002c),
                                                  .minus_sign = UINT16_C(0x002d)};
    mc_registry_status status =
        mc_registry_literal_ascii(classes, integer_us ? "#,##0" : "#0.00", &fields->pattern);
    if (status != MC_REGISTRY_OK)
        return status;
    status = mc_registry_literal_ascii(classes, "", &fields->positive_prefix);
    if (status != MC_REGISTRY_OK)
        return status;
    fields->positive_suffix = fields->positive_prefix;
    fields->negative_suffix = fields->positive_prefix;
    status = mc_registry_literal_ascii(classes, "-", &fields->negative_prefix);
    if (status != MC_REGISTRY_OK)
        return status;
    status = infinity_string(classes, &fields->infinity);
    if (status != MC_REGISTRY_OK)
        return status;
    status = mc_registry_literal_ascii(classes, "NaN", &fields->nan);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = format;
    return MC_REGISTRY_OK;
}

const mc_statistic_number_format_fields *
mc_statistic_number_format_fields_get(const mc_statistic_number_format *format)
{
    return format == NULL ? NULL : &format->fields;
}
