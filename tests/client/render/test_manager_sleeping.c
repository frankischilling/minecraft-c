/* Authored native-only TH sleeping/lifetime controls. Compile this file alone.
 * The real Statistics-first, 28-renderer fixture is shared without replacing
 * any constructors. No JVM, display, fabricated World or provider callbacks. */
int mc_manager_sleeping_core_fixture_main(int argc, char **argv);
#define main mc_manager_sleeping_core_fixture_main
#include "test_manager.c"
#undef main

#include "client/gui/loading_screen.h"
#include "entity/player/player_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/client/fonts/default_font.h"
#include "mc/client/settings/game_settings.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/entities.h"
#include <inttypes.h>
#include <stdarg.h>
#include <xmmintrin.h>

static unsigned sleeping_rows;

static void sleeping_row(const char *name, const char *format, ...) {
    printf("MANAGER_SLEEP %s ", name);
    va_list arguments;
    va_start(arguments, format);
    vprintf(format, arguments);
    va_end(arguments);
    putchar('\n');
    /* Registry source println writes its real captured native stdout handle.
     * Commit observer rows so later fresh-context source output cannot overtake
     * them in a C stdio buffer. Do not suppress or redirect source output. */
    if (fflush(stdout) != 0) {
        fputs("sleeping observer stdout flush failed\n", stderr);
        exit(2);
    }
    ++sleeping_rows;
}

