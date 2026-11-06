/* Prepared genuine GAME World/Store/Chunk components. The imported main is
 * never run. Message transport below observes the actual reached Player/key
 * boundary; it does not stand in for DC/App/UQ message execution. */
#define MC_WORLD_ENTITY_TICK_MAIN mc_bed_interaction_unused_tick_main
#include "test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN

#include "../../../src/block/registry/runtime_internal.h"
#include "../../../src/entity/player/sleep_internal.h"
#include "../../../src/world/runtime/lighting_internal.h"
#include "../../../src/world/runtime/mutation/mutation_internal.h"
#include "mc/entity/player/coordinates.h"
#include "mc/entity/player/sleep.h"
#include "mc/world/chunk/nibbles.h"
#include "mc/world/runtime/placement.h"

static const int32_t interaction_facing[4][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}};

typedef struct interaction_message {
    mc_entity *player;
    mc_registry_classes *registry;
    const mc_registry_string *keys[4];
    unsigned int count;
    bool refuse, order;
} interaction_message;

static mc_world_storage_status interaction_message_boundary(void *context, mc_entity *player,
                                                            mc_registry_classes *registry,
                                                            const mc_registry_string *key,
                                                            mc_world_placement_error *error) {
    interaction_message *m = context;
    if (player != m->player || registry != m->registry || m->count >= COUNT(m->keys))
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    m->order = error->player_message_entered && !error->player_message_returned &&
               error->bed_sleep_returned && error->site.pc == 424;
    m->keys[m->count++] = key;
    if (m->refuse) {
        error->status = MC_WORLD_STORAGE_IO_ERROR;
        error->operation = "authored Bed message-boundary refusal";
        error->native_policy = true;
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool interaction_cell(mc_chunk_data *data, int32_t x, int32_t y, int32_t z, int32_t id,
                             int32_t metadata) {
    data->blocks[boxes_index(x, y, z)] = (uint8_t)id;
    return mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z, (uint8_t)metadata);
}

/* Enumerate every nonzero ID/nibble, including metadata on an air cell. This
 * lossless sparse representation checks both complete planes without making
 * the driver manufacture the original capture's SHA256 values. */
typedef struct interaction_planes {
    uint8_t blocks[MC_CHUNK_BLOCKS], metadata[MC_CHUNK_NIBBLE_BYTES];
} interaction_planes;

static bool interaction_cells(const uint8_t *blocks, const uint8_t *nibbles) {
    bool first = true;
    if (putchar('[') == EOF)
        return false;
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i) {
        const unsigned int metadata = (nibbles[i >> 1] >> ((i & 1u) * 4u)) & 15u;
        if (blocks[i] == 0 && metadata == 0)
            continue;
        if (printf("%s[%zu,%zu,%zu,%u,%u]", first ? "" : ",", i / 2048u, i % 128u, (i / 128u) % 16u,
                   (unsigned int)blocks[i], metadata) < 0)
            return false;
        first = false;
    }
    return putchar(']') != EOF;
}

static bool interaction_random(const mc_java_random *r) {
    return printf("[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]", r->state,
                  r->have_next_next_gaussian ? "true" : "false",
                  movement_double_bits(r->next_next_gaussian)) >= 0;
}

typedef struct interaction_item {
    mc_item_stack_fields stack;
    double x, y, z;
    int32_t pickup_delay;
    size_t members;
} interaction_item;

static bool interaction_collect_items(fixture *f, interaction_item *rows, size_t *count) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[0];
    *count = 0;
    for (size_t i = 0; i < world->entity_membership.count; ++i) {
        mc_entity *item = world->entity_membership.entries[i];
        if (!mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM))
            continue;
        CHECK(*count < 1);
        interaction_item *row = &rows[(*count)++];
        CHECK(item->base.world == world &&
              mc_item_stack_fields_get(item->stack, &row->stack) == MC_ITEM_STACK_OK);
        size_t members = 0;
        for (size_t slice = 0; slice < 8; ++slice)
            for (size_t j = 0; j < f->chunks[0]->entities[slice].count; ++j)
                if (f->chunks[0]->entities[slice].items[j] == item)
                    ++members;
        CHECK(members == 1);
        row->members = members;
        row->x = item->base.x;
        row->y = item->base.y;
        row->z = item->base.z;
        row->pickup_delay = item->item.c;
    }
    ok = true;
cleanup:
    return ok;
}

