#include "nbt_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../construction_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/entity/player/sleep.h"
#include "mc/inventory/runtime/nbt.h"
#include "mc/world/runtime/placement.h"

#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static const uint16_t health[] = {'H', 'e', 'a', 'l', 't', 'h'};
static const uint16_t hurt[] = {'H', 'u', 'r', 't', 'T', 'i', 'm', 'e'};
static const uint16_t death[] = {'D', 'e', 'a', 't', 'h', 'T', 'i', 'm', 'e'};
static const uint16_t attack[] = {'A', 't', 't', 'a', 'c', 'k', 'T', 'i', 'm', 'e'};
static const uint16_t inventory[] = {'I', 'n', 'v', 'e', 'n', 't', 'o', 'r', 'y'};
static const uint16_t dimension[] = {'D', 'i', 'm', 'e', 'n', 's', 'i', 'o', 'n'};
static const uint16_t sleeping[] = {'S', 'l', 'e', 'e', 'p', 'i', 'n', 'g'};
static const uint16_t sleep_timer[] = {'S', 'l', 'e', 'e', 'p', 'T', 'i', 'm', 'e', 'r'};
static const uint16_t spawn_x[] = {'S', 'p', 'a', 'w', 'n', 'X'};
static const uint16_t spawn_y[] = {'S', 'p', 'a', 'w', 'n', 'Y'};
static const uint16_t spawn_z[] = {'S', 'p', 'a', 'w', 'n', 'Z'};
static const uint16_t score[] = {'S', 'c', 'o', 'r', 'e'};

typedef struct player_key {
    const uint16_t *name;
    size_t length;
    const char *field;
} player_key;

#define KEY(n, text) ((player_key){n, sizeof(n) / sizeof(*n), text})

static bool fail(mc_entity_nbt_error *e, mc_entity_nbt_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    return false;
}

struct mc_entity_nbt_sleep_work {
    mc_player_sleep_call *call;
    mc_registry_classes *entry_registry;
    mc_player_sleep_error error;
    mc_player_sleep_cleanup child_cleanup;
    mc_entity_nbt_sleep_cleanup cleanup;
};

