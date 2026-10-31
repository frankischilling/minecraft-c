#include "nbt_internal.h"
#include "player_internal.h"

#include "../../world/runtime/runtime_internal.h"
#include "../../world/runtime/spawn_internal.h"
#include "../construction_internal.h"
#include "../living/construction_internal.h"
#include "mc/client/input/movement_input.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/runtime.h"

mc_entity_status mc_local_player_allocate(mc_entity_classes *classes, mc_entity **out,
                                          mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER,
                                        "local Player allocation", out, error);
}

mc_entity_status mc_local_player_watcher_initialize(mc_entity *entity, mc_entity_error *error) {
    /* gs.b -> ls.b's actual empty return, then watcher index16 Byte0. This
     * executes before ANY ls initializer on the same newly allocated dc. */
    entity->local_player->fields.watcher_hook_pc = 1;
    const mc_entity_status status = mc_entity_construct_watch_byte_zero(entity, 16, error);
    if (status == MC_ENTITY_OK)
        entity->local_player->fields.watcher_hook_pc = 17;
    return status;
}

/* Shared production formula seams also permit a separately labelled scalar
 * replay of actual observed Math returns; production always draws its real
 * application profile above and never accepts supplied entropy. */
float mc_local_player_aj_from_draw(double value) {
    return (float)(value * (double)0.9f + (double)0.1f);
}

float mc_local_player_g_from_draw(double value) {
    return (float)(value + 1.0) * 0.01f;
}

float mc_local_player_f_from_draw(double value) {
    return (float)value * 12398.0f;
}

float mc_local_player_yaw_from_draw(double value) {
    return (float)((value * 3.1415927410125732) * 2.0);
}

static mc_player_status living_construct(mc_entity *entity, mc_player_error *error) {
    mc_local_player_snapshot *s = &entity->local_player->fields;
    if (mc_entity_construct_living(entity, &s->living, &s->living_pc, &error->entity) ==
        MC_ENTITY_OK)
        return MC_PLAYER_OK;
    return mc_player_fail(error, MC_PLAYER_PROFILE_FAILED, "LivingEntity Math.random", "ls",
                          "<init>", "(Lfd;)V", error->entity.bytecode);
}

static mc_player_status world_fields(mc_world_runtime *world, mc_world_runtime_fields *out,
                                     unsigned int pc, mc_player_error *error) {
    if (world == NULL)
        return mc_player_fail(error, MC_PLAYER_NULL_REFERENCE, "Player null World receiver", "gs",
                              "<init>", "(Lfd;)V", pc);
    const mc_world_storage_status status = mc_world_runtime_fields_get(world, out);
    if (status != MC_WORLD_STORAGE_OK) {
        error->world = (mc_world_storage_error){status, "Player actual World fields", 0};
        return mc_player_fail(error, MC_PLAYER_WORLD_FAILED, "Player actual World fields", "gs",
                              "<init>", "(Lfd;)V", pc);
    }
    return MC_PLAYER_OK;
}

