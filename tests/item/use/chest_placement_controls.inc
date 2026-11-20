/* Authored ordinary CK placement through the real IZ/CK, World mutation and
 * RW/JS owners. Nullable JL.E[54] is an explicit source-array fixture only for
 * selected normal returns; the first placement keeps the genuine statistic
 * and reaches its actual LocalPlayer/Minecraft failure after consumption. */
#include "mc/world/runtime/tile_entities/tile_entities.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/mutation/mutation_internal.h"
#include "world/runtime/mutation/world_execution_internal.h"

typedef struct chest_use_observer {
    mc_world_chunk *chunk;
    mc_item_stack *stack;
    mc_item_use_call **call;
    size_t cells, ranges, sounds, expected_tiles;
    int32_t x, z, count;
    bool valid;
} chest_use_observer;

static mc_world_storage_status chest_use_cell(void *context, mc_world_runtime *world, int32_t x,
                                              int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)error;
    ++((chest_use_observer *)context)->cells;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status chest_use_range(void *context, mc_world_runtime *world, int32_t x,
                                               int32_t y, int32_t z, int32_t max_x, int32_t max_y,
                                               int32_t max_z, mc_world_storage_error *error) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)max_x;
    (void)max_y;
    (void)max_z;
    (void)error;
    ++((chest_use_observer *)context)->ranges;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status chest_use_sound(void *context, mc_world_runtime *world,
                                               const mc_registry_string *name, double x, double y,
                                               double z, float volume, float pitch,
                                               mc_world_storage_error *error) {
    (void)error;
    chest_use_observer *o = context;
    ++o->sounds;
    mc_item_use_state use;
    mc_world_chunk_fields chunk;
    size_t tiles = SIZE_MAX;
    int32_t id = -1, metadata = -1, count = INT32_MIN;
    static const char expected[] = "step.wood";
    o->valid =
        o->valid && o->cells != 0 && mc_item_use_inspect(*o->call, &use) && use.active &&
        !use.returned && use.setter_returned && use.setter_value && use.placed_face_returned &&
        use.placed_by_returned && !use.count_stored && !use.statistic_entered &&
        mc_item_stack_source_count_get(o->stack, &count) == MC_ITEM_STACK_OK && count == o->count &&
        x == (double)o->x + .5 && y == 64.5 && z == (double)o->z + .5 && volume == 1.0f &&
        pitch == .8f && name != NULL && name->length == sizeof expected - 1 &&
        mc_world_runtime_block_get(world, o->x, 64, o->z, &id, NULL) == MC_WORLD_STORAGE_OK &&
        id == 54 &&
        mc_world_runtime_metadata_get(world, o->x, 64, o->z, &metadata, NULL) ==
            MC_WORLD_STORAGE_OK &&
        metadata == 0 && mc_world_chunk_fields_get(o->chunk, &chunk) == MC_WORLD_STORAGE_OK &&
        mc_world_chunk_tile_entity_count(chunk.l, &tiles) == MC_WORLD_STORAGE_OK &&
        tiles == o->expected_tiles;
    for (size_t i = 0; name != NULL && i < name->length && i < sizeof expected - 1; ++i)
        o->valid = o->valid && name->units[i] == (unsigned char)expected[i];
    /* Inspect the existing Map without calling its lazy source getter. The
     * placement itself must have installed the new JS before sound260. */
    bool found = false;
    for (size_t i = 0; i < tiles && tiles == o->expected_tiles; ++i) {
        mc_live_tile_entity_map_entry entry;
        mc_tile_entity_snapshot tile;
        if (mc_live_tile_entities_map_entry_at(o->chunk, i, &entry) != MC_WORLD_STORAGE_OK) {
            o->valid = false;
            break;
        }
        if (entry.x != o->x || entry.y != 64 || entry.z != o->z)
            continue;
        found = true;
        o->valid = o->valid &&
                   mc_tile_entity_get_snapshot(entry.tile_entity, &tile) == MC_TILE_ENTITY_OK &&
                   tile.kind == MC_TILE_ENTITY_CHEST && tile.phase == MC_TILE_ENTITY_READY &&
                   tile.world == world && tile.x == o->x && tile.y == 64 && tile.z == o->z &&
                   !tile.invalid && tile.chest_capacity == 36;
    }
    o->valid = o->valid && found;
    return MC_WORLD_STORAGE_OK;
}

