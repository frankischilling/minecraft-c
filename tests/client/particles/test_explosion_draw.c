/* Actual XW body over a constructed GY, World and CPU NW. The returned empty
 * DN call supplies a real native invocation owner. Direct body operands below
 * are declared scalar inputs; no GL draw, matrix or pixel parity is credited. */
#define main mc_explosion_statistics_fixture_main
#include "../statistics/test_player_dispatch.c"
#undef main

#include "client/particles/render_internal.h"
#include "client/render_engine/nw.h"
#include "entity/particles/particle_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"
#include <inttypes.h>

static uint32_t sprite_bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool sprite_case(fixture *f, mc_math_classes *math, mc_client_effect_renderer *effects,
                        size_t index) {
    static const int32_t textures[] = {0, 7, 16, -1, 256, 7};
    static const float partials[] = {0, .5f, 1, .5f, 1, -.5f};
    static const float scales[] = {1, .5f, 2, 1, 1, -1};
    mc_entity *particle = NULL;
    mc_particle_error *particle_error = calloc(1, sizeof(*particle_error));
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_effect_draw_call *call = NULL;
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error *graphics_error = calloc(1, sizeof(*graphics_error));
    mc_effect_draw_error *draw_error = calloc(1, sizeof(*draw_error));
    CHECK(particle_error != NULL && graphics_error != NULL && draw_error != NULL &&
          mc_explosion_particle_allocate(f->app.entities, &particle, NULL) == MC_ENTITY_OK &&
          mc_explosion_particle_construct(particle, f->world, math, 3, 4, 5, .01, .02, .03,
                                          particle_error) == MC_ENTITY_OK &&
          particle->phase == MC_ENTITY_READY &&
          mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &graphics) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(graphics, (mc_client_graphics_site){"dn", "a", "(Lsn;F)V", 228},
                                  &nw, graphics_error) &&
          mc_client_nw_begin_quads(nw, graphics_error));
    const mc_effect_draw_services services = {.math = math, .graphics = graphics};
    CHECK(mc_client_effect_renderer_sprites_begin(effects, f->player, partials[index], &services,
                                                  &call, draw_error) == MC_EFFECT_DRAW_OK &&
          call != NULL && call->state.empty && call->state.returned);
    /* Supply the independently evaluated arguments to GY.a -> XW.a. These
     * change no native phase, List membership or hold count directly. */
    particle->particle->fields.b = textures[index];
    particle->particle->fields.g = scales[index];
    particle->particle->fields.i = .25f;
    particle->particle->fields.j = .5f;
    particle->particle->fields.k = .75f;
    particle->base.aJ = 1;
    particle->base.aK = 2;
    particle->base.aL = 3;
    CHECK(mc_entity_particle_origin_store(f->app.entities, 0, .25, NULL) == MC_ENTITY_OK &&
          mc_entity_particle_origin_store(f->app.entities, 1, .5, NULL) == MC_ENTITY_OK &&
          mc_entity_particle_origin_store(f->app.entities, 2, .75, NULL) == MC_ENTITY_OK &&
          mc_entity_retain(particle) == MC_ENTITY_OK);
    call->current = particle; /* Genuine retained expression-local occurrence. */
    call->state.current = particle;
    call->nw = nw;
    call->state.partial = partials[index];
    call->state.cos_yaw = index < 4 ? 1 : .75f;
    call->state.cos_pitch = index < 4 ? 1 : .5f;
    call->state.sin_yaw = index < 4 ? 0 : -.25f;
    call->state.horizontal_y = index < 4 ? 0 : .125f;
    call->state.horizontal_x = index < 4 ? 0 : -.375f;
    const mc_java_random random_before = *particle->random;
    const int32_t next_id = f->app.entities->next_id;
    const size_t bindings = f->world->persistent_bindings;
    CHECK(mc_effect_draw_sprite(call, 291) && call->state.particle_pc == 345 &&
          call->state.item_particle_returned && call->state.item_vertices_returned == 4 &&
          call->state.brightness_returned && call->state.color_set &&
          memcmp(&random_before, particle->random, sizeof(random_before)) == 0 &&
          next_id == f->app.entities->next_id && bindings == f->world->persistent_bindings);
    mc_client_nw_state buffer;
    CHECK(mc_client_nw_inspect(graphics, &buffer) && buffer.static_b && buffer.fields.w &&
          buffer.fields.h == 6 && buffer.fields.o == 48 && buffer.fields.g != NULL &&
          buffer.fields.l && buffer.fields.m && call->state.brightness == 1.0f);
    /* UVs are the full particle tile and preserve negative Java remainder.
     * XW's first two vertices use u1, unlike PB/QM's quarter-tile sprite. */
    CHECK(call->state.item_u0 == (float)(textures[index] % 16) / 16.0f &&
          call->state.item_v0 == (float)(textures[index] / 16) / 16.0f &&
          call->state.item_u1 == call->state.item_u0 + .0624375f &&
          call->state.item_v1 == call->state.item_v0 + .0624375f &&
          call->state.item_vertices[0][3] == call->state.item_u1 &&
          call->state.item_vertices[1][3] == call->state.item_u1 &&
          call->state.item_vertices[2][3] == call->state.item_u0 &&
          call->state.item_vertices[3][3] == call->state.item_u0);
    printf("EXPLOSION_DRAW profile=%zu brightness=%08" PRIx32 " words=", index,
           sprite_bits(call->state.brightness));
    for (size_t i = 0; i < 48; ++i)
        printf("%08" PRIx32 "%c", (uint32_t)buffer.fields.g[i], i == 47 ? '\n' : ',');
    CHECK(mc_client_effect_renderer_draw_retire(&call, draw_error) == MC_EFFECT_DRAW_OK &&
          call == NULL &&
          mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_entity_release(particle);
    free(particle_error);
    free(graphics_error);
    free(draw_error);
    return true;
}

