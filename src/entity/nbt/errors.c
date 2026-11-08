#include "mc/entity/nbt/errors.h"

static bool same_site(mc_minecart_site a, mc_minecart_site b) {
    return a.selected == b.selected && a.entered == b.entered &&
           a.declaring_class == b.declaring_class && a.method == b.method &&
           a.bytecode == b.bytecode;
}

static bool same_entity(const mc_entity_error *a, const mc_entity_error *b) {
    return a->status == b->status && a->operation == b->operation && a->bytecode == b->bytecode &&
           a->profile == b->profile && a->item == b->item && a->world == b->world &&
           same_site(a->minecart_world_constructor, b->minecart_world_constructor) &&
           same_site(a->minecart_constructor, b->minecart_constructor) &&
           same_site(a->minecart_initializer, b->minecart_initializer) &&
           same_site(a->minecart_initializer_cause, b->minecart_initializer_cause);
}

static bool same_registry(const mc_registry_error *a, const mc_registry_error *b) {
    const mc_java_format_error *x = &a->format_error, *y = &b->format_error;
    return a->status == b->status && a->message == b->message &&
           a->initializer_pc == b->initializer_pc && a->block_id == b->block_id &&
           a->constructor_class == b->constructor_class && a->constructor_pc == b->constructor_pc &&
           a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument && x->status == y->status &&
           x->offset == y->offset && x->length == y->length && x->value == y->value &&
           x->flags == y->flags && x->conversion == y->conversion && x->date_time == y->date_time;
}

static bool empty_lower(const mc_mob_error *call) {
    static const mc_entity_error entity = {0};
    static const mc_registry_error registry = {0};
    return same_entity(&call->entity, &entity) && same_registry(&call->registry, &registry) &&
           call->stack == MC_ITEM_STACK_OK;
}

static bool empty_first(const mc_mob_class_cause *first) {
    static const mc_entity_error entity = {0};
    static const mc_registry_error registry = {0};
    return first->status == MC_MOB_OK && first->operation == NULL &&
           first->site.class_name == NULL && first->site.method == NULL &&
           first->site.descriptor == NULL && first->site.pc == 0 &&
           first->child.class_name == NULL && first->child.method == NULL &&
           first->child.descriptor == NULL && first->child.pc == 0 && !first->native_policy &&
           !first->source_exception && first->exception_class == NULL &&
           same_entity(&first->entity, &entity) && same_registry(&first->registry, &registry) &&
           first->stack == MC_ITEM_STACK_OK;
}

static void restore_common(mc_entity_nbt_error *error) {
    if (error->late_hook == MC_ENTITY_NBT_LATE_ARROW_CALL) {
        const mc_entity_error entity = error->arrow_call.entity;
        error->construction = entity;
        error->stack = (mc_item_stack_error){.status = MC_ITEM_STACK_OK};
        error->nbt = (mc_nbt_error){0};
        error->registry = (mc_entity_registry_error){0};
        error->late_hook = MC_ENTITY_NBT_LATE_NONE;
        return;
    }
    if (error->late_hook != MC_ENTITY_NBT_LATE_MOB_CALL)
        return;
    mc_entity_error entity = {0};
    mc_item_stack_status stack = MC_ITEM_STACK_OK;
    if (error->mob_call.payload != MC_ENTITY_MOB_CALL_FIRST_PREVIOUS) {
        entity = error->mob_call.cause.entity;
        stack = error->mob_call.cause.stack;
    }
    error->construction = entity;
    error->stack = (mc_item_stack_error){.status = stack};
    error->nbt = (mc_nbt_error){0};
    error->registry = (mc_entity_registry_error){0};
    error->late_hook = MC_ENTITY_NBT_LATE_NONE;
}

void mc_entity_nbt_error_init(mc_entity_nbt_error *error) {
    static const mc_entity_nbt_error empty = {
        .status = MC_ENTITY_NBT_OK,
        .index = SIZE_MAX,
        .construction = {.status = MC_ENTITY_OK,
                         .profile = MC_JAVA_RANDOM_PROFILE_OK,
                         .item = MC_ITEM_STACK_OK,
                         .world = MC_WORLD_STORAGE_OK},
        .stack = {.status = MC_ITEM_STACK_OK, .nbt = {MC_NBT_OK, 0}},
        .nbt = {MC_NBT_OK, 0},
        .inventory = {.status = MC_MINECART_INVENTORY_OK}};
    if (error != NULL)
        *error = empty;
}

void mc_entity_nbt_select_cart_detail(mc_entity_nbt_error *error) {
    static const mc_minecart_site site = {0};
    static const mc_minecart_inventory_context inventory = {0};
    if (error == NULL || error->late_hook == MC_ENTITY_NBT_LATE_CART)
        return;
    restore_common(error);
    error->minecart_read = site;
    error->minecart_write = site;
    error->inventory = inventory;
    error->late_hook = MC_ENTITY_NBT_LATE_CART;
}

