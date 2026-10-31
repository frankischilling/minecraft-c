#include "client/application_state.h"
#include "client/render/manager/draw_internal.h"
#include "client/world/entity_renderer/internal.h"
#include "entity/player/player_internal.h"
#include "held_stack_parent_internal.h"
#include "item/stack/source_internal.h"

#include <string.h>

static const uint16_t items_units[] = {'/', 'g', 'u', 'i', '/', 'i', 't',
                                       'e', 'm', 's', '.', 'p', 'n', 'g'};
static const uint16_t terrain_units[] = {'/', 't', 'e', 'r', 'r', 'a',
                                         'i', 'n', '.', 'p', 'n', 'g'};
/* Same genuine immortal native String representation used by living literals.
 * No global String-intern identity equivalence is claimed here. */
static const mc_player_string items = {SIZE_MAX, sizeof(items_units) / sizeof(*items_units),
                                       items_units};
static const mc_player_string terrain = {SIZE_MAX, sizeof(terrain_units) / sizeof(*terrain_units),
                                         terrain_units};

static mc_client_graphics_site source_site(int32_t pc) {
    return (mc_client_graphics_site){"ra", "a", "(Lls;Liz;)V", pc};
}

static void at(mc_held_stack_parent_error *e, int32_t pc) {
    e->site = source_site(pc);
    e->state.source_pc = (unsigned int)pc;
    e->selected = MC_HELD_STACK_PARENT_CHILD_NONE;
    e->child = (mc_client_graphics_site){.pc = -1};
}

static bool fail(mc_held_stack_parent_error *e, mc_held_stack_parent_status status,
                 const char *operation) {
    e->status = status;
    e->operation = operation;
    e->native_policy = e->throwable == MC_HELD_STACK_PARENT_NO_THROWABLE;
    return false;
}

static bool source_null(mc_held_stack_parent_error *e, const char *operation) {
    e->throwable = MC_HELD_STACK_PARENT_EXCEPTION;
    e->source_exception = true;
    e->exception_class = "java/lang/NullPointerException";
    return fail(e, MC_HELD_STACK_PARENT_SOURCE_EXCEPTION, operation);
}

static bool capture_failed(mc_held_stack_parent_error *e) {
    e->before_capture_failure =
        (mc_held_stack_parent_failure_summary){.present = true,
                                               .selected = e->selected,
                                               .throwable = e->throwable,
                                               .operation = e->operation,
                                               .exception_class = e->exception_class,
                                               .site = e->site,
                                               .child = e->child,
                                               .native_policy = e->native_policy,
                                               .source_exception = e->source_exception};
    e->capture_failed = true;
    e->throwable = MC_HELD_STACK_PARENT_NO_THROWABLE;
    e->source_exception = false;
    e->exception_class = NULL;
    return fail(e, MC_HELD_STACK_PARENT_CAPTURE_FAILED, e->capture_failure.operation);
}

static bool stack_id(mc_held_stack_parent_error *e, int32_t pc, int32_t *out, bool *read) {
    at(e, pc);
    if (e->state.stack == NULL)
        return source_null(e, "RA actual nullable IZ.c field receiver");
    e->stack = mc_item_stack_source_id_get(e->state.stack, out);
    if (e->stack != MC_ITEM_STACK_OK) {
        e->selected = MC_HELD_STACK_PARENT_CHILD_STACK;
        return fail(e, MC_HELD_STACK_PARENT_STACK_FAILED, "RA actual IZ id source field");
    }
    *read = true;
    return true;
}

static mc_registry_classes *registry(mc_client_draw_scope *scope, mc_held_stack_parent_error *e) {
    /* Native association of the actual retained TH, not a source Item-array,
     * actor.World, App registry, replacement Registry or class active use. */
    mc_client_render_manager *manager = scope->owner->manager_hold;
    mc_registry_classes *classes = manager->classes->registry;
    if (classes == NULL) {
        fail(e, MC_HELD_STACK_PARENT_STATE, "RA actual retained TH Registry association");
        return NULL;
    }
    e->state.registry = classes;
    return classes;
}

static bool registry_failed(mc_held_stack_parent_error *e) {
    e->selected = MC_HELD_STACK_PARENT_CHILD_REGISTRY;
    mc_item_render_throwable kind;
    mc_item_render_registry_classification(e->state.registry, &e->registry, &kind,
                                           &e->exception_class);
    if (kind == MC_ITEM_RENDER_SOURCE_EXCEPTION)
        e->throwable = MC_HELD_STACK_PARENT_EXCEPTION;
    else if (kind == MC_ITEM_RENDER_SOURCE_ERROR)
        e->throwable = MC_HELD_STACK_PARENT_ERROR;
    e->source_exception = kind == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    if (e->registry.active_initializer_class != NULL)
        e->child = (mc_client_graphics_site){e->registry.active_initializer_class, "<clinit>",
                                             "()V", e->registry.active_initializer_pc};
    return fail(e, MC_HELD_STACK_PARENT_REGISTRY_FAILED, e->registry.message);
}

