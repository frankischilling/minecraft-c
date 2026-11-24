#include "mc/entity/factory/factory.h"

#include "../mob/internal.h"
#include "mc/entity/minecart/minecart.h"
#include "mc/entity/primed_tnt/construction.h"

static mc_entity_factory_status finish(mc_entity_factory_error *out,
                                       const mc_entity_factory_error *frame) {
    if (out != NULL)
        *out = *frame;
    return frame->status;
}

static void fail(mc_entity_factory_error *e, mc_entity_factory_status status,
                 const char *operation) {
    e->status = status;
    e->operation = operation;
    e->native_policy = true;
}

static mc_entity_factory_status entity_status(mc_entity_status status) {
    return status == MC_ENTITY_LIMIT               ? MC_ENTITY_FACTORY_LIMIT
           : status == MC_ENTITY_ALLOCATION_FAILED ? MC_ENTITY_FACTORY_ALLOCATION_FAILED
           : status == MC_ENTITY_UNSUPPORTED_FP    ? MC_ENTITY_FACTORY_UNSUPPORTED_FP
                                                   : MC_ENTITY_FACTORY_CONSTRUCTOR_FAILED;
}

static void print_frontier(mc_entity_factory_error *e, const char *caught) {
    e->bytecode = 52;
    e->caught_exception_class = caught;
    fail(e, MC_ENTITY_FACTORY_UNIMPLEMENTED, "Named Entity factory Exception.printStackTrace");
}

static bool mob(mc_entity_classes *classes, mc_registry_classes *registry, mc_world_runtime *world,
                const mc_entity_class_literal *type, mc_entity **entity,
                mc_entity_factory_error *e) {
    e->mob_selected = true;
    mc_mob_status status = mc_mob_allocate(classes, registry, type, entity, &e->mob_call);
    if (status != MC_MOB_OK) {
        fail(e,
             status == MC_MOB_LIMIT            ? MC_ENTITY_FACTORY_LIMIT
             : status == MC_MOB_ALLOCATION     ? MC_ENTITY_FACTORY_ALLOCATION_FAILED
             : status == MC_MOB_UNSUPPORTED_FP ? MC_ENTITY_FACTORY_UNSUPPORTED_FP
                                               : MC_ENTITY_FACTORY_CONSTRUCTOR_FAILED,
             "Named Entity factory concrete Mob active use/allocation");
        /* Target initialization precedes constructor invocation. Actual linkage
         * Errors remain outside the factory's Exception handler. */
        e->native_policy = e->mob_call.native_policy;
        e->source_exception = e->mob_call.source_exception;
        e->exception_class = e->mob_call.exception_class;
        e->construction = e->mob_call.entity;
        return false;
    }
    status = mc_mob_construct(*entity, world, &e->mob_call);
    if (status == MC_MOB_OK)
        return true;
    e->construction = e->mob_call.entity;
    if (e->mob_call.source_exception) {
        print_frontier(e, "java/lang/reflect/InvocationTargetException");
        return false;
    }
    fail(e,
         status == MC_MOB_LIMIT            ? MC_ENTITY_FACTORY_LIMIT
         : status == MC_MOB_ALLOCATION     ? MC_ENTITY_FACTORY_ALLOCATION_FAILED
         : status == MC_MOB_UNSUPPORTED_FP ? MC_ENTITY_FACTORY_UNSUPPORTED_FP
                                           : MC_ENTITY_FACTORY_CONSTRUCTOR_FAILED,
         "Named Entity factory concrete Mob constructor");
    return false;
}

