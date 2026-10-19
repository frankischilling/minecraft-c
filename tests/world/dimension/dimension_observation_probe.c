#include "mc/world/dimension/dimension.h"

#include "dimension_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#define PROBE_SLOT_LIMIT 8u
#define PROBE_REFERENCE_LIMIT 128u
#define PROBE_OPERATION_LIMIT 60000u
#define PROBE_LINE_LIMIT 160u
#define PROBE_OUTPUT_LIMIT (4u * 1024u * 1024u)

_Static_assert(sizeof(float) == sizeof(uint32_t), "binary32 float storage required");

typedef struct dimension_observation {
    mc_world_dimension *owners[PROBE_SLOT_LIMIT];
    bool assigned[PROBE_SLOT_LIMIT];
    const void *references[PROBE_REFERENCE_LIMIT];
    unsigned int reference_count, requests, rows;
    unsigned int factories, snapshots, tables, angles;
    size_t length;
    const char *failure;
    char output[PROBE_OUTPUT_LIMIT + 1u];
} dimension_observation;

typedef enum probe_line_status {
    PROBE_LINE_OK,
    PROBE_LINE_END,
    PROBE_LINE_INVALID,
    PROBE_LINE_IO_ERROR
} probe_line_status;

static bool fail(dimension_observation *observation, const char *message)
{
    if (observation->failure == NULL)
        observation->failure = message;
    return false;
}

static bool emit(dimension_observation *observation, const char *format, ...)
{
    const size_t remaining = sizeof(observation->output) - observation->length;
    va_list arguments;
    va_start(arguments, format);
    const int written =
        vsnprintf(observation->output + observation->length, remaining, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= remaining)
        return fail(observation, "output byte limit or formatting failure");
    for (size_t i = 0; i < (size_t)written; ++i) {
        if (observation->output[observation->length + i] == '\n') {
            if (observation->rows == PROBE_OPERATION_LIMIT)
                return fail(observation, "output row limit");
            ++observation->rows;
        }
    }
    observation->length += (size_t)written;
    return true;
}

static bool reference(dimension_observation *observation, const void *value, unsigned int *out)
{
    if (value == NULL) {
        *out = 0;
        return true;
    }
    for (unsigned int i = 0; i < observation->reference_count; ++i) {
        if (observation->references[i] == value) {
            *out = i + 1u;
            return true;
        }
    }
    if (observation->reference_count == PROBE_REFERENCE_LIMIT)
        return fail(observation, "reference limit");
    observation->references[observation->reference_count++] = value;
    *out = observation->reference_count;
    return true;
}

static uint32_t float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/* Literal LF terminators are required, including on the final END line. */
static probe_line_status read_line(char line[PROBE_LINE_LIMIT + 1u])
{
    size_t length = 0;
    for (;;) {
        const int character = fgetc(stdin);
        if (character == EOF) {
            if (ferror(stdin))
                return PROBE_LINE_IO_ERROR;
            return length == 0 ? PROBE_LINE_END : PROBE_LINE_INVALID;
        }
        if (character == '\n') {
            line[length] = '\0';
            return PROBE_LINE_OK;
        }
        if ((character != '\t' && (character < 0x20 || character > 0x7e)) ||
            length == PROBE_LINE_LIMIT)
            return PROBE_LINE_INVALID;
        line[length++] = (char)character;
    }
}

static bool split_fields(char *line, char *fields[4], size_t *out_count)
{
    size_t count = 1;
    fields[0] = line;
    for (char *cursor = line; *cursor != '\0'; ++cursor) {
        if (*cursor == '\t') {
            if (count == 4u)
                return false;
            *cursor = '\0';
            fields[count++] = cursor + 1;
        }
    }
    for (size_t i = 0; i < count; ++i) {
        if (fields[i][0] == '\0')
            return false;
    }
    *out_count = count;
    return true;
}

/* Canonical signed decimal only: no plus, leading zeros or negative zero. */
static bool parse_i64(const char *text, int64_t *out)
{
    const char *digits = text;
    if (*digits == '-')
        ++digits;
    if (*digits == '\0' || (*digits == '0' && (digits[1] != '\0' || digits != text)))
        return false;
    for (const char *cursor = digits; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9')
            return false;
    }
    errno = 0;
    char *end = NULL;
    const intmax_t value = strtoimax(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value < INT64_MIN || value > INT64_MAX)
        return false;
    *out = (int64_t)value;
    return true;
}