static void graphics_classify(mc_held_stack_parent_error *e, const mc_client_graphics_error *v) {
    e->child = v->site;
    if (v->native_policy)
        return;
    /* A cached native initializer failure is not promoted to a Java Error.
     * Unknown class/print/VBO causes remain unclassified and bypass GV catch.
     * Complete immutable producer values remain in the selected graphics cause. */
    if (v->status == MC_CLIENT_GRAPHICS_CLASS_FAILED &&
        (v->class_cause.native_policy || v->dependency_class_cause.native_policy))
        return;
    const char *name = NULL;
    switch (v->status) {
        case MC_CLIENT_GRAPHICS_NULL_REFERENCE:
            name = "java/lang/NullPointerException";
            break;
        case MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT:
            name = "java/lang/IllegalArgumentException";
            break;
        case MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE:
            name = "java/lang/IllegalStateException";
            break;
        case MC_CLIENT_GRAPHICS_ARRAY_INDEX:
            name = "java/lang/ArrayIndexOutOfBoundsException";
            break;
        case MC_CLIENT_GRAPHICS_BUFFER_OVERFLOW:
            name = "java/nio/BufferOverflowException";
            break;
        case MC_CLIENT_GRAPHICS_BUFFER_UNDERFLOW:
            name = "java/nio/BufferUnderflowException";
            break;
        default:
            e->throwable = MC_HELD_STACK_PARENT_UNCLASSIFIED_THROWABLE;
            return;
    }
    e->throwable = MC_HELD_STACK_PARENT_EXCEPTION;
    e->source_exception = true;
    e->exception_class = name;
}

static void lookup_classify(mc_held_stack_parent_error *e) {
    const mc_client_texture_lookup_error *v = &e->lookup;
    e->child = v->site;
    if (v->native_policy)
        return;
    const char *name = NULL;
    switch (v->status) {
        case MC_CLIENT_LOOKUP_NULL_REFERENCE:
            name = "java/lang/NullPointerException";
            break;
        case MC_CLIENT_LOOKUP_ARRAY_FAILED:
            name = "java/lang/ArrayIndexOutOfBoundsException";
            break;
        case MC_CLIENT_LOOKUP_SOURCE_NEGATIVE_SIZE:
            name = "java/lang/NegativeArraySizeException";
            break;
        case MC_CLIENT_LOOKUP_SOURCE_ILLEGAL_ARGUMENT:
            name = "java/lang/IllegalArgumentException";
            break;
        default:
            e->throwable = MC_HELD_STACK_PARENT_UNCLASSIFIED_THROWABLE;
            return;
    }
    e->throwable = MC_HELD_STACK_PARENT_EXCEPTION;
    e->source_exception = true;
    e->exception_class = name;
}

static bool icon(mc_client_draw_scope *scope, mc_held_stack_parent_error *e) {
    at(e, 130);
    e->state.icon_attempted = true;
    if (e->state.actor == NULL)
        return source_null(e, "RA nullable LS.c(IZ) invoke receiver");
    mc_registry_classes *classes = registry(scope, e);
    if (classes == NULL)
        return false;
    e->selected = MC_HELD_STACK_PARENT_CHILD_ENTITY;
    e->state.icon_entered = true;
    const bool returned = mc_entity_render_item_icon(e->state.actor, classes, e->state.stack,
                                                     &e->state.icon, &e->entity);
    e->state.icon_returned = returned;
    if (!returned) {
        e->child = (mc_client_graphics_site){e->entity.site.class_name, e->entity.site.method,
                                             e->entity.site.descriptor, e->entity.site.pc};
        e->exception_class = e->entity.exception_class;
        e->source_exception = e->entity.source_exception;
        e->throwable = e->entity.throwable == MC_ENTITY_RENDER_SOURCE_EXCEPTION
                           ? MC_HELD_STACK_PARENT_EXCEPTION
                       : e->entity.throwable == MC_ENTITY_RENDER_SOURCE_ERROR
                           ? MC_HELD_STACK_PARENT_ERROR
                       : e->entity.throwable == MC_ENTITY_RENDER_UNCLASSIFIED_THROWABLE
                           ? MC_HELD_STACK_PARENT_UNCLASSIFIED_THROWABLE
                           : MC_HELD_STACK_PARENT_NO_THROWABLE;
        fail(e, MC_HELD_STACK_PARENT_ENTITY_FAILED, e->entity.operation);
    }
    if (!mc_client_draw_entity_queries(scope, &e->entity, &e->capture_failure))
        return capture_failed(e);
    e->state.entity_queries_published = true;
    for (size_t i = 0; i < sizeof(e->entity.world_reads) / sizeof(*e->entity.world_reads); ++i)
        if (e->entity.world_reads[i] != NULL &&
            !mc_client_draw_adopt_world(scope, &e->entity.world_reads[i], &e->capture_failure))
            return capture_failed(e);
    e->state.entity_worlds_adopted = true;
    return returned;
}

