#include "block/registry/runtime_internal.h"
#include "client/world/entity_renderer/held_stack_parent_internal.h"
#include "internal.h"
#include "item/stack/source_internal.h"

static void v_at(ld_frame *f, int32_t pc) {
    ld_at(f, "v", "b", "(Lls;F)V", pc);
}

/* Capture errors stay separate from the earlier complete tagged child and
 * cannot be routed through GV's Java Exception catch. */
static bool capture_failed(ld_frame *f) {
    f->error->throwable = MC_CLIENT_LIVING_DRAW_NO_THROWABLE;
    f->error->source_exception = false;
    f->error->exception_class = NULL;
    return ld_fail(f,
                   f->error->capture_failure.status == MC_CLIENT_MANAGER_LIMIT
                       ? MC_CLIENT_LIVING_DRAW_LIMIT
                       : MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                   f->error->capture_failure.operation);
}

static bool part_failed(ld_frame *f) {
    mc_client_living_draw_error *e = f->error;
    mc_client_model_draw_error *part = &e->detail.part;
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_PART;
    e->child = part->site;
    if (part->pending_list && !mc_client_draw_pending_list(f->scope, part, &e->capture_failure))
        return capture_failed(f);
    if (part->throwable == MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION && part->exception_class != NULL) {
        e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
        e->source_exception = true;
        e->exception_class = part->exception_class;
    } else if (part->throwable == MC_CLIENT_MODEL_DRAW_SOURCE_ERROR &&
               part->exception_class != NULL) {
        e->throwable = MC_CLIENT_LIVING_DRAW_ERROR;
        e->exception_class = part->exception_class;
    } else if (part->throwable != MC_CLIENT_MODEL_DRAW_NO_THROWABLE) {
        e->throwable = MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE;
    }
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, part->operation);
}

static bool registry_failed(ld_frame *f, mc_registry_classes *classes) {
    mc_client_living_draw_error *e = f->error;
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_REGISTRY;
    mc_item_render_throwable kind;
    mc_item_render_registry_classification(classes, &e->detail.registry, &kind,
                                           &e->exception_class);
    if (kind == MC_ITEM_RENDER_SOURCE_EXCEPTION) {
        e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
        e->source_exception = true;
    } else if (kind == MC_ITEM_RENDER_SOURCE_ERROR) {
        e->throwable = MC_CLIENT_LIVING_DRAW_ERROR;
    }
    if (e->detail.registry.active_initializer_class != NULL)
        e->child = ld_site(e->detail.registry.active_initializer_class, "<clinit>", "()V",
                           e->detail.registry.active_initializer_pc);
    return ld_fail(f,
                   e->detail.registry.status == MC_REGISTRY_LIMIT
                       ? MC_CLIENT_LIVING_DRAW_LIMIT
                       : MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                   e->detail.registry.message);
}

static bool item_failed(ld_frame *f) {
    mc_client_living_draw_error *e = f->error;
    const mc_item_render_error *item = &e->detail.item;
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_ITEM;
    e->child =
        ld_site(item->site.class_name, item->site.method, item->site.descriptor, item->site.pc);
    if (item->throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION) {
        e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
        e->source_exception = true;
    } else if (item->throwable == MC_ITEM_RENDER_SOURCE_ERROR) {
        e->throwable = MC_CLIENT_LIVING_DRAW_ERROR;
    }
    e->exception_class = item->exception_class;
    return ld_fail(f,
                   item->status == MC_REGISTRY_LIMIT ? MC_CLIENT_LIVING_DRAW_LIMIT
                                                     : MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                   item->operation);
}

static bool held_failed(ld_frame *f, mc_held_stack_parent_error *held) {
    mc_client_living_draw_error *e = f->error;
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_HELD;
    e->detail.held = held;
    e->child = held->site;
    if (held->capture_failed) {
        /* The full held child, including before_capture_failure and owning
         * slots, stays in Root's charged workspace at its stable address. */
        e->capture_failure = held->capture_failure;
        return capture_failed(f);
    } else if (held->throwable == MC_HELD_STACK_PARENT_EXCEPTION && held->exception_class != NULL) {
        e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
        e->source_exception = true;
        e->exception_class = held->exception_class;
    } else if (held->throwable == MC_HELD_STACK_PARENT_ERROR && held->exception_class != NULL) {
        e->throwable = MC_CLIENT_LIVING_DRAW_ERROR;
        e->exception_class = held->exception_class;
    } else if (held->throwable != MC_HELD_STACK_PARENT_NO_THROWABLE) {
        e->throwable = MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE;
    }
    return ld_fail(f,
                   held->status == MC_HELD_STACK_PARENT_UNIMPLEMENTED
                       ? MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED
                       : MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                   held->operation);
}

