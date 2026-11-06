/* Genuine GAME constructors and authored Chunk bytes; compile this file alone.
 * These are native callback/order controls, not original execution traces. */
#define MC_COLLISION_BOXES_MAIN mc_auxiliary_unused_boxes_main
#include "../collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "mc/world/runtime/auxiliary.h"

typedef struct auxiliary_row {
    unsigned int receiver;
    int32_t effect, x, y, z, data, live_id;
    mc_entity *player;
} auxiliary_row;

typedef struct auxiliary_log {
    auxiliary_row rows[64];
    size_t count;
    bool bad;
} auxiliary_log;

typedef struct auxiliary_receiver {
    auxiliary_log *log;
    mc_world_access *self, *append;
    unsigned int id;
    bool remove_self, nested, entered;
    int32_t reject;
} auxiliary_receiver;

/* These are native contract receivers, not substituted RenderGlobal methods.
 * The notifying setter must be allowed to finish its reached notifications. */
static mc_world_storage_status auxiliary_cell(void *context, mc_world_runtime *world, int32_t x,
                                              int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status auxiliary_range(void *context, mc_world_runtime *world, int32_t x0,
                                               int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                                               int32_t z1, mc_world_storage_error *error) {
    (void)x1;
    (void)y1;
    (void)z1;
    return auxiliary_cell(context, world, x0, y0, z0, error);
}

static mc_world_storage_status auxiliary_observed(void *context, mc_world_runtime *world,
                                                  mc_entity *player, int32_t effect, int32_t x,
                                                  int32_t y, int32_t z, int32_t data,
                                                  mc_world_storage_error *error) {
    auxiliary_receiver *r = context;
    auxiliary_log *log = r->log;
    if (log->count >= COUNT(log->rows) || world->active_calls == 0 ||
        mc_world_access_free(r->self, NULL) != MC_WORLD_STORAGE_BUSY) {
        log->bad = true;
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    int32_t id = -1;
    if (effect == 1004 &&
        mc_world_runtime_block_get(world, x, y, z, &id, error) != MC_WORLD_STORAGE_OK)
        return error->status;
    log->rows[log->count++] = (auxiliary_row){r->id, effect, x, y, z, data, id, player};
    if (r->remove_self) {
        r->remove_self = false;
        bool removed = false;
        if (mc_world_runtime_remove_world_access(world, r->self, &removed, error) !=
                MC_WORLD_STORAGE_OK ||
            !removed || mc_world_access_free(r->self, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_world_runtime_add_world_access(world, r->append, error) != MC_WORLD_STORAGE_OK) {
            log->bad = true;
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        }
    }
    if (r->nested && !r->entered) {
        r->entered = true;
        mc_world_auxiliary_error nested = {0};
        const mc_world_storage_status status =
            mc_world_runtime_auxiliary_effect(world, player, 9007, x, y, z, data, &nested);
        r->entered = false;
        const bool returned = nested.returned;
        (void)mc_world_auxiliary_error_retire(&nested);
        if (status != MC_WORLD_STORAGE_OK || !returned) {
            log->bad = true;
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        }
    }
    if (effect == r->reject) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "authored auxiliary receiver refusal", 0};
        return error->status;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool auxiliary_rows(const auxiliary_log *log, const unsigned int *ids, size_t count) {
    if (log->bad || log->count != count)
        return false;
    for (size_t i = 0; i < count; ++i)
        if (log->rows[i].receiver != ids[i])
            return false;
    return true;
}

static bool auxiliary_controls(fixture *f, auxiliary_receiver *r, auxiliary_log *log) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[0];
    mc_world_auxiliary_error error = {0};
    bool removed = false;
    const unsigned int changed_ids[] = {1, 2, 3}, nested_ids[] = {1, 2, 1, 2, 3, 3};
    CHECK(mc_world_runtime_auxiliary_effect(world, NULL, 2001, INT32_MIN, 64, INT32_MAX, INT32_MIN,
                                            &error) == MC_WORLD_STORAGE_OK &&
          error.returned && error.source_pc == 51 && world->active_calls == 0 &&
          mc_world_auxiliary_error_retire(&error));
    CHECK(mc_world_runtime_add_world_access(world, r[0].self, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, r[0].self, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, r[1].self, NULL) == MC_WORLD_STORAGE_OK);
    r[0].remove_self = true;
    r[0].append = r[2].self;
    CHECK(mc_world_runtime_auxiliary_effect(world, NULL, 2001, INT32_MIN, 64, INT32_MAX, INT32_MIN,
                                            &error) == MC_WORLD_STORAGE_OK &&
          error.returned && error.source_pc == 51 && world->active_calls == 0 &&
          auxiliary_rows(log, changed_ids, COUNT(changed_ids)));
    for (size_t i = 0; i < log->count; ++i)
        CHECK(log->rows[i].effect == 2001 && log->rows[i].x == INT32_MIN && log->rows[i].y == 64 &&
              log->rows[i].z == INT32_MAX && log->rows[i].data == INT32_MIN &&
              log->rows[i].player == NULL);
    CHECK(mc_world_auxiliary_error_retire(&error));
    *log = (auxiliary_log){0};
    r[1].nested = true;
    CHECK(mc_world_runtime_auxiliary_effect(world, NULL, 2001, 3, 64, 5, 7, &error) ==
              MC_WORLD_STORAGE_OK &&
          error.returned && world->active_calls == 0 &&
          auxiliary_rows(log, nested_ids, COUNT(nested_ids)));
    r[1].nested = false;
    CHECK(log->rows[0].effect == 2001 && log->rows[1].effect == 2001 &&
          log->rows[2].effect == 9007 && log->rows[3].effect == 9007 &&
          log->rows[4].effect == 9007 && log->rows[5].effect == 2001 &&
          mc_world_auxiliary_error_retire(&error));
    *log = (auxiliary_log){0};
    r[1].reject = 2001;
    CHECK(mc_world_runtime_auxiliary_effect(world, NULL, 2001, 3, 64, 5, 7, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !error.returned && error.source_pc == 40 && error.world_access_pc == 40 &&
          !error.source_exception && log->count == 2 && world->active_calls == 0 &&
          mc_world_auxiliary_error_retire(&error));
    r[1].reject = 0;
    CHECK(mc_world_runtime_remove_world_access(world, r[1].self, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed &&
          mc_world_runtime_remove_world_access(world, r[2].self, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_runtime_add_world_access(world, NULL, NULL) == MC_WORLD_STORAGE_OK);
    *log = (auxiliary_log){0};
    CHECK(mc_world_runtime_auxiliary_effect(world, NULL, 2001, 3, 64, 5, 7, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.source_exception && !error.native_policy && error.source_pc == 40 &&
          strcmp(error.exception_class, "java/lang/NullPointerException") == 0 && log->count == 1 &&
          world->active_calls == 0 && mc_world_auxiliary_error_retire(&error));
    CHECK(mc_world_runtime_remove_world_access(world, NULL, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    /* Every ordinary face reaches the actual neighbor getter and Fire static,
     * including the non-Fire arm. Source integer increments wrap. */
    boxes_air(f, 0);
    for (int32_t face = -1; face <= 6; ++face) {
        bool extinguished = true;
        CHECK(mc_world_runtime_extinguish_fire(world, NULL, 3, 64, 5, face, &extinguished,
                                               &error) == MC_WORLD_STORAGE_OK &&
              error.returned && error.source_pc == 93 && !extinguished &&
              mc_world_auxiliary_error_retire(&error));
    }
    bool extinguished = true;
    CHECK(mc_world_runtime_extinguish_fire(world, NULL, INT32_MAX, 64, 5, 5, &extinguished,
                                           &error) == MC_WORLD_STORAGE_OK &&
          !extinguished && mc_world_auxiliary_error_retire(&error));
    CHECK(boxes_cell(f, 0, 3, 64, 5, 51, 0));
    *log = (auxiliary_log){0};
    r[0].reject = 1004;
    extinguished = false;
    CHECK(mc_world_runtime_extinguish_fire(world, NULL, 3, 64, 4, 3, &extinguished, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !extinguished && error.source_pc == 80 && error.world_access_pc == 40 &&
          error.mutation == NULL && log->count == 1 && log->rows[0].effect == 1004 &&
          log->rows[0].x == 3 && log->rows[0].y == 64 && log->rows[0].z == 5 &&
          log->rows[0].data == 0 && log->rows[0].live_id == 51 &&
          f->states[0]->data->blocks[boxes_index(3, 64, 5)] == 51 &&
          mc_world_auxiliary_error_retire(&error));
    r[0].reject = 0;
    *log = (auxiliary_log){0};
    CHECK(mc_world_runtime_extinguish_fire(world, NULL, 3, 64, 4, 3, &extinguished, &error) ==
              MC_WORLD_STORAGE_OK &&
          extinguished && error.returned && error.source_pc == 93 && error.mutation == NULL &&
          log->count == 1 && log->rows[0].live_id == 51 &&
          f->states[0]->data->blocks[boxes_index(3, 64, 5)] == 0 && world->active_calls == 0 &&
          mc_world_auxiliary_error_retire(&error));
    CHECK(mc_world_runtime_remove_world_access(world, r[0].self, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && !log->bad);
    ok = true;
cleanup:
    (void)mc_world_auxiliary_error_retire(&error);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    auxiliary_receiver receivers[3] = {0};
    auxiliary_log log = {0};
    char root[8192];
    const uint16_t title[] = {'A', 'u', 'x'};
    if (argc != 3 || strcmp(argv[1], "world-auxiliary") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(boxes_open_store(f, 0, root) && boxes_create_world(f, 0));
    const mc_world_access_callbacks callbacks = {
        .cell = auxiliary_cell, .range = auxiliary_range, .auxiliary = auxiliary_observed};
    for (size_t i = 0; i < COUNT(receivers); ++i) {
        receivers[i].id = (unsigned int)i + 1u;
        receivers[i].log = &log;
        CHECK(mc_world_access_create(&callbacks, &receivers[i], &receivers[i].self, NULL) ==
              MC_WORLD_STORAGE_OK);
    }
    CHECK(auxiliary_controls(f, receivers, &log));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("auxiliary fixture cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    for (size_t i = 0; i < COUNT(receivers); ++i)
        if (mc_world_access_free(receivers[i].self, NULL) != MC_WORLD_STORAGE_OK)
            result = 1;
    if (result == 0)
        printf("PASS: %u World auxiliary live-list/fire-order checks\n", checks);
    return result;
}