static const mc_nbt_tag *chest_use_field(const mc_nbt_tag *tag, const char *name) {
    if (tag == NULL || tag->type != MC_NBT_COMPOUND)
        return NULL;
    for (size_t i = 0; i < tag->value.compound.length; ++i) {
        const mc_nbt_tag *child = tag->value.compound.items[i];
        if (child->name.length != strlen(name))
            continue;
        size_t j = 0;
        while (j < child->name.length && child->name.units[j] == (unsigned char)name[j])
            ++j;
        if (j == child->name.length)
            return child;
    }
    return NULL;
}

static bool chest_use_string(const mc_nbt_tag *tag, const char *value) {
    if (tag == NULL || tag->type != MC_NBT_STRING || tag->value.string.length != strlen(value))
        return false;
    for (size_t i = 0; i < tag->value.string.length; ++i)
        if (tag->value.string.units[i] != (unsigned char)value[i])
            return false;
    return true;
}

static bool chest_use_owner(fixture *f, mc_world_chunk *chunk, int32_t x, int32_t z,
                            mc_tile_entity **out) {
    mc_world_chunk_fields fields;
    size_t count;
    CHECK(mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_tile_entity_count(fields.l, &count) == MC_WORLD_STORAGE_OK);
    mc_tile_entity *mapped = NULL;
    for (size_t i = 0; i < count; ++i) {
        mc_live_tile_entity_map_entry entry;
        CHECK(mc_live_tile_entities_map_entry_at(chunk, i, &entry) == MC_WORLD_STORAGE_OK);
        if (entry.x == x && entry.y == 64 && entry.z == z) {
            CHECK(mapped == NULL);
            mapped = entry.tile_entity;
        }
    }
    CHECK(mapped != NULL &&
          mc_live_tile_entities_get(f->world, x, 64, z, out, NULL) == MC_WORLD_STORAGE_OK &&
          *out == mapped);
    mc_tile_entity_snapshot tile;
    mc_item_stack *empty = NULL;
    int32_t size = -1;
    CHECK(mc_tile_entity_get_snapshot(*out, &tile) == MC_TILE_ENTITY_OK &&
          tile.kind == MC_TILE_ENTITY_CHEST && tile.phase == MC_TILE_ENTITY_READY &&
          tile.world == f->world && tile.x == x && tile.y == 64 && tile.z == z && !tile.invalid &&
          tile.chest_capacity == 36 &&
          mc_tile_entity_chest_size(*out, &size) == MC_TILE_ENTITY_OK && size == 27 &&
          mc_tile_entity_chest_get(*out, 0, &empty) == MC_TILE_ENTITY_OK && empty == NULL);
    return true;
}

static void chest_use_failure(const mc_item_use_error *e) {
    fprintf(stderr, "Chest CK status=%d op=%s at=%s.%s:%u child=%s.%s:%u\n", (int)e->status,
            e->operation == NULL ? "-" : e->operation,
            e->site.class_name == NULL ? "-" : e->site.class_name,
            e->site.method == NULL ? "-" : e->site.method, e->site.pc,
            e->child.class_name == NULL ? "-" : e->child.class_name,
            e->child.method == NULL ? "-" : e->child.method, e->child.pc);
    if (e->mutation != NULL)
        fprintf(stderr, "  mutation status=%d op=%s wrapper=%u chunk=%u hook=%s.%s:%u\n",
                (int)e->mutation->status,
                e->mutation->operation == NULL ? "-" : e->mutation->operation,
                e->mutation->wrapper_bytecode, e->mutation->chunk_bytecode,
                e->mutation->hook_class == NULL ? "-" : e->mutation->hook_class,
                e->mutation->hook_method == NULL ? "-" : e->mutation->hook_method,
                e->mutation->hook_bytecode);
}

