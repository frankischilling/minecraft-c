/* Genuine World/Player owners; the renamed statistics entry is never run.
 * Direct IM/XW cases use declared field operands on a constructed IM and a
 * real returned empty DN invocation. The separate nonempty DN case reaches
 * its actual layer-zero atlas lookup and retains native display admission failure.
 * Neither CPU path claims a successful graphical DN draw. */
#define main mc_redstone_draw_statistics_fixture_main
#include "../statistics/test_player_dispatch.c"
#undef main

#include "client/particles/render_internal.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/nw.h"
#include "core/java/random_profile_internal.h"
#include "entity/particles/particle_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"

#include <inttypes.h>
#include <math.h>
#include <stdatomic.h>
#include <xmmintrin.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint32_t redstone_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool redstone_create(fixture *f, mc_math_classes *math, mc_entity **out) {
    mc_particle_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    const mc_entity_status allocated = mc_redstone_particle_allocate(f->app.entities, out, NULL);
    const mc_entity_status constructed =
        allocated == MC_ENTITY_OK
            ? mc_redstone_particle_construct(*out, f->world, math, 3, 4, 5, 1, .2f, 0, error)
            : allocated;
    free(error);
    CHECK(constructed == MC_ENTITY_OK && (*out)->phase == MC_ENTITY_READY &&
          mc_entity_is_kind(*out, MC_ENTITY_CLASS_KIND_REDSTONE_PARTICLE));
    /* Ordinary source-field inputs, matching the original component probes.
     * No class, phase, List, hold count or source action result is supplied. */
    (*out)->particle->fields.b = 7;
    (*out)->particle->fields.g = 3.5f;
    (*out)->particle->fields.i = .5f;
    (*out)->particle->fields.j = .25f;
    (*out)->particle->fields.k = .75f;
    (*out)->particle->redstone.a = 1.25f;
    (*out)->base.aJ = 1;
    (*out)->base.aK = 2;
    (*out)->base.aL = 3;
    return true;
}

typedef struct redstone_draw_profile {
    int32_t age, lifetime;
    float partial, fraction, scale, half;
    float center[3];
} redstone_draw_profile;

