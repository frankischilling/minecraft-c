/* Genuine saved World/Registry/Chunk helpers; earlier main is not executed. */
#if defined(_MSC_VER)
/* Profiles contain bounded numeric conversions only. */
#define _CRT_SECURE_NO_WARNINGS
#endif
#define MC_PICKING_BODY_MAIN_NAME mc_ao_picking_fixture_main
int mc_ao_picking_fixture_main(int argc, char **argv);
#include "../world/test_picking.c"
#undef main

#include "client/render/blocks/internal.h"
#include "client/render/model_graph_internal.h"
#include "client/render_engine/nw.h"
#include "client/startup/application_owner.h"
#include "mc/client/settings/game_settings.h"
#include "world/chunk/object_state_internal.h"

static uint32_t ao_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool ao_prepare(mc_chunk_state *chunk, int32_t id, int pattern) {
    memset(chunk->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(chunk->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    for (int32_t x = 7; x <= 9; ++x)
        for (int32_t y = 63; y <= 65; ++y)
            for (int32_t z = 7; z <= 9; ++z) {
                const int block = pattern == 0   ? 0
                                  : pattern == 1 ? 1
                                                 : ((x + 2 * y + 4 * z) & 2) != 0;
                const size_t index = ((uint32_t)x << 11u) | ((uint32_t)z << 7u) | (uint32_t)y;
                chunk->data->blocks[index] = (uint8_t)block;
                CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           (x + y + z) & 3));
                CHECK(mc_chunk_nibbles_set(chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           (x * 3 + y * 5 + z * 7) & 15));
                CHECK(mc_chunk_nibbles_set(chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           (x * 7 + y * 11 + z * 13) & 15));
            }
    chunk->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = (uint8_t)id;
    return true;
}

static bool ao_observe(unsigned int index, const int profile[7], const mc_client_render_blocks *b,
                       int result, const char *exception, int32_t before,
                       const mc_client_nw_state *t, bool chunk_light) {
    CHECK(printf("AO %u", index) >= 0);
    for (size_t i = 0; i < 7; ++i)
        CHECK(printf(" %d", profile[i]) >= 0);
    CHECK(printf(" %d %s %d", result, exception, b->m) >= 0);
    for (size_t i = 0; i < 27; ++i)
        CHECK(printf(" %08" PRIx32, ao_bits(b->n_to_N[i])) >= 0);
    unsigned int flags = 0;
    for (unsigned int i = 0; i < 12; ++i)
        if (b->ab_to_am[i])
            flags |= 1u << i;
    CHECK(printf(" %u", flags) >= 0);
    for (size_t i = 0; i < 12; ++i)
        CHECK(printf(" %08" PRIx32, ao_bits(b->P_to_aa[i])) >= 0);
    const int features = (t->fields.l ? 1 : 0) | (t->fields.m ? 2 : 0) | (t->fields.n ? 4 : 0);
    CHECK(printf(" %" PRId32 " %" PRId32 " %" PRId32 " %08" PRIx32 " %d %d %" PRId32, t->fields.h,
                 t->fields.o, t->fields.p, (uint32_t)t->fields.k, features, chunk_light,
                 t->fields.o - before) >= 0);
    for (int32_t i = before; i < t->fields.o; ++i)
        CHECK(printf(" %08" PRIx32, (uint32_t)t->fields.g[i]) >= 0);
    CHECK(putchar('\n') != EOF);
    return true;
}

