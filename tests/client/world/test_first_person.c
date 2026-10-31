#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/player/player_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/client/world/first_person.h"
#include "mc/entity/player/player.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/entities.h"
#include <emmintrin.h>
#include <math.h>
#ifdef MC_CAMERA_GPU
#include "renderer.h"
#include <SDL3/SDL_main.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
static mc_math_trig trig;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_world_runtime_classes *world_classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_world_info_name *name;
    mc_player_session_classes *sessions;
    mc_chunk_state *controlled_chunk;
} fixture;

static bool setup(fixture *f, const char *resources, const char *directory) {
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_statistics(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                          &f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->world_classes, f->registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->world_classes, f->entities) == MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options store = {.session_token = 6381,
                                          .block_map = map,
                                          .block_map_length = 256,
                                          .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, f->regions, &store, &f->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    static const uint16_t name[] = {'P', 'l', 'a', 'y', 'e', 'r'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){name, 6}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->store, .name = f->name, .seed = 5143, .dimension = f->dimension};
    mc_world_storage_error error;
    const mc_world_storage_status status =
        mc_world_runtime_construct_world(f->world, &args, &error);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "World: %d %s\n", (int)status, error.operation);
    CHECK(status == MC_WORLD_STORAGE_OK);
    CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
          MC_PLAYER_OK);
    return true;
}

