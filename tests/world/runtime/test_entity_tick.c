/* Genuine GAME World/Store/Chunk fixture; imported mains are never executed.
 * Source field controls are labelled below and restored before retirement. */
#define MC_PLAYER_ACTIVE_TICK_MAIN mc_world_tick_active_fixture_main
#include "../../entity/player/test_tick.c"
#undef MC_PLAYER_ACTIVE_TICK_MAIN
#include "../../../src/world/runtime/entity_publication_internal.h"
#include "../../../src/world/runtime/entity_tick_internal.h"
#include "../../../src/world/runtime/tile_entities/tick_internal.h"
#include "../../../src/world/runtime/tile_entities/tile_entities_internal.h"
#include "mc/entity/minecart/minecart.h"
#include "mc/world/runtime/entity_tick.h"
#include <fenv.h>
#include <math.h>

static bool tick_reference;
static const mc_registry_resources *tick_resources;
static mc_world_entity_tick_error *tick_error;
static mc_world_entity_tick_context tick_context;
static int32_t scenario;
static unsigned int removed_events;
static bool fail_removed, append_removed;
static mc_entity *append_actor;
static mc_world_chunk *tick_chunk;

static mc_world_storage_status tick_defer(mc_world_runtime *w, mc_entity *p) {
    const int32_t slice = p->base.bH;
    return mc_world_runtime_entity_batch_held(w, &tick_chunk->entities[(size_t)slice], true, NULL);
}

