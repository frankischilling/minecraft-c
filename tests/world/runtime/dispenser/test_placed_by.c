/* Constructed native XQ placed-by controls, not original execution/parity.
 * Reuse genuine GAME/Registry/Chunk constructors; the renamed main is never
 * run. Loaded cell bytes, remote and actual public Living yaw are explicit
 * authored prestate. No READY phase, Math callee or World result is replaced. */
int mc_dispenser_placed_by_unused_boxes_main(int, char **);
#define MC_COLLISION_BOXES_MAIN mc_dispenser_placed_by_unused_boxes_main
#include "../collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "mc/entity/player/player.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/placement.h"
#include "world/runtime/world_access_internal.h"

#include <xmmintrin.h>

typedef struct yaw_case {
    uint32_t bits;
    int32_t metadata;
} yaw_case;

/* Exact binary32 public inputs. Expected directions are independent authored
 * XQ branches: 0/1/2/3 -> 2/5/3/4. Adjacent words bracket each signed45 boundary.
 * IN's Java narrowing/decrement maps either infinity to direction3, NaN to0. */
static const yaw_case yaw_cases[] = {
    {0x00000000, 2}, {0x80000000, 2}, {0x42b40000, 5}, {0x43340000, 3}, {0x43870000, 4},
    {0x43b40000, 2}, {0xc2b40000, 4}, {0xc3340000, 3}, {0xc3870000, 5}, {0xc3b40000, 2},
    {0x4233ffff, 2}, {0x42340000, 5}, {0x42340001, 5}, {0xc2340001, 4}, {0xc2340000, 2},
    {0xc233ffff, 2}, {0x4306ffff, 5}, {0x43070000, 3}, {0x43070001, 3}, {0xc3070001, 3},
    {0xc3070000, 4}, {0xc306ffff, 4}, {0x4360ffff, 3}, {0x43610000, 4}, {0x43610001, 4},
    {0xc3610001, 5}, {0xc3610000, 3}, {0xc360ffff, 3}, {0x439d7fff, 4}, {0x439d8000, 2},
    {0x439d8001, 2}, {0xc39d8001, 2}, {0xc39d8000, 5}, {0xc39d7fff, 5}, {0x7fc00000, 2},
    {0xffc12345, 2}, {0x7f800000, 4}, {0xff800000, 4}, {0x7e800000, 4}, {0xfe800000, 4},
    {0x7f7fffff, 4}, {0xff7fffff, 4}, {0x00000001, 2}, {0x80000001, 2},
};

static void authored_yaw(mc_entity *living, uint32_t bits) {
    memcpy(&living->base.yaw, &bits, sizeof(bits));
}

typedef struct placed_by_observer {
    mc_world_runtime *world;
    mc_world_placement_error *placement;
    mc_world_access *self;
    mc_world_access_receipt *receipt;
    size_t calls;
    int32_t expected;
    bool valid, refuse, removed, busy;
} placed_by_observer;

/* This is an actual native WorldAccess producer, not a substituted FD/IN/XQ
 * body. Its failure receipt owns its literal class name until checked retire. */