static bool atlas(mc_client_draw_scope *scope, mc_held_stack_parent_error *e) {
    const bool terrain_branch = e->state.terrain_atlas;
    const int32_t app_pc = terrain_branch ? 89 : 110;
    const int32_t engine_pc = terrain_branch ? 92 : 113;
    const int32_t key_pc = terrain_branch ? 95 : 116;
    const int32_t lookup_pc = terrain_branch ? 97 : 118;
    const int32_t bind_pc = terrain_branch ? 100 : 121;
    at(e, app_pc);
    e->state.application = e->state.ra->a; /* Fresh actual RA.a. */
    e->state.application_read = true;
    if (!mc_client_draw_capture_application(scope, e->state.application, &e->capture_failure))
        return capture_failed(e);
    at(e, engine_pc);
    if (e->state.application == NULL)
        return source_null(e, "RA nullable App.p field receiver");
    e->state.engine = e->state.application->render_engine; /* Fresh captured App.p. */
    e->state.engine_read = true;
    if (!mc_client_draw_capture_engine(scope, e->state.engine, &e->capture_failure))
        return capture_failed(e);
    at(e, key_pc);
    e->state.key = terrain_branch ? &terrain : &items;
    e->state.key_loaded = true;
    if (!mc_client_draw_capture_string(scope, e->state.key, &e->capture_failure))
        return capture_failed(e);
    at(e, lookup_pc);
    e->state.lookup_attempted = true;
    if (e->state.engine == NULL)
        return source_null(e, "RA nullable JI.b(String) invoke receiver");
    e->state.pending_on_entry = mc_client_texture_lookup_pending(e->state.engine);
    mc_player_string_view text;
    if (!mc_player_string_read(e->state.key, &text))
        return fail(e, MC_HELD_STACK_PARENT_STATE, "RA immutable literal native representation");
    const mc_texture_pack_text key = {text.units, text.length, false};
    e->selected = MC_HELD_STACK_PARENT_CHILD_LOOKUP;
    e->state.lookup_entered = true;
    const mc_client_texture_lookup_status status = mc_client_render_engine_texture(
        e->state.engine, scope->model.display, NULL, key, &e->state.texture, &e->lookup);
    e->state.lookup_returned = status == MC_CLIENT_LOOKUP_OK;
    e->state.pending_after_call = mc_client_texture_lookup_pending(e->state.engine);
    if (status != MC_CLIENT_LOOKUP_OK) {
        lookup_classify(e);
        fail(e, MC_HELD_STACK_PARENT_LOOKUP_FAILED, e->lookup.operation);
    }
    /* Never claim or retire an attempt that was already pending on entry. */
    if (!e->state.pending_on_entry && e->state.pending_after_call) {
        if (!mc_client_draw_pending_lookup(scope, e->state.engine, false, &e->capture_failure))
            return capture_failed(e);
        e->state.pending_adopted = true;
    }
    if (status != MC_CLIENT_LOOKUP_OK)
        return false;
    at(e, bind_pc);
    e->selected = MC_HELD_STACK_PARENT_CHILD_DISPLAY;
    e->state.bind_attempted = true;
    /* RA calls GL directly: no negative-ID early return from JI.b(I). */
    if (!mc_client_display_source_bind_id(scope->model.display, e->state.texture, &e->display))
        return fail(e, MC_HELD_STACK_PARENT_DISPLAY_FAILED, e->display.operation);
    e->state.bind_returned = true;
    return true;
}

