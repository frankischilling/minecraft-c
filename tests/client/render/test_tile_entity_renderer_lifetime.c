/* Native ownership controls, independent of original-constructor parity.
 * Reuse the actual core fixture; no replacement dependency or copied body. */
#define main mc_tile_renderer_core_fixture_main
#include "test_tile_entity_renderer.c"
#undef main

#include "client/gui/loading_screen.h"
#include "client/render_engine/nw.h"
#include "core/java/random_profile_internal.h"
#include "entity/entity_internal.h"
#include "mc/entity/player/player.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/runtime_internal.h"

#include <xmmintrin.h>

typedef struct lifetime_assets {
    mc_client_render_engine_classes *graphics;
    mc_client_loading_classes *gui;
    mc_client_default_font *provider;
    mc_client_render_engine *engines[2];
    mc_client_bitmap_font *fonts[2];
    mc_client_bitmap_font_state *font_state;
} lifetime_assets;

typedef struct existing_owner_copy {
    const void *owner;
    void *bytes;
    size_t size;
} existing_owner_copy;

/* Snapshot actual quiescent owner representations, not separately initialized
 * structs whose padding could differ. All writes below target private copies
 * and disjoint API outputs. No native phase, hold or association is assigned. */
static bool existing_lookup(fixture *f, mc_client_render_classes *models,
                            mc_registry_classes *registry, mc_world_runtime_classes *worlds,
                            bool output, bool error_output, mc_client_tile_renderer_status expected,
                            mc_client_tile_renderer_classes *expected_owner) {
    existing_owner_copy copies[20] = {{0}};
    size_t count = 0;
#define COPY_OWNER(value)                                                                          \
    do {                                                                                           \
        if ((value) != NULL) {                                                                     \
            CHECK(count < sizeof(copies) / sizeof(copies[0]));                                     \
            copies[count] =                                                                        \
                (existing_owner_copy){(value), malloc(sizeof(*(value))), sizeof(*(value))};        \
            CHECK(copies[count].bytes != NULL);                                                    \
            memcpy(copies[count].bytes, copies[count].owner, copies[count].size);                  \
            ++count;                                                                               \
        }                                                                                          \
    } while (0)
    COPY_OWNER(f->models);
    COPY_OWNER(f->worlds);
    COPY_OWNER(f->entities);
    COPY_OWNER(f->profile);
    COPY_OWNER(models);
    COPY_OWNER(worlds);
    mc_client_tile_renderer_classes *owners[2] = {f->classes, models == NULL ? NULL : models->ll};
    for (size_t i = 0; i < 2; ++i) {
        mc_client_tile_renderer_classes *owner = owners[i];
        COPY_OWNER(owner);
        if (owner != NULL) {
            COPY_OWNER(owner->candidate);
            if (owner->candidate != NULL)
                for (size_t j = 0; j < owner->candidate->allocated_renderers; ++j) {
                    CHECK(j < sizeof(owner->candidate->renderers) /
                                  sizeof(owner->candidate->renderers[0]));
                    COPY_OWNER(owner->candidate->renderers[j]);
                }
        }
    }
#undef COPY_OWNER
    mc_tile_entity_piston_class_snapshot before, after;
    CHECK(mc_tile_entity_piston_class_get_snapshot(f->worlds, &before) == MC_TILE_ENTITY_OK);
    mc_registry_classes *registries[2] = {f->registry, registry};
    mc_registry_block_view registry_before[2];
    for (size_t i = 0; i < 2; ++i)
        if (registries[i] != NULL) {
            CHECK(mc_registry_classes_inspect_blocks(registries[i], &registry_before[i], NULL) ==
                  MC_REGISTRY_OK);
            CHECK(registry_before[i].phase == MC_REGISTRY_COLD &&
                  registry_before[i].named_blocks == 0 && registry_before[i].m == NULL);
        }
    mc_client_tile_renderer_error *error = malloc(sizeof(*error));
    CHECK(error != NULL);
    memset(error, 0xa5, sizeof(*error));
    /* The sentinel is a genuine live owner, or NULL when success must discover
     * a nonnull owner. No invented address is passed to the implementation. */
    mc_client_tile_renderer_classes *found =
        expected == MC_CLIENT_TILE_RENDERER_OK && expected_owner != NULL ? NULL : f->classes;
    mc_client_tile_renderer_classes *unchanged = found;
    CHECK(mc_client_tile_entity_renderer_classes_existing(models, registry, worlds,
                                                          output ? &found : NULL,
                                                          error_output ? error : NULL) == expected);
    CHECK(found == (expected == MC_CLIENT_TILE_RENDERER_OK ? expected_owner : unchanged));
    if (error_output) {
        CHECK(error->status == expected && error->initial_status == MC_CLIENT_TILE_RENDERER_OK &&
              error->pc == -1 && error->ll_pc == -1 && error->renderer_pc == -1 &&
              error->rf_pc == -1 && error->cv_pc == -1 && error->frame_pc == -1 &&
              error->source_class == NULL && error->source_method == NULL &&
              error->source_descriptor == NULL && error->model.status == MC_CLIENT_MODEL_OK &&
              error->tile_entity == MC_TILE_ENTITY_OK &&
              error->world.status == MC_WORLD_STORAGE_OK);
        CHECK((expected == MC_CLIENT_TILE_RENDERER_OK && error->operation == NULL &&
               !error->native_policy) ||
              (expected != MC_CLIENT_TILE_RENDERER_OK && error->operation != NULL &&
               error->native_policy));
    }
    free(error);
    for (size_t i = 0; i < count; ++i) {
        CHECK(memcmp(copies[i].bytes, copies[i].owner, copies[i].size) == 0);
        free(copies[i].bytes);
    }
    CHECK(mc_tile_entity_piston_class_get_snapshot(f->worlds, &after) == MC_TILE_ENTITY_OK);
    CHECK(before.base_phase == after.base_phase && before.piston_phase == after.piston_phase &&
          before.base_initializer_bytecode == after.base_initializer_bytecode &&
          before.piston_initializer_bytecode == after.piston_initializer_bytecode &&
          before.base_failure == after.base_failure &&
          before.piston_failure == after.piston_failure &&
          before.base_name_count == after.base_name_count &&
          before.base_class_count == after.base_class_count &&
          before.list_identity == after.list_identity && before.list_count == after.list_count &&
          before.list_capacity == after.list_capacity &&
          before.allocation_bytes == after.allocation_bytes &&
          before.live_tile_entities == after.live_tile_entities &&
          before.max_allocation_bytes == after.max_allocation_bytes &&
          before.class_literal_borrows == after.class_literal_borrows &&
          before.list_mod_count_bits == after.list_mod_count_bits);
    for (size_t i = 0; i < 2; ++i)
        if (registries[i] != NULL) {
            mc_registry_block_view after_registry;
            CHECK(mc_registry_classes_inspect_blocks(registries[i], &after_registry, NULL) ==
                  MC_REGISTRY_OK);
            CHECK(
                registry_before[i].phase == after_registry.phase &&
                registry_before[i].initializer_pc == after_registry.initializer_pc &&
                registry_before[i].allocation_bytes == after_registry.allocation_bytes &&
                registry_before[i].named_blocks == after_registry.named_blocks &&
                registry_before[i].m == after_registry.m && after_registry.d == NULL &&
                after_registry.e == NULL && after_registry.f == NULL && after_registry.g == NULL &&
                after_registry.h == NULL && after_registry.i == NULL && after_registry.j == NULL &&
                after_registry.k == NULL && after_registry.l == NULL && after_registry.n == NULL &&
                after_registry.o == NULL && after_registry.p == NULL && after_registry.q == NULL &&
                after_registry.r == NULL && after_registry.s == NULL && after_registry.t == NULL);
        }
    return true;
}