static bool chest_use_place(fixture *f, chest_use_observer *o, mc_entity *player, int32_t x,
                            int32_t z, int32_t before, size_t tiles, bool real_statistic) {
    mc_item_use_call *call = NULL;
    mc_item_use_state state;
    mc_item_use_error error;
    o->call = &call;
    o->x = x;
    o->z = z;
    o->count = before;
    o->expected_tiles = tiles;
    const size_t sounds = o->sounds;
    mc_statistic *const statistic = f->registry->statistics.E[54];
    CHECK(statistic != NULL);
    if (!real_statistic)
        f->registry->statistics.E[54] = NULL;
    const mc_item_use_status status = mc_item_use_on_block_begin(
        f->registry, o->stack, player, f->world, x, 63, z, 1, &call, &error);
    f->registry->statistics.E[54] = statistic;
    const mc_item_use_status expected = real_statistic ? MC_ITEM_USE_PLAYER : MC_ITEM_USE_OK;
    if (status != expected)
        chest_use_failure(&error);
    CHECK(status == expected);
    int32_t count = INT32_MIN, id = -1, metadata = -1;
    CHECK(mc_item_use_inspect(call, &state) && !state.active && state.result &&
          state.returned == !real_statistic && state.predicate_returned && state.can_place &&
          state.setter_returned && state.setter_value && state.metadata_returned &&
          state.metadata == 0 && state.placed_face_returned && state.placed_by_returned &&
          state.sound_returned && state.count_stored && state.count_after == before - 1 &&
          state.statistic_entered && state.statistic_returned == !real_statistic &&
          o->sounds == sounds + 1 && o->valid &&
          mc_item_stack_source_count_get(o->stack, &count) == MC_ITEM_STACK_OK &&
          count == before - 1 &&
          mc_world_runtime_block_get(f->world, x, 64, z, &id, NULL) == MC_WORLD_STORAGE_OK &&
          id == 54 &&
          mc_world_runtime_metadata_get(f->world, x, 64, z, &metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          metadata == 0 && mc_world_chunk_state(o->chunk)->dirty);
    if (real_statistic)
        CHECK(error.site.pc == 34 && error.child.pc == 85 && error.source_exception &&
              !error.native_policy && !error.source_error && error.statistic != NULL);
    CHECK(use_retire(&call));
    o->call = NULL;
    return true;
}

static bool chest_use_reject(fixture *f, chest_use_observer *o, mc_entity *player, int32_t x,
                             int32_t z, int32_t before, size_t tiles) {
    mc_item_use_call *call = NULL;
    mc_item_use_state state;
    mc_item_use_error error;
    mc_chunk_state *chunk = mc_world_chunk_state(o->chunk);
    /* Authored clean-flag input makes a reached write observable even when
     * the previously successful placement already made this Chunk dirty. */
    chunk->dirty = false;
    const size_t sounds = o->sounds, cells = o->cells, ranges = o->ranges;
    const mc_item_use_status status = mc_item_use_on_block_begin(
        f->registry, o->stack, player, f->world, x, 63, z, 1, &call, &error);
    if (status != MC_ITEM_USE_OK)
        chest_use_failure(&error);
    CHECK(status == MC_ITEM_USE_OK);
    mc_world_chunk_fields fields;
    size_t map_count;
    int32_t count = INT32_MIN, id = -1;
    CHECK(mc_item_use_inspect(call, &state) && state.returned && !state.result &&
          state.predicate_returned && !state.can_place && !state.metadata_returned &&
          !state.setter_returned && !state.placed_face_returned && !state.placed_by_returned &&
          !state.sound_returned && !state.count_stored && !state.statistic_entered &&
          o->sounds == sounds && o->cells == cells && o->ranges == ranges && !chunk->dirty &&
          mc_item_stack_source_count_get(o->stack, &count) == MC_ITEM_STACK_OK && count == before &&
          mc_world_runtime_block_get(f->world, x, 64, z, &id, NULL) == MC_WORLD_STORAGE_OK &&
          id == 0 && mc_world_chunk_fields_get(o->chunk, &fields) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_tile_entity_count(fields.l, &map_count) == MC_WORLD_STORAGE_OK &&
          map_count == tiles && use_retire(&call));
    return true;
}

/* Separate actual owned RW hook seam. The declared cell is its caller's Block
 * input; this does not claim that public CK selected an owned mutation route. */
static bool chest_use_owned_addition(fixture *f, mc_world_chunk *chunk) {
    mc_entity_tick_execution *execution = calloc(1, sizeof(*execution));
    mc_world_runtime_execution_result *owned = calloc(1, sizeof(*owned));
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    CHECK(execution != NULL && owned != NULL && error != NULL);
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->registry, 54, &block, NULL) == MC_REGISTRY_OK && block != NULL);
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    const size_t index = ((size_t)12 * 16u + 12u) * 128u + 64u;
    CHECK(state->data->blocks[index] == 0);
    state->data->blocks[index] = 54; /* Authored already-written RW caller input. */
    const size_t before = f->world->fields.c->count;
    const size_t bindings = f->world->persistent_bindings;
    mc_entity_tick_execution_init(execution, NULL);
    mc_world_runtime_execution_result_init(owned);
    const mc_entity_tick_execution_site site = {"rw", "c", "(Lfd;III)V", 18};
    mc_entity_tick_execution_world_token token;
    CHECK(mc_entity_tick_execution_push_world(execution, f->world, site, &token) ==
          MC_WORLD_STORAGE_OK);
    const mc_world_mutation_execution_context context = {
        .execution = execution,
        .code_classes = f->world_classes,
        .code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}},
        .result = owned,
        .world = token};
    mc_world_mutation_execution_call *call = NULL;
    mc_chunk_provider *const provider = f->world->fields.v;
    f->world->fields.v = NULL; /* Reached source getter failure after JS/List append. */
    const bool completed = mc_world_mutation_execution_added(&context, f->world, block, 12, 64, 12,
                                                             &call, error, site);
    f->world->fields.v = provider;
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    const mc_live_tile_entity_error *child =
        mc_world_mutation_execution_failed_tile_added(call, &raw);
    CHECK(!completed && call != NULL && child != NULL && raw != MC_WORLD_STORAGE_OK &&
          error->tile_entity.status == raw && child->status == raw &&
          error->tile_entity.operation == child->operation && child->caller_bytecode == 18 &&
          child->bytecode == 67 && error->hook_class != NULL &&
          strcmp(error->hook_class, "rw") == 0 && error->hook_bytecode == 18 &&
          f->world->fields.c->count == before + 1);
    mc_tile_entity_snapshot failed;
    CHECK(mc_tile_entity_get_snapshot(f->world->fields.c->items[before], &failed) ==
              MC_TILE_ENTITY_OK &&
          failed.kind == MC_TILE_ENTITY_CHEST && failed.phase == MC_TILE_ENTITY_READY &&
          failed.world == NULL && failed.x == 0 && failed.y == 0 && failed.z == 0 &&
          failed.chest_capacity == 36);
    /* Caller projection can end independently: the retained full child still
     * exists in the same mutation owner until native retirement. */
    const mc_world_storage_status first = child->status;
    const char *const operation = child->operation;
    memset(error, 0, sizeof(*error));
    CHECK(mc_world_mutation_execution_failed_tile_added(call, &raw) == child && raw == first &&
          child->operation == operation &&
          mc_world_mutation_execution_native_close(call) == MC_WORLD_STORAGE_OK &&
          mc_world_mutation_execution_retire(&call) == MC_WORLD_STORAGE_OK && call == NULL &&
          mc_entity_tick_execution_pop_world(execution, &token, true) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK &&
          f->world->persistent_bindings == bindings);
    /* A fresh reached invocation, rather than a Source retry of the failed
     * one, installs a distinct new JS in the existing source cell. */
    mc_entity_tick_execution_init(execution, NULL);
    mc_world_runtime_execution_result_init(owned);
    CHECK(mc_entity_tick_execution_push_world(execution, f->world, site, &token) ==
          MC_WORLD_STORAGE_OK);
    mc_world_mutation_execution_context next = context;
    next.world = token;
    CHECK(
        mc_world_mutation_execution_added(&next, f->world, block, 12, 64, 12, &call, error, site) &&
        call == NULL && error->status == MC_WORLD_STORAGE_OK &&
        owned->ownership_status == MC_WORLD_STORAGE_OK && f->world->fields.c->count == before + 2);
    mc_tile_entity *installed = NULL;
    CHECK(chest_use_owner(f, chunk, 12, 12, &installed) &&
          installed != f->world->fields.c->items[before]);
    mc_tile_entity_release(installed);
    CHECK(mc_entity_tick_execution_pop_world(execution, &token, false) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK);
    free(error);
    free(owned);
    free(execution);
    return true;
}

