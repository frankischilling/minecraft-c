#include "maintenance_fixture.h"
#include <inttypes.h>

static const char *trace[4096];
static size_t trace_count;

void mc_test_maintenance_trace(const char *event) {
    if (trace_count < 4096)
        trace[trace_count++] = event;
}

static void trace_reset(void) {
    trace_count = 0;
}

static bool trace_leaf(void) {
#ifdef MC_TEST_MAINTENANCE_TRACE
    REQUIRE(trace_count >= 2 && strcmp(trace[trace_count - 2], "loader") == 0 &&
            strcmp(trace[trace_count - 1], "generator") == 0);
#endif
    return true;
}

static bool corrupt_lock(const char *directory) {
    char path[8192];
    const int length = snprintf(path, sizeof(path), "%s/session.lock", directory);
    REQUIRE(length > 0 && (size_t)length < sizeof(path));
    FILE *file = NULL;
#ifdef _MSC_VER
    REQUIRE(fopen_s(&file, path, "wb") == 0 && file != NULL);
#else
    file = fopen(path, "wb");
    REQUIRE(file != NULL);
#endif
    const unsigned char bytes[8] = {0};
    REQUIRE(fwrite(bytes, sizeof(bytes), 1, file) == 1 && fclose(file) == 0);
    return true;
}

