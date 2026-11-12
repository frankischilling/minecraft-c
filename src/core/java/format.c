#include "format_internal.h"

#include <limits.h>

enum {
    F_LEFT = 1u << 0,
    F_UPPER = 1u << 1,
    F_ALTERNATE = 1u << 2,
    F_PLUS = 1u << 3,
    F_SPACE = 1u << 4,
    F_ZERO = 1u << 5,
    F_GROUP = 1u << 6,
    F_PARENTHESES = 1u << 7,
    F_PREVIOUS = 1u << 8
};

typedef struct format_part {
    struct format_part *next;
    size_t offset, length;
    int32_t index, width, precision;
    uint32_t flags;
    uint16_t conversion;
    bool date_time, fixed;
} format_part;

typedef struct format_context {
    const mc_java_format_options *options;
    const mc_java_format_text *pattern;
    mc_java_format_error *error;
    mc_java_format_status status;
    size_t limit, length, capacity;
    uint16_t *units;
} format_context;

static mc_java_format_status failure(format_context *context, mc_java_format_status status,
                                     const format_part *part, int32_t value, uint32_t flags) {
    context->status = status;
    if (context->error != NULL)
        *context->error = (mc_java_format_error){.status = status,
                                                 .offset = part == NULL ? 0 : part->offset,
                                                 .length = part == NULL ? 0 : part->length,
                                                 .value = value,
                                                 .flags = flags,
                                                 .conversion = part == NULL ? 0 : part->conversion,
                                                 .date_time = part != NULL && part->date_time};
    return status;
}

static void *allocate(format_context *context, size_t bytes, const format_part *part) {
    void *storage =
        context->options->allocator.allocate(context->options->allocator.context, bytes);
    if (storage == NULL)
        (void)failure(context, MC_JAVA_FORMAT_ALLOCATION_FAILED, part, 0,
                      part == NULL ? 0 : part->flags);
    return storage;
}

static bool digit(uint16_t unit) {
    /* Formatter's regex uses the default ASCII \d class. */
    return unit >= '0' && unit <= '9';
}

