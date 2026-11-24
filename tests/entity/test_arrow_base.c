/* Genuine World/Store/Registry/Chunk setup. Imported mains are not executed. */
#define MC_COLLISION_BOXES_MAIN mc_arrow_base_unused_collision_main
#include "../world/runtime/collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "../../src/core/math/execution_internal.h"
#include "../../src/entity/movement/environment_internal.h"
#include "../../src/entity/tick/tick_internal.h"
#include "../../src/world/runtime/material_movement_internal.h"
#include "mc/core/math/classes.h"
#include "mc/world/runtime/world_access.h"

static bool ab_setup(fixture **out, const char *directory) {
    bool ok = false;
    fixture *f = calloc(1, sizeof(*f));
    *out = f;
    CHECK(f != NULL && mc_math_trig_init(&trig) &&
          mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    const uint16_t title[] = {'A', 'r', 'r', 'o', 'w'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = (stream){.bytes = language};
    r->streams[2] = (stream){.bytes = language};
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
          r->opens == 3 && r->closes == 1 && !r->bad && boxes_open_store(f, 0, directory) &&
          boxes_create_world(f, 0));
    boxes_air(f, 0);
    ok = true;
cleanup:
    return ok;
}

static bool ab_state(mc_math_classes *math, mc_math_class_phase phase, size_t holds, bool has_array,
                     unsigned last_store) {
    mc_math_class_state state;
    return mc_math_classes_state(math, &state, NULL) == MC_MATH_CLASS_OK && state.phase == phase &&
           state.holds == holds && (state.source_a != NULL) == has_array &&
           state.last_store_pc == last_store;
}

static bool ab_same_vectors(const mc_entity_vector_snapshot *a,
                            const mc_entity_vector_snapshot *b) {
    return a->phase == b->phase && a->cursor == b->cursor && a->pool_identity == b->pool_identity &&
           a->size == b->size && a->capacity == b->capacity && a->holds == b->holds;
}

static bool ab_material_order(fixture *f) {
    bool ok = false;
    mc_math_classes *math = NULL, *limited = NULL;
    mc_math_execution execution;
    mc_math_execution_init(&execution, NULL);
    mc_world_material_movement_error *error = malloc(sizeof(*error));
    const mc_entity_bounds missing_area = {64, 64, 64, 65, 65, 65};
    const mc_entity_bounds reversed = {8, 64, 8, 7, 65, 7};
    mc_world_runtime *const world = f->worlds[0];
    mc_entity_vector_snapshot before, after;
    bool value = true;
    CHECK(error != NULL && mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_create(sizeof(mc_math_trig), &limited, NULL) == MC_MATH_CLASS_OK &&
          mc_entity_vector_get_snapshot(f->entity_classes[0], &before) == MC_ENTITY_OK);
    mc_math_execution_init(&execution, math);
    CHECK(mc_world_runtime_material_accelerate_in_execution(world, NULL, NULL, NULL, NULL, &value,
                                                            error, &execution) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->bytecode == 1 && value && !execution.failed && execution.owned == NULL &&
          ab_state(math, MC_MATH_CLASS_COLD, 1, false, 0));
    CHECK(mc_world_runtime_material_accelerate_in_execution(world, &missing_area, NULL, NULL, NULL,
                                                            NULL, error, &execution) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          error->bytecode == 0 && execution.owned == NULL &&
          ab_state(math, MC_MATH_CLASS_COLD, 1, false, 0));
    CHECK(mc_world_runtime_material_accelerate_in_execution(world, &missing_area, NULL, NULL, NULL,
                                                            &value, error,
                                                            &execution) == MC_WORLD_STORAGE_OK &&
          !value && !execution.failed && !execution.selected && execution.owned == math &&
          error->cell_work == 0 && ab_state(math, MC_MATH_CLASS_READY, 2, true, 37) &&
          mc_entity_vector_get_snapshot(f->entity_classes[0], &after) == MC_ENTITY_OK &&
          ab_same_vectors(&before, &after));
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK &&
          ab_state(math, MC_MATH_CLASS_READY, 1, true, 37));
    mc_math_execution_init(&execution, NULL);
    value = true;
    CHECK(mc_world_runtime_contains_material_in_execution(world, &reversed, NULL, NULL, &value,
                                                          error, &execution) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->bytecode == 4 && value && execution.failed && execution.owned == NULL &&
          execution.first.caller.pc == 4 &&
          strcmp(execution.first.caller.descriptor, "(Leq;Lln;)Z") == 0 &&
          execution.first.cause.source_pc == -1);
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, NULL);
    CHECK(mc_world_runtime_material_accelerate_in_execution(world, &missing_area, NULL, NULL, NULL,
                                                            &value, NULL, &execution) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value && execution.failed && execution.first.caller.pc == 4 && execution.owned == NULL);
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, limited);
    CHECK(mc_world_runtime_material_accelerate_in_execution(world, &missing_area, NULL, NULL, NULL,
                                                            &value, error,
                                                            &execution) == MC_WORLD_STORAGE_LIMIT &&
          error->bytecode == 4 && value && execution.first.caller.pc == 4 &&
          execution.first.child.pc == 2 && execution.owned == limited &&
          ab_state(limited, MC_MATH_CLASS_FAILED, 2, false, 0) &&
          mc_entity_vector_get_snapshot(f->entity_classes[0], &after) == MC_ENTITY_OK &&
          ab_same_vectors(&before, &after));
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, math);
    value = true;
    CHECK(mc_world_runtime_contains_material_in_execution(
              world, &reversed, NULL, NULL, &value, error, &execution) == MC_WORLD_STORAGE_OK &&
          !value && error->cell_work == 0 && !execution.selected && !execution.failed &&
          ab_state(math, MC_MATH_CLASS_READY, 2, true, 37) && world->active_calls == 0);
    ok = true;
