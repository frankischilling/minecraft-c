/* Genuine GAME construction and checked owner retirement. Earlier fixture
 * mains are renamed and never run; these are authored native controls. */
#define MC_PRESSURE_MAIN_NAME mc_sign_lifecycle_pressure_fixture_main
#include "../test_pressure_plate.c"
#undef MC_PRESSURE_MAIN_NAME

#include "../../../../src/core/math/execution_internal.h"
#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/mutation/piston_internal.h"
#include "../../../../src/world/runtime/mutation/world_execution_internal.h"
#include "../../../../src/world/runtime/tile_entities/execution_internal.h"

static bool sign_cell(mc_world_runtime *world, int32_t id, int32_t metadata) {
    int32_t current_id = -1, current_metadata = -1;
    return mc_world_runtime_block_get(world, 12, 64, 8, &current_id, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_metadata_get(world, 12, 64, 8, &current_metadata, NULL) ==
               MC_WORLD_STORAGE_OK &&
           current_id == id && current_metadata == metadata;
}

static bool sign_set(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                     int32_t metadata) {
    bool changed = false;
    return mc_world_runtime_block_metadata_set_quiet(world, x, y, z, id, metadata, &changed,
                                                     NULL) == MC_WORLD_STORAGE_OK;
}

static bool sign_call(mc_world_runtime *world, const mc_block *block,
                      mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_mutation_error_init(error);
    const bool completed = mc_world_mutation_block_neighbor(world, block, 12, 64, 8, -923, error);
    mc_world_runtime_call_leave(world);
    return completed;
}

static bool sign_payload(mc_entity *item) {
    mc_item_stack *stack = NULL;
    mc_item_stack_fields fields;
    const bool same = mc_item_entity_stack_get(item, &stack) == MC_ENTITY_OK &&
                      mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
                      fields.id == 323 && fields.count == 1 && fields.damage == 0;
    mc_item_stack_release(stack);
    return same;
}

typedef struct sign_observer {
    size_t added, cells;
    bool drop_before_air;
} sign_observer;

static mc_world_storage_status sign_added(void *context, mc_world_runtime *world, mc_entity *item,
                                          mc_world_storage_error *error) {
    (void)error;
    sign_observer *observer = context;
    ++observer->added;
    observer->drop_before_air = sign_cell(world, 68, 2) && sign_payload(item);
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status sign_cell_failure(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z,
                                                 mc_world_storage_error *error) {
    sign_observer *observer = context;
    ++observer->cells;
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "Sign reached cell observer", 57};
    if (x != 12 || y != 64 || z != 8 || !sign_cell(world, 0, 0))
        error->status = MC_WORLD_STORAGE_INVALID_DATA;
    return error->status;
}

static bool sign_null_world_removal_controls(fixture *f) {
    bool ok = false;
    mc_world_mutation_error *error = malloc(sizeof(*error));
    const int32_t ids[] = {63, 68};
    CHECK(error != NULL);
    for (size_t entry = 0; entry < 2; ++entry) {
        for (size_t i = 0; i < COUNT(ids); ++i) {
            const mc_block *block = f->registry[0]->blocks.m[ids[i]];
            CHECK(block != NULL && block->kind == MC_BLOCK_SIGN);
            mc_world_mutation_error_init(error);
            /* Ordinary native admission fails before the RW body. This is
             * distinct from the retained execution's actual source-null RW14. */
            const bool completed =
                entry == 0 ? mc_world_mutation_block_removed(NULL, block, 12, 64, 8, error)
                           : mc_world_mutation_block_removed_with_code(f->classes[0], NULL, block,
                                                                       12, 64, 8, error);
            CHECK(!completed && error->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  error->operation != NULL &&
                  strcmp(error->operation, "TileEntity World native admission") == 0 &&
                  error->cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  error->cause.operation != NULL &&
                  strcmp(error->cause.operation, "TileEntity World native admission") == 0 &&
                  error->cause.system_error == 0);
            CHECK(error->hook_class != NULL && strcmp(error->hook_class, "rw") == 0 &&
                  error->hook_method == NULL && error->hook_bytecode == 0 && error->bytecode == 0 &&
                  error->chunk_bytecode == 0 && error->provider_bytecode == 0 &&
                  !error->sign_source_null);
            CHECK(error->tile_entity.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  error->tile_entity.operation != NULL &&
                  strcmp(error->tile_entity.operation, "TileEntity World native admission") == 0 &&
                  error->tile_entity.cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  error->tile_entity.cause.operation != NULL &&
                  strcmp(error->tile_entity.cause.operation, "TileEntity World native admission") ==
                      0 &&
                  error->tile_entity.cause.system_error == 0 && error->tile_entity.bytecode == 0 &&
                  error->tile_entity.chunk_bytecode == 0 &&
                  error->tile_entity.world_access_bytecode == 0 &&
                  error->tile_entity.initializer_bytecode == 0 &&
                  error->tile_entity.caller_bytecode == 0 &&
                  error->tile_entity.hook_bytecode == 0 &&
                  error->tile_entity.remove_bytecode == 0 &&
                  error->tile_entity.provider_bytecode == 0 &&
                  error->tile_entity.tile_entity.status == MC_TILE_ENTITY_OK);
        }
    }
    ok = true;
cleanup:
    free(error);
    return ok;
}

static bool sign_controls(fixture *f) {
    bool ok = false, registered = false;
    mc_world_runtime *world = f->worlds[0];
    const mc_block *standing = f->registry[0]->blocks.m[63];
    const mc_block *wall = f->registry[0]->blocks.m[68];
    mc_block *glass = f->registry[0]->blocks.m[20];
    mc_world_mutation_error *error = malloc(sizeof(*error));
    mc_tile_entity *held = NULL;
    mc_world_access *access = NULL;
    sign_observer observer = {0};
    const int32_t xs[] = {12, 12, 13, 11};
    const int32_t zs[] = {9, 7, 8, 8};
    CHECK(error != NULL && standing != NULL && wall != NULL && glass != NULL &&
          standing->kind == MC_BLOCK_SIGN && wall->kind == MC_BLOCK_SIGN &&
          standing->subtype.uj.b && !wall->subtype.uj.b && mc_material_a(glass->fields.bA));
    CHECK(sign_set(world, 12, 63, 8, 20, 0) && sign_set(world, 12, 64, 8, 63, 9));
    CHECK(mc_live_tile_entities_get(world, 12, 64, 8, &held, NULL) == MC_WORLD_STORAGE_OK &&
          held != NULL);
    mc_tile_entity_snapshot tile;
    CHECK(mc_tile_entity_get_snapshot(held, &tile) == MC_TILE_ENTITY_OK &&
          tile.kind == MC_TILE_ENTITY_SIGN && !tile.invalid && tile.world == world);
    const size_t initial_items = world->fields.b->count;
    /* Standing support reaches Material.a without reading Sign metadata. */
    mc_world_chunk_nibble *const nibble = f->chunks[0]->e;
    f->chunks[0]->e = NULL;
    const bool standing_without_metadata = sign_call(world, standing, error);
    f->chunks[0]->e = nibble;
    CHECK(standing_without_metadata && sign_cell(world, 63, 9) &&
          world->fields.b->count == initial_items && error->hook_class == NULL);
    /* A nullable material field is returned successfully by World.f, then
     * fails at the actual Material.a receiver20, before metadata or drops. */
    mc_material *const material = glass->fields.bA;
    glass->fields.bA = NULL;
    const bool null_material = sign_call(world, standing, error);
    glass->fields.bA = material;
    CHECK(!null_material && error->sign_source_null && error->hook_bytecode == 20 &&
          strcmp(error->hook_class, "uj") == 0 && sign_cell(world, 63, 9) &&
          world->fields.b->count == initial_items);
    CHECK(sign_set(world, 12, 63, 8, 0, 0));
    CHECK(sign_call(world, standing, error) && sign_cell(world, 0, 0) &&
          mc_tile_entity_get_snapshot(held, &tile) == MC_TILE_ENTITY_OK && tile.invalid &&
          world->fields.b->count == initial_items + 1u &&
          sign_payload(world->fields.b->items[initial_items]));
    mc_tile_entity_release(held);
    held = NULL;
    for (size_t i = 0; i < COUNT(xs); ++i)
        CHECK(sign_set(world, xs[i], 64, zs[i], 20, 0));
    /* Full metadata equality, with real glass Material support. */
    for (int32_t metadata = 0; metadata < 16; ++metadata) {
        CHECK(sign_set(world, 12, 64, 8, 68, metadata));
        const size_t before = world->fields.b->count;
        const bool supported = metadata >= 2 && metadata <= 5;
        CHECK(sign_call(world, wall, error) &&
              sign_cell(world, supported ? 68 : 0, supported ? metadata : 0) &&
              world->fields.b->count == before + (supported ? 0u : 1u) &&
              error->status == MC_WORLD_STORAGE_OK && error->hook_class == NULL &&
              error->hook_method == NULL && error->hook_bytecode == 0 && world->active_calls == 0 &&
              world->local_leases == 0);
        if (!supported)
            CHECK(sign_payload(world->fields.b->items[before]));
    }
    for (size_t i = 0; i < COUNT(xs); ++i) {
        CHECK(sign_set(world, xs[i], 64, zs[i], 0, 0) &&
              sign_set(world, 12, 64, 8, 68, (int32_t)i + 2));
        const size_t before = world->fields.b->count;
        CHECK(sign_call(world, wall, error) && sign_cell(world, 0, 0) &&
              world->fields.b->count == before + 1u &&
              sign_payload(world->fields.b->items[before]));
        CHECK(sign_set(world, xs[i], 64, zs[i], 20, 0));
    }
    CHECK(sign_set(world, 12, 64, 8, 68, 2));
    f->chunks[0]->e = NULL;
    const bool failed_metadata = sign_call(world, wall, error);
    f->chunks[0]->e = nibble;
    CHECK(!failed_metadata && error->hook_bytecode == 37 && !error->sign_source_null &&
          error->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && sign_cell(world, 68, 2));
    glass->fields.bA = NULL;
    const bool null_wall_material = sign_call(world, wall, error);
    glass->fields.bA = material;
    CHECK(!null_wall_material && error->sign_source_null && error->hook_bytecode == 61 &&
          sign_cell(world, 68, 2));
    mc_world_mutation_error_init(error);
    CHECK(!mc_world_mutation_sign_neighbor(NULL, standing, 12, INT32_MIN, 8, 0, error) &&
          error->sign_source_null && error->hook_bytecode == 17);
    mc_world_mutation_error_init(error);
    CHECK(!mc_world_mutation_sign_neighbor(NULL, wall, 12, 64, 8, 0, error) &&
          error->sign_source_null && error->hook_bytecode == 37);
    /* The drop's real Item is published before notifying removal. A later
     * cell callback failure preserves the removed Sign and the published Item. */
    CHECK(sign_set(world, 12, 64, 9, 0, 0));
    const mc_world_access_callbacks callbacks = {
        .cell = sign_cell_failure, .range = plate_sound_range, .entity_added = sign_added};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    const size_t before = world->fields.b->count;
    CHECK(!sign_call(world, wall, error) && error->status == MC_WORLD_STORAGE_IO_ERROR &&
          error->cause.system_error == 57 && strcmp(error->hook_class, "uj") == 0 &&
          error->hook_bytecode == 173 && observer.added == 1 && observer.cells == 1 &&
          observer.drop_before_air && sign_cell(world, 0, 0) &&
          world->fields.b->count == before + 1u && world->active_calls == 0 &&
          world->local_leases == 0);
    ok = true;
cleanup:
    mc_tile_entity_release(held);
    if (registered) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(world, access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            ok = false;
    }
    if (mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    free(error);
    return ok;
}

typedef struct sign_execution_fixture {
    mc_entity_tick_execution execution;
    mc_entity_tick_execution_world_token world;
    mc_world_runtime_execution_result result;
    mc_world_mutation_execution_call *call;
    mc_world_mutation_error error;
    mc_world_storage_status raw;
    mc_math_execution math;
    mc_math_classes *math_owner;
} sign_execution_fixture;

static const mc_entity_tick_execution_site sign_execution_site = {"native.sign.control", "run",
                                                                  "()V", 0};
static const mc_world_runtime_execution_code sign_execution_code = {{"lm", "e", "()V", 6},
                                                                    {"lm", "e", "()V", 33}};

static bool sign_execution_begin(sign_execution_fixture *s, mc_world_runtime *world) {
    mc_math_execution_init(&s->math, s->math_owner);
    mc_entity_tick_execution_init(&s->execution, &s->math);
    mc_world_runtime_execution_result_init(&s->result);
    mc_world_mutation_error_init(&s->error);
    s->raw = MC_WORLD_STORAGE_LIMIT;
    return mc_entity_tick_execution_push_world(&s->execution, world, sign_execution_site,
                                               &s->world) == MC_WORLD_STORAGE_OK;
}

static bool sign_execution_end(sign_execution_fixture *s, bool failed) {
    return mc_world_mutation_execution_native_close(s->call) == MC_WORLD_STORAGE_OK &&
           mc_world_mutation_execution_retire(&s->call) == MC_WORLD_STORAGE_OK &&
           mc_entity_tick_execution_pop_world(&s->execution, &s->world, failed) ==
               MC_WORLD_STORAGE_OK &&
           mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
           mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK;
}

static bool sign_execution_controls(fixture *f) {
    bool ok = false;
    sign_execution_fixture *s = calloc(1, sizeof(*s));
    mc_world_runtime *world = f->worlds[0];
    mc_tile_entity *held = NULL;
    const mc_block *standing = f->registry[0]->blocks.m[63];
    CHECK(s != NULL && mc_math_classes_create(SIZE_MAX, &s->math_owner, NULL) == MC_MATH_CLASS_OK);
    CHECK(sign_set(world, 12, 64, 8, 0, 0) && sign_set(world, 12, 63, 8, 20, 0));
    const size_t active = world->fields.c->count;
    CHECK(sign_execution_begin(s, world));
    mc_world_mutation_execution_context context = {.execution = &s->execution,
                                                   .code_classes = f->classes[0],
                                                   .code = sign_execution_code,
                                                   .result = &s->result,
                                                   .world = s->world};
    bool changed = false;
    CHECK(mc_world_runtime_block_set_in_execution(&context, world, 12, 64, 8, 63, &changed,
                                                  &s->call, &s->raw, &s->error) &&
          changed && s->call == NULL && s->raw == MC_WORLD_STORAGE_OK && sign_cell(world, 63, 0) &&
          world->fields.c->count == active + 1u &&
          s->result.ownership_status == MC_WORLD_STORAGE_OK && !s->result.selection_ready &&
          sign_execution_end(s, false));
    CHECK(mc_live_tile_entities_get(world, 12, 64, 8, &held, NULL) == MC_WORLD_STORAGE_OK &&
          held != NULL);
    mc_tile_entity_snapshot tile;
    CHECK(mc_tile_entity_get_snapshot(held, &tile) == MC_TILE_ENTITY_OK &&
          tile.kind == MC_TILE_ENTITY_SIGN && !tile.invalid);
    CHECK(sign_execution_begin(s, world));
    context.world = s->world;
    CHECK(mc_world_runtime_block_set_in_execution(&context, world, 12, 64, 8, 0, &changed, &s->call,
                                                  &s->raw, &s->error) &&
          changed && s->call == NULL && s->raw == MC_WORLD_STORAGE_OK && sign_cell(world, 0, 0) &&
          mc_tile_entity_get_snapshot(held, &tile) == MC_TILE_ENTITY_OK && tile.invalid &&
          world->fields.c->count == active && sign_execution_end(s, false));
    mc_tile_entity_release(held);
    held = NULL;
    /* Retained neighbor support uses the real material child after a notifying
     * write beside the Sign. Successful inherited removal consumes its Tile. */
    CHECK(sign_set(world, 12, 64, 8, 63, 0) && sign_execution_begin(s, world));
    context.world = s->world;
    CHECK(mc_world_runtime_metadata_set_in_execution(&context, world, 12, 63, 8, 1, &s->call,
                                                     &s->raw, &s->error) &&
          s->call == NULL && s->raw == MC_WORLD_STORAGE_OK && sign_cell(world, 63, 0) &&
          sign_execution_end(s, false));
    CHECK(sign_execution_begin(s, world));
    context.world = s->world;
    const size_t items = world->fields.b->count;
    CHECK(mc_world_runtime_block_set_in_execution(&context, world, 12, 63, 8, 0, &changed, &s->call,
                                                  &s->raw, &s->error) &&
          changed && s->call == NULL && s->raw == MC_WORLD_STORAGE_OK && sign_cell(world, 0, 0) &&
          world->fields.b->count == items + 1u && sign_payload(world->fields.b->items[items]) &&
          world->fields.c->count == active && sign_execution_end(s, false));
    /* Leave the genuine Sign block while removing its real installed Tile.
     * The next retained RW removal must execute lm.d's missing-Map factory,
     * install that fresh Sign, then remove it through the same World.p body. */
    CHECK(sign_set(world, 12, 63, 8, 20, 0) && sign_set(world, 12, 64, 8, 63, 0) &&
          mc_live_tile_entities_remove(world, 12, 64, 8, NULL) == MC_WORLD_STORAGE_OK &&
          world->fields.c->count == active && sign_cell(world, 63, 0) &&
          sign_execution_begin(s, world));
    context.world = s->world;
    CHECK(mc_world_mutation_execution_removed(&context, world, standing, 12, 64, 8, &s->call,
                                              &s->error, sign_execution_site) &&
          s->call == NULL && world->fields.c->count == active && f->chunks[0]->l->count == 0 &&
          sign_cell(world, 63, 0) && sign_execution_end(s, false));
    /* The same missing-Map route captures actual NULL Chunk.d at lm.d67,
     * reaches the genuine Sign constructor, then fails at RW.c's World18.
     * Its failed child and copied capture site remain inspectable through
     * checked retirement. Restoring Chunk.d is teardown, never Source retry. */
    CHECK(sign_execution_begin(s, world));
    context.world = s->world;
    mc_world_runtime *const chunk_world = f->chunks[0]->d;
    f->chunks[0]->d = NULL;
    const bool lazy_removed = mc_world_mutation_execution_removed(
        &context, world, standing, 12, 64, 8, &s->call, &s->error, sign_execution_site);
    f->chunks[0]->d = chunk_world;
    const mc_live_tile_entity_query_execution_call *lazy =
        mc_world_mutation_execution_failed_tile_removed_lookup(s->call);
    const mc_world_mutation_error *addition = mc_live_tile_entity_query_execution_failure(lazy);
    mc_entity_tick_execution_site capture, source_null;
    CHECK(!lazy_removed && lazy != NULL && addition != NULL &&
          mc_live_tile_entity_query_execution_capture_site(lazy, &capture) &&
          strcmp(capture.class_name, "lm") == 0 && strcmp(capture.method, "d") == 0 &&
          strcmp(capture.descriptor, "(III)Low;") == 0 && capture.pc == 67 &&
          strcmp(addition->hook_class, "rw") == 0 && addition->hook_bytecode == 18 &&
          mc_world_mutation_execution_source_null(s->call, &source_null) &&
          strcmp(source_null.class_name, "rw") == 0 && strcmp(source_null.method, "c") == 0 &&
          source_null.pc == 18 && world->fields.c->count == active && f->chunks[0]->l->count == 0 &&
          sign_cell(world, 63, 0) && sign_execution_end(s, true));
    /* A real failed World lookup retains its typed removal prefix. Restoring
     * the prepared Source field is teardown only; the failed body never resumes. */
    CHECK(sign_set(world, 12, 63, 8, 20, 0) && sign_set(world, 12, 64, 8, 63, 0) &&
          sign_execution_begin(s, world));
    context.world = s->world;
    mc_chunk_provider *const provider = world->fields.v;
    world->fields.v = NULL;
    const bool removed = mc_world_mutation_execution_removed(
        &context, world, standing, 12, 64, 8, &s->call, &s->error, sign_execution_site);
    world->fields.v = provider;
    mc_world_storage_status raw = MC_WORLD_STORAGE_LIMIT;
    const mc_live_tile_entity_error *detail =
        mc_world_mutation_execution_failed_tile_removed(s->call, &raw);
    CHECK(!removed && s->call != NULL && detail != NULL &&
          raw == MC_WORLD_STORAGE_UNSUPPORTED_STATE && detail->remove_bytecode == 4 &&
          detail->bytecode == 7 && detail->provider_bytecode == 6 &&
          detail->caller_bytecode == 14 && s->error.hook_bytecode == 14 &&
          strcmp(s->error.hook_class, "rw") == 0 &&
          s->result.ownership_status == MC_WORLD_STORAGE_OK && sign_cell(world, 63, 0) &&
          sign_execution_end(s, true));
    CHECK(sign_set(world, 12, 64, 8, 0, 0) && sign_execution_begin(s, NULL));
    context.world = s->world;
    CHECK(!mc_world_mutation_execution_removed(&context, NULL, standing, 12, 64, 8, &s->call,
                                               &s->error, sign_execution_site));
    CHECK(s->call != NULL && mc_world_mutation_execution_source_null(s->call, &source_null) &&
          strcmp(source_null.class_name, "rw") == 0 && strcmp(source_null.method, "b") == 0 &&
          source_null.pc == 14 && sign_execution_end(s, true));
    CHECK(sign_execution_begin(s, NULL));
    context.world = s->world;
    CHECK(!mc_world_mutation_execution_added(&context, NULL, standing, 12, 64, 8, &s->call,
                                             &s->error, sign_execution_site));
    CHECK(s->call != NULL && mc_world_mutation_execution_source_null(s->call, &source_null) &&
          strcmp(source_null.class_name, "rw") == 0 && strcmp(source_null.method, "c") == 0 &&
          source_null.pc == 18 && sign_execution_end(s, true));
    ok = true;
cleanup:
    mc_tile_entity_release(held);
    if (s != NULL &&
        (s->call != NULL || mc_math_classes_release(&s->math_owner, NULL) != MC_MATH_CLASS_OK))
        return false;
    free(s);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'S', 'i', 'g', 'n'};
    if (argc != 3 || strcmp(argv[1], "world-sign") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
              MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(plate_open_store(f, slot, root) && plate_world(f, slot));
    CHECK(sign_null_world_removal_controls(f) && sign_controls(f) && sign_execution_controls(f));
    result = 0;
cleanup:
    if (!plate_cleanup(f)) {
        fputs("Sign cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u Sign lifecycle checks (world-sign)\n", checks) < 0)
        result = 1;
    return result;
}
