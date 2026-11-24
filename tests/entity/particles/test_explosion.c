/* Genuine Player/App/World fixture; its earlier entry point is never run. */
#define main mc_explosion_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "client/particles/effect_renderer_internal.h"
#include "core/java/random_profile_internal.h"
#include "entity/particles/particle_internal.h"
#include "mc/entity/particles/particle.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"

#include <inttypes.h>
#include <limits.h>
#include <stdatomic.h>
#include <xmmintrin.h>

static uint64_t bits(double value) {
    uint64_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static uint32_t fbits(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static bool create(fixture *f, mc_math_classes *math, mc_entity **out, mc_particle_error *error) {
    CHECK(mc_explosion_particle_allocate(f->app.entities, out, NULL) == MC_ENTITY_OK &&
          mc_explosion_particle_construct(*out, f->world, math, 8.5, 64.5, 8.5, .125, -.25, .0625,
                                          error) == MC_ENTITY_OK &&
          (*out)->phase == MC_ENTITY_READY && (*out)->holds == 1);
    return true;
}

static bool constructor_controls(fixture *f, mc_math_classes *math) {
    mc_particle_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    const uint64_t shared_before = atomic_load(&f->app.profile->math->state);
    mc_entity *particle = NULL;
    CHECK(create(f, math, &particle, error));
    const mc_particle_constructor_random *r = &error->constructor_random;
    CHECK(r->math_count == 8 && r->own_count == 8 && r->own_initial_stored &&
          r->own_identity == (uintptr_t)particle->random &&
          strcmp(error->source_class, "gy") == 0 && error->source_pc == 157 &&
          particle->particle->item_native.item_pc == 157);
    mc_java_random shared = {.state = shared_before};
    for (size_t i = 0; i < 8; ++i)
        CHECK(bits(mc_java_random_next_double(&shared)) == bits(r->math_values[i]));
    CHECK(shared.state == atomic_load(&f->app.profile->math->state));
    mc_java_random own = r->own_initial;
    for (size_t i = 0; i < 8; ++i)
        CHECK(fbits(mc_java_random_next_float(&own)) == fbits(r->own_values[i]));
    CHECK(own.state == particle->random->state);
    const double input[3] = {.125, -.25, .0625};
    const double observed[3] = {particle->base.velocity_x, particle->base.velocity_y,
                                particle->base.velocity_z};
    for (size_t i = 0; i < 3; ++i) {
        const double centered = r->math_values[i + 5] * 2.0 - 1.0;
        const float narrowed = (float)centered;
        const float jitter = narrowed * .05f;
        const double expected = input[i] + (double)jitter;
        CHECK(bits(observed[i]) == bits(expected));
    }
    const float multiplied = r->own_values[4] * .3f;
    const float color = multiplied + .7f;
    const float product = r->own_values[5] * r->own_values[6];
    const float scale = product * 6.0f + 1.0f;
    const double denominator = (double)r->own_values[7] * .8 + .2;
    const int32_t lifetime = (int32_t)(16.0 / denominator) + 2;
    CHECK(fbits(particle->particle->fields.i) == fbits(color) &&
          fbits(particle->particle->fields.j) == fbits(color) &&
          fbits(particle->particle->fields.k) == fbits(color) &&
          fbits(particle->particle->fields.g) == fbits(scale) &&
          particle->particle->fields.f == lifetime && particle->particle->fields.e == 0 &&
          particle->particle->fields.b == 0 && lifetime >= 18 && lifetime <= 82);
    int32_t layer = -1;
    CHECK(mc_explosion_particle_layer(particle, &layer) == MC_ENTITY_OK && layer == 0);
    mc_entity_release(particle);
    particle = NULL;

    mc_math_classes *limited = NULL, *captured = NULL;
    CHECK(mc_math_classes_create(4096, &limited, NULL) == MC_MATH_CLASS_OK &&
          mc_explosion_particle_allocate(f->app.entities, &particle, NULL) == MC_ENTITY_OK);
    CHECK(mc_explosion_particle_construct_entered(particle, f->world, limited, 1, 2, 3, 4, 5, 6,
                                                  &captured, error) == MC_ENTITY_LIMIT &&
          captured == limited && particle->phase == MC_ENTITY_FAILED_CONSTRUCTION &&
          error->constructor_random.math_count == 5 && error->constructor_random.own_count == 0 &&
          strcmp(error->source_class, "xw") == 0 && error->source_pc == 164 &&
          particle->particle->item_native.item_pc == 13 &&
          mc_math_classes_release(&captured, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_release(&limited, NULL) == MC_MATH_CLASS_OK);
    mc_entity_release(particle);
    free(error);
    return true;
}

static void words(const double *values, size_t count) {
    fputc('[', stdout);
    for (size_t i = 0; i < count; ++i)
        printf("%s\"%016" PRIx64 "\"", i ? "," : "", bits(values[i]));
    fputc(']', stdout);
}

static bool tick_profiles(fixture *f, mc_math_classes *math) {
    static const char *const profiles[] = {"age0",     "age7",      "age8",       "negative-age",
                                           "wrap-age", "zero-life", "ground-age0"};
    static const int32_t ages[] = {0, 7, 8, -1, INT_MAX, 0, 0};
    mc_particle_error *constructor = calloc(1, sizeof(*constructor));
    mc_item_particle_tick_error *error = calloc(1, sizeof(*error));
    CHECK(constructor != NULL && error != NULL);
    for (size_t i = 0; i < sizeof(profiles) / sizeof(*profiles); ++i) {
        mc_entity *particle = NULL;
        CHECK(create(f, math, &particle, constructor));
        CHECK(mc_entity_set_position(particle, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
        particle->base.aJ = -1;
        particle->base.aK = -2;
        particle->base.aL = -3;
        particle->base.velocity_x = .125;
        particle->base.velocity_y = -.25;
        particle->base.velocity_z = .0625;
        particle->base.bq = true;
        particle->base.aX = i == 6;
        particle->base.be = false;
        particle->particle->fields.e = ages[i];
        particle->particle->fields.f = i == 5 ? 0 : 8;
        particle->particle->fields.b = 0;
        const mc_java_random before = *particle->random;
        const uint64_t shared = atomic_load(&f->app.profile->math->state);
        const mc_entity_status status =
            mc_explosion_particle_tick(particle, f->app.registry, NULL, error);
        CHECK(status == (i == 5 ? MC_ENTITY_STATE_ERROR : MC_ENTITY_OK) &&
              !particle->particle->item_native.tick_active && particle->holds == 1 &&
              before.state == particle->random->state &&
              shared == atomic_load(&f->app.profile->math->state));
        if (i == 5)
            CHECK(error->particle.source_exception && !error->particle.native_policy &&
                  error->particle.source_pc == 60 &&
                  strcmp(error->particle.exception_class, "java/lang/ArithmeticException") == 0);
        const double position[] = {particle->base.x, particle->base.y, particle->base.z};
        const double previous[] = {particle->base.aJ, particle->base.aK, particle->base.aL};
        const double velocity[] = {particle->base.velocity_x, particle->base.velocity_y,
                                   particle->base.velocity_z};
        const double bounds[] = {particle->bounds->min_x, particle->bounds->min_y,
                                 particle->bounds->min_z, particle->bounds->max_x,
                                 particle->bounds->max_y, particle->bounds->max_z};
        printf("EXPLOSION_TICK {\"profile\":\"%s\",\"exception\":\"%s\",\"age\":%d,"
               "\"lifetime\":%d,\"icon\":%d,\"dead\":%s,\"ground\":%s,\"position\":",
               profiles[i], i == 5 ? "ArithmeticException" : "none", particle->particle->fields.e,
               particle->particle->fields.f, particle->particle->fields.b,
               particle->base.be ? "true" : "false", particle->base.aX ? "true" : "false");
        words(position, 3);
        fputs(",\"previous\":", stdout);
        words(previous, 3);
        fputs(",\"velocity\":", stdout);
        words(velocity, 3);
        fputs(",\"bounds\":", stdout);
        words(bounds, 6);
        puts("}");
        mc_entity_release(particle);
    }
    free(error);
    free(constructor);
    return true;
}

static bool collision_controls(fixture *f, mc_math_classes *math) {
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *chunk = NULL;
    CHECK(blocks != NULL);
    for (size_t x = 0; x < 16; ++x)
        for (size_t z = 0; z < 16; ++z)
            blocks[(x << 11) | (z << 7) | 63] = 3;
    CHECK(mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    CHECK(acquisition != NULL && state != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, state));
    mc_particle_error *constructor = calloc(1, sizeof(*constructor));
    mc_item_particle_tick_error *error = calloc(1, sizeof(*error));
    mc_entity *particle = NULL;
    CHECK(constructor != NULL && error != NULL && create(f, math, &particle, constructor));
    const double standing_y = 64.0 + (double).1f;
    CHECK(mc_entity_set_position(particle, 8.5, standing_y, 8.5) == MC_ENTITY_OK &&
          !particle->base.bq && particle->bounds->min_y == 64.0);
    particle->base.velocity_x = .125;
    particle->base.velocity_y = -.25;
    particle->base.velocity_z = .0625;
    particle->particle->fields.e = 0;
    particle->particle->fields.f = 8;
    const uint64_t random = particle->random->state;
    const uint64_t world_random = f->world->fields.r->state;
    CHECK(mc_explosion_particle_tick(particle, f->app.registry, NULL, error) == MC_ENTITY_OK &&
          particle->base.aX && particle->base.y == standing_y && particle->bounds->min_y == 64.0 &&
          particle->base.velocity_y == 0.0 && particle->particle->fields.e == 1 &&
          particle->particle->fields.b == 6 && particle->particle->item_native.tick_pc == 160 &&
          random == particle->random->state && world_random == f->world->fields.r->state);
    const double first_x = .125 * .8999999761581421;
    const double expected_x = first_x * .699999988079071;
    const double first_z = .0625 * .8999999761581421;
    const double expected_z = first_z * .699999988079071;
    CHECK(bits(particle->base.velocity_x) == bits(expected_x) &&
          bits(particle->base.velocity_z) == bits(expected_z) && particle->base.x == 8.625 &&
          particle->base.z == 8.5625 && particle->holds == 1);
    const mc_entity_base_fields before = particle->base;
    particle->particle->item_native.tick_active = true;
    CHECK(mc_explosion_particle_tick(particle, f->app.registry, NULL, error) == MC_ENTITY_BUSY &&
          memcmp(&before, &particle->base, sizeof(before)) == 0);
    particle->particle->item_native.tick_active = false;
    mc_entity_release(particle);
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK);
    free(error);
    free(constructor);
    return true;
}

static bool renderer_controls(fixture *f, mc_math_classes *math) {
    mc_client_effect_renderer *renderer = NULL;
    mc_effect_renderer_error *error = calloc(1, sizeof(*error));
    mc_particle_error *constructor = calloc(1, sizeof(*constructor));
    CHECK(error != NULL && constructor != NULL &&
          mc_client_effect_renderer_allocate(f->app.entities, NULL, &renderer, error) ==
              MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_construct(renderer, f->world, NULL, error) ==
              MC_EFFECT_RENDERER_OK);
    for (size_t i = 0; i < 20; ++i) {
        mc_entity *particle = NULL;
        CHECK(create(f, math, &particle, constructor));
        particle->base.bq = true;
        particle->particle->fields.f = 2; /* Declared finite component tick input. */
        CHECK(mc_client_effect_renderer_add(renderer, particle, error) == MC_EFFECT_RENDERER_OK &&
              particle->holds == 2);
        mc_entity_release(particle);
    }
    mc_effect_renderer_terminal_view view;
    CHECK(mc_client_effect_renderer_terminal_inspect(renderer, &view, error) ==
              MC_EFFECT_RENDERER_OK &&
          view.particle_occurrences == 20 && renderer->layers[0]->size == 20);
    for (size_t tick = 0; tick < 3; ++tick) {
        CHECK(mc_client_effect_renderer_tick_in_context(renderer, f->app.registry, NULL, error) ==
                  MC_EFFECT_RENDERER_OK &&
              renderer->layers[0]->size == (tick == 2 ? 0u : 20u));
    }
    mc_entity *particle = NULL;
    CHECK(create(f, math, &particle, constructor));
    particle->base.bq = true;
    particle->particle->fields.f = 0;
    CHECK(mc_client_effect_renderer_add(renderer, particle, error) == MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_tick_in_context(renderer, f->app.registry, NULL, error) ==
              MC_EFFECT_RENDERER_ENTITY_FAILED &&
          error->source_exception &&
          strcmp(error->exception_class, "java/lang/ArithmeticException") == 0 &&
          renderer->layers[0]->size == 1 && renderer->tick_call != NULL &&
          mc_client_effect_renderer_tick_cleanup(renderer, error) == MC_EFFECT_RENDERER_OK &&
          renderer->tick_call == NULL &&
          mc_client_effect_renderer_change_world(renderer, NULL, error) == MC_EFFECT_RENDERER_OK &&
          particle->holds == 1);
    mc_entity_release(particle);
    CHECK(mc_client_effect_renderer_free(&renderer) == MC_EFFECT_RENDERER_OK && renderer == NULL);
    free(constructor);
    free(error);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    mc_math_classes *math = NULL;
    if (f == NULL || !setup(f, argv[1], "explosion", true) ||
        mc_math_classes_create(SIZE_MAX, &math, NULL) != MC_MATH_CLASS_OK ||
        !constructor_controls(f, math) || !tick_profiles(f, math) || !collision_controls(f, math) ||
        !renderer_controls(f, math) || mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK ||
        !teardown(f))
        return 1;
    free(f);
    printf("PASS: %zu Explosion particle source-order/ownership checks\n", checks);
    return 0;
}