static mc_player_status spawn(mc_entity *entity, mc_world_runtime *world,
                              mc_player_coordinates **out, mc_player_error *error) {
    mc_player_coordinates *coordinate = mc_entity_construction_allocate(
        entity, sizeof(*coordinate), &error->entity, "World spawn coordinate allocation", 0);
    if (coordinate == NULL)
        return mc_player_fail(error,
                              error->entity.status == MC_ENTITY_LIMIT ? MC_PLAYER_LIMIT
                                                                      : MC_PLAYER_ALLOCATION_FAILED,
                              "World spawn coordinate allocation", "fd", "u", "()Lbr;", 0);
    /* fd.u reloads x for each of the three actual Info getters. No provider,
     * terrain-top guess or cached diagnostic spawn replaces these cells. */
    int32_t values[3];
    mc_world_spawn_error child = {0};
    const bool complete = mc_world_source_spawn_values(world, values, &child);
    if (child.info_getter_entered)
        error->info = child.info;
    if (!complete) {
        if (child.fields_failed) {
            /* Same old world_fields(world,...,100) diagnostic mapping. */
            if (world == NULL)
                mc_player_fail(error, MC_PLAYER_NULL_REFERENCE, "Player null World receiver", "gs",
                               "<init>", "(Lfd;)V", 100);
            else {
                error->world =
                    (mc_world_storage_error){child.world, "Player actual World fields", 0};
                mc_player_fail(error, MC_PLAYER_WORLD_FAILED, "Player actual World fields", "gs",
                               "<init>", "(Lfd;)V", 100);
            }
        } else {
            error->info = child.info;
            /* Actual Info NULL is exactly the old fields.x==NULL branch. */
            mc_player_fail(error,
                           child.info_was_null ? MC_PLAYER_NULL_REFERENCE : MC_PLAYER_WORLD_FAILED,
                           "World spawn Info getter", "fd", "u", "()Lbr;", child.pc);
        }
        goto failed;
    }
    coordinate->x = values[0];
    coordinate->y = values[1];
    coordinate->z = values[2];
    *out = coordinate;
    return MC_PLAYER_OK;
failed:
    mc_entity_construction_retire(entity, coordinate, sizeof(*coordinate));
    return error->status;
}

static void source_position_rotation(mc_entity *entity, double x, double y, double z) {
    /* Exact inherited sn.c(DDDFF)V stores; yaw/pitch arguments are source +0. */
    entity->base.x = x;
    entity->base.aJ = x;
    entity->base.bl = x;
    entity->base.y = y + (double)entity->base.offset;
    entity->base.aK = entity->base.y;
    entity->base.bm = entity->base.y;
    entity->base.z = z;
    entity->base.aL = z;
    entity->base.bn = z;
    entity->base.yaw = 0.0f;
    entity->base.aT = 0.0f;
    mc_entity_source_position(entity, entity->base.x, entity->base.y, entity->base.z);
}

static mc_player_status player_construct(mc_entity *entity, mc_world_runtime *world,
                                         const mc_inventory_limits *limits,
                                         mc_player_error *error) {
    mc_local_player_snapshot *s = &entity->local_player->fields;
    mc_inventory_player *inventory = NULL;
    if (mc_inventory_player_create(MC_REGISTRY_CLIENT, entity, limits, &inventory,
                                   &error->inventory) != MC_INVENTORY_OK)
        return mc_player_fail(error, MC_PLAYER_INVENTORY_FAILED,
                              "Player InventoryPlayer construction", "gs", "<init>", "(Lfd;)V", 11);
    s->player.c = inventory;
    s->player_pc = 14;
    s->player.f = 0;
    s->player_pc = 19;
    s->player.g = 0;
    s->player_pc = 24;
    s->player.j = false;
    s->player_pc = 29;
    s->player.k = 0;
    s->player_pc = 34;
    s->player.z = 20;
    s->player_pc = 40;
    s->player.A = false;
    s->player_pc = 45;
    s->player.private_bO = 0;
    s->player_pc = 50;
    s->player.D = NULL;
    s->player_pc = 55;
    mc_world_runtime_fields fields;
    mc_player_status status = world_fields(world, &fields, 68, error);
    if (status != MC_PLAYER_OK)
        return status;
    mc_container *container = NULL;
    if (mc_container_player_create(MC_REGISTRY_CLIENT, mc_world_runtime_registry(world), inventory,
                                   !fields.B, limits, &container,
                                   &error->inventory) != MC_INVENTORY_OK)
        return mc_player_fail(error, MC_PLAYER_INVENTORY_FAILED,
                              "Player actual Container construction", "gs", "<init>", "(Lfd;)V",
                              79);
    s->player.d = container;
    s->player_pc = 82;
    s->player.e = s->player.d;
    s->player_pc = 90;
    entity->base.offset = 1.62f;
    s->player_pc = 96;
    mc_player_coordinates *coordinate = NULL;
    status = spawn(entity, world, &coordinate, error);
    if (status != MC_PLAYER_OK)
        return status;
    s->player_pc = 100;
    const double x = (double)coordinate->x + 0.5;
    const double y = (double)mc_java_i32_add(coordinate->y, 1);
    const double z = (double)coordinate->z + 0.5;
    source_position_rotation(entity, x, y, z);
    s->player_pc = 132;
    mc_entity_construction_retire(entity, coordinate, sizeof(*coordinate));
    s->living.Y = 20;
    s->player_pc = 138;
    s->living.R = mc_player_model_literal();
    s->player_pc = 144;
    s->living.Q = 180.0f;
    s->player_pc = 150;
    entity->base.bu = 20;
    s->player_pc = 156;
    s->living.O = mc_player_texture_literal();
    s->player_pc = 165;
    return MC_PLAYER_OK;
}

