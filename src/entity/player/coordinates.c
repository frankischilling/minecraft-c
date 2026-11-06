#include "mc/entity/player/coordinates.h"

#include "../construction_internal.h"
#include "nbt_internal.h"

struct mc_player_coordinate_pin {
    mc_entity *player;
    struct mc_player_nbt_coordinate *node;
};

mc_player_status mc_player_coordinate_pin_held(mc_entity *p, const mc_player_coordinates *value,
                                               struct mc_player_nbt_coordinate **out,
                                               mc_player_error *e) {
    mc_player_error_clear(e);
    if (out == NULL || *out != NULL || p == NULL ||
        !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        return mc_player_fail(e, MC_PLAYER_INVALID_ARGUMENT, "Player local coordinate pin owner",
                              NULL, NULL, NULL, 0);
    if (value == NULL)
        return MC_PLAYER_OK;
    struct mc_player_nbt_coordinate *node = p->local_player->nbt_coordinates;
    while (node != NULL && &node->value != value)
        node = node->next;
    if (node == NULL)
        return mc_player_fail(e, MC_PLAYER_INVALID_ARGUMENT, "Player local coordinate provenance",
                              NULL, NULL, NULL, 0);
    if (node->pins == SIZE_MAX)
        return mc_player_fail(e, MC_PLAYER_LIMIT, "Player local coordinate pin ceiling", NULL, NULL,
                              NULL, 0);
    ++node->pins;
    *out = node;
    return MC_PLAYER_OK;
}

mc_player_status mc_player_coordinate_unpin_admit_held(mc_entity *p,
                                                       const struct mc_player_nbt_coordinate *node,
                                                       size_t occurrences) {
    if (node == NULL)
        return occurrences == 0 ? MC_PLAYER_OK : MC_PLAYER_INVALID_ARGUMENT;
    if (p == NULL || !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        return MC_PLAYER_INVALID_ARGUMENT;
    const struct mc_player_nbt_coordinate *current = p->local_player->nbt_coordinates;
    while (current != NULL && current != node)
        current = current->next;
    return current != NULL && current->pins >= occurrences ? MC_PLAYER_OK
                                                           : MC_PLAYER_INVALID_ARGUMENT;
}

void mc_player_coordinate_unpin_commit_held(mc_entity *p, struct mc_player_nbt_coordinate **slot) {
    if (slot != NULL && *slot != NULL) {
        --(*slot)->pins;
        *slot = NULL;
        mc_player_nbt_coordinates_collect_held(p);
    }
}

mc_player_status mc_player_coordinate_new_held(mc_entity *p, mc_player_site site,
                                               const char *operation,
                                               struct mc_player_nbt_coordinate **out,
                                               mc_player_error *e) {
    mc_player_error_clear(e);
    if (p == NULL || out == NULL || *out != NULL ||
        !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        return mc_player_fail(e, MC_PLAYER_INVALID_ARGUMENT, "Player coordinate allocation owner",
                              NULL, NULL, NULL, 0);
    mc_entity_error native = {0};
    struct mc_player_nbt_coordinate *node =
        mc_entity_construction_allocate(p, sizeof(*node), &native, operation, (uint16_t)site.pc);
    if (node == NULL) {
        const mc_player_status status =
            native.status == MC_ENTITY_LIMIT ? MC_PLAYER_LIMIT : MC_PLAYER_ALLOCATION_FAILED;
        mc_player_fail(e, status, operation, site.class_name, site.method, site.descriptor,
                       site.pc);
        if (e != NULL)
            e->entity = native;
        return status;
    }
    *node = (struct mc_player_nbt_coordinate){0};
    *out = node;
    return MC_PLAYER_OK;
}

static mc_player_status publish(mc_player_error *out, const mc_player_error *error) {
    if (out != NULL)
        *out = *error;
    return error->status;
}

static bool hold(mc_entity *p, const char *method, const char *descriptor, unsigned int null_pc,
                 mc_player_error *e) {
    if (p == NULL) {
        mc_player_fail(e, MC_PLAYER_NULL_REFERENCE, "Player coordinate null receiver", "gs", method,
                       descriptor, null_pc);
        return false;
    }
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        mc_player_fail(e, MC_PLAYER_INVALID_ARGUMENT, "Player coordinate canonical receiver", NULL,
                       NULL, NULL, 0);
        return false;
    }
    const mc_entity_status status = mc_entity_retain(p);
    if (status == MC_ENTITY_OK)
        return true;
    e->entity = (mc_entity_error){.status = status, .operation = "Player coordinate local hold"};
    mc_player_fail(e, status == MC_ENTITY_LIMIT ? MC_PLAYER_LIMIT : MC_PLAYER_ENTITY_FAILED,
                   e->entity.operation, NULL, NULL, NULL, 0);
    return false;
}

mc_player_status mc_player_saved_spawn_get(mc_entity *p, mc_player_coordinate_pin **out,
                                           mc_player_error *out_error) {
    mc_player_error e = {0};
    if (out == NULL || *out != NULL) {
        mc_player_fail(&e, MC_PLAYER_INVALID_ARGUMENT, "Player coordinate empty output", NULL, NULL,
                       NULL, 0);
        return publish(out_error, &e);
    }
    if (p == NULL) {
        mc_player_fail(&e, MC_PLAYER_NULL_REFERENCE, "Player coordinate null receiver", "gs", "Q",
                       "()Lbr;", 1);
        return publish(out_error, &e);
    }
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        mc_player_fail(&e, MC_PLAYER_INVALID_ARGUMENT, "Player coordinate canonical receiver", NULL,
                       NULL, NULL, 0);
        return publish(out_error, &e);
    }
    const mc_player_coordinates *value = p->local_player->fields.player.private_b; /* GS1. */
    if (value == NULL)
        return publish(out_error, &e);
    if (!hold(p, "Q", "()Lbr;", 1, &e))
        return publish(out_error, &e);
    struct mc_player_nbt_coordinate *node = p->local_player->nbt_coordinates;
    while (node != NULL && &node->value != value)
        node = node->next;
    if (node == NULL)
        mc_player_fail(&e, MC_PLAYER_INVALID_ARGUMENT, "Player coordinate unowned field pointer",
                       "gs", "Q", "()Lbr;", 1);
    else if (node->pins == SIZE_MAX)
        mc_player_fail(&e, MC_PLAYER_LIMIT, "Player coordinate pin overflow", "gs", "Q", "()Lbr;",
                       1);
    else {
        mc_player_coordinate_pin *pin = mc_entity_construction_allocate(
            p, sizeof(*pin), &e.entity, "Player coordinate native pin allocation", 0);
        if (pin == NULL)
            mc_player_fail(&e,
                           e.entity.status == MC_ENTITY_LIMIT ? MC_PLAYER_LIMIT
                                                              : MC_PLAYER_ALLOCATION_FAILED,
                           e.entity.operation, "gs", "Q", "()Lbr;", 1);
        else {
            *pin = (mc_player_coordinate_pin){p, node};
            ++node->pins;
            *out = pin;
            return publish(out_error, &e); /* Pin owns this actual Player hold. */
        }
    }
    mc_entity_release(p);
    return publish(out_error, &e);
}

const mc_player_coordinates *mc_player_coordinate_pin_value(const mc_player_coordinate_pin *pin) {
    return pin == NULL ? NULL : &pin->node->value;
}

void mc_player_coordinate_pin_release(mc_player_coordinate_pin **out) {
    if (out == NULL || *out == NULL)
        return;
    mc_player_coordinate_pin *pin = *out;
    mc_entity *p = pin->player;
    --pin->node->pins;
    *out = NULL;
    mc_entity_construction_retire(p, pin, sizeof(*pin));
    mc_player_nbt_coordinates_collect_held(p);
    mc_entity_release(p);
}

mc_player_status mc_player_saved_spawn_set(mc_entity *p, const mc_player_coordinates *value,
                                           mc_player_error *out_error) {
    mc_player_error e = {0};
    if (!hold(p, "a", "(Lbr;)V", value == NULL ? 21u : 13u, &e))
        return publish(out_error, &e);
    if (value == NULL) {
        p->local_player->fields.player.private_b = NULL; /* GS21. */
        mc_player_nbt_coordinates_collect_held(p);
    } else {
        struct mc_player_nbt_coordinate *node = mc_entity_construction_allocate(
            p, sizeof(*node), &e.entity, "Player saved-spawn actual BR copy allocation", 5);
        if (node == NULL)
            mc_player_fail(&e,
                           e.entity.status == MC_ENTITY_LIMIT ? MC_PLAYER_LIMIT
                                                              : MC_PLAYER_ALLOCATION_FAILED,
                           e.entity.operation, "gs", "a", "(Lbr;)V", 5);
        else {
            *node = (struct mc_player_nbt_coordinate){0};
            /* Actual BR copy constructor reads/stores x6/9, y14/17, z22/25. */
            node->value.x = value->x;
            node->value.y = value->y;
            node->value.z = value->z;
            mc_player_nbt_coordinate_store_held(
                p, node, &p->local_player->fields.player.private_b); /* GS13. */
        }
    }
    mc_entity_release(p);
    return publish(out_error, &e);
}