cleanup:
    if (mc_math_execution_retire(&execution) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&limited, NULL) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    free(error);
    return ok;
}

/* The real earlier bounds FP guard must win over later scanner IN activation.
 * Use a separate genuine actor: no prior case's reached source fields are reset. */
static bool ab_base_fp_prefix(fixture *f) {
    bool ok = false;
    mc_math_classes *math = NULL;
    mc_math_execution execution;
    mc_math_execution_init(&execution, NULL);
    mc_entity_tick_error *error = malloc(sizeof(*error));
    mc_entity *arrow = NULL;
    CHECK(error != NULL && mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          boxes_new_arrow(f, f->worlds[0], true, 8, 64, 8, &arrow));
    const mc_entity_base_fields before = arrow->base;
    const mc_java_random random = *arrow->random;
    const size_t holds = arrow->holds;
    const int32_t bounds_cursor = f->entity_classes[0]->bounds_cursor;
    const size_t bounds_holds = f->entity_classes[0]->bounds_holds;
    mc_math_execution_init(&execution, math);
    mc_entity_tick_error_init(error);
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(saved | 0x8000u);
    const bool returned = mc_tick_arrow_base_body(arrow, f->registry[0], &execution, NULL, error);
    _mm_setcsr(saved); /* Restore the host control before any assertion can branch. */
    CHECK(!returned && error->status == MC_ENTITY_UNSUPPORTED_FP && error->bytecode == 81 &&
          error->base_bytecode == 81 && error->environment.bytecode == 13 &&
          error->environment.cause == MC_ENTITY_ENVIRONMENT_FP && error->native_policy &&
          !error->source_exception && arrow->base.bt == mc_java_i32_add(before.bt, 1) &&
          arrow->base.bi == before.bj && arrow->base.aJ == before.x && arrow->base.aK == before.y &&
          arrow->base.aL == before.z && arrow->base.aV == before.aT &&
          arrow->base.aU == before.yaw && arrow->base.private_c == before.private_c &&
          arrow->base.bx == before.bx && arrow->base.bv == before.bv && arrow->holds == holds &&
          mc_test_java_random_equal(arrow->random, &random) && execution.owned == NULL &&
          !execution.failed && !execution.selected &&
          ab_state(math, MC_MATH_CLASS_COLD, 1, false, 0) &&
          f->entity_classes[0]->bounds_cursor == bounds_cursor &&
          f->entity_classes[0]->bounds_holds == bounds_holds && f->worlds[0]->active_calls == 0);
    ok = true;
cleanup:
    if (mc_math_execution_retire(&execution) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    free(error);
    return ok;
}

static bool ab_material_fp_prefix(fixture *f) {
    bool ok = false;
    mc_math_classes *cold = NULL, *hot = NULL;
    mc_math_execution execution;
    mc_math_execution_init(&execution, NULL);
    mc_world_material_movement_error *error = malloc(sizeof(*error));
    const mc_entity_bounds query = {64, 64, 64, 65, 65, 65};
    bool value = true;
    CHECK(error != NULL && mc_math_classes_create(SIZE_MAX, &cold, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_create(SIZE_MAX, &hot, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(hot, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, cold);
    unsigned saved = _mm_getcsr();
    _mm_setcsr(saved | 0x8000u);
    const mc_world_storage_status cold_status = mc_world_runtime_material_accelerate_in_execution(
        f->worlds[0], &query, NULL, NULL, NULL, &value, error, &execution);
    _mm_setcsr(saved);
    CHECK(cold_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error->bytecode == 4 &&
          execution.failed && execution.first.child.pc == 20 &&
          execution.first.cause.status == MC_MATH_CLASS_UNSUPPORTED_FP &&
          ab_state(cold, MC_MATH_CLASS_FAILED, 2, true, 4));
    mc_math_class_state state;
    CHECK(mc_math_classes_state(cold, &state, NULL) == MC_MATH_CLASS_OK && state.source_a != NULL);
    uint32_t nonzero = 0;
    for (size_t i = 0; i < MC_MATH_TRIG_VALUES; ++i) {
        uint32_t bits;
        memcpy(&bits, &state.source_a->values[i], sizeof(bits));
        nonzero |= bits;
    }
    CHECK(nonzero == 0 && mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, hot);
    saved = _mm_getcsr();
    _mm_setcsr(saved | 0x8000u);
    const mc_world_storage_status hot_status = mc_world_runtime_material_accelerate_in_execution(
        f->worlds[0], &query, NULL, NULL, NULL, &value, error, &execution);
    _mm_setcsr(saved);
    CHECK(hot_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error->bytecode == 4 &&
          !execution.failed && !execution.selected &&
          strcmp(error->operation, "Material movement FP admission") == 0 &&
          ab_state(hot, MC_MATH_CLASS_READY, 2, true, 37) && f->worlds[0]->active_calls == 0);
    CHECK(ab_base_fp_prefix(f));
    ok = true;
cleanup:
    if (mc_math_execution_retire(&execution) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&hot, NULL) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&cold, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    free(error);
    return ok;
}

static bool ab_base_entry_and_prefix(fixture *f) {
    bool ok = false;
    mc_math_classes *math = NULL;
    mc_math_execution execution;
    mc_math_execution_init(&execution, NULL);
    mc_entity_tick_error *error = malloc(sizeof(*error));
    mc_entity *raw = NULL, *none = NULL, *air = NULL;
    CHECK(error != NULL && mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          boxes_new_arrow(f, NULL, false, 0, 0, 0, &raw) &&
          boxes_new_arrow(f, NULL, true, 8, 64, 8, &none) &&
          boxes_new_arrow(f, f->worlds[0], true, 8, 64, 8, &air));
    mc_math_execution_init(&execution, math);
    mc_entity_tick_error_init(error);
    CHECK(!mc_tick_arrow_base_body(raw, f->registry[0], &execution, NULL, error) &&
          error->status == MC_ENTITY_INVALID_PHASE && raw->base.bt == 0 &&
          execution.owned == NULL && ab_state(math, MC_MATH_CLASS_COLD, 1, false, 0));
    mc_entity_tick_error_init(error);
    CHECK(!mc_tick_arrow_base_body(air, f->registry[0], NULL, NULL, error) &&
          error->status == MC_ENTITY_INVALID_ARGUMENT && air->base.bt == 0);
    mc_entity_tick_error_init(error);
    CHECK(!mc_tick_arrow_base_body(none, NULL, &execution, NULL, error) &&
          error->status == MC_ENTITY_STATE_ERROR && error->environment.bytecode == 28 &&
          error->environment.cause == MC_ENTITY_ENVIRONMENT_NAMESPACE && none->base.bt == 1 &&
          error->base_bytecode == 81 && !error->source_exception && execution.owned == NULL);
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, math);
    mc_entity_tick_error_init(error);
    CHECK(!mc_tick_arrow_base_body(none, f->registry[0], &execution, NULL, error) &&
          error->status == MC_ENTITY_WORLD_FAILED && error->base_bytecode == 81 &&
          error->environment.bytecode == 32 && error->source_exception &&
          strcmp(error->exception_class, "java/lang/NullPointerException") == 0 &&
          none->base.bt == 2 && none->base.aJ == 8 && none->base.aK == 64 &&
          execution.owned == NULL && ab_state(math, MC_MATH_CLASS_COLD, 1, false, 0));
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, math);
    const mc_java_random random = *air->random;
    mc_arrow_fields fields;
    memcpy(&fields, &air->arrow->fields, sizeof(fields));
    const size_t holds = air->holds;
    mc_entity_tick_error_init(error);
    CHECK(mc_entity_tick_base_body_with_math(air, f->registry[0], math, NULL, error) ==
              MC_ENTITY_STATE_ERROR &&
          air->base.bt == 0 && ab_state(math, MC_MATH_CLASS_COLD, 1, false, 0));
    mc_entity_tick_error_init(error);
    CHECK(mc_tick_arrow_base_body(air, f->registry[0], &execution, NULL, error) &&
          error->status == MC_ENTITY_OK && error->base_bytecode == 590 && air->base.bt == 1 &&
          air->base.aJ == air->base.x && air->base.aK == air->base.y &&
          air->base.aL == air->base.z && !air->base.private_c && !air->base.bx &&
          air->holds == holds && memcmp(&fields, &air->arrow->fields, sizeof(fields)) == 0 &&
          mc_test_java_random_equal(air->random, &random) &&
          ab_state(math, MC_MATH_CLASS_READY, 2, true, 37));
    bool output = true;
    mc_entity_environment_error env;
    CHECK(mc_entity_environment_water(air, f->registry[0], NULL, &output, &env) ==
              MC_ENTITY_STATE_ERROR &&
          output && env.bytecode == 0);
    CHECK(mc_entity_environment_lava(air, f->registry[0], NULL, &output, &env) ==
              MC_ENTITY_STATE_ERROR &&
          output);
    air->base.bx = true;
    CHECK(mc_entity_environment_stored_in_water(air, &output, &env) == MC_ENTITY_OK && output);
    air->base.bx = false;
    bool result = true;
    CHECK(mc_entity_effect_damage_with_math(none, NULL, NULL, air, INT32_MIN, &result,
                                            &error->effect) == MC_ENTITY_OK &&
          !result && none->base.bb && !none->base.be && error->effect.status == MC_ENTITY_OK &&
          none->holds == 1 && air->holds == holds);
    none->base.bb = false;
    CHECK(mc_entity_effect_attacked(none, &error->effect) == MC_ENTITY_OK && none->base.bb &&
          mc_entity_effect_dead(none, NULL) == MC_ENTITY_OK && none->base.be &&
          none->phase == MC_ENTITY_READY && none->holds == 1);
    ok = true;
cleanup:
    if (mc_math_execution_retire(&execution) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    free(error);
    return ok;
}

static bool ab_fire_lava_void(fixture *f) {
    bool ok = false;
    mc_math_classes *math = NULL;
    mc_math_execution execution;
    mc_math_execution_init(&execution, NULL);
    mc_entity_tick_error *error = malloc(sizeof(*error));
    mc_entity *fire = NULL, *lava = NULL, *below = NULL;
    CHECK(error != NULL && mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          boxes_new_arrow(f, f->worlds[0], true, 8, 64, 8, &fire) &&
          boxes_new_arrow(f, f->worlds[0], true, 8, 8, 8, &lava) &&
          boxes_new_arrow(f, f->worlds[0], true, 8, -65, 8, &below));
    fire->base.bv = 20;
    fire->base.bb = false;
    fire->base.private_c = false;
    mc_math_execution_init(&execution, math);
    mc_entity_tick_error_init(error);
    CHECK(mc_tick_arrow_base_body(fire, f->registry[0], &execution, NULL, error) &&
          fire->base.bv == 19 && fire->base.bb && !fire->base.be && !fire->base.private_c &&
          error->base_bytecode == 590);
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, math);
    CHECK(boxes_cell(f, 0, 8, 8, 8, 11, 0));
    lava->base.bb = false;
    lava->base.private_c = false;
    mc_entity_tick_error_init(error);
    CHECK(mc_tick_arrow_base_body(lava, f->registry[0], &execution, NULL, error) &&
          lava->base.bv == 600 && lava->base.bb && !lava->base.be && !lava->base.bx &&
          error->lava_bytecode == 13 && error->base_bytecode == 590);
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, math);
    boxes_air(f, 0);
    mc_entity_tick_error_init(error);
    CHECK(mc_tick_arrow_base_body(below, f->registry[0], &execution, NULL, error) &&
          below->base.be && !below->base.private_c && below->base.bt == 1 &&
          error->void_bytecode == 1 && error->base_bytecode == 590 && below->arrow->fields.j == 0 &&
          below->arrow->fields.k == 0 && f->worlds[0]->active_calls == 0);
    ok = true;
cleanup:
    if (mc_math_execution_retire(&execution) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    free(error);
    return ok;
}

typedef struct ab_splash_event {
    bool sound;
    const mc_registry_string *name;
    double x, y, z, vx, vy, vz;
    float volume, pitch;
    mc_java_random random;
} ab_splash_event;

typedef struct ab_splash_observer {
    mc_entity *actor;
    mc_world_runtime *world;
    mc_world_access *access;
    mc_math_classes *math;
    mc_math_execution *execution;
    size_t actor_holds, bindings, count;
    bool bad;
    ab_splash_event events[23];
} ab_splash_observer;

static bool ab_name(const mc_registry_string *name, const char *ascii) {
    mc_registry_string_view view;
    if (!mc_registry_string_read(name, &view) || view.length != strlen(ascii))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (uint16_t)(unsigned char)ascii[i])
            return false;
    return true;
}

static bool ab_same_double(double left, double right) {
    uint64_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    return a == b;
}

static bool ab_same_float(float left, float right) {
    uint32_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    return a == b;
}

static ab_splash_event *ab_splash_record(ab_splash_observer *o, mc_world_runtime *world, bool sound,
                                         mc_world_storage_error *error) {
    mc_math_class_state state;
    if (world != o->world || o->actor->phase != MC_ENTITY_READY ||
        o->actor->holds != o->actor_holds + (sound ? 1u : 0u) || world->active_calls == 0 ||
        world->persistent_bindings != o->bindings || o->actor->base.bt != 2 || o->actor->base.bx ||
        o->actor->base.private_c || o->execution->owned != o->math ||
        o->execution->phase != MC_MATH_EXECUTION_OPEN || o->execution->failed ||
        mc_math_classes_state(o->math, &state, NULL) != MC_MATH_CLASS_OK ||
        state.phase != MC_MATH_CLASS_READY || state.holds != 2 ||
        mc_world_access_free(o->access, NULL) != MC_WORLD_STORAGE_BUSY)
        o->bad = true;
    if (o->count == COUNT(o->events)) {
        o->bad = true;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                          "Arrow splash observer event ceiling", 0};
        return NULL;
    }
    *error = (mc_world_storage_error){0};
    ab_splash_event *event = &o->events[o->count++];
    event->sound = sound;
    event->random = *o->actor->random;
    return event;
}

