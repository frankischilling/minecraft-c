/* Genuine Registry, statistics, World, Session and LocalPlayer constructors.
 * The imported construction executable is never run by this component. */
#define main mc_item_use_construction_fixture_main
#include "../../entity/player/test_construction.c"
#undef main

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/item/render/internal.h"
#include "../../../src/item/stack/source_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/runtime/world_access_internal.h"
#include "../../../src/world/storage/generated_internal.h"
#include "mc/item/use/use.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/placement.h"
#include "mc/world/runtime/world_access.h"

#include <limits.h>

#ifdef MC_PLACEMENT_REAL_RENDER_GLOBAL
#include "client/render_engine/internal.h"
#include "client/world/render_global_access.h"
#include "client/world/render_global_internal.h"
#endif

/* Constructed contracts, not original-JAR observations. Authored Chunk bytes,
 * source nullable projections and native WorldAccess callbacks are fixture
 * input. Every Item, Player, World getter/setter, hook, sound loop and receipt
 * under test is the genuine implementation. Temporary projections are restored
 * before assertions; no game return, count store or phase is synthesized. */
typedef struct use_observer {
    mc_registry_classes *registry;
    mc_item_stack *stack;
    mc_block *replacement;
    int32_t before, sound_count, sound_id, mutate_count;
    size_t cells, ranges, sounds;
    double x, y, z;
    float volume, pitch;
    bool valid, fail_sound, change_count, append_null_sound, null_added;
    bool fail_cell;
    mc_world_access *access;
    mc_world_access_receipt *receipt;
    mc_world_access *inner;
    mc_world_access_failure inner_failure;
} use_observer;

static mc_world_storage_status use_cell(void *context, mc_world_runtime *world, int32_t x,
                                        int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)error;
    use_observer *o = context;
    ++o->cells;
    if (o->replacement != NULL)
        o->registry->blocks.m[3] = o->replacement;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status use_range(void *context, mc_world_runtime *world, int32_t x,
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
    ++((use_observer *)context)->ranges;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status use_sound(void *context, mc_world_runtime *world,
                                         const mc_registry_string *name, double x, double y,
                                         double z, float volume, float pitch,
                                         mc_world_storage_error *error) {
    use_observer *o = context;
    ++o->sounds;
    o->x = x;
    o->y = y;
    o->z = z;
    o->volume = volume;
    o->pitch = pitch;
    o->valid = o->valid && o->cells != 0 && name != NULL && name->length == 11 &&
               mc_item_stack_source_count_get(o->stack, &o->sound_count) == MC_ITEM_STACK_OK &&
               o->sound_count == o->before &&
               mc_world_runtime_block_get(world, (int32_t)x, (int32_t)y, (int32_t)z, &o->sound_id,
                                          NULL) == MC_WORLD_STORAGE_OK &&
               o->sound_id == 3;
    static const char text[] = "step.gravel";
    for (size_t i = 0; name != NULL && i < name->length && i < sizeof text - 1; ++i)
        o->valid = o->valid && name->units[i] == (uint16_t)text[i];
    if (o->change_count &&
        mc_item_stack_source_count_set(o->stack, o->mutate_count) != MC_ITEM_STACK_OK)
        o->valid = false;
    if (o->fail_sound) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "authored sound refusal", 0};
        return MC_WORLD_STORAGE_LIMIT;
    }
    if (o->append_null_sound) {
        const mc_world_storage_status status =
            mc_world_runtime_add_world_access(world, NULL, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        o->null_added = true;
    }
    return MC_WORLD_STORAGE_OK;
}

#ifdef MC_PLACEMENT_REAL_RENDER_GLOBAL
/* Authored nested producer contract around an actually executed cold N sound
 * body. Both receipts own their own producer identity. The outer tag is copied
 * only from the reached inner receipt API, never a fabricated exception flag. */
static mc_world_storage_status use_receipt(use_observer *o, mc_world_runtime *world,
                                           const mc_registry_string *name, double x, double y,
                                           double z, float volume, float pitch,
                                           mc_world_access_failure *failure,
                                           mc_world_storage_error *error) {
    mc_world_access_failure inner = {0};
    mc_world_storage_error child = {0};
    const mc_world_storage_status status =
        mc_world_access_call_sound(o->inner, world, name, x, y, z, volume, pitch, &child, &inner);
    if (status == MC_WORLD_STORAGE_OK)
        return status;
    mc_world_storage_status captured = mc_world_access_failure_retain(&inner, &o->inner_failure);
    if (captured != MC_WORLD_STORAGE_OK) {
        *error = (mc_world_storage_error){captured, "nested actual N producer occurrence", 0};
        return captured;
    }
    captured =
        mc_world_access_receipt_create(o->access, inner.render_global, &o->receipt, failure, error);
    if (captured != MC_WORLD_STORAGE_OK)
        return captured;
    const char *exception_class = NULL;
    if (mc_world_access_failure_source_exception(&o->inner_failure, &exception_class))
        o->receipt->exception_class = exception_class;
    *error = child;
    return status;
}