static bool sprite_failures(fixture *f, mc_math_classes *math, mc_client_effect_renderer *effects) {
    mc_entity *particle = NULL;
    mc_particle_error *particle_error = calloc(1, sizeof(*particle_error));
    mc_effect_draw_error *error = calloc(1, sizeof(*error));
    CHECK(particle_error != NULL && error != NULL &&
          mc_explosion_particle_allocate(f->app.entities, &particle, NULL) == MC_ENTITY_OK &&
          mc_explosion_particle_construct(particle, f->world, math, 3, 4, 5, 0, 0, 0,
                                          particle_error) == MC_ENTITY_OK);
    const mc_effect_draw_services services = {.math = math};
    for (size_t kind = 0; kind < 2; ++kind) {
        mc_client_effect_draw_call *call = NULL;
        CHECK(mc_client_effect_renderer_sprites_begin(effects, f->player, .5f, &services, &call,
                                                      error) == MC_EFFECT_DRAW_OK &&
              mc_entity_retain(particle) == MC_ENTITY_OK);
        call->current = particle;
        call->state.current = particle;
        if (kind == 0)
            particle->base.world = NULL; /* Actual nullable SN brightness receiver. */
        CHECK(!mc_effect_draw_sprite(call, 291));
        particle->base.world = f->world;
        CHECK(call->state.item_vertices_returned == 0 && !call->state.color_set &&
              call->failure.child.pc == (kind == 0 ? 123 : 150) &&
              strcmp(call->failure.child.class_name, "xw") == 0 && call->failure.source_exception &&
              strcmp(call->failure.exception_class, "java/lang/NullPointerException") == 0 &&
              call->state.brightness_returned == (kind != 0) &&
              mc_client_effect_renderer_draw_retire(&call, error) == MC_EFFECT_DRAW_OK &&
              call == NULL);
    }
    mc_entity_release(particle);
    free(particle_error);
    free(error);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    mc_math_classes *math = NULL;
    mc_client_effect_renderer *effects = NULL;
    CHECK(f != NULL && setup(f, argv[1], "sprite", true) &&
          mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *chunk = NULL;
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK);
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(state != NULL && state->data != NULL);
    /* Declared genuine Chunk illumination, matching the original observer's
     * prepared full-light nibble arrays. The actual SN/World query still runs. */
    memset(state->data->skylight, 255, sizeof(state->data->skylight));
    memset(state->data->block_light, 255, sizeof(state->data->block_light));
    CHECK(acquisition != NULL && state != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, state) &&
          mc_client_effect_renderer_allocate(f->app.entities, NULL, &effects, NULL) ==
              MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_construct(effects, f->world, NULL, NULL) ==
              MC_EFFECT_RENDERER_OK);
    for (size_t i = 0; i < 6; ++i)
        CHECK(sprite_case(f, math, effects, i));
    CHECK(sprite_failures(f, math, effects) &&
          mc_client_effect_renderer_free(&effects) == MC_EFFECT_RENDERER_OK &&
          mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK &&
          mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK && teardown(f));
    free(f);
    printf("Explosion sprite: %zu checks; six actual XW/NW CPU profiles\n", checks);
    return 0;
}