static bool letter_or_percent(uint16_t unit) {
    return (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z') || unit == '%';
}

static uint32_t flag(uint16_t unit) {
    switch (unit) {
        case '-':
            return F_LEFT;
        case '#':
            return F_ALTERNATE;
        case '+':
            return F_PLUS;
        case ' ':
            return F_SPACE;
        case '0':
            return F_ZERO;
        case ',':
            return F_GROUP;
        case '(':
            return F_PARENTHESES;
        case '<':
            return F_PREVIOUS;
        default:
            return 0;
    }
}

static bool valid_conversion(uint16_t unit) {
    switch (unit) {
        case 'b':
        case 'B':
        case 's':
        case 'S':
        case 'h':
        case 'H':
        case 'c':
        case 'C':
        case 'd':
        case 'o':
        case 'x':
        case 'X':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'f':
        case 'a':
        case 'A':
        case 'n':
        case '%':
            return true;
        default:
            return false;
    }
}

static bool valid_date(uint16_t unit) {
    switch (unit) {
        case 'H':
        case 'I':
        case 'k':
        case 'l':
        case 'M':
        case 'N':
        case 'L':
        case 'Q':
        case 'p':
        case 's':
        case 'S':
        case 'T':
        case 'z':
        case 'Z':
        case 'a':
        case 'A':
        case 'b':
        case 'B':
        case 'C':
        case 'd':
        case 'e':
        case 'h':
        case 'j':
        case 'm':
        case 'y':
        case 'Y':
        case 'r':
        case 'R':
        case 'c':
        case 'D':
        case 'F':
            return true;
        default:
            return false;
    }
}

static bool decimal(const uint16_t *units, size_t start, size_t end, int32_t *out) {
    int32_t value = 0;
    for (size_t index = start; index < end; ++index) {
        const int32_t next = (int32_t)(units[index] - '0');
        if (value > (INT32_MAX - next) / 10)
            return false;
        value = value * 10 + next;
    }
    *out = value;
    return true;
}

static bool mismatch(format_context *context, const format_part *part, uint32_t flags) {
    (void)failure(context, MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, part, 0, flags);
    return false;
}

static bool bad_flags(format_context *context, const format_part *part, const uint32_t *bad,
                      size_t count) {
    for (size_t index = 0; index < count; ++index)
        if ((part->flags & bad[index]) != 0)
            return mismatch(context, part, bad[index]);
    return true;
}

static bool check(format_context *context, const format_part *part) {
    const uint32_t flags = part->flags;
    const uint16_t conversion = part->conversion;
    static const uint32_t general_bad[] = {F_PLUS, F_SPACE, F_ZERO, F_GROUP, F_PARENTHESES};
    static const uint32_t character_bad[] = {F_ALTERNATE, F_PLUS,  F_SPACE,
                                             F_ZERO,      F_GROUP, F_PARENTHESES};
    if (part->date_time || conversion == 'c') {
        if (part->precision != -1) {
            (void)failure(context, MC_JAVA_FORMAT_ILLEGAL_PRECISION, part, part->precision, flags);
            return false;
        }
        if (part->date_time && !valid_date(conversion)) {
            (void)failure(context, MC_JAVA_FORMAT_UNKNOWN_CONVERSION, part, 0, flags);
            return false;
        }
        if (!bad_flags(context, part, character_bad,
                       sizeof(character_bad) / sizeof(*character_bad)))
            return false;
        if (part->width == -1 && (flags & F_LEFT) != 0) {
            (void)failure(context, MC_JAVA_FORMAT_MISSING_WIDTH, part, 0, flags);
            return false;
        }
        return true;
    }
    if (conversion == 's' || conversion == 'b' || conversion == 'h') {
        if (conversion != 's' && (flags & F_ALTERNATE) != 0)
            return mismatch(context, part, F_ALTERNATE);
        if (part->width == -1 && (flags & F_LEFT) != 0) {
            (void)failure(context, MC_JAVA_FORMAT_MISSING_WIDTH, part, 0, flags);
            return false;
        }
        return bad_flags(context, part, general_bad, sizeof(general_bad) / sizeof(*general_bad));
    }
    if (conversion == '%' || conversion == 'n') {
        if (part->precision != -1) {
            (void)failure(context, MC_JAVA_FORMAT_ILLEGAL_PRECISION, part, part->precision, flags);
            return false;
        }
        if (conversion == 'n' && part->width != -1) {
            (void)failure(context, MC_JAVA_FORMAT_ILLEGAL_WIDTH, part, part->width, flags);
            return false;
        }
        if ((conversion == '%' && flags != 0 && flags != F_LEFT) ||
            (conversion == 'n' && flags != 0)) {
            (void)failure(context, MC_JAVA_FORMAT_ILLEGAL_FLAGS, part, 0, flags);
            return false;
        }
        if (part->width == -1 && (flags & F_LEFT) != 0) {
            (void)failure(context, MC_JAVA_FORMAT_MISSING_WIDTH, part, 0, flags);
            return false;
        }
        return true;
    }
    /* Numeric constructor checks precede integer precision/conversion checks. */
    if (part->width == -1 && (flags & (F_LEFT | F_ZERO)) != 0) {
        (void)failure(context, MC_JAVA_FORMAT_MISSING_WIDTH, part, 0, flags);
        return false;
    }
    if (((flags & F_PLUS) != 0 && (flags & F_SPACE) != 0) ||
        ((flags & F_LEFT) != 0 && (flags & F_ZERO) != 0)) {
        (void)failure(context, MC_JAVA_FORMAT_ILLEGAL_FLAGS, part, 0, flags);
        return false;
    }
    if (conversion == 'd' || conversion == 'o' || conversion == 'x') {
        if (part->precision != -1) {
            (void)failure(context, MC_JAVA_FORMAT_ILLEGAL_PRECISION, part, part->precision, flags);
            return false;
        }
        const uint32_t bad = conversion == 'd' ? F_ALTERNATE : F_GROUP;
        return (flags & bad) == 0 || mismatch(context, part, bad);
    }
    if (conversion == 'a') {
        static const uint32_t bad[] = {F_PARENTHESES, F_GROUP};
        return bad_flags(context, part, bad, sizeof(bad) / sizeof(*bad));
    }
    if (conversion == 'e' && (flags & F_GROUP) != 0)
        return mismatch(context, part, F_GROUP);
    if (conversion == 'g' && (flags & F_ALTERNATE) != 0)
        return mismatch(context, part, F_ALTERNATE);
    return true;
}

static void normalize_conversion(format_part *part) {
    if (part->conversion >= 'A' && part->conversion <= 'Z') {
        part->flags |= F_UPPER;
        part->conversion = (uint16_t)(part->conversion + ('a' - 'A'));
    }
    if (part->conversion == 'n' || part->conversion == '%')
        part->index = -2;
}

static bool specifier(format_context *context, size_t start, format_part *part) {
    const uint16_t *units = context->pattern->units;
    const size_t length = context->pattern->length;
    *part = (format_part){.offset = start, .width = -1, .precision = -1};
    size_t position = start + 1u;
    if (position == length) {
        part->length = 1;
        part->conversion = '%';
        (void)failure(context, MC_JAVA_FORMAT_UNKNOWN_CONVERSION, part, 0, 0);
        return false;
    }
    if (valid_conversion(units[position])) {
        part->conversion = units[position];
        part->length = 2;
        normalize_conversion(part);
        return true; /* Actual one-character constructor has no check calls. */
    }
    /* Match the complete regex shape before invoking any constructor stage.
     * Its default \d is ASCII; flags greedily consume leading zeroes. */
    size_t index_end = position;
    while (index_end < length && digit(units[index_end]))
        ++index_end;
    const bool explicit_index =
        index_end != position && index_end < length && units[index_end] == '$';
    if (explicit_index)
        position = index_end + 1u;
    const size_t flags_start = position;
    while (position < length && flag(units[position]) != 0)
        ++position;
    const size_t flags_end = position;
    const size_t width_start = position;
    while (position < length && digit(units[position]))
        ++position;
    const size_t width_end = position;
    size_t precision_start = position, precision_end = position;
    if (position + 1u < length && units[position] == '.' && digit(units[position + 1u])) {
        precision_start = ++position;
        while (position < length && digit(units[position]))
            ++position;
        precision_end = position;
    }
    /* The optional t/T group backtracks if no final conversion can follow it. */
    bool date_time = false, upper_date = false;
    if (position + 1u < length && (units[position] == 't' || units[position] == 'T') &&
        letter_or_percent(units[position + 1u])) {
        date_time = true;
        upper_date = units[position] == 'T';
        ++position;
    }
    if (position >= length || !letter_or_percent(units[position])) {
        part->length = 2;
        part->conversion = units[start + 1u];
        (void)failure(context, MC_JAVA_FORMAT_UNKNOWN_CONVERSION, part, 0, 0);
        return false;
    }
    part->length = position - start + 1u;
    if (explicit_index) {
        if (!decimal(units, start + 1u, index_end, &part->index)) {
            (void)failure(context, MC_JAVA_FORMAT_ILLEGAL_ARGUMENT_INDEX, part, INT32_MIN, 0);
            return false;
        }
        if (part->index <= 0) {
            (void)failure(context, MC_JAVA_FORMAT_ILLEGAL_ARGUMENT_INDEX, part, part->index, 0);
            return false;
        }
    }
    for (size_t index = flags_start; index < flags_end; ++index) {
        const uint32_t bit = flag(units[index]);
        if ((part->flags & bit) != 0) {
            (void)failure(context, MC_JAVA_FORMAT_DUPLICATE_FLAGS, part, 0, bit);
            return false;
        }
        part->flags |= bit;
    }
    if ((part->flags & F_PREVIOUS) != 0)
        part->index = -1;
    if (width_end != width_start && !decimal(units, width_start, width_end, &part->width)) {
        (void)failure(context, MC_JAVA_FORMAT_ILLEGAL_WIDTH, part, INT32_MIN, part->flags);
        return false;
    }
    if (precision_end != precision_start &&
        !decimal(units, precision_start, precision_end, &part->precision)) {
        (void)failure(context, MC_JAVA_FORMAT_ILLEGAL_PRECISION, part, INT32_MIN, part->flags);
        return false;
    }
    part->date_time = date_time;
    if (upper_date)
        part->flags |= F_UPPER;
    part->conversion = units[position];
    if (!date_time) {
        if (!valid_conversion(part->conversion)) {
            (void)failure(context, MC_JAVA_FORMAT_UNKNOWN_CONVERSION, part, 0, part->flags);
            return false;
        }
        normalize_conversion(part);
    }
    return check(context, part);
}

static bool parse(format_context *context, format_part **out) {
    format_part *head = NULL;
    format_part **tail = &head;
    size_t offset = 0;
    while (offset < context->pattern->length) {
        size_t next = offset;
        while (next < context->pattern->length && context->pattern->units[next] != '%')
            ++next;
        format_part part;
        if (next != offset) {
            part = (format_part){.offset = offset,
                                 .length = next - offset,
                                 .index = -2,
                                 .width = -1,
                                 .precision = -1,
                                 .fixed = true};
        } else if (!specifier(context, offset, &part)) {
            return false;
        }
        format_part *stored = allocate(context, sizeof(*stored), &part);
        if (stored == NULL)
            return false;
        *stored = part;
        *tail = stored;
        tail = &stored->next;
        offset += part.length;
    }
    *out = head;
    return true;
}

static bool reserve(format_context *context, size_t additional, const format_part *part) {
    if (additional > context->limit - context->length) {
        (void)failure(context, MC_JAVA_FORMAT_LIMIT, part, 0, part->flags);
        return false;
    }
    const size_t required = context->length + additional;
    if (required <= context->capacity)
        return true;
    size_t capacity = context->capacity == 0 ? 16u : context->capacity;
    if (capacity > context->limit)
        capacity = context->limit;
    while (capacity < required) {
        const size_t growth = capacity / 2u + 1u;
        capacity = growth > context->limit - capacity ? context->limit : capacity + growth;
    }
    if (capacity > SIZE_MAX / sizeof(uint16_t)) {
        (void)failure(context, MC_JAVA_FORMAT_LIMIT, part, 0, part->flags);
        return false;
    }
    uint16_t *units = allocate(context, capacity * sizeof(*units), part);
    if (units == NULL)
        return false;
    for (size_t index = 0; index < context->length; ++index)
        units[index] = context->units[index];
    context->units = units;
    context->capacity = capacity;
    return true;
}

static bool append(format_context *context, const mc_java_format_text *text,
                   const format_part *part) {
    if (!reserve(context, text->length, part))
        return false;
    for (size_t index = 0; index < text->length; ++index)
        context->units[context->length++] = text->units[index];
    return true;
}

static bool spaces(format_context *context, size_t count, const format_part *part) {
    if (!reserve(context, count, part))
        return false;
    for (size_t index = 0; index < count; ++index)
        context->units[context->length++] = ' ';
    return true;
}

static bool valid_text(format_context *context, const mc_java_format_text *text,
                       const format_part *part) {
    if (text->units == NULL && text->length != 0) {
        (void)failure(context, MC_JAVA_FORMAT_INVALID_ARGUMENT, part, 0, part->flags);
        return false;
    }
    if (text->length > (size_t)INT32_MAX) {
        (void)failure(context, MC_JAVA_FORMAT_LIMIT, part, 0, part->flags);
        return false;
    }
    return true;
}

static mc_java_format_text hash_text(const mc_java_format_text *argument, uint16_t units[8]) {
    uint32_t hash = 0;
    for (size_t index = 0; index < argument->length; ++index)
        hash = hash * UINT32_C(31) + argument->units[index];
    static const uint16_t digits[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    size_t start = 8;
    do {
        units[--start] = digits[hash & UINT32_C(15)];
        hash >>= 4;
    } while (hash != 0);
    return (mc_java_format_text){.units = units + start, .length = 8u - start};
}

static bool render(format_context *context, const format_part *part,
                   const mc_java_format_text *argument) {
    if (part->fixed) {
        const mc_java_format_text literal = {.units = context->pattern->units + part->offset,
                                             .length = part->length};
        return append(context, &literal, part);
    }
    if (!part->date_time && part->conversion == 'n') {
        if (!valid_text(context, &context->options->line_separator, part))
            return false;
        return append(context, &context->options->line_separator, part);
    }
    static const uint16_t null_units[4] = {'n', 'u', 'l', 'l'};
    static const uint16_t true_units[4] = {'t', 'r', 'u', 'e'};
    static const uint16_t false_units[5] = {'f', 'a', 'l', 's', 'e'};
    static const uint16_t percent[1] = {'%'};
    uint16_t hash_units[8];
    mc_java_format_text text = {.units = null_units, .length = 4};
    if (!part->date_time && part->conversion == '%') {
        text = (mc_java_format_text){.units = percent, .length = 1};
    } else {
        if (!part->date_time && part->conversion == 's' && (part->flags & F_ALTERNATE) != 0)
            return mismatch(context, part, F_ALTERNATE);
        if (argument != NULL && !valid_text(context, argument, part))
            return false;
        if (part->date_time ||
            (part->conversion != 's' && part->conversion != 'b' && part->conversion != 'h')) {
            if (argument != NULL) {
                (void)failure(context, MC_JAVA_FORMAT_ILLEGAL_CONVERSION, part, 0, part->flags);
                return false;
            }
        } else if (part->conversion == 's') {
            if (argument != NULL)
                text = *argument;
        } else if (part->conversion == 'b') {
            text = argument == NULL ? (mc_java_format_text){.units = false_units, .length = 5}
                                    : (mc_java_format_text){.units = true_units, .length = 4};
        } else if (argument != NULL) {
            text = hash_text(argument, hash_units);
        }
    }
    if (part->precision != -1 && (size_t)part->precision < text.length)
        text.length = (size_t)part->precision;
    if ((part->flags & F_UPPER) != 0) {
        mc_java_format_text upper;
        const mc_java_format_status status = mc_java_format_upper(context->options, &text, &upper);
        if (status != MC_JAVA_FORMAT_OK) {
            (void)failure(context, status, part, 0, part->flags);
            return false;
        }
        text = upper;
    }
    const size_t padding = part->width != -1 && (size_t)part->width > text.length
                               ? (size_t)part->width - text.length
                               : 0;
    if ((part->flags & F_LEFT) != 0)
        return append(context, &text, part) && spaces(context, padding, part);
    return spaces(context, padding, part) && append(context, &text, part);
}

mc_java_format_status mc_java_format_strings(const mc_java_format_options *options,
                                             const mc_java_format_text *pattern,
                                             const mc_java_format_text *const *arguments,
                                             size_t argument_count, mc_java_format_text *result,
                                             mc_java_format_error *error) {
    format_context context = {.options = options, .pattern = pattern, .error = error};
    if (options == NULL || result == NULL || options->allocator.allocate == NULL ||
        (arguments == NULL && argument_count != 0))
        return failure(&context, MC_JAVA_FORMAT_INVALID_ARGUMENT, NULL, 0, 0);
    if (pattern == NULL)
        return failure(&context, MC_JAVA_FORMAT_NULL_FORMAT, NULL, 0, 0);
    if (pattern->units == NULL && pattern->length != 0)
        return failure(&context, MC_JAVA_FORMAT_INVALID_ARGUMENT, NULL, 0, 0);
    if (pattern->length > (size_t)INT32_MAX || argument_count > (size_t)INT32_MAX)
        return failure(&context, MC_JAVA_FORMAT_LIMIT, NULL, 0, 0);
    context.limit = options->max_output_units == 0 || options->max_output_units > (size_t)INT32_MAX
                        ? (size_t)INT32_MAX
                        : options->max_output_units;
    format_part *parts = NULL;
    if (!parse(&context, &parts))
        return context.status;
    int32_t last = -1, ordinary = -1;
    for (const format_part *part = parts; part != NULL; part = part->next) {
        const mc_java_format_text *argument = NULL;
        if (part->index != -2) {
            if (part->index == 0) {
                ++ordinary;
                last = ordinary;
            } else if (part->index > 0) {
                last = part->index - 1;
            }
            if (last < 0 || (arguments != NULL && (size_t)last >= argument_count))
                return failure(&context, MC_JAVA_FORMAT_MISSING_ARGUMENT, part, last, part->flags);
            if (arguments != NULL)
                argument = arguments[last];
        }
        if (!render(&context, part, argument))
            return context.status;
    }
    *result = (mc_java_format_text){.units = context.units, .length = context.length};
    return failure(&context, MC_JAVA_FORMAT_OK, NULL, 0, 0);
}
