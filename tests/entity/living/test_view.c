/* Genuine constructor fixture; renamed main remains an independent control. */
int mc_view_player_constructor_fixture_main(int argc, char **argv);
#define main mc_view_player_constructor_fixture_main
#include "../player/test_construction.c"
#undef main

#include "mc/client/controller/singleplayer.h"
#include "mc/entity/living/view.h"
#include <inttypes.h>
#include <xmmintrin.h>

static double double_bits(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float float_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint64_t bits_of(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool view_setup(fixture *f, const char *resources, const char *directory,
                       const mc_math_trig *same_table) {
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_statistics(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, same_table,
                                          &f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->world_classes, f->registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->world_classes, f->entities) == MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options store = {.session_token = 6381,
                                          .block_map = map,
                                          .block_map_length = 256,
                                          .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, f->regions, &store, &f->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    const uint16_t title[] = {'V', 'i', 'e', 'w'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, 4}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          NULL, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->store, .name = f->name, .seed = 5143, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
          MC_PLAYER_OK);
    return true;
}

static bool player(fixture *f, mc_entity **out) {
    const uint16_t units[] = {'V', 'i', 'e', 'w'};
    mc_player_string *name = NULL, *token = NULL;
    mc_player_session *session = NULL;
    CHECK(mc_player_string_create(units, 4, &name) == MC_PLAYER_OK);
    CHECK(mc_player_string_create(NULL, 0, &token) == MC_PLAYER_OK);
    CHECK(mc_player_session_create(f->sessions, name, token, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entities, out, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(*out, f->world, NULL, session, 0, NULL, NULL) == MC_PLAYER_OK &&
          (*out)->phase == MC_ENTITY_READY);
    mc_player_session_retire(&session);
    mc_player_string_release(token);
    mc_player_string_release(name);
    return true;
}

static bool vectors(mc_entity *entity, mc_math_classes *math, const char *path) {
    FILE *input = fopen(path, "rb");
    CHECK(input != NULL);
    unsigned int row = 0, count = 0;
    uint32_t partial, angles[4];
    uint64_t coordinates[6], expected[9];
    mc_entity_view_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    const mc_java_random ordinary = *entity->random;
    const size_t held = entity->holds;
    while (fscanf(input,
                  "%10u %8" SCNx32 " %16" SCNx64 " %16" SCNx64 " %16" SCNx64 " %16" SCNx64
                  " %16" SCNx64 " %16" SCNx64 " %8" SCNx32 " %8" SCNx32 " %8" SCNx32 " %8" SCNx32
                  " %16" SCNx64 " %16" SCNx64 " %16" SCNx64 " %16" SCNx64 " %16" SCNx64
                  " %16" SCNx64 " %16" SCNx64 " %16" SCNx64 " %16" SCNx64,
                  &row, &partial, &coordinates[0], &coordinates[1], &coordinates[2],
                  &coordinates[3], &coordinates[4], &coordinates[5], &angles[0], &angles[1],
                  &angles[2], &angles[3], &expected[0], &expected[1], &expected[2], &expected[3],
                  &expected[4], &expected[5], &expected[6], &expected[7], &expected[8]) == 21) {
        CHECK(row == count);
        /* These are authored public source fields on a genuinely constructed
         * DC, matching the original observer. No phase/owner/return is injected. */
        entity->base.x = double_bits(coordinates[0]);
        entity->base.y = double_bits(coordinates[1]);
        entity->base.z = double_bits(coordinates[2]);
        entity->base.aJ = double_bits(coordinates[3]);
        entity->base.aK = double_bits(coordinates[4]);
        entity->base.aL = double_bits(coordinates[5]);
        entity->base.yaw = float_bits(angles[0]);
        entity->base.aT = float_bits(angles[1]);
        entity->base.aU = float_bits(angles[2]);
        entity->base.aV = float_bits(angles[3]);
        const mc_entity_base_fields before = entity->base;
        mc_entity_vector_error pool;
        CHECK(mc_entity_vector_pool_reset(entity->classes, &pool) == MC_ENTITY_OK);
        mc_entity_vector *values[3] = {0};
        CHECK(mc_entity_living_position(entity, float_bits(partial), &values[0], error) ==
              MC_ENTITY_VIEW_OK);
        CHECK(mc_entity_living_look(entity, math, float_bits(partial), &values[1], error) ==
              MC_ENTITY_VIEW_OK);
        CHECK(mc_entity_living_look_current(entity, math, &values[2], error) == MC_ENTITY_VIEW_OK);
        for (size_t i = 0; i < 3; ++i) {
            const double components[] = {values[i]->x, values[i]->y, values[i]->z};
            for (size_t axis = 0; axis < 3; ++axis) {
                if (bits_of(components[axis]) != expected[i * 3 + axis])
                    fprintf(stderr,
                            "Original view row%u vector%zu axis%zu: %016" PRIx64 " != %016" PRIx64
                            "\n",
                            row, i, axis, bits_of(components[axis]), expected[i * 3 + axis]);
                CHECK(bits_of(components[axis]) == expected[i * 3 + axis]);
            }
            CHECK(mc_entity_vector_release(entity->classes, values[i]) == MC_ENTITY_OK);
        }
        CHECK(memcmp(&entity->base, &before, sizeof(before)) == 0 && entity->holds == held &&
              memcmp(entity->random, &ordinary, sizeof(ordinary)) == 0);
        mc_entity_vector_snapshot pool_state;
        CHECK(mc_entity_vector_get_snapshot(entity->classes, &pool_state) == MC_ENTITY_OK &&
              pool_state.cursor == 3 && pool_state.holds == 0);
        ++count;
    }
    CHECK(feof(input) && count == 192 && fclose(input) == 0);
    free(error);
    return true;
}

static bool leaves_and_errors(fixture *f, mc_entity *entity, mc_math_classes *math) {
    mc_entity_view_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    mc_entity_vector *out = NULL;
    CHECK(mc_entity_living_position(NULL, 1, &out, error) == MC_ENTITY_VIEW_SOURCE_NULL_REFERENCE &&
          out == NULL && error->source_exception && !error->native_policy && !error->site.entered);
    CHECK(mc_entity_living_look_current(NULL, math, &out, error) ==
              MC_ENTITY_VIEW_SOURCE_NULL_REFERENCE &&
          !error->site.entered && strcmp(error->site.method, "ac") == 0);
    mc_entity_vector_error pool;
    CHECK(mc_entity_vector_pool_reset(entity->classes, &pool) == MC_ENTITY_OK);
    entity->base.x = 11;
    CHECK(mc_entity_living_position(entity, 1, &out, error) == MC_ENTITY_VIEW_OK);
    mc_entity_vector *held = out;
    CHECK(mc_entity_living_position(entity, 1, &out, error) == MC_ENTITY_VIEW_ARGUMENT &&
          out == held);
    CHECK(mc_entity_vector_pool_reset(entity->classes, &pool) == MC_ENTITY_OK);
    entity->base.x = 23;
    mc_entity_vector *reused = NULL;
    CHECK(mc_entity_living_position(entity, 1, &reused, error) == MC_ENTITY_VIEW_OK &&
          reused == held && held->x == 23);
    CHECK(mc_entity_vector_release(entity->classes, held) == MC_ENTITY_OK &&
          mc_entity_vector_release(entity->classes, reused) == MC_ENTITY_OK);
    out = NULL;
    mc_math_classes *cold = NULL;
    mc_math_class_error math_error;
    CHECK(mc_math_classes_create(4096, &cold, &math_error) == MC_MATH_CLASS_OK);
    mc_entity_vector_snapshot before, after;
    CHECK(mc_entity_vector_get_snapshot(entity->classes, &before) == MC_ENTITY_OK);
    CHECK(mc_entity_living_look(entity, cold, 1, &out, error) == MC_ENTITY_VIEW_MATH_FAILED &&
          out == NULL && error->site.pc == 17 && error->math.status == MC_MATH_CLASS_LIMIT &&
          error->math.source_pc == 2 && error->native_policy && !error->source_exception);
    CHECK(mc_entity_vector_get_snapshot(entity->classes, &after) == MC_ENTITY_OK &&
          before.cursor == after.cursor);
    CHECK(mc_entity_living_look(entity, cold, 0.5f, &out, error) == MC_ENTITY_VIEW_MATH_FAILED &&
          error->site.pc == 122 && error->math.status == MC_MATH_CLASS_INITIALIZATION_FAILED);
    CHECK(mc_math_classes_release(&cold, &math_error) == MC_MATH_CLASS_OK && cold == NULL);
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(saved | 0x8000u);
    const mc_entity_view_status no_arithmetic = mc_entity_living_position(entity, 1, &out, error);
    _mm_setcsr(saved);
    CHECK(no_arithmetic == MC_ENTITY_VIEW_OK && out != NULL);
    CHECK(mc_entity_vector_release(entity->classes, out) == MC_ENTITY_OK);
    out = NULL;
    _mm_setcsr(saved | 0x8000u);
    const mc_entity_view_status bad_fp = mc_entity_living_look(entity, math, 1, &out, error);
    _mm_setcsr(saved);
    CHECK(bad_fp == MC_ENTITY_VIEW_UNSUPPORTED_FP && error->site.pc == 13 && out == NULL);
    mc_client_singleplayer_controller *controller = NULL;
    mc_singleplayer_controller_error controller_error;
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &controller,
                                                     &controller_error) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    float reach = -7;
    CHECK(mc_client_singleplayer_controller_reach(controller, &reach, &controller_error) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          reach == 4 && controller_error.site.pc == 2);
    CHECK(mc_client_singleplayer_controller_construct(controller, NULL, &controller_error) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_singleplayer_controller_reach(controller, &reach, &controller_error) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          reach == 4 && controller_error.site.pc == 2);
    CHECK(mc_client_singleplayer_controller_reach(NULL, &reach, &controller_error) ==
              MC_SINGLEPLAYER_CONTROLLER_NULL_REFERENCE &&
          reach == 4);
    CHECK(mc_client_singleplayer_controller_free(&controller, &controller_error) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    free(error);
    return true;
}

static bool vector_budget(fixture *f, mc_math_classes *math) {
    mc_entity_classes *classes = NULL;
    mc_entity *raw = NULL;
    mc_entity_view_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_local_player_allocate(classes, &raw, NULL) == MC_ENTITY_OK);
    mc_entity_vector *one = NULL;
    CHECK(mc_entity_living_position(raw, 1, &one, error) == MC_ENTITY_VIEW_OK);
    mc_entity_class_snapshot measured;
    CHECK(mc_entity_classes_get_snapshot(classes, &measured) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_release(classes, one) == MC_ENTITY_OK);
    mc_entity_release(raw);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    classes = NULL;
    raw = NULL;
    /* An explicit RAW-prefix control with a public native bound permitting
     * exactly one real vector. No phase, count or allocator result is patched. */
    const mc_entity_limits limit = {measured.allocation_bytes, SIZE_MAX};
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, &limit, &classes) == MC_ENTITY_OK);
    CHECK(mc_local_player_allocate(classes, &raw, NULL) == MC_ENTITY_OK);
    mc_entity_living_ray_call *call = NULL;
    CHECK(mc_entity_living_ray_begin(raw, math, 4, 1, &call) == MC_ENTITY_VIEW_LIMIT);
    mc_entity_living_ray_state state;
    CHECK(mc_entity_living_ray_inspect(call, &state) && !state.source_returned &&
          state.source_pc == 9 && state.eye != NULL && state.look == NULL && state.end == NULL &&
          state.world_call == NULL && state.failure->site.pc == 76 &&
          state.failure->caller.pc == 9 && state.failure->vector.status == MC_ENTITY_LIMIT &&
          state.failure->native_policy && !state.failure->source_exception);
    mc_entity_vector_snapshot pool;
    CHECK(mc_entity_vector_get_snapshot(classes, &pool) == MC_ENTITY_OK && pool.cursor == 1 &&
          pool.holds == 1);
    CHECK(mc_entity_living_ray_retire(&call) == MC_ENTITY_VIEW_OK && call == NULL);
    mc_entity_release(raw);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    free(error);
    return true;
}