static bool ao_controls(picking_fixture *f, const char *profiles) {
    mc_client_chunk_cache *cache = NULL, *edge_cache = NULL;
    mc_client_chunk_cache_error cache_error;
    const size_t bindings = f->base.world->persistent_bindings;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &cache_error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 15, 127, 15, &edge_cache,
                                       &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(f->models, &model_error));
    const bool prior_overlay = f->models->cv_a;
    mc_client_application_classes *application_classes = NULL;
    CHECK(mc_client_application_classes_create(NULL, &application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_classes_active_use(application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_lifetime_register(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_namespace_attach(application_classes, f->app, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    f->app->constructor_phase = MC_CLIENT_APP_CONSTRUCTING;
    CHECK(mc_client_application_classes_publish(application_classes, f->app, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    mc_game_settings *settings = NULL;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state settings_state;
    CHECK(mc_game_settings_read_state(settings, &settings_state));
    settings_state.values.ambient_occlusion = true;
    CHECK(mc_game_settings_set_values(settings, &settings_state.values, NULL) ==
          MC_GAME_SETTINGS_OK);
    f->app->settings = settings;
    mc_client_color_array *palette = NULL;
    CHECK(mc_client_color_array_create(
              65536, NULL, (mc_client_graphics_site){"native-test", "palette", "()V", -1}, &palette,
              NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    for (size_t i = 0; i < 65536; ++i)
        CHECK(mc_client_color_array_write(palette, i, UINT32_C(0x003569a7)) ==
              MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_colorizer_set(f->graphics, MC_CLIENT_COLORIZER_GRASS, palette, NULL, NULL) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(mc_client_color_array_release(&palette, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    mc_client_tessellator *tessellator = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f->graphics,
                                  (mc_client_graphics_site){"native-test", "AO", "()V", -1},
                                  &tessellator, &graphics));
    CHECK(mc_client_nw_begin_quads(tessellator, &graphics));
    f->base.world->fields.f = 3;
    FILE *input = fopen(profiles, "r");
    CHECK(input != NULL);
    int p[7], scanned;
    unsigned int index = 0;
    while ((scanned = fscanf(input, "%d%d%d%d%d%d%d", &p[0], &p[1], &p[2], &p[3], &p[4], &p[5],
                             &p[6])) == 7) {
        CHECK((p[0] == 1 || p[0] == 2) && p[1] >= -1 && p[1] <= 2 && p[2] >= 0 && p[2] <= 2 &&
              (p[3] == 0 || p[3] == 1) && (p[4] == -1 || p[4] == 17) && (p[5] == 0 || p[5] == 1) &&
              p[6] >= 0 && p[6] <= 4);
        CHECK(ao_prepare(f->chunks[0], p[0], p[2]));
        if (p[6] == 4)
            f->chunks[0]->data->blocks[(8u << 7u) | 64u] = (uint8_t)p[0];
        mc_client_render_blocks fields;
        mc_client_chunk_cache *selected_cache = p[6] == 4 ? edge_cache : cache;
        mc_client_render_blocks_construct_fields(&fields, p[6] == 1 ? NULL : selected_cache, true);
        fields.O = p[1];
        fields.f = p[3] != 0;
        fields.d = p[4];
        f->models->cv_a = p[5] != 0;
        CHECK(mc_client_entity_renderer_class_active_use(f->models, NULL) ==
              MC_CLIENT_ENTITY_RENDERER_OK);
        f->models->px_a = p[6] == 3;
        for (size_t i = 0; i < 27; ++i)
            fields.n_to_N[i] = (float)(i + 1) * 0.03125f;
        for (size_t i = 0; i < 12; ++i) {
            fields.P_to_aa[i] = (float)(i + 1) * 0.015625f;
            fields.ab_to_am[i] = (i & 1u) != 0;
        }
        const mc_block *block;
        CHECK(mc_registry_block_get(f->base.registry, p[0], &block, NULL) == MC_REGISTRY_OK &&
              block != NULL);
        mc_client_nw_state state;
        CHECK(mc_client_nw_inspect(f->graphics, &state));
        const int32_t before = state.fields.o;
        f->base.world_classes->chunk_a = false;
        mc_cv_context c = {.fields = &fields,
                           .cache = selected_cache,
                           .registry = f->base.registry,
                           .models = f->models,
                           .graphics = f->graphics,
                           .application = f->app,
                           .tessellator = tessellator};
        bool rendered = true; /* Failure preserves this unpublished result. */
        bool completed;
        const bool full_entry = p[6] == 2 || p[6] == 3;
        if (full_entry) {
            completed = mc_cv_render(&c, block, 8, 64, 8, &rendered);
        } else {
            CHECK(mc_cv_push(&c, "a", "(Luu;IIIFFF)Z"));
            completed = mc_source_cv_ambient_occlusion(&c, block, p[6] == 4 ? 0 : 8, 64, 8, 0.3f,
                                                       0.6f, 0.9f, &rendered);
        }
        const char *exception = "NONE";
        if (!completed && p[6] == 1) {
            CHECK(c.failed && rendered && fields.m && c.last_store_pc == 2 && c.depth >= 2 &&
                  c.frames[0].pc == 60 &&
                  c.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
                  !c.failure.native_policy);
            exception = "NULL";
        } else if (!completed) {
            CHECK(p[6] == 4 && c.failed && rendered && fields.m && c.last_store_pc == 63 &&
                  c.depth >= 2 && c.frames[0].pc == 78 && !c.failure.native_policy &&
                  c.cache_error.status == MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX &&
                  ao_bits(fields.n_to_N[0]) == UINT32_C(0x3d4ccccd) &&
                  ao_bits(fields.n_to_N[1]) == UINT32_C(0x3d800000));
            exception = "INDEX";
        } else {
            CHECK(p[6] != 1 && !c.failed && !fields.m &&
                  c.source_pc == (full_entry ? 32u : 5182u) && c.last_store_pc == 5177 &&
                  c.depth == (full_entry ? 0u : 1u) && (!full_entry || c.returned));
        }
        CHECK(mc_client_nw_inspect(f->graphics, &state) && state.fields.w &&
              f->base.world->persistent_bindings == bindings + 2u);
        CHECK(ao_observe(index++, p, &fields, completed ? (rendered ? 1 : 0) : -1, exception,
                         before, &state, f->base.world_classes->chunk_a));
    }
    CHECK(scanned == EOF && !ferror(input) && fclose(input) == 0 && index == 48);
    f->models->cv_a = prior_overlay;
    f->app->constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(mc_client_application_namespace_detach(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_classes_release(&application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_lifetime_retire(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    f->app->settings = NULL;
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL && f->base.world->persistent_bindings == bindings + 1u);
    CHECK(mc_client_chunk_cache_retire(&edge_cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK &&
          edge_cache == NULL && f->base.world->persistent_bindings == bindings);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !ao_controls(f, argv[3]) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf("Source AO: %u checks; 48 original whole-body rows, retained fields and NW vertices\n",
           checks);
    return 0;
}
