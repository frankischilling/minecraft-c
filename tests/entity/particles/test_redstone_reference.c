#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 4
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
/* Genuine App/World owner fixture; its earlier entry point is not run. */
#define main mc_redstone_unused_statistics_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "client/particles/effect_renderer_internal.h"
#include "core/java/random_profile_internal.h"
#include "entity/particles/particle_internal.h"
#include "world/runtime/runtime_internal.h"
#include <inttypes.h>
#include <stdatomic.h>
#include <xmmintrin.h>

typedef struct redstone_profile {
    char name[96], kind[32], operation[32];
    int32_t age, lifetime, texture;
    uint32_t g, a, partial;
    int ground, light, null_world;
    uint64_t vy;
    int orientation;
} redstone_profile;

typedef struct redstone_saved {
    mc_entity_base_fields base;
    mc_particle_fields fields;
    mc_entity_bounds bounds;
    float original_scale;
} redstone_saved;

static uint64_t redstone_dbits(double value) {
    uint64_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static uint32_t redstone_fbits(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static float redstone_float(uint32_t value) {
    float out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static double redstone_double(uint64_t value) {
    double out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static const char *redstone_bool(bool value) {
    return value ? "true" : "false";
}

static bool redstone_read(FILE *file, redstone_profile *p, bool *eof) {
    char line[512];
    if (fgets(line, sizeof(line), file) == NULL) {
        *eof = true;
        CHECK(feof(file));
        return true;
    }
    *eof = false;
    CHECK(strchr(line, '\n') != NULL);
    CHECK(sscanf(line,
                 "%95s%31s%31s%" SCNd32 "%" SCNd32 "%" SCNd32 "%" SCNx32 "%" SCNx32 "%" SCNx32
                 "%d%d%d%" SCNx64 "%d",
                 p->name, p->kind, p->operation, &p->age, &p->lifetime, &p->texture, &p->g, &p->a,
                 &p->partial, &p->ground, &p->light, &p->null_world, &p->vy,
                 &p->orientation) == 14);
    return true;
}

static redstone_saved redstone_save(const mc_entity *p) {
    return (redstone_saved){p->base, p->particle->fields, *p->bounds, p->particle->redstone.a};
}

static void redstone_dwords(const double *v, size_t count) {
    putchar('[');
    for (size_t i = 0; i < count; ++i)
        printf("%s\"%016" PRIx64 "\"", i ? "," : "", redstone_dbits(v[i]));
    putchar(']');
}

static void redstone_state(const redstone_saved *s) {
    const mc_entity_base_fields *b = &s->base;
    const mc_particle_fields *p = &s->fields;
    const double position[] = {b->x, b->y, b->z}, previous[] = {b->aJ, b->aK, b->aL},
                 velocity[] = {b->velocity_x, b->velocity_y, b->velocity_z};
    const double bounds[] = {s->bounds.min_x, s->bounds.min_y, s->bounds.min_z,
                             s->bounds.max_x, s->bounds.max_y, s->bounds.max_z};
    fputs("{\"position\":", stdout);
    redstone_dwords(position, 3);
    fputs(",\"previous\":", stdout);
    redstone_dwords(previous, 3);
    fputs(",\"velocity\":", stdout);
    redstone_dwords(velocity, 3);
    fputs(",\"bounds\":", stdout);
    redstone_dwords(bounds, 6);
    printf(",\"size\":[\"%08" PRIx32 "\",\"%08" PRIx32 "\",\"%08" PRIx32
           "\"],\"y_size\":\"%08" PRIx32 "\",\"texture\":%" PRId32 ",\"c\":\"%08" PRIx32
           "\",\"d\":\"%08" PRIx32 "\",\"age\":%" PRId32 ",\"lifetime\":%" PRId32
           ",\"scale\":\"%08" PRIx32 "\",\"gravity\":\"%08" PRIx32 "\",\"rgb\":[\"%08" PRIx32
           "\",\"%08" PRIx32 "\",\"%08" PRIx32 "\"],\"base_scale\":\"%08" PRIx32
           "\",\"dead\":%s,\"ground\":%s,\"no_clip\":%s,\"collision\":[%s,%s,%s,%s,%s,%s]}",
           redstone_fbits(b->offset), redstone_fbits(b->width), redstone_fbits(b->height),
           redstone_fbits(b->y_size), p->b, redstone_fbits(p->c), redstone_fbits(p->d), p->e, p->f,
           redstone_fbits(p->g), redstone_fbits(p->h), redstone_fbits(p->i), redstone_fbits(p->j),
           redstone_fbits(p->k), redstone_fbits(s->original_scale), redstone_bool(b->be),
           redstone_bool(b->aX), redstone_bool(b->bq), redstone_bool(b->aY), redstone_bool(b->aZ),
           redstone_bool(b->ba), redstone_bool(b->bb), redstone_bool(b->bc), redstone_bool(b->bd));
}

static bool redstone_create(fixture *f, mc_math_classes *math, const redstone_profile *p,
                            mc_entity **out, mc_particle_error *error) {
    CHECK(mc_redstone_particle_allocate(f->app.entities, out, NULL) == MC_ENTITY_OK);
    const uint64_t shared_before = atomic_load(&f->app.profile->math->state);
    mc_entity_status status;
    if (strcmp(p->kind, "redstone_default") == 0 || strcmp(p->kind, "redstone_negative_zero") == 0)
        status = mc_redstone_particle_construct(
            *out, f->world, math, 8.5, 64.5, 8.5,
            strcmp(p->kind, "redstone_negative_zero") == 0 ? -0.0f : 0.0f, 0, 0, error);
    else {
        const bool zero = strcmp(p->kind, "redstone_zero") == 0;
        const bool negative = strcmp(p->kind, "redstone_negative") == 0;
        const float scale = strcmp(p->kind, "redstone_large") == 0 ? 2.5f
                            : zero                                 ? 0
                            : negative                             ? -.5f
                                                                   : 1;
        CHECK(strcmp(p->kind, "redstone_scaled") == 0 || strcmp(p->kind, "redstone_large") == 0 ||
              zero || negative);
        status = mc_redstone_particle_construct_scaled(*out, f->world, math, 8.5, 64.5, 8.5, scale,
                                                       zero       ? 0
                                                       : negative ? -.5f
                                                                  : 1,
                                                       zero       ? 1
                                                       : negative ? .25f
                                                                  : .5f,
                                                       zero       ? 0
                                                       : negative ? 1
                                                                  : .25f,
                                                       error);
    }
    CHECK(status == MC_ENTITY_OK && (*out)->phase == MC_ENTITY_READY && (*out)->holds == 1);
    const mc_particle_constructor_random *r = &error->constructor_random;
    CHECK(r->math_count == 10 && r->own_count == 4 && r->own_initial_stored &&
          r->own_identity == (uintptr_t)(*out)->random);
    mc_java_random shared = {.state = shared_before}, own = r->own_initial;
    for (size_t i = 0; i < 10; ++i)
        CHECK(redstone_dbits(mc_java_random_next_double(&shared)) ==
              redstone_dbits(r->math_values[i]));
    for (size_t i = 0; i < 4; ++i)
        CHECK(redstone_fbits(mc_java_random_next_float(&own)) == redstone_fbits(r->own_values[i]));
    CHECK(shared.state == atomic_load(&f->app.profile->math->state) &&
          own.state == (*out)->random->state);
    printf("REDSTONE_PARTICLE_NATIVE_CONSTRUCTOR {\"kind\":\"%s\",\"math_values\":[", p->kind);
    for (size_t i = 0; i < 10; ++i)
        printf("%s\"%016" PRIx64 "\"", i ? "," : "", redstone_dbits(r->math_values[i]));
    fputs("],\"own_values\":[", stdout);
    for (size_t i = 0; i < 4; ++i)
        printf("%s\"%08" PRIx32 "\"", i ? "," : "", redstone_fbits(r->own_values[i]));
    fputs("],\"state\":", stdout);
    const redstone_saved saved = redstone_save(*out);
    redstone_state(&saved);
    puts("}");
    return true;
}

static bool redstone_prepare(mc_entity *p, const redstone_profile *profile) {
    const bool tick = strcmp(profile->operation, "tick") == 0;
    CHECK(mc_entity_set_size(p, .2f, .2f) == MC_ENTITY_OK);
    p->base.offset = .1f;
    p->base.y_size = 0;
    CHECK(mc_entity_set_position(p, tick ? 8.5 : 3, tick ? 64.5 : 4, tick ? 8.5 : 5) ==
          MC_ENTITY_OK);
    p->base.aJ = tick ? -1 : 1;
    p->base.aK = tick ? -2 : 2;
    p->base.aL = tick ? -3 : 3;
    p->base.velocity_x = .125;
    p->base.velocity_y = redstone_double(profile->vy);
    p->base.velocity_z = .0625;
    if (tick)
        p->base.bq = true;
    p->base.aX = profile->ground != 0;
    p->base.be = false;
    p->particle->fields = (mc_particle_fields){
        profile->texture,           .25f,  .75f, profile->age, profile->lifetime,
        redstone_float(profile->g), .125f, .25f, .5f,          .75f};
    p->particle->redstone.a = redstone_float(profile->a);
    CHECK(mc_entity_particle_origin_store(p->classes, 0, .25, NULL) == MC_ENTITY_OK &&
          mc_entity_particle_origin_store(p->classes, 1, .5, NULL) == MC_ENTITY_OK &&
          mc_entity_particle_origin_store(p->classes, 2, .75, NULL) == MC_ENTITY_OK);
    return true;
}

static void redstone_prefix(size_t index, const redstone_profile *p, const char *exception,
                            const redstone_saved *before) {
    printf("REDSTONE_PARTICLE_COMPONENT "
           "{\"profile\":%zu,\"name\":\"%s\",\"kind\":\"%s\",\"operation\":\"%s\",\"exception\":\"%"
           "s\",\"before\":",
           index, p->name, p->kind, p->operation, exception);
    redstone_state(before);
    fputs(",\"after\":", stdout);
}

static bool redstone_constructor_control(fixture *f, mc_math_classes *math,
                                         const redstone_profile *profile) {
    mc_particle_error *error = calloc(1, sizeof(*error));
    mc_entity *particle = NULL;
    CHECK(error != NULL && redstone_create(f, math, profile, &particle, error));
    const bool wrapper = strcmp(profile->kind, "redstone_default") == 0 ||
                         strcmp(profile->kind, "redstone_negative_zero") == 0;
    CHECK(error->source_pc == (wrapper ? 17u : 202u));
    int32_t layer = -1;
    CHECK(mc_particle_layer(particle, &layer) == MC_ENTITY_OK && layer == 0);
    mc_entity_release(particle);
    free(error);
    return true;
}

static bool redstone_tick_case(fixture *f, mc_math_classes *math, size_t index,
                               const redstone_profile *profile) {
    mc_particle_error *constructor = calloc(1, sizeof(*constructor));
    mc_item_particle_tick_error *error = calloc(1, sizeof(*error));
    mc_entity *p = NULL;
    CHECK(constructor != NULL && error != NULL &&
          redstone_create(f, math, profile, &p, constructor) && redstone_prepare(p, profile));
    const redstone_saved before = redstone_save(p);
    const mc_entity_bounds *bounds = p->bounds;
    const uint64_t own = p->random->state, shared = atomic_load(&f->app.profile->math->state),
                   world = f->world->fields.r->state;
    if (profile->null_world)
        p->base.world = NULL;
    const mc_entity_status status = mc_redstone_particle_tick(p, f->app.registry, NULL, error);
    const bool failure = index == 11;
    CHECK(status == (failure ? MC_ENTITY_STATE_ERROR : MC_ENTITY_OK) && p->bounds == bounds &&
          p->holds == 1 && !p->particle->item_native.tick_active && p->random->state == own &&
          shared == atomic_load(&f->app.profile->math->state) &&
          world == f->world->fields.r->state);
    if (failure)
        CHECK(error->particle.source_exception && !error->particle.native_policy &&
              error->particle.source_pc == 60 && strcmp(error->particle.source_class, "im") == 0 &&
              strcmp(error->particle.exception_class, "java/lang/ArithmeticException") == 0);
    redstone_prefix(index, profile, failure ? "ArithmeticException" : "none", &before);
    const redstone_saved after = redstone_save(p);
    redstone_state(&after);
    puts(",\"brightness\":null,\"nw\":null,\"packed\":[]}");
    p->base.world = f->world;
    mc_entity_release(p);
    free(constructor);
    free(error);
    return true;
}

#include "client/particles/render_internal.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/nw.h"
#include "world/storage/generated_internal.h"

static bool redstone_lights(fixture *f, mc_chunk_state **chunks, int mode) {
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

static bool redstone_draw_case(fixture *f, mc_math_classes *math,
                               mc_client_effect_renderer *effects, mc_chunk_state **chunks,
                               size_t index, const redstone_profile *profile, uint64_t *untouched) {
    mc_particle_error *constructor = calloc(1, sizeof(*constructor));
    mc_effect_draw_error *error = calloc(1, sizeof(*error));
    mc_client_graphics_error *graphics_error = calloc(1, sizeof(*graphics_error));
    mc_entity *p = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_tessellator *nw = NULL;
    mc_client_effect_draw_call *call = NULL;
    CHECK(constructor != NULL && error != NULL && graphics_error != NULL &&
          redstone_create(f, math, profile, &p, constructor) && redstone_prepare(p, profile) &&
          redstone_lights(f, chunks, profile->light) &&
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
    CHECK(mc_client_effect_renderer_sprites_begin(effects, f->player,
                                                  redstone_float(profile->partial), &services,
                                                  &call, error) == MC_EFFECT_DRAW_OK &&
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
          redstone_dbits(origin_before.l) == redstone_dbits(.25) &&
          redstone_dbits(origin_before.m) == redstone_dbits(.5) &&
          redstone_dbits(origin_before.n) == redstone_dbits(.75));
    call->current = p;
    call->state.current = p;
    call->nw = strcmp(profile->operation, "draw_null_nw") == 0 ? NULL : nw;
    call->state.partial = redstone_float(profile->partial);
    call->state.cos_yaw = profile->orientation ? .75f : 1;
    call->state.cos_pitch = profile->orientation ? .5f : 1;
    call->state.sin_yaw = profile->orientation ? -.25f : 0;
    call->state.horizontal_y = profile->orientation ? .125f : 0;
    call->state.horizontal_x = profile->orientation ? -.375f : 0;
    if (profile->null_world)
        p->base.world = NULL;
    const redstone_saved before = redstone_save(p);
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
          redstone_dbits(origin_after.l) == redstone_dbits(origin_before.l) &&
          redstone_dbits(origin_after.m) == redstone_dbits(origin_before.m) &&
          redstone_dbits(origin_after.n) == redstone_dbits(origin_before.n));
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
              call->failure.sprite_caller.pc == 61 &&
              strcmp(call->failure.sprite_caller.class_name, "im") == 0 &&
              strcmp(call->failure.sprite_caller.method, "a") == 0 &&
              strcmp(call->failure.sprite_caller.descriptor, "(Lnw;FFFFFF)V") == 0);
        if (profile->null_world)
            CHECK(call->failure.entity_render != NULL &&
                  call->failure.entity_render->site.pc == 118 &&
                  strcmp(call->failure.entity_render->site.class_name, "sn") == 0);
    } else
        CHECK(call->state.item_vertices_returned == 4 && call->state.item_particle_returned &&
              call->state.brightness_returned && call->state.color_set &&
              call->state.particle_pc == 64u);
    redstone_prefix(index, profile, failed ? "NullPointerException" : "none", &before);
    const redstone_saved after = redstone_save(p);
    redstone_state(&after);
    printf(",\"brightness\":null,\"nw\":{\"vertices\":%" PRId32 ",\"source_vertices\":%" PRId32
           ",\"words\":%" PRId32 ",\"uv\":%s,\"color\":%s,\"normal\":%s,\"begun\":%s},\"packed\":[",
           after_nw.h, after_nw.p, after_nw.o, redstone_bool(after_nw.m), redstone_bool(after_nw.l),
           redstone_bool(after_nw.n), redstone_bool(after_nw.w));
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

static bool redstone_brightness_case(fixture *f, mc_math_classes *math, mc_chunk_state **chunks,
                                     size_t index, const redstone_profile *profile) {
    mc_particle_error *constructor = calloc(1, sizeof(*constructor));
    mc_entity_render_error *error = calloc(1, sizeof(*error));
    mc_entity *p = NULL;
    CHECK(constructor != NULL && error != NULL &&
          redstone_create(f, math, profile, &p, constructor) && redstone_prepare(p, profile) &&
          redstone_lights(f, chunks, profile->light));
    if (profile->null_world)
        p->base.world = NULL;
    const redstone_saved before = redstone_save(p);
    const uint64_t own = p->random->state, shared = atomic_load(&f->app.profile->math->state),
                   world = f->world->fields.r->state;
    float result = 123.25f;
    const bool returned =
        mc_entity_render_brightness(p, math, redstone_float(profile->partial), &result, error);
    CHECK(returned == !profile->null_world && p->random->state == own &&
          shared == atomic_load(&f->app.profile->math->state) &&
          world == f->world->fields.r->state);
    if (profile->null_world)
        CHECK(redstone_fbits(result) == redstone_fbits(123.25f) && error->source_exception &&
              !error->native_policy && error->site.pc == 118 &&
              strcmp(error->site.class_name, "sn") == 0 &&
              strcmp(error->exception_class, "java/lang/NullPointerException") == 0);
    redstone_prefix(index, profile, returned ? "none" : "NullPointerException", &before);
    const redstone_saved after = redstone_save(p);
    redstone_state(&after);
    if (returned)
        printf(",\"brightness\":\"%08" PRIx32 "\"", redstone_fbits(result));
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

static bool redstone_fp_controls(fixture *f, mc_math_classes *math,
                                 mc_client_effect_renderer *effects) {
    for (size_t kind = 0; kind < 1; ++kind) {
        redstone_profile profile = {.age = 0,
                                    .lifetime = 8,
                                    .texture = 7,
                                    .g = UINT32_C(0x40600000),
                                    .a = UINT32_C(0x3fa00000),
                                    .partial = UINT32_C(0x3f000000),
                                    .vy = UINT64_C(0xbfd0000000000000)};
        strcpy(profile.kind, "redstone_default");
        strcpy(profile.operation, "draw");
        mc_particle_error *constructor = calloc(1, sizeof(*constructor));
        mc_effect_draw_error *error = calloc(1, sizeof(*error));
        mc_entity *p = NULL;
        mc_client_effect_draw_call *call = NULL;
        CHECK(constructor != NULL && error != NULL &&
              redstone_create(f, math, &profile, &p, constructor) && redstone_prepare(p, &profile));
        const mc_effect_draw_services services = {.math = math};
        CHECK(mc_client_effect_renderer_sprites_begin(effects, f->player, .5f, &services, &call,
                                                      error) == MC_EFFECT_DRAW_OK &&
              mc_entity_retain(p) == MC_ENTITY_OK);
        call->current = p;
        call->state.current = p;
        call->state.partial = .5f;
        const uint32_t before = redstone_fbits(p->particle->fields.g);
        const unsigned controls = _mm_getcsr();
        _mm_setcsr(controls | UINT32_C(0x8000));
        const bool returned = mc_effect_draw_animated_sprite(call, 291);
        _mm_setcsr(controls);
        CHECK(!returned && call->failure.native_policy && !call->failure.source_exception &&
              call->failure.status == MC_EFFECT_DRAW_UNSUPPORTED_FP &&
              call->failure.child.pc == 4 && redstone_fbits(p->particle->fields.g) == before &&
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
    if (f == NULL || profiles == NULL || !setup(f, argv[1], "redstone-draw", true)) {
        fprintf(stderr, "[REDSTONE_DRAW_SETUP] initial owner/profile/setup failed\n");
        return 1;
    }
    const mc_math_class_status math_status = mc_math_classes_create(SIZE_MAX, &math, NULL);
    if (math_status != MC_MATH_CLASS_OK) {
        fprintf(stderr, "[REDSTONE_DRAW_SETUP] Math owner status=%d\n", (int)math_status);
        return 1;
    }
    mc_chunk_state *chunks[4] = {0};
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
            mc_world_chunk *chunk = NULL;
            if (blocks == NULL) {
                fprintf(stderr, "[REDSTONE_DRAW_SETUP] block allocation x=%d z=%d\n", x, z);
                return 1;
            }
            const mc_world_storage_status status = mc_world_store_create_generated(
                f->store, f->world, 0, x, z, &blocks, MC_CHUNK_BLOCKS, &chunk, NULL);
            if (status != MC_WORLD_STORAGE_OK || blocks != NULL) {
                fprintf(stderr,
                        "[REDSTONE_DRAW_SETUP] generated Chunk x=%d z=%d status=%d "
                        "blocks-retained=%d\n",
                        x, z, (int)status, blocks != NULL);
                return 1;
            }
            chunks[x * 2 + z] = mc_world_chunk_state(chunk);
            if (acquisition == NULL || chunks[x * 2 + z] == NULL ||
                !mc_chunk_cache_publish(acquisition->cache, x, z, chunks[x * 2 + z])) {
                fprintf(stderr,
                        "[REDSTONE_DRAW_SETUP] cache publication x=%d z=%d acquisition=%d Chunk=%d "
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
        fprintf(stderr, "[REDSTONE_DRAW_SETUP] effect renderer status=%d\n", (int)effects_status);
        return 1;
    }
    redstone_profile warm = {0};
    strcpy(warm.kind, "redstone_default");
    if (!redstone_constructor_control(f, math, &warm))
        return 1;
    size_t index = 0, constructors = 0, ticks = 0, draws = 0, brightness = 0;
    uint64_t untouched = 0;
    bool eof = false;
    redstone_profile profile;
    while (redstone_read(profiles, &profile, &eof) && !eof) {
        if (strcmp(profile.operation, "constructor") == 0) {
            if (!redstone_constructor_control(f, math, &profile))
                return 1;
            ++constructors;
        } else if (strcmp(profile.operation, "tick") == 0) {
            if (!redstone_tick_case(f, math, index, &profile))
                return 1;
            ++ticks;
        } else if (strncmp(profile.operation, "draw", 4) == 0) {
            if (!redstone_draw_case(f, math, effects, chunks, index, &profile, &untouched))
                return 1;
            ++draws;
        } else if (strcmp(profile.operation, "brightness") == 0) {
            if (!redstone_brightness_case(f, math, chunks, index, &profile))
                return 1;
            ++brightness;
        }
        ++index;
    }
    if (!eof || fclose(profiles) != 0 || index != 36 || constructors != 6 || ticks != 12 ||
        draws != 12 || brightness != 6 || untouched != UINT64_C(25165440) ||
        !redstone_fp_controls(f, math, effects) ||
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
    printf("REDSTONE_PARTICLE_NATIVE constructors=38 ticks=12 draws=12 brightness=6 "
           "own-unchanged-words=%" PRIu64 " fp-admission=1 cleanup=1 checks=%zu\n",
           untouched, checks);
    return 0;
}
