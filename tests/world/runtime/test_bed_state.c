/* Prepared genuine GAME World/Store/Chunk components. The imported observation
 * main is not run. These are authored native controls of the static VE leaf. */
#define MC_WORLD_ENTITY_TICK_MAIN mc_bed_state_unused_tick_main
#include "test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN

#include "../../../src/block/registry/runtime_internal.h"
#include "mc/world/chunk/nibbles.h"
#include "mc/world/runtime/bed_state.h"
#include "mc/world/runtime/mutations.h"

typedef struct bed_state_observer {
    unsigned int calls;
    int32_t metadata;
    bool refuse, observed_store;
} bed_state_observer;

static mc_world_storage_status bed_state_cell(void *context, mc_world_runtime *world, int32_t x,
                                              int32_t y, int32_t z, mc_world_storage_error *error) {
    bed_state_observer *observer = context;
    ++observer->calls;
    int32_t metadata = -1;
    observer->observed_store =
        mc_world_runtime_metadata_get(world, x, y, z, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
        metadata == observer->metadata;
    if (observer->refuse) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored occupancy cell refusal", 0};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool bed_state_metadata(mc_chunk_data *data, int32_t value) {
    data->blocks[boxes_index(8, 64, 8)] = 1;
    return mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, (uint8_t)value);
}