bool entity_renderer_held_stack_entered(mc_client_draw_scope *scope, mc_client_held_item *hand,
                                        mc_entity *actor, mc_item_stack *stack,
                                        mc_client_graphics_site caller,
                                        mc_held_stack_parent_error *e) {
    if (e == NULL)
        return false;
    /* Caller guarantees fresh/discharged storage: this must not erase rights. */
    memset(e, 0, sizeof(*e));
    e->caller = caller;
    e->site.pc = e->child.pc = -1;
    e->lookup.site.pc = e->lookup.dependency.pc = -1;
    for (size_t i = 0; i < sizeof(e->lookup.frames) / sizeof(*e->lookup.frames); ++i)
        e->lookup.frames[i].pc = -1;
    e->registry.initializer_pc = e->registry.constructor_pc = e->registry.active_initializer_pc =
        -1;
    e->graphics.site.pc = e->graphics.caller.pc = e->graphics.entry.pc = e->graphics.parent.pc = -1;
    e->geometry.site.pc = -1;
    e->geometry.state.strip = -1;
    mc_entity_render_error_init(&e->entity);
    e->state.hand = hand;
    e->state.actor = actor;
    e->state.stack = stack;
    e->native_policy = true;
    if (scope == NULL || scope->owner == NULL)
        return fail(e, MC_HELD_STACK_PARENT_ARGUMENT, "RA actual charged Parent scope");
    e->selected = MC_HELD_STACK_PARENT_CHILD_GUARD;
    if (!mc_client_draw_held_guard_begin(scope, hand, e, &e->guard))
        return fail(e, MC_HELD_STACK_PARENT_GUARD_FAILED, e->guard.operation);
    e->state.guard_acquired = true;
    /* The guard and caller prove source/manager/native services
     * before these dereferences; no replacement RA/TH/App is constructed here. */
    e->state.ra = mc_client_held_item_identity(hand);
    e->selected = MC_HELD_STACK_PARENT_CHILD_NONE;
    if (!mc_client_draw_capture_stack(scope, stack, &e->capture_failure))
        return capture_failed(e);
    at(e, 0);
    e->state.source_entered = true;
    e->selected = MC_HELD_STACK_PARENT_CHILD_DISPLAY;
    if (!mc_client_display_source_push(scope->model.display, &e->display))
        return fail(e, MC_HELD_STACK_PARENT_DISPLAY_FAILED, e->display.operation);
    e->state.push_returned = true;
    if (!stack_id(e, 4, &e->state.id4, &e->state.id4_read))
        return false;
    if (e->state.id4 < 256) {
        at(e, 13);
        mc_registry_classes *classes = registry(scope, e);
        if (classes == NULL)
            return false;
        if (mc_registry_block_m_get(classes, &e->state.blocks, &e->registry) != MC_REGISTRY_OK)
            return registry_failed(e);
        e->state.block_array_read = true;
        if (!stack_id(e, 17, &e->state.id17, &e->state.id17_read))
            return false;
        at(e, 20);
        if (mc_registry_block_m_at(&e->state.blocks, e->state.id17, &e->state.block,
                                   &e->registry) != MC_REGISTRY_OK)
            return registry_failed(e);
        e->state.block_read = true;
        at(e, 21);
        if (e->state.block == NULL)
            return source_null(e, "RA null captured UU.b() invoke receiver");
        /* Held Bow path: no fabricated Block shape or CV result. */
        return fail(e, MC_HELD_STACK_PARENT_UNIMPLEMENTED,
                    "RA reached actual non-Bow UU.b()/CV.a branch");
    }
    if (!stack_id(e, 76, &e->state.id76, &e->state.id76_read))
        return false;
    e->state.terrain_atlas = e->state.id76 < 256;
    if (!atlas(scope, e))
        return false;
    at(e, 124);
    if (!mc_client_draw_capture_graphics(scope, &e->capture_failure))
        return capture_failed(e);
    e->selected = MC_HELD_STACK_PARENT_CHILD_GRAPHICS;
    e->state.nw_attempted = true;
    if (!mc_client_nw_active_use(scope->model.graphics, e->site, &e->state.nw, &e->graphics)) {
        graphics_classify(e, &e->graphics);
        return fail(e, MC_HELD_STACK_PARENT_GRAPHICS_FAILED, e->graphics.operation);
    }
    e->state.nw_returned = true;
    if (!icon(scope, e))
        return false;
    at(e, 133); /* Actual returned icon has been stored; use this same local. */
    e->selected = MC_HELD_STACK_PARENT_CHILD_GEOMETRY;
    e->state.geometry_attempted = true;
    if (!entity_renderer_held_bow_draw(scope, e->state.icon, e->state.nw, scope->model.graphics,
                                       scope->model.display, &e->geometry)) {
        e->site = e->geometry.site;
        e->state.source_pc = e->geometry.state.source_pc;
        if (e->geometry.status == MC_HELD_BOW_CAPTURE_FAILED) {
            e->capture_failure = e->geometry.capture_failure;
            return capture_failed(e);
        }
        if (e->geometry.source_null_invocation)
            return source_null(e, e->geometry.operation);
        if (e->geometry.status == MC_HELD_BOW_GRAPHICS)
            graphics_classify(e, &e->geometry.graphics);
        return fail(e, MC_HELD_STACK_PARENT_GEOMETRY_FAILED, e->geometry.operation);
    }
    e->state.geometry_returned = true;
    at(e, 970);
    e->state.source_returned = true;
    e->selected = MC_HELD_STACK_PARENT_CHILD_NONE;
    e->native_policy = false;
    /* Parent still owns the guard and every native slot. No discharge here. */
    return true;
}
