#include "mc/client/particles/render.h"

#include "client/particles/effect_renderer_internal.h"
#include "client/render_engine/nw.h"
#include "core/java/random_profile_internal.h"
#include "entity/entity_internal.h"
#include "mc/entity/particles/particle.h"

#include <emmintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Native CPU method/ownership controls. Every owner and particle is genuinely
 * allocated/constructed. These do not compare peer constructor entropy or
 * claim original particle/renderer draw completion. Only camera pose scalars
 * below are declared source inputs; no native phase, hold, list or pointer is
 * assigned to manufacture a branch. */
static unsigned int checks;
static const uint32_t partial_bits[] = {
    UINT32_C(0x00000000), UINT32_C(0x80000000), UINT32_C(0x3e800000), UINT32_C(0x3f000000),
    UINT32_C(0x3f800000), UINT32_C(0xbf800000), UINT32_C(0x7f800000), UINT32_C(0xff800000),
    UINT32_C(0x7fc12345), UINT32_C(0x7f812345)};

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *classes;
    mc_client_effect_renderer *renderer, *other;
    mc_client_render_engine_classes *graphics;
    mc_math_classes *math;
    mc_entity *picked, *target, *particle, *camera;
    mc_client_effect_draw_call *call, *other_call;
    mc_effect_draw_error error, cleanup;
    mc_effect_draw_state state;
    mc_client_nw_state nw_before, nw_after;
} fixture;

typedef struct source_snapshot {
    mc_client_effect_renderer owner;
    mc_effect_particle_list *list_identities[4];
    mc_effect_particle_list lists[4];
    mc_entity *elements[4][10];
    mc_java_random random;
    uint64_t uniquifier, math_state;
    const mc_java_math_random_owner *math;
    size_t profile_bytes;
} source_snapshot;

static float float_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool opened(fixture *f) {
    return mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK &&
           mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->classes) ==
               MC_ENTITY_OK &&
           mc_client_effect_renderer_allocate(f->classes, NULL, &f->renderer, NULL) ==
               MC_EFFECT_RENDERER_OK &&
           mc_client_effect_renderer_construct(f->renderer, NULL, NULL, NULL) ==
               MC_EFFECT_RENDERER_OK &&
           mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &f->graphics) ==
               MC_CLIENT_RENDER_ENGINE_OK &&
           mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK;
}