static mc_world_storage_status nbt_sleep_registry_status(mc_registry_status status) {
    return status == MC_REGISTRY_OK                  ? MC_WORLD_STORAGE_OK
           : status == MC_REGISTRY_LIMIT             ? MC_WORLD_STORAGE_LIMIT
           : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : status == MC_REGISTRY_BUSY              ? MC_WORLD_STORAGE_BUSY
                                                     : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status nbt_sleep_cleanup_status(const mc_player_sleep_cleanup *cleanup) {
    if (cleanup->status == MC_PLAYER_SLEEP_BUSY)
        return MC_WORLD_STORAGE_BUSY;
    if (cleanup->status == MC_PLAYER_SLEEP_LIMIT)
        return MC_WORLD_STORAGE_LIMIT;
    if (cleanup->status == MC_PLAYER_SLEEP_ALLOCATION_FAILED)
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    if (cleanup->status != MC_PLAYER_SLEEP_CLEANUP_FAILED)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    switch (cleanup->child) {
        case MC_PLAYER_SLEEP_CLEANUP_QUERY:
        case MC_PLAYER_SLEEP_CLEANUP_WORLD:
            return cleanup->world.status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                                                : cleanup->world.status;
        case MC_PLAYER_SLEEP_CLEANUP_REGISTRY:
            return cleanup->registry.status == MC_REGISTRY_OK
                       ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                       : nbt_sleep_registry_status(cleanup->registry.status);
        case MC_PLAYER_SLEEP_CLEANUP_OCCUPANCY:
            return cleanup->occupancy == NULL ||
                           cleanup->occupancy->cleanup.status == MC_WORLD_STORAGE_OK
                       ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                       : cleanup->occupancy->cleanup.status;
        case MC_PLAYER_SLEEP_CLEANUP_COORDINATE:
            return cleanup->coordinate == MC_PLAYER_BUSY    ? MC_WORLD_STORAGE_BUSY
                   : cleanup->coordinate == MC_PLAYER_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                            : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        default:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
}

static bool nbt_sleep_child_retire(mc_entity_nbt_error *e) {
    mc_entity_nbt_sleep_work *work = e->sleep_work;
    if (work == NULL)
        return true;
    work->cleanup = (mc_entity_nbt_sleep_cleanup){0};
    e->sleep_cleanup = NULL;
    if (mc_player_sleep_retire(&work->call, &work->child_cleanup) != MC_PLAYER_SLEEP_OK) {
        mc_player_sleep_state state;
        if (mc_player_sleep_inspect(work->call, &state) && state.failure != NULL)
            work->error = *state.failure;
        work->cleanup.status = MC_ENTITY_NBT_CLEANUP_FAILED;
        work->cleanup.operation = work->child_cleanup.operation;
        work->cleanup.child = MC_ENTITY_NBT_SLEEP_CLEANUP_WAKE;
        work->cleanup.wake = &work->child_cleanup;
        work->cleanup.cause = (mc_world_storage_error){
            nbt_sleep_cleanup_status(&work->child_cleanup), work->cleanup.operation, 0};
        if (work->child_cleanup.child == MC_PLAYER_SLEEP_CLEANUP_REGISTRY)
            work->cleanup.registry = work->child_cleanup.registry;
        e->sleep = &work->error;
        e->sleep_cleanup = &work->cleanup;
        return false;
    }
    /* End each exact producer borrow before any independent entry Registry
     * cleanup. Keep this small owner shell through the later NBT reads; their
     * actual failure must not be mistaken for a failed/completed wake receipt. */
    work->error.acquisition = NULL;
    work->error.occupancy = NULL;
    work->error.bed = NULL;
    work->error.world_sleep = NULL;
    work->error.position = NULL;
    /* A failed wake keeps its independent scalar cause in this stable work
     * even after its producers end. A successful wake must not classify a
     * later malformed Spawn/Score getter as that earlier child operation. */
    e->sleep = work->error.status == MC_PLAYER_SLEEP_OK ? NULL : &work->error;
    return true;
}

void mc_player_nbt_sleep_adopt_registry(mc_entity_nbt_error *e, mc_registry_classes *registry) {
    /* The one entered Entity reader moves its already-owned occurrence once.
     * This operation has no allocation, active use, callback or source read. */
    e->sleep_work->entry_registry = registry;
}

bool mc_entity_nbt_error_retire(mc_entity_nbt_error *e) {
    if (e == NULL)
        return false;
    mc_entity_nbt_sleep_work *work = e->sleep_work;
    if (work == NULL)
        return true;
    if (!nbt_sleep_child_retire(e))
        return false;
    if (work->entry_registry != NULL) {
        const mc_registry_status status =
            mc_registry_classes_release(work->entry_registry, &work->cleanup.registry);
        if (status != MC_REGISTRY_OK) {
            work->cleanup.status = MC_ENTITY_NBT_CLEANUP_FAILED;
            work->cleanup.operation = "Player NBT entry Registry checked retirement";
            work->cleanup.child = MC_ENTITY_NBT_SLEEP_CLEANUP_REGISTRY;
            work->cleanup.cause = (mc_world_storage_error){nbt_sleep_registry_status(status),
                                                           work->cleanup.operation, 0};
            e->sleep_cleanup = &work->cleanup;
            return false;
        }
        work->entry_registry = NULL;
    }
    free(work);
    e->sleep_work = NULL;
    e->sleep = NULL;
    e->sleep_cleanup = NULL;
    return true;
}

static mc_entity_nbt_status nbt_sleep_failure_status(const mc_player_sleep_error *error) {
    if (error->status == MC_PLAYER_SLEEP_LIMIT || error->world.status == MC_WORLD_STORAGE_LIMIT)
        return MC_ENTITY_NBT_LIMIT;
    if (error->status == MC_PLAYER_SLEEP_ALLOCATION_FAILED ||
        error->world.status == MC_WORLD_STORAGE_ALLOCATION_FAILED)
        return MC_ENTITY_NBT_ALLOCATION_FAILED;
    if (error->status == MC_PLAYER_SLEEP_BUSY || error->world.status == MC_WORLD_STORAGE_BUSY)
        return MC_ENTITY_NBT_BUSY;
    if (error->status == MC_PLAYER_SLEEP_SOURCE_NULL && !error->native_policy)
        return MC_ENTITY_NBT_NULL_REFERENCE;
    return MC_ENTITY_NBT_SLEEP_FAILED;
}

static bool nbt_sleep_wake(mc_entity *p, mc_registry_classes *registry, bool can_transfer_children,
                           mc_entity_nbt_error *e) {
    if (!can_transfer_children)
        return fail(e, MC_ENTITY_NBT_UNSUPPORTED_TYPE,
                    "Player NBT actual wake(true,true,false) dependency");
    mc_entity_nbt_sleep_work *work = calloc(1, sizeof(*work));
    if (work == NULL)
        return fail(e, MC_ENTITY_NBT_ALLOCATION_FAILED, "Player NBT retained wake workspace");
    e->sleep_work = work;
    e->sleep = &work->error;
    const mc_player_sleep_context context = {.code_registry = registry};
    const mc_player_sleep_status status =
        mc_player_wake_begin(p, true, true, false, &context, &work->call, &work->error);
    if (status != MC_PLAYER_SLEEP_OK)
        return fail(e, nbt_sleep_failure_status(&work->error), work->error.operation);
    mc_player_sleep_state state;
    if (work->call == NULL || !mc_player_sleep_inspect(work->call, &state) || !state.returned)
        return fail(e, MC_ENTITY_NBT_SLEEP_FAILED, "Player NBT actual wake returned child");
    if (!nbt_sleep_child_retire(e))
        return fail(e, MC_ENTITY_NBT_CLEANUP_FAILED, work->cleanup.operation);
    /* Actual wake has returned and owns no child. The independent parent
     * entry Registry is adopted only by the enclosing reader, after the same
     * source body continues through its fresh Spawn and DC Score operations. */
    return true;
}

static bool get(const mc_nbt_tag *compound, player_key key, mc_nbt_type type,
                const mc_nbt_tag **out, mc_entity_nbt_error *e) {
    e->field = key.field;
    if (compound == NULL)
        return fail(e, MC_ENTITY_NBT_NULL_REFERENCE, "Player NBT Compound receiver");
    if (compound->type != MC_NBT_COMPOUND)
        return fail(e, MC_ENTITY_NBT_WRONG_TAG, "Player NBT Compound view type");
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, key.name, key.length);
    if (tag != NULL && tag->type != type)
        return fail(e, MC_ENTITY_NBT_WRONG_TAG, "Player NBT named tag cast");
    *out = tag;
    return true;
}

static bool inventory_result(mc_inventory_nbt_status status, mc_entity_nbt_error *e) {
    if (status == MC_INVENTORY_NBT_OK)
        return true;
    e->index = e->player_inventory.index;
    if (e->player_inventory.field != NULL)
        e->field = e->player_inventory.field;
    return fail(e,
                status == MC_INVENTORY_NBT_LIMIT                 ? MC_ENTITY_NBT_LIMIT
                : status == MC_INVENTORY_NBT_ALLOCATION_FAILED   ? MC_ENTITY_NBT_ALLOCATION_FAILED
                : status == MC_INVENTORY_NBT_NULL_REFERENCE      ? MC_ENTITY_NBT_NULL_REFERENCE
                : status == MC_INVENTORY_NBT_WRONG_TAG           ? MC_ENTITY_NBT_WRONG_TAG
                : status == MC_INVENTORY_NBT_INDEX_OUT_OF_BOUNDS ? MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS
                : status == MC_INVENTORY_NBT_REGISTRY_FAILED     ? MC_ENTITY_NBT_REGISTRY_FAILED
                : status == MC_INVENTORY_NBT_STACK_FAILED        ? MC_ENTITY_NBT_STACK_FAILED
                                                                 : MC_ENTITY_NBT_NBT_FAILED,
                "Player NBT actual Inventory operation");
}

/* Native ownership nodes wrap real br values; they are not source fields or
 * an arena. Replacing a field immediately retires nodes no longer referenced
 * by this Player's three coordinate fields or an entered source-local pin. */

static struct mc_player_nbt_coordinate *coordinate_new(mc_entity *p, mc_entity_nbt_error *e) {
    struct mc_player_nbt_coordinate *value = mc_entity_construction_allocate(
        p, sizeof(*value), &e->construction, "Player NBT actual br allocation", 0);
    if (value == NULL)
        fail(e,
             e->construction.status == MC_ENTITY_LIMIT ? MC_ENTITY_NBT_LIMIT
                                                       : MC_ENTITY_NBT_ALLOCATION_FAILED,
             "Player NBT actual br allocation");
    else
        *value = (struct mc_player_nbt_coordinate){0};
    return value;
}

void mc_player_nbt_coordinates_collect_held(mc_entity *p) {
    mc_local_player_state *s = p->local_player;
    struct mc_player_nbt_coordinate **link = &s->nbt_coordinates;
    while (*link != NULL) {
        struct mc_player_nbt_coordinate *node = *link;
        const mc_player_coordinates *value = &node->value;
        if (node->pins != 0 || value == s->fields.player.v || value == s->fields.player.private_b ||
            value == s->fields.player.private_bN)
            link = &node->next;
        else {
            *link = node->next;
            mc_entity_construction_retire(p, node, sizeof(*node));
        }
    }
}

void mc_player_nbt_coordinate_store_held(mc_entity *p, struct mc_player_nbt_coordinate *node,
                                         const mc_player_coordinates **field) {
    node->next = p->local_player->nbt_coordinates;
    p->local_player->nbt_coordinates = node;
    *field = &node->value;
    mc_player_nbt_coordinates_collect_held(p);
}

void mc_player_nbt_coordinates_dispose(mc_entity *p) {
    struct mc_player_nbt_coordinate *node = p->local_player->nbt_coordinates;
    while (node != NULL) {
        struct mc_player_nbt_coordinate *next = node->next;
        mc_entity_construction_retire(p, node, sizeof(*node));
        node = next;
    }
    p->local_player->nbt_coordinates = NULL;
}

static bool living_read(mc_entity *p, const mc_nbt_tag *compound, mc_entity_nbt_error *e) {
    mc_living_entity_fields *v = &p->local_player->fields.living;
    const mc_nbt_tag *tag;
    e->living_bytecode = 4;
    if (!get(compound, KEY(health, "Health"), MC_NBT_SHORT, &tag, e))
        return false;
    v->Y = tag == NULL ? 0 : tag->value.short_value; /* source store7 */
    e->living_bytecode = 13;
    if (mc_nbt_compound_get(compound, health, sizeof health / sizeof *health) == NULL)
        v->Y = 10; /* Source default22 follows the first zero store. */
    e->living_bytecode = 29;
    if (!get(compound, KEY(hurt, "HurtTime"), MC_NBT_SHORT, &tag, e))
        return false;
    v->aa = tag == NULL ? 0 : tag->value.short_value;
    e->living_bytecode = 39;
    if (!get(compound, KEY(death, "DeathTime"), MC_NBT_SHORT, &tag, e))
        return false;
    v->ad = tag == NULL ? 0 : tag->value.short_value;
    e->living_bytecode = 49;
    if (!get(compound, KEY(attack, "AttackTime"), MC_NBT_SHORT, &tag, e))
        return false;
    v->ae = tag == NULL ? 0 : tag->value.short_value;
    e->living_bytecode = 55;
    return true;
}

static bool player_read(mc_entity *p, mc_registry_classes *classes, const mc_nbt_tag *compound,
                        const mc_nbt_limits *limits, bool can_transfer_children,
                        mc_entity_nbt_error *e) {
    mc_entity_nbt_select_player_detail(e);
    e->local_player_bytecode = 2;
    e->player_bytecode = 2;
    if (!living_read(p, compound, e))
        return false;
    mc_player_fields *v = &p->local_player->fields.player;
    const mc_nbt_tag *tag;
    e->player_bytecode = 8;
    if (!get(compound, KEY(inventory, "Inventory"), MC_NBT_LIST, &tag, e))
        return false;
    const mc_nbt_tag empty = {.type = MC_NBT_LIST};
    e->player_bytecode = 17;
    if (!inventory_result(mc_inventory_player_read_nbt(v->c, classes, tag == NULL ? &empty : tag,
                                                       limits, &e->player_inventory),
                          e))
        return false;
    e->player_bytecode = 24;
    if (!get(compound, KEY(dimension, "Dimension"), MC_NBT_INT, &tag, e))
        return false;
    v->m = tag == NULL ? 0 : tag->value.int_value;
    e->player_bytecode = 34;
    if (!get(compound, KEY(sleeping, "Sleeping"), MC_NBT_BYTE, &tag, e))
        return false;
    v->u = tag != NULL && tag->value.byte_value != 0;
    e->player_bytecode = 44;
    if (!get(compound, KEY(sleep_timer, "SleepTimer"), MC_NBT_SHORT, &tag, e))
        return false;
    v->private_a = tag == NULL ? 0 : tag->value.short_value;
    if (v->u) {
        e->player_bytecode = 58;
        struct mc_player_nbt_coordinate *node = coordinate_new(p, e);
        if (node == NULL)
            return false;
        e->player_bytecode = 66;
        /* A direct dc.a caller need not have entered the base pose reader.
         * Inventory class work may also precede this first reached floor.
         * Keep allocation58 and all earlier scalar/array stores, but never
         * execute the source compare with native DAZ/rounding controls. */
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            mc_entity_construction_retire(p, node, sizeof(*node));
            return fail(e, MC_ENTITY_NBT_UNSUPPORTED_FP,
                        "Player NBT reached bed-coordinate floor floating environment");
        }
        node->value.x = mc_math_floor_double(p->base.x);
        e->player_bytecode = 73;
        node->value.y = mc_math_floor_double(p->base.y);
        e->player_bytecode = 80;
        node->value.z = mc_math_floor_double(p->base.z);
        e->player_bytecode = 86;
        mc_player_nbt_coordinate_store_held(p, node, &v->v);
        e->player_bytecode = 93;
        if (!nbt_sleep_wake(p, classes, can_transfer_children, e))
            return false;
    }
    if (mc_nbt_compound_get(compound, spawn_x, 6) != NULL &&
        mc_nbt_compound_get(compound, spawn_y, 6) != NULL &&
        mc_nbt_compound_get(compound, spawn_z, 6) != NULL) {
        e->player_bytecode = 124;
        struct mc_player_nbt_coordinate *node = coordinate_new(p, e);
        if (node == NULL)
            return false;
        const player_key keys[] = {KEY(spawn_x, "SpawnX"), KEY(spawn_y, "SpawnY"),
                                   KEY(spawn_z, "SpawnZ")};
        int32_t *values[] = {&node->value.x, &node->value.y, &node->value.z};
        for (size_t i = 0; i < 3; ++i) {
            e->player_bytecode = 131u + (unsigned int)i * 6u;
            if (!get(compound, keys[i], MC_NBT_INT, &tag, e)) {
                mc_entity_construction_retire(p, node, sizeof(*node));
                return false;
            }
            *values[i] = tag == NULL ? 0 : tag->value.int_value;
        }
        e->player_bytecode = 149;
        mc_player_nbt_coordinate_store_held(p, node, &v->private_b);
    }
    e->player_bytecode = 152;
    e->local_player_bytecode = 9;
    if (!get(compound, KEY(score, "Score"), MC_NBT_INT, &tag, e))
        return false;
    v->g = tag == NULL ? 0 : tag->value.int_value;
    e->local_player_bytecode = 15;
    return true;
}