static mc_world_storage_status use_source_cell(void *context, mc_world_runtime *world, int32_t x,
                                               int32_t y, int32_t z, mc_world_storage_error *error,
                                               mc_world_access_failure *failure) {
    use_observer *o = context;
    const mc_world_storage_status status = use_cell(context, world, x, y, z, error);
    return status == MC_WORLD_STORAGE_OK && o->fail_cell
               ? use_receipt(o, world, NULL, (double)x, (double)y, (double)z, 1, 1, failure, error)
               : status;
}

static mc_world_storage_status use_source_range(void *context, mc_world_runtime *world, int32_t x,
                                                int32_t y, int32_t z, int32_t max_x, int32_t max_y,
                                                int32_t max_z, mc_world_storage_error *error,
                                                mc_world_access_failure *failure) {
    (void)failure;
    return use_range(context, world, x, y, z, max_x, max_y, max_z, error);
}

static mc_world_storage_status use_source_sound(void *context, mc_world_runtime *world,
                                                const mc_registry_string *name, double x, double y,
                                                double z, float volume, float pitch,
                                                mc_world_storage_error *error,
                                                mc_world_access_failure *failure) {
    const mc_world_storage_status status =
        use_sound(context, world, name, x, y, z, volume, pitch, error);
    return status == MC_WORLD_STORAGE_OK
               ? status
               : use_receipt(context, world, name, x, y, z, volume, pitch, failure, error);
}
#endif

static bool use_stack(int32_t id, int32_t count, int32_t damage, mc_item_stack **out) {
    CHECK(mc_item_stack_allocate(out) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_construct(*out, id, count, damage) == MC_ITEM_STACK_OK);
    return true;
}

static bool use_retire(mc_item_use_call **call) {
    CHECK(mc_item_use_retire(call, NULL) == MC_ITEM_USE_OK && *call == NULL);
    return true;
}

/* A real absent resource stream reaches the NH InputStreamReader null
 * invocation during the actual JL bootstrap. No class failure is injected. */
static mc_registry_status use_absent_resource(void *context, const char *name,
                                              mc_java_properties_input *out) {
    (void)name;
    (void)out;
    ++*(size_t *)context;
    return MC_REGISTRY_NULL_REFERENCE;
}

