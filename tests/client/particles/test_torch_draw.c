/* Reuse the genuine owner setup and field transport; its main is not run. */
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 4
#define MC_TORCH_PARTICLE_MAIN mc_torch_unused_tick_main
#include "../../entity/particles/test_torch.c"
#undef MC_TORCH_PARTICLE_MAIN
#undef MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS

#include "client/particles/render_internal.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/nw.h"
#include "world/storage/generated_internal.h"

static bool torch_lights(fixture *f, mc_chunk_state **chunks, int mode) {
    for (size_t i = 0; i < 4; ++i) {
        CHECK(chunks[i] != NULL && chunks[i]->data != NULL && chunks[i]->data->blocks != NULL &&
              chunks[i]->data->blocks_capacity >= MC_CHUNK_BLOCKS);
        memset(chunks[i]->data->blocks, 0, MC_CHUNK_BLOCKS);
        memset(chunks[i]->data->metadata, 0, sizeof(chunks[i]->data->metadata));
        memset(chunks[i]->data->skylight, mode == 1 ? 255 : 0, sizeof(chunks[i]->data->skylight));
        memset(chunks[i]->data->block_light, mode == 2 ? 255 : 0,
               sizeof(chunks[i]->data->block_light));
    }
    f->world->fields.f = 3;
    return true;
}