static bool run_case(const char *resources, const char *root, const char *kind) {
    maintenance_fixture f = {0};
    char directory[8192];
    const int length = snprintf(directory, sizeof(directory), "%s/%s", root, kind);
    REQUIRE(length > 0 && (size_t)length < sizeof(directory));
    REQUIRE(fixture_open(&f, resources, directory));
    const bool many = strcmp(kind, "pending101") == 0, missing = strcmp(kind, "missing") == 0,
               lock = strcmp(kind, "lock") == 0, with_item = strcmp(kind, "item") == 0;
    const size_t count = many ? 101 : strcmp(kind, "cached") == 0 || lock || with_item ? 1 : 0;
    mc_chunk_state *chunks[101] = {0};
    for (size_t i = 0; i < count; ++i) {
        REQUIRE(mc_world_runtime_get(f.world, (int32_t)(20 + i), 0, &chunks[i]) ==
                MC_CHUNK_ACQUIRE_OK);
        REQUIRE(chunks[i] != NULL && chunks[i]->active);
    }
    mc_entity *item = NULL;
    mc_item_stack *stack = NULL;
    if (with_item) {
        REQUIRE(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
                mc_item_stack_construct(stack, 1, 1, 0) == MC_ITEM_STACK_OK);
        REQUIRE(mc_item_entity_allocate(f.entities, &item, NULL) == MC_ENTITY_OK &&
                mc_item_entity_construct(item, f.world, 328.5, 80.0, 8.5, stack, NULL) ==
                    MC_ENTITY_OK);
        bool inserted = false;
        REQUIRE(mc_world_runtime_insert_entity(f.world, item, &inserted, NULL) ==
                    MC_WORLD_STORAGE_OK &&
                inserted);
    }
    if (many || lock || with_item)
        for (size_t i = 0; i < count; ++i)
            REQUIRE(pending_input(&f, (int32_t)(20 + i), 0));
    if (missing)
        REQUIRE(pending_input(&f, 20, 0));
    if (lock)
        REQUIRE(corrupt_lock(directory));
    /* The actual base provider has no enqueue route. Its native dedicated-only
     * mark_drop API must stay rejected; never change provider policy to test it. */
    REQUIRE(mc_world_runtime_mark_drop(f.world, 20, 0) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
    trace_reset();
    mc_world_runtime_lifecycle_error error;
    const mc_chunk_lifecycle_status status = mc_world_runtime_finish_maintenance(f.world, &error);
    if (missing) {
        REQUIRE(status == MC_CHUNK_LIFECYCLE_STATE_ERROR && f.provider.pending->count == 1 &&
                f.provider.cache->count == 0);
        REQUIRE(error.cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
#ifdef MC_TEST_MAINTENANCE_TRACE
        REQUIRE(trace_count == 0);
#endif
    } else if (lock) {
        REQUIRE(status == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED &&
                error.cause.status == MC_WORLD_STORAGE_SESSION_MISMATCH);
        REQUIRE(!chunks[0]->active && chunks[0]->last_save_time == 123 &&
                f.provider.pending->count == 1 && f.provider.cache->count == 1);
#ifdef MC_TEST_MAINTENANCE_TRACE
        REQUIRE(trace_count == 10 && strcmp(trace[0], "detach") == 0 &&
                strcmp(trace[9], "main") == 0);
#endif
    } else {
        REQUIRE(status == MC_CHUNK_LIFECYCLE_OK && error.status == MC_CHUNK_LIFECYCLE_OK &&
                error.cause.status == MC_WORLD_STORAGE_OK);
        REQUIRE(trace_leaf());
        if (many) {
            REQUIRE(f.provider.pending->count == 1 && f.provider.cache->count == 1 &&
                    f.provider.cache->insertion_count == 1);
            size_t inactive = 0;
            for (size_t i = 0; i < count; ++i) {
                if (!chunks[i]->active) {
                    ++inactive;
                    REQUIRE(chunks[i]->last_save_time == 123);
                }
            }
            REQUIRE(inactive == 100);
#ifdef MC_TEST_MAINTENANCE_TRACE
            REQUIRE(trace_count == 1102);
            for (size_t i = 0; i < 100; ++i) {
                REQUIRE(strcmp(trace[i * 11], "detach") == 0 &&
                        strcmp(trace[i * 11 + 9], "main") == 0 &&
                        strcmp(trace[i * 11 + 10], "extra") == 0);
            }
#endif
        } else if (with_item) {
            mc_world_runtime_fields fields;
            REQUIRE(mc_world_runtime_fields_get(f.world, &fields) == MC_WORLD_STORAGE_OK);
            REQUIRE(f.provider.pending->count == 0 && f.provider.cache->count == 0 &&
                    !chunks[0]->active && chunks[0]->last_save_time == 123 &&
                    chunks[0]->has_entities);
            REQUIRE(fields.b->count == 1 && fields.D->count == 1);
#ifdef MC_TEST_MAINTENANCE_TRACE
            REQUIRE(trace_count == 13);
#endif
        } else {
            REQUIRE(f.provider.pending->count == 0 && f.provider.cache->count == count);
            if (count)
                REQUIRE(chunks[0]->active && chunks[0]->last_save_time == 0);
#ifdef MC_TEST_MAINTENANCE_TRACE
            REQUIRE(trace_count == 2);
#endif
        }
    }
    printf("case\t%s\tstatus=%d\tcache=%zu\tpending=%zu\ttrace=%zu\n", kind, (int)status,
           f.provider.cache->count, f.provider.pending->count, trace_count);
    printf("states\t%s\t[", kind);
    for (size_t i = 0; i < count; ++i)
        printf("%s[%d,%d,%s,%" PRId64 ",%s,%s]", i ? "," : "", chunks[i]->x, chunks[i]->z,
               chunks[i]->active ? "true" : "false", chunks[i]->last_save_time,
               chunks[i]->dirty ? "true" : "false", chunks[i]->has_entities ? "true" : "false");
    printf("]\nremaining\t%s\t[", kind);
    for (size_t i = 0; i < f.provider.cache->count; ++i)
        printf("%s[%d,%d]", i ? "," : "", f.provider.cache->entries[i].chunk->x,
               f.provider.cache->entries[i].chunk->z);
    printf("]\n");
    mc_entity_release(item);
    mc_item_stack_release(stack);
    REQUIRE(fixture_close(&f));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || !mc_math_trig_init(&trig))
        return 2;
    const char *cases[] = {"empty", "cached", "pending101", "missing", "lock", "item"};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        if (!run_case(argv[1], argv[2], cases[i]))
            return 1;
    mc_world_runtime_lifecycle_error error;
    if (mc_world_runtime_finish_maintenance(NULL, &error) != MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT)
        return 1;
    printf("Provider maintenance: %u checks\n", checks);
    return 0;
}