typedef struct chest_use_owned_observer {
    mc_world_mutation_execution_call **call;
    bool close_busy, retire_busy;
    size_t cells;
} chest_use_owned_observer;

static mc_world_storage_status chest_use_owned_cell(void *context, mc_world_runtime *world,
                                                    int32_t x, int32_t y, int32_t z,
                                                    mc_world_storage_error *error) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)error;
    chest_use_owned_observer *o = context;
    ++o->cells;
    mc_world_mutation_execution_call *before = *o->call;
    o->close_busy = mc_world_mutation_execution_native_close(before) == MC_WORLD_STORAGE_BUSY;
    o->retire_busy =
        mc_world_mutation_execution_retire(o->call) == MC_WORLD_STORAGE_BUSY && *o->call == before;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status chest_use_owned_range(void *context, mc_world_runtime *world,
                                                     int32_t x, int32_t y, int32_t z, int32_t max_x,
                                                     int32_t max_y, int32_t max_z,
                                                     mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)max_x;
    (void)max_y;
    (void)max_z;
    (void)error;
    return MC_WORLD_STORAGE_OK;
}

/* Complete owned setter, distinct from public CK and the entered RW hook. */
static bool chest_use_owned_setter(fixture *f, mc_world_chunk *chunk) {
    mc_entity_tick_execution *execution = calloc(1, sizeof(*execution));
    mc_world_runtime_execution_result *owned = calloc(1, sizeof(*owned));
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    CHECK(execution != NULL && owned != NULL && error != NULL);
    const size_t bindings = f->world->persistent_bindings;
    const mc_entity_tick_execution_site site = {"ck", "a", "(Liz;Lgs;Lfd;IIII)Z", 162};
    for (unsigned int failure = 0; failure < 2; ++failure) {
        mc_entity_tick_execution_init(execution, NULL);
        mc_world_runtime_execution_result_init(owned);
        mc_entity_tick_execution_world_token token;
        CHECK(mc_entity_tick_execution_push_world(execution, f->world, site, &token) ==
              MC_WORLD_STORAGE_OK);
        const mc_world_mutation_execution_context context = {
            .execution = execution,
            .code_classes = f->world_classes,
            .code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}},
            .result = owned,
            .world = token};
        mc_world_mutation_execution_call *call = NULL;
        chest_use_owned_observer observer = {.call = &call};
        const mc_world_access_callbacks callbacks = {.cell = chest_use_owned_cell,
                                                     .range = chest_use_owned_range};
        mc_world_access *access = NULL;
        if (!failure)
            CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
        const bool deferred = f->world->fields.L;
        mc_world_reference_list *const pending = f->world->fields.G;
        if (failure) {
            /* Explicit owning-descriptor fault, reached after the actual JS
             * factory and written Block. It is not claimed as a Java NPE. */
            f->world->fields.L = true;
            f->world->fields.G = NULL;
        }
        bool changed = false;
        mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
        const int32_t coordinate = failure ? 13 : 11;
        const bool completed = mc_world_runtime_block_set_in_execution(
            &context, f->world, coordinate, 64, coordinate, 54, &changed, &call, &raw, error);
        f->world->fields.L = deferred;
        f->world->fields.G = pending;
        if (failure) {
            mc_world_storage_status child_status = MC_WORLD_STORAGE_OK;
            const mc_live_tile_entity_error *child =
                mc_world_mutation_execution_failed_tile_added(call, &child_status);
            CHECK(!completed && !changed && call != NULL && child != NULL &&
                  raw != MC_WORLD_STORAGE_OK && child_status == raw &&
                  child->caller_bytecode == 18 && child->bytecode == 39 &&
                  error->tile_entity.status == raw && error->tile_entity.bytecode == 39);
            /* End the caller's typed borrow, then retire only native work.
             * The successful case separately refuses reentrant close/retire
             * while this genuine Chest setter is still executing. */
            memset(error, 0, sizeof(*error));
            CHECK(mc_world_mutation_execution_native_close(call) == MC_WORLD_STORAGE_OK &&
                  mc_world_mutation_execution_retire(&call) == MC_WORLD_STORAGE_OK && call == NULL);
        } else {
            if (!completed)
                fprintf(stderr, "Owned Chest setter status=%d op=%s hook=%s:%u owner=%d\n",
                        (int)raw, error->operation == NULL ? "-" : error->operation,
                        error->hook_class == NULL ? "-" : error->hook_class, error->hook_bytecode,
                        (int)owned->ownership_status);
            CHECK(completed && changed && call == NULL && raw == MC_WORLD_STORAGE_OK &&
                  observer.cells != 0 && observer.close_busy && observer.retire_busy);
            bool removed = false;
            CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
            mc_tile_entity *tile = NULL;
            CHECK(chest_use_owner(f, chunk, coordinate, coordinate, &tile));
            mc_tile_entity_release(tile);
        }
        int32_t id = -1, metadata = -1;
        CHECK(mc_world_runtime_block_get(f->world, coordinate, 64, coordinate, &id, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              id == 54 &&
              mc_world_runtime_metadata_get(f->world, coordinate, 64, coordinate, &metadata,
                                            NULL) == MC_WORLD_STORAGE_OK &&
              metadata == 0 &&
              mc_entity_tick_execution_pop_world(execution, &token, failure != 0) ==
                  MC_WORLD_STORAGE_OK &&
              mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK);
    }
    /* Successful install adds the actual JS World backedge; the rejected
     * descriptor path installs none. Both execution scopes are fully ended. */
    CHECK(f->world->persistent_bindings == bindings + 1);
    free(error);
    free(owned);
    free(execution);
    return true;
}

