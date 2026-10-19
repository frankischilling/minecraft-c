#include "mc/item/stack/stack.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/item/registry/stacks_internal.h"

#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define MAX_BYTES 16384u

/* Input declarations shared with the original numeric observer. No expected
 * fields or original reflective animation inputs are used by this probe. */
static const int32_t inputs[][3] = {{0, 0, 0},
                                    {1, 1, 0},
                                    {-1, -1, -1},
                                    {INT32_MAX, INT32_MIN, INT32_MAX},
                                    {INT32_MIN, INT32_MAX, INT32_MIN},
                                    {32000, 0, -1},
                                    {-2147483647, 2, 65535},
                                    {2147483646, -64, -65536},
                                    {123456789, 64, 32768},
                                    {-987654321, 65, -32769},
                                    {256, INT32_MAX, INT32_MIN},
                                    {-42, INT32_MIN, INT32_MAX}};

typedef struct observation {
    mc_item_stack *source, *copy;
    mc_item_stack_fields constructed, copied;
} observation;

typedef struct identities {
    const mc_item_stack *objects[24];
    size_t count;
} identities;

typedef struct protocol {
    char bytes[MAX_BYTES];
    size_t length, rows;
} protocol;

static bool row(protocol *output, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(output->bytes + output->length, sizeof(output->bytes) - output->length,
                           format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= sizeof(output->bytes) - output->length)
        return false;
    output->length += (size_t)length;
    ++output->rows;
    return true;
}

/* IDs arise from actual live pointer identity at first emitted occurrence.
 * Equal field payloads do not merge objects. */
static size_t identity(identities *ids, const mc_item_stack *object)
{
    if (object == NULL)
        return 0;
    for (size_t i = 0; i < ids->count; ++i)
        if (ids->objects[i] == object)
            return i + 1;
    if (ids->count == COUNT(ids->objects))
        return 0;
    ids->objects[ids->count] = object;
    ++ids->count;
    return ids->count;
}

static bool fresh(const observation *observed, size_t complete, const mc_item_stack *object)
{
    if (object == NULL)
        return false;
    for (size_t i = 0; i < complete; ++i)
        if (observed[i].source == object || observed[i].copy == object)
            return false;
    return true;
}

static bool cold(const mc_registry_classes *owner)
{
    if (owner->blocks_phase != MC_REGISTRY_COLD || owner->nr_state.phase != MC_REGISTRY_COLD ||
        owner->jp_state.phase != MC_REGISTRY_COLD || owner->rp_state.phase != MC_REGISTRY_COLD ||
        owner->rw_state.phase != MC_REGISTRY_COLD ||
        owner->pressure_state.phase != MC_REGISTRY_COLD ||
        owner->item_dq_state.phase != MC_REGISTRY_COLD ||
        owner->item_cp_state.phase != MC_REGISTRY_COLD ||
        owner->item_tool_state.phase != MC_REGISTRY_COLD ||
        owner->translation_state.phase != MC_REGISTRY_COLD ||
        owner->translation_wrapper_state.phase != MC_REGISTRY_COLD ||
        owner->statistic_key_state.phase != MC_REGISTRY_COLD ||
        owner->statistic_base_state.phase != MC_REGISTRY_COLD ||
        owner->statistics_state.phase != MC_REGISTRY_COLD ||
        owner->achievements_state.phase != MC_REGISTRY_COLD ||
        owner->crafting_state.phase != MC_REGISTRY_COLD ||
        owner->smelting_state.phase != MC_REGISTRY_COLD || owner->initializer != NULL)
        return false;
    for (size_t i = 0; i < COUNT(owner->block_states); ++i)
        if (owner->block_states[i].phase != MC_REGISTRY_COLD)
            return false;
    for (size_t i = 0; i < COUNT(owner->item_states); ++i)
        if (owner->item_states[i].phase != MC_REGISTRY_COLD)
            return false;
    return true;
}

static bool emit_fields(protocol *output, identities *ids, const char *kind, size_t index,
                        const mc_item_stack *object, const mc_item_stack_fields *fields)
{
    const size_t id = identity(ids, object);
    return id != 0 &&
           row(output, "%s\t%zu\tR:%zu\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\n", kind,
               index, id, fields->count, fields->animation, fields->id, fields->damage);
}

