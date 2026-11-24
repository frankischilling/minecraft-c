#include "../classes/memory_internal.h"
#include "../construction_internal.h"
#include "entity_internal.h"
#include "inventory_internal.h"
#include "storage_internal.h"

#include <float.h>
#include <limits.h>

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Minecart construction requires strict floating transformations"
#endif

_Static_assert(CHAR_BIT == 8 && sizeof(float) == sizeof(uint32_t) &&
                   sizeof(double) == sizeof(uint64_t) && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 &&
                   FLT_EVAL_METHOD == 0,
               "strict binary32/binary64 evaluation required");
_Static_assert(36 <= (SIZE_MAX - sizeof(mc_minecart_stack_array)) / sizeof(mc_item_stack *),
               "Minecart inventory request must fit size_t");

static mc_minecart_site site(const mc_entity *entity, bool full, uint16_t pc) {
    const bool client = entity->classes->source == MC_ENTITY_CLIENT;
    return (mc_minecart_site){.selected = true,
                              .entered = true,
                              .declaring_class = client ? "yl" : "pr",
                              .method = full ? (client ? "<init>(Lfd;DDDI)V" : "<init>(Ldj;DDDI)V")
                                             : (client ? "<init>(Lfd;)V" : "<init>(Ldj;)V"),
                              .bytecode = pc};
}

mc_entity_status mc_minecart_entity_allocate(mc_entity_classes *classes, mc_entity **out,
                                             mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_MINECART,
                                        "minecart allocation", out, error);
}

static mc_entity_status world_prefix(mc_entity *entity, mc_world_runtime *world,
                                     mc_entity_error *error) {
    const mc_entity_status status = mc_entity_construct_base(entity, world, error);
    if (status != MC_ENTITY_OK) {
        error->minecart_world_constructor = site(entity, false, 2);
        return status;
    }
    struct mc_minecart_entity_state *state = entity->minecart;
    state->world_bytecode = 2;
    mc_minecart_stack_array *inventory;
    const mc_entity_status allocated = mc_minecart_inventory_array_allocate(
        entity, 36, "minecart constructor", 8, &inventory, error);
    if (allocated != MC_ENTITY_OK) {
        error->minecart_world_constructor = site(entity, false, 8);
        return allocated;
    }
    (void)mc_minecart_inventory_array_swap(entity, inventory);
    state->world_bytecode = 11;
    state->fields.a = 0;
    state->world_bytecode = 16;
    state->fields.b = 0;
    state->world_bytecode = 21;
    state->fields.c = 1;
    state->world_bytecode = 26;
    state->fields.private_i = false;
    state->world_bytecode = 31;
    entity->base.aF = true;
    state->world_bytecode = 36;
    entity->base.width = 0.98f;
    entity->base.height = 0.7f;
    state->world_bytecode = 44;
    entity->base.offset = entity->base.height / 2.0f;
    state->world_bytecode = 54;
    state->world_bytecode = 57;
    return MC_ENTITY_OK;
}

static mc_entity_status construct(mc_entity *entity, mc_world_runtime *world, double x, double y,
                                  double z, int32_t type, bool full, mc_entity_error *error) {
    mc_entity_error frame = {0};
    mc_entity_status status =
        mc_entity_constructor_begin(entity, MC_ENTITY_CLASS_KIND_MINECART, world,
                                    "minecart constructor", "minecart world profile", &frame);
    if (status == MC_ENTITY_OK) {
        status = world_prefix(entity, world, &frame);
        if (status != MC_ENTITY_OK) {
            if (full)
                frame.minecart_constructor = site(entity, true, 2);
        } else if (full) {
            struct mc_minecart_entity_state *state = entity->minecart;
            state->bytecode = 2;
            const double positioned_y = y + (double)entity->base.offset;
            mc_entity_source_position(entity, x, positioned_y, z);
            state->bytecode = 17;
            entity->base.velocity_x = 0.0;
            state->bytecode = 22;
            entity->base.velocity_y = 0.0;
            state->bytecode = 27;
            entity->base.velocity_z = 0.0;
            state->bytecode = 32;
            entity->base.aJ = x;
            state->bytecode = 37;
            entity->base.aK = y;
            state->bytecode = 43;
            entity->base.aL = z;
            state->bytecode = 49;
            state->fields.d = type;
            state->bytecode = 55;
            state->bytecode = 58;
        }
        status = mc_entity_constructor_finish(entity, status, &frame);
    }
    if (error != NULL)
        *error = frame;
    return status;
}

mc_entity_status mc_minecart_entity_construct_world(mc_entity *raw, mc_world_runtime *world,
                                                    mc_entity_error *error) {
    return construct(raw, world, 0.0, 0.0, 0.0, 0, false, error);
}

mc_entity_status mc_minecart_entity_construct(mc_entity *raw, mc_world_runtime *world, double x,
                                              double y, double z, int32_t type,
                                              mc_entity_error *error) {
    return construct(raw, world, x, y, z, type, true, error);
}

void mc_minecart_entity_dispose(mc_entity *entity) {
    mc_minecart_inventory_array_retire(entity, entity->minecart->fields.h);
    mc_minecart_storage_root_retire(entity);
}

mc_entity_status mc_minecart_entity_get_snapshot(const mc_entity *entity,
                                                 mc_minecart_entity_snapshot *out) {
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_MINECART) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    const struct mc_minecart_entity_state *state = entity->minecart;
    mc_minecart_entity_snapshot value = {.source = entity->classes->source,
                                         .class_literal = entity->class_literal,
                                         .phase = entity->phase,
                                         .world_bytecode = state->world_bytecode,
                                         .bytecode = state->bytecode,
                                         .fields = state->fields};
    if (state->fields.h != NULL) {
        if (state->fields.h->length != 27 && state->fields.h->length != 36)
            return MC_ENTITY_STATE_ERROR;
        value.inventory_length = state->fields.h->length;
        for (size_t i = 0; i < state->fields.h->length; ++i)
            value.inventory[i] = state->fields.h->elements[i];
    }
    *out = value;
    return MC_ENTITY_OK;
}