static bool stack_id(ld_frame *f, mc_item_stack *stack, int32_t *out) {
    f->error->detail.stack = mc_item_stack_source_id_get(stack, out);
    if (f->error->detail.stack == MC_ITEM_STACK_OK)
        return true;
    f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_STACK;
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Living held actual IZ source id field");
}

static mc_registry_classes *registry(ld_frame *f) {
    /* The defining class namespace is a retained native association, not a
     * Source World, App or current BW.manager field read. */
    mc_registry_classes *classes = f->renderer->parent->classes->registry;
    if (classes == NULL)
        ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Living held actual Registry association");
    return classes;
}

bool ld_v_held(ld_frame *f, float partial) {
    (void)partial;
    mc_client_living_draw_error *e = f->error;
    mc_client_living_draw_held_state *s = &e->state.held;
    const mc_client_graphics_site parent = e->parent;
    e->parent = ld_site("gv", "a", "(Lls;DDDFF)V", 365);
    s->entered = true;
    v_at(f, 1);
    if (f->actor == NULL)
        return ld_null(f, "Living held null LS getter invoke receiver");
    mc_item_stack *stack;
    if (!ld_entity(f, mc_entity_render_held_stack(f->actor, &stack, &e->detail.entity)))
        return false;
    s->stack_returned = true;
    s->stack_identity = (uintptr_t)stack;
    if (!mc_client_draw_capture_stack(f->scope, stack, &e->capture_failure)) {
        e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_ENTITY;
        e->child = ld_site(e->detail.entity.site.class_name, e->detail.entity.site.method,
                           e->detail.entity.site.descriptor, e->detail.entity.site.pc);
        return capture_failed(f);
    }
    if (stack == NULL) {
        v_at(f, 223);
        s->returned = true;
        e->parent = parent;
        return true;
    }
    v_at(f, 9);
    if (!ld_display(f, mc_client_display_source_push(f->scope->model.display, &e->detail.display)))
        return false;
    manager_model *own = f->renderer->own_model; /* Actual fresh V.a13. */
    s->own_model_identity = (uintptr_t)own;
    v_at(f, 16);
    if (own == NULL)
        return ld_null(f, "Living held null actual V.a FH.d field receiver");
    bool owned = false;
    for (manager_model *p = f->renderer->models; p != NULL; p = p->next)
        if (p == own) {
            owned = true;
            break;
        }
    if (!owned)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Living held actual V.a model provenance");
    mc_client_model_part *arm = own->fields.d;
    s->arm_identity = (uintptr_t)arm;
    v_at(f, 21);
    if (arm == NULL)
        return ld_null(f, "Living held null captured PS attachment invoke receiver");
    s->attachment_attempted = true;
    if (!mc_client_model_part_attach(arm, f->scope->model.graphics, f->scope->model.entities,
                                     f->scope->model.display, .0625f, &e->detail.part))
        return part_failed(f);
    s->attachment_returned = true;
    v_at(f, 30);
    if (!ld_display(f, mc_client_display_source_translate(f->scope->model.display, -.0625f, .4375f,
                                                          .0625f, &e->detail.display)))
        return false;
    v_at(f, 34);
    int32_t first_id;
    if (!stack_id(f, stack, &first_id))
        return false;
    s->first_id = first_id;
    s->first_id_read = true;
    if (first_id < 256) {
        v_at(f, 43);
        mc_registry_classes *classes = registry(f);
        if (classes == NULL)
            return false;
        mc_registry_block_m_view blocks;
        if (mc_registry_block_m_get(classes, &blocks, &e->detail.registry) != MC_REGISTRY_OK)
            return registry_failed(f, classes);
        v_at(f, 47);
        int32_t id;
        if (!stack_id(f, stack, &id))
            return false;
        s->id = id;
        s->id_read = true;
        v_at(f, 50);
        const mc_block *block;
        if (mc_registry_block_m_at(&blocks, id, &block, &e->detail.registry) != MC_REGISTRY_OK)
            return registry_failed(f, classes);
        v_at(f, 51);
        if (block == NULL)
            return ld_null(f, "Living held null captured UU render-type invoke receiver");
        e->child = ld_site("uu", "b", "()I", 0);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Living held actual UU.b/CV Block branch");
    }
    v_at(f, 108);
    mc_registry_classes *classes = registry(f);
    if (classes == NULL)
        return false;
    const mc_registry_item_array *items;
    if (mc_registry_item_c_get(classes, &items, &e->detail.registry) != MC_REGISTRY_OK)
        return registry_failed(f, classes);
    v_at(f, 112);
    int32_t id;
    if (!stack_id(f, stack, &id))
        return false;
    s->id = id;
    s->id_read = true;
    v_at(f, 115);
    const mc_item *item;
    if (mc_registry_item_c_at(classes, items, id, &item, &e->detail.registry) != MC_REGISTRY_OK)
        return registry_failed(f, classes);
    s->item_identity = (uintptr_t)item;
    v_at(f, 116);
    if (item == NULL)
        return ld_null(f, "Living held null captured GM full3D invoke receiver");
    s->full3d_attempted = true;
    bool full3d;
    if (!mc_item_render_full_3d(classes, item, &full3d, &e->detail.item))
        return item_failed(f);
    s->full3d_returned = true;
    s->full3d = full3d;
    if (full3d) {
        v_at(f, 130);
        if (!ld_display(f, mc_client_display_source_translate(f->scope->model.display, 0.0f, .1875f,
                                                              0.0f, &e->detail.display)))
            return false;
        v_at(f, 140);
        if (!ld_display(f, mc_client_display_source_scale(f->scope->model.display, .625, -.625,
                                                          .625, &e->detail.display)))
            return false;
        v_at(f, 148);
        if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display, -100.0f, 1.0f,
                                                           0.0f, 0.0f, &e->detail.display)))
            return false;
        v_at(f, 156);
        if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display, 45.0f, 0.0f,
                                                           1.0f, 0.0f, &e->detail.display)))
            return false;
    } else {
        v_at(f, 172);
        if (!ld_display(f, mc_client_display_source_translate(f->scope->model.display, .25f, .1875f,
                                                              -.1875f, &e->detail.display)))
            return false;
        v_at(f, 181);
        if (!ld_display(f, mc_client_display_source_scale(f->scope->model.display, .375, .375, .375,
                                                          &e->detail.display)))
            return false;
        v_at(f, 189);
        if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display, 60.0f, 0.0f,
                                                           0.0f, 1.0f, &e->detail.display)))
            return false;
        v_at(f, 197);
        if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display, -90.0f, 1.0f,
                                                           0.0f, 0.0f, &e->detail.display)))
            return false;
        v_at(f, 205);
        if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display, 20.0f, 0.0f,
                                                           0.0f, 1.0f, &e->detail.display)))
            return false;
    }
    v_at(f, 212);
    mc_client_render_manager *manager = ld_manager(f, 212); /* Fresh actual BW.b209. */
    if (manager == NULL)
        return false;
    s->manager_identity = (uintptr_t)manager;
    const mc_client_source_ra *source = manager->fields.f;
    s->hand_identity = (uintptr_t)source;
    mc_client_held_item *hand;
    if (!mc_client_draw_capture_hand(f->scope, manager, source, &hand, &e->capture_failure))
        return capture_failed(f);
    s->hand_owner_identity = (uintptr_t)hand;
    v_at(f, 217);
    if (source == NULL)
        return ld_null(f, "Living held null captured RA invoke receiver");
    mc_held_stack_parent_error *held = mc_client_draw_held_workspace(f->scope, &e->capture_failure);
    if (held == NULL)
        return capture_failed(f);
    e->detail.held = held;
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_HELD;
    s->child_attempted = true;
    const bool returned =
        entity_renderer_held_stack_entered(f->scope, hand, f->actor, stack, e->site, held);
    s->child_source_entered = held->state.source_entered;
    s->child_returned = returned;
    if (!returned)
        return held_failed(f, held);
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_NONE;
    v_at(f, 220);
    if (!ld_display(f, mc_client_display_source_pop(f->scope->model.display, &e->detail.display)))
        return false;
    v_at(f, 223);
    s->returned = true;
    e->parent = parent;
    return true;
}
