/* Genuine production storage and actual saved-Item acquisition callbacks.
 * The included finite fixture also remains independently executable. */
#define main mc_camera_finite_fixture_main
#include "test_first_person.c"
#undef main
#include "../../../src/client/application_state.h"
#include "../../../src/client/particles/application.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "mc/entity/movement/material.h"
#include "mc/entity/nbt/storage.h"
#include "mc/storage/nbt/nbt.h"
#include "mc/world/runtime/world_access.h"

#ifdef MC_CAMERA_OLD_MATERIAL
mc_entity_status mc_entity_inside_material_old(mc_entity *, mc_material *, bool *,
                                               mc_entity_material_error *);
#endif

typedef struct record_input {
    mc_nbt_tag root, level, fields[11];
    const mc_nbt_tag *root_fields[1], *level_fields[11];
    uint16_t names[12][20];
    uint8_t arrays[5][MC_CHUNK_BLOCKS];
} record_input;

static mc_nbt_string ascii_name(const char *text, uint16_t *units) {
    const size_t n = strlen(text);
    for (size_t i = 0; i < n; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, n};
}

/* Only the supplied record bytes are authored. Its Item tag comes from the
 * real Item constructor/serializer; the real region loader/factory activates it. */
static bool stored_item(fixture *f, int id, int metadata) {
    record_input *r = calloc(1, sizeof(*r));
    mc_item_stack *stack = NULL;
    mc_entity *item = NULL;
    mc_nbt_tree *tree = NULL;
    uint8_t *encoded = NULL;
    bool serialized = false;
    size_t length = 0, written = 0;
    const mc_nbt_tag *entities[1];
    const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                 MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    CHECK(r != NULL && mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 1, 1, 0) == MC_ITEM_STACK_OK &&
          mc_item_entity_allocate(f->entities, &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(item, f->world, 8.5, 64, 8.5, stack, NULL) == MC_ENTITY_OK &&
          mc_entity_save_nbt(item, NULL, &serialized, &tree, NULL) == MC_ENTITY_NBT_OK &&
          serialized);
    entities[0] = mc_nbt_tree_root(tree);
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii_name("Level", r->names[11]);
    for (size_t i = 0; i < 11; ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii_name(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[8].value.byte_value = 1;
    for (size_t i = 0; i < 5; ++i) {
        r->fields[i + 3].value.byte_array.data = r->arrays[i];
        r->fields[i + 3].value.byte_array.length = lengths[i];
    }
    r->arrays[0][8u * 2048u + 8u * 128u + 64u] = (uint8_t)id;
    CHECK(mc_chunk_nibbles_set(r->arrays[1], MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, metadata));
    r->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[9].value.list.items = entities;
    r->fields[9].value.list.length = 1;
    r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = 11;
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->store), 0, 0, encoded, length,
                                 UINT32_C(0xfedcba98), &growth, NULL) == MC_WORLD_STORAGE_OK);
    free(encoded);
    mc_nbt_tree_free(tree);
    mc_entity_release(item);
    mc_item_stack_release(stack);
    free(r);
    return true;
}