static bool existing_controls(void) {
    fixture f = {0};
    CHECK(fixture_open(&f));
    mc_client_render_classes *absent_models = NULL;
    mc_world_runtime_classes *other_worlds = NULL;
    mc_registry_classes *foreign_registry = NULL;
    CHECK(mc_client_render_classes_create(f.entities, f.profile, &absent_models) ==
          MC_CLIENT_MODEL_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f.profile, f.worlds->trig,
                                          &other_worlds, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(other_worlds, f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(other_worlds, f.entities) == MC_ENTITY_OK);
    const mc_registry_options options = {
        .side = MC_REGISTRY_CLIENT, .random_profile = f.profile, .materials = f.materials};
    CHECK(mc_registry_classes_allocate(&options, &foreign_registry, NULL) == MC_REGISTRY_OK);
    CHECK(absent_models->ll == NULL && !absent_models->ll_retired &&
          other_worlds->tile_entities == NULL && !other_worlds->initialized);
    CHECK(existing_lookup(&f, absent_models, f.registry, f.worlds, true, true,
                          MC_CLIENT_TILE_RENDERER_OK, NULL));
    CHECK(existing_lookup(&f, absent_models, f.registry, other_worlds, true, false,
                          MC_CLIENT_TILE_RENDERER_OK, NULL));
    CHECK(other_worlds->tile_entities == NULL && !other_worlds->initialized);
    CHECK(existing_lookup(&f, f.models, f.registry, f.worlds, true, true,
                          MC_CLIENT_TILE_RENDERER_OK, f.classes));
    CHECK(f.classes->phase == MC_CLIENT_TILE_RENDERER_COLD && f.classes->candidate == NULL &&
          f.classes->a == NULL && passive_classes(&f));
    CHECK(existing_lookup(&f, f.models, f.registry, other_worlds, true, true,
                          MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, NULL));
    CHECK(existing_lookup(&f, f.models, foreign_registry, f.worlds, true, true,
                          MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, NULL));
    CHECK(existing_lookup(&f, absent_models, foreign_registry, f.worlds, true, false,
                          MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, NULL));
    CHECK(existing_lookup(&f, NULL, f.registry, f.worlds, true, true,
                          MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, NULL));
    CHECK(existing_lookup(&f, f.models, NULL, f.worlds, true, true,
                          MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, NULL));
    CHECK(existing_lookup(&f, f.models, f.registry, NULL, true, true,
                          MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, NULL));
    CHECK(existing_lookup(&f, f.models, f.registry, f.worlds, false, true,
                          MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, NULL));

    /* A genuine failed active-use attempt, on a distinct actual Models owner:
     * the declared native quota admits COLD metadata but no dispatcher. This
     * is a native FAILED-owner control, not a Java constructor-failure claim. */
    mc_client_tile_renderer_options limited = mc_client_tile_renderer_default_options();
    limited.class_bytes = sizeof(mc_client_tile_renderer_classes);
    mc_client_tile_renderer_classes *failed = NULL;
    CHECK(mc_client_tile_renderer_classes_bind(absent_models, f.worlds, f.profile, &limited,
                                               &failed, &f.error) == MC_CLIENT_TILE_RENDERER_OK);
    mc_client_tile_renderer_dispatcher *unpublished = NULL;
    CHECK(mc_client_tile_renderer_static_get(failed, &unpublished, &f.error) ==
          MC_CLIENT_TILE_RENDERER_LIMIT);
    CHECK(unpublished == NULL && failed->phase == MC_CLIENT_TILE_RENDERER_FAILED &&
          failed->a == NULL && failed->candidate == NULL && !failed->active);
    CHECK(existing_lookup(&f, absent_models, f.registry, f.worlds, true, true,
                          MC_CLIENT_TILE_RENDERER_OK, failed));
    CHECK(existing_lookup(&f, absent_models, f.registry, f.worlds, true, false,
                          MC_CLIENT_TILE_RENDERER_OK, failed));
    CHECK(mc_client_tile_renderer_classes_retire(&failed, &f.error) == MC_CLIENT_TILE_RENDERER_OK &&
          failed == NULL && absent_models->ll == NULL && absent_models->ll_retired);
    CHECK(existing_lookup(&f, absent_models, f.registry, f.worlds, true, true,
                          MC_CLIENT_TILE_RENDERER_OK, NULL));

    CHECK(mc_client_tile_renderer_static_get(f.classes, &f.manager, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    mc_client_tile_renderer_consumer consumer = {0};
    CHECK(mc_client_tile_renderer_consumer_attach(f.classes, &consumer, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_dispatcher_retain(f.manager, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(existing_lookup(&f, f.models, f.registry, f.worlds, true, true,
                          MC_CLIENT_TILE_RENDERER_OK, f.classes));
    CHECK(existing_lookup(&f, f.models, f.registry, f.worlds, true, false,
                          MC_CLIENT_TILE_RENDERER_OK, f.classes));
    CHECK(consumer.attached && consumer.classes == f.classes && f.classes->consumers == 1 &&
          f.manager->holds == 2);
    mc_client_tile_renderer_dispatcher *extra = f.manager;
    CHECK(mc_client_tile_renderer_dispatcher_release(&extra, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          extra == NULL);
    CHECK(mc_client_tile_renderer_consumer_detach(&consumer, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_classes_retire(&f.classes, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          f.classes == NULL);
    f.manager = NULL;
    CHECK(existing_lookup(&f, f.models, f.registry, f.worlds, true, false,
                          MC_CLIENT_TILE_RENDERER_OK, NULL));
    CHECK(mc_client_render_classes_free(absent_models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_world_runtime_classes_free(other_worlds, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_registry_classes_free(foreign_registry, NULL) == MC_REGISTRY_OK);
    CHECK(fixture_close(&f));
    /* Active reentry and inactive RETIRING are intentionally not reached by
     * this fixture: no private guard write or fabricated cleanup failure. */
    return true;
}

static bool assets_open(lifetime_assets *a) {
    CHECK(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &a->graphics) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_loading_classes_create(a->graphics, &a->gui) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_default_font_create(SIZE_MAX, &a->provider) == MC_CLIENT_FONT_OK);
    a->font_state = calloc(1, sizeof(*a->font_state));
    CHECK(a->font_state != NULL);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_client_render_engine_allocate(a->graphics, a->provider, NULL, &a->engines[i]) ==
              MC_CLIENT_RENDER_ENGINE_OK);
        CHECK(mc_client_bitmap_font_allocate(a->gui, NULL, &a->fonts[i]) ==
              MC_CLIENT_BITMAP_FONT_OK);
    }
    return true;
}

static bool asset_holds(lifetime_assets *a, size_t engine0, size_t engine1, size_t font0,
                        size_t font1) {
    const size_t engines[2] = {engine0, engine1}, fonts[2] = {font0, font1};
    for (size_t i = 0; i < 2; ++i) {
        mc_client_render_engine_state engine;
        CHECK(mc_client_render_engine_inspect(a->engines[i], &engine));
        CHECK(engine.phase == MC_CLIENT_RENDER_ENGINE_RAW && engine.native_holds == engines[i] &&
              engine.constructor_bytes == 0 && engine.constructor_allocations == 0 &&
              !engine.graphics_created && engine.fields.b == NULL && engine.fields.m == NULL);
        CHECK(mc_client_bitmap_font_inspect(a->fonts[i], a->font_state));
        CHECK(a->font_state->phase == MC_CLIENT_BITMAP_FONT_RAW &&
              a->font_state->holds == fonts[i] && !a->font_state->active &&
              !a->font_state->has_native_generation && a->font_state->constructor_bytes == 0 &&
              a->font_state->constructor_allocations == 0 && a->font_state->fields.b == NULL &&
              a->font_state->fields.d == NULL);
    }
    mc_client_render_engine_class_state graphics;
    CHECK(mc_client_render_engine_classes_inspect(a->graphics, &graphics));
    CHECK(!graphics.ji_ready && graphics.ge_phase == MC_CLIENT_RENDER_ENGINE_RAW &&
          graphics.ge_a == NULL && graphics.ge_b == NULL && graphics.native_resource_count == 0 &&
          graphics.active_resource_calls == 0 && !graphics.resource_retirement_started);
    mc_client_font_snapshot provider;
    CHECK(mc_client_default_font_inspect(a->provider, &provider));
    CHECK(provider.native_holds == 3 && !provider.ready && !provider.failed &&
          provider.font_bytes == 0 && provider.completed_glyphs == 0);
    return true;
}

static bool assets_close(lifetime_assets *a) {
    CHECK(asset_holds(a, 1, 1, 1, 1));
    for (size_t i = 0; i < 2; ++i)
        CHECK(mc_client_bitmap_font_release(&a->fonts[i], NULL) == MC_CLIENT_BITMAP_FONT_OK &&
              a->fonts[i] == NULL);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_client_render_engine_release(a->engines[i]) == MC_CLIENT_RENDER_ENGINE_OK);
        a->engines[i] = NULL;
    }
    CHECK(mc_client_loading_classes_release(a->gui) == MC_CLIENT_LOADING_OK);
    a->gui = NULL;
    CHECK(mc_client_render_engine_classes_release(a->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    a->graphics = NULL;
    CHECK(mc_client_default_font_release(a->provider) == MC_CLIENT_FONT_OK);
    a->provider = NULL;
    free(a->font_state);
    a->font_state = NULL;
    return true;
}

static bool raw_world(fixture *f, mc_world_runtime **out) {
    mc_world_storage_error error;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->worlds, NULL,
                                          out, &error) == MC_WORLD_STORAGE_OK);
    mc_world_runtime_construction state;
    CHECK(mc_world_runtime_construction_get(*out, &state) == MC_WORLD_STORAGE_OK &&
          state.mode == MC_WORLD_RUNTIME_GAME && state.phase == MC_WORLD_RUNTIME_RAW);
    CHECK((*out)->persistent_bindings == 0);
    return true;
}

static bool world_close(mc_world_runtime **slot) {
    mc_world_storage_error error;
    CHECK(*slot != NULL && (*slot)->persistent_bindings == 0);
    CHECK(mc_world_runtime_retire_world(*slot, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(*slot, &error) == MC_WORLD_STORAGE_OK);
    *slot = NULL;
    return true;
}

/* Declared scalar inputs on the real RAW Living base, with no phase/reference
 * mutation. These dyadic values make the checked interpolation results exact. */
static void living_scalars(mc_entity *living, bool second) {
    mc_entity_base_fields *b = mc_entity_runtime_fields(living);
    b->aU = second ? 0.0f : 8.0f;
    b->yaw = second ? 16.0f : 24.0f;
    b->aV = second ? 2.0f : -4.0f;
    b->aT = second ? 6.0f : 12.0f;
    b->bl = second ? -4.0 : 1.0;
    b->x = second ? 4.0 : 9.0;
    b->bm = second ? 8.0 : -2.0;
    b->y = second ? 0.0 : 6.0;
    b->bn = second ? 1.0 : 4.0;
    b->z = second ? 9.0 : -4.0;
}

static bool scalar_state(const mc_client_tile_renderer_state *s, float h, float i, double j,
                         double k, double l) {
    CHECK(fb(s->fields.h) == fb(h) && fb(s->fields.i) == fb(i) && db(s->fields.j) == db(j) &&
          db(s->fields.k) == db(k) && db(s->fields.l) == db(l));
    return true;
}

static bool lifetime_controls(size_t *constructor_bytes) {
    fixture f = {0};
    lifetime_assets a = {0};
    mc_entity *living[2] = {0}, *foreign = NULL, *nonliving = NULL;
    mc_entity_classes *foreign_classes = NULL;
    mc_entity_error entity_error;
    mc_world_runtime *worlds[2] = {0};
    CHECK(fixture_open(&f));
    CHECK(mc_client_tile_renderer_static_get(f.classes, &f.manager, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    mc_client_tile_renderer_state s;
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s));
    *constructor_bytes = s.class_bytes;
    CHECK(assets_open(&a) && asset_holds(&a, 1, 1, 1, 1));
    CHECK(raw_world(&f, &worlds[0]) && raw_world(&f, &worlds[1]));
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_local_player_allocate(f.entities, &living[i], &entity_error) == MC_ENTITY_OK);
        living_scalars(living[i], i != 0);
        CHECK(living[i]->phase == MC_ENTITY_RAW && living[i]->holds == 1);
    }
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f.profile, NULL, &foreign_classes) ==
          MC_ENTITY_OK);
    CHECK(mc_local_player_allocate(foreign_classes, &foreign, &entity_error) == MC_ENTITY_OK);
    CHECK(mc_item_entity_allocate(f.entities, &nonliving, &entity_error) == MC_ENTITY_OK);

    CHECK(mc_client_tile_renderer_bind_frame(f.manager, worlds[0], a.engines[0], a.fonts[0], NULL,
                                             0.25f,
                                             &f.error) == MC_CLIENT_TILE_RENDERER_NULL_REFERENCE);
    CHECK(f.error.pc == 32 && f.error.ll_pc == 32 && f.error.frame_pc == 32 &&
          !f.error.native_policy && strcmp(f.error.source_descriptor, "(Lfd;Lji;Lsj;Lls;F)V") == 0);
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s) &&
          s.phase == MC_CLIENT_TILE_RENDERER_READY && s.fields.f == worlds[0] &&
          s.fields.e == a.engines[0] && s.fields.n == a.fonts[0] && s.fields.g == NULL &&
          s.last_frame_pc == 26 && s.retained_engines == 1 && s.retained_fonts == 1 &&
          !s.living_retained && worlds[0]->persistent_bindings == 2);
    CHECK(asset_holds(&a, 2, 1, 2, 1));
    CHECK(mc_client_bitmap_font_retire(&a.fonts[0], NULL) == MC_CLIENT_BITMAP_FONT_BUSY &&
          a.fonts[0] != NULL);
    CHECK(mc_client_render_engine_retire(&a.engines[0], NULL) == MC_CLIENT_RENDER_ENGINE_BUSY &&
          a.engines[0] != NULL);
    mc_client_tile_renderer_map_entry piston;
    CHECK(mc_client_tile_renderer_map_find(f.manager, f.literals[7], &piston, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          piston.present && piston.value != NULL);
    mc_client_tile_renderer_cv *old_cv = NULL;
    CHECK(mc_client_tile_renderer_cv_retain_current(piston.value, &old_cv, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          old_cv != NULL);
    mc_client_tile_renderer_cv_state cv;
    CHECK(mc_client_tile_renderer_cv_inspect(old_cv, &cv) && cv.current &&
          cv.fields->c == worlds[0] && cv.world_occurrence == worlds[0]);
    const void *cv_identity = cv.identity;

    CHECK(mc_client_tile_renderer_bind_frame(f.manager, worlds[0], a.engines[0], a.fonts[0],
                                             living[0], 0.25f,
                                             &f.error) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s) && s.last_frame_pc == 152 &&
          s.fields.g == living[0] && s.living_retained && s.retained_engines == 1 &&
          s.retained_fonts == 1 && living[0]->holds == 2 && living[0]->phase == MC_ENTITY_RAW);
    CHECK(scalar_state(&s, 12, 0, 3, 0, 2));
    CHECK(mc_client_tile_renderer_cv_inspect(old_cv, &cv) && cv.current &&
          cv.identity == cv_identity && worlds[0]->persistent_bindings == 2);
    CHECK(asset_holds(&a, 2, 1, 2, 1));

    _mm_setcsr(0x3f80u);
    const mc_client_tile_renderer_status fp_status = mc_client_tile_renderer_bind_frame(
        f.manager, worlds[0], a.engines[1], a.fonts[1], living[1], 0.5f, &f.error);
    const unsigned int rejected_fp = _mm_getcsr();
    _mm_setcsr(0x1f80u);
    CHECK(fp_status == MC_CLIENT_TILE_RENDERER_UNSUPPORTED_FP && rejected_fp == 0x3f80u &&
          f.error.pc == 45 && f.error.frame_pc == 45 && f.error.native_policy);
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s) && s.last_frame_pc == 26 &&
          s.fields.e == a.engines[1] && s.fields.n == a.fonts[1] && s.fields.g == living[1] &&
          s.retained_engines == 2 && s.retained_fonts == 2 && living[0]->holds == 1 &&
          living[1]->holds == 2 && !f.manager->active && !f.classes->active);
    CHECK(scalar_state(&s, 12, 0, 3, 0, 2));
    CHECK(asset_holds(&a, 2, 2, 2, 2));
    CHECK(mc_client_tile_renderer_bind_frame(f.manager, worlds[0], a.engines[1], a.fonts[1],
                                             living[1], 0.5f,
                                             &f.error) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s) && s.last_frame_pc == 152 &&
          s.retained_engines == 2 && s.retained_fonts == 2 && living[1]->holds == 2);
    CHECK(scalar_state(&s, 8, 4, 0, 4, 5));

    /* The stored Living occurrence must also survive when its caller drops
     * the last external hold, then retire its actual RAW subtype on replacement. */
    mc_entity_class_snapshot before_transient, after_transient;
    CHECK(mc_entity_classes_get_snapshot(f.entities, &before_transient) == MC_ENTITY_OK);
    mc_entity *transient = NULL;
    CHECK(mc_local_player_allocate(f.entities, &transient, &entity_error) == MC_ENTITY_OK);
    CHECK(mc_client_tile_renderer_bind_frame(f.manager, worlds[0], a.engines[1], a.fonts[1],
                                             transient, 0.5f,
                                             &f.error) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(transient->holds == 2 && transient->phase == MC_ENTITY_RAW);
    mc_entity_release(transient);
    transient = NULL;
    CHECK(mc_entity_classes_get_snapshot(f.entities, &after_transient) == MC_ENTITY_OK &&
          after_transient.live_entities == before_transient.live_entities + 1);
    CHECK(mc_client_tile_renderer_bind_frame(f.manager, worlds[0], a.engines[1], a.fonts[1],
                                             living[1], 0.5f,
                                             &f.error) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_entity_classes_get_snapshot(f.entities, &after_transient) == MC_ENTITY_OK &&
          after_transient.live_entities == before_transient.live_entities);
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s) && s.fields.g == living[1] &&
          living[1]->holds == 2 && s.retained_engines == 2 && s.retained_fonts == 2);
    CHECK(scalar_state(&s, 8, 4, 0, 4, 5));

    CHECK(mc_client_tile_renderer_bind_frame(f.manager, worlds[0], a.engines[0], a.fonts[0],
                                             foreign, 0.5f,
                                             &f.error) == MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT);
    CHECK(f.error.pc == 21 && f.error.frame_pc == 21 && f.error.native_policy &&
          f.error.entity_status == MC_ENTITY_INVALID_ARGUMENT && foreign->holds == 1);
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s) && s.last_frame_pc == 15 &&
          s.fields.e == a.engines[0] && s.fields.g == living[1] && s.fields.n == a.fonts[1]);
    CHECK(scalar_state(&s, 8, 4, 0, 4, 5));
    CHECK(mc_client_tile_renderer_bind_frame(f.manager, worlds[0], a.engines[1], a.fonts[0],
                                             nonliving, 0.5f,
                                             &f.error) == MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT);
    CHECK(f.error.pc == 21 && f.error.native_policy && nonliving->holds == 1);
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s) && s.last_frame_pc == 15 &&
          s.fields.e == a.engines[1] && s.fields.g == living[1] && s.fields.n == a.fonts[1]);

    for (size_t repeat = 0; repeat < 2; ++repeat) {
        _mm_setcsr(0x3f80u);
        const mc_client_tile_renderer_status status = mc_client_tile_renderer_bind_frame(
            f.manager, worlds[0], NULL, NULL, NULL, 0.5f, &f.error);
        const unsigned int unchanged_fp = _mm_getcsr();
        _mm_setcsr(0x1f80u);
        CHECK(status == MC_CLIENT_TILE_RENDERER_NULL_REFERENCE && f.error.pc == 32 &&
              !f.error.native_policy && unchanged_fp == 0x3f80u);
        CHECK(mc_client_tile_renderer_inspect(f.classes, &s) && s.fields.e == NULL &&
              s.fields.g == NULL && s.fields.n == NULL && !s.living_retained &&
              s.retained_engines == 2 && s.retained_fonts == 2 && living[1]->holds == 1);
        CHECK(scalar_state(&s, 8, 4, 0, 4, 5));
        CHECK(asset_holds(&a, 2, 2, 2, 2));
    }

    CHECK(mc_client_tile_renderer_bind_frame(f.manager, worlds[1], a.engines[0], a.fonts[0],
                                             living[0], 0.5f,
                                             &f.error) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s) && s.fields.f == worlds[1] &&
          s.fields.e == a.engines[0] && s.fields.n == a.fonts[0] && s.fields.g == living[0] &&
          s.last_frame_pc == 152 && worlds[0]->persistent_bindings == 1 &&
          worlds[1]->persistent_bindings == 2 && living[0]->holds == 2);
    CHECK(scalar_state(&s, 16, 4, 5, 2, 0));
    CHECK(mc_client_tile_renderer_cv_inspect(old_cv, &cv) && !cv.current &&
          cv.identity == cv_identity && cv.world_occurrence == worlds[0] &&
          cv.fields->c == worlds[0]);
    CHECK(mc_client_tile_renderer_classes_retire(&f.classes, &f.error) ==
              MC_CLIENT_TILE_RENDERER_BUSY &&
          f.classes != NULL);
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s) &&
          s.phase == MC_CLIENT_TILE_RENDERER_READY && s.fields.g == living[0] &&
          s.retained_engines == 2 && s.retained_fonts == 2);
    CHECK(asset_holds(&a, 2, 2, 2, 2));
    mc_world_storage_error world_error;
    CHECK(mc_world_runtime_retire_world(worlds[0], &world_error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(worlds[0], &world_error) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_client_tile_renderer_cv_release(&old_cv, &f.error) == MC_CLIENT_TILE_RENDERER_OK &&
          old_cv == NULL && worlds[0]->persistent_bindings == 0);
    CHECK(world_close(&worlds[0]));

    CHECK(mc_client_tile_renderer_classes_retire(&f.classes, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          f.classes == NULL);
    f.manager = NULL;
    CHECK(living[0]->holds == 1 && living[1]->holds == 1 && worlds[1]->persistent_bindings == 0);
    CHECK(asset_holds(&a, 1, 1, 1, 1));
    CHECK(world_close(&worlds[1]));
    CHECK(assets_close(&a));
    for (size_t i = 0; i < 2; ++i)
        mc_entity_release(living[i]);
    mc_entity_release(nonliving);
    mc_entity_release(foreign);
    CHECK(mc_entity_classes_free(foreign_classes) == MC_ENTITY_OK);
    CHECK(fixture_close(&f));
    return true;
}

static bool quota_controls(size_t constructor_bytes, bool font_failure) {
    fixture f = {0};
    lifetime_assets a = {0};
    CHECK(fixture_open(&f));
    /* End a genuinely COLD native association and create a distinct real
     * RenderClasses owner. No existing namespace or source phase is reset. */
    CHECK(mc_client_tile_renderer_classes_retire(&f.classes, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_render_classes_free(f.models) == MC_CLIENT_MODEL_OK);
    f.models = NULL;
    CHECK(mc_client_render_classes_create(f.entities, f.profile, &f.models) == MC_CLIENT_MODEL_OK);
    mc_client_tile_renderer_options options = mc_client_tile_renderer_default_options();
    const size_t captured = font_failure ? sizeof(tile_renderer_engine_occurrence) : 0;
    const size_t rejected = font_failure ? sizeof(tile_renderer_font_occurrence)
                                         : sizeof(tile_renderer_engine_occurrence);
    CHECK(constructor_bytes <= SIZE_MAX - captured &&
          constructor_bytes + captured <= SIZE_MAX - rejected);
    options.class_bytes = constructor_bytes + captured + rejected - 1;
    CHECK(mc_client_tile_renderer_classes_bind(f.models, f.worlds, f.profile, &options, &f.classes,
                                               &f.error) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_static_get(f.classes, &f.manager, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    mc_client_tile_renderer_state s;
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s) && s.class_bytes == constructor_bytes);
    CHECK(assets_open(&a));
    mc_entity *living = NULL;
    mc_entity_error entity_error;
    CHECK(mc_local_player_allocate(f.entities, &living, &entity_error) == MC_ENTITY_OK);
    mc_world_runtime *world = NULL;
    CHECK(raw_world(&f, &world));
    CHECK(mc_client_tile_renderer_bind_frame(f.manager, world, a.engines[0], a.fonts[0], living,
                                             0.5f, &f.error) == MC_CLIENT_TILE_RENDERER_LIMIT);
    const int32_t pc = font_failure ? 26 : 15;
    CHECK(f.error.pc == pc && f.error.frame_pc == pc && f.error.native_policy &&
          strcmp(f.error.source_descriptor, "(Lfd;Lji;Lsj;Lls;F)V") == 0);
    CHECK(mc_client_tile_renderer_inspect(f.classes, &s) &&
          s.phase == MC_CLIENT_TILE_RENDERER_READY && s.fields.f == world &&
          world->persistent_bindings == 2 && s.fields.n == NULL &&
          s.fields.e == (font_failure ? a.engines[0] : NULL) &&
          s.fields.g == (font_failure ? living : NULL) &&
          s.last_frame_pc == (font_failure ? 21 : 10) &&
          s.retained_engines == (font_failure ? 1u : 0u) && s.retained_fonts == 0 &&
          s.class_bytes == constructor_bytes + captured &&
          living->holds == (font_failure ? 2u : 1u));
    CHECK(asset_holds(&a, font_failure ? 2u : 1u, 1, 1, 1));
    CHECK(mc_client_tile_renderer_classes_retire(&f.classes, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          f.classes == NULL && living->holds == 1 && world->persistent_bindings == 0);
    f.manager = NULL;
    CHECK(world_close(&world) && assets_close(&a));
    mc_entity_release(living);
    CHECK(fixture_close(&f));
    return true;
}

int main(void) {
    const unsigned int saved_fp = _mm_getcsr();
    _mm_setcsr(0x1f80u);
    size_t constructor_bytes = 0;
    const bool ok = existing_controls() && lifetime_controls(&constructor_bytes) &&
                    quota_controls(constructor_bytes, false) &&
                    quota_controls(constructor_bytes, true);
    _mm_setcsr(saved_fp);
    if (!ok)
        return 1;
    printf("TileEntityRenderer lifetime: %u checks; cleanup=1\n", checks);
    return ferror(stdout) == 0 ? 0 : 1;
}