void mc_entity_nbt_select_player_detail(mc_entity_nbt_error *error) {
    static const mc_inventory_nbt_error inventory = {.index = SIZE_MAX,
                                                     .slot = -1,
                                                     .registry = {.initializer_pc = -1,
                                                                  .constructor_pc = -1,
                                                                  .block_id = -1,
                                                                  .item_argument = -1,
                                                                  .active_initializer_pc = -1}};
    if (error == NULL || error->late_hook == MC_ENTITY_NBT_LATE_PLAYER)
        return;
    restore_common(error);
    error->living_bytecode = 0;
    error->player_inventory = inventory;
    error->late_hook = MC_ENTITY_NBT_LATE_PLAYER;
}

mc_entity_nbt_error mc_entity_nbt_empty_error(void) {
    mc_entity_nbt_error error;
    mc_entity_nbt_error_init(&error);
    return error;
}

void mc_entity_nbt_select_mob_detail(mc_entity_nbt_error *error) {
    static const mc_entity_mob_nbt_detail empty = {
        .concrete = {.pc = -1}, .parent = {.pc = -1}, .child = {.pc = -1}, .watcher = {.pc = -1}};
    if (error == NULL || error->late_hook == MC_ENTITY_NBT_LATE_MOB)
        return;
    restore_common(error);
    error->mob = empty;
    error->late_hook = MC_ENTITY_NBT_LATE_MOB;
}

void mc_entity_nbt_select_arrow_detail(mc_entity_nbt_error *error) {
    static const mc_entity_arrow_nbt_detail empty = {.site = {.pc = -1}, .child = {.pc = -1}};
    if (error == NULL || error->late_hook == MC_ENTITY_NBT_LATE_ARROW)
        return;
    restore_common(error);
    error->arrow = empty;
    error->late_hook = MC_ENTITY_NBT_LATE_ARROW;
}

bool mc_entity_nbt_select_arrow_call_detail(mc_entity_nbt_error *error,
                                            const mc_arrow_error *call) {
    if (error == NULL || call == NULL || error->sleep_work != NULL)
        return false;
    if (error->late_hook != MC_ENTITY_NBT_LATE_ARROW_CALL) {
        static const mc_entity_error entity = {0};
        const mc_entity_registry_error *r = &error->registry;
        if (error->late_hook == MC_ENTITY_NBT_LATE_MOB_CALL ||
            !same_entity(&error->construction, &entity) ||
            error->stack.status != MC_ITEM_STACK_OK || error->stack.field != NULL ||
            error->stack.nbt.status != MC_NBT_OK || error->stack.nbt.offset != 0 ||
            error->nbt.status != MC_NBT_OK || error->nbt.offset != 0 ||
            r->status != MC_ENTITY_REGISTRY_OK || r->cause != MC_ENTITY_REGISTRY_OK ||
            r->operation != NULL || r->initializer_bytecode != 0 || r->registration_bytecode != 0 ||
            r->class_cause != MC_ENTITY_OK)
            return false;
    }
    error->arrow_call = *call;
    error->late_hook = MC_ENTITY_NBT_LATE_ARROW_CALL;
    return true;
}

bool mc_entity_nbt_arrow_call_error(const mc_entity_nbt_error *error, mc_arrow_error *out) {
    if (error == NULL || out == NULL || error->late_hook != MC_ENTITY_NBT_LATE_ARROW_CALL)
        return false;
    *out = error->arrow_call;
    return true;
}