mc_entity_factory_status mc_entity_factory_named(mc_entity_classes *classes,
                                                 mc_registry_classes *registry, mc_nbt_string name,
                                                 mc_world_runtime *world, mc_entity **out,
                                                 mc_entity_factory_error *error) {
    mc_entity_factory_error e = {0};
    if (classes == NULL || out == NULL || (name.length != 0 && name.units == NULL)) {
        fail(&e, MC_ENTITY_FACTORY_ARGUMENT, "Named Entity factory native arguments");
        return finish(error, &e);
    }
    if (mc_entity_classes_initialize_registry(classes, &e.registry) != MC_ENTITY_REGISTRY_OK) {
        fail(&e,
             e.registry.status == MC_ENTITY_REGISTRY_LIMIT ? MC_ENTITY_FACTORY_LIMIT
             : e.registry.status == MC_ENTITY_REGISTRY_ALLOCATION_FAILED
                 ? MC_ENTITY_FACTORY_ALLOCATION_FAILED
                 : MC_ENTITY_FACTORY_REGISTRY_FAILED,
             "Named Entity factory EntityList active use");
        return finish(error, &e);
    }
    e.bytecode = 6;
    const mc_entity_class_literal *type = NULL;
    if (mc_entity_registry_named_class(classes, name, &type, &e.registry) !=
        MC_ENTITY_REGISTRY_OK) {
        fail(&e, MC_ENTITY_FACTORY_REGISTRY_FAILED, "Named Entity factory name Map lookup");
        return finish(error, &e);
    }
    if (type == NULL) {
        e.bytecode = 56;
        *out = NULL;
        return finish(error, &e);
    }
    e.selected_class = type;
    e.bytecode = 40;
    mc_entity_class_literal_view view;
    if (mc_entity_class_literal_inspect(type, &view) != MC_ENTITY_OK || view.owner != classes) {
        fail(&e, MC_ENTITY_FACTORY_ARGUMENT, "Named Entity factory canonical Class");
        return finish(error, &e);
    }
    if (view.kind == MC_ENTITY_CLASS_KIND_LIVING_ENTITY) {
        print_frontier(&e, "java/lang/InstantiationException");
        return finish(error, &e);
    }
    mc_entity *entity = NULL;
    mc_entity_status status = MC_ENTITY_OK;
    bool completed = false;
    if (view.source == MC_ENTITY_CLIENT && mc_entity_is_mob_kind(view.kind)) {
        completed = mob(classes, registry, world, type, &entity, &e);
    } else if (view.source == MC_ENTITY_CLIENT && view.kind == MC_ENTITY_CLASS_KIND_ARROW) {
        e.arrow_selected = true;
        status = mc_arrow_allocate(classes, &entity, &e.construction);
        if (status == MC_ENTITY_OK) {
            status = mc_arrow_construct_world(entity, world, &e.arrow_call);
            if (status != MC_ENTITY_OK)
                e.construction = e.arrow_call.entity;
        }
        if (status != MC_ENTITY_OK && e.arrow_call.source_exception)
            print_frontier(&e, "java/lang/reflect/InvocationTargetException");
        else if (status != MC_ENTITY_OK)
            fail(&e, entity_status(status), "Named Entity factory Arrow constructor");
        completed = status == MC_ENTITY_OK;
    } else if (view.kind == MC_ENTITY_CLASS_KIND_ITEM) {
        status = mc_item_entity_allocate(classes, &entity, &e.construction);
        if (status == MC_ENTITY_OK)
            status = mc_item_entity_construct_world(entity, world, &e.construction);
        completed = status == MC_ENTITY_OK;
    } else if (view.kind == MC_ENTITY_CLASS_KIND_FALLING_SAND) {
        status = mc_falling_entity_allocate(classes, &entity, &e.construction);
        if (status == MC_ENTITY_OK)
            status = mc_falling_entity_construct_world(entity, world, &e.construction);
        completed = status == MC_ENTITY_OK;
    } else if (view.kind == MC_ENTITY_CLASS_KIND_MINECART) {
        status = mc_minecart_entity_allocate(classes, &entity, &e.construction);
        if (status == MC_ENTITY_OK)
            status = mc_minecart_entity_construct_world(entity, world, &e.construction);
        completed = status == MC_ENTITY_OK;
    } else if (view.source == MC_ENTITY_CLIENT && view.kind == MC_ENTITY_CLASS_KIND_PRIMED_TNT) {
        status = mc_primed_tnt_entity_allocate(classes, &entity, &e.construction);
        if (status == MC_ENTITY_OK)
            status = mc_primed_tnt_entity_construct_world(entity, world, &e.construction);
        completed = status == MC_ENTITY_OK;
    } else {
        fail(&e, MC_ENTITY_FACTORY_UNIMPLEMENTED, "Named Entity factory concrete constructor");
    }
    if (!completed) {
        if (e.status == MC_ENTITY_FACTORY_OK)
            fail(&e, entity_status(status), "Named Entity factory constructor");
        mc_entity_release(entity);
        return finish(error, &e);
    }
    *out = entity;
    e.bytecode = 56;
    return finish(error, &e);
}