static bool parse_slot(const char *text, size_t *out)
{
    int64_t value = 0;
    if (!parse_i64(text, &value) || value < 0 || value >= (int64_t)PROBE_SLOT_LIMIT)
        return false;
    *out = (size_t)value;
    return true;
}

static bool parse_float_bits(const char *text, uint32_t *out)
{
    if (strlen(text) != 8u)
        return false;
    uint32_t value = 0;
    for (size_t i = 0; i < 8u; ++i) {
        unsigned int digit;
        if (text[i] >= '0' && text[i] <= '9')
            digit = (unsigned int)(text[i] - '0');
        else if (text[i] >= 'a' && text[i] <= 'f')
            digit = (unsigned int)(text[i] - 'a') + 10u;
        else
            return false;
        value = (value << 4u) | (uint32_t)digit;
    }
    if ((value & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000))
        return false;
    *out = value;
    return true;
}

static bool factory(dimension_observation *observation, size_t slot, int32_t requested)
{
    if (observation->assigned[slot])
        return fail(observation, "factory slot already assigned");
    mc_world_dimension_error error = {0};
    if (mc_world_dimension_create(requested, NULL, &observation->owners[slot], &error) !=
        MC_WORLD_DIMENSION_OK)
        return fail(observation, "native factory failed");
    observation->assigned[slot] = true;
    unsigned int owner = 0;
    if (!reference(observation, observation->owners[slot], &owner))
        return false;
    if (observation->owners[slot] == NULL) {
        if (!emit(observation, "FACTORY\t%zu\t%" PRId32 "\t%u\t-\n", slot, requested, owner))
            return false;
    } else {
        mc_world_dimension_state state = {0};
        if (mc_world_dimension_state_get(observation->owners[slot], &state) !=
            MC_WORLD_DIMENSION_OK)
            return fail(observation, "native factory state failed");
        if (!emit(observation, "FACTORY\t%zu\t%" PRId32 "\t%u\t%d\n", slot, requested, owner,
                  (int)state.kind))
            return false;
    }
    ++observation->factories;
    return true;
}

static bool array(dimension_observation *observation, size_t slot, const char *field,
                  const float *values, size_t length)
{
    unsigned int id = 0;
    if (!reference(observation, values, &id) ||
        !emit(observation, "ARRAY\t%zu\t%s\t%u\t", slot, field, id))
        return false;
    if (values == NULL)
        return emit(observation, "-1\t-\n");
    if (!emit(observation, "%zu\t", length))
        return false;
    for (size_t i = 0; i < length; ++i) {
        if (!emit(observation, "%08" PRIx32, float_bits(values[i])))
            return false;
    }
    return emit(observation, "\n");
}

static bool snapshot(dimension_observation *observation, size_t slot)
{
    mc_world_dimension_fields fields = {0};
    mc_world_dimension_state state = {0};
    if (mc_world_dimension_fields_get(observation->owners[slot], &fields) !=
            MC_WORLD_DIMENSION_OK ||
        mc_world_dimension_state_get(observation->owners[slot], &state) != MC_WORLD_DIMENSION_OK)
        return fail(observation, "native snapshot failed");
    unsigned int owner = 0, world = 0, biome = 0;
    if (!reference(observation, observation->owners[slot], &owner) ||
        !reference(observation, fields.a, &world) || !reference(observation, fields.b, &biome) ||
        !emit(observation, "STATE\t%zu\t%u\t%d\t%u\t%u\t%u\t%u\t%u\t%" PRId32 "\n", slot, owner,
              (int)state.kind, world, biome, fields.c ? 1u : 0u, fields.d ? 1u : 0u,
              fields.e ? 1u : 0u, fields.g))
        return false;
    /* The public constructor contract supplies fixed bounds for these actual
     * arrays; no dynamic length or detached array is manufactured here. */
    if (!array(observation, slot, "f", fields.f, 16u) ||
        !array(observation, slot, "h", fields.h, 4u))
        return false;
    ++observation->snapshots;
    return true;
}

static bool table(dimension_observation *observation, size_t slot)
{
    mc_world_dimension_error error = {0};
    if (mc_world_dimension_initialize_light_table(observation->owners[slot], &error) !=
        MC_WORLD_DIMENSION_OK)
        return fail(observation, "native table initialization failed");
    if (!emit(observation, "TABLE\t%zu\n", slot))
        return false;
    ++observation->tables;
    return true;
}

