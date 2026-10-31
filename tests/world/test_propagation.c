#include "mc/resources/files/files.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/propagation.h"
#include "mc/world/runtime/world_access.h"
#include "session.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool put32(FILE *file, uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)(value >> 24u), (uint8_t)(value >> 16u), (uint8_t)(value >> 8u),
                        (uint8_t)value};
    return fwrite(bytes, sizeof(bytes), 1, file) == 1;
}

static bool snapshot(mc_world_runtime *world, const char *output, const char *phase) {
    const mc_chunk_cache *cache = mc_world_runtime_acquisition(world)->cache;
    char name[2048];
    if (snprintf(name, sizeof(name), "%s/%s.chunks", output, phase) < 0)
        return false;
    FILE *file = fopen(name, "wb");
    if (file == NULL)
        return false;
    bool ok = put32(file, UINT32_C(0x4c495431)) && put32(file, (uint32_t)cache->count);
    for (size_t i = 0; ok && i < cache->count; ++i) {
        const mc_chunk_state *chunk = cache->entries[i].chunk;
        const mc_chunk_data *data = chunk->data;
        if (chunk->blank || data == NULL || data->blocks == NULL) {
            ok = false;
            break;
        }
        ok = put32(file, (uint32_t)chunk->x) && put32(file, (uint32_t)chunk->z) &&
             fwrite(data->blocks, 32768, 1, file) == 1 &&
             fwrite(data->metadata, 16384, 1, file) == 1 &&
             fwrite(data->skylight, 16384, 1, file) == 1 &&
             fwrite(data->block_light, 16384, 1, file) == 1 &&
             fwrite(data->heightmap, 256, 1, file) == 1;
    }
    if (fclose(file) != 0)
        ok = false;
    mc_world_runtime_fields fields;
    mc_world_light_list_view queue;
    if (mc_world_runtime_fields_get(world, &fields) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_light_list_get(world, &queue, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    printf("snapshot\t%s\tchunks=%zu\tqueue=%zu\tM=%d\n", phase, cache->count, queue.count,
           fields.M);
    return ok;
}

typedef struct callback_state {
    const mc_world_light_type *sky;
    mc_world_access *self, *added;
    unsigned calls, added_calls, recursive_next, maximum_m;
    bool mutate, reenter, missing_prefix, saw_m_one, recursive, guard_seen;
} callback_state;

static mc_world_storage_status added_cell(void *owner, mc_world_runtime *world, int32_t x,
                                          int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)error;
    callback_state *state = owner;
    ++state->added_calls;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status cell(void *owner, mc_world_runtime *world, int32_t x, int32_t y,
                                    int32_t z, mc_world_storage_error *error) {
    mc_world_light_operation_error nested;
    callback_state *state = owner;
    ++state->calls;
    mc_world_query_error query;
    int32_t value;
    if (mc_world_runtime_saved_light(world, state->sky, x, y, z, &value, &query) !=
        MC_WORLD_STORAGE_OK)
        return query.status;
    state->missing_prefix = value == 9 || value == 15;
    if (state->mutate) {
        state->mutate = false;
        bool removed = false;
        if (mc_world_runtime_remove_world_access(world, state->self, &removed, error) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        if (mc_world_runtime_add_world_access(world, state->added, error) != MC_WORLD_STORAGE_OK)
            return error->status;
        /* An in-progress occurrence remains held after self-removal. */
        if (mc_world_access_free(state->self, error) != MC_WORLD_STORAGE_BUSY)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    if (state->reenter) {
        state->reenter = false;
        mc_world_runtime_fields fields;
        if (mc_world_runtime_fields_get(world, &fields) != MC_WORLD_STORAGE_OK)
            return error->status;
        state->saw_m_one = fields.M == 1;
        /* A genuine submit grows/mutates the queue while its outer record is
         * detached; nested drain retires the remaining queue records. */
        mc_light_bounds empty = {
            .min_x = 1, .min_y = 1, .min_z = 0, .max_x = 0, .max_y = 0, .max_z = 0};
        mc_light_submit_result submitted;
        mc_world_light_error submission;
        if (mc_world_runtime_light_submit(world, state->sky, &empty, false, &submitted,
                                          &submission) != MC_WORLD_STORAGE_OK)
            return submission.status;
        bool more;
        if (mc_world_runtime_light_drain(world, SIZE_MAX, &more, &nested) != MC_WORLD_STORAGE_OK)
            return nested.status;
        if (more)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    if (state->recursive) {
        mc_world_runtime_fields fields;
        if (mc_world_runtime_fields_get(world, &fields) != MC_WORLD_STORAGE_OK)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        if (fields.M < 0)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        if ((unsigned)fields.M > state->maximum_m)
            state->maximum_m = (unsigned)fields.M;
        if (state->recursive_next < 51) {
            unsigned next = state->recursive_next++;
            int32_t next_x = 1 + (int32_t)(next % 15u) * 3;
            int32_t next_z = 1 + (int32_t)(next / 15u) * 3;
            mc_light_bounds point = {.min_x = next_x,
                                     .min_y = 110,
                                     .min_z = next_z,
                                     .max_x = next_x,
                                     .max_y = 110,
                                     .max_z = next_z};
            mc_light_submit_result submitted;
            mc_world_light_error submission;
            if (mc_world_runtime_light_submit(world, state->sky, &point, false, &submitted,
                                              &submission) != MC_WORLD_STORAGE_OK)
                return submission.status;
            mc_world_light_list_view before, after;
            if (mc_world_runtime_light_list_get(world, &before, error) != MC_WORLD_STORAGE_OK)
                return error->status;
            bool more = true;
            if (mc_world_runtime_light_drain(world, SIZE_MAX, &more, &nested) !=
                MC_WORLD_STORAGE_OK)
                return nested.status;
            if (fields.M == 50) {
                if (mc_world_runtime_light_list_get(world, &after, error) != MC_WORLD_STORAGE_OK)
                    return error->status;
                state->guard_seen = !more && before.count == after.count;
            }
        }
    }
    return MC_WORLD_STORAGE_OK;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *registry = NULL;
    mc_entity_classes *entities = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_client_world_session *session = NULL;
    mc_world_access *access = NULL, *added = NULL, *missing = NULL;
    mc_math_trig *trig = malloc(sizeof(*trig));
    mc_client_world_session_error error = {0};
    mc_world_light_operation_error light = {0};
    CHECK((argc == 4 || (argc == 5 && strcmp(argv[4], "--startup-grid") == 0)) && trig != NULL &&
          mc_math_trig_init(trig));
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(mc_resource_files_create(argv[1], &files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = profile,
                                         .materials = materials,
                                         .resources = mc_resource_files_source(files)};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, trig, &classes, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(classes, registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(classes, entities) == MC_ENTITY_OK);
    CHECK(mc_client_world_session_allocate(classes, registry, &session, &error));
    CHECK(mc_client_world_session_open(session, argv[2], (mc_nbt_string){0}, 23, 512, &error));
    mc_world_runtime *world = mc_client_world_session_world(session);
    const mc_world_runtime_state *live = mc_world_runtime_live(world);
    const int32_t sx = *live->spawn_x, sz = *live->spawn_z;
    printf("world\tseed=23\tspawn=%d,%d,%d\n", sx, *live->spawn_y, sz);
    CHECK(snapshot(world, argv[3], "constructed"));
    const int32_t cx = (sx >> 4) * 16, cz = (sz >> 4) * 16;
    for (int32_t x = -1; x <= 1; ++x)
        for (int32_t z = -1; z <= 1; ++z)
            for (int32_t y = 0; y < 128; y += 16) {
                (void)y;
                for (int32_t dx = -1; dx <= 1; ++dx)
                    for (int32_t dz = -1; dz <= 1; ++dz) {
                        mc_chunk_state *chunk;
                        CHECK(mc_world_runtime_get(world, (cx >> 4) + x + dx, (cz >> 4) + z + dz,
                                                   &chunk) == MC_CHUNK_ACQUIRE_OK);
                    }
            }
    CHECK(snapshot(world, argv[3], "view-undrained"));
    bool more = true;
    unsigned calls = 0;
    while (more) {
        mc_world_light_list_view before, after;
        CHECK(mc_world_runtime_light_list_get(world, &before, NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_light_drain(world, SIZE_MAX, &more, &light) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_light_list_get(world, &after, NULL) == MC_WORLD_STORAGE_OK);
        printf("drain\tview\tcall=%u\tqueueBefore=%zu\tqueueAfter=%zu\tmore=%s\n", ++calls,
               before.count, after.count, more ? "true" : "false");
        CHECK(calls < 10000);
    }
    CHECK(snapshot(world, argv[3], "view-drained"));
    if (argc == 5) {
        calls = 0;
        for (int32_t x = -128; x <= 128; x += 16)
            for (int32_t z = -128; z <= 128; z += 16) {
                int32_t id;
                mc_world_storage_error acquisition = {0};
                mc_world_storage_status acquired =
                    mc_world_runtime_block_get(world, sx + x, 64, sz + z, &id, &acquisition);
                if (acquired != MC_WORLD_STORAGE_OK)
                    fprintf(stderr, "startup frontier x=%d z=%d status=%d operation=%s system=%d\n",
                            sx + x, sz + z, (int)acquired,
                            acquisition.operation == NULL ? "" : acquisition.operation,
                            acquisition.system_error);
                CHECK(acquired == MC_WORLD_STORAGE_OK);
                do {
                    CHECK(mc_world_runtime_light_drain(world, SIZE_MAX, &more, &light) ==
                          MC_WORLD_STORAGE_OK);
                    ++calls;
                    CHECK(calls < 100000);
                } while (more);
            }
        CHECK(snapshot(world, argv[3], "startup-grid-drained"));
        printf("startup\tpoints=289\tdrainCalls=%u\n", calls);
    }

    const mc_world_light_type *sky, *block;
    CHECK(mc_world_runtime_light_type_get(classes, MC_LIGHT_SKY, &sky, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_type_get(classes, MC_LIGHT_BLOCK, &block, NULL) ==
          MC_WORLD_STORAGE_OK);
    const size_t cache_count = mc_world_runtime_acquisition(world)->cache->count;
    bool exposed = false;
    CHECK(mc_world_runtime_sky_exposed(world, 30000000, 128, 30000000, &exposed, &light) ==
              MC_WORLD_STORAGE_OK &&
          exposed);
    CHECK(mc_world_runtime_sky_exposed(world, 32000000, 128, 0, &exposed, &light) ==
              MC_WORLD_STORAGE_OK &&
          !exposed);
    CHECK(mc_world_runtime_sky_exposed(world, 0, -1, 0, &exposed, &light) == MC_WORLD_STORAGE_OK &&
          !exposed);
    CHECK(mc_world_runtime_sky_exposed(world, 1000000, 100, 1000000, &exposed, &light) ==
              MC_WORLD_STORAGE_OK &&
          !exposed);
    CHECK(mc_world_runtime_saved_light_set(world, sky, 1000000, 100, 1000000, 7, &light) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_acquisition(world)->cache->count == cache_count);
    CHECK(mc_world_runtime_saved_light_set(world, block, 1, 110, 1, -1, &light) ==
          MC_WORLD_STORAGE_OK);
    int32_t value;
    CHECK(mc_world_runtime_saved_light(world, block, 1, 110, 1, &value, NULL) ==
              MC_WORLD_STORAGE_OK &&
          value == 15);
    CHECK(mc_world_runtime_saved_light_set(world, sky, 1, 110, 1, 0, &light) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_recheck(world, sky, 1, 110, 1, 0, &light) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_drain(world, SIZE_MAX, &more, &light) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_saved_light(world, sky, 1, 110, 1, &value, NULL) ==
              MC_WORLD_STORAGE_OK &&
          value == 15);

    mc_world_light_list_view list;
    CHECK(mc_world_runtime_light_list_get(world, &list, NULL) == MC_WORLD_STORAGE_OK &&
          list.count == 0);
    const mc_light_bounds empty = {
        .min_x = 1, .min_y = 1, .min_z = 0, .max_x = 0, .max_y = 0, .max_z = 0};
    for (size_t i = 0; i < 500; ++i) {
        mc_light_submit_result submitted;
        CHECK(mc_world_runtime_light_submit(world, sky, &empty, false, &submitted, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              submitted == MC_LIGHT_SUBMIT_APPENDED);
    }
    CHECK(mc_world_runtime_light_drain(world, 0, &more, &light) == MC_WORLD_STORAGE_OK && more);
    CHECK(mc_world_runtime_light_list_get(world, &list, NULL) == MC_WORLD_STORAGE_OK &&
          list.count == 1);
    CHECK(mc_world_runtime_light_drain(world, 0, &more, &light) == MC_WORLD_STORAGE_OK && !more);
    mc_world_runtime_fields fields;
    CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK && fields.M == 0);
    mc_light_bounds point = {
        .min_x = 1, .min_y = 110, .min_z = 1, .max_x = 1, .max_y = 110, .max_z = 1};
    mc_light_submit_result submitted;
    CHECK(mc_world_runtime_light_submit(world, sky, &point, false, &submitted, NULL) ==
          MC_WORLD_STORAGE_OK);
    more = true;
    CHECK(mc_world_runtime_light_drain(world, 0, &more, &light) == MC_WORLD_STORAGE_LIMIT && more);
    CHECK(light.drain_bytecode == 82 && light.region_bytecode == 79);
    CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK && fields.M == 0);
    CHECK(mc_world_runtime_light_list_get(world, &list, NULL) == MC_WORLD_STORAGE_OK &&
          list.count == 0);

    const mc_light_bounds oversized = {
        .min_x = -32, .min_y = 0, .min_z = -32, .max_x = 47, .max_y = 127, .max_z = 47};
    CHECK(mc_world_runtime_light_submit(world, sky, &oversized, false, &submitted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          submitted == MC_LIGHT_SUBMIT_APPENDED);
    CHECK(mc_world_runtime_light_drain(world, 0, &more, &light) == MC_WORLD_STORAGE_OK && !more);
    CHECK(mc_world_runtime_light_list_get(world, &list, NULL) == MC_WORLD_STORAGE_OK &&
          list.count == 0);
    /* Wrapped inclusive X extent is zero and Z is empty. A native step bound
     * must still stop the source's wrapping outer loop without a hang. */
    const mc_light_bounds wrapped_empty = {
        .min_x = INT32_MIN, .min_y = 0, .min_z = 1, .max_x = INT32_MAX, .max_y = 0, .max_z = 0};
    CHECK(mc_world_runtime_light_submit(world, sky, &wrapped_empty, false, &submitted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          submitted == MC_LIGHT_SUBMIT_APPENDED);
    more = true;
    CHECK(mc_world_runtime_light_drain(world, 3, &more, &light) == MC_WORLD_STORAGE_LIMIT && more);
    CHECK(light.region_bytecode == 79 && light.drain_bytecode == 82);
    CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK && fields.M == 0);
    CHECK(mc_world_runtime_light_list_get(world, &list, NULL) == MC_WORLD_STORAGE_OK &&
          list.count == 0);

    callback_state state = {.sky = sky, .mutate = true};
    const mc_world_access_callbacks callbacks = {.cell = cell},
                                    added_callbacks = {.cell = added_cell}, absent = {0};
    CHECK(mc_world_access_create(&callbacks, &state, &access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_access_create(&added_callbacks, &state, &added, NULL) == MC_WORLD_STORAGE_OK);
    state.self = access;
    state.added = added;
    CHECK(mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_saved_light_set(world, sky, 1, 110, 1, 9, &light) ==
          MC_WORLD_STORAGE_OK);
    CHECK(state.calls == 1 && state.added_calls == 0 && state.missing_prefix);
    CHECK(mc_world_runtime_saved_light_set(world, NULL, 1, 110, 1, 3, &light) ==
              MC_WORLD_STORAGE_OK &&
          state.added_calls == 1);
    CHECK(mc_world_runtime_saved_light(world, sky, 1, 110, 1, &value, NULL) ==
              MC_WORLD_STORAGE_OK &&
          value == 9);
    bool removed;
    CHECK(mc_world_runtime_remove_world_access(world, added, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    CHECK(mc_world_access_create(&absent, NULL, &missing, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_add_world_access(world, missing, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_saved_light_set(world, sky, 1, 110, 1, 7, &light) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(light.setter_bytecode == 121 && light.world_access_bytecode == 121);
    CHECK(mc_world_runtime_saved_light(world, sky, 1, 110, 1, &value, NULL) ==
              MC_WORLD_STORAGE_OK &&
          value == 7);
    CHECK(mc_world_runtime_light_submit(world, sky, &point, false, &submitted, NULL) ==
          MC_WORLD_STORAGE_OK);
    more = true;
    CHECK(mc_world_runtime_light_drain(world, SIZE_MAX, &more, &light) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          more);
    CHECK(light.drain_bytecode == 82 && light.region_bytecode == 575 &&
          light.setter_bytecode == 121);
    CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK && fields.M == 0);
    CHECK(mc_world_runtime_light_list_get(world, &list, NULL) == MC_WORLD_STORAGE_OK &&
          list.count == 0);
    CHECK(mc_world_runtime_saved_light(world, sky, 1, 110, 1, &value, NULL) ==
              MC_WORLD_STORAGE_OK &&
          value == 15);
    CHECK(mc_world_runtime_remove_world_access(world, missing, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    CHECK(mc_world_runtime_saved_light_set(world, sky, 1, 110, 1, 7, &light) ==
          MC_WORLD_STORAGE_OK);
    state.reenter = true;
    state.mutate = false;
    state.calls = 0;
    CHECK(mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_submit(world, sky, &empty, false, &submitted, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_submit(world, sky, &point, false, &submitted, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_drain(world, SIZE_MAX, &more, &light) == MC_WORLD_STORAGE_OK);
    CHECK(state.calls >= 1 && state.saw_m_one && !state.reenter);
    CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK && fields.M == 0);
    CHECK(mc_world_runtime_remove_world_access(world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    /* Reach the source recursion guard through 50 genuine nested drains,
     * driven only by actual setters, submissions and WorldAccess callbacks. */
    for (unsigned i = 0; i < 51; ++i) {
        int32_t point_x = 1 + (int32_t)(i % 15u) * 3;
        int32_t point_z = 1 + (int32_t)(i / 15u) * 3;
        CHECK(mc_world_runtime_saved_light_set(world, sky, point_x, 110, point_z, 0, &light) ==
              MC_WORLD_STORAGE_OK);
    }
    state.recursive = true;
    state.recursive_next = 1;
    state.calls = 0;
    CHECK(mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_submit(world, sky, &point, false, &submitted, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_drain(world, SIZE_MAX, &more, &light) == MC_WORLD_STORAGE_OK &&
          !more);
    CHECK(state.maximum_m == 50 && state.guard_seen && state.recursive_next == 51 &&
          state.calls == 51);
    CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK && fields.M == 0);
    CHECK(mc_world_runtime_light_list_get(world, &list, NULL) == MC_WORLD_STORAGE_OK &&
          list.count == 0);
    CHECK(mc_world_runtime_remove_world_access(world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    result = 0;
cleanup:
    if (result != 0)
        fprintf(stderr,
                "Light failure: %d %s drain%u region%u point%u setter%u sky%u; session %d %s\n",
                (int)light.status, light.operation == NULL ? "" : light.operation,
                light.drain_bytecode, light.region_bytecode, light.point_bytecode,
                light.setter_bytecode, light.sky_bytecode, (int)error.storage.status,
                error.storage.operation == NULL ? "" : error.storage.operation);
    if (!mc_client_world_session_retire(&session, &error))
        return 1;
    if (mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_access_free(added, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_access_free(missing, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    if (mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK ||
        mc_registry_classes_free(registry, NULL) != MC_REGISTRY_OK ||
        mc_entity_classes_free(entities) != MC_ENTITY_OK)
        return 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    if (mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    free(trig);
    if (result == 0)
        printf("Lighting propagation: %u checks\n", checks);
    return result;
}