static bool torch_draw_case(fixture *f, mc_math_classes *math, mc_client_effect_renderer *effects,
                            mc_chunk_state **chunks, size_t index, const torch_profile *profile,
                            uint64_t *untouched) {
    mc_particle_error *constructor = calloc(1, sizeof(*constructor));
    mc_effect_draw_error *error = calloc(1, sizeof(*error));
    mc_client_graphics_error *graphics_error = calloc(1, sizeof(*graphics_error));
    mc_entity *p = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_tessellator *nw = NULL;
    mc_client_effect_draw_call *call = NULL;
    CHECK(constructor != NULL && error != NULL && graphics_error != NULL &&
          torch_create(f, math, profile, &p, constructor) && torch_prepare(p, profile) &&
          torch_lights(f, chunks, profile->light) &&
          mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &graphics) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(graphics, (mc_client_graphics_site){"dn", "a", "(Lsn;F)V", 228},
                                  &nw, graphics_error) &&
          mc_client_nw_begin_quads(nw, graphics_error));
    mc_client_nw_fields before_nw;
    CHECK(mc_client_nw_fields_get(nw, &before_nw) && before_nw.g != NULL && before_nw.o == 0);
    CHECK(before_nw.d != NULL && before_nw.e != NULL && before_nw.f != NULL && before_nw.y == NULL);
    const size_t cursors[] = {before_nw.d->position, before_nw.d->limit, before_nw.d->capacity,
                              before_nw.e->position, before_nw.e->limit, before_nw.e->capacity,
                              before_nw.f->position, before_nw.f->limit, before_nw.f->capacity};
    int32_t *prior = malloc(2097152u * sizeof(*prior));
    CHECK(prior != NULL);
    memcpy(prior, before_nw.g, 2097152u * sizeof(*prior));
    const mc_effect_draw_services services = {.math = math, .graphics = graphics};
    CHECK(mc_client_effect_renderer_sprites_begin(effects, f->player, torch_float(profile->partial),
                                                  &services, &call, error) == MC_EFFECT_DRAW_OK &&
          call != NULL && call->state.empty && call->state.returned &&
          mc_entity_retain(p) == MC_ENTITY_OK);
    /* The returned empty DN invocation computes its own camera origin. Author
     * the direct component's declared XW inputs after that setup, matching the
     * original observer and the existing direct Explosion sprite fixture. */
    CHECK(mc_entity_particle_origin_store(f->app.entities, 0, .25, NULL) == MC_ENTITY_OK &&
          mc_entity_particle_origin_store(f->app.entities, 1, .5, NULL) == MC_ENTITY_OK &&
          mc_entity_particle_origin_store(f->app.entities, 2, .75, NULL) == MC_ENTITY_OK);
    mc_particle_origin origin_before;
    CHECK(mc_entity_particle_origin_read(f->app.entities, &origin_before, NULL) == MC_ENTITY_OK &&
          torch_dbits(origin_before.l) == torch_dbits(.25) &&
          torch_dbits(origin_before.m) == torch_dbits(.5) &&
          torch_dbits(origin_before.n) == torch_dbits(.75));
    call->current = p;
    call->state.current = p;
    call->nw = strcmp(profile->operation, "draw_null_nw") == 0 ? NULL : nw;
    call->state.partial = torch_float(profile->partial);
    call->state.cos_yaw = profile->orientation ? .75f : 1;
    call->state.cos_pitch = profile->orientation ? .5f : 1;
    call->state.sin_yaw = profile->orientation ? -.25f : 0;
    call->state.horizontal_y = profile->orientation ? .125f : 0;
    call->state.horizontal_x = profile->orientation ? -.375f : 0;
    if (profile->null_world)
        p->base.world = NULL;
    const torch_saved before = torch_save(p);
    const uint64_t own = p->random->state, shared = atomic_load(&f->app.profile->math->state),
                   world = f->world->fields.r->state;
    const mc_entity_bounds *bounds = p->bounds;
    const bool returned = mc_effect_draw_animated_sprite(call, 291),
               failed = profile->null_world || strcmp(profile->operation, "draw_null_nw") == 0;
    CHECK(returned != failed && p->bounds == bounds && p->random->state == own &&
          shared == atomic_load(&f->app.profile->math->state) &&
          world == f->world->fields.r->state);
    mc_particle_origin origin_after;
    CHECK(mc_entity_particle_origin_read(f->app.entities, &origin_after, NULL) == MC_ENTITY_OK &&
          torch_dbits(origin_after.l) == torch_dbits(origin_before.l) &&
          torch_dbits(origin_after.m) == torch_dbits(origin_before.m) &&
          torch_dbits(origin_after.n) == torch_dbits(origin_before.n));
    mc_client_nw_fields after_nw;
    CHECK(mc_client_nw_fields_get(nw, &after_nw));
    CHECK(before_nw.d == after_nw.d && before_nw.e == after_nw.e && before_nw.f == after_nw.f &&
          before_nw.g == after_nw.g && before_nw.y == after_nw.y);
    CHECK(cursors[0] == after_nw.d->position && cursors[1] == after_nw.d->limit &&
          cursors[2] == after_nw.d->capacity && cursors[3] == after_nw.e->position &&
          cursors[4] == after_nw.e->limit && cursors[5] == after_nw.e->capacity &&
          cursors[6] == after_nw.f->position && cursors[7] == after_nw.f->limit &&
          cursors[8] == after_nw.f->capacity);
    CHECK(after_nw.o == (failed ? 0 : 48) && after_nw.h == (failed ? 0 : 6) &&
          after_nw.p == (failed ? 0 : 4));
    for (size_t i = (size_t)after_nw.o; i < 2097152u; ++i)
        CHECK(prior[i] == after_nw.g[i]);
    *untouched += 2097152u - (uint64_t)after_nw.o;
    if (failed) {
        CHECK(call->failure.source_exception && !call->failure.native_policy &&
              strcmp(call->failure.exception_class, "java/lang/NullPointerException") == 0 &&
              call->failure.child.pc == (profile->null_world ? 123 : 150) &&
              strcmp(call->failure.child.class_name, "xw") == 0 && !call->state.color_set &&
              call->state.item_vertices_returned == 0 &&
              call->state.brightness_returned == !profile->null_world &&
              call->failure.sprite_caller.pc == (torch_flame(profile) ? 46 : 61) &&
              strcmp(call->failure.sprite_caller.class_name, torch_flame(profile) ? "qu" : "xn") ==
                  0 &&
              strcmp(call->failure.sprite_caller.method, "a") == 0 &&
              strcmp(call->failure.sprite_caller.descriptor, "(Lnw;FFFFFF)V") == 0);
        if (profile->null_world)
            CHECK(call->failure.entity_render != NULL &&
                  call->failure.entity_render->site.pc == 118 &&
                  strcmp(call->failure.entity_render->site.class_name, "sn") == 0 &&
                  (!torch_flame(profile) ||
                   (call->failure.entity_render->parent.pc == 32 &&
                    strcmp(call->failure.entity_render->parent.class_name, "qu") == 0)));
    } else
        CHECK(call->state.item_vertices_returned == 4 && call->state.item_particle_returned &&
              call->state.brightness_returned && call->state.color_set &&
              call->state.particle_pc == (torch_flame(profile) ? 49u : 64u));
    torch_prefix(index, profile, failed ? "NullPointerException" : "none", &before);
    const torch_saved after = torch_save(p);
    torch_state(&after);
    printf(",\"brightness\":null,\"nw\":{\"vertices\":%" PRId32 ",\"source_vertices\":%" PRId32
           ",\"words\":%" PRId32 ",\"uv\":%s,\"color\":%s,\"normal\":%s,\"begun\":%s},\"packed\":[",
           after_nw.h, after_nw.p, after_nw.o, torch_bool(after_nw.m), torch_bool(after_nw.l),
           torch_bool(after_nw.n), torch_bool(after_nw.w));
    for (size_t i = 0; i < 100; ++i)
        printf("%s\"%08" PRIx32 "\"", i ? "," : "", (uint32_t)after_nw.g[i]);
    puts("]}");
    p->base.world = f->world;
    CHECK(mc_client_effect_renderer_draw_retire(&call, error) == MC_EFFECT_DRAW_OK &&
          call == NULL && p->holds == 1 &&
          mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_entity_release(p);
    free(prior);
    free(constructor);
    free(error);
    free(graphics_error);
    return true;
}