static bool bed_state_controls(const char *root) {
    bool ok = false, registered = false, fields_saved = false, observation_saved = false;
    fixture *f = calloc(1, sizeof(*f));
    mc_world_placement_error *error = calloc(1, sizeof(*error));
    mc_registry_classes *cold = NULL;
    mc_world_access *access = NULL;
    mc_block *saved_bed = NULL;
    int32_t **saved_table = NULL;
    size_t bed_index = 0, saved_borrows = 0;
    bool saved_observation = false, restore_borrows = false;
    CHECK(f != NULL && error != NULL && tick_setup(f, root));
    mc_world_runtime *world = f->worlds[0];
    mc_registry_classes *registry = f->registry[0];
    mc_chunk_data *data = f->states[0]->data;
    memset(data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    CHECK(mc_registry_block_static_index("T", &bed_index));
    saved_bed = registry->blocks.named[bed_index];
    saved_table = registry->ve_a;
    registry->blocks.named[bed_index] = NULL;
    registry->ve_a = NULL;
    fields_saved = true;
    const size_t bindings = world->persistent_bindings, borrows = registry->runtime_borrows;
    for (int32_t occupied = 0; occupied < 2; ++occupied)
        for (int32_t metadata = 0; metadata < 16; ++metadata) {
            CHECK(bed_state_metadata(data, metadata));
            world->fields.B = (metadata & 1) != 0;
            const int32_t expected = occupied != 0 ? metadata | 4 : metadata & ~4;
            CHECK(mc_world_bed_set_occupied(registry, world, 8, 64, 8, occupied != 0, error) ==
                      MC_WORLD_STORAGE_OK &&
                  error->returned && error->site.pc == 39 && error->query_count == 1 &&
                  error->acquisition != NULL && error->mutation != NULL &&
                  strcmp(error->site.class_name, "ve") == 0 &&
                  strcmp(error->site.descriptor, "(Lfd;IIIZ)V") == 0 && !error->native_policy &&
                  !error->source_exception && !error->source_error &&
                  world->persistent_bindings == bindings + 1 &&
                  registry->runtime_borrows == borrows + 1);
            int32_t actual = -1;
            CHECK(mc_world_runtime_metadata_get(world, 8, 64, 8, &actual, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  actual == expected && data->blocks[boxes_index(8, 64, 8)] == 1 &&
                  registry->blocks.named[bed_index] == NULL && registry->ve_a == NULL);
            CHECK(mc_world_placement_error_retire(error) && error->work == NULL &&
                  world->persistent_bindings == bindings && registry->runtime_borrows == borrows);
        }
    registry->blocks.named[bed_index] = saved_bed;
    registry->ve_a = saved_table;
    fields_saved = false;
    world->fields.B = false;

    /* Static method invocation initializes its declaring class even when the
     * first actual World receiver is null. No singleton read admits this call. */
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         tick_resources};
    CHECK(mc_registry_classes_allocate(&options, &cold, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_bind_entities(cold, f->entity_classes[0], NULL) == MC_REGISTRY_OK &&
          cold->block_states[MC_BLOCK_BED].phase == MC_REGISTRY_COLD && cold->ve_a == NULL);
    CHECK(mc_world_bed_set_occupied(cold, NULL, 8, 64, 8, true, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !error->returned && error->site.pc == 4 && error->query_count == 0 &&
          error->source_exception && !error->source_error && !error->native_policy &&
          strcmp(error->exception_class, "java/lang/NullPointerException") == 0 &&
          cold->block_states[MC_BLOCK_BED].phase == MC_REGISTRY_READY && cold->ve_a != NULL &&
          cold->runtime_borrows == 1 && mc_world_placement_error_retire(error) &&
          cold->runtime_borrows == 0);
    CHECK(mc_registry_classes_free(cold, NULL) == MC_REGISTRY_OK);
    cold = NULL;

    /* A retired native World is refused at its first actual invocation, after
     * the declaring VE active use. It is not a source null exception. */
    const mc_world_runtime_phase phase = world->construction.phase;
    world->construction.phase = MC_WORLD_RUNTIME_RETIRED;
    const mc_world_storage_status refused =
        mc_world_bed_set_occupied(registry, world, 8, 64, 8, true, error);
    world->construction.phase = phase;
    CHECK(refused == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error->site.pc == 4 &&
          !error->returned && error->native_policy && !error->source_exception &&
          error->mutation == NULL && mc_world_placement_error_retire(error));

    /* A reached query retains its own diagnostic when native call admission
     * hits its count ceiling. The notifying setter is still unreached. */
    const size_t active_calls = world->active_calls;
    world->active_calls = SIZE_MAX;
    const mc_world_storage_status limited =
        mc_world_bed_set_occupied(registry, world, 8, 64, 8, true, error);
    world->active_calls = active_calls;
    CHECK(limited == MC_WORLD_STORAGE_LIMIT && error->site.pc == 4 && !error->returned &&
          error->query_count == 1 && error->acquisition != NULL && error->mutation == NULL &&
          error->native_policy && !error->source_exception &&
          error->cause.status == MC_WORLD_STORAGE_LIMIT && mc_world_placement_error_retire(error));

    /* This static leaf has no bounds guard of its own. Both actual World
     * children return normally outside the World domain. */
    CHECK(mc_world_bed_set_occupied(registry, world, 8, -1, 8, true, error) ==
              MC_WORLD_STORAGE_OK &&
          error->returned && error->site.pc == 39 && error->query_count == 1 &&
          error->mutation != NULL && mc_world_placement_error_retire(error));

    bed_state_observer observer = {.metadata = 4};
    const mc_world_access_callbacks callbacks = {.cell = bed_state_cell};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    saved_observation = registry->blocks.t[1];
    registry->blocks.t[1] = true;
    observation_saved = true;
    CHECK(bed_state_metadata(data, 0));
    CHECK(mc_world_bed_set_occupied(registry, world, 8, 64, 8, true, error) ==
              MC_WORLD_STORAGE_OK &&
          observer.calls == 1 && observer.observed_store && error->returned &&
          mc_world_placement_error_retire(error));
    /* FD.e returns true for an in-domain write even when the nibble is equal,
     * so FD.d still emits its actual cell notification on this second call. */
    CHECK(mc_world_bed_set_occupied(registry, world, 8, 64, 8, true, error) ==
              MC_WORLD_STORAGE_OK &&
          observer.calls == 2 && observer.observed_store && error->returned &&
          error->query_count == 1 && error->mutation != NULL &&
          mc_world_placement_error_retire(error));
    CHECK(bed_state_metadata(data, 0));
    observer.refuse = true;
    observer.observed_store = false;
    CHECK(mc_world_bed_set_occupied(registry, world, 8, 64, 8, true, error) ==
              MC_WORLD_STORAGE_IO_ERROR &&
          !error->returned && error->site.pc == 36 && error->query_count == 1 &&
          error->mutation != NULL && error->mutation->wrapper_bytecode == 40 &&
          error->mutation->status == MC_WORLD_STORAGE_IO_ERROR && error->native_policy &&
          !error->source_exception && observer.calls == 3 && observer.observed_store);
    int32_t actual = -1;
    CHECK(mc_world_runtime_metadata_get(world, 8, 64, 8, &actual, NULL) == MC_WORLD_STORAGE_OK &&
          actual == 4 && world->persistent_bindings == bindings + 1 &&
          registry->runtime_borrows == borrows + 1);
    mc_world_placement_work *pending = error->work;
    CHECK(mc_world_bed_set_occupied(registry, world, 8, 64, 8, false, error) ==
              MC_WORLD_STORAGE_BUSY &&
          error->work == pending && error->status == MC_WORLD_STORAGE_IO_ERROR &&
          error->site.pc == 36 && observer.calls == 3);
    saved_borrows = registry->runtime_borrows;
    registry->runtime_borrows = 0;
    restore_borrows = true;
    const bool retired = mc_world_placement_error_retire(error);
    registry->runtime_borrows = saved_borrows;
    restore_borrows = false;
    CHECK(!retired && error->work == pending && error->cleanup.status != MC_WORLD_STORAGE_OK &&
          error->status == MC_WORLD_STORAGE_IO_ERROR && error->site.pc == 36 &&
          error->mutation != NULL && observer.calls == 3 &&
          world->persistent_bindings == bindings + 1);
    CHECK(mc_world_placement_error_retire(error) && observer.calls == 3 &&
          world->persistent_bindings == bindings && registry->runtime_borrows == borrows);
    ok = true;
cleanup:
    if (f != NULL && f->registry[0] != NULL) {
        if (restore_borrows)
            f->registry[0]->runtime_borrows = saved_borrows;
        if (fields_saved) {
            f->registry[0]->blocks.named[bed_index] = saved_bed;
            f->registry[0]->ve_a = saved_table;
        }
        if (observation_saved)
            f->registry[0]->blocks.t[1] = saved_observation;
    }
    if (error != NULL && !mc_world_placement_error_retire(error))
        ok = false;
    free(error);
    if (registered) {
        bool ignored;
        if (mc_world_runtime_remove_world_access(f->worlds[0], access, &ignored, NULL) !=
            MC_WORLD_STORAGE_OK)
            ok = false;
    }
    if (mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK ||
        mc_registry_classes_free(cold, NULL) != MC_REGISTRY_OK || !tick_cleanup(f))
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    char root[8192], resources_path[8192];
    if (argc != 4 || strcmp(argv[1], "bed-state") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) ||
        !decode_path(argv[3], resources_path, sizeof(resources_path)) || !mc_math_trig_init(&trig))
        return 2;
    CHECK(mc_resource_files_create(resources_path, &files) == MC_REGISTRY_OK);
    tick_resources = mc_resource_files_source(files);
    CHECK(bed_state_controls(root));
    result = 0;
cleanup:
    mc_resource_files_free(files);
    if (result == 0 && printf("Bed state: %u checks\n", checks) < 0)
        return 1;
    return result;
}