static bool teardown(fixture *f) {
    CHECK(mc_player_session_classes_free(&f->sessions, NULL) == MC_PLAYER_OK);
    if (f->controlled_chunk != NULL) {
        /* Authored native retirement input, not a fabricated base-provider drop method. */
        const mc_chunk_acquire_context *actual = mc_world_runtime_acquisition(f->world);
        bool added = false;
        CHECK(actual != NULL &&
              mc_chunk_pending_add(actual->pending, mc_chunk_cache_key(0, -1), &added) ==
                  MC_CHUNK_ACQUIRE_OK &&
              added);
        bool result = true;
        CHECK(mc_world_runtime_maintain(f->world, &result) == MC_CHUNK_LIFECYCLE_OK);
        CHECK(mc_world_store_retire_chunk(f->store, f->controlled_chunk, NULL) ==
              MC_WORLD_STORAGE_OK);
        f->controlled_chunk = NULL;
    }
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_unbind(f->dimension);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_chunk_count(f->store) == 0);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(f->dimension);
    CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(f->regions);
    mc_world_info_name_release(f->name);
    CHECK(mc_world_runtime_classes_free(f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    mc_resource_files_free(f->files);
    return true;
}

static bool make_player(fixture *f, mc_entity **out) {
    static const uint16_t name[] = {'I', 'n', 'p', 'u', 't'};
    mc_player_string *username = NULL;
    mc_player_session *session = NULL;
    CHECK(mc_player_string_create(name, 5, &username) == MC_PLAYER_OK);
    CHECK(mc_player_session_create(f->sessions, username, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entities, out, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(*out, f->world, NULL, session, 0, NULL, NULL) == MC_PLAYER_OK);
    mc_player_session_retire(&session);
    CHECK(session == NULL);
    mc_player_string_release(username);
    return true;
}

static uint32_t bits(float v) {
    uint32_t out;
    memcpy(&out, &v, 4);
    return out;
}

static uint64_t dbits(double v) {
    uint64_t out;
    memcpy(&out, &v, 8);
    return out;
}

static bool row(mc_entity *p, mc_game_settings *settings, fixture *fx, int which) {
    mc_client_camera_frame f;
    mc_client_camera_error e;
    mc_game_settings_state state;
    CHECK(mc_game_settings_read_state(settings, &state));
    mc_game_settings_values options = state.values;
    mc_entity_base_fields *b = &p->base;
    mc_local_player_snapshot *v = &p->local_player->fields;
    options.view_bobbing = which >= 1;
    options.anaglyph = which == 5 || which == 6;
    options.third_person_view = which == 10 || which == 11;
    options.debug_camera = which == 8 || which == 10;
    options.render_distance = which == 12 ? -1 : which == 13 ? 32 : 0;
    CHECK(mc_game_settings_set_values(settings, &options, NULL) == MC_GAME_SETTINGS_OK);
    v->living.Y = which == 7 ? 0 : 20;
    v->living.ad = which == 7 ? 17 : 0;
    v->living.aa = which == 2 ? 7 : 0;
    v->living.ab = which == 14 ? 0 : 10;
    v->living.ac = 60;
    b->bi = which >= 1 ? 1.125f : 0;
    b->bj = which >= 1 ? 1.5f : 0;
    v->player.h = which >= 1 ? .1f : 0;
    v->player.i = which >= 1 ? .175f : 0;
    v->living.af = which >= 1 ? 2.0f : 0.0f;
    v->living.ag = which >= 1 ? -1.0f : 0.0f;
    v->player.C = which == 3 ? .2f : 0;
    v->player.B = which == 3 ? .7f : 0;
    if (which == 22) {
        const uint32_t nan = 0x7fc01234u;
        memcpy(&v->player.B, &nan, 4);
    }
    v->player.u = which == 9 || which == 10 || which == 26 || which == 27 || which == 28;
    if (which >= 25) {
        mc_chunk_state *chunk = NULL;
        CHECK(mc_world_runtime_get(fx->world, 0, -1, &chunk) == MC_CHUNK_ACQUIRE_OK);
        CHECK(chunk != NULL && chunk->data != NULL && !chunk->blank);
        fx->controlled_chunk = chunk;
        chunk->data->blocks[11u * 2048u + 9u * 128u + 64u] = (uint8_t)(which == 25 ? 8 : 26);
        CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, 11, 64, 9,
                                   which == 27 ? 3 : 0));
        b->y = 64.1;
        b->aK = b->y + .25;
    }
    b->offset = which == 4 ? 1.4f : 1.62f;
    const float t = which == 14 ? 0 : which == 15 ? 1 : which == 16 ? 1.5f : .5f;
    mc_world_runtime *world = b->world;
    if (which == 21)
        b->world = NULL;
    const unsigned before = _mm_getcsr();
    if (which == 19 || which == 20)
        _mm_setcsr((before & ~0x6000u) | 0x2000u);
    const mc_client_camera_status status = mc_client_first_person_camera(
        which == 18 ? NULL : p, which == 17 || which == 19 ? NULL : settings,
        which == 28 ? NULL : fx->world, fx->registry, &trig, which == 23 ? 0 : 854,
        which == 24 ? 0 : 480, t, which == 6 ? 1 : 0, &f, &e);
    _mm_setcsr(before);
    b->world = world;
    const mc_client_camera_status expected =
        which == 3 || which == 11 ? MC_CLIENT_CAMERA_UNIMPLEMENTED_DEPENDENCY
        : which == 17 || which == 18 || which == 19 || which == 28 ? MC_CLIENT_CAMERA_NULL_REFERENCE
        : which == 20                                              ? MC_CLIENT_CAMERA_UNSUPPORTED_FP
        : which == 21 ? MC_CLIENT_CAMERA_MATERIAL_FAILED
                      : MC_CLIENT_CAMERA_OK;
    CHECK(status == expected);
    CHECK(f.complete == (status == MC_CLIENT_CAMERA_OK));
    CHECK(p->holds == 1);
    CHECK(f.operation_count <= MC_CLIENT_CAMERA_MAX_OPERATIONS);
    if (which == 3) {
        CHECK(e.pc == 292 && strcmp(e.method, "a") == 0 && f.portal > 0 && f.portal_scale > 0);
    }
    if (which == 11) {
        CHECK(e.pc == 292 && strcmp(e.method, "g") == 0);
    }
    if (which == 17 || which == 19)
        CHECK(e.pc == 11 && f.operation_count == 0);
    if (which == 18 || which == 21)
        CHECK(e.pc == 15 && strcmp(e.method, "d") == 0);
    if (which == 28)
        CHECK(e.pc == 159 && strcmp(e.method, "g") == 0 && f.operation_count == 7);
    if (which == 20)
        CHECK(e.pc == 15 && f.operation_count == 0);
    if (f.complete) {
        CHECK(f.render_x == b->aJ + (b->x - b->aJ) * (double)t);
        CHECK(f.render_y == b->aK + (b->y - b->aK) * (double)t);
        CHECK(f.render_z == b->aL + (b->z - b->aL) * (double)t);
        CHECK(!f.cloud_fog);
        CHECK(f.operation_count > 0);
        const mc_client_camera_operation *last = &f.operations[f.operation_count - 1];
        CHECK(last->pc == 787 && strcmp(last->method, "g") == 0);
        CHECK(f.fog_y == f.render_y - (double)last->arguments[1]);
        CHECK(which == 22 ? isnan(f.portal) : true);
        if (which == 12)
            CHECK(f.far_distance == 0);
        if (which == 13)
            CHECK(f.far_distance == 256);
        if (which == 23)
            CHECK(f.projection[0] == 1 && f.projection[5] == 1 && f.projection[15] == 1);
        if (which == 24)
            CHECK(f.projection[0] == 0);
        if (which == 25)
            CHECK(f.vertical_fov == 60);
        if (which == 26 || which == 27) {
            bool bed = false;
            for (size_t i = 0; i < f.operation_count; ++i)
                if (f.operations[i].pc == 223) {
                    CHECK(f.operations[i].arguments[0] == (which == 27 ? 270.0f : 0.0f));
                    bed = true;
                }
            CHECK(bed);
        }
    }
    printf("CAMERA %d %d %s %u %zu %08x %08x %08x %08x\n", which, (int)status,
           e.method == NULL ? "-" : e.method, e.pc, f.operation_count,
           (unsigned)bits(f.vertical_fov), (unsigned)bits(f.far_distance), (unsigned)bits(f.portal),
           (unsigned)bits(f.portal_scale));
    if (f.complete) {
        printf("ORIGIN %d %016llx %016llx %016llx %016llx\n", which,
               (unsigned long long)dbits(f.render_x), (unsigned long long)dbits(f.render_y),
               (unsigned long long)dbits(f.render_z), (unsigned long long)dbits(f.fog_y));
        printf("PROJECTION %d", which);
        for (size_t i = 0; i < 16; ++i)
            printf(" %08x", (unsigned)bits(f.projection[i]));
        puts("");
        printf("MODELVIEW %d", which);
        for (size_t i = 0; i < 16; ++i)
            printf(" %08x", (unsigned)bits(f.modelview[i]));
        puts("");
    }
    for (size_t i = 0; i < f.operation_count; ++i) {
        const mc_client_camera_operation *op = &f.operations[i];
        printf("OP %d %d %d %s %u", which, (int)op->kind, op->projection ? 1 : 0, op->method,
               op->pc);
        for (size_t k = 0; k < 4; ++k)
            printf(" %08x", (unsigned)bits(op->arguments[k]));
        puts("");
    }
    return true;
}

