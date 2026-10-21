#include "mc/world/runtime/plants.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "runtime_internal.h"

/* Concrete source receiver predicates over actual World and Registry owners. */
typedef struct plant_frame {
    mc_world_runtime *world;
    const mc_block *receiver;
    mc_world_plant_error *error;
} plant_frame;

static mc_registry_error empty_registry(void) {
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static mc_world_plant_error empty_error(void) {
    const mc_registry_error registry = empty_registry();
    return (mc_world_plant_error){.registry = registry,
                                  .query = {.registry = registry},
                                  .material = {.registry = registry},
                                  .support = {.registry = registry}};
}

static bool fail(mc_world_plant_error *error, mc_world_storage_status status,
                 const char *operation) {
    error->status = status;
    error->operation = operation;
    error->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool storage_fail(mc_world_plant_error *error, const mc_world_storage_error *cause) {
    error->status = cause->status;
    error->operation = cause->operation;
    error->cause = *cause;
    return false;
}

static bool registry_fail(plant_frame *frame, const mc_registry_error *cause,
                          const char *operation) {
    frame->error->registry = *cause;
    frame->error->status = mc_world_runtime_registry_result(frame->world, cause->status, cause,
                                                            operation, &frame->error->cause);
    frame->error->operation = frame->error->cause.operation;
    return false;
}

static bool null_reference(plant_frame *frame, const char *operation) {
    mc_registry_error cause = empty_registry();
    cause.status = MC_REGISTRY_NULL_REFERENCE;
    cause.message = operation;
    return registry_fail(frame, &cause, operation);
}

static void stage(plant_frame *frame, const char *client_class, const char *server_class,
                  const char *client_method, const char *server_method, unsigned int pc) {
    const bool client = frame->receiver->classes->options.side == MC_REGISTRY_CLIENT;
    frame->error->declaring_class = client ? client_class : server_class;
    frame->error->declaring_method = client ? client_method : server_method;
    frame->error->bytecode = pc;
}

static void caller(plant_frame *frame, const char *client_class, const char *server_class,
                   const char *client_method, const char *server_method, unsigned int pc) {
    const bool client = frame->receiver->classes->options.side == MC_REGISTRY_CLIENT;
    frame->error->caller_class = client ? client_class : server_class;
    frame->error->caller_method = client ? client_method : server_method;
    frame->error->caller_bytecode = pc;
}

static bool block_get(plant_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                      int32_t *out) {
    frame->error->bytecode = pc;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_block_get(frame->world, x, y, z, out, &cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame->error, &cause);
    return true;
}

static bool material_get(plant_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                         mc_material **out) {
    frame->error->bytecode = pc;
    if (mc_world_runtime_material_get_detailed(frame->world, x, y, z, out,
                                               &frame->error->material) == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->registry = frame->error->material.registry;
    return storage_fail(frame->error, &frame->error->material.cause);
}

/* Each static and following nullable receiver field remain separate source
 * operations. This does not cache a block ID across acquiring calls. */
static bool static_id(plant_frame *frame, const char *name, unsigned int load_pc,
                      unsigned int field_pc, int32_t *out) {
    frame->error->bytecode = load_pc;
    mc_registry_error cause = empty_registry();
    const mc_block *block = NULL;
    if (mc_registry_block_static_get(frame->receiver->classes, name, &block, &cause) !=
        MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Plant predicate Block static active use");
    frame->error->bytecode = field_pc;
    if (block == NULL)
        return null_reference(frame, "Plant predicate Block static receiver");
    *out = block->fields.bn;
    return true;
}

/* Shared exact prefix of uu/na.a and pumpkin fc/di.a. Their World ID query
 * precedes Block active use. Air does not initialize Block or read m/bA. */
static bool replaceable(plant_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    int32_t id;
    if (!block_get(frame, 5, x, y, z, &id))
        return false;
    if (id == 0) {
        *out = true;
        return true;
    }
    frame->error->bytecode = 15;
    mc_registry_error cause = empty_registry();
    mc_registry_block_m_view array;
    if (mc_registry_block_m_get(frame->receiver->classes, &array, &cause) != MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Plant placement Block.m active use");
    frame->error->bytecode = 20;
    const mc_block *selected = NULL;
    if (mc_registry_block_m_at(&array, id, &selected, &cause) != MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Plant placement Block.m element");
    frame->error->bytecode = 21;
    if (selected == NULL)
        return null_reference(frame, "Plant placement Block receiver");
    mc_material *material = selected->fields.bA;
    frame->error->bytecode = 24;
    if (material == NULL)
        return null_reference(frame, "Plant placement Material receiver");
    *out = mc_material_g(material);
    return true;
}

static bool base_can_place(plant_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    stage(frame, "uu", "na", "a", "a", 0);
    return replaceable(frame, x, y, z, out);
}

/* wb.d / nw.c dispatches on the ACTUAL retained receiver. Mushroom and dead
 * bush select their own substrate body; tall grass inherits wb/nw. */
static bool substrate(plant_frame *frame, int32_t id, bool *out) {
    int32_t candidate;
    switch (frame->receiver->kind) {
        case MC_BLOCK_TL_MD: {
            stage(frame, "tl", "md", "d", "c", 0);
            mc_registry_block_o_view array;
            mc_registry_error cause = empty_registry();
            if (mc_registry_block_o_get(frame->receiver->classes, &array, &cause) != MC_REGISTRY_OK)
                return registry_fail(frame, &cause, "Mushroom substrate Block.o active use");
            frame->error->bytecode = 4;
            if (mc_registry_block_o_at(&array, id, out, &cause) != MC_REGISTRY_OK)
                return registry_fail(frame, &cause, "Mushroom substrate Block.o element");
            return true;
        }
        case MC_BLOCK_JB_FZ:
            stage(frame, "jb", "fz", "d", "c", 0);
            if (!static_id(frame, "F", 1, 4, &candidate))
                return false;
            *out = id == candidate;
            return true;
        case MC_BLOCK_NI_IJ:
            stage(frame, "ni", "ij", "d", "c", 0);
            if (!static_id(frame, "aB", 1, 4, &candidate))
                return false;
            *out = id == candidate;
            return true;
        case MC_BLOCK_WB_NW:
        case MC_BLOCK_HE_EU:
        case MC_BLOCK_RU_KZ:
            stage(frame, "wb", "nw", "d", "c", 0);
            if (!static_id(frame, "v", 1, 4, &candidate))
                return false;
            if (id == candidate) {
                *out = true;
                return true;
            }
            if (!static_id(frame, "w", 11, 14, &candidate))
                return false;
            if (id == candidate) {
                *out = true;
                return true;
            }
            if (!static_id(frame, "aB", 21, 24, &candidate))
                return false;
            *out = id == candidate;
            return true;
        default:
            return fail(frame->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Plant substrate receiver kind");
    }
}

static bool flower_can_place(plant_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    bool accepted;
    if (!base_can_place(frame, x, y, z, &accepted)) {
        caller(frame, "wb", "nw", "a", "a", 6);
        return false;
    }
    if (!accepted) {
        *out = false;
        return true;
    }
    stage(frame, "wb", "nw", "a", "a", 20);
    int32_t below;
    if (!block_get(frame, 20, x, mc_java_i32_sub(y, 1), z, &below))
        return false;
    if (!substrate(frame, below, out)) {
        caller(frame, "wb", "nw", "a", "a", 23);
        return false;
    }
    return true;
}

static bool flower_survives(plant_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    stage(frame, "wb", "nw", "g", "f", 5);
    int32_t light;
    if (mc_world_runtime_plant_light(frame->world, x, y, z, &light, &frame->error->query) !=
        MC_WORLD_STORAGE_OK) {
        frame->error->registry = frame->error->query.registry;
        return storage_fail(frame->error, &frame->error->query.cause);
    }
    if (light < 8) {
        frame->error->bytecode = 18;
        bool visible;
        if (mc_world_runtime_sky_visible(frame->world, x, y, z, &visible, &frame->error->query) !=
            MC_WORLD_STORAGE_OK) {
            frame->error->registry = frame->error->query.registry;
            return storage_fail(frame->error, &frame->error->query.cause);
        }
        if (!visible) {
            *out = false;
            return true;
        }
    }
    int32_t below;
    if (!block_get(frame, 32, x, mc_java_i32_sub(y, 1), z, &below))
        return false;
    if (!substrate(frame, below, out)) {
        caller(frame, "wb", "nw", "g", "f", 35);
        return false;
    }
    return true;
}

static bool mushroom_survives(plant_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    stage(frame, "tl", "md", "g", "f", 0);
    if (y < 0 || y >= 128) {
        *out = false;
        return true;
    }
    frame->error->bytecode = 18;
    int32_t light;
    if (mc_world_runtime_plant_light(frame->world, x, y, z, &light, &frame->error->query) !=
        MC_WORLD_STORAGE_OK) {
        frame->error->registry = frame->error->query.registry;
        return storage_fail(frame->error, &frame->error->query.cause);
    }
    if (light >= 13) {
        *out = false;
        return true;
    }
    int32_t below;
    if (!block_get(frame, 34, x, mc_java_i32_sub(y, 1), z, &below))
        return false;
    if (!substrate(frame, below, out)) {
        caller(frame, "tl", "md", "g", "f", 37);
        return false;
    }
    return true;
}

static bool reed_water(plant_frame *frame, unsigned int query_pc, unsigned int static_pc, int32_t x,
                       int32_t y, int32_t z, bool *out) {
    mc_material *material;
    if (!material_get(frame, query_pc, x, y, z, &material))
        return false;
    frame->error->bytecode = static_pc;
    mc_material *water;
    mc_registry_error cause = empty_registry();
    if (mc_registry_material_get(frame->receiver->classes, MC_MATERIAL_g, &water, &cause) !=
        MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Reed placement water Material.g");
    /* A nullable queried material is a legal reference for identity compare. */
    *out = material == water;
    return true;
}

static bool reed_can_place(plant_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    stage(frame, "ri", "ko", "a", "a", 7);
    int32_t below;
    if (!block_get(frame, 7, x, mc_java_i32_sub(y, 1), z, &below))
        return false;
    frame->error->bytecode = 15;
    if (below == frame->receiver->fields.bn) {
        *out = true;
        return true;
    }
    int32_t candidate;
    if (!static_id(frame, "v", 25, 28, &candidate))
        return false;
    if (below != candidate) {
        if (!static_id(frame, "w", 36, 39, &candidate))
            return false;
        if (below != candidate) {
            *out = false;
            return true;
        }
    }
    const int32_t below_y = mc_java_i32_sub(y, 1);
    bool water;
    if (!reed_water(frame, 56, 59, mc_java_i32_sub(x, 1), below_y, z, &water))
        return false;
    if (water) {
        *out = true;
        return true;
    }
    if (!reed_water(frame, 76, 79, mc_java_i32_add(x, 1), below_y, z, &water))
        return false;
    if (water) {
        *out = true;
        return true;
    }
    if (!reed_water(frame, 96, 99, x, below_y, mc_java_i32_sub(z, 1), &water))
        return false;
    if (water) {
        *out = true;
        return true;
    }
    return reed_water(frame, 116, 119, x, below_y, mc_java_i32_add(z, 1), out);
}

static bool cactus_survives(plant_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    static const unsigned int query_pc[4] = {7, 25, 43, 61};
    static const unsigned int invoke_pc[4] = {10, 28, 46, 64};
    stage(frame, "or", "je", "g", "f", 0);
    for (size_t direction = 0; direction < 4; ++direction) {
        int32_t nx = x, nz = z;
        if (direction == 0)
            nx = mc_java_i32_sub(x, 1);
        else if (direction == 1)
            nx = mc_java_i32_add(x, 1);
        else if (direction == 2)
            nz = mc_java_i32_sub(z, 1);
        else
            nz = mc_java_i32_add(z, 1);
        mc_material *material;
        if (!material_get(frame, query_pc[direction], nx, y, nz, &material))
            return false;
        frame->error->bytecode = invoke_pc[direction];
        if (material == NULL)
            return null_reference(frame, "Cactus survival Material receiver");
        if (mc_material_a(material)) {
            *out = false;
            return true;
        }
    }
    int32_t below, candidate;
    if (!block_get(frame, 79, x, mc_java_i32_sub(y, 1), z, &below))
        return false;
    if (!static_id(frame, "aW", 86, 89, &candidate))
        return false;
    if (below == candidate) {
        *out = true;
        return true;
    }
    if (!static_id(frame, "F", 97, 100, &candidate))
        return false;
    *out = below == candidate;
    return true;
}

static bool cactus_can_place(plant_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    bool accepted;
    if (!base_can_place(frame, x, y, z, &accepted)) {
        caller(frame, "or", "je", "a", "a", 6);
        return false;
    }
    if (!accepted) {
        *out = false;
        return true;
    }
    if (!cactus_survives(frame, x, y, z, out)) {
        caller(frame, "or", "je", "a", "a", 20);
        return false;
    }
    return true;
}

static bool pumpkin_can_place(plant_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    stage(frame, "fc", "di", "a", "a", 0);
    bool accepted;
    if (!replaceable(frame, x, y, z, &accepted))
        return false;
    if (!accepted) {
        *out = false;
        return true;
    }
    frame->error->bytecode = 37;
    if (mc_world_runtime_solid_support_get(frame->world, x, mc_java_i32_sub(y, 1), z, out,
                                           &frame->error->support) != MC_WORLD_STORAGE_OK) {
        frame->error->registry = frame->error->support.registry;
        return storage_fail(frame->error, &frame->error->support.cause);
    }
    return true;
}

static bool supported(const mc_block *receiver) {
    switch (receiver->kind) {
        case MC_BLOCK_WB_NW:
        case MC_BLOCK_HE_EU:
        case MC_BLOCK_NI_IJ:
        case MC_BLOCK_RU_KZ:
        case MC_BLOCK_JB_FZ:
        case MC_BLOCK_TL_MD:
        case MC_BLOCK_RI_KO:
        case MC_BLOCK_OR_JE:
        case MC_BLOCK_FC_DI:
            return true;
        default:
            return false;
    }
}

static bool survives(plant_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    switch (frame->receiver->kind) {
        case MC_BLOCK_WB_NW:
        case MC_BLOCK_HE_EU:
        case MC_BLOCK_NI_IJ:
        case MC_BLOCK_RU_KZ:
        case MC_BLOCK_JB_FZ:
            return flower_survives(frame, x, y, z, out);
        case MC_BLOCK_TL_MD:
            return mushroom_survives(frame, x, y, z, out);
        case MC_BLOCK_RI_KO:
            if (!reed_can_place(frame, x, y, z, out)) {
                caller(frame, "ri", "ko", "g", "f", 6);
                return false;
            }
            return true;
        case MC_BLOCK_OR_JE:
            return cactus_survives(frame, x, y, z, out);
        case MC_BLOCK_FC_DI:
            /* Actual inherited uu.g/na.f body, not a default for other kinds. */
            stage(frame, "uu", "na", "g", "f", 0);
            *out = true;
            return true;
        default:
            return fail(frame->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Plant survival receiver kind");
    }
}

static bool can_place(plant_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    switch (frame->receiver->kind) {
        case MC_BLOCK_WB_NW:
        case MC_BLOCK_HE_EU:
        case MC_BLOCK_NI_IJ:
        case MC_BLOCK_RU_KZ:
        case MC_BLOCK_JB_FZ:
        case MC_BLOCK_TL_MD:
            return flower_can_place(frame, x, y, z, out);
        case MC_BLOCK_RI_KO:
            return reed_can_place(frame, x, y, z, out);
        case MC_BLOCK_OR_JE:
            return cactus_can_place(frame, x, y, z, out);
        case MC_BLOCK_FC_DI:
            return pumpkin_can_place(frame, x, y, z, out);
        default:
            return fail(frame->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Plant placement receiver kind");
    }
}

static mc_world_storage_status enter(mc_world_runtime *world, const mc_block *receiver, int32_t x,
                                     int32_t y, int32_t z, bool placement, bool *out,
                                     mc_world_plant_error *error) {
    mc_world_plant_error frame = empty_error();
    if (world == NULL || out == NULL)
        (void)fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Plant predicate arguments");
    else if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
             world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Plant predicate World phase");
    else if (receiver != NULL && !supported(receiver))
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Plant predicate receiver kind");
    else if (receiver != NULL && receiver->classes != mc_world_runtime_registry(world))
        (void)fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Plant predicate class binding");
    if (frame.status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    if (mc_world_runtime_call_enter(world, &frame.cause) != MC_WORLD_STORAGE_OK) {
        (void)storage_fail(&frame, &frame.cause);
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    bool value = false;
    if (receiver == NULL) {
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Plant predicate receiver is null");
    } else {
        plant_frame entered = {world, receiver, &frame};
        /* Both dispatchers set value only on complete normal source return. */
        if (placement)
            (void)can_place(&entered, x, y, z, &value);
        else
            (void)survives(&entered, x, y, z, &value);
    }
    if (frame.status == MC_WORLD_STORAGE_OK) {
        *out = value;
        frame = empty_error();
    } else {
        world->last_error = frame.cause;
    }
    mc_world_runtime_call_leave(world);
    if (error != NULL)
        *error = frame;
    return frame.status;
}

mc_world_storage_status mc_world_runtime_plant_survives(mc_world_runtime *world,
                                                        const mc_block *receiver, int32_t x,
                                                        int32_t y, int32_t z, bool *out,
                                                        mc_world_plant_error *error) {
    return enter(world, receiver, x, y, z, false, out, error);
}

mc_world_storage_status mc_world_runtime_plant_can_place(mc_world_runtime *world,
                                                         const mc_block *receiver, int32_t x,
                                                         int32_t y, int32_t z, bool *out,
                                                         mc_world_plant_error *error) {
    return enter(world, receiver, x, y, z, true, out, error);
}