bool mc_player_nbt_read_held(mc_entity *p, mc_registry_classes *classes, const mc_nbt_tag *compound,
                             const mc_nbt_limits *limits, mc_entity_nbt_error *e) {
    return player_read(p, classes, compound, limits, false, e);
}

bool mc_player_nbt_read_with_child_output_held(mc_entity *p, mc_registry_classes *classes,
                                               const mc_nbt_tag *compound,
                                               const mc_nbt_limits *limits,
                                               bool can_transfer_children, mc_entity_nbt_error *e) {
    return player_read(p, classes, compound, limits, can_transfer_children, e);
}

static mc_nbt_tag scalar(player_key key, mc_nbt_type type, int32_t value) {
    mc_nbt_tag tag = {.type = type, .name = {key.name, key.length}};
    if (type == MC_NBT_SHORT)
        tag.value.short_value = mc_java_i16_narrow(value);
    else if (type == MC_NBT_BYTE)
        tag.value.byte_value = mc_java_i8_narrow(value);
    else
        tag.value.int_value = value;
    return tag;
}

bool mc_player_nbt_build_fields_held(mc_entity *p, const mc_nbt_limits *requested,
                                     mc_nbt_tree **out, mc_entity_nbt_error *e) {
    mc_entity_nbt_select_player_detail(e);
    mc_local_player_snapshot *s = &p->local_player->fields;
    mc_nbt_tag fields[12];
    const mc_nbt_tag *children[12];
    for (size_t i = 0; i < 12; ++i)
        children[i] = &fields[i];
    e->local_player_bytecode = 2;
    e->player_bytecode = 2;
    e->living_bytecode = 8;
    fields[0] = scalar(KEY(health, "Health"), MC_NBT_SHORT, s->living.Y);
    e->living_bytecode = 19;
    fields[1] = scalar(KEY(hurt, "HurtTime"), MC_NBT_SHORT, s->living.aa);
    e->living_bytecode = 30;
    fields[2] = scalar(KEY(death, "DeathTime"), MC_NBT_SHORT, s->living.ad);
    e->living_bytecode = 41;
    fields[3] = scalar(KEY(attack, "AttackTime"), MC_NBT_SHORT, s->living.ae);
    e->living_bytecode = 44;
    mc_nbt_tree *inventory_tree = NULL;
    e->player_bytecode = 19;
    if (!inventory_result(mc_inventory_player_build_nbt(s->player.c, requested, &inventory_tree,
                                                        &e->player_inventory),
                          e))
        return false;
    const mc_nbt_tag *root = mc_nbt_tree_root(inventory_tree);
    const mc_nbt_tag *list = mc_nbt_compound_get(root, inventory, 9);
    fields[4] = *list;
    e->player_bytecode = 32;
    fields[5] = scalar(KEY(dimension, "Dimension"), MC_NBT_INT, s->player.m);
    e->player_bytecode = 42;
    fields[6] = scalar(KEY(sleeping, "Sleeping"), MC_NBT_BYTE, s->player.u ? 1 : 0);
    e->player_bytecode = 53;
    fields[7] = scalar(KEY(sleep_timer, "SleepTimer"), MC_NBT_SHORT, s->player.private_a);
    size_t count = 8;
    if (s->player.private_b != NULL) {
        e->player_bytecode = 73;
        fields[count++] = scalar(KEY(spawn_x, "SpawnX"), MC_NBT_INT, s->player.private_b->x);
        e->player_bytecode = 86;
        fields[count++] = scalar(KEY(spawn_y, "SpawnY"), MC_NBT_INT, s->player.private_b->y);
        e->player_bytecode = 99;
        fields[count++] = scalar(KEY(spawn_z, "SpawnZ"), MC_NBT_INT, s->player.private_b->z);
    }
    e->player_bytecode = 102;
    e->local_player_bytecode = 12;
    fields[count++] = scalar(KEY(score, "Score"), MC_NBT_INT, s->player.g);
    const mc_nbt_tag result = {.type = MC_NBT_COMPOUND, .value.compound = {children, count}};
    mc_nbt_limits remaining = requested == NULL ? mc_nbt_default_limits() : *requested;
    const size_t retained = mc_nbt_tree_allocation_bytes(inventory_tree);
    bool completed = false;
    if (retained > remaining.max_allocation_bytes)
        fail(e, MC_ENTITY_NBT_LIMIT, "Player NBT retained Inventory tree budget");
    else {
        remaining.max_allocation_bytes -= retained;
        const mc_nbt_status copied = mc_nbt_tree_copy_compound(&result, &remaining, out, &e->nbt);
        if (copied == MC_NBT_OK)
            completed = true;
        else
            fail(e,
                 copied == MC_NBT_LIMIT               ? MC_ENTITY_NBT_LIMIT
                 : copied == MC_NBT_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                      : MC_ENTITY_NBT_NBT_FAILED,
                 "Player NBT independent own-field tree");
    }
    mc_nbt_tree_free(inventory_tree);
    return completed;
}
