#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Genuine prepared owners; the included main is never executed. */
#define MC_PICKING_BODY_MAIN_NAME mc_lever_picking_fixture_main
int mc_lever_picking_fixture_main(int argc, char **argv);
#include "../world/test_picking.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/render/blocks/internal.h"
#include "client/render/model_graph_internal.h"
#include "client/render_engine/nw.h"
#include "client/startup/application_owner.h"
#include "mc/client/settings/game_settings.h"
#include "world/chunk/object_state_internal.h"

static uint64_t lever_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool lever_prepare(picking_fixture *f, int32_t metadata) {
    mc_chunk_state *chunk = f->chunks[0];
    memset(chunk->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(chunk->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->skylight, 0xff, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    chunk->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = 69;
    CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8,
                               (uint8_t)metadata));
    return true;
}

static bool lever_controls(picking_fixture *f) {
    mc_client_application_classes *application = NULL;
    CHECK(mc_client_application_classes_create(NULL, &application, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_classes_active_use(application, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_lifetime_register(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_namespace_attach(application, f->app, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    f->app->constructor_phase = MC_CLIENT_APP_CONSTRUCTING;
    CHECK(mc_client_application_classes_publish(application, f->app, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    mc_game_settings *settings = NULL;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state settings_state;
    CHECK(mc_game_settings_read_state(settings, &settings_state));
    settings_state.values.ambient_occlusion = false;
    CHECK(mc_game_settings_set_values(settings, &settings_state.values, NULL) ==
          MC_GAME_SETTINGS_OK);
    f->app->settings = settings;
    f->app->source_math_classes = f->math;
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error cache_error;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &cache_error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(f->models, &model_error));
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->base.registry, 69, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && block->kind == MC_BLOCK_LEVER);
    const mc_block_fields original_bounds = block->fields;
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(
        f->graphics, (mc_client_graphics_site){"native-test", "lever", "()V", 0}, &nw, &graphics));
    CHECK(mc_client_nw_begin_quads(nw, &graphics));
    f->base.world->fields.f = 3;
    unsigned int rows = 0;
    for (int32_t metadata = 0; metadata < 16; ++metadata)
        for (unsigned int override = 0; override < 2; ++override) {
            CHECK(lever_prepare(f, metadata));
            mc_client_render_blocks fields;
            mc_client_render_blocks_construct_fields(&fields, cache, true);
            fields.f = true;
            fields.d = override ? 17 : -1;
            mc_cv_context context = {.fields = &fields,
                                     .cache = cache,
                                     .registry = f->base.registry,
                                     .models = f->models,
                                     .graphics = f->graphics,
                                     .application = f->app};
            mc_client_nw_state state;
            mc_entity_vector_snapshot before, after;
            CHECK(mc_client_nw_inspect(f->graphics, &state) &&
                  mc_entity_vector_get_snapshot(f->base.entities, &before) == MC_ENTITY_OK);
            const int32_t first = state.fields.o;
            bool rendered = false;
            CHECK(mc_cv_render(&context, block, 8, 64, 8, &rendered) && rendered &&
                  !context.failed && context.returned && context.depth == 0 &&
                  context.source_pc == 251 && fields.d == (override ? 17 : -1) && !fields.m);
            CHECK(mc_client_nw_inspect(f->graphics, &state) &&
                  mc_entity_vector_get_snapshot(f->base.entities, &after) == MC_ENTITY_OK &&
                  after.cursor == before.cursor + 8 && after.holds == before.holds &&
                  state.fields.o == first + 72 * 8 && state.fields.w);
            printf("LEVER\t%" PRId32 "\t%d\ttrue\t%" PRId32 "\tfalse\t8\t%016" PRIx64 ",%016" PRIx64
                   ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 "\t",
                   metadata, override ? 17 : -1, fields.d, lever_bits(block->fields.bs),
                   lever_bits(block->fields.bt), lever_bits(block->fields.bu),
                   lever_bits(block->fields.bv), lever_bits(block->fields.bw),
                   lever_bits(block->fields.bx));
            for (int32_t i = first; i < state.fields.o; ++i)
                printf("%s%08" PRIx32, i == first ? "" : ",", (uint32_t)state.fields.g[i]);
            putchar('\n');
            ++rows;
        }
    CHECK(rows == 32);
    /* A reached base failure retains its current override and bound stores.
     * A missing Cobblestone fails before either store; an existing override
     * bypasses that static field and still reaches the complete geometry. */
    mc_block *const cobblestone = f->base.registry->blocks.named[3];
    f->base.registry->blocks.named[3] = NULL;
    mc_client_render_blocks fields;
    mc_client_render_blocks_construct_fields(&fields, cache, true);
    mc_cv_context failure = {.fields = &fields,
                             .cache = cache,
                             .registry = f->base.registry,
                             .models = f->models,
                             .graphics = f->graphics,
                             .application = f->app};
    bool sentinel = false;
    CHECK(mc_cv_push(&failure, "e", "(Luu;III)Z"));
    CHECK(!mc_cv_lever(&failure, block, 8, 64, 8, &sentinel) && !sentinel && failure.failed &&
          fields.d == -1 && failure.source_pc == 65 &&
          failure.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE);
    fields.d = 17;
    failure = (mc_cv_context){.fields = &fields,
                              .cache = cache,
                              .registry = f->base.registry,
                              .models = f->models,
                              .graphics = f->graphics,
                              .application = f->app};
    CHECK(mc_cv_push(&failure, "e", "(Luu;III)Z") &&
          mc_cv_lever(&failure, block, 8, 64, 8, &sentinel) && sentinel && !failure.failed &&
          fields.d == 17);
    f->base.registry->blocks.named[3] = cobblestone;
    CHECK(lever_prepare(f, 5));
    fields.d = -1;
    failure = (mc_cv_context){.fields = &fields,
                              .cache = cache,
                              .registry = f->base.registry,
                              .models = f->models,
                              .graphics = f->graphics};
    mc_client_nw_state before_failure, after_failure;
    mc_entity_vector_snapshot before_pool, after_pool;
    CHECK(mc_client_nw_inspect(f->graphics, &before_failure) &&
          mc_entity_vector_get_snapshot(f->base.entities, &before_pool) == MC_ENTITY_OK);
    sentinel = false;
    CHECK(mc_cv_push(&failure, "e", "(Luu;III)Z") &&
          !mc_cv_lever(&failure, block, 8, 64, 8, &sentinel) && !sentinel && failure.failed &&
          fields.d == 16 && block->fields.bs == .3125 && block->fields.bt == 0 &&
          block->fields.bu == .25 && block->fields.bv == .6875 && block->fields.bw == .1875 &&
          block->fields.bx == .75 && failure.depth == 2 && failure.frames[0].pc == 307 &&
          failure.frames[1].pc == 128 && failure.source_pc == 128 &&
          failure.failure.status == MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE &&
          mc_client_nw_inspect(f->graphics, &after_failure) &&
          after_failure.fields.o == before_failure.fields.o &&
          mc_entity_vector_get_snapshot(f->base.entities, &after_pool) == MC_ENTITY_OK &&
          after_pool.cursor == before_pool.cursor && after_pool.holds == before_pool.holds);
    /* Keep three genuine reusable slots, then refuse the fourth factory.
     * This is native allocation admission, not Java OOM timing. The reached
     * base quads/cursor stay committed while all three native holds retire. */
    CHECK(mc_entity_vector_pool_clear_client(f->base.entities, NULL) == MC_ENTITY_OK);
    for (unsigned int i = 0; i < 3; ++i) {
        mc_entity_vector *slot = NULL;
        CHECK(mc_entity_vector_pool_get(f->base.entities, 1, 2, 3, &slot, NULL) == MC_ENTITY_OK &&
              mc_entity_vector_release(f->base.entities, slot) == MC_ENTITY_OK);
    }
    CHECK(mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_get_snapshot(f->base.entities, &before_pool) == MC_ENTITY_OK &&
          before_pool.size == 3 && before_pool.cursor == 0 && before_pool.holds == 0);
    const size_t allocation_limit = f->base.entities->limits.max_allocation_bytes;
    const size_t allocated = f->base.entities->allocated_bytes;
    f->base.entities->limits.max_allocation_bytes = allocated;
    fields.d = -1;
    fields.f = true;
    failure = (mc_cv_context){.fields = &fields,
                              .cache = cache,
                              .registry = f->base.registry,
                              .models = f->models,
                              .graphics = f->graphics,
                              .application = f->app};
    CHECK(mc_client_nw_inspect(f->graphics, &before_failure));
    CHECK(mc_cv_push(&failure, "e", "(Luu;III)Z"));
    const bool completed = mc_cv_lever(&failure, block, 8, 64, 8, &sentinel);
    f->base.entities->limits.max_allocation_bytes = allocation_limit;
    CHECK(!completed && !sentinel && failure.failed && failure.depth == 1 &&
          failure.source_pc == 510 && failure.failure.status == MC_CLIENT_RENDER_GLOBAL_LIMIT &&
          failure.failure.native_policy && failure.last_store_pc == 318 && fields.d == -1 &&
          !fields.m &&
          mc_entity_vector_get_snapshot(f->base.entities, &after_pool) == MC_ENTITY_OK &&
          after_pool.cursor == 3 && after_pool.size == 3 && after_pool.holds == 0 &&
          after_pool.capacity == before_pool.capacity &&
          f->base.entities->allocated_bytes == allocated &&
          mc_client_nw_inspect(f->graphics, &after_failure) &&
          after_failure.fields.o == before_failure.fields.o + 36 * 8);
    for (unsigned int i = 0; i < 3; ++i) {
        const mc_entity_vector *slot = f->base.entities->vector_pool->elements[i];
        CHECK(slot->x == (i == 0 ? -.0625 : .0625) && slot->y == 0 &&
              slot->z == (i == 2 ? .0625 : -.0625));
    }
    ((mc_block *)block)->fields = original_bounds;
    f->app->constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(mc_client_application_namespace_detach(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_classes_release(&application, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_lifetime_retire(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    f->app->settings = NULL;
    f->app->source_math_classes = NULL;
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !lever_controls(f) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf("Lever World: %u checks; 32 original geometry rows and retained failure controls\n",
           checks);
    return 0;
}