static mc_world_storage_status added_callback(void *context, mc_world_runtime *w, mc_entity *p,
                                              mc_world_storage_error *e) {
    (void)context;
    (void)w;
    (void)p;
    (void)e;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status removed_callback(void *context, mc_world_runtime *world,
                                                mc_entity *p, mc_world_storage_error *e) {
    (void)context;
    if (tick_reference)
        printf("EVENT %" PRId32 " %u R %d\n", scenario, removed_events, p == append_actor ? 1 : 0);
    ++removed_events;
    if (append_removed && removed_events == 1) {
        const mc_world_storage_status status = tick_defer(world, append_actor);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    if (fail_removed) {
        *e = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "authored removal failure", 0};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool tick_seed(fixture *f, size_t slot, int32_t x, int32_t z) {
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
    uint8_t *encoded = NULL;
    size_t length = 0, written = 0;
    const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                 MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    CHECK(r != NULL);
    r->root.type = MC_NBT_COMPOUND;
    r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[0].value.int_value = x;
    r->fields[1].value.int_value = z;
    r->fields[2].value.long_value = -123;
    r->fields[8].value.byte_value = 1;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3u].value.byte_array.data = r->arrays[i];
        r->fields[i + 3u].value.byte_array.length = lengths[i];
    }
    r->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), x, z, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool tick_setup(fixture *f, const char *root) {
    bool ok = false;
    const uint16_t title[] = {'T', 'i', 'c', 'k'};
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_java_random_profile_create(16u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 1, 1, 0) == MC_ITEM_STACK_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof language - 1};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options ro = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                    tick_resources == NULL ? &r->api : tick_resources};
    CHECK(mc_registry_classes_allocate(&ro, &f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    char path[8192];
    CHECK(snprintf(path, sizeof path, "%s/world0", root) > 0);
    uint8_t map[256];
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options so = {.session_token = 137,
                                 .block_map = map,
                                 .block_map_length = 256,
                                 .limits = mc_world_store_default_limits()};
    so.limits.max_loaded_chunks = 64;
    CHECK(mc_world_store_open(path, f->regions, &so, &f->stores[0], NULL) == MC_WORLD_STORAGE_OK);
    const mc_level_info info = {
        .seed = 23, .spawn_y = 64, .level_name = {title, COUNT(title)}, .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[0], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 64;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          &limits, &f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments a = {
        .store = f->stores[0], .name = f->name, .seed = -99, .dimension = f->dimensions[0]};
    CHECK(mc_world_runtime_construct_world(f->worlds[0], &a, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &f->providers[0], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[0] = true;
    CHECK(tick_seed(f, 0, 0, 0));
    CHECK(mc_world_runtime_get(f->worlds[0], 0, 0, &f->states[0]) == MC_CHUNK_ACQUIRE_OK);
    f->chunks[0] = mc_world_store_chunk_object(f->stores[0], f->states[0]);
    CHECK(f->chunks[0] != NULL);
    for (size_t i = 0; i < 8; ++i)
        CHECK(mc_world_chunk_slice_at(f->chunks[0]->m, i, &f->slices[0][i]) == MC_WORLD_STORAGE_OK);
    tick_chunk = f->chunks[0];
    tick_context.player.travel.code_registry = f->registry[0];
    tick_context.player.travel.trig = &trig;
    ok = true;
cleanup:
    return ok;
}

static bool tick_presence(fixture *f) {
    bool ok = false;
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z)
            if (x != 0 || z != 0) {
                mc_chunk_state *state = NULL;
                CHECK(tick_seed(f, 0, x, z));
                CHECK(mc_world_runtime_get(f->worlds[0], x, z, &state) == MC_CHUNK_ACQUIRE_OK &&
                      state != NULL && state->active);
            }
    ok = true;
cleanup:
    return ok;
}

static void world_tick_row(mc_world_runtime *w, mc_entity *p, bool complete) {
    if (!tick_reference)
        return;
    printf("STATE %" PRId32 " %d %zu %zu %zu %d %d %d %" PRId32 " %" PRId32 " %" PRId32, scenario,
           complete ? 1 : 0, w->fields.b->count, w->fields.D->count, w->fields.e->count,
           w->fields.L ? 1 : 0, p->base.be ? 1 : 0, p->base.bF ? 1 : 0, p->base.bG, p->base.bH,
           p->base.bI);
    for (size_t i = 0; i < 8; ++i)
        printf(" %zu", ((mc_world_chunk *)mc_world_store_chunk_object(
                            w->fields.w, w->lifecycle.cache->insertions[0]))
                           ->entities[i]
                           .count);
    for (size_t i = 0; i < 3; ++i) {
        const double v = i == 0 ? p->base.x : i == 1 ? p->base.y : p->base.z;
        printf(" %016" PRIx64, movement_double_bits(v));
    }
    for (size_t i = 0; i < 3; ++i) {
        const double v = i == 0 ? p->base.bl : i == 1 ? p->base.bm : p->base.bn;
        printf(" %016" PRIx64, movement_double_bits(v));
    }
    printf(" %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %" PRId32 " %" PRId32 " %u\n",
           movement_float_bits(p->base.yaw), movement_float_bits(p->base.aT),
           movement_float_bits(p->base.aU), movement_float_bits(p->base.aV), p->base.bt, p->item.b,
           removed_events);
}

static bool tick_clear(fixture *f) {
    bool ok = false;
    wt_scope *s = calloc(1, sizeof *s);
    CHECK(s != NULL);
    s->world = f->worlds[0];
    s->error.max_loop_work = 1048576;
    s->error.max_temporary_bytes = 16u * 1024u * 1024u;
    CHECK(wt_list_clear(s, f->worlds[0]->fields.b, 0) &&
          wt_list_clear(s, f->worlds[0]->fields.D, 0) &&
          wt_list_clear(s, f->worlds[0]->fields.e, 0));
    for (size_t i = 0; i < 8; ++i)
        while (f->chunks[0]->entities[i].count != 0) {
            mc_entity *p = f->chunks[0]->entities[i].items[0];
            CHECK(mc_world_chunk_remove_entity(f->chunks[0], p, (int32_t)i, NULL, 0, NULL) ==
                  MC_WORLD_STORAGE_OK);
        }
    ok = true;
cleanup:
    free(s);
    return ok;
}

static bool world_tick_cases(fixture *f, mc_entity *p, mc_entity *q) {
    bool ok = false, added = false;
    mc_world_access *access = NULL;
    const mc_world_access_callbacks cb = {.entity_added = added_callback,
                                          .entity_removed = removed_callback};
    CHECK(mc_world_access_create(&cb, NULL, &access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_add_world_access(f->worlds[0], access, NULL) == MC_WORLD_STORAGE_OK);
    added = true;
    mc_world_runtime *w = f->worlds[0];
    append_actor = q;
    for (scenario = 0; scenario < 21; ++scenario) {
        CHECK(tick_clear(f));
        removed_events = 0;
        fail_removed = append_removed = false;
        CHECK(movement_input(p, 8.5, 8.5, 8.5));
        p->base.offset = .125f;
        p->base.bl = -11;
        p->base.bm = -12;
        p->base.bn = -13;
        p->base.aU = -14;
        p->base.aV = -15;
        p->base.yaw = 25;
        p->base.aT = -17;
        p->base.bt = 17;
        p->item.b = p->item.e = 0;
        p->item.c = 0;
        p->base.bF = false;
        p->base.bG = p->base.bH = p->base.bI = 0;
        p->base.velocity_x = p->base.velocity_y = p->base.velocity_z = 0;
        p->base.bq = true;
        mc_java_random_reseed(p->random, 38);
        q->base.be = false;
        q->base.bF = false;
        q->base.bG = q->base.bH = q->base.bI = 0;
        bool complete = true;
        if (scenario < 13) {
            if (scenario == 1)
                p->base.y = -.25;
            if (scenario == 2)
                p->base.y = 128.0;
            if (scenario == 3)
                p->base.y = -32.0;
            if (scenario == 4)
                p->base.x = 48.0;
            if (scenario == 5)
                p->base.x = 16.0;
            if (scenario == 6)
                p->base.x = -16.0;
            if (scenario == 7)
                p->base.x = 2147483648.0;
            if (scenario == 8) {
                p->base.x = NAN;
                p->base.y = INFINITY;
                p->base.z = NAN;
                p->base.yaw = NAN;
                p->base.aT = INFINITY;
            }
            if (scenario == 9) {
                p->base.bF = true;
                p->base.bH = 7;
                CHECK(mc_world_chunk_insert_entity(f->chunks[0], p, NULL) == MC_WORLD_STORAGE_OK);
                p->base.y = 33.0;
            }
            if (scenario == 10) {
                CHECK(mc_world_chunk_insert_entity(f->chunks[0], p, NULL) == MC_WORLD_STORAGE_OK);
                p->base.be = true;
            }
            if (scenario == 11) {
                CHECK(mc_world_chunk_insert_entity(f->chunks[0], p, NULL) == MC_WORLD_STORAGE_OK);
                p->base.x = 48.0;
            }
            if (scenario == 12) {
                p->base.bF = true;
                p->base.bG = 20;
                p->base.bH = -1;
                p->base.bI = 20;
            }
            const mc_world_storage_status update =
                mc_world_runtime_update_entity(w, p, false, &tick_context, NULL, tick_error);
            if (scenario == 7) {
                CHECK(update == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                      tick_error->update_bytecode == 408);
                CHECK(p->base.bF);
                continue; /* Native known wrong-location diagnostic frontier. */
            }
            CHECK(update == MC_WORLD_STORAGE_OK);
        } else if (scenario == 13) {
            CHECK(mc_world_runtime_tick_entity(w, p, &tick_context, NULL, tick_error) ==
                  MC_WORLD_STORAGE_OK);
        } else {
            if (scenario == 14)
                CHECK(mc_world_runtime_add_weather_entity(w, p, &added, NULL) ==
                          MC_WORLD_STORAGE_OK &&
                      added);
            if (scenario == 14)
                p->base.be = true;
            if (scenario >= 15) {
                CHECK(mc_world_runtime_insert_entity(w, p, &added, NULL) == MC_WORLD_STORAGE_OK &&
                      added);
                if (scenario != 20)
                    p->base.be = true;
            }
            if (scenario >= 16 && scenario <= 19)
                CHECK(tick_defer(w, p) == MC_WORLD_STORAGE_OK);
            if (scenario == 17)
                CHECK(tick_defer(w, p) == MC_WORLD_STORAGE_OK);
            if (scenario == 18)
                fail_removed = true;
            if (scenario == 19) {
                append_removed = true;
                CHECK(mc_entity_set_position(q, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
                CHECK(mc_world_chunk_insert_entity(f->chunks[0], q, NULL) == MC_WORLD_STORAGE_OK);
            }
            const mc_world_storage_status st =
                mc_world_runtime_tick_entities(w, &tick_context, NULL, tick_error);
            complete = st == MC_WORLD_STORAGE_OK;
            CHECK(scenario == 18 ? st == MC_WORLD_STORAGE_IO_ERROR : st == MC_WORLD_STORAGE_OK);
        }
        world_tick_row(w, p, complete);
    }
    CHECK(tick_clear(f));
    /* Real presence changes the wrapper from an early return into Item.w_. */
    CHECK(tick_presence(f));
    for (scenario = 21; scenario < 24; ++scenario) {
        CHECK(movement_input(p, 8.5, 8.5, 8.5));
        p->base.offset = .125f;
        p->base.bq = true;
        p->base.bt = 17;
        p->item.b = 0;
        p->base.velocity_x = p->base.velocity_y = p->base.velocity_z = 0;
        p->base.bF = false;
        CHECK(mc_world_chunk_insert_entity(f->chunks[0], p, NULL) == MC_WORLD_STORAGE_OK);
        if (scenario == 22)
            p->base.be = true;
        if (scenario == 23)
            p->item.b = 5999;
        CHECK(mc_world_runtime_tick_entity(w, p, &tick_context, NULL, tick_error) ==
              MC_WORLD_STORAGE_OK);
        CHECK(p->base.bt == 18 && p->item.b == (scenario == 23 ? 6000 : 1));
        world_tick_row(w, p, true);
        CHECK(tick_clear(f));
    }
    CHECK(mc_world_runtime_update_entity(w, NULL, false, NULL, NULL, tick_error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          tick_error->update_bytecode == 1);
    mc_world_entity_tick_limits zero = {0};
    CHECK(mc_world_runtime_add_weather_entity(w, p, &added, NULL) == MC_WORLD_STORAGE_OK);
    const size_t holds = p->holds;
    CHECK(mc_world_runtime_tick_entities(w, &tick_context, &zero, tick_error) ==
              MC_WORLD_STORAGE_LIMIT &&
          tick_error->loop_bytecode == 7 && p->holds == holds);
    CHECK(tick_clear(f));
    ok = true;
cleanup:
    if (added) {
        bool removed = false;
        (void)mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL);
    }
    (void)mc_world_access_free(access, NULL);
    return ok;
}

static bool world_riding_records(fixture *f, mc_entity *p, mc_entity *q) {
    bool ok = false;
    mc_entity *falling = NULL, *cart = NULL;
    CHECK(mc_falling_entity_allocate(f->entity_classes[0], &falling, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct(falling, f->worlds[0], 8.5, 8.5, 8.5, 1, NULL) ==
              MC_ENTITY_OK);
    CHECK(mc_minecart_entity_allocate(f->entity_classes[0], &cart, NULL) == MC_ENTITY_OK &&
          mc_minecart_entity_construct(cart, f->worlds[0], 8.5, 8.5, 8.5, 0, NULL) == MC_ENTITY_OK);
    for (int scene = 0; scene < 7; ++scene) {
        CHECK(tick_clear(f));
        CHECK(movement_input(p, 8.5, 8.5, 8.5) && movement_input(q, 8.5, 8.5, 8.5));
        p->base.offset = q->base.offset = .125f;
        p->base.bq = q->base.bq = true;
        p->base.velocity_x = p->base.velocity_y = p->base.velocity_z = 0;
        q->base.velocity_x = q->base.velocity_y = q->base.velocity_z = 0;
        p->base.yaw = q->base.yaw = 25;
        p->base.aT = q->base.aT = -17;
        p->base.bt = q->base.bt = 17;
        p->item.b = q->item.b = 0;
        p->base.private_d = p->base.private_e = 0;
        q->base.private_e = 370;
        q->base.private_d = -370;
        mc_entity *mount = scene == 4 ? falling : scene == 5 ? cart : p;
        if (scene >= 4) {
            CHECK(mc_entity_set_size(mount, 1, 1) == MC_ENTITY_OK &&
                  mc_entity_set_position(mount, 8.5, 8.5, 8.5) == MC_ENTITY_OK);
            mount->base.aU = mount->base.yaw;
            mount->base.aV = mount->base.aT;
        }
        mount->base.aG = q;
        q->base.aH = mount;
        CHECK(mc_world_chunk_insert_entity(f->chunks[0], p, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_chunk_insert_entity(f->chunks[0], q, NULL) == MC_WORLD_STORAGE_OK);
        if (scene == 6) {
            mount->base.yaw = NAN;
            mount->base.aT = NAN;
        }
        if (scene == 1)
            p->item.b = 5999;
        if (scene == 2)
            q->base.be = true;
        if (scene == 3)
            q->base.aH = NULL;
        mc_entity *actors[] = {p, q, mount};
        mc_world_entity_tick_context c = tick_context;
        c.relationship_actors = actors;
        c.relationship_actor_count = COUNT(actors);
        CHECK(mc_world_runtime_tick_entity(f->worlds[0], scene >= 4 ? q : p, &c, NULL,
                                           tick_error) == MC_WORLD_STORAGE_OK);
        if (tick_reference) {
            printf("RIDE %d %d %d %d %d %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32, scene,
                   mount->base.be ? 1 : 0, q->base.be ? 1 : 0, mount->base.aG == q,
                   q->base.aH == mount, mount->base.bt, q->base.bt, scene >= 4 ? 0 : p->item.b,
                   q->item.b);
            printf(" %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64,
                   movement_double_bits(mount->base.y), movement_double_bits(q->base.x),
                   movement_double_bits(q->base.y), movement_double_bits(q->base.z));
            printf(" %08" PRIx32 " %08" PRIx32 " %016" PRIx64 " %016" PRIx64 "\n",
                   movement_float_bits(q->base.yaw), movement_float_bits(q->base.aT),
                   movement_double_bits(q->base.private_e),
                   movement_double_bits(q->base.private_d));
        }
        mount->base.aG = NULL;
        q->base.aH = NULL;
    }
    CHECK(tick_clear(f));
    ok = true;
cleanup:
    p->base.aG = p->base.aH = q->base.aG = q->base.aH = NULL;
    if (falling != NULL)
        falling->base.aG = NULL;
    if (cart != NULL)
        cart->base.aG = NULL;
    mc_entity_release(falling);
    mc_entity_release(cart);
    return ok;
}

static bool world_native_ownership(fixture *f, mc_entity *p, mc_entity *q) {
    bool ok = false, inserted = false;
    mc_world_runtime *w = f->worlds[0];
    const int32_t old_id = q->base.id;
    mc_entity *null_world = NULL;
    wt_scope *scope = calloc(1, sizeof *scope);
    CHECK(scope != NULL);
    scope->world = w;
    scope->error.max_loop_work = 1048576;
    scope->error.max_temporary_bytes = 16u * 1024u * 1024u;
    CHECK(tick_clear(f));
    CHECK(mc_world_runtime_tick_entities(w, NULL, NULL, tick_error) == MC_WORLD_STORAGE_OK);
    CHECK(tick_error->weather_index == -1 && tick_error->child == MC_WORLD_ENTITY_TICK_CHILD_NONE);
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr((csr & ~0x6000u) | 0x6000u);
    const mc_world_storage_status empty_fp =
        mc_world_runtime_tick_entities(w, NULL, NULL, tick_error);
    const double old_backup = p->base.bl;
    const mc_world_storage_status bad_fp =
        mc_world_runtime_update_entity(w, p, false, NULL, NULL, tick_error);
    _mm_setcsr(csr);
    CHECK(empty_fp == MC_WORLD_STORAGE_OK && bad_fp == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          p->base.bl == old_backup && tick_error->update_bytecode == 4);
    /* Distinct real owners with equal source IDs: first-equals removal retires
     * that actual occurrence, never the argument's independent hold. */
    CHECK(movement_input(p, 8.5, 8.5, 8.5) && movement_input(q, 8.5, 8.5, 8.5));
    q->base.id = p->base.id;
    CHECK(mc_world_chunk_insert_entity(f->chunks[0], p, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_chunk_insert_entity(f->chunks[0], q, NULL) == MC_WORLD_STORAGE_OK);
    size_t ph = p->holds, qh = q->holds;
    CHECK(mc_world_chunk_remove_entity(f->chunks[0], q, 0, NULL, 0, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(p->holds == ph - 1 && q->holds == qh && f->chunks[0]->entities[0].count == 1 &&
          f->chunks[0]->entities[0].items[0] == q);
    q->base.id = old_id;
    CHECK(tick_clear(f));
    /* Source List alias removeAll(self) on the actual captured loaded owner. */
    CHECK(mc_world_runtime_insert_entity(w, p, &inserted, NULL) == MC_WORLD_STORAGE_OK && inserted);
    CHECK(mc_world_runtime_insert_entity(w, q, &inserted, NULL) == MC_WORLD_STORAGE_OK && inserted);
    mc_world_reference_list *deferred = w->fields.D;
    w->fields.D = w->fields.b;
    const mc_world_storage_status self =
        mc_world_runtime_tick_entities(w, &tick_context, NULL, tick_error);
    w->fields.D = deferred;
    CHECK(self == MC_WORLD_STORAGE_OK && w->fields.b->count == 0);
    CHECK(tick_clear(f));
    /* The first deferred Chunk.b call is139 (138 only loads the Entity).
     * Spend one comparison removing b, then fail in actual Chunk search30. */
    CHECK(mc_world_runtime_insert_entity(w, p, &inserted, NULL) == MC_WORLD_STORAGE_OK && inserted);
    CHECK(tick_defer(w, p) == MC_WORLD_STORAGE_OK);
    const mc_world_entity_tick_limits deferred_limit = {2, 4096, 16u * 1024u * 1024u};
    CHECK(mc_world_runtime_tick_entities(w, &tick_context, &deferred_limit, tick_error) ==
          MC_WORLD_STORAGE_LIMIT);
    CHECK(tick_error->loop_bytecode == 139 && tick_error->chunk_bytecode == 30 &&
          tick_error->chunk.bytecode == 30 && w->fields.b->count == 0 && w->fields.D->count == 1 &&
          f->chunks[0]->entities[0].count == 1);
    CHECK(tick_clear(f));
    /* Native bounded batchRemove commits its selected prefix and retains the
     * pending tail. Actual D contains only p, and no source callback is faked. */
    CHECK(mc_world_runtime_insert_entity(w, p, &inserted, NULL) == MC_WORLD_STORAGE_OK && inserted);
    CHECK(mc_entity_set_position(q, 8.5, 24.5, 8.5) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_insert_entity(w, q, &inserted, NULL) == MC_WORLD_STORAGE_OK && inserted);
    CHECK(tick_defer(w, p) == MC_WORLD_STORAGE_OK);
    ph = p->holds;
    qh = q->holds;
    const mc_world_entity_tick_limits one = {1, 4096, 16u * 1024u * 1024u};
    CHECK(mc_world_runtime_tick_entities(w, &tick_context, &one, tick_error) ==
          MC_WORLD_STORAGE_LIMIT);
    CHECK(w->fields.b->count == 1 && w->fields.b->items[0] == q && w->fields.D->count == 1 &&
          p->holds == ph - 1 && q->holds == qh);
    CHECK(tick_clear(f));
    CHECK(mc_world_runtime_add_weather_entity(w, NULL, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted);
    CHECK(mc_world_runtime_tick_entities(w, &tick_context, NULL, tick_error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          tick_error->loop_bytecode == 30 && w->fields.e->count == 1);
    CHECK(tick_clear(f));
    /* The nullable Entity.world is independent of the outer actual World.
     * flag=false can relocate it; a later true tick fails inside genuine sn.U. */
    CHECK(make_entity(f, 0, NULL, 8.5, 8.5, 8.5, &null_world));
    CHECK(mc_world_runtime_update_entity(w, null_world, false, &tick_context, NULL, tick_error) ==
              MC_WORLD_STORAGE_OK &&
          null_world->base.world == NULL && null_world->base.bF);
    const mc_entity_bounds bounds = *null_world->bounds;
    CHECK(mc_world_runtime_tick_entity(w, null_world, &tick_context, NULL, tick_error) !=
          MC_WORLD_STORAGE_OK);
    CHECK(tick_error->child == MC_WORLD_ENTITY_TICK_CHILD_ITEM &&
          tick_error->update_bytecode == 121 &&
          memcmp(&bounds, null_world->bounds, sizeof bounds) == 0);
    CHECK(tick_clear(f));
    CHECK(wt_chunk_remove(scope, &w->fallback, NULL, true, INT32_MIN, 0));
    /* Real riding Item body on a genuine two-way association. The explicit
     * independently-held declaration protects raw links without owning cycles. */
    CHECK(movement_input(p, 8.5, 8.5, 8.5) && movement_input(q, 8.5, 8.5, 8.5));
    p->item.b = q->item.b = 0;
    p->base.bq = q->base.bq = true;
    p->base.velocity_x = p->base.velocity_y = p->base.velocity_z = 0;
    q->base.velocity_x = q->base.velocity_y = q->base.velocity_z = 0;
    p->base.aG = q;
    q->base.aH = p;
    CHECK(mc_world_chunk_insert_entity(f->chunks[0], p, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_insert_entity(f->chunks[0], q, NULL) == MC_WORLD_STORAGE_OK);
    mc_entity *actors[] = {p, q};
    mc_world_entity_tick_context relationships = tick_context;
    relationships.relationship_actors = actors;
    relationships.relationship_actor_count = COUNT(actors);
    ph = p->holds;
    qh = q->holds;
    CHECK(mc_world_runtime_tick_entity(w, p, &relationships, NULL, tick_error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(p->holds == ph && q->holds == qh && q->base.y == p->base.y + .75 && q->base.aH == p &&
          p->base.aG == q);
    const mc_world_entity_tick_limits no_temporary = {1048576, 4096, 0};
    CHECK(mc_world_runtime_tick_entity(w, p, &relationships, &no_temporary, tick_error) ==
              MC_WORLD_STORAGE_LIMIT &&
          tick_error->update_bytecode == 479 && tick_error->passenger_calls == 0);
    CHECK(p->holds == ph && q->holds == qh);
    CHECK(mc_world_runtime_tick_entity(w, p, &tick_context, NULL, tick_error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          tick_error->relationship_bytecode == 441);
    CHECK(p->holds == ph && q->holds == qh);
    /* Genuine cyclic association follows source calls until the pending native
     * passenger bound; no C recursion or early successful truncation. */
    q->base.aG = p;
    p->base.aH = q;
    const mc_world_entity_tick_limits cyclic = {1048576, 9, 16u * 1024u * 1024u};
    CHECK(mc_world_runtime_tick_entity(w, p, &relationships, &cyclic, tick_error) ==
              MC_WORLD_STORAGE_LIMIT &&
          tick_error->passenger_calls == 9 && tick_error->update_bytecode == 479);
    CHECK(p->holds == ph && q->holds == qh);
    p->base.aG = p->base.aH = q->base.aG = q->base.aH = NULL;
    CHECK(tick_clear(f));
    /* Real deep chain retains all ancestor source locals through its tail. */
    mc_entity *chain[20];
    for (size_t i = 0; i < COUNT(chain); ++i) {
        CHECK(make_entity(f, 0, w, 8.5, 8.5, 8.5, &chain[i]));
        CHECK(movement_input(chain[i], 8.5, 8.5, 8.5));
        chain[i]->base.bq = true;
        chain[i]->base.velocity_x = chain[i]->base.velocity_y = chain[i]->base.velocity_z = 0;
        CHECK(mc_world_chunk_insert_entity(f->chunks[0], chain[i], NULL) == MC_WORLD_STORAGE_OK);
        if (i != 0) {
            chain[i - 1]->base.aG = chain[i];
            chain[i]->base.aH = chain[i - 1];
        }
    }
    relationships.relationship_actors = chain;
    relationships.relationship_actor_count = COUNT(chain);
    size_t holds[20];
    for (size_t i = 0; i < COUNT(chain); ++i)
        holds[i] = chain[i]->holds;
    CHECK(mc_world_runtime_tick_entity(w, chain[0], &relationships, NULL, tick_error) ==
          MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < COUNT(chain); ++i) {
        CHECK(chain[i]->holds == holds[i]);
        chain[i]->base.aG = chain[i]->base.aH = NULL;
    }
    CHECK(tick_clear(f));
    ok = true;
cleanup:
    p->base.aG = p->base.aH = q->base.aG = q->base.aH = NULL;
    q->base.id = old_id;
    free(scope);
    return ok;
}

static bool world_player_and_tiles(fixture *f, client_application *app, const char *root) {
    bool ok = false, inserted = false;
    mc_entity *player = NULL;
    mc_movement_input *input = NULL;
    mc_tile_entity *chest = NULL, *spawner = NULL, *piston = NULL;
    wt_scope *scope = NULL;
    mc_entity *nearest = NULL;
    mc_world_runtime *w = f->worlds[0];
    CHECK(app_setup(f, app, root));
    CHECK(mc_local_player_allocate(app->entities, &player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(player, w, app, app->session, 0, NULL, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_create_from_options(app->settings, &input, NULL) ==
          MC_MOVEMENT_INPUT_OK);
    CHECK(reset_tick(f, player, input));
    CHECK(mc_world_runtime_insert_entity(w, player, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted);
    /* Full World.g -> World.f -> actual dc.w_ chain with actual writer/Input. */
    for (size_t i = 0; i < 32; ++i) {
        CHECK(mc_world_runtime_tick_entities(w, &tick_context, NULL, tick_error) ==
              MC_WORLD_STORAGE_OK);
        CHECK(player->base.bt == 18 + (int32_t)i && !w->fields.L);
    }
    CHECK(w->fields.b->count == 1 && w->fields.d->count == 1);
    /* Nearest uses genuine lowercase d; dead Players are deliberately included. */
    scope = calloc(1, sizeof *scope);
    CHECK(scope != NULL);
    scope->world = w;
    scope->error.max_loop_work = 1048576;
    scope->error.max_temporary_bytes = 16u * 1024u * 1024u;
    CHECK(mc_entity_set_position(player, 16.5, .5, .5) == MC_ENTITY_OK);
    player->base.be = true;
    CHECK(wt_nearest(scope, w, .5, .5, .5, 16, &nearest) && nearest == NULL);
    CHECK(wt_nearest(scope, w, .5, .5, .5, -1, &nearest) && nearest == player);
    mc_entity_release(nearest);
    nearest = NULL;
    CHECK(wt_nearest(scope, w, .5, .5, .5, NAN, &nearest) && nearest == NULL);
    CHECK(mc_entity_set_position(player, 8.5, 8.5, 8.5) == MC_ENTITY_OK);
    /* Add actual three concrete TileEntity owners through actual Chunk/World
     * installation. Fixture Blocks are authored source input, not tick results. */
    CHECK(mc_live_tile_entities_new(w, MC_TILE_ENTITY_CHEST, &chest, NULL) == MC_WORLD_STORAGE_OK);
    f->states[0]->data->blocks[boxes_index(5, 8, 5)] = 54;
    CHECK(mc_live_tile_entities_set(w, 5, 8, 5, chest, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_live_tile_entities_new(w, MC_TILE_ENTITY_SPAWNER, &spawner, NULL) ==
          MC_WORLD_STORAGE_OK);
    f->states[0]->data->blocks[boxes_index(6, 8, 6)] = 52;
    CHECK(mc_live_tile_entities_set(w, 6, 8, 6, spawner, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_tile_entity_allocate(w, MC_TILE_ENTITY_PISTON, &piston) == MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_piston_construct(piston, 1, 0, 0, false, false, NULL) ==
          MC_TILE_ENTITY_OK);
    f->states[0]->data->blocks[boxes_index(7, 8, 7)] = 36;
    CHECK(mc_live_tile_entities_set(w, 7, 8, 7, piston, NULL) == MC_WORLD_STORAGE_OK);
    /* Direct active phase avoids source loaded-dead removal while isolating
     * the actual dead-Player nearest predicate and positive-delay Spawner. */
    const int32_t before = spawner->state.spawner.delay;
    CHECK(wt_tile_active(scope));
    CHECK(!w->fields.L && spawner->state.spawner.delay == before - 1 && !chest->invalid);
    CHECK(mc_tile_entity_set_invalid(chest, true) == MC_TILE_ENTITY_OK);
    CHECK(wt_tile_active(scope) && !w->fields.L);
    CHECK(w->fields.c->count == 2 && chest->invalid);
    /* A reached real factory leaves L=true and all prior rotation/draws intact. */
    spawner->state.spawner.delay = 0;
    CHECK(!wt_tile_active(scope) && scope->error.spawner_bytecode == 223 && w->fields.L);
    scope->error.status = MC_WORLD_STORAGE_OK;
    scope->error.operation = NULL;
    w->fields.B = true;
    CHECK(wt_tile_active(scope) && !w->fields.L && spawner->state.spawner.delay == 0);
    w->fields.B = false;
    /* A real captured iterator continues to use its original List when c is
     * replaced by the independently owned G descriptor. No copied array. */
    mc_live_tile_iterator captured;
    mc_live_tile_entity_error captured_error;
    CHECK(mc_live_tile_iterator_begin_held(w, w->fields.c, &captured, &captured_error));
    mc_world_reference_list *active = w->fields.c;
    w->fields.c = w->fields.G;
    bool captured_has = false;
    mc_tile_entity *captured_tile = NULL;
    const bool captured_ok =
        mc_live_tile_iterator_has_next_held(w, &captured, &captured_has, &captured_error) &&
        captured_has &&
        mc_live_tile_iterator_next_held(w, &captured, &captured_tile, &captured_error);
    w->fields.c = active;
    CHECK(captured_ok && captured.identity == active && captured_tile != NULL);
    mc_tile_entity_release(captured_tile);
    /* Iterator hasNext does not perform a fail-fast check; next does. */
    mc_live_tile_iterator it;
    mc_live_tile_entity_error child;
    bool has = false;
    CHECK(mc_live_tile_iterator_begin_held(w, w->fields.c, &it, &child));
    const int32_t mod = w->tile_entity_membership[0].mod_count;
    w->tile_entity_membership[0].mod_count = mc_java_i32_add(mod, 1);
    CHECK(mc_live_tile_iterator_has_next_held(w, &it, &has, &child) && has);
    mc_tile_entity *next = NULL;
    CHECK(!mc_live_tile_iterator_next_held(w, &it, &next, &child) && child.bytecode == 373);
    CHECK(!mc_live_tile_iterator_remove_held(w, &it, &child) && child.bytecode == 401);
    w->tile_entity_membership[0].mod_count = mod;
    free(scope);
    scope = NULL;
    ok = true;
cleanup:
    free(scope);
    mc_entity_release(nearest);
    (void)mc_movement_input_retire(&input, NULL);
    mc_tile_entity_release(chest);
    mc_tile_entity_release(spawner);
    mc_tile_entity_release(piston);
    mc_entity_release(player);
    return ok;
}

typedef struct spawner_observation {
    int scene, event;
    bool emit, poison_fp;
} spawner_observation;

static mc_world_storage_status spawner_particle(void *context, mc_world_runtime *w,
                                                const mc_registry_string *name, double x, double y,
                                                double z, double vx, double vy, double vz,
                                                mc_world_storage_error *error) {
    spawner_observation *o = context;
    (void)w;
    (void)vx;
    (void)vy;
    (void)vz;
    (void)error;
    if (o->emit && tick_reference) {
        printf("TP %d %d ", o->scene, o->event);
        for (size_t i = 0; i < name->length; ++i)
            putchar((int)name->units[i]);
        printf(" %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n", movement_double_bits(x),
               movement_double_bits(y), movement_double_bits(z));
    }
    ++o->event;
    if (o->poison_fp && o->event == 2)
        _mm_setcsr((_mm_getcsr() & ~0x6000u) | 0x6000u);
    return MC_WORLD_STORAGE_OK;
}

static bool world_spawner_records(fixture *f) {
    bool ok = false, registered = false;
    mc_tile_entity *tile = NULL;
    mc_world_access *access = NULL;
    mc_world_runtime *w = f->worlds[0];
    mc_world_reference_list *players = w->fields.d;
    spawner_observation observation = {0};
    const mc_world_access_callbacks callbacks = {.particle = spawner_particle};
    CHECK(tick_clear(f));
    CHECK(mc_live_tile_entities_get(w, 6, 8, 6, &tile, NULL) == MC_WORLD_STORAGE_OK &&
          tile != NULL && tile->kind == MC_TILE_ENTITY_SPAWNER);
    CHECK(mc_world_access_create(&callbacks, &observation, &access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_add_world_access(w, access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    for (int scene = 0; scene < 7; ++scene) {
        observation = (spawner_observation){.scene = scene, .emit = true};
        tile->state.spawner.rotation = scene == 4 || scene == 5 ? NAN : 7;
        tile->state.spawner.previous_rotation = -5;
        tile->state.spawner.delay = scene == 1 || scene == 5 ? 0 : scene == 3 ? -1 : 20;
        w->fields.B = scene == 1 || scene == 5;
        mc_java_random_reseed(w->fields.r, 38);
        if (scene == 0)
            w->fields.d = w->fields.D;
        if (scene == 6)
            CHECK(mc_tile_entity_bind_world(tile, NULL) == MC_TILE_ENTITY_OK);
        const mc_world_storage_status status =
            mc_world_runtime_tick_entities(w, &tick_context, NULL, tick_error);
        const size_t player_count = w->fields.d->count;
        w->fields.d = players;
        CHECK(scene == 6 ? status == MC_WORLD_STORAGE_UNSUPPORTED_STATE
                         : status == MC_WORLD_STORAGE_OK);
        if (scene == 6)
            CHECK(w->fields.L && tick_error->spawner_bytecode == 9 &&
                  tick_error->tile_bytecode == 34 && tick_error->nearest_bytecode == 0);
        const uint64_t state = w->fields.r->state;
        const int32_t continuation = mc_java_random_next_i32(w->fields.r);
        if (tick_reference)
            printf("TILE %d %d %d %" PRId32 " %016" PRIx64 " %016" PRIx64 " %012" PRIx64 " %" PRId32
                   " %zu %zu\n",
                   scene, status == MC_WORLD_STORAGE_OK, w->fields.L ? 1 : 0,
                   tile->state.spawner.delay, movement_double_bits(tile->state.spawner.rotation),
                   movement_double_bits(tile->state.spawner.previous_rotation), state, continuation,
                   w->fields.c->count, player_count);
        if (scene == 6)
            CHECK(mc_tile_entity_bind_world(tile, w) == MC_TILE_ENTITY_OK);
    }
    observation.emit = false;
    observation.poison_fp = true;
    observation.event = 0;
    w->fields.B = false;
    tile->state.spawner.delay = 20;
    tile->state.spawner.rotation = 7;
    tile->state.spawner.previous_rotation = -5;
    mc_java_random_reseed(w->fields.r, 38);
    mc_java_random expected;
    mc_java_random_seed(&expected, 38);
    for (size_t i = 0; i < 3; ++i)
        (void)mc_java_random_next_float(&expected);
    const unsigned int saved_csr = _mm_getcsr();
    const mc_world_storage_status fp_status =
        mc_world_runtime_tick_entities(w, &tick_context, NULL, tick_error);
    _mm_setcsr(saved_csr);
    CHECK(fp_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && tick_error->spawner_bytecode == 114 &&
          tick_error->loop_bytecode == 390);
    CHECK(observation.event == 2 && w->fields.L && tile->state.spawner.previous_rotation == 7 &&
          tile->state.spawner.rotation == 7 && tile->state.spawner.delay == 20 &&
          w->fields.r->state == expected.state);
    observation.poison_fp = false;
    w->fields.B = false;
    tile->state.spawner.delay = 20;
    CHECK(mc_world_runtime_tick_entities(w, &tick_context, NULL, tick_error) ==
              MC_WORLD_STORAGE_OK &&
          !w->fields.L);
    ok = true;
cleanup:
    w->fields.d = players;
    if (tile != NULL && tile->world == NULL)
        (void)mc_tile_entity_bind_world(tile, w);
    if (registered) {
        bool removed = false;
        (void)mc_world_runtime_remove_world_access(w, access, &removed, NULL);
    }
    (void)mc_world_access_free(access, NULL);
    mc_tile_entity_release(tile);
    return ok;
}

static bool world_server_admission(fixture *f, const char *root) {
    bool ok = false;
    mc_entity *entity = NULL;
    resources *r = &f->sources[1];
    r->streams[0] = (stream){.bytes = language, .length = sizeof language - 1};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_SERVER, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[1], NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_SERVER, f->profile, &trig, &f->classes[1],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[1], f->registry[1], NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_SERVER, f->profile, NULL, &f->entity_classes[1]) ==
          MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->classes[1], f->entity_classes[1]) ==
          MC_ENTITY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry[1], NULL) == MC_REGISTRY_OK);
    CHECK(boxes_open_store(f, 1, root) && boxes_create_world(f, 1));
    CHECK(make_entity(f, 1, f->worlds[1], 8.5, 8.5, 8.5, &entity));
    mc_world_runtime *w = f->worlds[1];
    const size_t holds = entity->holds, bindings = w->persistent_bindings, active = w->active_calls;
    const uint64_t random = w->fields.r->state, entity_random = entity->random->state;
    const int32_t next_id = entity->classes->next_id, ticks = entity->base.bt;
    const double backup = entity->base.bl, x = entity->base.x;
    const mc_world_storage_error previous = w->last_error;
    for (int method = 0; method < 4; ++method) {
        mc_world_entity_tick_error_init(tick_error);
        tick_error->operation = "previous attempt";
        const mc_world_storage_status status =
            method == 0 ? mc_world_runtime_tick_entity(w, entity, &tick_context, NULL, tick_error)
            : method == 1
                ? mc_world_runtime_update_entity(w, entity, false, &tick_context, NULL, tick_error)
            : method == 2 ? mc_world_runtime_tick_entities(w, &tick_context, NULL, tick_error)
                          : mc_world_runtime_tick_entities(w, NULL, NULL, NULL);
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        if (method != 3)
            CHECK(tick_error->bytecode == 0 && tick_error->update_bytecode == 0 &&
                  tick_error->source_class == NULL &&
                  tick_error->child == MC_WORLD_ENTITY_TICK_CHILD_NONE &&
                  tick_error->weather_index == -1);
        CHECK(w->active_calls == active && w->persistent_bindings == bindings &&
              entity->holds == holds && entity->classes->next_id == next_id);
        CHECK(w->fields.r->state == random && entity->random->state == entity_random &&
              entity->base.bt == ticks && entity->base.x == x && entity->base.bl == backup);
        CHECK(w->last_error.status == previous.status &&
              w->last_error.operation == previous.operation &&
              w->last_error.system_error == previous.system_error);
        CHECK(w->fields.b->count == 0 && w->fields.e->count == 0 && !w->fields.L &&
              f->chunks[1]->entities[0].count == 0);
    }
    ok = true;
cleanup:
    return ok;
}

static bool tick_cleanup(fixture *f) {
    if (f != NULL && f->worlds[0] != NULL && f->bound[0]) {
        mc_chunk_cache *cache = f->providers[0].cache;
        if (cache->insertion_count > 64)
            return false;
        mc_chunk_state *states[64];
        const size_t count = cache->insertion_count;
        if (count != 0)
            memcpy(states, cache->insertions, count * sizeof *states);
        if (mc_world_runtime_retire_entity_containers(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->terminal[0] = true;
        if (!retire_descriptors(f, 0) ||
            mc_world_runtime_retire_world(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->world_retired[0] = true;
        for (size_t i = 0; i < count; ++i)
            if (mc_world_store_retire_chunk(f->stores[0], states[i], NULL) != MC_WORLD_STORAGE_OK)
                return false;
        f->states[0] = NULL;
        f->chunks[0] = NULL;
    }
    return cleanup_fixture(f);
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_entity *p = NULL, *q = NULL;
    client_application *app = NULL;
    char root[8192], resource_root[8192];
    if (argc != 4 ||
        (strcmp(argv[1], "world-entity-tick") != 0 &&
         strcmp(argv[1], "world-entity-tick-reference") != 0) ||
        !decode_path(argv[2], root, sizeof root) ||
        !decode_path(argv[3], resource_root, sizeof resource_root) || !mc_math_trig_init(&trig))
        return 2;
    tick_reference = strcmp(argv[1], "world-entity-tick-reference") == 0;
    f = calloc(1, sizeof *f);
    tick_error = malloc(sizeof *tick_error);
    CHECK(f != NULL && tick_error != NULL);
    app = calloc(1, sizeof *app);
    CHECK(app != NULL);
    CHECK(mc_resource_files_create(resource_root, &app->files) == MC_REGISTRY_OK);
    tick_resources = mc_resource_files_source(app->files);
    CHECK(tick_setup(f, root));
    CHECK(make_entity(f, 0, f->worlds[0], 8.5, 8.5, 8.5, &p));
    CHECK(make_entity(f, 0, f->worlds[0], 8.5, 8.5, 8.5, &q));
    CHECK(world_tick_cases(f, p, q));
    CHECK(world_riding_records(f, p, q));
    CHECK(world_native_ownership(f, p, q));
    CHECK(world_player_and_tiles(f, app, root));
    CHECK(world_spawner_records(f));
    CHECK(fflush(stdout) ==
          0); /* Keep earlier complete observer rows ahead of direct initializer stdout. */
    CHECK(world_server_admission(f, root));
    result = 0;
cleanup:
    free(tick_error);
    if (app != NULL) {
        if (mc_client_application_retire_statistics(app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
            mc_client_application_retire_water_effect(app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
            mc_client_achievement_notice_free(&app->achievement_notice, NULL) !=
                MC_CLIENT_NOTICE_OK ||
            (app->render_classes != NULL &&
             mc_client_render_classes_free(app->render_classes) != MC_CLIENT_MODEL_OK))
            return 1;
    }
    if (!tick_cleanup(f))
        return 1;
    if (app != NULL) {
        if (mc_game_settings_retire(&app->settings, NULL) != MC_GAME_SETTINGS_OK)
            return 1;
        mc_client_game_directory_free(app->game_directory);
        mc_resource_files_free(app->files);
        free(app);
    }
    if (result == 0 && printf("World Entity tick: %u checks\n", checks) < 0)
        return 1;
    return result;
}