static bool redstone_billboard(fixture *f, mc_math_classes *math,
                               mc_client_effect_renderer *effects,
                               const redstone_draw_profile *profile, unsigned int failure) {
    mc_entity *particle = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_tessellator *nw = NULL;
    mc_client_effect_draw_call *call = NULL;
    mc_effect_draw_error *error = calloc(1, sizeof(*error));
    mc_client_graphics_error *graphics_error = calloc(1, sizeof(*graphics_error));
    CHECK(error != NULL && graphics_error != NULL && redstone_create(f, math, &particle) &&
          mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &graphics) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(graphics, (mc_client_graphics_site){"dn", "a", "(Lsn;F)V", 228},
                                  &nw, graphics_error));
    CHECK(mc_client_nw_begin_quads(nw, graphics_error));
    mc_client_nw_fields before;
    CHECK(mc_client_nw_fields_get(nw, &before) && before.g != NULL && before.o == 0);
    int32_t old_words[96];
    memcpy(old_words, before.g, sizeof(old_words));
    const mc_effect_draw_services services = {.math = math, .graphics = graphics};
    CHECK(mc_client_effect_renderer_sprites_begin(effects, f->player, profile->partial, &services,
                                                  &call, error) == MC_EFFECT_DRAW_OK &&
          call != NULL && call->state.empty && call->state.returned &&
          mc_entity_retain(particle) == MC_ENTITY_OK);
    CHECK(mc_entity_particle_origin_store(f->app.entities, 0, .25, NULL) == MC_ENTITY_OK &&
          mc_entity_particle_origin_store(f->app.entities, 1, .5, NULL) == MC_ENTITY_OK &&
          mc_entity_particle_origin_store(f->app.entities, 2, .75, NULL) == MC_ENTITY_OK);
    particle->particle->fields.e = profile->age;
    particle->particle->fields.f = profile->lifetime;
    call->current = particle;
    call->state.current = particle;
    call->nw = failure == 1 ? NULL : nw;
    call->state.partial = profile->partial;
    call->state.cos_yaw = call->state.cos_pitch = 1;
    call->state.sin_yaw = call->state.horizontal_y = call->state.horizontal_x = 0;
    if (failure == 2)
        particle->base.world = NULL;
    const mc_java_random own = *particle->random, world = *f->world->fields.r;
    const uint64_t shared = atomic_load(&f->app.profile->math->state);
    const unsigned int controls = _mm_getcsr();
    if (failure == 4)
        _mm_setcsr(controls | UINT32_C(0x8000));
    const bool returned = mc_effect_draw_animated_sprite(call, 291);
    _mm_setcsr(controls);
    particle->base.world = f->world;
    CHECK(returned == (failure == 0));
    CHECK(memcmp(&own, particle->random, sizeof(own)) == 0);
    CHECK(memcmp(&world, f->world->fields.r, sizeof(world)) == 0);
    CHECK(shared == atomic_load(&f->app.profile->math->state));
    mc_client_nw_fields after;
    CHECK(mc_client_nw_fields_get(nw, &after) && after.g == before.g && after.d == before.d &&
          after.e == before.e && after.f == before.f);
    if (failure == 4) {
        CHECK(call->failure.status == MC_EFFECT_DRAW_UNSUPPORTED_FP &&
              call->failure.native_policy && !call->failure.source_exception &&
              strcmp(call->failure.child.class_name, "im") == 0 && call->failure.child.pc == 4 &&
              redstone_bits(particle->particle->fields.g) == redstone_bits(3.5f));
    } else {
        CHECK(redstone_bits(particle->particle->fields.g) == redstone_bits(profile->scale) &&
              redstone_bits(call->state.fraction) == redstone_bits(profile->fraction));
    }
    if (failure != 0 && failure != 4) {
        CHECK(call->failure.source_exception && !call->failure.native_policy &&
              call->failure.exception_class != NULL &&
              strcmp(call->failure.sprite_caller.class_name, "im") == 0 &&
              call->failure.sprite_caller.pc == 61 && call->state.item_vertices_returned == 0);
        CHECK(call->failure.child.pc == (failure == 1 ? 150 : 123) &&
              strcmp(call->failure.child.class_name, "xw") == 0 &&
              strcmp(call->failure.exception_class, "java/lang/NullPointerException") == 0 &&
              call->state.brightness_returned == (failure != 2) && !call->state.color_set);
    }
    if (failure == 0) {
        CHECK(after.o == 48 && after.h == 6 && after.p == 4 && after.m && after.l && !after.n &&
              call->state.particle_pc == 64 && call->state.item_particle_returned &&
              call->state.item_vertices_returned == 4 && call->state.brightness_returned &&
              call->state.brightness == 1 && call->state.color_set &&
              redstone_bits(call->state.item_half_size) == redstone_bits(profile->half) &&
              call->state.item_u0 == .4375f && call->state.item_v0 == 0 &&
              call->state.item_u1 == .4375f + .0624375f && call->state.item_v1 == .0624375f);
        static const unsigned int corners[6] = {0, 1, 2, 0, 2, 3};
        for (size_t i = 0; i < 6; ++i) {
            const unsigned int corner = corners[i];
            const float x = profile->center[0] + (corner >= 2 ? profile->half : -profile->half);
            const float y =
                profile->center[1] + (corner == 1 || corner == 2 ? profile->half : -profile->half);
            const float u = corner >= 2 ? .4375f : .4375f + .0624375f;
            const float v = corner == 1 || corner == 2 ? 0 : .0624375f;
            const size_t at = i * 8u;
            CHECK((uint32_t)after.g[at] == redstone_bits(x) &&
                  (uint32_t)after.g[at + 1u] == redstone_bits(y) &&
                  (uint32_t)after.g[at + 2u] == redstone_bits(profile->center[2]) &&
                  (uint32_t)after.g[at + 3u] == redstone_bits(u) &&
                  (uint32_t)after.g[at + 4u] == redstone_bits(v) &&
                  (uint32_t)after.g[at + 5u] == UINT32_C(0xffbf3f7f));
        }
    } else
        CHECK(after.o == 0 && after.h == 0 && after.p == 0);
    for (size_t i = failure == 0 ? 48u : 0u; i < COUNT(old_words); ++i)
        CHECK(after.g[i] == old_words[i]);
    CHECK(particle->holds == 2 &&
          mc_client_effect_renderer_draw_retire(&call, error) == MC_EFFECT_DRAW_OK &&
          call == NULL && particle->holds == 1 &&
          mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_entity_release(particle);
    free(error);
    free(graphics_error);
    return true;
}