static bool chest_use_nullable_hook(fixture *f) {
    mc_world_runtime_classes *classes = NULL;
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &classes,
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(classes, f->registry, NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(classes, f->entities) == MC_ENTITY_OK);
    mc_tile_entity_piston_class_snapshot cold, reached;
    CHECK(mc_tile_entity_piston_class_get_snapshot(classes, &cold) == MC_TILE_ENTITY_OK &&
          cold.base_phase == MC_TILE_ENTITY_CLASS_COLD && cold.live_tile_entities == 0);
    mc_entity_tick_execution *execution = calloc(1, sizeof(*execution));
    mc_world_runtime_execution_result *owned = calloc(1, sizeof(*owned));
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    CHECK(execution != NULL && owned != NULL && error != NULL);
    mc_entity_tick_execution_init(execution, NULL);
    mc_world_runtime_execution_result_init(owned);
    const mc_entity_tick_execution_site caller = {"lm", "a", "(IIIII)Z", 294};
    mc_entity_tick_execution_world_token token;
    CHECK(mc_entity_tick_execution_push_world(execution, NULL, caller, &token) ==
          MC_WORLD_STORAGE_OK);
    const mc_world_mutation_execution_context context = {
        .execution = execution,
        .code_classes = classes,
        .code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}},
        .result = owned,
        .world = token};
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->registry, 54, &block, NULL) == MC_REGISTRY_OK && block != NULL);
    mc_world_mutation_execution_call *call = NULL;
    const bool completed =
        mc_world_mutation_execution_added(&context, NULL, block, 8, 64, 8, &call, error, caller);
    mc_entity_tick_execution_site source_null = {0};
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    const mc_live_tile_entity_error *child =
        mc_world_mutation_execution_failed_tile_added(call, &raw);
    CHECK(!completed && call != NULL && child != NULL &&
          raw == MC_WORLD_STORAGE_UNSUPPORTED_STATE && child->bytecode == 18 &&
          child->tile_entity.status == MC_TILE_ENTITY_OK && error->hook_bytecode == 18 &&
          mc_world_mutation_execution_source_null(call, &source_null) && source_null.pc == 18 &&
          strcmp(source_null.class_name, "rw") == 0 && strcmp(source_null.method, "c") == 0 &&
          owned->ownership_status == MC_WORLD_STORAGE_OK && !owned->selection_ready &&
          mc_tile_entity_piston_class_get_snapshot(classes, &reached) == MC_TILE_ENTITY_OK &&
          reached.base_phase == MC_TILE_ENTITY_CLASS_READY && reached.live_tile_entities == 0);
    /* The actual cold factory ran despite NULL B and released only its private
     * JS temporary. A native invocation-admission error is not a Source NPE. */
    memset(error, 0, sizeof(*error));
    CHECK(mc_world_mutation_execution_native_close(call) == MC_WORLD_STORAGE_OK &&
          mc_world_mutation_execution_retire(&call) == MC_WORLD_STORAGE_OK && call == NULL &&
          mc_entity_tick_execution_pop_world(execution, &token, true) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_free(classes, NULL) == MC_WORLD_STORAGE_OK);
    free(error);
    free(owned);
    free(execution);
    return true;
}