static int complete_manager(fixture *f, const char *resources, mc_client_render_manager **manager) {
    CHECK(open_fixture(f, resources, NULL) == 0);
    CHECK(mc_registry_classes_initialize_statistics(f->registry, &f->error->registry) ==
          MC_REGISTRY_OK);
    CHECK(mc_client_render_manager_static_get(f->classes, manager, f->error) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(inspect_complete(f, *manager, true) == 0);
    return 0;
}

/* Only declared source pose/scalar inputs are changed. No phase, Class,
 * reference, count, provider callback or native ownership field is assigned. */
static void pose(mc_entity *actor) {
    actor->base.x = 8.5;
    actor->base.y = 64.0;
    actor->base.z = 8.5;
    actor->base.bl = 6.5;
    actor->base.bm = 62.0;
    actor->base.bn = 4.5;
    actor->local_player->fields.player.u = true;
}

static void scalar_inputs(mc_client_render_manager *manager) {
    manager->fields.i = 13.25f;
    manager->fields.j = -9.5f;
    manager->fields.l = 101.25;
    manager->fields.m = -202.5;
    manager->fields.n = 303.75;
}

static int unchanged_scalars(const mc_client_render_manager *manager) {
    CHECK(
        bits32(manager->fields.i) == bits32(13.25f) && bits32(manager->fields.j) == bits32(-9.5f) &&
        bits64(manager->fields.l) == bits64(101.25) &&
        bits64(manager->fields.m) == bits64(-202.5) && bits64(manager->fields.n) == bits64(303.75));
    return 0;
}

static int five_references(const mc_client_render_manager *manager, mc_world_runtime *world,
                           mc_entity *actor) {
    CHECK(manager->fields.g == world && manager->fields.e == NULL && manager->fields.k == NULL &&
          manager->fields.h == actor && manager->fields.p == NULL);
    return 0;
}

static int failed_math(fixture *f, mc_client_render_manager *manager, mc_entity *actor,
                       mc_math_classes **namespace_math) {
    mc_math_class_state state;
    CHECK(mc_math_classes_create(SIZE_MAX, namespace_math, NULL) == MC_MATH_CLASS_OK);
    mc_math_classes *failed = *namespace_math;
    CHECK(mc_math_classes_state(failed, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_COLD && state.source_a == NULL);
    scalar_inputs(manager);
    const unsigned strict = _mm_getcsr();
    _mm_setcsr((strict & ~0x6000u) | 0x2000u);
    const mc_client_manager_status first = mc_client_render_manager_bind_frame(
        manager, failed, NULL, NULL, NULL, actor, NULL, 0.5f, f->error);
    _mm_setcsr(strict);
    CHECK(first == MC_CLIENT_MANAGER_MATH_FAILED && f->error->frame_pc == 41 &&
          f->error->math.status == MC_MATH_CLASS_UNSUPPORTED_FP && f->error->math.source_pc == 20 &&
          f->error->native_policy && manager->last_frame_pc == 29);
    CHECK(five_references(manager, NULL, actor) == 0 && unchanged_scalars(manager) == 0);
    CHECK(mc_math_classes_state(failed, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_FAILED && state.last_store_pc == 4 &&
          state.source_a != NULL);
    const mc_math_trig *published = state.source_a;
    for (size_t i = 0; i < MC_MATH_TRIG_VALUES; ++i)
        CHECK(bits32(published->values[i]) == 0);
    sleeping_row("math_failed", "%d %d %d %d %d %u %u", (int)first, f->error->frame_pc,
                 (int)f->error->math.status, f->error->math.source_pc, (int)state.phase,
                 state.last_store_pc, MC_MATH_TRIG_VALUES);
    CHECK(mc_client_render_manager_bind_frame(manager, failed, NULL, NULL, NULL, actor, NULL, 0.5f,
                                              f->error) == MC_CLIENT_MANAGER_MATH_FAILED &&
          f->error->frame_pc == 41 &&
          f->error->math.status == MC_MATH_CLASS_INITIALIZATION_FAILED &&
          f->error->math.initial_cause == MC_MATH_CLASS_UNSUPPORTED_FP &&
          f->error->math.source_pc == 20);
    CHECK(mc_math_classes_state(failed, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_FAILED && state.source_a == published &&
          state.last_store_pc == 4);
    for (size_t i = 0; i < MC_MATH_TRIG_VALUES; ++i)
        CHECK(bits32(published->values[i]) == 0);
    CHECK(unchanged_scalars(manager) == 0 && five_references(manager, NULL, actor) == 0);
    sleeping_row("math_retry", "%d %d %d %d %d %u", (int)f->error->status, f->error->frame_pc,
                 (int)f->error->math.status, (int)f->error->math.initial_cause, (int)state.phase,
                 state.last_store_pc);
    return 0;
}

static int null_world(fixture *f, mc_client_render_manager *manager, mc_entity *actor,
                      mc_math_classes **ready) {
    mc_math_class_state state;
    scalar_inputs(manager);
    CHECK(mc_math_classes_create(SIZE_MAX, ready, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_math_classes_state(*ready, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_COLD && state.source_a == NULL);
    CHECK(mc_client_render_manager_bind_frame(manager, *ready, NULL, NULL, NULL, actor, NULL, 0.5f,
                                              f->error) == MC_CLIENT_MANAGER_NULL_REFERENCE &&
          f->error->frame_pc == 60 && !f->error->native_policy && manager->last_frame_pc == 29);
    CHECK(mc_math_classes_state(*ready, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_READY && state.last_store_pc == 37 &&
          state.source_a == mc_math_classes_table(*ready));
    CHECK(unchanged_scalars(manager) == 0 && five_references(manager, NULL, actor) == 0);
    sleeping_row("null_world", "%d %d %d %u %d", (int)f->error->status, f->error->frame_pc,
                 (int)state.phase, state.last_store_pc, manager->last_frame_pc);
    return 0;
}

typedef struct sleeping_world {
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_runtime_classes *classes;
    mc_world_info_name *name;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_chunk_state *chunk;
} sleeping_world;

static int open_world(sleeping_world *w, fixture *f, mc_math_classes *math, const char *root) {
    CHECK(mc_world_regions_create(NULL, &w->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options options = {.session_token = 6381,
                                            .block_map = map,
                                            .block_map_length = sizeof(map),
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(root, w->regions, &options, &w->store, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile,
                                          mc_math_classes_table(math), &w->classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(w->classes, f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(w->classes, f->entities) == MC_ENTITY_OK);
    static const uint16_t name[] = {'S', 'l', 'e', 'e', 'p'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){name, 5}, SIZE_MAX, &w->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &w->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, w->classes,
                                          &limits, &w->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = w->store, .name = w->name, .seed = 23, .dimension = w->dimension};
    CHECK(mc_world_runtime_construct_world(w->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_runtime_construction construction;
    CHECK(mc_world_runtime_construction_get(w->world, &construction) == MC_WORLD_STORAGE_OK &&
          construction.mode == MC_WORLD_RUNTIME_GAME &&
          construction.phase == MC_WORLD_RUNTIME_READY && construction.contexts_bound &&
          construction.cache_registered);
    /* The real store reads the authored populated region, then the genuine
     * provider activates and publishes this exact chunk. Absence is not air. */
    CHECK(mc_world_runtime_get(w->world, 0, 0, &w->chunk) == MC_CHUNK_ACQUIRE_OK &&
          w->chunk != NULL && !w->chunk->blank && w->chunk->active && w->chunk->terrain_populated &&
          w->chunk->data != NULL && w->chunk->data->blocks != NULL &&
          w->chunk->data->blocks_capacity == MC_CHUNK_BLOCKS);
    const mc_chunk_acquire_context *context = mc_world_runtime_acquisition(w->world);
    CHECK(context != NULL && context->cache->count == 1 && context->cache->capacity == 1 &&
          mc_world_store_chunk_count(w->store) == 1);
    return 0;
}

static int close_world(sleeping_world *w) {
    const mc_chunk_acquire_context *context = mc_world_runtime_acquisition(w->world);
    bool added = false, result = true;
    /* Native retirement input through the actual pending-set API. No cache,
     * chunk active flag, counts, source object or constructor phase is reset. */
    CHECK(context != NULL &&
          mc_chunk_pending_add(context->pending, mc_chunk_cache_key(0, 0), &added) ==
              MC_CHUNK_ACQUIRE_OK &&
          added);
    CHECK(mc_world_runtime_maintain(w->world, &result) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(mc_world_store_retire_chunk(w->store, w->chunk, NULL) == MC_WORLD_STORAGE_OK);
    w->chunk = NULL;
    CHECK(mc_world_runtime_retire_entity_containers(w->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_unbind(w->dimension);
    CHECK(mc_world_runtime_retire_world(w->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_chunk_count(w->store) == 0);
    CHECK(mc_world_runtime_free(w->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(w->dimension);
    CHECK(mc_world_store_free_checked(&w->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(w->regions);
    mc_world_info_name_release(w->name);
    CHECK(mc_world_runtime_classes_free(w->classes, NULL) == MC_WORLD_STORAGE_OK);
    return 0;
}

static int sleeping_branches(fixture *f, mc_client_render_manager *manager, mc_entity *actor,
                             mc_math_classes *math, sleeping_world *w) {
    const mc_block *bed = NULL;
    CHECK(mc_registry_block_static_get(f->registry, "T", &bed, &f->error->registry) ==
              MC_REGISTRY_OK &&
          bed != NULL);
    const int32_t bed_id = mc_block_fields_get(bed)->bn;
    CHECK(bed_id > 0 && bed_id < 256);
    const size_t at = (8u * 16u + 8u) * 128u + 64u;
    /* Declared caller-owned mutable data input on a genuinely loaded chunk;
     * no placement hook or full gameplay Bed construction is being claimed. */
    w->chunk->data->blocks[at] = (uint8_t)bed_id;
    const float yaws[] = {180.0f, 270.0f, 360.0f, 450.0f};
    for (int32_t orientation = 0; orientation < 4; ++orientation) {
        bool changed = false;
        CHECK(mc_world_runtime_metadata_set_quiet(w->world, 8, 64, 8, orientation, &changed,
                                                  NULL) == MC_WORLD_STORAGE_OK &&
              changed);
        scalar_inputs(manager);
        CHECK(mc_client_render_manager_bind_frame(manager, math, w->world, NULL, NULL, actor, NULL,
                                                  0.5f, f->error) == MC_CLIENT_MANAGER_OK &&
              f->error->frame_pc == 257 && manager->last_frame_pc == 257);
        CHECK(five_references(manager, w->world, actor) == 0);
        CHECK(bits32(manager->fields.i) == bits32(yaws[orientation]) &&
              bits32(manager->fields.j) == 0 && bits64(manager->fields.l) == bits64(7.5) &&
              bits64(manager->fields.m) == bits64(63.0) &&
              bits64(manager->fields.n) == bits64(6.5));
        char name[16];
        const int count = snprintf(name, sizeof(name), "bed%d", (int)orientation);
        CHECK(count > 0 && (size_t)count < sizeof(name));
        sleeping_row(
            name, "%d %d %d %08" PRIx32 " %08" PRIx32 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64,
            bed_id, (int)orientation, manager->last_frame_pc, bits32(manager->fields.i),
            bits32(manager->fields.j), bits64(manager->fields.l), bits64(manager->fields.m),
            bits64(manager->fields.n));
    }
    w->chunk->data->blocks[at] = 0;
    scalar_inputs(manager);
    CHECK(mc_client_render_manager_bind_frame(manager, math, w->world, NULL, NULL, actor, NULL,
                                              0.5f, f->error) == MC_CLIENT_MANAGER_OK &&
          bits32(manager->fields.i) == bits32(13.25f) &&
          bits32(manager->fields.j) == bits32(-9.5f) && bits64(manager->fields.l) == bits64(7.5) &&
          bits64(manager->fields.m) == bits64(63.0) && bits64(manager->fields.n) == bits64(6.5));
    sleeping_row("nonbed",
                 "%d %08" PRIx32 " %08" PRIx32 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64,
                 manager->last_frame_pc, bits32(manager->fields.i), bits32(manager->fields.j),
                 bits64(manager->fields.l), bits64(manager->fields.m), bits64(manager->fields.n));

    actor->base.x = 128.5;
    actor->base.z = 128.5;
    scalar_inputs(manager);
    CHECK(mc_client_render_manager_bind_frame(manager, math, w->world, NULL, NULL, actor, NULL,
                                              0.5f, f->error) == MC_CLIENT_MANAGER_WORLD_FAILED &&
          f->error->frame_pc == 60 && f->error->native_policy && manager->last_frame_pc == 29 &&
          f->error->acquisition != NULL &&
          f->error->acquisition->status == MC_CHUNK_ACQUIRE_LIMIT &&
          f->error->acquisition->cause.status == MC_WORLD_STORAGE_OK &&
          f->error->world.status == MC_WORLD_STORAGE_LIMIT);
    CHECK(unchanged_scalars(manager) == 0 && five_references(manager, w->world, actor) == 0);
    /* This is the actual complete child borrowed from the ONE reached block
     * getter, inspected before another manager mutation can invalidate it. */
    sleeping_row(
        "world60_failure", "%d %d %d %d %d %u %u %u %u", (int)f->error->status, f->error->frame_pc,
        (int)f->error->world.status, (int)f->error->acquisition->status,
        (int)f->error->acquisition->cause.status, f->error->acquisition->activation_bytecode,
        f->error->acquisition->activation_world_bytecode, f->error->acquisition->load_bytecode,
        f->error->acquisition->load_builder_bytecode);
    mc_world_runtime_acquisition_error *child = calloc(1, sizeof(*child));
    CHECK(child != NULL);
    mc_world_storage_error error = {0};
    int32_t value = -12345;
    const mc_world_storage_status metadata =
        mc_world_runtime_metadata_get_detailed(w->world, 128, 64, 128, &value, &error, child);
    CHECK(metadata == MC_WORLD_STORAGE_LIMIT && value == -12345 &&
          child->status == MC_CHUNK_ACQUIRE_LIMIT && child->cause.status == MC_WORLD_STORAGE_OK);
    sleeping_row("metadata_failure", "%d %d %d %d", (int)metadata, (int)child->status,
                 (int)child->cause.status, value);
    free(child);
    CHECK(mc_client_render_manager_detach_world(manager, w->world, f->error) ==
              MC_CLIENT_MANAGER_OK &&
          manager->fields.g == NULL);
    return 0;
}

typedef struct settings_refusal {
    fixture *f;
    mc_game_settings *settings;
    size_t calls;
    bool checked;
} settings_refusal;

static mc_game_settings_status active_retirement(void *context,
                                                 const mc_game_settings_diagnostic *event,
                                                 mc_game_settings_error *error) {
    settings_refusal *o = context;
    ++o->calls;
    mc_client_render_manager_classes *same = o->f->classes;
    const mc_client_manager_status status =
        mc_client_render_manager_classes_retire(&o->f->classes, o->f->error);
    mc_client_manager_state state;
    mc_game_settings_state settings;
    o->checked =
        event->kind == MC_GAME_SETTINGS_SAVE_FAILED &&
        event->exception == MC_GAME_SETTINGS_SOURCE_NULL_FILE &&
        status == MC_CLIENT_MANAGER_SETTINGS_FAILED && o->f->classes == same &&
        o->f->error->settings.status == MC_GAME_SETTINGS_BUSY &&
        mc_client_render_manager_inspect(o->f->classes, &state) &&
        state.phase == MC_CLIENT_MANAGER_RETIRING && !state.source_views_available &&
        state.retained_fonts == 0 && state.retained_settings == 1 && state.retained_engines == 1 &&
        mc_game_settings_read_state(o->settings, &settings) && settings.native_borrows == 1;
    /* Deliberate diagnostic refusal, never successful substitute I/O. Legacy
     * default Settings exposes active at release, so actual font-first cleanup
     * has already completed when the Settings BUSY result retains a retry. */
    if (error != NULL)
        *error = (mc_game_settings_error){MC_GAME_SETTINGS_DIAGNOSTIC_FAILED, 0,
                                          "authored TH Settings active diagnostic refusal"};
    return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
}

static int lifetime_controls(const char *resources) {
    fixture f;
    mc_client_render_manager *manager = NULL;
    CHECK(complete_manager(&f, resources, &manager) == 0);
    mc_client_render_engine_classes *engines = NULL;
    mc_client_default_font *default_font = NULL;
    mc_client_render_engine *engine = NULL;
    mc_client_loading_classes *gui = NULL;
    mc_client_bitmap_font *font = NULL;
    mc_game_settings *settings = NULL;
    mc_client_bitmap_font_state *font_state = calloc(1, sizeof(*font_state));
    CHECK(font_state != NULL);
    CHECK(mc_client_render_engine_classes_create(NULL, &engines) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_default_font_create(0, &default_font) == MC_CLIENT_FONT_OK &&
          mc_client_render_engine_allocate(engines, default_font, NULL, &engine) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_loading_classes_create(engines, &gui) == MC_CLIENT_LOADING_OK &&
          mc_client_bitmap_font_allocate(gui, NULL, &font) == MC_CLIENT_BITMAP_FONT_OK &&
          mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_render_manager_bind_frame(manager, NULL, NULL, engine, font, NULL, settings,
                                              0.5f, f.error) == MC_CLIENT_MANAGER_NULL_REFERENCE &&
          f.error->frame_pc == 29 && manager->fields.e == engine && manager->fields.p == font &&
          manager->fields.k == settings && manager->fields.g == NULL && manager->fields.h == NULL);
    mc_client_manager_state state;
    mc_client_render_engine_state engine_state;
    mc_game_settings_state settings_state;
    CHECK(mc_client_render_manager_inspect(f.classes, &state) && state.retained_fonts == 1 &&
          state.retained_engines == 1 && state.retained_settings == 1 &&
          mc_client_render_engine_inspect(engine, &engine_state) &&
          engine_state.native_holds == 2 && engine_state.phase == MC_CLIENT_RENDER_ENGINE_RAW &&
          mc_client_bitmap_font_inspect(font, font_state) && font_state->holds == 2 &&
          font_state->phase == MC_CLIENT_BITMAP_FONT_RAW &&
          mc_game_settings_read_state(settings, &settings_state) &&
          settings_state.native_borrows == 1);
    mc_client_render_engine *engine_slot = engine;
    mc_client_bitmap_font *font_slot = font;
    mc_game_settings *settings_slot = settings;
    CHECK(mc_client_render_engine_retire(&engine_slot, NULL) == MC_CLIENT_RENDER_ENGINE_BUSY &&
          engine_slot == engine &&
          mc_client_bitmap_font_retire(&font_slot, NULL) == MC_CLIENT_BITMAP_FONT_BUSY &&
          font_slot == font &&
          mc_game_settings_retire(&settings_slot, NULL) == MC_GAME_SETTINGS_BUSY &&
          settings_slot == settings);
    sleeping_row("lifetime_holds", "%zu %zu %zu %zu %zu %zu", state.retained_engines,
                 state.retained_fonts, state.retained_settings, engine_state.native_holds,
                 font_state->holds, settings_state.native_borrows);
    CHECK(mc_client_render_manager_bind_frame(manager, NULL, NULL, NULL, NULL, NULL, NULL, 0.5f,
                                              f.error) == MC_CLIENT_MANAGER_NULL_REFERENCE &&
          f.error->frame_pc == 29 && manager->fields.e == NULL && manager->fields.p == NULL &&
          manager->fields.k == NULL);
    CHECK(mc_client_render_manager_inspect(f.classes, &state) && state.retained_engines == 1 &&
          state.retained_fonts == 1 && state.retained_settings == 1);
    CHECK(mc_client_render_engine_retire(&engine_slot, NULL) == MC_CLIENT_RENDER_ENGINE_BUSY &&
          engine_slot == engine &&
          mc_client_bitmap_font_retire(&font_slot, NULL) == MC_CLIENT_BITMAP_FONT_BUSY &&
          font_slot == font &&
          mc_game_settings_retire(&settings_slot, NULL) == MC_GAME_SETTINGS_BUSY &&
          settings_slot == settings);
    sleeping_row("lifetime_displaced", "%zu %zu %zu", state.retained_engines, state.retained_fonts,
                 state.retained_settings);
    settings_refusal observation = {.f = &f, .settings = settings};
    const mc_game_settings_diagnostics sink = {&observation, active_retirement};
    mc_game_settings_report report = {0};
    mc_game_settings_error error = {0};
    CHECK(mc_game_settings_save(settings, &sink, &report, &error) ==
              MC_GAME_SETTINGS_DIAGNOSTIC_FAILED &&
          observation.calls == 1 && observation.checked && report.outer_exception_caught &&
          report.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_FILE);
    CHECK(mc_client_render_manager_inspect(f.classes, &state) &&
          state.phase == MC_CLIENT_MANAGER_RETIRING && state.retained_fonts == 0 &&
          state.retained_settings == 1 && state.retained_engines == 1 &&
          mc_client_bitmap_font_inspect(font, font_state) && font_state->holds == 1 &&
          mc_client_render_engine_inspect(engine, &engine_state) && engine_state.native_holds == 2);
    sleeping_row("settings_active_refusal", "%zu %d %d %zu %zu %zu", observation.calls,
                 (int)f.error->status, (int)f.error->settings.status, state.retained_engines,
                 state.retained_fonts, state.retained_settings);
    CHECK(mc_client_render_manager_classes_retire(&f.classes, f.error) == MC_CLIENT_MANAGER_OK &&
          f.classes == NULL && mc_client_bitmap_font_inspect(font, font_state) &&
          font_state->holds == 1 && mc_client_render_engine_inspect(engine, &engine_state) &&
          engine_state.native_holds == 1 &&
          mc_game_settings_read_state(settings, &settings_state) &&
          settings_state.native_borrows == 0);
    sleeping_row("lifetime_released", "%zu %zu %zu", engine_state.native_holds, font_state->holds,
                 settings_state.native_borrows);
    CHECK(mc_client_bitmap_font_retire(&font, NULL) == MC_CLIENT_BITMAP_FONT_OK && font == NULL);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK && settings == NULL);
    CHECK(mc_client_loading_classes_release(gui) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_render_engine_retire(&engine, NULL) == MC_CLIENT_RENDER_ENGINE_OK &&
          engine == NULL);
    CHECK(mc_client_render_engine_classes_release(engines) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_release(default_font) == MC_CLIENT_FONT_OK);
    free(font_state);
    CHECK(close_fixture(&f) == 0);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s RESOURCE_ROOT OWNED_AUTHORED_WORLD\n", argv[0]);
        return 2;
    }
    const unsigned incoming_fp = _mm_getcsr();
    _mm_setcsr(0x1f80u);
    /* Complete the entire failed-Math application namespace before opening a
     * second genuine TH/Models/Registry/Entity namespace with its own Math.
     * The failed source class is never replaced to let that TH make progress. */
    fixture negative;
    mc_client_render_manager *negative_manager = NULL;
    CHECK(complete_manager(&negative, argv[1], &negative_manager) == 0);
    sleeping_row("complete28", "%zu %zu %d", negative_manager->put_count,
                 negative_manager->bw_count, negative_manager->last_constructor_pc);
    mc_entity *negative_actor = NULL;
    CHECK(mc_local_player_allocate(negative.entities, &negative_actor, NULL) == MC_ENTITY_OK &&
          negative_actor != NULL && negative_actor->local_player != NULL &&
          negative_actor->phase == MC_ENTITY_RAW);
    pose(negative_actor);
    mc_math_classes *failed_namespace_math = NULL;
    CHECK(failed_math(&negative, negative_manager, negative_actor, &failed_namespace_math) == 0);
    mc_entity_release(negative_actor);
    CHECK(close_fixture(&negative) == 0);
    CHECK(mc_math_classes_release(&failed_namespace_math, NULL) == MC_MATH_CLASS_OK &&
          failed_namespace_math == NULL);

    fixture f;
    mc_client_render_manager *manager = NULL;
    CHECK(complete_manager(&f, argv[1], &manager) == 0);
    mc_entity *actor = NULL;
    CHECK(mc_local_player_allocate(f.entities, &actor, NULL) == MC_ENTITY_OK && actor != NULL &&
          actor->local_player != NULL && actor->phase == MC_ENTITY_RAW);
    pose(actor);
    bool sleeping = false;
    CHECK(mc_player_is_sleeping(actor, &sleeping, NULL) == MC_PLAYER_OK && sleeping);
    mc_math_classes *math = NULL;
    CHECK(null_world(&f, manager, actor, &math) == 0);
    sleeping_world world = {0};
    CHECK(open_world(&world, &f, math, argv[2]) == 0);
    CHECK(sleeping_branches(&f, manager, actor, math, &world) == 0);
    CHECK(close_world(&world) == 0);
    mc_entity_release(actor);
    /* GAME WorldClasses joined this Registry to the SAME Entity namespace.
     * End its independent Registry->Entity hold before final Entity teardown. */
    CHECK(mc_client_render_manager_classes_retire(&f.classes, f.error) == MC_CLIENT_MANAGER_OK &&
          f.classes == NULL);
    CHECK(mc_client_render_classes_free(f.models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_registry_classes_free(f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f.entities) == MC_ENTITY_OK);
    CHECK(mc_resource_files_free(f.resources) == MC_REGISTRY_OK);
    mc_materials_destroy(f.materials);
    mc_java_random_profile_free(f.profile);
    free(f.error);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK && math == NULL);
    CHECK(lifetime_controls(argv[1]) == 0);
    _mm_setcsr(incoming_fp);
    CHECK(sleeping_rows == 15);
    printf("Native TH sleeping controls: %u rows; %u checks; cleanup complete\n", sleeping_rows,
           checks);
    return 0;
}