static mc_player_status local_construct(mc_entity *entity, void *minecraft,
                                        const mc_player_session *session, int32_t dimension,
                                        mc_player_error *error) {
    mc_local_player_snapshot *s = &entity->local_player->fields;
    static const unsigned int store_pcs[] = {13, 24, 35};
    for (size_t i = 0; i < 3; ++i) {
        mc_player_mouse_filter_fields *filter = mc_entity_construction_allocate(
            entity, sizeof(*filter), &error->entity, "local Player MouseFilter allocation",
            (uint16_t)(10u + (unsigned int)i * 11u));
        if (filter == NULL)
            return mc_player_fail(
                error,
                error->entity.status == MC_ENTITY_LIMIT ? MC_PLAYER_LIMIT
                                                        : MC_PLAYER_ALLOCATION_FAILED,
                "local Player MouseFilter allocation", "dc", "<init>",
                "(Lnet/minecraft/client/Minecraft;Lfd;Lgr;I)V", 6u + (unsigned int)i * 11u);
        *filter = (mc_player_mouse_filter_fields){0};
        s->filters[i] = filter;
        s->local_pc = store_pcs[i];
    }
    s->minecraft = minecraft;
    s->local_pc = 40;
    s->player.m = dimension;
    s->local_pc = 46;
    if (session != NULL && session->username != NULL && session->username->length != 0) {
        mc_player_string *skin = NULL;
        const mc_player_status status = mc_player_skin_string(session->username, &skin);
        if (status != MC_PLAYER_OK)
            return mc_player_fail(error, status, "local Player skin String construction", "dc",
                                  "<init>", "(Lnet/minecraft/client/Minecraft;Lfd;Lgr;I)V", 95);
        s->skin_url = skin;
        entity->base.client_only.bA = skin;
        s->local_pc = 98;
    }
    if (session == NULL)
        return mc_player_fail(error, MC_PLAYER_NULL_REFERENCE,
                              "local Player null Session username read", "dc", "<init>",
                              "(Lnet/minecraft/client/Minecraft;Lfd;Lgr;I)V", 103);
    const mc_player_status status = mc_player_string_retain(session->username);
    if (status != MC_PLAYER_OK)
        return mc_player_fail(error, status, "local Player username String hold", "dc", "<init>",
                              "(Lnet/minecraft/client/Minecraft;Lfd;Lgr;I)V", 106);
    s->player.l = session->username;
    s->local_pc = 109;
    return MC_PLAYER_OK;
}

