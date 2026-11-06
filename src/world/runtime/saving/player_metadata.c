#include "player_metadata_internal.h"

#include "../../../entity/entity_internal.h"
#include "../construction_internal.h"
#include "../runtime_internal.h"
#include "mc/entity/nbt/nbt.h"

#include <string.h>

void mc_world_player_metadata_error_init(mc_world_player_metadata_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->player_index = SIZE_MAX;
    mc_entity_nbt_error_init(&e->entity);
}

static bool fail(mc_world_player_metadata_error *e, mc_world_storage_status status,
                 const char *operation) {
    e->status = status;
    e->operation = operation;
    return false;
}

static mc_world_storage_status info_status(mc_world_info_status s) {
    return s == MC_WORLD_INFO_LIMIT               ? MC_WORLD_STORAGE_LIMIT
           : s == MC_WORLD_INFO_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : s == MC_WORLD_INFO_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
           : s == MC_WORLD_INFO_CLOCK_FAILURE     ? MC_WORLD_STORAGE_IO_ERROR
           : s == MC_WORLD_INFO_INVALID_PHASE     ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                                  : MC_WORLD_STORAGE_INVALID_DATA;
}

static bool build(mc_world_runtime *world, mc_world_info_record **out,
                  mc_world_player_metadata_error *e) {
    /* These are the actual arguments captured by the World before handler
     * dispatch. Later scalar/serializer calls do not reread the World cells. */
    mc_world_info *info = world->fields.x;
    const mc_world_reference_list *players = world->fields.d;
    e->wrapper_bytecode = 4; /* mx.a: ei.d(19132), before fm/ei List work. */
    const mc_world_info_status version = mc_world_info_version_set(info, 19132);
    if (version != MC_WORLD_INFO_OK) {
        e->info.status = version;
        return fail(e, info_status(version), "McRegion live Info version setter");
    }
    e->wrapper_bytecode = 10;
    e->list_bytecode = 14;
    if (players == NULL)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldInfo Player List.size receiver");
    if (players->count > players->capacity ||
        players->capacity > SIZE_MAX / sizeof(*players->items) ||
        (players->capacity != 0 && players->items == NULL))
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldInfo readable Player List");
    mc_entity *player = NULL;
    bool player_held = false;
    mc_nbt_tree *tree = NULL;
    mc_world_info_player *selected = NULL;
    bool completed = false;
    if (players->count != 0) {
        e->player_index = 0;
        e->list_bytecode = 24;
        /* Prove only the occurrence that the source actually gets. Later
         * entries are neither inspected nor serialized by ei.a(List). */
        if (players != world->player_membership.list || world->player_membership.count == 0 ||
            world->player_membership.entries == NULL ||
            players->items[0] != world->player_membership.entries[0]) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldInfo typed first Player occurrence");
            goto done;
        }
        player = world->player_membership.entries[0];
        e->list_bytecode = 29;
        if (player != NULL && (player->classes != world->classes->entities ||
                               !mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldInfo actual Player cast/dispatch");
            goto done;
        }
    }
    if (player != NULL) {
        e->list_bytecode = 49;
        const mc_entity_status held = mc_entity_retain(player);
        if (held != MC_ENTITY_OK) {
            e->entity.construction.status = held;
            fail(e,
                 held == MC_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                         : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 "WorldInfo selected Player native hold");
            goto done;
        }
        player_held = true;
        const mc_entity_nbt_status saved = mc_entity_build_nbt(player, NULL, &tree, &e->entity);
        if (saved != MC_ENTITY_NBT_OK) {
            fail(e,
                 saved == MC_ENTITY_NBT_LIMIT               ? MC_WORLD_STORAGE_LIMIT
                 : saved == MC_ENTITY_NBT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                            : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 "WorldInfo actual first Player direct writer");
            goto done;
        }
        mc_nbt_limits limits = mc_nbt_default_limits();
        const size_t retained = mc_nbt_tree_allocation_bytes(tree);
        if (retained > limits.max_allocation_bytes) {
            fail(e, MC_WORLD_STORAGE_LIMIT, "WorldInfo retained Player writer tree budget");
            goto done;
        }
        limits.max_allocation_bytes -= retained;
        const mc_world_info_status copied =
            mc_world_info_player_copy(mc_nbt_tree_root(tree), &limits, &selected, &e->info);
        if (copied != MC_WORLD_INFO_OK) {
            fail(e, info_status(copied), "WorldInfo independent selected Player NBT owner");
            goto done;
        }
        mc_nbt_tree_free(tree);
        tree = NULL;
    }
    e->list_bytecode = 56;
    const mc_world_info_status written =
        mc_world_info_record_create_with_player(info, selected, out, &e->info);
    if (written != MC_WORLD_INFO_OK) {
        if (written == MC_WORLD_INFO_CLOCK_FAILURE)
            e->info_writer_bytecode = 63;
        else if (e->info.field != NULL && strcmp(e->info.field, "LevelName") == 0)
            e->info_writer_bytecode = 76;
        fail(e, info_status(written),
             e->info.field == NULL ? mc_world_info_status_string(written) : e->info.field);
        goto done;
    }
    completed = true;
done:
    mc_world_info_player_release(selected);
    mc_nbt_tree_free(tree);
    if (player_held)
        mc_entity_release(player);
    return completed;
}

mc_world_storage_status
mc_world_runtime_build_player_metadata(mc_world_runtime *world, mc_world_info_record **out,
                                       mc_world_player_metadata_error *error) {
    mc_world_player_metadata_error e;
    mc_world_player_metadata_error_init(&e);
    if (world == NULL || out == NULL)
        fail(&e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "WorldInfo live Player native arguments");
    else
        (void)build(world, out, &e);
    const mc_world_storage_status status = e.status;
    if (status == MC_WORLD_STORAGE_OK)
        mc_world_player_metadata_error_init(&e);
    if (error != NULL)
        *error = e;
    return status;
}