static bool closed(fixture *f) {
    bool ok = true;
    if (mc_client_effect_renderer_draw_retire(&f->other_call, &f->cleanup) != MC_EFFECT_DRAW_OK)
        ok = false;
    if (mc_client_effect_renderer_draw_retire(&f->call, &f->cleanup) != MC_EFFECT_DRAW_OK)
        ok = false;
    if (mc_client_effect_renderer_free(&f->other) != MC_EFFECT_RENDERER_OK)
        ok = false;
    if (mc_client_effect_renderer_free(&f->renderer) != MC_EFFECT_RENDERER_OK)
        ok = false;
    if (!ok)
        return false;
    mc_entity_release(f->particle);
    mc_entity_release(f->camera);
    mc_entity_release(f->target);
    mc_entity_release(f->picked);
    if (f->graphics != NULL &&
        mc_client_render_engine_classes_release(f->graphics) != MC_CLIENT_RENDER_ENGINE_OK)
        ok = false;
    if (mc_math_classes_release(&f->math, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    if (f->classes != NULL && mc_entity_classes_free(f->classes) != MC_ENTITY_OK)
        ok = false;
    if (ok)
        mc_java_random_profile_free(f->profile);
    return ok;
}

static bool snapshot(const fixture *f, source_snapshot *s) {
    memcpy(&s->owner, f->renderer, sizeof(s->owner));
    memcpy(&s->random, f->renderer->random, sizeof(s->random));
    for (size_t i = 0; i < 4; ++i) {
        const mc_effect_particle_list *list = f->renderer->layers[i];
        if (list->capacity > 10)
            return false;
        s->list_identities[i] = f->renderer->layers[i];
        memcpy(&s->lists[i], list, sizeof(s->lists[i]));
        if (list->capacity != 0)
            memcpy(s->elements[i], list->elements, list->capacity * sizeof(*list->elements));
    }
    s->uniquifier = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    s->math = f->profile->math;
    s->math_state =
        s->math == NULL ? 0 : atomic_load_explicit(&s->math->state, memory_order_relaxed);
    s->profile_bytes = mc_java_random_profile_allocation_bytes(f->profile);
    return true;
}

static bool same_source(const fixture *f, const source_snapshot *s) {
    const mc_client_effect_renderer *r = f->renderer;
    if (r->classes != s->owner.classes || r->phase != s->owner.phase ||
        r->constructor_pc != s->owner.constructor_pc || r->world != s->owner.world ||
        r->render_engine != s->owner.render_engine || r->layers != s->owner.layers ||
        r->random != s->owner.random || r->random_candidate != s->owner.random_candidate ||
        r->allocation_bytes != s->owner.allocation_bytes ||
        r->max_allocation_bytes != s->owner.max_allocation_bytes ||
        memcmp(r->random, &s->random, sizeof(s->random)) != 0 ||
        atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) != s->uniquifier ||
        f->profile->math != s->math ||
        mc_java_random_profile_allocation_bytes(f->profile) != s->profile_bytes ||
        (s->math != NULL &&
         atomic_load_explicit(&s->math->state, memory_order_relaxed) != s->math_state))
        return false;
    for (size_t i = 0; i < 4; ++i) {
        const mc_effect_particle_list *list = r->layers[i];
        if (list != s->list_identities[i] || memcmp(list, &s->lists[i], sizeof(*list)) != 0 ||
            (list->capacity != 0 &&
             memcmp(list->elements, s->elements[i], list->capacity * sizeof(*list->elements)) != 0))
            return false;
    }
    return true;
}

static bool blocked(fixture *f, const source_snapshot *s) {
    bool ok = false;
    mc_effect_renderer_error owner_error;
    mc_effect_renderer_terminal_view view, sentinel;
    memset(&view, 0xa5, sizeof(view));
    memcpy(&sentinel, &view, sizeof(view));
    const mc_effect_draw_status original = f->state.failure->status;
    const int32_t original_pc = f->state.failure->site.pc;
    const mc_client_effect_renderer *identity = f->renderer;
    const size_t particle_holds = f->particle == NULL ? 0 : f->particle->holds;
    CHECK(mc_client_effect_renderer_add(f->renderer, f->particle, &owner_error) ==
          MC_EFFECT_RENDERER_BUSY);
    CHECK(mc_client_effect_renderer_tick(f->renderer, &owner_error) == MC_EFFECT_RENDERER_BUSY);
    CHECK(mc_client_effect_renderer_change_world(f->renderer, NULL, &owner_error) ==
          MC_EFFECT_RENDERER_BUSY);
    CHECK(mc_client_effect_renderer_retire_world(f->renderer, NULL, &owner_error) ==
          MC_EFFECT_RENDERER_BUSY);
    CHECK(mc_client_effect_renderer_terminal_inspect(f->renderer, &view, &owner_error) ==
          MC_EFFECT_RENDERER_BUSY);
    CHECK(memcmp(&view, &sentinel, sizeof(view)) == 0);
    CHECK(mc_client_effect_renderer_free(&f->renderer) == MC_EFFECT_RENDERER_BUSY &&
          f->renderer == identity);
    CHECK(mc_client_effect_renderer_lit_begin(f->renderer, NULL, 0.0f, NULL, &f->other_call,
                                              &f->error) == MC_EFFECT_DRAW_BUSY &&
          f->other_call == NULL);
    CHECK(mc_client_effect_renderer_sprites_begin(f->renderer, NULL, 0.0f, NULL, &f->other_call,
                                                  &f->error) == MC_EFFECT_DRAW_BUSY &&
          f->other_call == NULL);
    CHECK(mc_client_effect_renderer_draw_inspect(f->call, &f->state) &&
          f->state.failure->status == original && f->state.failure->site.pc == original_pc &&
          same_source(f, s));
    CHECK(f->particle == NULL || f->particle->holds == particle_holds);
    ok = true;
cleanup:
    return ok;
}

static bool empty_lit(void) {
    bool ok = false;
    fixture *f = calloc(1, sizeof(*f));
    source_snapshot s;
    const unsigned int controls = _mm_getcsr();
    CHECK(f != NULL && opened(f));
    CHECK(snapshot(f, &s));
    CHECK(mc_client_nw_inspect(f->graphics, &f->nw_before) &&
          f->nw_before.phase == MC_CLIENT_RENDER_ENGINE_RAW && f->nw_before.a == NULL);
    mc_math_class_state math_before, math_after;
    CHECK(mc_math_classes_state(f->math, &math_before, NULL) == MC_MATH_CLASS_OK &&
          math_before.phase == MC_MATH_CLASS_COLD);
    for (size_t i = 0; i < sizeof(partial_bits) / sizeof(*partial_bits); ++i) {
        _mm_setcsr(controls | 0x8040u);
        const mc_effect_draw_status status = mc_client_effect_renderer_lit_begin(
            f->renderer, NULL, float_from_bits(partial_bits[i]), NULL, &f->call, &f->error);
        const unsigned int after = _mm_getcsr();
        _mm_setcsr(controls);
        CHECK(status == MC_EFFECT_DRAW_OK && after == (controls | 0x8040u));
        CHECK(mc_client_effect_renderer_draw_inspect(f->call, &f->state));
        CHECK(f->state.returned && f->state.lit && f->state.empty && f->state.source_pc == 16 &&
              f->state.layer == 3 && f->state.size_reads == 1 && f->state.visited == 0 &&
              !f->state.nw_captured && f->state.orientation_values == 0 &&
              f->state.origins_stored == 0 && f->state.camera == NULL &&
              float_bits(f->state.partial) == partial_bits[i]);
        CHECK(f->renderer->draw_call == f->call && f->renderer->holds == s.owner.holds + 1 &&
              !f->renderer->entered && same_source(f, &s));
        if (i == 0)
            CHECK(blocked(f, &s));
        CHECK(mc_client_effect_renderer_draw_retire(&f->call, &f->cleanup) == MC_EFFECT_DRAW_OK &&
              f->call == NULL && f->renderer->draw_call == NULL &&
              f->renderer->holds == s.owner.holds && same_source(f, &s));
    }
    CHECK(mc_client_nw_inspect(f->graphics, &f->nw_after) &&
          f->nw_after.phase == f->nw_before.phase && f->nw_after.a == NULL);
    CHECK(mc_math_classes_state(f->math, &math_after, NULL) == MC_MATH_CLASS_OK &&
          math_after.phase == math_before.phase && math_after.holds == math_before.holds &&
          math_after.source_a == NULL && f->classes->entity_phase == MC_ENTITY_CLASS_COLD);
    CHECK(mc_client_effect_renderer_change_world(f->renderer, NULL, NULL) ==
              MC_EFFECT_RENDERER_OK &&
          same_source(f, &s));
    CHECK(mc_client_effect_renderer_draw_retire(&f->call, NULL) == MC_EFFECT_DRAW_OK);
    CHECK(mc_client_effect_renderer_draw_retire(NULL, &f->error) == MC_EFFECT_DRAW_ARGUMENT);
    ok = true;
cleanup:
    _mm_setcsr(controls);
    if (f != NULL) {
        if (!closed(f))
            ok = false;
        free(f);
    }
    return ok;
}

static bool item(mc_entity_classes *classes, mc_entity **out, double x, double y, double z) {
    return mc_item_entity_allocate(classes, out, NULL) == MC_ENTITY_OK &&
           mc_item_entity_construct(*out, NULL, x, y, z, NULL, NULL) == MC_ENTITY_OK;
}

static bool pickup(fixture *f, bool target) {
    return item(f->classes, &f->picked, 2.0, 3.0, 4.0) &&
           (!target || item(f->classes, &f->target, 8.0, 9.0, 10.0)) &&
           mc_pickup_particle_allocate(f->classes, &f->particle, NULL) == MC_ENTITY_OK &&
           mc_pickup_particle_construct(f->particle, NULL, f->picked, f->target, 0.25f, NULL) ==
               MC_ENTITY_OK &&
           mc_client_effect_renderer_add(f->renderer, f->particle, NULL) == MC_EFFECT_RENDERER_OK;
}

static bool nonempty_lit(bool target) {
    bool ok = false;
    fixture *f = calloc(1, sizeof(*f));
    source_snapshot s;
    CHECK(f != NULL && opened(f) && pickup(f, target));
    CHECK(snapshot(f, &s));
    const size_t particle_holds = f->particle->holds, picked_holds = f->picked->holds;
    const size_t target_holds = f->target == NULL ? 0 : f->target->holds;
    mc_effect_draw_services services = {.graphics = f->graphics, .math = f->math};
    mc_client_render_engine_class_state graphics_before, graphics_retained, graphics_after;
    mc_math_class_state math_before;
    CHECK(mc_client_render_engine_classes_inspect(f->graphics, &graphics_before));
    CHECK(mc_math_classes_state(f->math, &math_before, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_client_nw_inspect(f->graphics, &f->nw_before) &&
          f->nw_before.phase == MC_CLIENT_RENDER_ENGINE_RAW);
    for (size_t i = 0; i < sizeof(partial_bits) / sizeof(*partial_bits); ++i) {
        CHECK(mc_client_effect_renderer_lit_begin(
                  f->renderer, NULL, float_from_bits(partial_bits[i]), &services, &f->call,
                  &f->error) == MC_EFFECT_DRAW_SOURCE_NULL);
        CHECK(mc_client_effect_renderer_draw_inspect(f->call, &f->state));
        CHECK(!f->state.returned && f->state.lit && f->state.nw_captured &&
              f->state.source_pc == 69 && f->state.particle_pc == (target ? 220u : 53u) &&
              f->state.visited == 1 && f->state.size_reads == 2 &&
              f->state.current == f->particle && !f->state.brightness_returned &&
              !f->state.color_set && float_bits(f->state.partial) == partial_bits[i]);
        CHECK(f->state.failure->status == MC_EFFECT_DRAW_SOURCE_NULL &&
              !f->state.failure->native_policy && f->state.failure->site.pc == 69 &&
              strcmp(f->state.failure->site.class_name, "dn") == 0 &&
              strcmp(f->state.failure->child.class_name, "em") == 0 &&
              f->state.failure->child.pc == (target ? 220 : 53));
        CHECK(mc_client_nw_inspect(f->graphics, &f->nw_after) &&
              f->nw_after.phase == MC_CLIENT_RENDER_ENGINE_READY && f->nw_after.a == f->state.nw &&
              !f->nw_after.fields.w && f->nw_after.fields.h == 0);
        mc_math_class_state math;
        CHECK(mc_math_classes_state(f->math, &math, NULL) == MC_MATH_CLASS_OK &&
              math.phase == (target ? MC_MATH_CLASS_READY : MC_MATH_CLASS_COLD) &&
              math.holds == math_before.holds + (target ? 1u : 0u));
        CHECK(mc_client_render_engine_classes_inspect(f->graphics, &graphics_retained) &&
              graphics_retained.native_holds == graphics_before.native_holds + 1);
        CHECK(f->particle->holds == particle_holds + 1 && f->picked->holds == picked_holds &&
              (f->target == NULL || f->target->holds == target_holds) && same_source(f, &s));
        CHECK(blocked(f, &s));
        CHECK(mc_client_effect_renderer_draw_retire(&f->call, &f->cleanup) == MC_EFFECT_DRAW_OK &&
              f->call == NULL && f->renderer->draw_call == NULL &&
              f->renderer->holds == s.owner.holds && same_source(f, &s));
        CHECK(f->particle->holds == particle_holds && f->picked->holds == picked_holds &&
              (f->target == NULL || f->target->holds == target_holds));
        CHECK(mc_client_nw_inspect(f->graphics, &f->nw_after) &&
              f->nw_after.phase == MC_CLIENT_RENDER_ENGINE_READY && !f->nw_after.fields.w);
        CHECK(mc_client_render_engine_classes_inspect(f->graphics, &graphics_after) &&
              graphics_after.native_holds == graphics_before.native_holds);
        mc_math_class_state math_after;
        CHECK(mc_math_classes_state(f->math, &math_after, NULL) == MC_MATH_CLASS_OK &&
              math_after.holds == math_before.holds && math_after.phase == math.phase &&
              math_after.source_a == math.source_a);
    }
    CHECK(mc_client_effect_renderer_change_world(f->renderer, NULL, NULL) == MC_EFFECT_RENDERER_OK);
    CHECK(f->renderer->layers[3]->size == 0 && f->particle->holds + 1 == particle_holds &&
          f->renderer->layers[3]->capacity == s.lists[3].capacity &&
          f->renderer->layers[3]->elements == s.lists[3].elements);
    ok = true;
cleanup:
    if (f != NULL) {
        if (!closed(f))
            ok = false;
        free(f);
    }
    return ok;
}

static bool sprites(void) {
    bool ok = false;
    fixture *f = calloc(1, sizeof(*f));
    source_snapshot s;
    const unsigned int controls = _mm_getcsr();
    CHECK(f != NULL && opened(f));
    mc_math_class_state math;
    CHECK(snapshot(f, &s));
    for (size_t i = 0; i < sizeof(partial_bits) / sizeof(*partial_bits); ++i) {
        _mm_setcsr(controls | 0x8040u);
        const mc_effect_draw_status null_status = mc_client_effect_renderer_sprites_begin(
            f->renderer, NULL, float_from_bits(partial_bits[i]), NULL, &f->call, &f->error);
        _mm_setcsr(controls);
        CHECK(null_status == MC_EFFECT_DRAW_SOURCE_NULL);
        CHECK(mc_client_effect_renderer_draw_inspect(f->call, &f->state) &&
              f->state.source_pc == 1 && !f->state.failure->native_policy &&
              f->state.orientation_values == 0 && f->state.origins_stored == 0 &&
              f->state.size_reads == 0 && !f->state.nw_captured &&
              float_bits(f->state.partial) == partial_bits[i]);
        CHECK(mc_math_classes_state(f->math, &math, NULL) == MC_MATH_CLASS_OK &&
              math.phase == MC_MATH_CLASS_COLD && math.source_a == NULL);
        CHECK(mc_client_effect_renderer_draw_retire(&f->call, &f->cleanup) == MC_EFFECT_DRAW_OK &&
              same_source(f, &s) && f->renderer->holds == s.owner.holds &&
              f->renderer->draw_call == NULL);
    }
    CHECK(item(f->classes, &f->camera, 10.0, -4.0, 0.0));
    /* Declared actual camera pose inputs; these are source scalars only. */
    mc_entity_base_fields *pose = mc_entity_runtime_fields(f->camera);
    pose->yaw = 0.0f;
    pose->aT = 0.0f;
    pose->bl = 2.0;
    pose->bm = 4.0;
    pose->bn = 8.0;
    const mc_effect_draw_services services = {.math = f->math};
    CHECK(snapshot(f, &s));
    CHECK(mc_client_effect_renderer_sprites_begin(f->renderer, f->camera, 0.5f, &services, &f->call,
                                                  &f->error) == MC_EFFECT_DRAW_OK);
    CHECK(mc_client_effect_renderer_draw_inspect(f->call, &f->state));
    CHECK(f->state.returned && !f->state.lit && f->state.empty && f->state.source_pc == 311 &&
          f->state.orientation_values == 5 && f->state.origins_stored == 3 &&
          f->state.last_store_pc == 137 && f->state.size_reads == 3 && f->state.visited == 0 &&
          !f->state.nw_captured && f->state.current == NULL);
    CHECK(double_bits(f->state.origin.l) == double_bits(6.0) &&
          double_bits(f->state.origin.m) == double_bits(0.0) &&
          double_bits(f->state.origin.n) == double_bits(4.0));
    CHECK(float_bits(f->state.cos_yaw) == UINT32_C(0x3f800000) &&
          float_bits(f->state.sin_yaw) == 0 &&
          float_bits(f->state.cos_pitch) == UINT32_C(0x3f800000) &&
          float_bits(f->state.horizontal_y) == UINT32_C(0x80000000) &&
          float_bits(f->state.horizontal_x) == 0 && same_source(f, &s));
    CHECK(mc_math_classes_state(f->math, &math, NULL) == MC_MATH_CLASS_OK &&
          math.phase == MC_MATH_CLASS_READY && math.source_a != NULL);
    CHECK(mc_client_effect_renderer_draw_retire(&f->call, &f->cleanup) == MC_EFFECT_DRAW_OK &&
          same_source(f, &s));

    CHECK(pickup(f, false)); /* Real layer3 body would fail53 if invoked. */
    CHECK(snapshot(f, &s));
    const size_t particle_holds = f->particle->holds;
    CHECK(mc_client_effect_renderer_sprites_begin(f->renderer, f->camera, 1.0f, &services, &f->call,
                                                  &f->error) == MC_EFFECT_DRAW_OK);
    CHECK(mc_client_effect_renderer_draw_inspect(f->call, &f->state) && f->state.returned &&
          f->state.source_pc == 311 && f->state.size_reads == 3 && f->state.visited == 0 &&
          f->state.particle_pc == 0 && !f->state.nw_captured &&
          f->particle->holds == particle_holds &&
          double_bits(f->state.origin.l) == double_bits(10.0) &&
          double_bits(f->state.origin.m) == double_bits(-4.0) &&
          double_bits(f->state.origin.n) == double_bits(0.0) && same_source(f, &s));
    CHECK(blocked(f, &s));
    CHECK(mc_client_effect_renderer_draw_retire(&f->call, &f->cleanup) == MC_EFFECT_DRAW_OK);
    CHECK(mc_client_nw_inspect(f->graphics, &f->nw_after) &&
          f->nw_after.phase == MC_CLIENT_RENDER_ENGINE_RAW && f->nw_after.a == NULL);

    /* A second genuine DN in the same Entity namespace shares actual XW
     * origins; constructors retain their independent real Random owners. */
    CHECK(mc_client_effect_renderer_allocate(f->classes, NULL, &f->other, NULL) ==
          MC_EFFECT_RENDERER_OK);
    CHECK(mc_client_effect_renderer_construct(f->other, NULL, NULL, NULL) == MC_EFFECT_RENDERER_OK);
    CHECK(f->other->random != f->renderer->random && f->other->layers != f->renderer->layers);
    CHECK(mc_client_effect_renderer_sprites_begin(f->other, f->camera, 0.0f, &services,
                                                  &f->other_call, &f->error) == MC_EFFECT_DRAW_OK);
    mc_particle_origin origin;
    CHECK(mc_entity_particle_origin_read(f->classes, &origin, NULL) == MC_ENTITY_OK &&
          double_bits(origin.l) == double_bits(2.0) && double_bits(origin.m) == double_bits(4.0) &&
          double_bits(origin.n) == double_bits(8.0));
    CHECK(mc_client_effect_renderer_draw_retire(&f->other_call, &f->cleanup) == MC_EFFECT_DRAW_OK);
    CHECK(mc_client_effect_renderer_change_world(f->renderer, NULL, NULL) ==
              MC_EFFECT_RENDERER_OK &&
          f->particle->holds + 1 == particle_holds);
    ok = true;
cleanup:
    _mm_setcsr(controls);
    if (f != NULL) {
        if (!closed(f))
            ok = false;
        free(f);
    }
    return ok;
}

int main(void) {
    if (!empty_lit() || !nonempty_lit(false) || !nonempty_lit(true) || !sprites())
        return 1;
    printf("EffectRenderer draw CPU controls: %u checks\n", checks);
    return 0;
}