mc_player_status mc_local_player_construct(mc_entity *entity, mc_world_runtime *world,
                                           void *minecraft, const mc_player_session *session,
                                           int32_t dimension, const mc_inventory_limits *limits,
                                           mc_player_error *out) {
    mc_player_error error = {0};
    if (entity == NULL || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        mc_player_fail(&error, MC_PLAYER_INVALID_ARGUMENT, "local Player native owner", NULL, NULL,
                       NULL, 0);
        goto done;
    }
    if (entity->phase != MC_ENTITY_RAW) {
        mc_player_fail(&error, MC_PLAYER_INVALID_PHASE, "local Player RAW admission", NULL, NULL,
                       NULL, 0);
        goto done;
    }
    if (session != NULL &&
        !mc_player_session_matches(session, entity->classes, mc_world_runtime_registry(world))) {
        mc_player_fail(&error, MC_PLAYER_INVALID_ARGUMENT, "local Player exact Session namespace",
                       NULL, NULL, NULL, 0);
        goto done;
    }
    if (world != NULL && mc_world_runtime_entities(world) != entity->classes) {
        mc_player_fail(&error, MC_PLAYER_INVALID_ARGUMENT,
                       "local Player exact World Entity namespace", NULL, NULL, NULL, 0);
        goto done;
    }
    mc_entity_status status = mc_entity_constructor_begin(
        entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER, world, "local Player constructor",
        "local Player World profile", &error.entity);
    if (status != MC_ENTITY_OK) {
        mc_player_fail(&error, MC_PLAYER_ENTITY_FAILED, "local Player constructor admission", NULL,
                       NULL, NULL, 0);
        goto done;
    }
    status = mc_entity_construct_base(entity, world, &error.entity);
    if (status != MC_ENTITY_OK)
        mc_player_fail(&error, MC_PLAYER_ENTITY_FAILED, "local Player base Entity construction",
                       "ls", "<init>", "(Lfd;)V", 2);
    else if (living_construct(entity, &error) == MC_PLAYER_OK &&
             player_construct(entity, world, limits, &error) == MC_PLAYER_OK)
        (void)local_construct(entity, minecraft, session, dimension, &error);
    entity->phase = error.status == MC_PLAYER_OK ? MC_ENTITY_READY : MC_ENTITY_FAILED_CONSTRUCTION;
done:
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_entity_status mc_local_player_get_snapshot(const mc_entity *entity,
                                              mc_local_player_snapshot *out) {
    if (entity == NULL || out == NULL ||
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = entity->local_player->fields;
    return MC_ENTITY_OK;
}

mc_player_status mc_player_is_sleeping(mc_entity *entity, bool *out, mc_player_error *out_error) {
    mc_player_error error = {0};
    if (out == NULL)
        mc_player_fail(&error, MC_PLAYER_INVALID_ARGUMENT, "Player sleeping output", NULL, NULL,
                       NULL, 0);
    else if (entity == NULL)
        mc_player_fail(&error, MC_PLAYER_NULL_REFERENCE, "Player sleeping receiver", "gs", "N",
                       "()Z", 1);
    else if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        mc_player_fail(&error, MC_PLAYER_INVALID_ARGUMENT, "Player sleeping concrete owner", NULL,
                       NULL, NULL, 0);
    else {
        mc_entity_status held = mc_entity_retain(entity);
        if (held != MC_ENTITY_OK) {
            error.entity =
                (mc_entity_error){.status = held, .operation = "Player sleeping Entity hold"};
            mc_player_fail(&error, MC_PLAYER_ENTITY_FAILED, error.entity.operation, NULL, NULL,
                           NULL, 0);
        } else {
            *out = entity->local_player->fields.player.u;
            mc_entity_release(entity);
        }
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

void mc_local_player_dispose(mc_entity *entity) {
    mc_player_nbt_coordinates_dispose(entity);
    mc_local_player_snapshot *s = &entity->local_player->fields;
    /* Drop this field's native occurrence hold, without a source input reset. */
    (void)mc_movement_input_retire(&s->input, NULL);
    /* Constructor aliases d/e own one Container, with a borrowed inventory
     * back-link. Native teardown is quiescent; no source close/drop is called. */
    (void)mc_container_retire(&s->player.d, NULL);
    s->player.e = NULL;
    (void)mc_inventory_player_retire(&s->player.c, NULL);
    mc_player_string_release((mc_player_string *)s->skin_url);
    mc_player_string_release((mc_player_string *)s->player.l);
    mc_player_string_release((mc_player_string *)s->player.n);
    mc_player_string_release((mc_player_string *)entity->base.client_only.bB);
    for (size_t i = 0; i < 3; ++i)
        mc_entity_construction_retire(entity, (void *)s->filters[i], sizeof(*s->filters[i]));
}