static mc_world_storage_status placed_by_cell(void *context, mc_world_runtime *world, int32_t x,
                                              int32_t y, int32_t z, mc_world_storage_error *cause,
                                              mc_world_access_failure *failure) {
    placed_by_observer *o = context;
    ++o->calls;
    o->busy = !mc_world_placement_error_retire(o->placement) &&
              o->placement->cleanup.status == MC_WORLD_STORAGE_BUSY;
    int32_t actual = -1;
    o->valid &=
        world == o->world && x == 8 && y == 64 && z == 8 && o->busy &&
        mc_world_runtime_metadata_get(world, x, y, z, &actual, cause) == MC_WORLD_STORAGE_OK &&
        actual == o->expected;
    if (!o->valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                          "Dispenser placed-by actual notification operands", 0};
        return cause->status;
    }
    if (!o->refuse)
        return MC_WORLD_STORAGE_OK;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, o->self, &o->removed, cause);
    if (status != MC_WORLD_STORAGE_OK || !o->removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_INVALID_DATA : status;
    o->valid &= mc_world_access_free(o->self, NULL) == MC_WORLD_STORAGE_BUSY;
    status = mc_world_access_receipt_create(o->self, NULL, &o->receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    o->receipt->exception_class = "test/DispenserPlacedByFailure";
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Dispenser placed-by native producer refusal", 77};
    return cause->status;
}

static bool placed_by_controls(fixture *f, mc_entity *living) {
    bool ok = false, registered = false;
    mc_world_runtime *world = f->worlds[0];
    const bool remote = world->fields.B;
    const size_t bindings = world->persistent_bindings, holds = living->holds;
    const mc_block *block = NULL;
    mc_math_classes *math = NULL;
    mc_math_class_state state;
    mc_world_placement_error e = {0};
    placed_by_observer observer = {.world = world, .placement = &e, .valid = true};
    mc_world_access_failure outside = {0};
    const mc_world_access_source_callbacks callbacks = {.cell = placed_by_cell};
    CHECK(mc_registry_block_get(f->registry[0], 23, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_world_access_source_create(&callbacks, &observer, &observer.self, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, observer.self, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    boxes_air(f, 0);
    for (size_t i = 0; i < COUNT(yaw_cases); ++i) {
        CHECK(boxes_cell(f, 0, 8, 64, 8, 23, 0));
        authored_yaw(living, yaw_cases[i].bits);
        observer.expected = yaw_cases[i].metadata;
        const size_t calls = observer.calls;
        CHECK(mc_world_block_placed_by_with_math(f->registry[0], block, world, 8, 64, 8, living,
                                                 math, &e) == MC_WORLD_STORAGE_OK &&
              e.returned && strcmp(e.site.class_name, "xq") == 0 && e.site.pc == 82 &&
              strcmp(e.site.descriptor, "(Lfd;IIILls;)V") == 0 && !e.remote_read &&
              e.mutation != NULL && e.query_count == 0 && observer.calls == calls + 1 &&
              observer.valid && observer.busy && living->holds == holds + 1 &&
              world->persistent_bindings == bindings + 1 &&
              mc_math_classes_state(math, &state, NULL) == MC_MATH_CLASS_OK &&
              state.phase == MC_MATH_CLASS_READY && state.holds == 2);
        uint32_t yaw_after;
        memcpy(&yaw_after, &living->base.yaw, sizeof(yaw_after));
        int32_t actual = -1;
        CHECK(yaw_after == yaw_cases[i].bits &&
              mc_world_runtime_metadata_get(world, 8, 64, 8, &actual, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              actual == yaw_cases[i].metadata && mc_world_placement_error_retire(&e) &&
              living->holds == holds && world->persistent_bindings == bindings &&
              world->active_calls == 0 &&
              mc_math_classes_state(math, &state, NULL) == MC_MATH_CLASS_OK && state.holds == 1);
    }
    /* Remote=true still reaches this override's notifying metadata setter.
     * Equal metadata also still invokes the actual WorldAccess cell. */
    CHECK(boxes_cell(f, 0, 8, 64, 8, 23, 2));
    authored_yaw(living, 0);
    observer.expected = 2;
    world->fields.B = true;
    const size_t calls = observer.calls;
    const mc_world_storage_status changed = mc_world_block_placed_by_with_math(
        f->registry[0], block, world, 8, 64, 8, living, math, &e);
    world->fields.B = remote;
    CHECK(changed == MC_WORLD_STORAGE_OK && e.returned && e.site.pc == 82 && !e.remote_read &&
          observer.calls == calls + 1 && observer.valid && mc_world_placement_error_retire(&e));

    /* A real self-removing producer fails after metadata changed. Its same
     * immutable receipt survives placement retirement through an external hold. */
    CHECK(boxes_cell(f, 0, 8, 64, 8, 23, 0));
    observer.refuse = true;
    const mc_world_storage_status failure = mc_world_block_placed_by_with_math(
        f->registry[0], block, world, 8, 64, 8, living, math, &e);
    if (failure != MC_WORLD_STORAGE_UNSUPPORTED_STATE || e.returned || !e.source_exception ||
        e.native_policy || e.site.pc != 34 || e.mutation == NULL || !observer.removed ||
        !observer.valid || observer.receipt == NULL || observer.receipt->holds != 2 ||
        e.retained_producers != 1 ||
        (e.mutation != NULL &&
         (e.mutation->wrapper_bytecode != 40 || e.mutation->world_access_bytecode != 34)) ||
        (e.mutation != NULL && e.mutation->world_access_failure.receipt != observer.receipt) ||
        (observer.receipt != NULL && e.exception_class != observer.receipt->exception_class))
        fprintf(stderr,
                "Dispenser placed-by failure status=%d returned=%d exception=%d native=%d "
                "pc=%u mutation=%d wrapper=%u access=%u removed=%d valid=%d receipt=%d "
                "holds=%zu retained=%zu matched=%d class-matched=%d\n",
                (int)failure, e.returned, e.source_exception, e.native_policy, e.site.pc,
                e.mutation != NULL, e.mutation == NULL ? 0 : e.mutation->wrapper_bytecode,
                e.mutation == NULL ? 0 : e.mutation->world_access_bytecode, observer.removed,
                observer.valid, observer.receipt != NULL,
                observer.receipt == NULL ? 0 : observer.receipt->holds, e.retained_producers,
                e.mutation != NULL && e.mutation->world_access_failure.receipt == observer.receipt,
                observer.receipt != NULL && e.exception_class == observer.receipt->exception_class);
    CHECK(failure == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !e.returned && e.source_exception &&
          !e.native_policy && e.site.pc == 34 && strcmp(e.site.class_name, "xq") == 0 &&
          e.mutation != NULL && e.mutation->wrapper_bytecode == 40 &&
          e.mutation->world_access_bytecode == 34 && observer.removed && observer.valid &&
          observer.receipt != NULL && observer.receipt->holds == 2 && e.retained_producers == 1 &&
          e.mutation->world_access_failure.receipt == observer.receipt &&
          e.exception_class == observer.receipt->exception_class);
    registered = false;
    int32_t actual = -1;
    CHECK(mc_world_runtime_metadata_get(world, 8, 64, 8, &actual, NULL) == MC_WORLD_STORAGE_OK &&
          actual == 2 && mc_world_access_free(observer.self, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_access_failure_retain(&e.mutation->world_access_failure, &outside) ==
              MC_WORLD_STORAGE_OK &&
          observer.receipt->holds == 3 && mc_world_placement_error_retire(&e) &&
          observer.receipt->holds == 2 && living->holds == holds &&
          world->persistent_bindings == bindings);
    const char *exception = NULL;
    CHECK(mc_world_access_failure_source_exception(&outside, &exception) &&
          strcmp(exception, "test/DispenserPlacedByFailure") == 0 &&
          mc_world_access_receipt_retire(&observer.receipt, NULL) == MC_WORLD_STORAGE_BUSY);
    mc_world_access_failure_release(&outside);
    CHECK(observer.receipt->holds == 1 &&
          mc_world_access_receipt_retire(&observer.receipt, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_access_free(observer.self, NULL) == MC_WORLD_STORAGE_OK);
    observer.self = NULL;
    ok = true;
cleanup:
    world->fields.B = remote;
    if (!mc_world_placement_error_retire(&e))
        return false;
    mc_world_access_failure_release(&outside);
    if (observer.receipt != NULL &&
        mc_world_access_receipt_retire(&observer.receipt, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (observer.self != NULL) {
        bool removed = false;
        if (registered && mc_world_runtime_remove_world_access(world, observer.self, &removed,
                                                               NULL) != MC_WORLD_STORAGE_OK)
            return false;
        if (mc_world_access_free(observer.self, NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    return mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK && ok;
}

static bool placed_by_failure_order(fixture *f, mc_entity *living) {
    bool ok = false;
    const size_t holds = living->holds;
    mc_math_classes *cold = NULL, *limited = NULL;
    mc_math_class_state state;
    mc_world_placement_error e = {0};
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->registry[0], 23, &block, NULL) == MC_REGISTRY_OK &&
          mc_math_classes_create(SIZE_MAX, &cold, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_create(sizeof(mc_math_trig), &limited, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_world_block_placed_by_with_math(f->registry[0], block, NULL, 8, 64, 8, NULL, cold,
                                             &e) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.source_exception && !e.native_policy && !e.returned && e.site.pc == 2 &&
          strcmp(e.site.class_name, "xq") == 0 && e.exception_class != NULL &&
          strcmp(e.exception_class, "java/lang/NullPointerException") == 0 && e.mutation == NULL &&
          mc_math_classes_state(cold, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_COLD && state.source_a == NULL && state.holds == 1 &&
          living->holds == holds && mc_world_placement_error_retire(&e));
    authored_yaw(living, 0);
    const unsigned int control = _mm_getcsr();
    _mm_setcsr((control & ~0x6000u) | 0x2000u);
    const mc_world_storage_status floating =
        mc_world_block_placed_by_with_math(f->registry[0], block, NULL, 8, 64, 8, living, cold, &e);
    _mm_setcsr(control);
    CHECK(floating == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e.native_policy &&
          !e.source_exception && e.site.pc == 7 && strcmp(e.site.class_name, "xq") == 0 &&
          e.mutation == NULL && mc_math_classes_state(cold, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_COLD && state.holds == 1 &&
          mc_world_placement_error_retire(&e) && living->holds == holds);
    CHECK(mc_world_block_placed_by(f->registry[0], block, NULL, 8, 64, 8, living, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.native_policy && !e.source_exception && e.site.pc == 16 &&
          strcmp(e.site.class_name, "xq") == 0 && e.mutation == NULL &&
          living->holds == holds + 1 && mc_world_placement_error_retire(&e) &&
          living->holds == holds);
    /* Genuine first Math active use fails and latches before nullable World.
     * No forged Math phase or replaced floor function is involved. */
    for (unsigned int attempt = 0; attempt < 2; ++attempt) {
        CHECK(mc_world_block_placed_by_with_math(f->registry[0], block, NULL, 8, 64, 8, living,
                                                 limited,
                                                 &e) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              !e.returned && e.native_policy && !e.source_exception && e.mutation == NULL &&
              strcmp(e.site.class_name, "in") == 0 && e.site.pc == 2 &&
              strcmp(e.caller.class_name, "xq") == 0 && e.caller.pc == 16 &&
              e.math.status ==
                  (attempt == 0 ? MC_MATH_CLASS_LIMIT : MC_MATH_CLASS_INITIALIZATION_FAILED) &&
              mc_math_classes_state(limited, &state, NULL) == MC_MATH_CLASS_OK &&
              state.phase == MC_MATH_CLASS_FAILED && state.source_a == NULL && state.holds == 2 &&
              living->holds == holds + 1 && mc_world_placement_error_retire(&e) &&
              living->holds == holds);
    }
    static const uint32_t cardinal[] = {0x00000000, 0x42b40000, 0x43340000, 0x43870000};
    static const unsigned int pcs[] = {34, 49, 64, 79};
    for (size_t i = 0; i < COUNT(cardinal); ++i) {
        authored_yaw(living, cardinal[i]);
        CHECK(mc_world_block_placed_by_with_math(f->registry[0], block, NULL, 8, 64, 8, living,
                                                 cold, &e) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              !e.returned && e.source_exception && !e.native_policy &&
              strcmp(e.site.class_name, "xq") == 0 && e.site.pc == pcs[i] &&
              e.exception_class != NULL &&
              strcmp(e.exception_class, "java/lang/NullPointerException") == 0 &&
              e.mutation == NULL && mc_math_classes_state(cold, &state, NULL) == MC_MATH_CLASS_OK &&
              state.phase == MC_MATH_CLASS_READY && state.holds == 2 &&
              living->holds == holds + 1 && mc_world_placement_error_retire(&e) &&
              living->holds == holds);
    }
    ok = true;
cleanup:
    if (!mc_world_placement_error_retire(&e))
        return false;
    return mc_math_classes_release(&cold, NULL) == MC_MATH_CLASS_OK &&
           mc_math_classes_release(&limited, NULL) == MC_MATH_CLASS_OK && ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_entity *living = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL;
    char root[8192];
    const uint16_t title[] = {'X', 'Q', ' ', 'y', 'a', 'w'};
    const uint16_t name[] = {'X', 'Q', 'L', 'i', 'v', 'i', 'n', 'g'};
    if (argc != 3 || strcmp(argv[1], "world-dispenser-placed-by") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
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
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          boxes_open_store(f, 0, root) && boxes_create_world(f, 0) &&
          mc_registry_classes_initialize_statistics(f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &sessions, NULL) ==
              MC_PLAYER_OK &&
          mc_player_string_create(name, COUNT(name), &username) == MC_PLAYER_OK &&
          mc_player_session_create(sessions, username, NULL, &session, NULL) == MC_PLAYER_OK &&
          mc_local_player_allocate(f->entity_classes[0], &living, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(living, f->worlds[0], NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          placed_by_failure_order(f, living) && placed_by_controls(f, living));
    result = 0;
cleanup:
    mc_entity_release(living);
    (void)mc_player_session_retire(&session);
    mc_player_string_release(username);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !cleanup_fixture(f)) {
        fputs("Dispenser placed-by cleanup refused\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("Dispenser placed-by CPU controls passed: %u\n", checks);
    return result;
}
