#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Reuse genuine constructor helpers; the included main never runs. */
#define MC_PICKING_BODY_MAIN_NAME mc_lever_scene_picking_fixture_main
int mc_lever_scene_picking_fixture_main(int argc, char **argv);
#include "../world/test_picking.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/render/blocks/internal.h"
#include "client/render/model_graph_internal.h"
#include "client/render_engine/nw.h"
#include "client/startup/application_owner.h"
#include "mc/client/settings/game_settings.h"
#include "world/chunk/object_state_internal.h"

static uint64_t scene_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool scene_controls(picking_fixture *f, const char *input) {
    uint8_t *planes = malloc(82176);
    CHECK(planes != NULL);
    FILE *file = fopen(input, "rb");
    CHECK(file != NULL && fread(planes, 1, 82176, file) == 82176 && fgetc(file) == EOF &&
          !ferror(file) && fclose(file) == 0);
    mc_chunk_data *data = f->chunks[0]->data;
    uint8_t *arrays[5] = {data->blocks, data->metadata, data->skylight, data->block_light,
                          data->heightmap};
    const size_t lengths[5] = {32768, 16384, 16384, 16384, 256};
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
    f->app->settings = settings;
    f->app->source_math_classes = f->math;
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error cache_error;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &cache_error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(f->models, &model_error));
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f->graphics,
                                  (mc_client_graphics_site){"native-test", "lever-scene", "()V", 0},
                                  &nw, &graphics) &&
          mc_client_nw_begin_quads(nw, &graphics) &&
          mc_client_nw_translation(nw, 0, -64, 0, &graphics));
    unsigned int rows = 0;
    for (int32_t light = 0; light < 16; ++light)
        for (int32_t ao = 0; ao < 2; ++ao)
            for (int32_t powered = -1; powered < 2; ++powered) {
                settings_state.values.ambient_occlusion = ao != 0;
                CHECK(mc_game_settings_set_values(settings, &settings_state.values, NULL) ==
                      MC_GAME_SETTINGS_OK);
                f->base.world->fields.f = light;
                size_t offset = 0;
                for (size_t i = 0; i < 5; ++i) {
                    memcpy(arrays[i], planes + offset, lengths[i]);
                    offset += lengths[i];
                }
                data->blocks[(3u << 11u) | (2u << 7u) | 65u] = powered < 0 ? 0 : 69;
                CHECK(mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, 3, 65, 2,
                                           powered < 0    ? 0
                                           : powered == 0 ? 4
                                                          : 12) &&
                      mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, 4, 65, 2,
                                           powered < 1 ? 0 : 15));
                uint8_t *unchanged = malloc(82176);
                CHECK(unchanged != NULL);
                offset = 0;
                for (size_t i = 0; i < 5; ++i) {
                    memcpy(unchanged + offset, arrays[i], lengths[i]);
                    offset += lengths[i];
                }
                const int32_t ids[2] = {69, 55};
                for (size_t which = powered < 0 ? 1u : 0u; which < 2; ++which) {
                    const int32_t id = ids[which], x = id == 69 ? 3 : 4;
                    const mc_block *block = NULL;
                    CHECK(mc_registry_block_get(f->base.registry, id, &block, NULL) ==
                              MC_REGISTRY_OK &&
                          block != NULL);
                    mc_client_render_blocks fields;
                    mc_client_render_blocks_construct_fields(&fields, cache, true);
                    CHECK(!fields.f && fields.d == -1);
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
                    CHECK(mc_cv_render(&context, block, x, 65, 2, &rendered) && rendered &&
                          !context.failed && context.returned && context.depth == 0 &&
                          fields.d == -1 && !fields.m);
                    CHECK(mc_client_nw_inspect(f->graphics, &state) &&
                          mc_entity_vector_get_snapshot(f->base.entities, &after) == MC_ENTITY_OK &&
                          after.cursor == before.cursor + (id == 69 ? 8 : 0) &&
                          after.holds == before.holds &&
                          state.fields.o == first + (id == 69 ? 66 : 12) * 8);
                    const mc_block_fields *bounds = &block->fields;
                    printf("SCENE\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32
                           "\ttrue\t-1\tfalse\t%d\t%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64
                           ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 "\t",
                           light, ao, powered, id, id == 69 ? 8 : 0, scene_bits(bounds->bs),
                           scene_bits(bounds->bt), scene_bits(bounds->bu), scene_bits(bounds->bv),
                           scene_bits(bounds->bw), scene_bits(bounds->bx));
                    for (int32_t i = first; i < state.fields.o; ++i)
                        printf("%s%08" PRIx32, i == first ? "" : ",", (uint32_t)state.fields.g[i]);
                    putchar('\n');
                    offset = 0;
                    for (size_t i = 0; i < 5; ++i) {
                        CHECK(memcmp(arrays[i], unchanged + offset, lengths[i]) == 0);
                        offset += lengths[i];
                    }
                    ++rows;
                }
                free(unchanged);
            }
    CHECK(rows == 160);
    f->app->constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(mc_client_application_namespace_detach(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_classes_release(&application, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_lifetime_retire(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    f->app->settings = NULL;
    f->app->source_math_classes = NULL;
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK &&
          mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    free(planes);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !scene_controls(f, argv[3]) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf("Lever scene geometry: %u checks; 160 original translated AO/flat rows\n", checks);
    return 0;
}
