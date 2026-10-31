/* Reuse the exact canonical CPU fixture and checked retirement, not copied
 * constructor bodies or a replacement dependency. Compile this file alone. */
#define main mc_tile_renderer_core_fixture_main
#include "test_tile_entity_renderer.c"
#undef main

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef struct world_observation {
    fixture f;
    const void *map;
    mc_client_tile_renderer *renderers[3];
    const mc_client_sign_model *sign;
    const void *spawner_map;
    mc_client_tile_renderer_cv *retained[3];
} world_observation;

/* Exactly the five declared source-field inputs in the unchanged observer.
 * No references, constructor phase, native hold or other source cell is seeded. */
static void seed_scalars(mc_client_tile_renderer_dispatcher *manager) {
    const uint32_t floats[2] = {UINT32_C(0x3f800001), UINT32_C(0xbf400002)};
    const uint64_t doubles[3] = {UINT64_C(0x3ff4000000000001), UINT64_C(0xc004000000000002),
                                 UINT64_C(0x400e000000000003)};
    memcpy(&manager->fields.h, &floats[0], sizeof(floats[0]));
    memcpy(&manager->fields.i, &floats[1], sizeof(floats[1]));
    memcpy(&manager->fields.j, &doubles[0], sizeof(doubles[0]));
    memcpy(&manager->fields.k, &doubles[1], sizeof(doubles[1]));
    memcpy(&manager->fields.l, &doubles[2], sizeof(doubles[2]));
}

/* All declared reference operands are NULL. An unexpected nonnull source
 * reference is reported explicitly, never assigned a fabricated Java type. */
static const char *null_reference(const void *value) {
    return value == NULL ? "null" : "non-null";
}

static const char *cv_type(const mc_client_render_blocks *value) {
    return value == NULL ? "null" : "cv"; /* This canonical storage type IS actual CV. */
}