static mc_world_storage_status ab_splash_sound(void *context, mc_world_runtime *world,
                                               const mc_registry_string *name, double x, double y,
                                               double z, float volume, float pitch,
                                               mc_world_storage_error *error) {
    ab_splash_observer *o = context;
    ab_splash_event *event = ab_splash_record(o, world, true, error);
    if (event == NULL)
        return error->status;
    event->name = name;
    event->x = x;
    event->y = y;
    event->z = z;
    event->volume = volume;
    event->pitch = pitch;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ab_splash_particle(void *context, mc_world_runtime *world,
                                                  const mc_registry_string *name, double x,
                                                  double y, double z, double vx, double vy,
                                                  double vz, mc_world_storage_error *error) {
    ab_splash_observer *o = context;
    ab_splash_event *event = ab_splash_record(o, world, false, error);
    if (event == NULL)
        return error->status;
    event->name = name;
    event->x = x;
    event->y = y;
    event->z = z;
    event->vx = vx;
    event->vy = vy;
    event->vz = vz;
    return MC_WORLD_STORAGE_OK;
}

static bool ab_splash_success(fixture *f) {
    bool ok = false, registered = false;
    mc_math_classes *math = NULL;
    mc_math_execution *execution = calloc(1, sizeof(*execution));
    if (execution != NULL)
        mc_math_execution_init(execution, NULL);
    mc_entity_tick_error *error = malloc(sizeof(*error));
    ab_splash_observer *observer = calloc(1, sizeof(*observer));
    mc_world_access *access = NULL;
    mc_entity *arrow = NULL;
    const mc_world_access_callbacks callbacks = {.sound = ab_splash_sound,
                                                 .particle = ab_splash_particle};
    CHECK(error != NULL && observer != NULL && execution != NULL &&
          mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          boxes_new_arrow(f, f->worlds[0], true, 8, 8, 8, &arrow));
    /* A real prior dry base call clears the constructor's first-update flag.
     * Never project, replace or restore the actor's source fields/Random. */
    boxes_air(f, 0);
    mc_math_execution_init(execution, math);
    mc_entity_tick_error_init(error);
    CHECK(arrow->base.private_c && arrow->base.bt == 0 &&
          mc_tick_arrow_base_body(arrow, f->registry[0], execution, NULL, error) &&
          !arrow->base.private_c && !arrow->base.bx && arrow->base.bt == 1 &&
          arrow->base.width == .5f && arrow->base.x == 8 && arrow->base.y == 8 &&
          arrow->base.z == 8 && arrow->bounds->min_y == 8 &&
          ab_same_double(arrow->base.velocity_x, 0.0) &&
          ab_same_double(arrow->base.velocity_y, 0.0) &&
          ab_same_double(arrow->base.velocity_z, 0.0));
    CHECK(mc_math_execution_retire(execution) == MC_MATH_CLASS_OK &&
          boxes_cell(f, 0, 8, 8, 8, 9, 0));
    mc_math_execution_init(execution, math);
    observer->actor = arrow;
    observer->world = f->worlds[0];
    observer->math = math;
    observer->execution = execution;
    observer->actor_holds = arrow->holds;
    observer->bindings = f->worlds[0]->persistent_bindings;
    CHECK(mc_world_access_create(&callbacks, observer, &access, NULL) == MC_WORLD_STORAGE_OK);
    observer->access = access;
    CHECK(mc_world_runtime_add_world_access(f->worlds[0], access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    mc_java_random expected = *arrow->random; /* Only this independent value is advanced. */
    const mc_java_random world_random = *f->worlds[0]->fields.r;
    const mc_entity_base_fields before = arrow->base;
    mc_entity_tick_error_init(error);
    CHECK(mc_tick_arrow_base_body(arrow, f->registry[0], execution, NULL, error) &&
          error->status == MC_ENTITY_OK && error->base_bytecode == 590 &&
          error->particle_work == 22 && arrow->base.bt == 2 && arrow->base.bx &&
          !arrow->base.private_c && arrow->base.bv == 0 && !arrow->base.be &&
          arrow->holds == observer->actor_holds && observer->count == 23 && !observer->bad &&
          f->worlds[0]->active_calls == 0 &&
          f->worlds[0]->persistent_bindings == observer->bindings &&
          ab_state(math, MC_MATH_CLASS_READY, 2, true, 37) &&
          mc_test_java_random_equal(f->worlds[0]->fields.r, &world_random) &&
          ab_same_double(arrow->base.velocity_x, 0.0) &&
          ab_same_double(arrow->base.velocity_y, 0.0) &&
          ab_same_double(arrow->base.velocity_z, 0.0));
    const float first = mc_java_random_next_float(&expected);
    const float second = mc_java_random_next_float(&expected);
    volatile float difference = first - second;
    volatile float scaled_pitch = difference * .4f;
    volatile float pitch = 1.0f + scaled_pitch;
    const ab_splash_event *sound = &observer->events[0];
    CHECK(sound->sound && ab_name(sound->name, "random.splash") &&
          ab_same_double(sound->x, before.x) &&
          ab_same_double(sound->y, before.y - (double)before.offset) &&
          ab_same_double(sound->z, before.z) && ab_same_float(sound->volume, 0.0f) &&
          ab_same_float(sound->pitch, pitch) &&
          mc_test_java_random_equal(&sound->random, &expected));
    for (size_t i = 0; i < 22; ++i) {
        const ab_splash_event *event = &observer->events[i + 1];
        const bool bubble = i < 11;
        volatile float twice_x = mc_java_random_next_float(&expected) * 2.0f;
        volatile float centered_x = twice_x - 1.0f;
        volatile float ox = centered_x * .5f;
        volatile float twice_z = mc_java_random_next_float(&expected) * 2.0f;
        volatile float centered_z = twice_z - 1.0f;
        volatile float oz = centered_z * .5f;
        double vy = 0.0;
        if (bubble) {
            volatile float dy = mc_java_random_next_float(&expected) * .2f;
            vy = 0.0 - (double)dy;
        }
        CHECK(!event->sound && ab_name(event->name, bubble ? "bubble" : "splash") &&
              ab_same_double(event->x, before.x + (double)ox) && ab_same_double(event->y, 9.0) &&
              ab_same_double(event->z, before.z + (double)oz) && ab_same_double(event->vx, 0.0) &&
              ab_same_double(event->vy, vy) && ab_same_double(event->vz, 0.0) &&
              mc_test_java_random_equal(&event->random, &expected));
    }
    /* 2 sound + 11*3 bubble + 11*2 splash = 57 nextFloat calls.
     * The helper compares explicit LCG/cache fields and ignores native padding. */
    CHECK(mc_test_java_random_equal(arrow->random, &expected));
    ok = true;
cleanup:
    if (registered) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            ok = false;
        else
            registered = false;
    }
    if (access != NULL) {
        if (mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK) {
            /* Keep every borrowed callback context and owning Math slot live.
             * The test fails; native cleanup never forces a source mutation. */
            fputs("Arrow splash observer cleanup refused; actual graph retained\n", stderr);
            return false;
        } else {
            access = NULL;
            free(observer);
            observer = NULL;
        }
    } else
        free(observer);
    if (mc_math_execution_retire(execution) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    else
        free(execution);
    boxes_air(f, 0);
    free(error);
    return ok;
}

static bool ab_water_and_limits(fixture *f) {
    bool ok = false, registered = false;
    mc_math_classes *math = NULL;
    mc_math_execution execution;
    mc_math_execution_init(&execution, NULL);
    mc_entity_tick_error *error = malloc(sizeof(*error));
    mc_entity *first = NULL, *limited = NULL, *sound_failure = NULL;
    CHECK(error != NULL && mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          boxes_new_arrow(f, f->worlds[0], true, 8, 8, 8, &first) &&
          boxes_new_arrow(f, f->worlds[0], true, 8, 8, 8, &limited) &&
          boxes_new_arrow(f, f->worlds[0], true, 8, 8, 8, &sound_failure) &&
          boxes_cell(f, 0, 8, 8, 8, 9, 0));
    mc_math_execution_init(&execution, math);
    mc_entity_tick_error_init(error);
    CHECK(first->base.private_c &&
          mc_tick_arrow_base_body(first, f->registry[0], &execution, NULL, error) &&
          first->base.bx && !first->base.private_c && error->particle_work == 0 &&
          first->base.bv == 0 && error->base_bytecode == 590);
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, math);
    limited->base.private_c = false;
    limited->base.bx = false;
    const mc_entity_tick_limits limits = {.max_particle_work = 0,
                                          .material = {.max_cell_work = 32}};
    mc_entity_tick_error_init(error);
    error->max_particle_work = 999;
    CHECK(!mc_tick_arrow_base_body(limited, f->registry[0], &execution, &limits, error) &&
          error->status == MC_ENTITY_LIMIT && error->max_particle_work == 0 &&
          error->particle_work == 0 && error->bytecode == 217 && error->base_bytecode == 209 &&
          !limited->base.bx && !limited->base.private_c && limited->base.bt == 1);
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, math);
    sound_failure->base.private_c = false;
    sound_failure->base.bx = false;
    CHECK(mc_world_runtime_add_world_access(f->worlds[0], NULL, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    mc_entity_tick_error_init(error);
    CHECK(!mc_tick_arrow_base_body(sound_failure, f->registry[0], &execution, NULL, error) &&
          error->status == MC_ENTITY_WORLD_FAILED && error->source_exception &&
          error->base_bytecode == 181 && error->world.world_access_bytecode == 53 &&
          error->world.world_access_failure.source_null_receiver &&
          strcmp(error->exception_class, "java/lang/NullPointerException") == 0 &&
          !sound_failure->base.bx && sound_failure->base.bt == 1 &&
          ab_state(math, MC_MATH_CLASS_READY, 2, true, 37));
    bool null_removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->worlds[0], NULL, &null_removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          null_removed);
    registered = false;
    CHECK(ab_splash_success(f));
    ok = true;
cleanup:
    if (registered) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(f->worlds[0], NULL, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            ok = false;
    }
    if (mc_math_execution_retire(&execution) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    boxes_air(f, 0);
    free(error);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char directory[8192];
    if (argc != 3 || strcmp(argv[1], "arrow-base") != 0 ||
        !decode_path(argv[2], directory, sizeof(directory)))
        return 1;
    CHECK(ab_setup(&f, directory));
    puts("ARROW_BASE_CASE id=0");
    CHECK(ab_material_order(f));
    puts("ARROW_BASE_CASE id=1");
    CHECK(ab_material_fp_prefix(f));
    puts("ARROW_BASE_CASE id=2");
    CHECK(ab_base_entry_and_prefix(f));
    puts("ARROW_BASE_CASE id=3");
    CHECK(ab_fire_lava_void(f));
    puts("ARROW_BASE_CASE id=4");
    CHECK(ab_water_and_limits(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Arrow base cleanup refused; surviving actual owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u Arrow base and contextual Math checks;5 native controls\n", checks) < 0)
        result = 1;
    return result;
}