static bool ray_controls(fixture *f, mc_entity *entity, mc_math_classes *math) {
    mc_entity_view_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    entity->base.x = double_bits(UINT64_C(0x7ff8000000000123));
    mc_entity_living_ray_call *call = NULL;
    CHECK(mc_entity_living_ray_begin(entity, math, 4, 1, &call) == MC_ENTITY_VIEW_OK);
    mc_entity_living_ray_state state;
    CHECK(mc_entity_living_ray_inspect(call, &state) && state.source_returned &&
          state.source_pc == 53 && state.result == NULL && state.world == f->world &&
          state.world_call != NULL && state.eye != NULL && state.look != NULL && state.end != NULL);
    mc_world_raytrace_state child;
    CHECK(mc_world_raytrace_inspect(state.world_call, &child) && child.source_returned &&
          child.start == state.eye && child.end == state.end && child.cells_read == 0 &&
          mc_world_store_chunk_count(f->store) == 0);
    CHECK(mc_entity_living_ray_begin(entity, math, 4, 1, &call) == MC_ENTITY_VIEW_ARGUMENT);
    CHECK(mc_entity_living_ray_retire(&call) == MC_ENTITY_VIEW_OK && call == NULL);
    /* A real failed DC constructor completed LS217 before GS's NULL World68.
     * This is an explicit failed-prefix callee control, never a READY Player. */
    mc_entity *failed = NULL;
    CHECK(mc_local_player_allocate(f->entities, &failed, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(failed, NULL, NULL, NULL, 0, NULL, NULL) ==
              MC_PLAYER_NULL_REFERENCE &&
          failed->phase == MC_ENTITY_FAILED_CONSTRUCTION &&
          failed->local_player->fields.living_pc == 217);
    const size_t holds = failed->holds;
    CHECK(mc_entity_living_ray_begin(failed, math, 4, 1, &call) ==
          MC_ENTITY_VIEW_SOURCE_NULL_REFERENCE);
    CHECK(mc_entity_living_ray_inspect(call, &state) && !state.source_returned &&
          state.source_pc == 50 && state.failure->source_exception &&
          !state.failure->native_policy && state.failure->world == NULL &&
          state.world_call == NULL && state.world == NULL && state.eye != NULL &&
          state.look != NULL && state.end != NULL && failed->holds == holds + 1);
    mc_entity_release(failed);
    CHECK(mc_entity_living_ray_inspect(call, &state) && state.receiver != NULL);
    CHECK(mc_entity_living_ray_retire(&call) == MC_ENTITY_VIEW_OK && call == NULL);
    CHECK(mc_entity_living_ray_retire(&call) == MC_ENTITY_VIEW_OK);
    CHECK(mc_entity_living_ray_begin(entity, NULL, 4, 1, &call) == MC_ENTITY_VIEW_MATH_FAILED);
    CHECK(mc_entity_living_ray_inspect(call, &state) && state.source_pc == 9 && state.eye != NULL &&
          state.look == NULL && state.end == NULL && state.failure->site.pc == 17 &&
          state.failure->caller.pc == 9);
    CHECK(mc_entity_living_ray_retire(&call) == MC_ENTITY_VIEW_OK && call == NULL);
    CHECK(mc_entity_living_ray_begin(NULL, math, 4, 1, &call) ==
          MC_ENTITY_VIEW_SOURCE_NULL_REFERENCE);
    CHECK(mc_entity_living_ray_inspect(call, &state) && state.receiver == NULL &&
          state.eye == NULL && !state.failure->site.entered && state.failure->source_exception);
    CHECK(mc_entity_living_ray_retire(&call) == MC_ENTITY_VIEW_OK && call == NULL);
    free(error);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    mc_math_classes *math = NULL;
    mc_math_class_error math_error;
    if (mc_math_classes_create(SIZE_MAX, &math, &math_error) != MC_MATH_CLASS_OK ||
        mc_math_classes_ensure(math, &math_error) != MC_MATH_CLASS_OK)
        return 1;
    fixture f = {0};
    mc_entity *entity = NULL;
    if (!view_setup(&f, argv[1], argv[2], mc_math_classes_table(math)) || !player(&f, &entity) ||
        !vectors(entity, math, argv[3]) || !leaves_and_errors(&f, entity, math) ||
        !ray_controls(&f, entity, math) || !vector_budget(&f, math))
        return 1;
    mc_entity_release(entity);
    if (!teardown(&f) || mc_math_classes_release(&math, &math_error) != MC_MATH_CLASS_OK)
        return 1;
    printf("LivingEntity view contracts: %u checks; 192 original vectors\n", checks);
    return 0;
}