static bool cases(fixture *f) {
    mc_entity *p = NULL;
    mc_game_settings *settings = NULL;
    CHECK(mc_world_info_spawn_set(mc_world_runtime_info(f->world), 11, 190, -7) ==
          MC_WORLD_INFO_OK);
    CHECK(make_player(f, &p));
    p->base.yaw = 33;
    p->base.aU = 12;
    p->base.aT = -17;
    p->base.aV = 3;
    p->base.aJ = p->base.x - .75;
    p->base.aK = p->base.y + .25;
    p->base.aL = p->base.z + 1.25;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    for (int which = 0; which < 29; ++which)
        CHECK(row(p, settings, f, which));
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_entity_release(p);
    return true;
}

#ifdef MC_CAMERA_GPU
static bool gpu(fixture *f) {
    mc_entity *p = NULL;
    mc_game_settings *settings = NULL;
    mc_client_display *display = NULL;
    mc_client_world_renderer *renderer = NULL;
    mc_client_world_mesh *mesh = NULL;
    mc_client_display_error e = {0};
    mc_client_capture image = {0};
    mc_client_camera_frame frame;
    mc_client_camera_error camera;
    CHECK(make_player(f, &p));
    p->base.yaw = p->base.aU = p->base.aT = p->base.aV = 0;
    p->base.aJ = p->base.x;
    p->base.aK = p->base.y;
    p->base.aL = p->base.z;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state s;
    CHECK(mc_game_settings_read_state(settings, &s));
    mc_game_settings_values options = s.values;
    options.view_bobbing = false;
    CHECK(mc_game_settings_set_values(settings, &options, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_display_create("Genuine LocalPlayer camera", 128, 128, true, &display, &e));
    const uint8_t white[4] = {255, 255, 255, 255};
    const mc_image_view texture = {1, 1, 4, white};
    CHECK(mc_client_world_renderer_create(display, &texture, false, NULL, &renderer, &e));
    const int32_t x = mc_math_floor_double(p->base.x), y = mc_math_floor_double(p->base.y),
                  z = mc_math_floor_double(p->base.z);
    const float dx = (float)(p->base.x - x), dy = (float)(p->base.y - y),
                dz = (float)(p->base.z - z);
    const mc_client_world_vertex corner[4] = {{dx - 1, dy - 1, dz + 3, 0, 1, 0xffff0000u},
                                              {dx - 1, dy + 1, dz + 3, 0, 0, 0xffff0000u},
                                              {dx + 1, dy + 1, dz + 3, 1, 0, 0xffff0000u},
                                              {dx + 1, dy - 1, dz + 3, 1, 1, 0xffff0000u}};
    const unsigned order[6] = {0, 1, 2, 0, 2, 3};
    mc_client_world_vertex vertices[6];
    for (size_t i = 0; i < 6; ++i)
        vertices[i] = corner[order[i]];
    CHECK(mc_client_world_mesh_create(renderer, x, y, z, vertices, 6, &mesh, &e));
    const mc_client_world_fog fog = {.kind = MC_CLIENT_WORLD_FOG_NONE, .blue = 1};
    int width, height;
    CHECK(mc_client_display_pixel_size(display, &width, &height, &e));
    for (int view = 0; view < 2; ++view) {
        if (view == 1)
            p->base.yaw = p->base.aU = 90;
        CHECK(mc_client_first_person_camera(p, settings, f->world, f->registry, &trig, width,
                                            height, .5f, 0, &frame,
                                            &camera) == MC_CLIENT_CAMERA_OK);
        CHECK(mc_client_world_renderer_begin_first_person(renderer, &frame, &fog, &e));
        CHECK(mc_client_world_renderer_draw(renderer, mesh, false, true, true, &e));
        CHECK(mc_client_display_capture_rgba(display, &image, &e));
        const uint8_t *pixel = image.pixels + (size_t)(image.height / 2) * image.stride +
                               (size_t)(image.width / 2) * 4u;
        CHECK(pixel[0] == (view == 0 ? 255 : 0) && pixel[1] == 0 &&
              pixel[2] == (view == 0 ? 0 : 255));
        mc_client_capture_free(&image);
    }
    frame.complete = false;
    CHECK(!mc_client_world_renderer_begin_first_person(renderer, &frame, &fog, &e) &&
          e.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(!mc_client_world_renderer_draw(renderer, mesh, false, true, true, &e));
    frame.complete = true;
    frame.width++;
    CHECK(!mc_client_world_renderer_begin_first_person(renderer, &frame, &fog, &e));
    frame.width--;
    frame.modelview[0] = NAN;
    CHECK(!mc_client_world_renderer_begin_first_person(renderer, &frame, &fog, &e));
    CHECK(mc_client_world_mesh_retire(&mesh, &e));
    CHECK(mc_client_world_renderer_retire(&renderer, &e));
    CHECK(mc_client_display_destroy(display, &e));
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_entity_release(p);
    return true;
}
#endif
int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("First-person camera input: resources world-directory\n", stderr);
        return 2;
    }
    if (!mc_math_trig_init(&trig))
        return 1;
    fixture f = {0};
    if (!setup(&f, argv[1], argv[2]) || !cases(&f))
        return 1;
#ifdef MC_CAMERA_GPU
    if (!gpu(&f))
        return 1;
#endif
    if (!teardown(&f))
        return 1;
    printf("First-person camera: %u checks\n", checks);
    return 0;
}