static bool world_open(world_observation *o) {
    fixture *f = &o->f;
    CHECK(fixture_open(f));
    CHECK(mc_client_tile_renderer_static_get(f->classes, &f->manager, &f->error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    o->map = f->manager->fields.m;
    const size_t keys[3] = {4, 5, 7};
    for (size_t i = 0; i < 3; ++i) {
        mc_client_tile_renderer_map_entry entry;
        mc_client_tile_renderer_object_state object;
        CHECK(mc_client_tile_renderer_map_find(f->manager, f->literals[keys[i]], &entry,
                                               &f->error) == MC_CLIENT_TILE_RENDERER_OK &&
              entry.present && entry.value != NULL);
        o->renderers[i] = entry.value;
        CHECK(mc_client_tile_renderer_object_inspect(entry.value, &object));
        if (i == 0)
            o->sign = object.sign_model;
        if (i == 1)
            o->spawner_map = object.spawner_map;
    }
    CHECK(o->map != NULL && o->sign != NULL && o->spawner_map != NULL);
    mc_world_runtime_class_state worlds;
    CHECK(mc_world_runtime_classes_get(f->worlds, &worlds) == MC_WORLD_STORAGE_OK &&
          worlds.live_worlds == 0 && !worlds.initialized);
    seed_scalars(f->manager);
    return true;
}

static bool world_state(world_observation *o, const char *stage) {
    fixture *f = &o->f;
    mc_client_tile_renderer_state state;
    mc_client_tile_renderer_object_state objects[3];
    CHECK(mc_client_tile_renderer_inspect(f->classes, &state));
    CHECK(state.phase == MC_CLIENT_TILE_RENDERER_READY && state.a == f->manager);
    row("TILE_WORLD_LL %s %s %s %s %s %08" PRIx32 " %08" PRIx32 " %016" PRIx64 " %016" PRIx64
        " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %zu %d %d",
        stage, null_reference(state.fields.n), null_reference(state.fields.e),
        null_reference(state.fields.f), null_reference(state.fields.g), fb(state.fields.h),
        fb(state.fields.i), db(state.fields.j), db(state.fields.k), db(state.fields.l), db(state.b),
        db(state.c), db(state.d), state.entries, state.fields.m == o->map, state.a == f->manager);
    const size_t keys[3] = {4, 5, 7};
    for (size_t i = 0; i < 3; ++i) {
        mc_client_tile_renderer_map_entry entry;
        mc_tile_entity_class_literal_view key;
        CHECK(mc_client_tile_renderer_map_find(f->manager, f->literals[keys[i]], &entry,
                                               &f->error) == MC_CLIENT_TILE_RENDERER_OK &&
              entry.present && entry.value != NULL);
        CHECK(mc_client_tile_renderer_object_inspect(entry.value, &objects[i]));
        CHECK(mc_tile_entity_class_literal_inspect(entry.key, &key) == MC_TILE_ENTITY_OK);
        row("TILE_WORLD_JE %s %s %s %d %d", stage, key.client_name, objects[i].source_class,
            entry.value == o->renderers[i], objects[i].manager == f->manager);
    }
    row("TILE_WORLD_CHILDREN %s %d %d %zu %s", stage, objects[0].sign_model == o->sign,
        objects[1].spawner_map == o->spawner_map, objects[1].spawner_entries,
        cv_type(objects[2].piston_blocks));
    return true;
}

static bool cv_fields(world_observation *o, size_t step, mc_client_tile_renderer_cv *handle) {
    mc_client_tile_renderer_cv_state state;
    CHECK(mc_client_tile_renderer_cv_inspect(handle, &state));
    CHECK(state.fields != NULL && state.identity == state.fields && state.external_holds != 0);
    const mc_client_render_blocks *cv = state.fields;
    const unsigned int before = rows;
    bool ready = false, a = false;
    CHECK(mc_client_render_blocks_static_get(o->f.models, &ready, &a) && ready &&
          state.static_ready == ready && state.static_a == a);
    /* Semantic field-name encoding reads the real typed source cells. The
     * source declared static a is the shared model-Class cell, not in CV. */
    row("TILE_WORLD_FIELD %zu c I Lxp; %s", step, null_reference(cv->c));
    row("TILE_WORLD_FIELD %zu d I I %" PRId32, step, cv->d);
    row("TILE_WORLD_FIELD %zu e I Z %d", step, cv->e);
    row("TILE_WORLD_FIELD %zu f I Z %d", step, cv->f);
    row("TILE_WORLD_FIELD %zu a S Z %d", step, a);
    row("TILE_WORLD_FIELD %zu b I Z %d", step, cv->b);
    const int32_t *integers[6] = {&cv->g, &cv->h, &cv->i, &cv->j, &cv->k, &cv->l};
    for (size_t i = 0; i < 6; ++i)
        row("TILE_WORLD_FIELD %zu %c I I %" PRId32, step, (int)('g' + i), *integers[i]);
    row("TILE_WORLD_FIELD %zu m I Z %d", step, cv->m);
    for (size_t i = 0; i < COUNT(cv->n_to_N); ++i) {
        const int name = i < 13 ? (int)('n' + i) : (int)('A' + i - 13);
        row("TILE_WORLD_FIELD %zu %c I F %08" PRIx32, step, name, fb(cv->n_to_N[i]));
    }
    row("TILE_WORLD_FIELD %zu O I I %" PRId32, step, cv->O);
    for (size_t i = 0; i < 11; ++i)
        row("TILE_WORLD_FIELD %zu %c I F %08" PRIx32, step, (int)('P' + i), fb(cv->P_to_aa[i]));
    row("TILE_WORLD_FIELD %zu aa I F %08" PRIx32, step, fb(cv->P_to_aa[11]));
    for (size_t i = 0; i < COUNT(cv->ab_to_am); ++i)
        row("TILE_WORLD_FIELD %zu a%c I Z %d", step, (int)('b' + i), cv->ab_to_am[i]);
    CHECK(rows - before == 65);
    return true;
}

static bool world_callbacks(world_observation *o) {
    const mc_client_render_blocks *previous = NULL;
    for (size_t step = 0; step < COUNT(o->retained); ++step) {
        CHECK(mc_client_tile_renderer_change_world(o->f.manager, NULL, &o->f.error) ==
              MC_CLIENT_TILE_RENDERER_OK);
        CHECK(mc_client_tile_renderer_cv_retain_current(
                  o->renderers[2], &o->retained[step], &o->f.error) == MC_CLIENT_TILE_RENDERER_OK &&
              o->retained[step] != NULL);
        mc_client_tile_renderer_cv_state current;
        CHECK(mc_client_tile_renderer_cv_inspect(o->retained[step], &current));
        bool distinct = true;
        for (size_t prior = 0; prior < step; ++prior) {
            mc_client_tile_renderer_cv_state older;
            CHECK(mc_client_tile_renderer_cv_inspect(o->retained[prior], &older));
            distinct = distinct && older.identity != current.identity;
        }
        row("TILE_WORLD_CV_ID %zu %s %d %d %d", step, cv_type(current.fields), previous == NULL,
            current.fields != previous, distinct);
        CHECK(cv_fields(o, step, o->retained[step]));
        char stage[24];
        (void)snprintf(stage, sizeof(stage), "world%zu", step);
        CHECK(world_state(o, stage));
        previous = current.fields; /* Held by retained[step] across the next replacement. */
    }
    return true;
}

static bool world_frame_null(world_observation *o, bool cold) {
    mc_client_tile_renderer_object_state before, after;
    CHECK(mc_client_tile_renderer_object_inspect(o->renderers[2], &before));
    CHECK(world_state(o, "before_frame"));
    const uint32_t partial_bits = UINT32_C(0x3f000000);
    float partial;
    memcpy(&partial, &partial_bits, sizeof(partial));
    CHECK(mc_client_tile_renderer_bind_frame(o->f.manager, NULL, NULL, NULL, NULL, partial,
                                             &o->f.error) ==
          MC_CLIENT_TILE_RENDERER_NULL_REFERENCE);
    const mc_client_tile_renderer_error error = o->f.error;
    CHECK(!error.native_policy && error.pc == 32 && error.ll_pc == 32 && error.frame_pc == 32 &&
          strcmp(error.source_class, "ll") == 0 && strcmp(error.source_method, "a") == 0 &&
          strcmp(error.source_descriptor, "(Lfd;Lji;Lsj;Lls;F)V") == 0);
    printf("TILE_WORLD_NATIVE_ERROR %d NULL_REFERENCE %d %s %s %s %d %d %d\n", (int)error.status,
           error.native_policy, error.source_class, error.source_method, error.source_descriptor,
           error.pc, error.ll_pc, error.frame_pc);
    /* Source NPE category with actual native PC32; the driver records the
     * sole original Java stack-line75 -> PC32 projection separately. */
    row("TILE_WORLD_FRAME_NULL java.lang.NullPointerException %s %s pc%d", error.source_class,
        error.source_method, error.pc);
    CHECK(mc_client_tile_renderer_object_inspect(o->renderers[2], &after));
    row("TILE_WORLD_FRAME_ID %d %d %d", before.piston_blocks == NULL, after.piston_blocks == NULL,
        before.piston_blocks == after.piston_blocks);
    CHECK(before.piston_blocks == after.piston_blocks && before.live_cvs == after.live_cvs &&
          before.retained_cv_holds == after.retained_cv_holds);
    bool ready = false, a = false;
    CHECK(mc_client_render_blocks_static_get(o->f.models, &ready, &a));
    CHECK(ready == !cold && a == !cold);
    CHECK(world_state(o, "after_frame"));
    return true;
}

static bool world_observe(const char *profile) {
    world_observation o = {0};
    CHECK(world_open(&o));
    const bool callbacks = strcmp(profile, "callbacks") == 0;
    row("TILE_WORLD_CASE %s", profile);
    CHECK(world_state(&o, "initial"));
    if (callbacks)
        CHECK(world_callbacks(&o));
    CHECK(world_frame_null(&o, !callbacks));
    row("TILE_WORLD_DONE %s %d 1", profile, callbacks ? 3 : 0);
    for (size_t i = 0; i < COUNT(o.retained); ++i)
        CHECK(mc_client_tile_renderer_cv_release(&o.retained[i], &o.f.error) ==
                  MC_CLIENT_TILE_RENDERER_OK &&
              o.retained[i] == NULL);
    CHECK(fixture_close(&o.f));
    CHECK(ferror(stdout) == 0);
    printf("Source TileEntityRenderer World: %s; rows=%u; checks=%u; cleanup=1\n", profile, rows,
           checks);
    return true;
}

static bool world_controls(void) {
    world_observation o = {0};
    CHECK(world_open(&o));
    fixture *f = &o.f;
    for (size_t i = 0; i < COUNT(o.retained); ++i)
        CHECK(mc_client_tile_renderer_change_world(f->manager, NULL, &f->error) ==
                  MC_CLIENT_TILE_RENDERER_OK &&
              mc_client_tile_renderer_cv_retain_current(o.renderers[2], &o.retained[i],
                                                        &f->error) == MC_CLIENT_TILE_RENDERER_OK &&
              o.retained[i] != NULL);
    mc_client_tile_renderer_state before, after;
    mc_client_tile_renderer_object_state objects_before, objects_after;
    mc_client_tile_renderer_cv_state first;
    CHECK(mc_client_tile_renderer_inspect(f->classes, &before) &&
          mc_client_tile_renderer_object_inspect(o.renderers[2], &objects_before) &&
          mc_client_tile_renderer_cv_inspect(o.retained[0], &first) &&
          objects_before.live_cvs == COUNT(o.retained) &&
          objects_before.retained_cv_holds == COUNT(o.retained));
    mc_client_tile_renderer_classes *same = f->classes;
    CHECK(mc_client_tile_renderer_classes_retire(&f->classes, &f->error) ==
              MC_CLIENT_TILE_RENDERER_BUSY &&
          f->classes == same && mc_client_tile_renderer_inspect(f->classes, &after) &&
          after.phase == before.phase && after.phase == MC_CLIENT_TILE_RENDERER_READY &&
          after.a == before.a && after.class_bytes == before.class_bytes &&
          mc_client_tile_renderer_object_inspect(o.renderers[2], &objects_after) &&
          objects_after.live_cvs == objects_before.live_cvs &&
          objects_after.retained_cv_holds == objects_before.retained_cv_holds &&
          objects_after.piston_blocks == objects_before.piston_blocks);
    CHECK(mc_client_tile_renderer_cv_release(&o.retained[0], &f->error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          mc_client_tile_renderer_object_inspect(o.renderers[2], &objects_after) &&
          objects_after.live_cvs + 1 == objects_before.live_cvs &&
          objects_after.allocation_bytes + first.allocation_bytes ==
              objects_before.allocation_bytes);
    for (size_t i = 1; i < COUNT(o.retained); ++i)
        CHECK(mc_client_tile_renderer_cv_release(&o.retained[i], &f->error) ==
              MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_object_inspect(o.renderers[2], &objects_before) &&
          objects_before.live_cvs == 1 && objects_before.retained_cv_holds == 0);
    for (size_t i = 0; i < 8; ++i) {
        CHECK(mc_client_tile_renderer_change_world(f->manager, NULL, &f->error) ==
                  MC_CLIENT_TILE_RENDERER_OK &&
              mc_client_tile_renderer_object_inspect(o.renderers[2], &objects_after) &&
              objects_after.live_cvs == objects_before.live_cvs &&
              objects_after.allocation_bytes == objects_before.allocation_bytes &&
              objects_after.retained_cv_holds == 0);
    }
    /* A separate genuine namespace with an explicit native byte limit. It
     * fails before the first RF body, preserving the caller's output and the
     * failed candidate on later active use; no class phase is manufactured. */
    mc_client_render_classes *limited_models = NULL;
    mc_client_tile_renderer_classes *limited = NULL;
    CHECK(mc_client_render_classes_create(f->entities, f->profile, &limited_models) ==
          MC_CLIENT_MODEL_OK);
    mc_client_tile_renderer_options options = mc_client_tile_renderer_default_options();
    options.renderer_bytes = 1;
    CHECK(mc_client_tile_renderer_classes_bind(limited_models, f->worlds, f->profile, &options,
                                               &limited, &f->error) == MC_CLIENT_TILE_RENDERER_OK);
    mc_client_tile_renderer_dispatcher *preserved = f->manager;
    const mc_client_tile_renderer_status failed =
        mc_client_tile_renderer_static_get(limited, &preserved, &f->error);
    const mc_client_tile_renderer_error first_error = f->error;
    CHECK(failed == MC_CLIENT_TILE_RENDERER_MODEL_FAILED && preserved == f->manager &&
          f->error.model.status == MC_CLIENT_MODEL_LIMIT &&
          mc_client_tile_renderer_inspect(limited, &before) &&
          before.phase == MC_CLIENT_TILE_RENDERER_FAILED && before.a == NULL &&
          before.candidate != NULL && before.allocated_renderers == 1 && before.entries == 0);
    CHECK(mc_client_tile_renderer_static_get(limited, &preserved, &f->error) ==
              MC_CLIENT_TILE_RENDERER_CLASS_ERRONEOUS &&
          f->error.initial_status == failed && f->error.model.status == first_error.model.status &&
          f->error.pc == first_error.pc && f->error.ll_pc == first_error.ll_pc &&
          f->error.source_class == first_error.source_class &&
          f->error.source_method == first_error.source_method &&
          f->error.source_descriptor == first_error.source_descriptor &&
          f->error.native_policy == first_error.native_policy && preserved == f->manager &&
          mc_client_tile_renderer_inspect(limited, &after) && before.candidate == after.candidate &&
          before.class_bytes == after.class_bytes && after.phase == before.phase);
    CHECK(mc_client_tile_renderer_classes_retire(&limited, &f->error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          limited == NULL && mc_client_render_classes_free(limited_models) == MC_CLIENT_MODEL_OK);
    CHECK(fixture_close(f));
    CHECK(ferror(stdout) == 0);
    printf("Source TileEntityRenderer World controls: checks=%u; cleanup=1\n", checks);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--controls") == 0)
        return world_controls() ? 0 : 1;
    if (argc != 2 ||
        (strcmp(argv[1], "callbacks") != 0 && strcmp(argv[1], "frame_null_cold") != 0)) {
        fputs("TileEntityRenderer World requires callbacks|frame_null_cold|--controls\n", stderr);
        return 1;
    }
    return world_observe(argv[1]) ? 0 : 1;
}