int main(int argc, char **argv)
{
    int result = 1;
    const char *failure = "Stack values observation setup failed";
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *owner = NULL;
    observation observed[COUNT(inputs)] = {0};
    identities ids = {0};
    protocol output = {0};
#ifdef _WIN32
    if (_setmode(_fileno(stderr), _O_BINARY) < 0 || _setmode(_fileno(stdout), _O_BINARY) < 0)
        goto cleanup;
#endif
#ifdef SIGPIPE
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        goto cleanup;
#endif
    if (argc != 2 || (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0)) {
        failure = "Usage: stack_values_probe client|server";
        goto cleanup;
    }
    const mc_registry_side side =
        strcmp(argv[1], "client") == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER;
    if (!mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {side, profile, materials, 0, NULL};
    if (mc_registry_classes_allocate(&options, &owner, NULL) != MC_REGISTRY_OK || !cold(owner))
        goto cleanup;
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(profile);
    /* Isolated actual numeric-method admission, never class phase fabrication.
     * The real ledger holds every RAW/constructed source and copy through END. */
    owner->entered = 1;
    failure = "Actual Stack construction or copy failed";
    for (size_t i = 0; i < COUNT(inputs); ++i) {
        mc_item_stack_phase phase;
        if (mc_registry_stack_allocate(owner, &observed[i].source) != MC_REGISTRY_OK ||
            !fresh(observed, i, observed[i].source) ||
            mc_item_stack_phase_get(observed[i].source, &phase) != MC_ITEM_STACK_OK ||
            phase != MC_ITEM_STACK_RAW ||
            mc_registry_stack_construct_numeric(owner, observed[i].source, inputs[i][0],
                                                inputs[i][1], inputs[i][2]) != MC_REGISTRY_OK ||
            mc_item_stack_phase_get(observed[i].source, &phase) != MC_ITEM_STACK_OK ||
            phase != MC_ITEM_STACK_READY ||
            mc_item_stack_fields_get(observed[i].source, &observed[i].constructed) !=
                MC_ITEM_STACK_OK)
            goto cleanup;
        if (mc_registry_stack_copy(owner, observed[i].source, &observed[i].copy) !=
                MC_REGISTRY_OK ||
            !fresh(observed, i, observed[i].copy) || observed[i].copy == observed[i].source ||
            mc_item_stack_phase_get(observed[i].copy, &phase) != MC_ITEM_STACK_OK ||
            phase != MC_ITEM_STACK_READY ||
            mc_item_stack_fields_get(observed[i].copy, &observed[i].copied) != MC_ITEM_STACK_OK)
            goto cleanup;
    }
    failure = "Stack observation lifetime or cold-state check failed";
    if (!cold(owner) || mc_registry_stack_count(owner) != 2 * COUNT(inputs) ||
        mc_java_random_profile_allocation_bytes(profile) != profile_bytes)
        goto cleanup;
    failure = "Stack values protocol assembly failed";
    if (!row(&output, "STACK_NATIVE_VALUES_V1\t%s\n", argv[1]))
        goto cleanup;
    for (size_t i = 0; i < COUNT(inputs); ++i)
        if (!row(&output, "INPUT\t%zu\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\n", i, inputs[i][0],
                 inputs[i][1], inputs[i][2]) ||
            !emit_fields(&output, &ids, "CONSTRUCTED", i, observed[i].source,
                         &observed[i].constructed) ||
            !emit_fields(&output, &ids, "COPY", i, observed[i].copy, &observed[i].copied))
            goto cleanup;
    if (ids.count != 2 * COUNT(inputs) ||
        !row(&output, "END\t%zu\t%zu\t%zu\n", COUNT(inputs), ids.count, output.rows))
        goto cleanup;
    for (size_t i = 0; i < output.length; ++i)
        if ((unsigned char)output.bytes[i] > 0x7fu)
            goto cleanup;
    failure = "Stack values protocol output failed";
    if (fwrite(output.bytes, 1, output.length, stdout) != output.length || fflush(stdout) != 0 ||
        ferror(stdout))
        goto cleanup;
    result = 0;
cleanup:
    if (owner != NULL) {
        owner->entered = 0;
        if (mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK) {
            /* A refused retirement cannot leave borrowed dependencies freed. */
            fputs("Stack observation class retirement refused; dependencies retained\n", stderr);
            return 1;
        }
    }
    mc_java_random_profile_free(profile);
    mc_materials_destroy(materials);
    if (result != 0)
        fprintf(stderr, "%s\n", failure);
    return result;
}
