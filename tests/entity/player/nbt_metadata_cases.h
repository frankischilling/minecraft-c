/* Included after the real App/World fixture and authored NBT observation tools.
 * This is test code only; every successful simulation operation is production. */
#include "../../../src/world/runtime/saving/player_metadata_internal.h"
#include "mc/world/runtime/players.h"

typedef struct nbt_metadata_fixture {
    mc_world_store *store;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_world_info_name *name;
    mc_entity *player, *second;
    mc_chunk_provider_fields provider;
    bool bound, terminal, retired;
} nbt_metadata_fixture;

static bool nbt_metadata_close_world(nbt_metadata_fixture *m) {
    bool ok = false;
    if (m->world == NULL)
        return true;
    if (m->bound && !m->terminal) {
        CHECK(mc_world_runtime_retire_entity_containers(m->world, NULL) == MC_WORLD_STORAGE_OK);
        m->terminal = true;
    }
    mc_entity_release(m->second);
    m->second = NULL;
    mc_entity_release(m->player);
    m->player = NULL;
    if (m->bound) {
        CHECK(mc_chunk_pending_clear(m->provider.pending) == MC_CHUNK_ACQUIRE_OK &&
              mc_chunk_cache_init(m->provider.cache, m->provider.cache->entries,
                                  m->provider.cache->capacity, m->provider.cache->insertions,
                                  m->provider.cache->insertion_capacity));
        mc_world_provider_unbind(m->dimension);
    }
    if (!m->retired) {
        CHECK(mc_world_runtime_retire_world(m->world, NULL) == MC_WORLD_STORAGE_OK);
        m->retired = true;
    }
    while (mc_world_store_chunk_count(m->store) != 0) {
        mc_chunk_state *state = NULL;
        CHECK(mc_world_store_chunk_at(m->store, 0, &state, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_store_retire_chunk(m->store, state, NULL) == MC_WORLD_STORAGE_OK);
    }
    CHECK(mc_world_runtime_free(m->world, NULL) == MC_WORLD_STORAGE_OK);
    m->world = NULL;
    mc_world_provider_release(m->dimension);
    m->dimension = NULL;
    m->bound = m->terminal = m->retired = false;
    ok = true;
cleanup:
    return ok;
}

static bool nbt_metadata_construct(fixture *f, nbt_metadata_fixture *m) {
    bool ok = false;
    CHECK(mc_world_provider_create(0, NULL, &m->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 512;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          &limits, &m->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = m->store, .name = m->name, .seed = 23, .dimension = m->dimension};
    mc_world_storage_error error;
    const mc_world_storage_status status =
        mc_world_runtime_construct_world(m->world, &args, &error);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "NBT World construction: %d %s\n", (int)status,
                error.operation == NULL ? "-" : error.operation);
    CHECK(status == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(m->world->fields.v, &m->provider, NULL) ==
              MC_WORLD_STORAGE_OK);
    m->bound = true;
    ok = true;
cleanup:
    return ok;
}

static const mc_nbt_tag *nbt_named(const mc_nbt_tag *root, const char *name) {
    uint16_t key[32];
    if (strlen(name) > COUNT(key))
        return NULL;
    for (size_t i = 0; i < strlen(name); ++i)
        key[i] = (uint16_t)(unsigned char)name[i];
    return mc_nbt_compound_get(root, key, strlen(name));
}

static mc_world_storage_status nbt_metadata_build(mc_world_runtime *w, mc_world_info_record **out,
                                                  mc_world_player_metadata_error *error) {
    mc_world_storage_error cause;
    const mc_world_storage_status entered = mc_world_runtime_call_enter(w, &cause);
    if (entered != MC_WORLD_STORAGE_OK)
        return entered;
    const mc_world_storage_status status = mc_world_runtime_build_player_metadata(w, out, error);
    mc_world_runtime_call_leave(w);
    return status;
}

static bool nbt_metadata_clock_record(nbt_metadata_fixture *m, const char *name,
                                      mc_world_player_metadata_error *error) {
    bool ok = false;
    mc_world_info_record *record = NULL;
    int64_t before = 0, after = 0;
    CHECK(mc_platform_current_time_millis(&before) == MC_PLATFORM_MILLIS_OK &&
          nbt_metadata_build(m->world, &record, error) == MC_WORLD_STORAGE_OK &&
          mc_platform_current_time_millis(&after) == MC_PLATFORM_MILLIS_OK);
    const mc_nbt_tag *root = mc_world_info_record_root(record);
    const mc_nbt_tag *last = nbt_named(root, "LastPlayed");
    CHECK(last != NULL && last->type == MC_NBT_LONG && last->value.long_value >= before &&
          last->value.long_value <= after && nbt_tags(name, "data", "$", root, 0));
    printf("CLOCK %s 1\n", name);
    ok = true;
cleanup:
    mc_world_info_record_release(record);
    return ok;
}

static bool nbt_composition(fixture *f, client_application *app, nbt_input *a,
                            mc_entity_nbt_error *entity_error) {
    bool ok = false, first_null = false;
    nbt_metadata_fixture m = {0};
    mc_world_runtime *original_app_world = app->active_world;
    mc_world_info *saved = NULL;
    mc_world_info_player *opaque = NULL;
    mc_world_runtime_lifecycle_error *save_error = calloc(1, sizeof(*save_error));
    mc_world_player_metadata_error *metadata_error = calloc(1, sizeof(*metadata_error));
    mc_world_player_attachment_error *attach_error = calloc(1, sizeof(*attach_error));
    mc_nbt_tree *tree = NULL;
    mc_inventory_player *second_inventory = NULL, *first_inventory = NULL;
    char path[8192];
    const int length =
        snprintf(path, sizeof path, "%s/../fresh", mc_world_store_root(f->stores[0]));
    uint8_t map[256];
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 137,
                                            .block_map = map,
                                            .block_map_length = sizeof map,
                                            .limits = mc_world_store_default_limits()};
    const uint16_t fresh_name[] = {'f', 'r', 'e', 's', 'h'};
    CHECK(save_error != NULL && metadata_error != NULL && attach_error != NULL && length > 0 &&
          (size_t)length < sizeof path &&
          mc_world_store_open(path, f->regions, &options, &m.store, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_info_name_copy((mc_nbt_string){fresh_name, COUNT(fresh_name)}, SIZE_MAX,
                                  &m.name) == MC_WORLD_INFO_OK);
    /* No level.dat is authored here. Source LOAD_OR_CREATE must itself obtain
     * s=true, perform initial spawn and construct its real provider/Chunk owners. */
    CHECK(nbt_metadata_construct(f, &m) && m.world->fields.s);
    app->active_world = m.world;
    CHECK(mc_local_player_allocate(app->entities, &m.player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(m.player, m.world, app, app->session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          nbt_prepare(app, m.player, "full_save", a, entity_error) &&
          mc_world_runtime_attach_player(m.world, m.player, attach_error) == MC_WORLD_STORAGE_OK &&
          m.world->fields.d->count == 1 && m.world->fields.d->items[0] == m.player);
    mc_java_random_reseed(m.player->random, 38);
    mc_java_random_reseed(m.world->fields.r, 38);
    const mc_java_random er = *m.player->random, wr = *m.world->fields.r;
    int64_t before = 0, after = 0;
    CHECK(mc_platform_current_time_millis(&before) == MC_PLATFORM_MILLIS_OK &&
          mc_world_runtime_save_detailed(m.world, true, save_error) == MC_CHUNK_LIFECYCLE_OK &&
          mc_platform_current_time_millis(&after) == MC_PLATFORM_MILLIS_OK &&
          mc_world_store_load_world_info(m.store, MC_WORLD_INFO_CLIENT, &saved, NULL) ==
              MC_WORLD_STORAGE_OK);
    mc_world_info_fields saved_fields;
    CHECK(mc_world_info_fields_get(saved, &saved_fields) == MC_WORLD_INFO_OK &&
          saved_fields.player != NULL && saved_fields.value.version == 19132 &&
          saved_fields.value.last_played >= before && saved_fields.value.last_played <= after &&
          nbt_random_equal(&er, m.player->random) && nbt_random_equal(&wr, m.world->fields.r) &&
          nbt_tags("full_save", "player", "$", mc_world_info_player_root(saved_fields.player), 0));
    printf("SAVE fresh 1 %zu %" PRId32 " %" PRId32 " %" PRId32 " 1\n", m.world->fields.d->count,
           m.player->local_player->fields.player.m, m.player->local_player->fields.player.g,
           saved_fields.value.version);
    mc_world_info_release(saved);
    saved = NULL;

    /* Retained Info.Player is a genuine independent immutable owner. The live
     * List selector must omit it for an empty or selected-null input. */
    memset(a, 0, sizeof(*a));
    nbt_input_node *opaque_root = nbt_node(a, "", MC_NBT_COMPOUND);
    CHECK(nbt_scalar(a, opaque_root, "Dimension", MC_NBT_INT, 99) &&
          nbt_scalar(a, opaque_root, "Marker", MC_NBT_INT, 7) &&
          mc_world_info_player_copy(&opaque_root->tag, NULL, &opaque, NULL) == MC_WORLD_INFO_OK &&
          mc_world_info_player_set(m.world->fields.x, opaque) == MC_WORLD_INFO_OK);
    /* Explicit authored mutable List input on the actual constructor-created
     * array and typed occurrence ledger. Original held occurrence is restored
     * before any failure assertion/teardown; no replacement List is fabricated. */
    m.world->fields.d->count = m.world->player_membership.count = 0;
    const bool empty_ok = nbt_metadata_clock_record(&m, "select_empty", metadata_error);
    m.world->fields.d->count = m.world->player_membership.count = 1;
    CHECK(empty_ok);
    m.world->fields.d->items[0] = NULL;
    m.world->player_membership.entries[0] = NULL;
    first_null = true;
    const bool null_ok = nbt_metadata_clock_record(&m, "select_null", metadata_error);
    m.world->fields.d->items[0] = m.player;
    m.world->player_membership.entries[0] = m.player;
    first_null = false;
    CHECK(null_ok);
    bool inserted = false;
    CHECK(mc_local_player_allocate(app->entities, &m.second, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(m.second, m.world, app, app->session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          mc_world_runtime_insert_entity(m.world, m.second, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted && m.world->fields.d->count == 2);
    second_inventory = m.second->local_player->fields.player.c;
    m.second->local_player->fields.player.c = NULL;
    const bool first_only = nbt_metadata_clock_record(&m, "select_first_only", metadata_error);
    m.second->local_player->fields.player.c = second_inventory;
    second_inventory = NULL;
    CHECK(first_only);
    mc_world_info_player *retained = NULL;
    CHECK(mc_world_info_player_get(m.world->fields.x, &retained) == MC_WORLD_INFO_OK &&
          retained == opaque);
    printf("SELECTION 3 1\n");

    /* Actual full-save entry failure retains its exact selected writer child
     * and earlier version store. It must not reach transport or clear Player. */
    CHECK(mc_world_info_version_set(m.world->fields.x, 7) == MC_WORLD_INFO_OK);
    first_inventory = m.player->local_player->fields.player.c;
    m.player->local_player->fields.player.c = NULL;
    const mc_chunk_lifecycle_status failed =
        mc_world_runtime_save_detailed(m.world, true, save_error);
    m.player->local_player->fields.player.c = first_inventory;
    first_inventory = NULL;
    mc_world_info_fields current;
    CHECK(failed != MC_CHUNK_LIFECYCLE_OK && save_error->save.metadata.list_bytecode == 49 &&
          save_error->save.metadata.entity.status != MC_ENTITY_NBT_OK &&
          save_error->save.metadata.entity.base_bytecode == 145 &&
          save_error->save.metadata.entity.player_bytecode == 19 &&
          mc_world_info_fields_get(m.world->fields.x, &current) == MC_WORLD_INFO_OK &&
          current.value.version == 19132 && current.player == opaque &&
          mc_world_store_load_world_info(m.store, MC_WORLD_INFO_CLIENT, &saved, NULL) ==
              MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_fields_get(saved, &saved_fields) == MC_WORLD_INFO_OK &&
          saved_fields.player != NULL &&
          nbt_named(mc_world_info_player_root(saved_fields.player), "Score")->value.int_value ==
              900);
    mc_world_info_release(saved);
    saved = NULL;

    /* Retire every fresh World/Chunk owner, retain its real Store, then reopen
     * the actual persisted Player through a genuine loaded World constructor. */
    app->active_world = original_app_world;
    CHECK(nbt_metadata_close_world(&m) && nbt_metadata_construct(f, &m) && !m.world->fields.s);
    app->active_world = m.world;
    CHECK(mc_local_player_allocate(app->entities, &m.player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(m.player, m.world, app, app->session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          nbt_prepare(app, m.player, "persisted", a, entity_error));
    const uintptr_t old_main = (uintptr_t)m.player->local_player->fields.player.c->main_slots;
    const uintptr_t old_armor = (uintptr_t)m.player->local_player->fields.player.c->armor_slots;
    CHECK(mc_world_info_player_get(m.world->fields.x, &retained) == MC_WORLD_INFO_OK &&
          retained != NULL &&
          mc_world_runtime_attach_player(m.world, m.player, attach_error) == MC_WORLD_STORAGE_OK &&
          mc_world_info_player_get(m.world->fields.x, &retained) == MC_WORLD_INFO_OK &&
          retained == NULL && m.world->fields.d->count == 1 &&
          m.world->fields.d->items[0] == m.player &&
          m.player->local_player->fields.player.g == 900 &&
          nbt_state("persisted_load", "OK", m.player, old_main, old_armor) &&
          mc_entity_build_nbt(m.player, NULL, &tree, entity_error) == MC_ENTITY_NBT_OK &&
          nbt_tags("persisted_load", "after", "$", mc_nbt_tree_root(tree), 0));
    printf("PERSISTED_LOAD 1\n");
    mc_nbt_tree_free(tree);
    tree = NULL;
    /* A separate real loaded World has no published Player yet. Stop at the
     * exact wake invocation matched by the unchanged-original JDI observer. */
    app->active_world = original_app_world;
    CHECK(nbt_metadata_close_world(&m) && nbt_metadata_construct(f, &m));
    app->active_world = m.world;
    CHECK(mc_local_player_allocate(app->entities, &m.player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(m.player, m.world, app, app->session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          nbt_prepare(app, m.player, "sleep", a, entity_error));
    const uintptr_t sleep_main = (uintptr_t)m.player->local_player->fields.player.c->main_slots;
    const uintptr_t sleep_armor = (uintptr_t)m.player->local_player->fields.player.c->armor_slots;
    const mc_nbt_tag *sleep_input = nbt_make_input(a, "sleep");
    mc_world_info_player_release(opaque);
    opaque = NULL;
    CHECK(sleep_input != NULL &&
          mc_world_info_player_copy(sleep_input, NULL, &opaque, NULL) == MC_WORLD_INFO_OK &&
          mc_world_info_player_set(m.world->fields.x, opaque) == MC_WORLD_INFO_OK);
    const mc_world_storage_status sleep_status =
        mc_world_runtime_attach_player(m.world, m.player, attach_error);
    CHECK(sleep_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && attach_error->bytecode == 14 &&
          attach_error->nbt.base_bytecode == 311 &&
          attach_error->nbt.late_hook == MC_ENTITY_NBT_LATE_PLAYER &&
          attach_error->nbt.living_bytecode == 55 && attach_error->nbt.player_bytecode == 93 &&
          attach_error->nbt.local_player_bytecode == 2 && m.world->fields.d->count == 0 &&
          m.world->fields.b->count == 0 &&
          mc_world_info_player_get(m.world->fields.x, &retained) == MC_WORLD_INFO_OK &&
          retained == opaque &&
          nbt_state("sleep", "PENDING_WAKE", m.player, sleep_main, sleep_armor) &&
          mc_entity_build_nbt(m.player, NULL, &tree, entity_error) == MC_ENTITY_NBT_OK &&
          nbt_tags("sleep", "prefix", "$", mc_nbt_tree_root(tree), 0));
    printf("SITE sleep gs a (Lnu;)V 93\nRANDOM sleep ");
    nbt_random(m.player->random);
    putchar(' ');
    nbt_random(m.world->fields.r);
    printf("\nATTACH_PREFIX sleep 1 0 0\n");
    ok = true;
cleanup:
    if (first_null) {
        m.world->fields.d->items[0] = m.player;
        m.world->player_membership.entries[0] = m.player;
    }
    if (first_inventory != NULL)
        m.player->local_player->fields.player.c = first_inventory;
    if (second_inventory != NULL)
        m.second->local_player->fields.player.c = second_inventory;
    app->active_world = original_app_world;
    if (!nbt_metadata_close_world(&m))
        ok = false;
    if (mc_world_store_free_checked(&m.store, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    mc_world_info_name_release(m.name);
    mc_world_info_player_release(opaque);
    mc_world_info_release(saved);
    mc_nbt_tree_free(tree);
    free(save_error);
    free(metadata_error);
    free(attach_error);
    return ok;
}