bool mc_entity_nbt_select_mob_call_detail(mc_entity_nbt_error *error, const mc_mob_error *call) {
    if (error == NULL || call == NULL || error->sleep_work != NULL ||
        error->late_hook == MC_ENTITY_NBT_LATE_ARROW_CALL)
        return false;
    if (error->late_hook != MC_ENTITY_NBT_LATE_MOB_CALL) {
        static const mc_entity_error entity = {0};
        const mc_entity_registry_error *r = &error->registry;
        if (!same_entity(&error->construction, &entity) ||
            error->stack.status != MC_ITEM_STACK_OK || error->stack.field != NULL ||
            error->stack.nbt.status != MC_NBT_OK || error->stack.nbt.offset != 0 ||
            error->nbt.status != MC_NBT_OK || error->nbt.offset != 0 ||
            r->status != MC_ENTITY_REGISTRY_OK || r->cause != MC_ENTITY_REGISTRY_OK ||
            r->operation != NULL || r->initializer_bytecode != 0 || r->registration_bytecode != 0 ||
            r->class_cause != MC_ENTITY_OK)
            return false;
    }
    mc_entity_mob_call_payload payload;
    if (empty_first(&call->first_class_failure))
        payload = MC_ENTITY_MOB_CALL_CURRENT;
    else if (same_entity(&call->entity, &call->first_class_failure.entity) &&
             same_registry(&call->registry, &call->first_class_failure.registry) &&
             call->stack == call->first_class_failure.stack)
        payload = MC_ENTITY_MOB_CALL_FIRST_SHARED;
    else if (call->status == MC_MOB_CLASS_ERRONEOUS && empty_lower(call))
        payload = MC_ENTITY_MOB_CALL_FIRST_PREVIOUS;
    else
        return false;
    mc_entity_mob_call_detail *selected = &error->mob_call;
    selected->current.status = call->status;
    selected->current.operation = call->operation;
    selected->current.site = call->site;
    selected->current.child = call->child;
    selected->current.native_policy = call->native_policy;
    selected->current.source_exception = call->source_exception;
    selected->current.exception_class = call->exception_class;
    if (payload == MC_ENTITY_MOB_CALL_CURRENT) {
        selected->cause.status = call->status;
        selected->cause.operation = call->operation;
        selected->cause.site = call->site;
        selected->cause.child = call->child;
        selected->cause.native_policy = call->native_policy;
        selected->cause.source_exception = call->source_exception;
        selected->cause.exception_class = call->exception_class;
        selected->cause.entity = call->entity;
        selected->cause.registry = call->registry;
        selected->cause.stack = call->stack;
    } else
        selected->cause = call->first_class_failure;
    selected->payload = payload;
    error->late_hook = MC_ENTITY_NBT_LATE_MOB_CALL;
    return true;
}

bool mc_entity_nbt_mob_call_error(const mc_entity_nbt_error *error, mc_mob_error *out) {
    static const mc_mob_error empty = {0};
    if (error == NULL || out == NULL)
        return false;
    if (error->late_hook == MC_ENTITY_NBT_LATE_MOB &&
        error->mob.call_kind == MC_ENTITY_MOB_NBT_SLIME_SIZE) {
        *out = empty;
        out->status = error->mob.call_status;
        out->operation = error->operation;
        out->site = error->mob.child;
        out->child = error->mob.watcher;
        out->native_policy = error->mob.native_policy;
        out->source_exception = error->mob.source_exception;
        out->exception_class = error->mob.exception_class;
        out->entity = error->construction;
        out->stack = error->stack.status;
        return true;
    }
    if (error->late_hook != MC_ENTITY_NBT_LATE_MOB_CALL ||
        error->mob_call.payload < MC_ENTITY_MOB_CALL_CURRENT ||
        error->mob_call.payload > MC_ENTITY_MOB_CALL_FIRST_PREVIOUS)
        return false;
    const mc_entity_mob_call_context *c = &error->mob_call.current;
    *out = empty;
    out->status = c->status;
    out->operation = c->operation;
    out->site = c->site;
    out->child = c->child;
    out->native_policy = c->native_policy;
    out->source_exception = c->source_exception;
    out->exception_class = c->exception_class;
    if (error->mob_call.payload != MC_ENTITY_MOB_CALL_FIRST_PREVIOUS) {
        out->entity = error->mob_call.cause.entity;
        out->registry = error->mob_call.cause.registry;
        out->stack = error->mob_call.cause.stack;
    }
    if (error->mob_call.payload != MC_ENTITY_MOB_CALL_CURRENT)
        out->first_class_failure = error->mob_call.cause;
    return true;
}

const char *mc_entity_nbt_status_string(mc_entity_nbt_status status) {
    switch (status) {
        case MC_ENTITY_NBT_OK:
            return "ok";
        case MC_ENTITY_NBT_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_ENTITY_NBT_NULL_REFERENCE:
            return "null source reference";
        case MC_ENTITY_NBT_WRONG_TAG:
            return "wrong source tag type";
        case MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS:
            return "source List index out of bounds";
        case MC_ENTITY_NBT_LIMIT:
            return "resource limit";
        case MC_ENTITY_NBT_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_ENTITY_NBT_ENTITY_FAILED:
            return "Entity ownership failed";
        case MC_ENTITY_NBT_STACK_FAILED:
            return "ItemStack operation failed";
        case MC_ENTITY_NBT_NBT_FAILED:
            return "NBT operation failed";
        case MC_ENTITY_NBT_UNSUPPORTED_FP:
            return "unsupported floating environment";
        case MC_ENTITY_NBT_UNSUPPORTED_TYPE:
            return "unsupported concrete Entity type";
        case MC_ENTITY_NBT_REGISTRY_FAILED:
            return "EntityList operation failed";
        case MC_ENTITY_NBT_SLEEP_FAILED:
            return "Player wake operation failed";
        case MC_ENTITY_NBT_BUSY:
            return "native owner busy";
        case MC_ENTITY_NBT_CLEANUP_FAILED:
            return "Player wake cleanup failed";
    }
    return "unknown Entity NBT status";
}