static bool interaction_items(const interaction_item *rows, size_t count) {
    bool ok = false;
    CHECK(putchar('[') != EOF);
    for (size_t i = 0; i < count; ++i) {
        const interaction_item *row = &rows[i];
        CHECK(
            printf("%s{\"class\":\"hl\",\"stack\":[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
                   "],\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                   "\"],\"pickup_delay\":%" PRId32 ",\"world_member\":true,\"chunk_members\":%zu}",
                   i == 0 ? "" : ",", row->stack.id, row->stack.count, row->stack.damage,
                   row->stack.animation, movement_double_bits(row->x), movement_double_bits(row->y),
                   movement_double_bits(row->z), row->pickup_delay, row->members) >= 0);
    }
    CHECK(putchar(']') != EOF);
    ok = true;
cleanup:
    return ok;
}

static bool interaction_player(fixture *f, mc_entity **out, mc_player_session_classes **sessions) {
    bool ok = false;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL, *token = NULL;
    const uint16_t user[] = {'B', 'e', 'd', 'I', 'n', 't', 'e', 'r', 'a', 'c',
                             't', 'i', 'o', 'n', 'P', 'l', 'a', 'y', 'e', 'r'},
                   sid[] = {'-'};
    CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], sessions, NULL) ==
          MC_PLAYER_OK);
    CHECK(mc_player_string_create(user, COUNT(user), &username) == MC_PLAYER_OK &&
          mc_player_string_create(sid, COUNT(sid), &token) == MC_PLAYER_OK);
    CHECK(mc_player_session_create(*sessions, username, token, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], out, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(*out, f->worlds[0], NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->worlds[0], *out, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    const mc_entity_class_literal *player_class = NULL;
    bool assignable = false;
    CHECK(mc_entity_is_kind(*out, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
          !mc_entity_is_kind(*out, MC_ENTITY_CLASS_KIND_PLAYER) &&
          mc_entity_class_literal_get(f->entity_classes[0], MC_ENTITY_CLASS_KIND_PLAYER,
                                      &player_class) == MC_ENTITY_OK &&
          mc_entity_class_literal_is_assignable_from(player_class, (*out)->class_literal,
                                                     &assignable) == MC_ENTITY_OK &&
          assignable && f->worlds[0]->fields.d->count == 1 &&
          f->worlds[0]->fields.d->items[0] == *out);
    ok = true;
cleanup:
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    mc_player_string_release(token);
    return ok;
}

static bool interaction_profile(int32_t number, const char *root, int32_t metadata, bool partner,
                                bool remote, bool activate) {
    bool ok = false, entered = false, returned = false;
    fixture *f = calloc(1, sizeof(*f));
    mc_entity *player = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_world_placement_error *placement = calloc(1, sizeof(*placement));
    mc_world_mutation_error *mutation = malloc(sizeof(*mutation));
    interaction_planes *input = malloc(sizeof(*input));
    interaction_item items[1] = {0};
    size_t item_count = 0;
    mc_registry_string_view message_keys[4] = {0};
    int32_t id = 0, current = 0, partner_id = 0, resolved_metadata = 0;
    mc_world_storage_error admission = {0};
    char directory[8192];
    CHECK(f != NULL && placement != NULL && mutation != NULL && input != NULL &&
          snprintf(directory, sizeof(directory), "%s/case-%" PRId32, root, number) > 0);
    CHECK(tick_setup(f, directory));
    mc_world_runtime *world = f->worlds[0];
    mc_chunk_data *data = f->states[0]->data;
    const mc_block *bed = NULL;
    CHECK(mc_registry_block_static_get(f->registry[0], "T", &bed, NULL) == MC_REGISTRY_OK &&
          bed != NULL);
    const int32_t dx = interaction_facing[(uint32_t)metadata & 3u][0],
                  dz = interaction_facing[(uint32_t)metadata & 3u][1];
    const bool head = ((uint32_t)metadata & 8u) != 0;
    const int32_t px = 8 + (head ? -dx : dx), pz = 8 + (head ? -dz : dz), hx = head ? 8 : 8 + dx,
                  hz = head ? 8 : 8 + dz;
    CHECK(interaction_cell(data, 8, 64, 8, 26, metadata));
    if (partner)
        CHECK(interaction_cell(data, px, 64, pz, 26,
                               activate ? (head ? metadata & 7 : metadata | 8)
                                        : (head ? metadata & 3 : (metadata & 3) | 8)));
    if (activate && !remote && (head || partner))
        CHECK(interaction_player(f, &player, &sessions));
    mc_entity_base_fields before_base = {0};
    mc_local_player_snapshot before_player = {0};
    if (player != NULL) {
        before_base = player->base;
        before_player = player->local_player->fields;
    }
    world->fields.B = remote;
    world->fields.f = 0;
    mc_java_random_seed(world->fields.r, 2468);
    const mc_java_random before_random = *world->fields.r;
    memcpy(input->blocks, data->blocks, sizeof(input->blocks));
    memcpy(input->metadata, data->metadata, sizeof(input->metadata));
    CHECK(fflush(stdout) == 0);
    interaction_message message = {.player = player, .registry = f->registry[0]};
    const mc_world_block_activation_context context = {
        .context = &message, .player_message = interaction_message_boundary};
    if (activate) {
        const mc_world_storage_status status = mc_world_block_activate_with_context(
            f->registry[0], bed, world, 8, 64, 8, player, &context, &returned, placement);
        if (status != MC_WORLD_STORAGE_OK)
            fprintf(stderr, "Bed activation case%" PRId32 " status%d %s:%u %s\n", number,
                    (int)status, placement->site.class_name, placement->site.pc,
                    placement->operation == NULL ? "-" : placement->operation);
        CHECK(status == MC_WORLD_STORAGE_OK && returned && placement->returned);
    } else {
        mc_world_mutation_error_init(mutation);
        CHECK(mc_world_runtime_call_enter(world, &admission) == MC_WORLD_STORAGE_OK);
        entered = true;
        const bool success = mc_world_mutation_block_neighbor(world, bed, 8, 64, 8, 81, mutation);
        mc_world_runtime_call_leave(world);
        entered = false;
        if (!success)
            fprintf(stderr, "Bed neighbor case%" PRId32 " status%d %s:%u %s\n", number,
                    (int)mutation->status, mutation->hook_class, mutation->hook_bytecode,
                    mutation->operation == NULL ? "-" : mutation->operation);
        CHECK(success);
    }
    CHECK(mc_world_runtime_block_get(world, 8, 64, 8, &id, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(world, 8, 64, 8, &current, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_block_get(world, activate ? hx : px, 64, activate ? hz : pz, &partner_id,
                                     NULL) == MC_WORLD_STORAGE_OK);
    if (!activate)
        CHECK(interaction_collect_items(f, items, &item_count));
    else {
        CHECK(mc_world_runtime_metadata_get(world, hx, 64, hz, &resolved_metadata, NULL) ==
              MC_WORLD_STORAGE_OK);
        const bool unchanged =
            player != NULL && memcmp(&before_base, &player->base, sizeof(before_base)) == 0 &&
            memcmp(&before_player, &player->local_player->fields, sizeof(before_player)) == 0;
        CHECK(player == NULL || unchanged);
        const mc_player_sleep_result *expected = NULL;
        if (player != NULL) {
            CHECK(mc_player_sleep_result_static_get(player->classes, "c", &expected, NULL) ==
                      MC_PLAYER_SLEEP_RESULT_OK &&
                  expected != NULL && placement->bed_sleep_result_identity == (uintptr_t)expected &&
                  message.count == 1 && message.order && placement->player_message_returned);
        } else
            CHECK(message.count == 0 && !placement->bed_sleep_returned);
        for (unsigned int i = 0; i < message.count; ++i)
            CHECK(mc_registry_string_read(message.keys[i], &message_keys[i]));
    }
    /* No source invocation is interleaved with a partial record. In
     * particular the real System.out initializer writes remain separate
     * from this stdio observer, with a flush before the next source call. */
    CHECK(printf("BED_INTERACTION {\"case\":%" PRId32 ",\"kind\":\"%s\",\"metadata\":%" PRId32
                 ",\"partner\":%s,\"remote\":%s,\"input_cells\":",
                 number, activate ? "activate" : "neighbor", metadata, partner ? "true" : "false",
                 remote ? "true" : "false") >= 0 &&
          interaction_cells(input->blocks, input->metadata) && printf(",\"output_cells\":") >= 0 &&
          interaction_cells(data->blocks, data->metadata) && printf(",\"random_before\":") >= 0 &&
          interaction_random(&before_random) && printf(",\"random_after\":") >= 0 &&
          interaction_random(world->fields.r));
    if (!activate) {
        CHECK(printf(",\"id\":%" PRId32 ",\"metadata_after\":%" PRId32 ",\"partner_id\":%" PRId32
                     ",\"items\":",
                     id, current, partner_id) >= 0 &&
              interaction_items(items, item_count));
    } else {
        CHECK(printf(",\"returned\":true,\"origin_id\":%" PRId32 ",\"resolved_id\":%" PRId32
                     ",\"resolved_metadata\":%" PRId32
                     ",\"daytime\":true,\"provider_allows\":true,\"player_unchanged\":%s,"
                     "\"sleep_result\":%s,\"message_keys\":[",
                     id, partner_id, resolved_metadata, player == NULL ? "null" : "true",
                     player == NULL ? "null" : "\"cw.c\"") >= 0);
        for (unsigned int i = 0; i < message.count; ++i) {
            const mc_registry_string_view key = message_keys[i];
            CHECK(printf("%s\"", i == 0 ? "" : ",") >= 0);
            for (size_t j = 0; j < key.length; ++j)
                CHECK(printf("%04x", (unsigned int)key.units[j]) >= 0);
            CHECK(putchar('"') != EOF);
        }
        CHECK(putchar(']') != EOF);
    }
    CHECK(puts("}") >= 0 && fflush(stdout) == 0);
    CHECK(mc_world_placement_error_retire(placement));
    ok = true;
cleanup:
    if (entered)
        mc_world_runtime_call_leave(f->worlds[0]);
    if (placement != NULL && !mc_world_placement_error_retire(placement))
        ok = false;
    free(placement);
    free(mutation);
    free(input);
    mc_entity_release(player);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK)
        ok = false;
    if (!tick_cleanup(f))
        ok = false;
    return ok;
}

typedef struct interaction_observer {
    unsigned int cells, ranges;
    bool late_remote, refuse;
} interaction_observer;

static mc_world_storage_status interaction_observe_cell(void *context, mc_world_runtime *world,
                                                        int32_t x, int32_t y, int32_t z,
                                                        mc_world_storage_error *error) {
    (void)x;
    (void)y;
    (void)z;
    interaction_observer *o = context;
    ++o->cells;
    if (o->late_remote)
        world->fields.B = true;
    if (o->refuse) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored Bed WorldAccess refusal", 0};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status interaction_observe_range(void *context, mc_world_runtime *world,
                                                         int32_t a, int32_t b, int32_t c, int32_t d,
                                                         int32_t e, int32_t f,
                                                         mc_world_storage_error *error) {
    (void)world;
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)error;
    ++((interaction_observer *)context)->ranges;
    return MC_WORLD_STORAGE_OK;
}

static bool interaction_reset(fixture *f) {
    mc_chunk_data *data = f->states[0]->data;
    memset(data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(data->heightmap, 0, MC_CHUNK_HEIGHTMAP_BYTES);
    memset(data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    f->worlds[0]->fields.B = false;
    f->worlds[0]->fields.f = 0;
    return mc_world_runtime_light_list_clear(f->worlds[0], NULL) == MC_WORLD_STORAGE_OK;
}

/* Authored source/native fault inputs. They are not additional original rows
 * or evidence of complete sleep, explosion or client message execution. */
static bool interaction_controls(const char *root) {
    bool ok = false, entered = false, registered = false;
    fixture *f = calloc(1, sizeof(*f));
    mc_entity *player = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_world_placement_error *error = calloc(1, sizeof(*error));
    mc_world_mutation_error *mutation = malloc(sizeof(*mutation));
    mc_world_provider *nether = NULL;
    mc_world_access *access = NULL;
    int32_t **facing = NULL;
    mc_player_sleep_result *saved_a = NULL;
    bool saved_enum = false;
    char directory[8192];
    CHECK(f != NULL && error != NULL && mutation != NULL &&
          snprintf(directory, sizeof(directory), "%s/controls", root) > 0 &&
          tick_setup(f, directory));
    mc_world_runtime *world = f->worlds[0];
    mc_chunk_data *data = f->states[0]->data;
    const mc_block *bed = NULL;
    CHECK(mc_registry_block_static_get(f->registry[0], "T", &bed, NULL) == MC_REGISTRY_OK &&
          bed != NULL && interaction_player(f, &player, &sessions));
    interaction_message message = {.player = player, .registry = f->registry[0]};
    const mc_world_block_activation_context context = {
        .context = &message, .player_message = interaction_message_boundary};
    bool out = false;
    CHECK(mc_world_block_activate_with_context(f->registry[0], bed, NULL, 8, 64, 8, NULL, &context,
                                               &out, error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !out && error->source_exception && !error->native_policy && error->site.pc == 1);
    CHECK(mc_world_placement_error_retire(error));
    mc_world_mutation_error_init(mutation);
    CHECK(!mc_world_mutation_block_neighbor(NULL, bed, 8, 64, 8, 81, mutation) &&
          mutation->bed_source_null && mutation->hook_bytecode == 5);

    /* Stale occupied state is cleared before the null clicked Player fault. */
    CHECK(interaction_reset(f) && interaction_cell(data, 8, 64, 8, 26, 12));
    CHECK(mc_world_block_activate_with_context(f->registry[0], bed, world, 8, 64, 8, NULL, &context,
                                               &out, error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !out && error->source_exception && !error->native_policy && error->site.pc == 388 &&
          error->bed_occupancy_cleared && error->query_count == 2);
    int32_t metadata;
    CHECK(mc_world_runtime_metadata_get(world, 8, 64, 8, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
          metadata == 8 && message.count == 0 && mc_world_placement_error_retire(error));

    /* A genuine nullable captured VE.a fails on the first reached row read. */
    CHECK(interaction_reset(f) && interaction_cell(data, 8, 64, 8, 26, 0));
    mc_registry_bed_table_view table;
    CHECK(mc_registry_bed_table_get(f->registry[0], &table, NULL) == MC_REGISTRY_OK);
    facing = f->registry[0]->ve_a;
    f->registry[0]->ve_a = NULL;
    CHECK(mc_world_block_activate_with_context(f->registry[0], bed, world, 8, 64, 8, player,
                                               &context, &out,
                                               error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !out && error->source_exception && !error->native_policy && error->site.pc == 40 &&
          error->registry.status == MC_REGISTRY_NULL_REFERENCE && error->query_count == 1);
    f->registry[0]->ve_a = facing;
    facing = NULL;
    CHECK(mc_world_placement_error_retire(error));

    /* Actual sleeping entries use N() and their own coordinate identities.
     * These are explicitly authored sleep fields, not a completed GS sleep. */
    CHECK(interaction_reset(f) && interaction_cell(data, 8, 64, 8, 26, 12));
    player->local_player->fields.player.u = true;
    player->local_player->fields.player.v = NULL;
    CHECK(mc_world_block_activate_with_context(f->registry[0], bed, world, 8, 64, 8, player,
                                               &context, &out,
                                               error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !out && error->source_exception && error->site.pc == 323 && error->bed_occupant != NULL &&
          error->bed_occupant->status == MC_PLAYER_OK && !error->bed_sleep_returned &&
          mc_world_placement_error_retire(error));
    const mc_player_coordinates bed_position = {8, 64, 8};
    CHECK(mc_player_saved_spawn_set(player, &bed_position, NULL) == MC_PLAYER_OK);
    player->local_player->fields.player.v = player->local_player->fields.player.private_b;
    CHECK(mc_world_block_activate_with_context(f->registry[0], bed, world, 8, 64, 8, NULL, &context,
                                               &out, error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !out && error->source_exception && error->site.pc == 377 &&
          error->bed_sleeper_identity == (uintptr_t)player && !error->bed_sleep_returned &&
          !error->bed_occupancy_cleared && message.count == 0 &&
          mc_world_placement_error_retire(error));
    player->local_player->fields.player.u = false;
    player->local_player->fields.player.v = NULL;
    CHECK(mc_player_saved_spawn_set(player, NULL, NULL) == MC_PLAYER_OK);

    /* Independently reached Player.aI stays distinct from the VE World. */
    CHECK(interaction_reset(f) && interaction_cell(data, 8, 64, 8, 26, 8));
    player->base.world = NULL;
    CHECK(mc_world_block_activate_with_context(f->registry[0], bed, world, 8, 64, 8, player,
                                               &context, &out,
                                               error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !out && error->source_exception && error->sleep != NULL &&
          error->sleep->status == MC_PLAYER_SLEEP_SOURCE_NULL && error->site.pc == 4 &&
          strcmp(error->site.class_name, "gs") == 0 && error->caller.pc == 388 &&
          message.count == 0 && error->sleep_result == NULL &&
          f->entity_classes[0]->sleep_result_phase == MC_ENTITY_CLASS_COLD);
    player->base.world = world;
    CHECK(mc_world_placement_error_retire(error));

    /* The reached native message adapter may refuse AFTER the real GS/CW day
     * result and after stale occupancy mutation. Both Worlds/Player survive. */
    CHECK(interaction_reset(f) && interaction_cell(data, 8, 64, 8, 26, 12));
    const size_t bindings = world->persistent_bindings, holds = player->holds,
                 namespace_holds = f->entity_classes[0]->borrowers;
    message.refuse = true;
    CHECK(
        mc_world_block_activate_with_context(f->registry[0], bed, world, 8, 64, 8, player, &context,
                                             &out, error) == MC_WORLD_STORAGE_IO_ERROR &&
        !out && error->bed_occupancy_cleared && error->bed_sleep_returned &&
        error->player_message_entered && !error->player_message_returned && message.count == 1 &&
        message.order && world->persistent_bindings == bindings + 2 && player->holds == holds + 1 &&
        mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(f->entity_classes[0]->borrowers == namespace_holds + 1);
    /* Native metadata corruption refuses checked cleanup without consuming
     * the remaining error graph or replaying the failed source callback. */
    const size_t retained_namespace_holds = f->entity_classes[0]->borrowers;
    f->entity_classes[0]->borrowers = 0;
    const bool retired = mc_world_placement_error_retire(error);
    f->entity_classes[0]->borrowers = retained_namespace_holds;
    CHECK(!retired && error->work != NULL && error->cleanup_enum_classes == MC_ENTITY_STATE_ERROR &&
          error->status == MC_WORLD_STORAGE_IO_ERROR && error->site.pc == 424 &&
          message.count == 1 && world->persistent_bindings == bindings + 2 &&
          player->holds == holds + 1);
    CHECK(mc_world_placement_error_retire(error) && world->persistent_bindings == bindings &&
          player->holds == holds && f->entity_classes[0]->borrowers == namespace_holds);
    message = (interaction_message){.player = player, .registry = f->registry[0]};

    /* Actual replaceable CW cells compare as identities. Aliasing a to c
     * selects the real occupancy setter after the genuine daytime return. */
    CHECK(interaction_reset(f) && interaction_cell(data, 8, 64, 8, 26, 8));
    const mc_player_sleep_result *canonical = NULL;
    CHECK(mc_player_sleep_result_static_get(player->classes, "a", &canonical, NULL) ==
          MC_PLAYER_SLEEP_RESULT_OK);
    saved_a = player->classes->sleep_results->a;
    saved_enum = true;
    player->classes->sleep_results->a = player->classes->sleep_results->c;
    CHECK(mc_world_block_activate_with_context(f->registry[0], bed, world, 8, 64, 8, player,
                                               &context, &out, error) == MC_WORLD_STORAGE_OK &&
          out && error->bed_sleep_returned && !error->player_message_entered &&
          error->site.pc == 411 && error->query_count == 2);
    CHECK(mc_world_runtime_metadata_get(world, 8, 64, 8, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
          metadata == 12 && mc_world_placement_error_retire(error));
    player->classes->sleep_results->a = saved_a;
    saved_enum = false;

    /* Actual nighttime prefix stops before GS resize119. No occupancy or
     * invented sleep result is published by the World wrapper. */
    out = false;
    CHECK(interaction_reset(f) && interaction_cell(data, 8, 64, 8, 26, 8));
    world->fields.f = 4;
    CHECK(mc_entity_set_position(player, 8.5, 64, 8.5) == MC_ENTITY_OK);
    CHECK(mc_world_block_activate_with_context(f->registry[0], bed, world, 8, 64, 8, player,
                                               &context, &out,
                                               error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !out && error->native_policy && !error->source_exception && error->site.pc == 119 &&
          error->sleep != NULL && error->sleep->status == MC_PLAYER_SLEEP_UNIMPLEMENTED &&
          !error->bed_sleep_returned && !player->local_player->fields.player.u &&
          message.count == 0 && mc_world_placement_error_retire(error));

    /* The neighbor's World.B read115 is genuinely late, after notifying
     * removal and this explicitly authored WorldAccess callback. */
    CHECK(interaction_reset(f) && interaction_cell(data, 8, 64, 8, 26, 0));
    interaction_observer observer = {.late_remote = true};
    const mc_world_access_callbacks callbacks = {.cell = interaction_observe_cell,
                                                 .range = interaction_observe_range};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    mc_world_mutation_error_init(mutation);
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    CHECK(mc_world_mutation_block_neighbor(world, bed, 8, 64, 8, 81, mutation));
    mc_world_runtime_call_leave(world);
    entered = false;
    CHECK(observer.cells != 0 && world->fields.B && world->fields.b->count == 1 &&
          mc_world_runtime_block_get(world, 8, 64, 8, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
          metadata == 0);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    registered = false;
    CHECK(mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    access = NULL;

    /* Genuine Nether predicate false: reached head and forward removals are
     * retained. The actual explosion child256 is an explicit native frontier. */
    CHECK(interaction_reset(f) && interaction_cell(data, 8, 64, 8, 26, 8) &&
          interaction_cell(data, 8, 64, 9, 26, 8));
    CHECK(mc_world_provider_create(-1, NULL, &nether, NULL) == MC_WORLD_PROVIDER_OK);
    world->fields.t = nether;
    out = false;
    CHECK(mc_world_block_activate_with_context(f->registry[0], bed, world, 8, 64, 8, NULL, &context,
                                               &out, error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !out && error->site.pc == 256 && error->bed_explosion_pending && error->native_policy &&
          !error->source_exception && error->bed_explosion_x == 8.5 &&
          error->bed_explosion_y == 64.5 && error->bed_explosion_z == 9.5 &&
          mc_world_runtime_block_get(world, 8, 64, 8, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
          metadata == 0 &&
          mc_world_runtime_block_get(world, 8, 64, 9, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
          metadata == 0 && mc_world_placement_error_retire(error));
    world->fields.t = f->dimensions[0];

    /* Authored List.add(Item) input keeps canonical native occurrence
     * ownership. The actual awake DC passes GS checkcast/N first; the later
     * genuine Item is a source ClassCastException at301, not a native refusal. */
    CHECK(interaction_reset(f) && interaction_cell(data, 8, 64, 8, 26, 12));
    mc_entity *wrong_type = NULL;
    CHECK(make_entity(f, 0, world, 8.5, 64, 8.5, &wrong_type));
    mc_world_entity_error append_error = {0};
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    const bool appended = mc_world_runtime_entity_append_owned(
        world, world->fields.d, &world->player_membership, wrong_type, 301, &append_error);
    mc_world_runtime_call_leave(world);
    entered = false;
    CHECK(appended);
    out = false;
    CHECK(mc_world_block_activate_with_context(f->registry[0], bed, world, 8, 64, 8, player,
                                               &context, &out,
                                               error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !out && error->source_exception && !error->native_policy && error->site.pc == 301 &&
          strcmp(error->exception_class, "java/lang/ClassCastException") == 0 &&
          error->bed_player_index == 1 && error->bed_occupant != NULL &&
          error->bed_occupant->status == MC_PLAYER_OK && !error->bed_sleep_returned &&
          mc_world_placement_error_retire(error));
    ok = true;
cleanup:
    if (f != NULL && f->worlds[0] != NULL) {
        if (entered)
            mc_world_runtime_call_leave(f->worlds[0]);
        if (player != NULL)
            player->base.world = f->worlds[0];
        f->worlds[0]->fields.t = f->dimensions[0];
        if (facing != NULL)
            f->registry[0]->ve_a = facing;
        if (saved_enum)
            player->classes->sleep_results->a = saved_a;
        if (registered) {
            bool ignored;
            if (mc_world_runtime_remove_world_access(f->worlds[0], access, &ignored, NULL) !=
                MC_WORLD_STORAGE_OK)
                ok = false;
        }
    }
    if (error != NULL && !mc_world_placement_error_retire(error))
        ok = false;
    free(error);
    free(mutation);
    if (mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    mc_world_provider_release(nether);
    mc_entity_release(player);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !tick_cleanup(f))
        ok = false;
    return ok;
}

static bool interaction_namespaces(const char *root) {
    bool ok = false;
    fixture *a = calloc(1, sizeof(*a)), *b = calloc(1, sizeof(*b));
    mc_entity *player = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_world_placement_error *error = calloc(1, sizeof(*error));
    mc_entity_classes *caller_join = NULL;
    char directory[8192];
    CHECK(a != NULL && b != NULL && error != NULL);
    CHECK(snprintf(directory, sizeof(directory), "%s/caller-namespace", root) > 0 &&
          tick_setup(a, directory));
    CHECK(snprintf(directory, sizeof(directory), "%s/player-namespace", root) > 0 &&
          tick_setup(b, directory) && interaction_player(b, &player, &sessions));
    const mc_block *bed = NULL;
    CHECK(mc_registry_block_static_get(a->registry[0], "T", &bed, NULL) == MC_REGISTRY_OK &&
          bed != NULL && interaction_cell(a->states[0]->data, 8, 64, 8, 26, 8));
    a->worlds[0]->fields.f = b->worlds[0]->fields.f = 0;
    CHECK(a->entity_classes[0]->sleep_result_phase == MC_ENTITY_CLASS_COLD &&
          b->entity_classes[0]->sleep_result_phase == MC_ENTITY_CLASS_COLD);
    const size_t caller_holds = a->entity_classes[0]->borrowers,
                 player_holds = b->entity_classes[0]->borrowers,
                 caller_bindings = a->worlds[0]->persistent_bindings,
                 player_bindings = b->worlds[0]->persistent_bindings;
    interaction_message message = {.player = player, .registry = a->registry[0]};
    const mc_world_block_activation_context context = {
        .context = &message, .player_message = interaction_message_boundary};
    bool out = false;
    /* Missing caller dependency is reached after GS, never preflighted as a
     * substitute for that actual Player/World operation. */
    caller_join = a->registry[0]->entities;
    a->registry[0]->entities = NULL;
    const mc_world_storage_status missing = mc_world_block_activate_with_context(
        a->registry[0], bed, a->worlds[0], 8, 64, 8, player, &context, &out, error);
    a->registry[0]->entities = caller_join;
    caller_join = NULL;
    CHECK(missing == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !out && error->site.pc == 395 &&
          error->bed_sleep_returned && error->sleep_result != NULL &&
          error->sleep_result->native.status == MC_ENTITY_INVALID_ARGUMENT &&
          error->native_policy && !error->source_exception && message.count == 0 &&
          a->entity_classes[0]->sleep_result_phase == MC_ENTITY_CLASS_COLD &&
          b->entity_classes[0]->sleep_result_phase == MC_ENTITY_CLASS_READY &&
          mc_world_placement_error_retire(error));
    /* Explicitly authored native cross-namespace input; no original Java
     * cross-loader assignability or complete client-message claim. GS returns
     * its actual c; VE resolves its own a/c and neither comparison matches. */
    CHECK(mc_world_block_activate_with_context(a->registry[0], bed, a->worlds[0], 8, 64, 8, player,
                                               &context, &out, error) == MC_WORLD_STORAGE_OK &&
          out && error->site.pc == 428 && error->bed_sleep_returned &&
          !error->player_message_entered && message.count == 0 &&
          a->entity_classes[0]->sleep_result_phase == MC_ENTITY_CLASS_READY &&
          b->entity_classes[0]->sleep_result_phase == MC_ENTITY_CLASS_READY &&
          a->entity_classes[0]->borrowers == caller_holds + 1 &&
          b->entity_classes[0]->borrowers == player_holds &&
          a->worlds[0]->persistent_bindings == caller_bindings + 1 &&
          b->worlds[0]->persistent_bindings == player_bindings + 1);
    const mc_player_sleep_result *actual = NULL, *caller = NULL;
    CHECK(mc_player_sleep_result_static_get(b->entity_classes[0], "c", &actual, NULL) ==
              MC_PLAYER_SLEEP_RESULT_OK &&
          mc_player_sleep_result_static_get(a->entity_classes[0], "c", &caller, NULL) ==
              MC_PLAYER_SLEEP_RESULT_OK &&
          actual != caller && error->bed_sleep_result_identity == (uintptr_t)actual);
    CHECK(mc_world_placement_error_retire(error) &&
          a->entity_classes[0]->borrowers == caller_holds &&
          b->entity_classes[0]->borrowers == player_holds &&
          a->worlds[0]->persistent_bindings == caller_bindings &&
          b->worlds[0]->persistent_bindings == player_bindings);
    ok = true;
cleanup:
    if (caller_join != NULL)
        a->registry[0]->entities = caller_join;
    if (error != NULL && !mc_world_placement_error_retire(error))
        ok = false;
    free(error);
    mc_entity_release(player);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK)
        ok = false;
    if (!tick_cleanup(a) || !tick_cleanup(b))
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    char root[8192], resources_path[8192];
    if (argc != 4 || strcmp(argv[1], "bed-interaction") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) ||
        !decode_path(argv[3], resources_path, sizeof(resources_path)) || !mc_math_trig_init(&trig))
        return 2;
    CHECK(mc_resource_files_create(resources_path, &files) == MC_REGISTRY_OK);
    tick_resources = mc_resource_files_source(files);
    int32_t number = 0;
    for (int32_t metadata = 0; metadata < 16; ++metadata)
        for (int32_t partner = 0; partner < 2; ++partner)
            for (int32_t remote = 0; remote < 2; ++remote)
                CHECK(interaction_profile(number++, root, metadata, partner != 0, remote != 0,
                                          false));
    for (int32_t metadata = 0; metadata < 16; ++metadata)
        CHECK(interaction_profile(number++, root, metadata, true, false, true));
    for (int32_t metadata = 0; metadata < 4; ++metadata)
        CHECK(interaction_profile(number++, root, metadata, false, false, true));
    CHECK(interaction_profile(number++, root, 0, false, true, true) && number == 85);
    CHECK(interaction_controls(root) && interaction_namespaces(root));
    result = 0;
cleanup:
    mc_resource_files_free(files);
    if (result == 0 && printf("Bed interaction: %u checks\n", checks) < 0)
        return 1;
    return result;
}