static bool torch_brightness_case(fixture *f, mc_math_classes *math, mc_chunk_state **chunks,
                                  size_t index, const torch_profile *profile) {
    mc_particle_error *constructor = calloc(1, sizeof(*constructor));
    mc_entity_render_error *error = calloc(1, sizeof(*error));
    mc_entity *p = NULL;
    CHECK(constructor != NULL && error != NULL && torch_create(f, math, profile, &p, constructor) &&
          torch_prepare(p, profile) && torch_lights(f, chunks, profile->light));
    if (profile->null_world)
        p->base.world = NULL;
    const torch_saved before = torch_save(p);
    const uint64_t own = p->random->state, shared = atomic_load(&f->app.profile->math->state),
                   world = f->world->fields.r->state;
    float result = 123.25f;
    const bool returned =
        mc_entity_render_brightness(p, math, torch_float(profile->partial), &result, error);
    CHECK(returned == !profile->null_world && p->random->state == own &&
          shared == atomic_load(&f->app.profile->math->state) &&
          world == f->world->fields.r->state);
    if (profile->null_world)
        CHECK(torch_fbits(result) == torch_fbits(123.25f) && error->source_exception &&
              !error->native_policy && error->site.pc == 118 &&
              strcmp(error->site.class_name, "sn") == 0 && error->parent.pc == 32 &&
              strcmp(error->parent.class_name, "qu") == 0 &&
              strcmp(error->exception_class, "java/lang/NullPointerException") == 0);
    else
        CHECK(error->site.pc == 43 && strcmp(error->site.class_name, "qu") == 0 &&
              error->query_count == 2 && error->queries[0].returned && error->queries[1].returned);
    torch_prefix(index, profile, returned ? "none" : "NullPointerException", &before);
    const torch_saved after = torch_save(p);
    torch_state(&after);
    if (returned)
        printf(",\"brightness\":\"%08" PRIx32 "\"", torch_fbits(result));
    else
        fputs(",\"brightness\":null", stdout);
    puts(",\"nw\":null,\"packed\":[]}");
    p->base.world = f->world;
    CHECK(mc_entity_render_worlds_release(error) == MC_ENTITY_OK && p->holds == 1);
    mc_entity_release(p);
    free(constructor);
    free(error);
    return true;
}