static bool use_initializer_controls(fixture *f) {
    size_t opens = 0;
    const mc_registry_resources resources = {.context = &opens, .open = use_absent_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         &resources};
    mc_registry_classes *registry = NULL;
    mc_registry_error first, cached;
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_statistics(registry, &first) ==
          MC_REGISTRY_NULL_REFERENCE);
    CHECK(opens == 1 && registry->statistics_state.phase == MC_REGISTRY_FAILED &&
          registry->translation_state.phase == MC_REGISTRY_FAILED &&
          registry->blocks_phase == MC_REGISTRY_COLD && first.active_initializer_class != NULL &&
          strcmp(first.active_initializer_class, "nh") == 0);
    mc_item_render_throwable throwable;
    const char *name = NULL;
    mc_item_render_registry_classification(registry, &first, &throwable, &name);
    CHECK(throwable == MC_ITEM_RENDER_SOURCE_ERROR &&
          strcmp(name, "java/lang/ExceptionInInitializerError") == 0);
    CHECK(mc_registry_classes_initialize_statistics(registry, &cached) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    mc_item_render_registry_classification(registry, &cached, &throwable, &name);
    CHECK(opens == 1 && throwable == MC_ITEM_RENDER_SOURCE_ERROR &&
          strcmp(name, "java/lang/NoClassDefFoundError") == 0 && first.message == cached.message &&
          first.active_initializer_pc == cached.active_initializer_pc &&
          mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    return true;
}

static bool literal_and_dispatch(fixture *f) {
    mc_item_use_call *call = NULL;
    mc_item_use_state s;
    mc_item_use_error e;
    mc_item_stack *stack = NULL;
    CHECK(use_stack(262, -7, INT32_MIN, &stack)); /* Actual base GM Arrow. */
    CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, NULL, 1, 2, 3, INT32_MIN, &call,
                                     &e) == MC_ITEM_USE_OK);
    CHECK(mc_item_use_inspect(call, &s) && s.returned && !s.result && !s.count_read &&
          !s.predicate_entered && !s.statistic_entered && s.source_pc == 39);
    CHECK(use_retire(&call));
    mc_item_stack_release(stack);
    stack = NULL;

    CHECK(use_stack(3, 0, 239, &stack));
    CHECK(mc_item_use_in_air_begin(f->registry, stack, NULL, NULL, &call, &e) == MC_ITEM_USE_OK);
    CHECK(mc_item_use_inspect(call, &s) && s.returned && s.result_stack == stack && !s.count_read &&
          !s.statistic_entered && s.source_pc == 10);
    /* Drop the input and retain the actual returned occurrence independently. */
    mc_item_stack_release(stack);
    stack = s.result_stack;
    CHECK(mc_item_stack_retain(stack) == MC_ITEM_STACK_OK && use_retire(&call));
    int32_t count;
    CHECK(mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK && count == 0);
    CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, NULL, 1, 2, 3, 1, &call, &e) ==
          MC_ITEM_USE_SOURCE_NULL);
    CHECK(e.site.pc == 7 && strcmp(e.site.class_name, "ck") == 0 && e.source_exception &&
          !e.native_policy && mc_item_use_inspect(call, &s) && !s.count_read);
    CHECK(use_retire(&call));
    mc_item_stack_release(stack);

    CHECK(mc_item_use_on_block_begin(f->registry, NULL, NULL, NULL, 0, 0, 0, 0, &call, &e) ==
          MC_ITEM_USE_SOURCE_NULL);
    CHECK(e.site.pc == 1 && strcmp(e.site.class_name, "iz") == 0 && use_retire(&call));
    CHECK(use_stack(0, 1, 0, &stack));
    CHECK(mc_item_use_in_air_begin(f->registry, stack, NULL, NULL, &call, &e) ==
          MC_ITEM_USE_SOURCE_NULL);
    CHECK(e.site.pc == 7 && e.source_exception && !e.native_policy && use_retire(&call));
    mc_item_stack_release(stack);
    CHECK(use_stack(259, 0, 0, &stack)); /* YE overrides use-on-block, before World read. */
    CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, NULL, 0, 0, 0, 0, &call, &e) ==
          MC_ITEM_USE_UNSUPPORTED);
    CHECK(e.site.pc == 14 && e.child.pc == 0 && strcmp(e.child.class_name, "ye") == 0 &&
          e.native_policy && !e.source_exception && use_retire(&call));
    mc_item_stack_release(stack);
    CHECK(use_stack(261, 0, 0, &stack)); /* QZ's actual in-air override. */
    CHECK(mc_item_use_in_air_begin(f->registry, stack, NULL, NULL, &call, &e) ==
          MC_ITEM_USE_UNSUPPORTED);
    CHECK(e.site.pc == 7 && strcmp(e.child.class_name, "qz") == 0 && use_retire(&call));
    mc_item_stack_release(stack);
    return true;
}

/* RK's non-UP gate precedes every World/Player/count access. The real IZ
 * wrapper still resolves the registered Seeds receiver, then returns false. */
static bool seeds_face_controls(fixture *f) {
    static const int32_t faces[] = {0, 2, 3, 4, 5, 6, INT32_MIN, INT32_MAX};
    mc_item_stack *stack = NULL;
    CHECK(use_stack(295, -7, INT32_MIN, &stack));
    for (size_t i = 0; i < sizeof faces / sizeof *faces; ++i) {
        mc_item_use_call *call = NULL;
        mc_item_use_error error;
        mc_item_use_state state;
        const mc_item_use_status status =
            mc_item_use_on_block_begin(f->registry, stack, NULL, NULL, INT32_MAX, INT32_MIN,
                                       INT32_MAX, faces[i], &call, &error);
        if (status != MC_ITEM_USE_OK)
            fprintf(stderr, "Seeds non-UP face=%d status=%d site=%s:%u child=%s:%u\n", faces[i],
                    (int)status, error.site.class_name == NULL ? "none" : error.site.class_name,
                    error.site.pc, error.child.class_name == NULL ? "none" : error.child.class_name,
                    error.child.pc);
        CHECK(status == MC_ITEM_USE_OK && mc_item_use_inspect(call, &state) && state.returned &&
              !state.result && !state.count_read && !state.setter_returned &&
              !state.statistic_entered && state.source_pc == 39);
        int32_t count;
        CHECK(mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK && count == -7 &&
              use_retire(&call));
    }
    mc_item_stack_release(stack);
    return true;
}

