/* Reuse genuine constructor helpers; their original main is not executed. */
int mc_picking_constructor_fixture_main(int argc, char **argv);
#define main mc_picking_constructor_fixture_main
#include "../../entity/player/test_construction.c"
#undef main
#ifdef MC_PICKING_BODY_MAIN_NAME
#define main MC_PICKING_BODY_MAIN_NAME
#endif

#include "client/application_state.h"
#include "client/world/entity_renderer/internal.h"
#include "mc/client/controller/singleplayer.h"
#include "mc/client/world/picking.h"
#include "mc/core/math/classes.h"
#include "mc/entity/hit_result.h"
#include "mc/entity/living/view.h"
#include "mc/entity/minecart/minecart.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/runtime_internal.h"

#include <inttypes.h>

typedef struct picking_fixture {
    fixture base;
    mc_math_classes *math;
    mc_client_render_classes *models;
    mc_client_render_engine_classes *graphics;
    mc_client_source_px *px;
    client_application *app;
    mc_client_singleplayer_controller *controller;
    mc_entity *actors[6];
    mc_item_stack *stack;
    mc_chunk_state *chunks[4];
    mc_chunk_provider_fields provider;
    mc_entity_hit_result *sentinel;
    mc_entity_hit_result *external_result;
    bool foreign_world_hold;
    const mc_entity_hit_result *before_hit;
    const mc_entity *before_entity;
    int32_t before_cursor;
    mc_client_entity_renderer_pick_call *call;
    mc_client_entity_renderer_pick_state state;
    mc_client_entity_renderer_pick_error detach;
    mc_client_entity_renderer_error owner_error;
} picking_fixture;

static const char *const actor_names[] = {"camera",   "player",   "cart",
                                          "fallingA", "fallingB", "item"};
static const char *const case_names[] = {"empty",
                                         "block_near",
                                         "block_os4_beyond_entity3",
                                         "block_beyond_os4",
                                         "falling_hit",
                                         "minecart_hit",
                                         "player_hit",
                                         "entity_behind_block",
                                         "entity_before_block",
                                         "inside_box",
                                         "excluded_self",
                                         "dead_not_pickable",
                                         "item_not_pickable",
                                         "equal_distance_first_A",
                                         "equal_distance_first_B",
                                         "duplicate_order_ABA",
                                         "inside_order_AB",
                                         "inside_order_BA",
                                         "inside_then_far",
                                         "far_then_inside",
                                         "entity_before_cap3",
                                         "entity_beyond_cap3",
                                         "block4_entity3_cap",
                                         "finite_partial_half",
                                         "early_null_camera",
                                         "early_null_world"};

static uint64_t picking_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static const char *picking_actor(const picking_fixture *f, const mc_entity *entity) {
    if (entity == NULL)
        return NULL;
    for (size_t i = 0; i < 6; ++i)
        if (f->actors[i] == entity)
            return actor_names[i];
    return "FOREIGN";
}

static void picking_string(const char *text) {
    if (text == NULL)
        fputs("null", stdout);
    else
        printf("\"%s\"", text);
}

static void picking_vector(const mc_entity_vector *vector) {
    printf("[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]", picking_bits(vector->x),
           picking_bits(vector->y), picking_bits(vector->z));
}

static int picking_slot(const mc_entity_classes *classes, const mc_entity_vector *vector) {
    if (vector == NULL)
        return -1;
    if (classes->vector_pool != NULL)
        for (size_t i = 0; i < classes->vector_pool->length; ++i)
            if (classes->vector_pool->elements[i] == vector)
                return (int)i;
    return -2;
}