static bool redstone_atlas_prefix(fixture *f, mc_math_classes *math) {
    mc_entity *particle = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_default_font *font = NULL;
    mc_client_render_engine *engine = NULL;
    mc_client_effect_renderer *effects = NULL;
    mc_client_effect_draw_call *call = NULL;
    mc_effect_draw_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL && redstone_create(f, math, &particle) &&
          mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_default_font_create(32u * 1024u * 1024u, &font) == MC_CLIENT_FONT_OK &&
          mc_client_render_engine_allocate(graphics, font, NULL, &engine) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_construct(engine, NULL, NULL, NULL) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_effect_renderer_allocate(f->app.entities, NULL, &effects, NULL) ==
              MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_construct(effects, f->world, engine, NULL) ==
              MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_add(effects, particle, NULL) == MC_EFFECT_RENDERER_OK);
    const mc_effect_draw_services services = {.math = math, .graphics = graphics};
    const float scale = particle->particle->fields.g;
    const mc_effect_draw_status status =
        mc_client_effect_renderer_sprites_begin(effects, f->player, .5f, &services, &call, error);
    CHECK(status == MC_EFFECT_DRAW_CHILD_FAILED && call != NULL && call->state.layer == 0 &&
          call->state.size_reads == 1 && call->state.visited == 0 &&
          call->state.orientation_values == 5 && call->state.origins_stored == 3 &&
          !call->state.nw_captured && !call->state.returned && call->failure.site.pc == 181 &&
          call->failure.texture != NULL && call->failure.texture->site.pc == -1 &&
          call->failure.native_policy &&
          call->failure.texture->status == MC_CLIENT_LOOKUP_INVALID_ARGUMENT &&
          call->failure.texture->operation != NULL &&
          strcmp(call->failure.texture->operation, "lookup native resource namespace") == 0 &&
          particle->particle->fields.g == scale && particle->holds == 2);
    CHECK(!mc_client_texture_lookup_pending(engine) && engine->lookup == NULL &&
          mc_client_effect_renderer_free(&effects) == MC_EFFECT_RENDERER_BUSY);
    CHECK(mc_client_effect_renderer_draw_retire(&call, error) == MC_EFFECT_DRAW_OK &&
          !mc_client_texture_lookup_pending(engine) && particle->holds == 2 &&
          mc_client_effect_renderer_free(&effects) == MC_EFFECT_RENDERER_OK &&
          particle->holds == 1 && mc_client_texture_lookup_free_metadata(engine) &&
          mc_client_render_engine_release(engine) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_default_font_release(font) == MC_CLIENT_FONT_OK &&
          mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_entity_release(particle);
    free(error);
    return true;
}

static bool redstone_controls(fixture *f) {
    mc_math_classes *math = NULL;
    mc_client_effect_renderer *effects = NULL;
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_client_effect_renderer_allocate(f->app.entities, NULL, &effects, NULL) ==
              MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_construct(effects, f->world, NULL, NULL) ==
              MC_EFFECT_RENDERER_OK);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *chunk = NULL;
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    CHECK(state != NULL && mc_chunk_cache_publish(acquisition->cache, 0, 0, state));
    memset(state->data->skylight, 255, MC_CHUNK_NIBBLE_BYTES);
    f->world->fields.f = 0;
    /* Literal expected fractions, sizes and interpolated centers are derived
     * from the Source equations at these exactly representable operands. */
    static const redstone_draw_profile profiles[] = {
        {0, 8, 0, 0, 0, 0, {.75f, 1.5f, 2.25f}},
        {0, 8, .125f, .5f, .625f, .0625f, {1, 1.75f, 2.5f}},
        {1, 8, 0, 1, 1.25f, .125f, {.75f, 1.5f, 2.25f}},
        {-1, 8, -.5f, 0, 0, 0, {-.25f, .5f, 1.25f}},
        {8, 8, 1, 1, 1.25f, .125f, {2.75f, 3.5f, 4.25f}},
        {0, 0, 1, 1, 1.25f, .125f, {2.75f, 3.5f, 4.25f}},
        {0, 0, -1, 0, 0, 0, {-1.25f, -.5f, .25f}},
        {1, 1024, 0, .03125f, .0390625f, .00390625f, {.75f, 1.5f, 2.25f}}};
    for (size_t i = 0; i < COUNT(profiles); ++i)
        CHECK(redstone_billboard(f, math, effects, &profiles[i], 0));
    static const unsigned int failures[] = {1, 2, 4};
    for (size_t i = 0; i < COUNT(failures); ++i)
        CHECK(redstone_billboard(f, math, effects, &profiles[1], failures[i]));
    CHECK(redstone_atlas_prefix(f, math) &&
          mc_client_effect_renderer_free(&effects) == MC_EFFECT_RENDERER_OK &&
          mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK &&
          mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], argv[2], true) || !redstone_controls(f) || !teardown(f))
        return 1;
    free(f);
    printf("PASS: %zu Redstone draw CPU prefix and atlas-failure checks\n", checks);
    return 0;
}