static bool adjustments_and_frontiers(fixture *f, mc_chunk_state *chunk) {
    mc_item_use_call *call = NULL;
    mc_item_use_state s;
    mc_item_use_error e;
    mc_item_stack *stack = NULL;
    CHECK(use_stack(3, 0, 0, &stack));
    static const int32_t delta[8][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1},
                                        {-1, 0, 0}, {1, 0, 0}, {0, 0, 0},  {0, 0, 0}};
    for (int32_t face = 0; face < 8; ++face) {
        CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 64, 8, face, &call,
                                         &e) == MC_ITEM_USE_OK);
        CHECK(mc_item_use_inspect(call, &s) && s.returned && !s.result && s.count_read &&
              !s.predicate_entered && !s.statistic_entered && s.target_x == 8 + delta[face][0] &&
              s.target_y == 64 + delta[face][1] && s.target_z == 8 + delta[face][2] &&
              s.target_face == face && use_retire(&call));
    }
    /* Out-of-domain clicked reads return source air, then coordinate arithmetic
     * still wraps. The zero count returns before any target-domain work. */
    CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, INT32_MAX, INT32_MIN,
                                     INT32_MIN, 5, &call, &e) == MC_ITEM_USE_OK);
    CHECK(mc_item_use_inspect(call, &s) && s.target_x == INT32_MIN && use_retire(&call));
    CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, INT32_MIN, INT32_MIN,
                                     INT32_MIN, 0, &call, &e) == MC_ITEM_USE_OK);
    CHECK(mc_item_use_inspect(call, &s) && s.target_y == INT32_MAX && use_retire(&call));
    const size_t index = ((size_t)8 * 16 + 8) * 128 + 64;
    chunk->data->blocks[index] = 78; /* Actual Snow ID; no placed hook is simulated. */
    CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 64, 8, 5, &call, &e) ==
          MC_ITEM_USE_OK);
    chunk->data->blocks[index] = 0;
    CHECK(mc_item_use_inspect(call, &s) && s.snow && s.target_face == 0 && s.target_x == 8 &&
          s.target_y == 64 && s.target_z == 8 && !s.predicate_entered && use_retire(&call));
    mc_item_stack_release(stack);

    CHECK(use_stack(3, 1, 0, &stack));
    CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 126, 8, 1, &call, &e) ==
          MC_ITEM_USE_OK);
    CHECK(mc_item_use_inspect(call, &s) && s.returned && !s.result && s.ceiling_checked &&
          !s.predicate_entered && !s.setter_returned && use_retire(&call));
    mc_item_stack_release(stack);

    /* Same inherited CK body for metadata subtypes. Count0 is handled before
     * their actual distinct metadata dispatch at CK159; it is not preflighted. */
    static const int32_t ids[] = {35, 44, 17, 6, 29, 18};
    static const int32_t metadata[] = {15, 15, 15, 15, 7, 15};
    for (size_t i = 0; i < sizeof ids / sizeof *ids; ++i) {
        CHECK(use_stack(ids[i], 0, 15, &stack));
        CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 64, 8, 6, &call,
                                         &e) == MC_ITEM_USE_OK);
        CHECK(mc_item_use_inspect(call, &s) && s.returned && !s.result && use_retire(&call));
        mc_item_stack_release(stack);
        CHECK(use_stack(ids[i], 1, 15, &stack));
        const size_t below = ((size_t)8 * 16 + 8) * 128 + 127;
        const uint8_t previous_below = chunk->data->blocks[below];
        if (i == 3)
            chunk->data->blocks[below] = 2; /* Declared Grass support below the y128 target. */
        const mc_item_use_status status =
            mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 128, 8, 6, &call, &e);
        chunk->data->blocks[below] = previous_below;
        /* The ordinary out-of-height setter returns false after the real
         * metadata dispatch, including the supported Sapling predicate. */
        CHECK(mc_item_use_inspect(call, &s) && !s.returned && s.predicate_entered &&
              !s.count_stored && s.predicate_returned);
        CHECK(status == MC_ITEM_USE_SOURCE_NULL && s.can_place && s.metadata_returned &&
              s.metadata == metadata[i] && s.damage == 15 && s.setter_returned && !s.setter_value &&
              s.result && e.site.pc == 34 && e.source_exception && !e.native_policy &&
              !s.sound_returned && !s.placed_face_returned);
        CHECK(use_retire(&call));
        mc_item_stack_release(stack);
    }
    return true;
}

