#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
/* Genuine App/World owner fixture; its earlier entry point is not run. */
#define main mc_torch_unused_statistics_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "client/particles/effect_renderer_internal.h"
#include "core/java/random_profile_internal.h"
#include "entity/particles/particle_internal.h"
#include "world/runtime/runtime_internal.h"
#include <inttypes.h>
#include <stdatomic.h>
#include <xmmintrin.h>

typedef struct torch_profile {
    char name[96], kind[32], operation[32];
    int32_t age, lifetime, texture;
    uint32_t g, a, partial;
    int ground, light, null_world;
    uint64_t vy;
    int orientation;
} torch_profile;

typedef struct torch_saved {
    mc_entity_base_fields base;
    mc_particle_fields fields;
    mc_entity_bounds bounds;
    float original_scale;
} torch_saved;

static uint64_t torch_dbits(double value) {
    uint64_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static uint32_t torch_fbits(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static float torch_float(uint32_t value) {
    float out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static double torch_double(uint64_t value) {
    double out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static const char *torch_bool(bool value) {
    return value ? "true" : "false";
}

static bool torch_flame(const torch_profile *p) {
    return strcmp(p->kind, "flame") == 0;
}

static bool torch_read(FILE *file, torch_profile *p, bool *eof) {
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

static torch_saved torch_save(const mc_entity *p) {
    return (torch_saved){p->base, p->particle->fields, *p->bounds,
                         mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_FLAME_PARTICLE)
                             ? p->particle->flame.a
                             : p->particle->smoke.a};
}

static void torch_dwords(const double *v, size_t count) {
    putchar('[');
    for (size_t i = 0; i < count; ++i)
        printf("%s\"%016" PRIx64 "\"", i ? "," : "", torch_dbits(v[i]));
    putchar(']');
}

static void torch_state(const torch_saved *s) {
    const mc_entity_base_fields *b = &s->base;
    const mc_particle_fields *p = &s->fields;
    const double position[] = {b->x, b->y, b->z}, previous[] = {b->aJ, b->aK, b->aL},
                 velocity[] = {b->velocity_x, b->velocity_y, b->velocity_z};
    const double bounds[] = {s->bounds.min_x, s->bounds.min_y, s->bounds.min_z,
                             s->bounds.max_x, s->bounds.max_y, s->bounds.max_z};
    fputs("{\"position\":", stdout);
    torch_dwords(position, 3);
    fputs(",\"previous\":", stdout);
    torch_dwords(previous, 3);
    fputs(",\"velocity\":", stdout);
    torch_dwords(velocity, 3);
    fputs(",\"bounds\":", stdout);
    torch_dwords(bounds, 6);
    printf(",\"size\":[\"%08" PRIx32 "\",\"%08" PRIx32 "\",\"%08" PRIx32
           "\"],\"y_size\":\"%08" PRIx32 "\",\"texture\":%" PRId32 ",\"c\":\"%08" PRIx32
           "\",\"d\":\"%08" PRIx32 "\",\"age\":%" PRId32 ",\"lifetime\":%" PRId32
           ",\"scale\":\"%08" PRIx32 "\",\"gravity\":\"%08" PRIx32 "\",\"rgb\":[\"%08" PRIx32
           "\",\"%08" PRIx32 "\",\"%08" PRIx32 "\"],\"base_scale\":\"%08" PRIx32
           "\",\"dead\":%s,\"ground\":%s,\"no_clip\":%s,\"collision\":[%s,%s,%s,%s,%s,%s]}",
           torch_fbits(b->offset), torch_fbits(b->width), torch_fbits(b->height),
           torch_fbits(b->y_size), p->b, torch_fbits(p->c), torch_fbits(p->d), p->e, p->f,
           torch_fbits(p->g), torch_fbits(p->h), torch_fbits(p->i), torch_fbits(p->j),
           torch_fbits(p->k), torch_fbits(s->original_scale), torch_bool(b->be), torch_bool(b->aX),
           torch_bool(b->bq), torch_bool(b->aY), torch_bool(b->aZ), torch_bool(b->ba),
           torch_bool(b->bb), torch_bool(b->bc), torch_bool(b->bd));
}

static bool torch_create(fixture *f, mc_math_classes *math, const torch_profile *p, mc_entity **out,
                         mc_particle_error *error) {
    CHECK((torch_flame(p)
               ? mc_flame_particle_allocate(f->app.entities, out, NULL)
               : mc_smoke_particle_allocate(f->app.entities, out, NULL)) == MC_ENTITY_OK);
    mc_entity_status status;
    if (torch_flame(p))
        status = mc_flame_particle_construct(*out, f->world, math, 8.5, 64.5, 8.5, .125, -.25,
                                             .0625, error);
    else if (strcmp(p->kind, "smoke_default") == 0)
        status = mc_smoke_particle_construct(*out, f->world, math, 8.5, 64.5, 8.5, .125, -.25,
                                             .0625, error);
    else {
        const float scale = strcmp(p->kind, "large_smoke") == 0      ? 2.5f
                            : strcmp(p->kind, "smoke_zero") == 0     ? 0
                            : strcmp(p->kind, "smoke_negative") == 0 ? -.5f
                                                                     : 1;
        CHECK(strcmp(p->kind, "large_smoke") == 0 || strcmp(p->kind, "smoke_zero") == 0 ||
              strcmp(p->kind, "smoke_negative") == 0 || strcmp(p->kind, "smoke_scaled") == 0);
        status = mc_smoke_particle_construct_scaled(*out, f->world, math, 8.5, 64.5, 8.5, .125,
                                                    -.25, .0625, scale, error);
    }
    CHECK(status == MC_ENTITY_OK && (*out)->phase == MC_ENTITY_READY && (*out)->holds == 1);
    return true;
}

static bool torch_prepare(mc_entity *p, const torch_profile *profile) {
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
    p->base.velocity_y = torch_double(profile->vy);
    p->base.velocity_z = .0625;
    if (tick)
        p->base.bq = true;
    p->base.aX = profile->ground != 0;
    p->base.be = false;
    p->particle->fields =
        (mc_particle_fields){profile->texture,        .25f,  .75f, profile->age, profile->lifetime,
                             torch_float(profile->g), .125f, .25f, .5f,          .75f};
    if (torch_flame(profile))
        p->particle->flame.a = torch_float(profile->a);
    else
        p->particle->smoke.a = torch_float(profile->a);
    CHECK(mc_entity_particle_origin_store(p->classes, 0, .25, NULL) == MC_ENTITY_OK &&
          mc_entity_particle_origin_store(p->classes, 1, .5, NULL) == MC_ENTITY_OK &&
          mc_entity_particle_origin_store(p->classes, 2, .75, NULL) == MC_ENTITY_OK);
    return true;
}

static void torch_prefix(size_t index, const torch_profile *p, const char *exception,
                         const torch_saved *before) {
    printf("TORCH_PARTICLE_COMPONENT "
           "{\"profile\":%zu,\"name\":\"%s\",\"kind\":\"%s\",\"operation\":\"%s\",\"exception\":\"%"
           "s\",\"before\":",
           index, p->name, p->kind, p->operation, exception);
    torch_state(before);
    fputs(",\"after\":", stdout);
}

static bool torch_constructor_control(fixture *f, mc_math_classes *math,
                                      const torch_profile *profile) {
    mc_particle_error *error = calloc(1, sizeof(*error));
    mc_entity *particle = NULL;
    CHECK(error != NULL);
    const uint64_t shared_before = atomic_load(&f->app.profile->math->state);
    CHECK(torch_create(f, math, profile, &particle, error));
    const mc_particle_constructor_random *r = &error->constructor_random;
    const size_t math_count = torch_flame(profile) ? 6u : 7u,
                 own_count = torch_flame(profile) ? 10u : 4u;
    CHECK(r->math_count == math_count && r->own_count == own_count && r->own_initial_stored &&
          r->own_identity == (uintptr_t)particle->random);
    mc_java_random shared = {.state = shared_before}, own = r->own_initial;
    for (size_t i = 0; i < math_count; ++i)
        CHECK(torch_dbits(mc_java_random_next_double(&shared)) == torch_dbits(r->math_values[i]));
    for (size_t i = 0; i < own_count; ++i)
        CHECK(torch_fbits(mc_java_random_next_float(&own)) == torch_fbits(r->own_values[i]));
    CHECK(shared.state == atomic_load(&f->app.profile->math->state) &&
          own.state == particle->random->state);
    CHECK(error->source_pc == (torch_flame(profile)                          ? 187u
                               : strcmp(profile->kind, "smoke_default") == 0 ? 17u
                                                                             : 172u));
    int32_t layer = -1;
    CHECK(mc_particle_layer(particle, &layer) == MC_ENTITY_OK && layer == 0);
    printf("TORCH_PARTICLE_NATIVE_CONSTRUCTOR {\"kind\":\"%s\",\"math_values\":[", profile->kind);
    for (size_t i = 0; i < math_count; ++i)
        printf("%s\"%016" PRIx64 "\"", i ? "," : "", torch_dbits(r->math_values[i]));
    fputs("],\"own_values\":[", stdout);
    for (size_t i = 0; i < own_count; ++i)
        printf("%s\"%08" PRIx32 "\"", i ? "," : "", torch_fbits(r->own_values[i]));
    fputs("],\"state\":", stdout);
    const torch_saved saved = torch_save(particle);
    torch_state(&saved);
    puts("}");
    mc_entity_release(particle);
    free(error);
    return true;
}

static bool torch_tick_case(fixture *f, mc_math_classes *math, size_t index,
                            const torch_profile *profile) {
    mc_particle_error *constructor = calloc(1, sizeof(*constructor));
    mc_item_particle_tick_error *error = calloc(1, sizeof(*error));
    mc_entity *p = NULL;
    CHECK(constructor != NULL && error != NULL && torch_create(f, math, profile, &p, constructor) &&
          torch_prepare(p, profile));
    const torch_saved before = torch_save(p);
    const mc_entity_bounds *bounds = p->bounds;
    const uint64_t own = p->random->state, shared = atomic_load(&f->app.profile->math->state),
                   world = f->world->fields.r->state;
    const mc_entity_status status = torch_flame(profile)
                                        ? mc_flame_particle_tick(p, f->app.registry, NULL, error)
                                        : mc_smoke_particle_tick(p, f->app.registry, NULL, error);
    const bool failure = index == 11;
    CHECK(status == (failure ? MC_ENTITY_STATE_ERROR : MC_ENTITY_OK) && p->bounds == bounds &&
          p->holds == 1 && !p->particle->item_native.tick_active && p->random->state == own &&
          shared == atomic_load(&f->app.profile->math->state) &&
          world == f->world->fields.r->state);
    if (failure)
        CHECK(error->particle.source_exception && !error->particle.native_policy &&
              error->particle.source_pc == 60 && strcmp(error->particle.source_class, "xn") == 0 &&
              strcmp(error->particle.exception_class, "java/lang/ArithmeticException") == 0);
    torch_prefix(index, profile, failure ? "ArithmeticException" : "none", &before);
    const torch_saved after = torch_save(p);
    torch_state(&after);
    puts(",\"brightness\":null,\"nw\":null,\"packed\":[]}");
    mc_entity_release(p);
    free(constructor);
    free(error);
    return true;
}

static bool torch_owner_controls(fixture *f, mc_math_classes *math) {
    mc_particle_error *error = calloc(1, sizeof(*error));
    mc_effect_renderer_error *effects_error = calloc(1, sizeof(*effects_error));
    mc_client_effect_renderer *effects = NULL;
    CHECK(error != NULL && effects_error != NULL &&
          mc_client_effect_renderer_allocate(f->app.entities, NULL, &effects, effects_error) ==
              MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_construct(effects, f->world, NULL, effects_error) ==
              MC_EFFECT_RENDERER_OK);
    for (size_t kind = 0; kind < 2; ++kind) {
        torch_profile profile = {0};
        strcpy(profile.kind, kind ? "flame" : "smoke_default");
        mc_entity *p = NULL;
        CHECK(torch_create(f, math, &profile, &p, error));
        p->base.bq = true;
        p->particle->fields.f = 2;
        CHECK(mc_client_effect_renderer_add(effects, p, effects_error) == MC_EFFECT_RENDERER_OK &&
              p->holds == 2);
        mc_entity_release(p);
        mc_math_classes *limited = NULL, *captured = NULL;
        CHECK(mc_math_classes_create(4096, &limited, NULL) == MC_MATH_CLASS_OK &&
              (kind ? mc_flame_particle_allocate(f->app.entities, &p, NULL)
                    : mc_smoke_particle_allocate(f->app.entities, &p, NULL)) == MC_ENTITY_OK);
        const mc_entity_status status =
            kind ? mc_flame_particle_construct_entered(p, f->world, limited, 1, 2, 3, 4, 5, 6,
                                                       &captured, error)
                 : mc_smoke_particle_construct_entered(p, f->world, limited, 1, 2, 3, 4, 5, 6,
                                                       &captured, error);
        CHECK(status == MC_ENTITY_LIMIT && p->phase == MC_ENTITY_FAILED_CONSTRUCTION &&
              captured == limited && error->constructor_random.math_count == 5 &&
              error->constructor_random.own_count == 0 && error->source_pc == 164 &&
              strcmp(error->source_class, "xw") == 0 &&
              mc_math_classes_release(&captured, NULL) == MC_MATH_CLASS_OK &&
              mc_math_classes_release(&limited, NULL) == MC_MATH_CLASS_OK);
        mc_entity_release(p);
        p = NULL;
        CHECK((kind ? mc_flame_particle_allocate(f->app.entities, &p, NULL)
                    : mc_smoke_particle_allocate(f->app.entities, &p, NULL)) == MC_ENTITY_OK &&
              p->phase == MC_ENTITY_RAW);
        mc_entity_release(p);
    }
    for (size_t tick = 0; tick < 3; ++tick)
        CHECK(mc_client_effect_renderer_tick_in_context(effects, f->app.registry, NULL,
                                                        effects_error) == MC_EFFECT_RENDERER_OK &&
              effects->layers[0]->size == (tick == 2 ? 0u : 2u));
    CHECK(mc_client_effect_renderer_free(&effects) == MC_EFFECT_RENDERER_OK && effects == NULL);
    free(error);
    free(effects_error);
    return true;
}

#ifndef MC_TORCH_PARTICLE_MAIN
#define MC_TORCH_PARTICLE_MAIN main
#endif
int MC_TORCH_PARTICLE_MAIN(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    mc_math_classes *math = NULL;
    FILE *profiles = fopen(argv[3], "rb");
    if (f == NULL || profiles == NULL || !setup(f, argv[1], "torch-particle", true) ||
        mc_math_classes_create(SIZE_MAX, &math, NULL) != MC_MATH_CLASS_OK)
        return 1;
    size_t index = 0, ticks = 0, constructors = 0;
    bool eof = false;
    torch_profile profile;
    while (torch_read(profiles, &profile, &eof) && !eof) {
        if (strcmp(profile.operation, "constructor") == 0) {
            if (!torch_constructor_control(f, math, &profile))
                return 1;
            ++constructors;
        } else if (strcmp(profile.operation, "tick") == 0) {
            if (!torch_tick_case(f, math, index, &profile))
                return 1;
            ++ticks;
        }
        ++index;
    }
    if (!eof || fclose(profiles) != 0 || index != 36 || constructors != 6 || ticks != 12 ||
        !torch_owner_controls(f, math) ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK || !teardown(f))
        return 1;
    free(f);
    printf("TORCH_PARTICLE_NATIVE ticks=12 constructors=6 failed-constructor=2 raw-retirement=2 "
           "lifecycle=2 cleanup=1 checks=%zu\n",
           checks);
    return 0;
}