#include "chest_nullable_controls.inc"

static bool chest_placement_controls(fixture *f) {
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL);
    const int32_t supports[][2] = {{8, 8}, {9, 8}, {10, 8}, {4, 5}, {6, 5}, {5, 5}};
    for (size_t i = 0; i < sizeof supports / sizeof *supports; ++i)
        blocks[((size_t)supports[i][0] * 16u + (size_t)supports[i][1]) * 128u + 63u] = 1;
    mc_world_chunk *chunk = NULL;
    CHECK(mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(state != NULL && acquisition != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, state));
    mc_world_chunk_fields fields;
    size_t map_count;
    CHECK(mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_tile_entity_count(fields.l, &map_count) == MC_WORLD_STORAGE_OK &&
          map_count == 0);
    mc_player_session *session = NULL;
    mc_entity *player = NULL;
    CHECK(mc_player_session_create(f->sessions, NULL, NULL, &session, NULL) == MC_PLAYER_OK &&
          mc_local_player_allocate(f->entities, &player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(player, f->world, NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK);
    mc_player_session_retire(&session);
    mc_item_stack *stack = NULL;
    CHECK(use_stack(54, 8, 7, &stack));
    chest_use_observer observer = {.chunk = chunk, .stack = stack, .valid = true};
    const mc_world_access_callbacks callbacks = {
        .cell = chest_use_cell, .range = chest_use_range, .sound = chest_use_sound};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    mc_tile_entity *owners[4] = {0};
    CHECK(chest_use_place(f, &observer, player, 8, 8, 8, 1, true) &&
          chest_use_owner(f, chunk, 8, 8, &owners[0]) &&
          chest_use_place(f, &observer, player, 9, 8, 7, 2, false) &&
          chest_use_owner(f, chunk, 9, 8, &owners[1]) && owners[0] != owners[1] &&
          chest_use_reject(f, &observer, player, 10, 8, 6, 2) &&
          chest_use_place(f, &observer, player, 4, 5, 6, 3, false) &&
          chest_use_owner(f, chunk, 4, 5, &owners[2]) &&
          chest_use_place(f, &observer, player, 6, 5, 5, 4, false) &&
          chest_use_owner(f, chunk, 6, 5, &owners[3]) &&
          chest_use_reject(f, &observer, player, 5, 5, 4, 4));
    for (size_t i = 0; i < 4; ++i)
        for (size_t j = i + 1; j < 4; ++j)
            CHECK(owners[i] != owners[j]);
    /* A fresh actual Map.values save projection must contain each new JS and
     * an empty Items list. This is a save-readiness check, not disk/JVM parity. */
    mc_nbt_tree *records = NULL;
    CHECK(mc_live_tile_entities_build_source_records(chunk, NULL, &records, NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_nbt_tag *list = chest_use_field(mc_nbt_tree_root(records), "TileEntities");
    CHECK(list != NULL && list->type == MC_NBT_LIST && list->value.list.length == 4);
    const int32_t xs[] = {8, 9, 4, 6}, zs[] = {8, 8, 5, 5};
    bool seen[4] = {0};
    for (size_t i = 0; i < list->value.list.length; ++i) {
        const mc_nbt_tag *record = list->value.list.items[i];
        const mc_nbt_tag *x = chest_use_field(record, "x"), *y = chest_use_field(record, "y"),
                         *z = chest_use_field(record, "z"),
                         *items = chest_use_field(record, "Items");
        CHECK(chest_use_string(chest_use_field(record, "id"), "Chest") && x != NULL &&
              x->type == MC_NBT_INT && y != NULL && y->type == MC_NBT_INT &&
              y->value.int_value == 64 && z != NULL && z->type == MC_NBT_INT && items != NULL &&
              items->type == MC_NBT_LIST && items->value.list.length == 0);
        size_t j = 0;
        while (j < 4 && (x->value.int_value != xs[j] || z->value.int_value != zs[j]))
            ++j;
        CHECK(j < 4 && !seen[j]);
        seen[j] = true;
    }
    mc_nbt_tree_free(records);
    CHECK(observer.valid && observer.sounds == 4);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(chest_use_owned_addition(f, chunk) && chest_use_owned_setter(f, chunk));
    for (size_t i = 0; i < 4; ++i)
        mc_tile_entity_release(owners[i]);
    mc_item_stack_release(stack);
    mc_entity_release(player);
    /* Native joint cleanup consumes actual Map and c/G rights, including the
     * legitimate failed World.set List-only prefix. No source close/removal,
     * replacement Tile lookup or failed constructor completion is supplied. */
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_tile_entity_count(fields.l, &map_count) == MC_WORLD_STORAGE_OK &&
          map_count == 0);
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(chest_use_nullable_hook(f) && chest_use_nullable_controls(f));
    return true;
}