static bool torch_fp_controls(fixture *f, mc_math_classes *math,
                              mc_client_effect_renderer *effects) {
    for (size_t kind = 0; kind < 2; ++kind) {
        torch_profile profile = {.age = 0,
                                 .lifetime = 8,
                                 .texture = 7,
                                 .g = UINT32_C(0x40600000),
                                 .a = UINT32_C(0x3fa00000),
                                 .partial = UINT32_C(0x3f000000),
                                 .vy = UINT64_C(0xbfd0000000000000)};
        strcpy(profile.kind, kind ? "flame" : "smoke_default");
        strcpy(profile.operation, "draw");
        mc_particle_error *constructor = calloc(1, sizeof(*constructor));
        mc_effect_draw_error *error = calloc(1, sizeof(*error));
        mc_entity *p = NULL;
        mc_client_effect_draw_call *call = NULL;
        CHECK(constructor != NULL && error != NULL &&
              torch_create(f, math, &profile, &p, constructor) && torch_prepare(p, &profile));
        const mc_effect_draw_services services = {.math = math};
        CHECK(mc_client_effect_renderer_sprites_begin(effects, f->player, .5f, &services, &call,
                                                      error) == MC_EFFECT_DRAW_OK &&
              mc_entity_retain(p) == MC_ENTITY_OK);
        call->current = p;
        call->state.current = p;
        call->state.partial = .5f;
        const uint32_t before = torch_fbits(p->particle->fields.g);
        const unsigned controls = _mm_getcsr();
        _mm_setcsr(controls | UINT32_C(0x8000));
        const bool returned = mc_effect_draw_animated_sprite(call, 291);
        _mm_setcsr(controls);
        CHECK(!returned && call->failure.native_policy && !call->failure.source_exception &&
              call->failure.status == MC_EFFECT_DRAW_UNSUPPORTED_FP &&
              call->failure.child.pc == 4 && torch_fbits(p->particle->fields.g) == before &&
              call->state.item_vertices_returned == 0 &&
              mc_client_effect_renderer_draw_retire(&call, error) == MC_EFFECT_DRAW_OK &&
              p->holds == 1);
        mc_entity_release(p);
        free(constructor);
        free(error);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    mc_math_classes *math = NULL;
    mc_client_effect_renderer *effects = NULL;
    FILE *profiles = fopen(argv[3], "rb");
    if (f == NULL || profiles == NULL || !setup(f, argv[1], "torch-draw", true)) {
        fprintf(stderr, "[TORCH_DRAW_SETUP] initial owner/profile/setup failed\n");
        return 1;
    }
    const mc_math_class_status math_status = mc_math_classes_create(SIZE_MAX, &math, NULL);
    if (math_status != MC_MATH_CLASS_OK) {
        fprintf(stderr, "[TORCH_DRAW_SETUP] Math owner status=%d\n", (int)math_status);
        return 1;
    }
    mc_chunk_state *chunks[4] = {0};
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
            mc_world_chunk *chunk = NULL;
            if (blocks == NULL) {
                fprintf(stderr, "[TORCH_DRAW_SETUP] block allocation x=%d z=%d\n", x, z);
                return 1;
            }
            const mc_world_storage_status status = mc_world_store_create_generated(
                f->store, f->world, 0, x, z, &blocks, MC_CHUNK_BLOCKS, &chunk, NULL);
            if (status != MC_WORLD_STORAGE_OK || blocks != NULL) {
                fprintf(
                    stderr,
                    "[TORCH_DRAW_SETUP] generated Chunk x=%d z=%d status=%d blocks-retained=%d\n",
                    x, z, (int)status, blocks != NULL);
                return 1;
            }
            chunks[x * 2 + z] = mc_world_chunk_state(chunk);
            if (acquisition == NULL || chunks[x * 2 + z] == NULL ||
                !mc_chunk_cache_publish(acquisition->cache, x, z, chunks[x * 2 + z])) {
                fprintf(stderr,
                        "[TORCH_DRAW_SETUP] cache publication x=%d z=%d acquisition=%d Chunk=%d "
                        "count=%zu capacity=%zu insertions=%zu insertion-capacity=%zu\n",
                        x, z, acquisition != NULL, chunks[x * 2 + z] != NULL,
                        acquisition == NULL ? 0u : acquisition->cache->count,
                        acquisition == NULL ? 0u : acquisition->cache->capacity,
                        acquisition == NULL ? 0u : acquisition->cache->insertion_count,
                        acquisition == NULL ? 0u : acquisition->cache->insertion_capacity);
                return 1;
            }
        }
    mc_effect_renderer_status effects_status =
        mc_client_effect_renderer_allocate(f->app.entities, NULL, &effects, NULL);
    if (effects_status == MC_EFFECT_RENDERER_OK)
        effects_status = mc_client_effect_renderer_construct(effects, f->world, NULL, NULL);
    if (effects_status != MC_EFFECT_RENDERER_OK) {
        fprintf(stderr, "[TORCH_DRAW_SETUP] effect renderer status=%d\n", (int)effects_status);
        return 1;
    }
    size_t index = 0, draws = 0, brightness = 0;
    uint64_t untouched = 0;
    bool eof = false;
    torch_profile profile;
    while (torch_read(profiles, &profile, &eof) && !eof) {
        if (strncmp(profile.operation, "draw", 4) == 0) {
            if (!torch_draw_case(f, math, effects, chunks, index, &profile, &untouched))
                return 1;
            ++draws;
        } else if (strcmp(profile.operation, "brightness") == 0) {
            if (!torch_brightness_case(f, math, chunks, index, &profile))
                return 1;
            ++brightness;
        }
        ++index;
    }
    if (!eof || fclose(profiles) != 0 || index != 36 || draws != 12 || brightness != 6 ||
        untouched != UINT64_C(25165440) || !torch_fp_controls(f, math, effects) ||
        mc_client_effect_renderer_free(&effects) != MC_EFFECT_RENDERER_OK ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK ||
        mc_world_runtime_retire_entity_containers(f->world, NULL) != MC_WORLD_STORAGE_OK ||
        !mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                             acquisition->cache->capacity, acquisition->cache->insertions,
                             acquisition->cache->insertion_capacity))
        return 1;
    for (size_t i = 0; i < 4; ++i)
        if (mc_world_store_retire_chunk(f->store, chunks[i], NULL) != MC_WORLD_STORAGE_OK)
            return 1;
    if (!teardown(f))
        return 1;
    free(f);
    printf("TORCH_PARTICLE_NATIVE draws=12 brightness=6 own-unchanged-words=%" PRIu64
           " fp-admission=2 cleanup=1 checks=%zu\n",
           untouched, checks);
    return 0;
}