static bool extra_world(fixture *owner, const char *directory, fixture *f) {
    f->profile = owner->profile;
    f->registry = owner->registry;
    f->entities = owner->entities;
    f->world_classes = owner->world_classes;
    f->regions = owner->regions;
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options options = {.session_token = 6381,
                                            .block_map = map,
                                            .block_map_length = 256,
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, f->regions, &options, &f->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->store, .name = owner->name, .seed = 5143, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

/* Explicit terminal native ownership retirement, not source unload/save. */
static bool retire_scene_world(fixture *f) {
    mc_chunk_state *states[2];
    mc_chunk_cache *cache = f->world->options.cache;
    const size_t count = cache->insertion_count;
    CHECK(count <= 2 && f->world->active_calls == 0 && f->world->local_leases == 0);
    for (size_t i = 0; i < count; ++i)
        states[i] = cache->insertions[i];
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_pending_clear(f->world->options.pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                              cache->insertion_capacity));
    mc_world_provider_unbind(f->dimension);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < count; ++i)
        CHECK(mc_world_store_retire_chunk(f->store, states[i], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    f->world = NULL;
    mc_world_provider_release(f->dimension);
    CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

typedef struct observer {
    client_application *app;
    mc_entity *player;
    mc_game_settings *replacement;
    mc_world_runtime *new_world;
    int action;
    unsigned calls;
    bool valid;
    size_t saved_bindings;
    mc_world_runtime *limited_world;
} observer;

static mc_world_storage_status added(void *context, mc_world_runtime *world, mc_entity *entity,
                                     mc_world_storage_error *error) {
    observer *o = context;
    ++o->calls;
    mc_item_stack *stack = NULL;
    o->valid = o->calls == 1 && mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
               mc_item_entity_stack_get(entity, &stack) == MC_ENTITY_OK && stack != NULL &&
               world->fields.b->count == 1 && world->fields.b->items[0] == entity;
    mc_item_stack_release(stack); /* Getter returns one independent native hold. */
    if (!o->valid) {
        *error = (mc_world_storage_error){.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          .operation = "camera actual Item callback"};
        return error->status;
    }
    if (o->action == 0 || o->action == 1) {
        mc_game_settings_state state;
        if (!mc_game_settings_read_state(o->app->settings, &state))
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        state.values.anaglyph = false;
        state.values.view_bobbing = true;
        state.values.debug_camera = true;
        if (o->action == 1)
            o->app->settings = o->replacement;
        if (mc_game_settings_set_values(o->app->settings, &state.values, NULL) !=
            MC_GAME_SETTINGS_OK)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    } else if (o->action == 2) {
        if (mc_client_application_publish_world(o->app, o->new_world) != MC_EFFECT_RENDERER_OK)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    } else if (o->action == 3) {
        o->app->settings = NULL;
    } else if (o->action == 4) {
        _mm_setcsr((_mm_getcsr() & ~0x6000u) | 0x2000u);
    } else if (o->action == 5 || o->action == 7) {
        /* Authored source-field assignment, with real native binding holds. */
        if (o->new_world != NULL && !mc_world_runtime_binding_retain(o->new_world))
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        mc_world_runtime *old = o->player->base.world;
        o->player->base.world = o->new_world;
        mc_world_runtime_binding_release(old);
    } else if (o->action == 6) {
        if (mc_client_application_publish_world(o->app, o->new_world) != MC_EFFECT_RENDERER_OK)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    if (o->action == 6 || o->action == 7) {
        o->limited_world = o->new_world;
        o->saved_bindings = o->new_world->persistent_bindings;
        o->new_world->persistent_bindings = SIZE_MAX; /* Explicit native counter fault. */
    }
    return MC_WORLD_STORAGE_OK;
}

static bool has(const mc_client_camera_frame *frame, const char *method, unsigned pc) {
    for (size_t i = 0; i < frame->operation_count; ++i)
        if (frame->operations[i].pc == pc && strcmp(frame->operations[i].method, method) == 0)
            return true;
    return false;
}

static bool scenario(const char *resources, const char *directory, int which) {
    fixture f = {0}, b = {0}, c = {0};
    mc_entity *p = NULL;
    mc_game_settings *settings = NULL, *replacement = NULL;
    mc_world_access *access = NULL;
    client_application *app = calloc(1, sizeof(*app));
    char path[4096];
    CHECK(app != NULL && setup(&f, resources, directory) && make_player(&f, &p));
    CHECK(mc_entity_set_position(p, 8.5, 64.2, 8.5) == MC_ENTITY_OK);
    p->base.aJ = p->base.x;
    p->base.aK = p->base.y;
    p->base.aL = p->base.z;
    p->base.yaw = p->base.aU = 33;
    p->base.aT = p->base.aV = 12;
    p->local_player->fields.living.Y = 20;
    p->local_player->fields.living.aa = 0;
    app->profile = f.profile;
    app->registry = f.registry;
    app->entities = f.entities;
    app->world_classes = f.world_classes;
    app->trig = &trig;
    app->display_width = 854;
    app->display_height = 480;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK &&
          mc_game_settings_create_defaults(NULL, &replacement, NULL) == MC_GAME_SETTINGS_OK);
    app->settings = settings;
    mc_game_settings_state state;
    CHECK(mc_game_settings_read_state(settings, &state));
    state.values.anaglyph = true;
    state.values.view_bobbing = false;
    state.values.debug_camera = false;
    CHECK(mc_game_settings_set_values(settings, &state.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_application_publish_world(app, f.world) == MC_EFFECT_RENDERER_OK);
    const bool sleeping = (which >= 2 && which <= 5) || which == 11 || which == 12 || which == 14;
    p->local_player->fields.player.u = sleeping;
    if (sleeping || which == 8 || which == 9 || which == 15) {
        snprintf(path, sizeof(path), "%s-B", directory);
        CHECK(extra_world(&f, path, &b));
        CHECK(stored_item(&f, 0, 0));
        int32_t air = -1;
        CHECK(mc_world_runtime_block_get(f.world, 8, 64, 8, &air, NULL) == MC_WORLD_STORAGE_OK &&
              air == 0);
    } else
        CHECK(stored_item(&f, which == 7 || which == 10 ? 8 : 0, 0));
    fixture *watched = &f;
    observer o = {
        .app = app, .player = p, .replacement = replacement, .action = which == 1 ? 1 : 0};
    if (sleeping) {
        watched = &b;
        CHECK(stored_item(&b, which == 12 ? 1 : 26, 0));
        CHECK(mc_client_application_publish_world(app, b.world) == MC_EFFECT_RENDERER_OK);
        o.action = which == 4 ? 3 : which == 5 || which == 12 ? 4 : 2;
        if (which == 2 || which == 11 || which == 14) {
            snprintf(path, sizeof(path), "%s-C", directory);
            CHECK(extra_world(&f, path, &c) && stored_item(&c, 26, 3));
            o.new_world = c.world;
            if (which == 14)
                o.action = 6;
        }
    } else if (which == 6 || which == 7 || which == 10 || which == 13)
        o.action = 4;
    else if (which == 8 || which == 9 || which == 15) {
        /* Fresh block receiver must be cold, while new metadata receiver is ready. */
        watched = &b;
        CHECK(stored_item(&b, 8, 0));
        mc_world_runtime_binding_release(p->base.world);
        CHECK(mc_world_runtime_binding_retain(b.world));
        p->base.world = b.world;
        o.action = which == 15 ? 7 : 5;
        o.new_world = which == 8 || which == 15 ? f.world : NULL;
        f.world->options.cache->insertions[0]->data->blocks[8u * 2048u + 8u * 128u + 64u] = 8;
        CHECK(mc_chunk_nibbles_set(f.world->options.cache->insertions[0]->data->metadata,
                                   MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, 7));
    }
    if (which == 13)
        p->local_player->fields.living.Y = 0;
    observer second = {.app = app, .player = p, .action = which == 11 ? 4 : 2};
    mc_world_access *second_access = NULL;
    if (c.world != NULL) {
        const mc_world_access_callbacks second_callbacks = {.entity_added = added};
        CHECK(mc_world_access_create(&second_callbacks, &second, &second_access, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(c.world, second_access, NULL) ==
                  MC_WORLD_STORAGE_OK);
    }
    const mc_world_access_callbacks callbacks = {.entity_added = added};
    CHECK(mc_world_access_create(&callbacks, &o, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(watched->world, access, NULL) == MC_WORLD_STORAGE_OK);
    const size_t bindings[3] = {f.world->persistent_bindings,
                                b.world == NULL ? 0 : b.world->persistent_bindings,
                                c.world == NULL ? 0 : c.world->persistent_bindings};
    mc_client_camera_frame frame = {0};
    mc_client_camera_error error;
    mc_client_camera_error_init(&error);
    const unsigned csr = _mm_getcsr();
    mc_client_camera_status status;
    bool immersion_out = false;
    mc_entity_status direct = MC_ENTITY_OK;
    if (which == 10) {
        mc_material *water = NULL;
        CHECK(mc_registry_material_get(f.registry, MC_MATERIAL_g, &water, NULL) == MC_REGISTRY_OK);
#ifdef MC_CAMERA_OLD_MATERIAL
        direct = mc_entity_inside_material_old(p, water, &immersion_out, &error.material);
#else
        direct = mc_entity_inside_material(p, water, &immersion_out, &error.material);
#endif
        status = MC_CLIENT_CAMERA_UNSUPPORTED_FP;
    } else {
#ifdef MC_CAMERA_OLD_BODY
        status = mc_client_first_person_camera(p, app->settings, app->active_world, f.registry,
                                               &trig, 854, 480, .5f, 0, &frame, &error);
#else
        status = mc_client_first_person_camera_for_application(app, p, f.registry, &trig, .5f, 0,
                                                               &frame, &error);
#endif
    }
    if (o.limited_world != NULL)
        o.limited_world->persistent_bindings = o.saved_bindings;
    _mm_setcsr(csr); /* Test owns this restoration; production never resets FP. */
    CHECK(o.calls == 1 && o.valid);
    if (which <= 1) {
        CHECK(status == MC_CLIENT_CAMERA_OK && frame.complete);
        CHECK(has(&frame, "a", 55) && !has(&frame, "a", 196) && has(&frame, "f", 112) &&
              !has(&frame, "g", 756));
    } else if (which == 2) {
        CHECK(status == MC_CLIENT_CAMERA_OK && frame.complete && has(&frame, "g", 223));
        bool angle = false;
        for (size_t i = 0; i < frame.operation_count; ++i)
            if (frame.operations[i].pc == 223)
                angle = frame.operations[i].arguments[0] == 270;
        CHECK(angle && app->active_world == NULL && second.calls == 1 && second.valid);
    } else if (which == 3)
        CHECK(status == MC_CLIENT_CAMERA_NULL_REFERENCE && error.pc == 203 &&
              has(&frame, "g", 115) && !has(&frame, "g", 223));
    else if (which == 4)
        CHECK(status == MC_CLIENT_CAMERA_NULL_REFERENCE && error.pc == 731 &&
              has(&frame, "g", 249) && has(&frame, "g", 272) && !has(&frame, "g", 787));
    else if (which == 5)
        CHECK(status == MC_CLIENT_CAMERA_UNSUPPORTED_FP && error.pc == 185 &&
              has(&frame, "g", 115) && !has(&frame, "g", 223));
    else if (which == 6)
        CHECK(status == MC_CLIENT_CAMERA_UNSUPPORTED_FP && error.pc == 62 &&
              strcmp(error.method, "d") == 0 && has(&frame, "a", 55) && !has(&frame, "a", 160));
    else if (which == 7)
        CHECK(status == MC_CLIENT_CAMERA_UNSUPPORTED_FP && error.pc == 15 &&
              error.material.bytecode == 85 && !has(&frame, "a", 160));
    else if (which == 8)
        CHECK(status == MC_CLIENT_CAMERA_OK && frame.vertical_fov == 70);
    else if (which == 9)
        CHECK(status == MC_CLIENT_CAMERA_MATERIAL_FAILED && error.material.bytecode == 82);
    else if (which == 10)
        CHECK(direct == MC_ENTITY_UNSUPPORTED_FP && error.material.bytecode == 85 &&
              !immersion_out);
    else if (which == 11)
        CHECK(status == MC_CLIENT_CAMERA_UNSUPPORTED_FP && error.pc == 219 && second.calls == 1 &&
              second.valid && !has(&frame, "g", 223));
    else if (which == 12)
        CHECK(status == MC_CLIENT_CAMERA_UNSUPPORTED_FP && error.pc == 238 &&
              has(&frame, "g", 115) && !has(&frame, "g", 249));
    else if (which == 13)
        CHECK(status == MC_CLIENT_CAMERA_UNSUPPORTED_FP && error.pc == 35 &&
              strcmp(error.method, "d") == 0 && has(&frame, "a", 55) && !has(&frame, "a", 160));
    else if (which == 14)
        CHECK(status == MC_CLIENT_CAMERA_OWNER_FAILED && error.pc == 179 &&
              error.world.status == MC_WORLD_STORAGE_LIMIT && has(&frame, "g", 115) &&
              !has(&frame, "g", 223));
    else if (which == 15)
        CHECK(status == MC_CLIENT_CAMERA_MATERIAL_FAILED &&
              error.material.status == MC_ENTITY_LIMIT && error.material.bytecode == 82 &&
              error.material.world.status == MC_WORLD_STORAGE_LIMIT && has(&frame, "a", 55) &&
              !has(&frame, "a", 160));
    CHECK(f.world->active_calls == 0 && f.world->local_leases == 0 &&
          (b.world == NULL || (b.world->active_calls == 0 && b.world->local_leases == 0)));
    /* Account only for declared field replacements; no temporary receiver hold leaked. */
    CHECK(f.world->persistent_bindings ==
          bindings[0] + (which <= 1 || which == 6 || which == 7 || which == 8 || which == 10 ||
                                 which == 13 || which == 15
                             ? 1u
                             : 0u));
    if (b.world != NULL)
        CHECK(b.world->persistent_bindings ==
              bindings[1] + (which == 4 || which == 5 || which == 12 ? 1u : 0u));
    if (c.world != NULL)
        CHECK(c.world->persistent_bindings == bindings[2] + (which == 11 ? 2u : 1u));
    bool removed = false;
    if (second_access != NULL) {
        CHECK(mc_world_runtime_remove_world_access(c.world, second_access, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed);
        CHECK(mc_world_access_free(second_access, NULL) == MC_WORLD_STORAGE_OK);
    }
    CHECK(mc_world_runtime_remove_world_access(watched->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    CHECK(mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_client_application_publish_world(app, NULL) == MC_EFFECT_RENDERER_OK);
    mc_entity_release(p);
    app->settings = NULL;
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK &&
          mc_game_settings_retire(&replacement, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_player_session_classes_free(&f.sessions, NULL) == MC_PLAYER_OK);
    if (c.world != NULL)
        CHECK(retire_scene_world(&c));
    if (b.world != NULL)
        CHECK(retire_scene_world(&b));
    CHECK(retire_scene_world(&f));
    mc_world_regions_free(f.regions);
    mc_world_info_name_release(f.name);
    CHECK(mc_world_runtime_classes_free(f.world_classes, NULL) == MC_WORLD_STORAGE_OK &&
          mc_registry_classes_free(f.registry, NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_free(f.entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f.profile);
    mc_materials_destroy(f.materials);
    mc_resource_files_free(f.files);
    free(app);
    if (which <= 4) {
        printf("CBPROJECTION %d", which);
        for (size_t i = 0; i < 16; ++i)
            printf(" %08x", (unsigned)bits(frame.projection[i]));
        puts("");
        printf("CBMODELVIEW %d", which);
        for (size_t i = 0; i < 16; ++i)
            printf(" %08x", (unsigned)bits(frame.modelview[i]));
        puts("");
    }
    printf("CALLBACK %d %d %u %zu\n", which, (int)status, error.pc, frame.operation_count);
    CHECK(fflush(stdout) == 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("Camera callback input: resources world-directory\n", stderr);
        return 2;
    }
    if (!mc_math_trig_init(&trig))
        return 1;
    char path[4096];
#ifdef MC_CAMERA_OLD_MATERIAL
    const int begin = 10, end = 11;
#else
    const int begin = 0, end = 16;
#endif
    for (int which = begin; which < end; ++which) {
        snprintf(path, sizeof(path), "%s-%d", argv[2], which);
        if (!scenario(argv[1], path, which))
            return 1;
    }
    printf("Camera callbacks: %u checks\n", checks);
    return 0;
}