static bool picking_open(picking_fixture *f, const char *resources) {
    fixture *b = &f->base;
    CHECK(mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_resource_files_create(resources, &b->files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&b->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &b->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, b->profile, NULL, &b->entities) ==
          MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, b->profile, b->materials, 0,
                                         mc_resource_files_source(b->files)};
    CHECK(mc_registry_classes_allocate(&options, &b->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_statistics(b->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, b->profile,
                                          mc_math_classes_table(f->math), &b->world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(b->world_classes, b->registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(b->world_classes, b->entities) == MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &b->regions, NULL) == MC_WORLD_STORAGE_OK);
    const uint16_t title[] = {'P', 'i', 'c', 'k'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, 4}, SIZE_MAX, &b->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_player_session_classes_create(b->entities, b->registry, &b->sessions, NULL) ==
          MC_PLAYER_OK);
    CHECK(mc_client_render_classes_create(b->entities, b->profile, &f->models) ==
          MC_CLIENT_MODEL_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    const mc_client_entity_renderer_services services = {f->models, f->graphics, b->profile, NULL};
    CHECK(mc_client_entity_renderer_allocate(&services, NULL, &f->px, &f->owner_error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    f->app = calloc(1, sizeof(*f->app));
    CHECK(f->app != NULL && f->px->phase == MC_CLIENT_ENTITY_RENDERER_RAW);
    /* Explicit prepared-body App/PX source field, matching ReflectionFactory
     * original scope. No constructor, READY phase or source return is injected. */
    f->px->fields.j = f->app;
    CHECK(mc_client_singleplayer_controller_allocate(b->entities, b->profile, 0, &f->controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_singleplayer_controller_construct(f->controller, f->app, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_construct(f->stack, 1, 1, 0) == MC_ITEM_STACK_OK);
    return true;
}

static bool picking_player(picking_fixture *f, mc_entity **out) {
    mc_player_session *session = NULL;
    CHECK(mc_player_session_create(f->base.sessions, NULL, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->base.entities, out, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(*out, f->base.world, NULL, session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    mc_player_session_retire(&session);
    return true;
}

static bool picking_case_open(picking_fixture *f, const char *directory) {
    fixture *b = &f->base;
    uint8_t ids[256];
    for (size_t i = 0; i < sizeof(ids); ++i)
        ids[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 6381,
                                            .block_map = ids,
                                            .block_map_length = sizeof(ids),
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, b->regions, &options, &b->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_provider_create(0, NULL, &b->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, b->world_classes,
                                          &limits, &b->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = b->store, .name = b->name, .seed = 23, .dimension = b->dimension};
    CHECK(mc_world_runtime_construct_world(b->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(b->world->construction.mode == MC_WORLD_RUNTIME_GAME &&
          b->world->construction.phase == MC_WORLD_RUNTIME_READY);
    CHECK(mc_chunk_provider_bind_fields(b->world->fields.v, &f->provider, NULL) ==
          MC_WORLD_STORAGE_OK);
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            const size_t index = (size_t)(x * 2 + z);
            CHECK(mc_world_runtime_get(b->world, x, z, &f->chunks[index]) == MC_CHUNK_ACQUIRE_OK);
            CHECK(f->chunks[index] != NULL && f->chunks[index]->active &&
                  f->chunks[index]->terrain_populated);
        }
    CHECK(picking_player(f, &f->actors[0]) && picking_player(f, &f->actors[1]));
    CHECK(mc_minecart_entity_allocate(b->entities, &f->actors[2], NULL) == MC_ENTITY_OK);
    CHECK(mc_minecart_entity_construct(f->actors[2], b->world, 8.5, 64.5, 10.5, 0, NULL) ==
          MC_ENTITY_OK);
    for (size_t i = 3; i < 5; ++i) {
        CHECK(mc_falling_entity_allocate(b->entities, &f->actors[i], NULL) == MC_ENTITY_OK);
        CHECK(mc_falling_entity_construct(f->actors[i], b->world, 8.5, 64.5, 10.5, 13, NULL) ==
              MC_ENTITY_OK);
    }
    CHECK(mc_item_entity_allocate(b->entities, &f->actors[5], NULL) == MC_ENTITY_OK);
    CHECK(mc_item_entity_construct(f->actors[5], b->world, 8.5, 64.5, 10.5, f->stack, NULL) ==
          MC_ENTITY_OK);
    for (size_t i = 0; i < 6; ++i)
        CHECK(mc_entity_set_position(f->actors[i], 8.5, 64.5, 10.5) == MC_ENTITY_OK);
    mc_entity *camera = f->actors[0];
    camera->base.offset = 0;
    CHECK(mc_entity_set_position(camera, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    camera->base.aJ = camera->base.x;
    camera->base.aK = camera->base.y;
    camera->base.aL = camera->base.z;
    camera->base.yaw = camera->base.aT = camera->base.aU = camera->base.aV = 0;
    f->app->i = camera;
    f->app->h = camera;
    f->app->active_world = b->world;
    f->app->controller = f->controller;
    f->app->y = NULL;
    f->px->fields.m = f->actors[2]; /* Prepared borrowed source field, no native m occurrence. */
    CHECK(mc_entity_bounds_pool_reset(b->entities, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_pool_reset(b->entities, NULL) == MC_ENTITY_OK);
    return true;
}

static bool picking_publish(picking_fixture *f, size_t actor) {
    bool published = false;
    CHECK(mc_world_runtime_insert_entity(f->base.world, f->actors[actor], &published, NULL) ==
              MC_WORLD_STORAGE_OK &&
          published);
    return true;
}

static bool picking_position(picking_fixture *f, size_t actor, double z) {
    CHECK(mc_entity_set_position(f->actors[actor], 8.5, 64.5, z) == MC_ENTITY_OK);
    return true;
}

static bool picking_prepare(picking_fixture *f, size_t index, float *partial) {
    *partial = 1;
    /* The original observer keeps this authored target offset after row6. */
    if (index >= 6) {
        f->actors[1]->base.offset = 0;
        CHECK(picking_position(f, 1, 10.5));
    }
    if (index == 1 || index == 2 || index == 3 || index == 7 || index == 8 || index == 22) {
        const int32_t z = index == 2 || index == 22 ? 12 : index == 3 ? 13 : 10;
        f->chunks[0]->data->blocks[(8u << 11u) | ((size_t)z << 7u) | 64u] = 1;
    }
    switch (index) {
        case 4:
            return picking_publish(f, 3);
        case 5:
            return picking_publish(f, 2);
        case 6:
            f->actors[1]->base.offset = 0;
            return picking_position(f, 1, 10.5) && picking_publish(f, 1);
        case 7:
            return picking_position(f, 3, 11.5) && picking_publish(f, 3);
        case 8:
            return picking_position(f, 3, 9.5) && picking_publish(f, 3);
        case 9:
            return picking_position(f, 3, 8.5) && picking_publish(f, 3);
        case 10:
            return picking_publish(f, 0);
        case 11:
            f->actors[3]->base.be = true;
            return picking_publish(f, 3);
        case 12:
            return picking_publish(f, 5);
        case 13:
            return picking_publish(f, 3) && picking_publish(f, 4);
        case 14:
            return picking_publish(f, 4) && picking_publish(f, 3);
        case 15:
            return picking_publish(f, 3) && picking_publish(f, 4) && picking_publish(f, 3);
        case 16:
        case 17:
            CHECK(picking_position(f, 3, 8.5) && picking_position(f, 4, 8.5));
            return index == 16 ? picking_publish(f, 3) && picking_publish(f, 4)
                               : picking_publish(f, 4) && picking_publish(f, 3);
        case 18:
        case 19:
            CHECK(picking_position(f, 3, 8.5));
            return index == 18 ? picking_publish(f, 3) && picking_publish(f, 4)
                               : picking_publish(f, 4) && picking_publish(f, 3);
        case 20:
            return picking_position(f, 3, 12.0) && picking_publish(f, 3);
        case 21:
        case 22:
            return picking_position(f, 3, 12.25) && picking_publish(f, 3);
        case 23:
            f->actors[0]->base.aJ = 7.5;
            f->actors[0]->base.aU = 90;
            *partial = .5f;
            break;
        case 24:
        case 25:
            CHECK(mc_entity_hit_result_allocate(f->base.entities, &f->sentinel, NULL) ==
                  MC_ENTITY_HIT_OK);
            CHECK(mc_entity_hit_result_construct_entity(f->sentinel, f->actors[3], NULL) ==
                  MC_ENTITY_HIT_OK);
            f->app->y = f->sentinel;
            if (index == 24)
                f->app->i = NULL;
            else
                f->app->active_world = NULL;
            break;
        default:
            break;
    }
    return true;
}

static bool picking_emit(picking_fixture *f, size_t index) {
    const mc_entity_classes *classes = f->base.entities;
    float reach = 0;
    CHECK(mc_client_singleplayer_controller_reach(f->controller, &reach, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    uint32_t partial_bits, reach_bits;
    memcpy(&partial_bits, &f->state.partial, sizeof(partial_bits));
    memcpy(&reach_bits, &reach, sizeof(reach_bits));
    printf("PICK {\"case\":\"%s\",\"partial_bits\":\"%08" PRIx32
           "\",\"controller\":\"os\",\"reach_bits\":\"%08" PRIx32
           "\",\"camera_null\":%s,\"world_null\":%s,\"same_hit_as_before\":%s,"
           "\"same_m_as_before\":%s,\"bt_cursor_before\":%" PRId32 ",\"actors\":[",
           case_names[index], partial_bits, reach_bits, f->app->i == NULL ? "true" : "false",
           f->app->active_world == NULL ? "true" : "false",
           f->app->y == f->before_hit ? "true" : "false",
           f->px->fields.m == f->before_entity ? "true" : "false", f->before_cursor);
    for (size_t i = 0; i < 6; ++i) {
        const mc_entity *actor = f->actors[i];
        const mc_entity_bounds *box = actor->bounds;
        bool pickable = false;
        float border = 0;
        CHECK(mc_entity_pickable(f->actors[i], &pickable, NULL) == MC_ENTITY_HIT_OK);
        CHECK(mc_entity_pick_border_size(f->actors[i], &border, NULL) == MC_ENTITY_HIT_OK);
        uint32_t border_bits;
        memcpy(&border_bits, &border, sizeof(border_bits));
        if (i != 0)
            fputc(',', stdout);
        printf("{\"name\":\"%s\",\"class\":\"%s\",\"dead\":%s,\"pickable\":%s,"
               "\"border_bits\":\"%08" PRIx32 "\",\"position_bits\":",
               actor_names[i],
               i < 2    ? "dc"
               : i == 2 ? "yl"
               : i < 5  ? "ju"
                        : "hl",
               actor->base.be ? "true" : "false", pickable ? "true" : "false", border_bits);
        const mc_entity_vector position = {actor->base.x, actor->base.y, actor->base.z};
        picking_vector(&position);
        printf(",\"bounds_bits\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
               "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]}",
               picking_bits(box->min_x), picking_bits(box->min_y), picking_bits(box->min_z),
               picking_bits(box->max_x), picking_bits(box->max_y), picking_bits(box->max_z));
    }
    fputs("],\"app_y\":", stdout);
    if (f->app->y == NULL)
        fputs("null", stdout);
    else {
        mc_entity_hit_result_view hit;
        mc_entity_hit_kind_view kind;
        CHECK(mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK);
        CHECK(mc_entity_hit_kind_inspect(hit.fields.a, &kind) == MC_ENTITY_HIT_OK);
        printf("{\"kind\":\"%s\",\"coords\":[%" PRId32 ",%" PRId32 ",%" PRId32 "],\"face\":%" PRId32
               ",\"entity\":",
               kind.ordinal == 0 ? "TILE" : "ENTITY", hit.fields.b, hit.fields.c, hit.fields.d,
               hit.fields.e);
        picking_string(picking_actor(f, hit.fields.g));
        printf(",\"vector_slot\":%d,\"vector_bits\":", picking_slot(classes, hit.fields.f));
        picking_vector(hit.fields.f);
        fputc('}', stdout);
    }
    fputs(",\"px_m\":", stdout);
    picking_string(picking_actor(f, f->px->fields.m));
    fputs(",\"nearby_order\":[", stdout);
    const mc_world_reference_list *nearby = f->base.world->fields.R;
    for (size_t i = 0; i < nearby->count; ++i) {
        if (i != 0)
            fputc(',', stdout);
        picking_string(picking_actor(f, nearby->items[i]));
    }
    printf("],\"bt_cursor_after\":%" PRId32 ",\"bt_pool_size\":%zu,\"vectors\":[",
           classes->vector_cursor, classes->vector_pool->length);
    for (int32_t i = 0; i < classes->vector_cursor; ++i) {
        if (i != 0)
            fputc(',', stdout);
        printf("{\"slot\":%" PRId32 ",\"bits\":", i);
        picking_vector(classes->vector_pool->elements[i]);
        fputc('}', stdout);
    }
    fputs("]}\n", stdout);
    return true;
}

static bool picking_case_close(picking_fixture *f) {
    fixture *b = &f->base;
    CHECK(mc_client_entity_renderer_pick_retire(&f->call) == MC_CLIENT_PICK_OK && f->call == NULL);
    CHECK(mc_client_entity_renderer_pick_detach(f->px, &f->detach) == MC_CLIENT_PICK_OK);
    f->px->fields.m = NULL; /* End an unowned prepared early-return borrow. */
    f->app->y = NULL;
    CHECK(mc_entity_hit_result_release(&f->sentinel, NULL) == MC_ENTITY_HIT_OK);
    f->app->i = f->app->h = NULL;
    f->app->active_world = NULL;
    CHECK(mc_world_runtime_retire_entity_containers(b->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_cache_init(f->provider.cache, f->provider.cache->entries,
                              f->provider.cache->capacity, f->provider.cache->insertions,
                              f->provider.cache->insertion_capacity));
    mc_world_provider_unbind(b->dimension);
    CHECK(mc_world_runtime_retire_world(b->world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_world_store_retire_chunk(b->store, f->chunks[i], NULL) == MC_WORLD_STORAGE_OK);
        f->chunks[i] = NULL;
    }
    for (size_t i = 0; i < 6; ++i) {
        mc_entity_release(f->actors[i]);
        f->actors[i] = NULL;
    }
    if (f->external_result != NULL) {
        mc_entity_hit_result_view view;
        CHECK(mc_world_runtime_free(b->world, NULL) == MC_WORLD_STORAGE_BUSY);
        CHECK(mc_entity_hit_result_inspect(f->external_result, &view) == MC_ENTITY_HIT_OK &&
              view.phase == MC_ENTITY_HIT_READY && view.native_holds == 1 &&
              view.fields.g != NULL && view.fields.f->x == 91 && view.fields.f->y == 92 &&
              view.fields.f->z == 93);
        CHECK(mc_entity_hit_result_release(&f->external_result, NULL) == MC_ENTITY_HIT_OK &&
              f->external_result == NULL);
    }
    if (f->foreign_world_hold) {
        CHECK(mc_world_runtime_free(b->world, NULL) == MC_WORLD_STORAGE_BUSY);
        mc_world_runtime_binding_release(b->world);
        f->foreign_world_hold = false;
    }
    CHECK(mc_world_runtime_free(b->world, NULL) == MC_WORLD_STORAGE_OK);
    b->world = NULL;
    mc_world_provider_release(b->dimension);
    b->dimension = NULL;
    CHECK(mc_world_store_free_checked(&b->store, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool picking_source_null(picking_fixture *f, unsigned int pc) {
    CHECK(mc_client_entity_renderer_pick_inspect(f->call, &f->state));
    CHECK(!f->state.active && !f->state.source_returned && f->state.source_pc == pc &&
          f->state.failure->status == MC_CLIENT_PICK_SOURCE_NULL_REFERENCE &&
          !f->state.failure->native_policy &&
          strcmp(f->state.failure->exception_class, "java/lang/NullPointerException") == 0 &&
          strcmp(f->state.failure->site.class_name, "px") == 0 &&
          strcmp(f->state.failure->site.method, "a") == 0 &&
          strcmp(f->state.failure->site.descriptor, "(F)V") == 0 &&
          f->state.failure->site.pc == (int32_t)pc);
    return true;
}

static bool picking_controls(picking_fixture *f, const char *directory) {
    CHECK(picking_case_open(f, directory));
    CHECK(mc_client_entity_renderer_pick_begin(NULL, f->math, 1, &f->call) ==
          MC_CLIENT_PICK_SOURCE_NULL_REFERENCE);
    CHECK(picking_source_null(f, 0));
    CHECK(mc_client_entity_renderer_pick_retire(&f->call) == MC_CLIENT_PICK_OK && f->call == NULL);
    puts("CONTROL source-null-px");

    f->px->fields.j = NULL;
    CHECK(mc_client_entity_renderer_pick_begin(f->px, f->math, 1, &f->call) ==
          MC_CLIENT_PICK_SOURCE_NULL_REFERENCE);
    f->px->fields.j = f->app;
    CHECK(picking_source_null(f, 4) && f->state.origin == NULL && f->state.app == NULL);
    unsigned char before[sizeof(*f->state.failure)];
    memcpy(before, f->state.failure, sizeof(before));
    mc_client_entity_renderer_pick_call *other = NULL;
    CHECK(mc_client_entity_renderer_pick_begin(f->px, f->math, 1, &other) == MC_CLIENT_PICK_BUSY &&
          other == NULL);
    CHECK(mc_client_entity_renderer_retire(&f->px, &f->owner_error) ==
              MC_CLIENT_ENTITY_RENDERER_BUSY &&
          f->px != NULL);
    CHECK(mc_client_entity_renderer_pick_detach(f->px, &f->detach) == MC_CLIENT_PICK_BUSY);
    CHECK(mc_client_entity_renderer_pick_inspect(f->call, &f->state) &&
          memcmp(before, f->state.failure, sizeof(before)) == 0);
    CHECK(mc_client_entity_renderer_retain(f->px, &f->owner_error) == MC_CLIENT_ENTITY_RENDERER_OK);
    mc_client_source_px *extra_px = f->px;
    CHECK(mc_client_entity_renderer_pick_retire(&f->call) == MC_CLIENT_PICK_OK && f->call == NULL);
    CHECK(mc_client_entity_renderer_pick_detach(f->px, &f->detach) == MC_CLIENT_PICK_BUSY);
    CHECK(mc_client_entity_renderer_release(&extra_px, &f->owner_error) ==
              MC_CLIENT_ENTITY_RENDERER_OK &&
          extra_px == NULL);
    CHECK(mc_client_entity_renderer_pick_detach(f->px, &f->detach) == MC_CLIENT_PICK_OK);
    puts("CONTROL source-null-app-retained-call");

    f->app->controller = NULL;
    CHECK(mc_client_entity_renderer_pick_begin(f->px, f->math, 1, &f->call) ==
          MC_CLIENT_PICK_SOURCE_NULL_REFERENCE);
    CHECK(picking_source_null(f, 29) && !f->state.block_published && !f->state.target_cleared &&
          f->state.origin == NULL && f->app->y == NULL && f->px->fields.m == f->actors[2]);
    CHECK(mc_client_entity_renderer_pick_retire(&f->call) == MC_CLIENT_PICK_OK);
    f->app->controller = f->controller;
    puts("CONTROL source-null-controller");

    mc_entity *failed = NULL;
    CHECK(mc_local_player_allocate(f->base.entities, &failed, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(failed, NULL, NULL, NULL, 0, NULL, NULL) ==
              MC_PLAYER_NULL_REFERENCE &&
          failed->phase == MC_ENTITY_FAILED_CONSTRUCTION &&
          failed->local_player->fields.living_pc == 217);
    f->app->i = failed;
    CHECK(mc_client_entity_renderer_pick_begin(f->px, f->math, 1, &f->call) ==
          MC_CLIENT_PICK_SOURCE_NULL_REFERENCE);
    CHECK(picking_source_null(f, 47) && f->state.failure->child.pc == 50 &&
          strcmp(f->state.failure->child.class_name, "ls") == 0 && f->state.failure->ray != NULL &&
          f->state.failure->ray->eye != NULL && f->state.failure->ray->look != NULL &&
          f->state.failure->ray->end != NULL && f->state.failure->ray->world_call == NULL &&
          !f->state.block_published && f->app->y == NULL && f->px->fields.m == f->actors[2]);
    f->app->i = f->actors[0];
    mc_entity_release(failed); /* The retained ray keeps its exact failed DC alive. */
    CHECK(f->state.failure->ray->receiver != NULL &&
          f->state.failure->ray->receiver->phase == MC_ENTITY_FAILED_CONSTRUCTION);
    CHECK(mc_client_entity_renderer_pick_retire(&f->call) == MC_CLIENT_PICK_OK && f->call == NULL);
    puts("CONTROL source-null-ray-world");

    CHECK(mc_client_entity_renderer_pick_begin(f->px, NULL, 1, &f->call) == MC_CLIENT_PICK_STATE);
    CHECK(mc_client_entity_renderer_pick_inspect(f->call, &f->state) &&
          f->state.failure->native_policy && !f->state.source_returned &&
          f->state.source_pc == 47 && !f->state.block_published &&
          (f->state.failure->math == NULL || f->state.failure->math->status != MC_MATH_CLASS_OK));
    CHECK(mc_client_entity_renderer_pick_retire(&f->call) == MC_CLIENT_PICK_OK);
    puts("CONTROL native-null-math");

    CHECK(mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_bounds_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
    CHECK(picking_publish(f, 3));
    const size_t world_holds = f->base.world->persistent_bindings;
    CHECK(mc_client_entity_renderer_pick_begin(f->px, f->math, 1, &f->call) == MC_CLIENT_PICK_OK);
    CHECK(mc_client_entity_renderer_pick_inspect(f->call, &f->state) && f->state.source_returned &&
          f->state.block_published && f->state.target_cleared && f->state.query_returned &&
          f->state.entity_published && f->state.selected == f->actors[3] &&
          f->state.result == f->app->y && f->app->y != NULL &&
          f->base.world->persistent_bindings > world_holds);
    CHECK(mc_world_runtime_retire_entity_containers(f->base.world, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_client_entity_renderer_pick_detach(f->px, &f->detach) == MC_CLIENT_PICK_BUSY);
    f->external_result = f->app->y;
    CHECK(mc_entity_hit_result_retain(f->external_result, NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_client_entity_renderer_pick_retire(&f->call) == MC_CLIENT_PICK_OK && f->call == NULL &&
          f->app->y == f->external_result && f->px->fields.m == f->actors[3] &&
          f->base.world->persistent_bindings == world_holds);

    mc_entity_hit_result *foreign = NULL;
    CHECK(mc_entity_hit_result_allocate(f->base.entities, &foreign, NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_hit_result_construct_entity(foreign, f->actors[2], NULL) == MC_ENTITY_HIT_OK);
    f->app->y = foreign;
    const mc_client_pick_status wrong_y = mc_client_entity_renderer_pick_detach(f->px, &f->detach);
    f->app->y = f->external_result;
    CHECK(wrong_y == MC_CLIENT_PICK_BUSY && f->px->pick_y_occurrence == f->external_result &&
          f->px->fields.m == f->actors[3]);
    f->px->fields.m = f->actors[2];
    const mc_client_pick_status wrong_m = mc_client_entity_renderer_pick_detach(f->px, &f->detach);
    f->px->fields.m = f->actors[3];
    CHECK(wrong_m == MC_CLIENT_PICK_BUSY && f->app->y == f->external_result &&
          f->px->pick_m_occurrence == f->actors[3]);
    CHECK(mc_entity_hit_result_release(&foreign, NULL) == MC_ENTITY_HIT_OK && foreign == NULL);
    puts("CONTROL owned-source-field-replacement");

    CHECK(mc_client_entity_renderer_retain(f->px, &f->owner_error) == MC_CLIENT_ENTITY_RENDERER_OK);
    extra_px = f->px;
    CHECK(mc_client_entity_renderer_pick_detach(f->px, &f->detach) == MC_CLIENT_PICK_BUSY &&
          f->app->y == f->external_result && f->px->fields.m == f->actors[3]);
    CHECK(mc_client_entity_renderer_release(&extra_px, &f->owner_error) ==
              MC_CLIENT_ENTITY_RENDERER_OK &&
          extra_px == NULL);
    puts("CONTROL foreign-px-hold");

    mc_entity_hit_result_view held;
    CHECK(mc_entity_hit_result_inspect(f->external_result, &held) == MC_ENTITY_HIT_OK);
    mc_entity_vector_snapshot pool;
    CHECK(mc_entity_vector_get_snapshot(f->base.entities, &pool) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
    bool reused = false;
    for (size_t i = 0; i < pool.size && !reused; ++i) {
        mc_entity_vector *next = NULL;
        CHECK(mc_entity_vector_pool_get(f->base.entities, 91, 92, 93, &next, NULL) == MC_ENTITY_OK);
        reused = next == held.fields.f;
        CHECK(mc_entity_vector_release(f->base.entities, next) == MC_ENTITY_OK);
    }
    CHECK(reused && held.fields.f->x == 91 && held.fields.f->y == 92 && held.fields.f->z == 93);
    CHECK(mc_world_runtime_binding_retain(f->base.world));
    f->foreign_world_hold = true;
    CHECK(picking_case_close(f));
    puts("CONTROL retained-result-pool-and-world");
    return true;
}

static bool picking_close(picking_fixture *f) {
    fixture *b = &f->base;
    CHECK(mc_client_singleplayer_controller_free(&f->controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_entity_renderer_retire(&f->px, &f->owner_error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    CHECK(mc_client_render_engine_classes_release(f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_classes_free(f->models) == MC_CLIENT_MODEL_OK);
    mc_item_stack_release(f->stack);
    free(f->app);
    CHECK(mc_player_session_classes_free(&b->sessions, NULL) == MC_PLAYER_OK);
    mc_world_regions_free(b->regions);
    mc_world_info_name_release(b->name);
    CHECK(mc_world_runtime_classes_free(b->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_registry_classes_free(b->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(b->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(b->profile);
    mc_materials_destroy(b->materials);
    CHECK(mc_resource_files_free(b->files) == MC_REGISTRY_OK);
    CHECK(mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]))
        return 1;
    for (size_t i = 0; i < sizeof(case_names) / sizeof(*case_names); ++i) {
        char path[8192];
        const int length = snprintf(path, sizeof(path), "%s/case%02zu", argv[2], i);
        float partial = 0;
        if (length < 0 || (size_t)length >= sizeof(path) || !picking_case_open(f, path) ||
            !picking_prepare(f, i, &partial))
            return 1;
        f->before_hit = f->app->y;
        f->before_entity = f->px->fields.m;
        f->before_cursor = f->base.entities->vector_cursor;
        const mc_client_pick_status status =
            mc_client_entity_renderer_pick_begin(f->px, f->math, partial, &f->call);
        if (status != MC_CLIENT_PICK_OK) {
            if (mc_client_entity_renderer_pick_inspect(f->call, &f->state))
                fprintf(stderr, "Picker case%zu: status%d pc%u: %s\n", i, (int)status,
                        f->state.source_pc, f->state.failure->operation);
            return 1;
        }
        if (!mc_client_entity_renderer_pick_inspect(f->call, &f->state) ||
            !f->state.source_returned || !picking_emit(f, i) || !picking_case_close(f))
            return 1;
    }
    char controls[8192];
    const int length = snprintf(controls, sizeof(controls), "%s/controls", argv[2]);
    if (length < 0 || (size_t)length >= sizeof(controls) || !picking_controls(f, controls))
        return 1;
    if (!picking_close(f))
        return 1;
    free(f);
    printf("Prepared PX picking: %u checks; 26 original OS cases (PJ explicitly excluded)\n",
           checks);
    return 0;
}