static bool placement_metadata_controls(fixture *f, mc_chunk_state *chunk) {
    static const struct {
        int32_t id, damage, metadata;
    } cases[] = {{35, 13, 13}, {17, 2, 2}, {18, 1, 9}, {35, -1, -1}, {18, INT32_MIN, INT32_MIN + 8},
                 {6, 2, 2},    {37, -7, 0}};

    const size_t index = ((size_t)8 * 16 + 8) * 128 + 64;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        chunk->data->blocks[index] = 0;
        mc_item_stack *stack = NULL;
        mc_item_use_call *call = NULL;
        mc_item_use_state s;
        mc_item_use_error e;
        CHECK(use_stack(cases[i].id, 2, cases[i].damage, &stack));
        const uint8_t previous_below = chunk->data->blocks[index - 1];
        if (cases[i].id == 6 || cases[i].id == 37)
            chunk->data->blocks[index - 1] = 2;
        /* NULL Player fails only at IZ's statistic invocation, after the
         * complete placement, sound and count-store prefix has returned. */
        const mc_item_use_status status =
            mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 64, 8, 6, &call, &e);
        chunk->data->blocks[index - 1] = previous_below;
        CHECK(status == MC_ITEM_USE_SOURCE_NULL);
        int32_t id, stored, count;
        CHECK(mc_item_use_inspect(call, &s) && !s.returned && s.result && s.metadata_returned &&
              s.damage == cases[i].damage && s.metadata == cases[i].metadata && s.setter_returned &&
              s.setter_value && s.placed_face_returned && s.placed_by_returned &&
              s.sound_returned && s.count_stored && s.count_after == 1 && e.site.pc == 34 &&
              e.source_exception && !e.native_policy &&
              mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK && count == 1 &&
              mc_world_runtime_block_get(f->world, 8, 64, 8, &id, NULL) == MC_WORLD_STORAGE_OK &&
              id == cases[i].id &&
              mc_world_runtime_metadata_get(f->world, 8, 64, 8, &stored, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              stored == (cases[i].metadata & 15) && use_retire(&call));
        mc_item_stack_release(stack);
    }
    chunk->data->blocks[index] = 0;
    return true;
}

static bool placement_order(fixture *f, mc_chunk_state *chunk, mc_entity *player) {
    mc_item_use_call *call = NULL;
    mc_item_use_state s;
    mc_item_use_error e;
    mc_item_stack *stack = NULL;
    use_observer o = {.registry = f->registry, .valid = true};
    const mc_world_access_callbacks callbacks = {
        .cell = use_cell, .range = use_range, .sound = use_sound};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, &o, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    const size_t index = ((size_t)8 * 16 + 8) * 128 + 64;

    /* Ordinary setter false still returns CK true274, reaching IZ statistic34.
     * y128 is outside setter bounds but neither CK's y127 check nor UU's
     * source occupancy predicate rejects this coordinate. */
    CHECK(use_stack(3, 4, 99, &stack));
    o.stack = stack;
    o.before = 4;
    CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 128, 8, 6, &call, &e) ==
          MC_ITEM_USE_SOURCE_NULL);
    CHECK(mc_item_use_inspect(call, &s) && !s.returned && s.result && s.predicate_returned &&
          s.can_place && s.setter_returned && !s.setter_value && !s.placed_face_returned &&
          !s.sound_returned && !s.count_stored && e.site.pc == 34 && o.sounds == 0);
    CHECK(use_retire(&call));
    mc_item_stack_release(stack);

    /* A reached sound failure preserves the genuine notifying write and both
     * hooks, while the source count store and IZ statistic remain unreached. */
    CHECK(use_stack(3, 4, 99, &stack));
    o.stack = stack;
    o.before = 4;
    o.fail_sound = true;
    CHECK(mc_item_use_on_block_begin(f->registry, stack, player, f->world, 8, 64, 8, 6, &call,
                                     &e) == MC_ITEM_USE_WORLD);
    CHECK(mc_item_use_inspect(call, &s) && !s.returned && s.setter_returned && s.setter_value &&
          s.placed_face_returned && s.placed_by_returned && !s.sound_returned && !s.count_stored &&
          !s.statistic_entered && e.site.pc == 260 && e.native_policy && !e.source_exception &&
          !e.source_error && e.retained_producers == 0 && o.valid && o.sound_count == 4 &&
          chunk->data->blocks[index] == 3 && s.metadata == 0 && s.sound_x == 8.5 &&
          s.sound_y == 64.5 && s.sound_z == 8.5 && s.volume == 1.0f && s.pitch == 0.8f &&
          use_retire(&call));
    mc_item_stack_release(stack);
    chunk->data->blocks[index] = 0;

    /* Sound can mutate this real Stack. CK reload265 uses that late count,
     * then IZ's actual LocalPlayer reaches its nullable Minecraft field85. */
    CHECK(use_stack(3, -1, INT32_MIN, &stack));
    o.stack = stack;
    o.before = -1;
    o.fail_sound = false;
    o.change_count = true;
    o.mutate_count = 9;
    CHECK(mc_item_use_on_block_begin(f->registry, stack, player, f->world, 8, 64, 8, 6, &call,
                                     &e) == MC_ITEM_USE_PLAYER);
    int32_t count;
    CHECK(mc_item_use_inspect(call, &s) && !s.returned && s.result && s.sound_returned &&
          s.count_stored && s.count_after == 8 && s.statistic_entered && !s.statistic_returned &&
          e.site.pc == 34 && e.child.pc == 85 && e.source_exception && !e.native_policy &&
          mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK && count == 8 &&
          chunk->data->blocks[index] == 3 && use_retire(&call));
    mc_item_stack_release(stack);
    chunk->data->blocks[index] = 0;

    /* Actual GS's nullable statistic branch permits a normal IZ return.
     * This explicit nullable source-array fixture is restored before checking. */
    CHECK(use_stack(3, 2, 7, &stack));
    o.stack = stack;
    o.before = 2;
    o.change_count = false;
    mc_statistic *const saved_stat = f->registry->statistics.E[3];
    f->registry->statistics.E[3] = NULL;
    const mc_item_use_status normal =
        mc_item_use_on_block_begin(f->registry, stack, player, f->world, 8, 64, 8, 6, &call, &e);
    f->registry->statistics.E[3] = saved_stat;
    CHECK(normal == MC_ITEM_USE_OK && mc_item_use_inspect(call, &s) && s.returned && s.result &&
          s.sound_returned && s.count_stored && s.count_after == 1 && s.statistic_returned &&
          s.source_pc == 39 && o.valid && use_retire(&call));
    mc_item_stack_release(stack);
    chunk->data->blocks[index] = 0;

    /* A callback replacing current Block.m changes the later hook receiver,
     * while CK's earlier captured Block remains the genuine Dirt identity. */
    CHECK(use_stack(3, 2, 0, &stack));
    o.stack = stack;
    o.before = 2;
    o.replacement = f->registry->blocks.m[50];
    mc_block *const saved_block = f->registry->blocks.m[3];
    const mc_item_use_status replaced =
        mc_item_use_on_block_begin(f->registry, stack, player, f->world, 8, 64, 8, 6, &call, &e);
    f->registry->blocks.m[3] = saved_block;
    o.replacement = NULL;
    CHECK(replaced == MC_ITEM_USE_PLAYER && mc_item_use_inspect(call, &s) && !s.returned &&
          s.result && s.setter_returned && s.setter_value && s.placed_face_returned &&
          s.placed_by_returned && s.sound_returned && s.count_stored && s.count_after == 1 &&
          s.statistic_entered && !s.statistic_returned && e.site.pc == 34 && e.source_exception &&
          !e.native_policy && s.captured_block == saved_block && chunk->data->blocks[index] == 3 &&
          use_retire(&call));
    mc_item_stack_release(stack);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