static bool angle(dimension_observation *observation, size_t slot, int64_t time,
                  uint32_t partial_bits)
{
    float partial;
    memcpy(&partial, &partial_bits, sizeof(partial));
    float value = 0.0f;
    mc_world_dimension_error error = {0};
    if (mc_world_dimension_celestial_angle(observation->owners[slot], time, partial, &value,
                                           &error) != MC_WORLD_DIMENSION_OK)
        return fail(observation, "native celestial angle failed");
    if (!emit(observation, "ANGLE\t%zu\t%" PRId64 "\t%08" PRIx32 "\t%08" PRIx32 "\n", slot, time,
              partial_bits, float_bits(value)))
        return false;
    ++observation->angles;
    return true;
}

static bool execute(dimension_observation *observation, char *fields[4], size_t count)
{
    size_t slot = 0;
    if (count < 2u || !parse_slot(fields[1], &slot))
        return fail(observation, "invalid command or slot");
    if (strcmp(fields[0], "FACTORY") == 0) {
        int64_t requested = 0;
        return count == 3u && parse_i64(fields[2], &requested) && requested >= INT32_MIN &&
                       requested <= INT32_MAX
                   ? factory(observation, slot, (int32_t)requested)
                   : fail(observation, "invalid factory fields");
    }
    if (!observation->assigned[slot] || observation->owners[slot] == NULL)
        return fail(observation, "command requires a nonnull factory owner");
    if (strcmp(fields[0], "SNAP") == 0)
        return count == 2u ? snapshot(observation, slot) : fail(observation, "invalid SNAP fields");
    if (strcmp(fields[0], "TABLE") == 0)
        return count == 2u ? table(observation, slot) : fail(observation, "invalid TABLE fields");
    if (strcmp(fields[0], "ANGLE") == 0) {
        int64_t time = 0;
        uint32_t partial_bits = 0;
        return count == 4u && parse_i64(fields[2], &time) &&
                       parse_float_bits(fields[3], &partial_bits)
                   ? angle(observation, slot, time, partial_bits)
                   : fail(observation, "invalid ANGLE fields");
    }
    return fail(observation, "unknown command");
}

static bool run(dimension_observation *observation)
{
    char line[PROBE_LINE_LIMIT + 1u];
    if (read_line(line) != PROBE_LINE_OK || strcmp(line, "DIMENSION_NATIVE_REQUEST_V1") != 0)
        return fail(observation, "missing exact request header");
    if (!emit(observation, "DIMENSION_NATIVE_OBSERVATION_V1\n"))
        return false;
    for (;;) {
        if (read_line(line) != PROBE_LINE_OK)
            return fail(observation, "incomplete, invalid or unreadable request line");
        if (observation->requests == PROBE_OPERATION_LIMIT)
            return fail(observation, "request count limit");
        ++observation->requests;
        char *fields[4] = {0};
        size_t count = 0;
        if (!split_fields(line, fields, &count))
            return fail(observation, "invalid request fields");
        if (strcmp(fields[0], "END") == 0) {
            if (count != 1u || read_line(line) != PROBE_LINE_END)
                return fail(observation, "END must be the final complete request");
            return emit(observation, "END\t%u\t%u\t%u\t%u\t%u\t%u\n", observation->factories,
                        observation->snapshots, observation->tables, observation->angles,
                        observation->reference_count, observation->rows);
        }
        if (!execute(observation, fields, count))
            return false;
    }
}

int main(int argc, char **argv)
{
    (void)argv;
    static dimension_observation observation;
    int result = 1;
    if (argc != 1) {
        (void)fail(&observation, "no arguments accepted");
        goto cleanup;
    }
#if defined(_WIN32)
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1) {
        (void)fail(&observation, "binary stream mode failed");
        goto cleanup;
    }
#endif
    if (!run(&observation))
        goto cleanup;
    if (fwrite(observation.output, 1, observation.length, stdout) != observation.length ||
        fflush(stdout) != 0 || ferror(stdout)) {
        (void)fail(&observation, "stdout failed");
        goto cleanup;
    }
    result = 0;
cleanup:
    for (size_t i = 0; i < PROBE_SLOT_LIMIT; ++i)
        mc_world_dimension_release(observation.owners[i]);
    if (result != 0)
        fprintf(stderr, "dimension observation probe: %s (request %u)\n",
                observation.failure != NULL ? observation.failure : "failed", observation.requests);
    return result;
}