/* The first real sound recipient appends a real NULL List occurrence. The
 * sound loop's next live lookup reaches that invocation after CK's real
 * setter/hooks, without exposing earlier cell/range loops to a null receiver. */
static bool placement_null_sound_controls(fixture *f, mc_chunk_state *chunk, mc_entity *player) {
    const size_t index = ((size_t)8 * 16 + 8) * 128 + 64;
    for (unsigned int mode = 0; mode < 2; ++mode) {
        chunk->data->blocks[index] = 0;
        mc_item_stack *stack = NULL;
        CHECK(use_stack(3, 4, 0, &stack));
        use_observer o = {.registry = f->registry,
                          .stack = stack,
                          .before = 4,
                          .valid = true,
                          .append_null_sound = true};
        const mc_world_access_callbacks callbacks = {
            .cell = use_cell, .range = use_range, .sound = mode == 0 ? NULL : use_sound};
        mc_world_access *access = NULL;
        CHECK(mc_world_access_create(&callbacks, &o, &access, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
        mc_item_use_call *call = NULL;
        mc_item_use_state state;
        mc_item_use_error error;
        CHECK(mc_item_use_on_block_begin(f->registry, stack, player, f->world, 8, 64, 8, 6, &call,
                                         &error) == MC_ITEM_USE_WORLD);
        bool removed = false;
        if (o.null_added)
            CHECK(mc_world_runtime_remove_world_access(f->world, NULL, &removed, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  removed);
        const char *exception = NULL;
        int32_t count = 0;
        const bool source =
            error.sound != NULL && mc_world_access_failure_source_exception(
                                       &error.sound->world_access_failure, &exception);
        const bool observed =
            mc_item_use_inspect(call, &state) && !state.returned && state.setter_returned &&
            state.setter_value && state.placed_face_returned && state.placed_by_returned &&
            !state.sound_returned && !state.count_stored && !state.statistic_entered &&
            error.site.pc == 260 && error.sound != NULL &&
            error.sound->world_access_bytecode == 41 && error.retained_producers == 0 &&
            error.source_exception == (mode != 0) && error.native_policy == (mode == 0) &&
            !error.source_error && source == (mode != 0) &&
            (mode == 0
                 ? exception == NULL
                 : exception != NULL && strcmp(exception, "java/lang/NullPointerException") == 0 &&
                       error.exception_class != NULL &&
                       strcmp(error.exception_class, exception) == 0) &&
            error.sound->world_access_failure.receiver == NULL &&
            error.sound->world_access_failure.render_global == NULL &&
            error.sound->world_access_failure.receipt == NULL && o.null_added == (mode != 0) &&
            o.sounds == (size_t)mode && o.valid &&
            mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK && count == 4 &&
            chunk->data->blocks[index] == 3 && f->world->active_calls == 0;
        CHECK(use_retire(&call) &&
              mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
        mc_item_stack_release(stack);
        if (!observed)
            fprintf(stderr, "CK null sound mode=%u pc=%u source=%u native=%u producers=%zu\n", mode,
                    error.site.pc, error.source_exception ? 1u : 0u, error.native_policy ? 1u : 0u,
                    error.retained_producers);
        CHECK(observed);
    }
    chunk->data->blocks[index] = 0;
    return true;
}

#ifdef MC_PLACEMENT_REAL_RENDER_GLOBAL
static bool placement_receipt_controls(fixture *f, mc_chunk_state *chunk, mc_entity *player) {
    const size_t index = ((size_t)8 * 16 + 8) * 128 + 64;
    for (size_t mode = 0; mode < 2; ++mode) {
        chunk->data->blocks[index] = 0;
        mc_item_stack *stack = NULL;
        CHECK(use_stack(3, 2, 0, &stack));
        use_observer o = {.registry = f->registry,
                          .stack = stack,
                          .before = 2,
                          .valid = true,
                          .fail_sound = mode == 0,
                          .fail_cell = mode == 1};
        mc_client_render_engine_classes *graphics = NULL;
        mc_client_render_global *owner = NULL;
        mc_client_render_global_error *n_error = calloc(1, sizeof(*n_error));
        CHECK(n_error != NULL &&
              mc_client_render_engine_classes_create(NULL, &graphics) ==
                  MC_CLIENT_RENDER_ENGINE_OK &&
              mc_client_render_global_allocate(graphics, NULL, &owner, n_error) ==
                  MC_CLIENT_RENDER_GLOBAL_OK &&
              mc_render_global_access_create(owner, &owner->access, n_error) ==
                  MC_CLIENT_RENDER_GLOBAL_OK &&
              mc_render_global_access_source_add(owner->access, f->world, n_error) ==
                  MC_CLIENT_RENDER_GLOBAL_OK);
        CHECK(f->world->world_accesses.count == 1);
        o.inner = f->world->world_accesses.entries[0];
        CHECK(o.inner != NULL &&
              mc_render_global_access_source_remove(owner->access, f->world, n_error) ==
                  MC_CLIENT_RENDER_GLOBAL_OK &&
              f->world->world_accesses.count == 0);
        const mc_world_access_source_callbacks callbacks = {
            .cell = use_source_cell, .range = use_source_range, .sound = use_source_sound};
        CHECK(mc_world_access_source_create(&callbacks, &o, &o.access, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(f->world, o.access, NULL) == MC_WORLD_STORAGE_OK);
        mc_item_use_call *call = NULL;
        mc_item_use_state s;
        mc_item_use_error e;
        CHECK(mc_item_use_on_block_begin(f->registry, stack, player, f->world, 8, 64, 8, 6, &call,
                                         &e) == MC_ITEM_USE_WORLD);
        CHECK(mc_item_use_inspect(call, &s) && !s.returned && !s.count_stored &&
              !s.statistic_entered && e.site.pc == (mode == 0 ? 260u : 162u) &&
              e.source_exception && !e.native_policy && !e.source_error &&
              strcmp(e.exception_class, "java/lang/NullPointerException") == 0 &&
              e.retained_producers == 1 && o.receipt != NULL && o.receipt->holds == 2 &&
              chunk->data->blocks[index] == 3 &&
              mc_world_access_receipt_retire(&o.receipt, NULL) == MC_WORLD_STORAGE_BUSY);
        CHECK((mode == 0 ? e.sound : e.mutation)->world_access_failure.receipt == o.receipt &&
              o.inner_failure.render_global != NULL &&
              o.inner_failure.render_global->status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
              o.inner_failure.render_global->source_pc == 22 &&
              strcmp(o.inner_failure.render_global->source_class, "n") == 0 &&
              strcmp(o.inner_failure.render_global->source_descriptor,
                     "(Ljava/lang/String;DDDFF)V") == 0 &&
              mc_render_global_access_retire(&owner->access, n_error) ==
                  MC_CLIENT_RENDER_GLOBAL_BUSY);
        CHECK(use_retire(&call) && o.receipt->holds == 1 &&
              mc_world_access_receipt_retire(&o.receipt, NULL) == MC_WORLD_STORAGE_OK &&
              o.receipt == NULL);
        mc_world_access_failure_release(&o.inner_failure);
        CHECK(mc_render_global_access_retire(&owner->access, n_error) ==
                  MC_CLIENT_RENDER_GLOBAL_OK &&
              mc_client_render_global_release(&owner, n_error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
        free(n_error);
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(f->world, o.access, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed && mc_world_access_free(o.access, NULL) == MC_WORLD_STORAGE_OK);
        mc_item_stack_release(stack);
    }
    return true;
}

static bool placement_real_n_control(fixture *f, mc_chunk_state *chunk, mc_entity *player) {
    const size_t index = ((size_t)8 * 16 + 8) * 128 + 64;
    chunk->data->blocks[index] = 0;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_render_global *owner = NULL;
    mc_client_render_global_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL &&
          mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_global_allocate(graphics, NULL, &owner, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    /* Authored source grid dimensions on a genuine RAW N. The real nullable
     * o field is left at its allocation default, so actual marking reaches
     * N's own aaload179 NPE. No constructor/GL work or diagnostic is fabricated. */
    owner->fields.p = owner->fields.q = owner->fields.r = 1;
    CHECK(mc_render_global_access_create(owner, &owner->access, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_source_add(owner->access, f->world, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    mc_item_stack *stack = NULL;
    CHECK(use_stack(3, 2, 0, &stack));
    mc_item_use_call *call = NULL;
    mc_item_use_state s;
    mc_item_use_error e;
    CHECK(mc_item_use_on_block_begin(f->registry, stack, player, f->world, 8, 64, 8, 6, &call,
                                     &e) == MC_ITEM_USE_WORLD);
    CHECK(mc_item_use_inspect(call, &s) && !s.returned && !s.count_stored && e.site.pc == 162 &&
          e.source_exception && !e.native_policy && !e.source_error && e.retained_producers == 1 &&
          e.mutation != NULL && e.mutation->world_access_failure.render_global != NULL &&
          e.mutation->world_access_failure.render_global->status ==
              MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          e.mutation->world_access_failure.render_global->source_pc == 179 &&
          strcmp(e.mutation->world_access_failure.render_global->source_class, "n") == 0 &&
          strcmp(e.mutation->world_access_failure.render_global->source_descriptor, "(IIIIII)V") ==
              0 &&
          chunk->data->blocks[index] == 3);
    CHECK(mc_render_global_access_detach_all(owner->access, error) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          use_retire(&call) &&
          mc_render_global_access_detach_all(owner->access, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_retire(&owner->access, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_global_release(&owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_item_stack_release(stack);
    free(error);
    return true;
}
#endif

#include "dye_controls.inc"
#include "farming_controls.inc"
#include "food_controls.inc"

static bool use_controls(fixture *f) {
    CHECK(use_initializer_controls(f) && literal_and_dispatch(f) && seeds_face_controls(f));
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL);
    mc_world_chunk *chunk = NULL;
    CHECK(mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(state != NULL && acquisition != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, state));
    CHECK(adjustments_and_frontiers(f, state) && placement_metadata_controls(f, state));
    mc_player_session *session = NULL;
    mc_entity *player = NULL;
    CHECK(mc_player_session_create(f->sessions, NULL, NULL, &session, NULL) == MC_PLAYER_OK &&
          mc_local_player_allocate(f->entities, &player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(player, f->world, NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK);
    mc_player_session_retire(&session);
    CHECK(placement_order(f, state, player) && placement_null_sound_controls(f, state, player) &&
          farming_controls(f, state, player) && food_controls(f, player) && dye_controls(f, state));
#ifdef MC_PLACEMENT_REAL_RENDER_GLOBAL
    CHECK(placement_receipt_controls(f, state, player) &&
          placement_real_n_control(f, state, player));
#endif
    mc_entity_release(player);
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity));
    CHECK(mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("usage: item-use-controls RESOURCES EMPTY_WORLD_DIRECTORY\n", stderr);
        return 2;
    }
    mc_math_trig_init(&trig);
    fixture f = {0};
    if (!setup(&f, argv[1], argv[2]) || !use_controls(&f) || !teardown(&f))
        return 1;
    printf("PASS: %u Item use source-order/ownership checks\n", checks);
    return 0;
}
