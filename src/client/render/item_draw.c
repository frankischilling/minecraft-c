#include "living_draw/internal.h"

#include "block/registry/runtime_internal.h"
#include "blocks/internal.h"
#include "item/stack/source_internal.h"
#include "item_renderer_internal.h"
#include "mc/core/java/java.h"

static const char item_descriptor[] = "(Lhl;DDDFF)V";
static const uint16_t terrain_units[] = {'/', 't', 'e', 'r', 'r', 'a',
                                         'i', 'n', '.', 'p', 'n', 'g'};
static const mc_player_string terrain = {SIZE_MAX, sizeof terrain_units / sizeof *terrain_units,
                                         terrain_units};

static void item_at(ld_frame *f, int32_t pc) {
    ld_at(f, "bb", "a", item_descriptor, pc);
}

static bool block_at(ld_frame *f, mc_item_stack *stack, int32_t pc, const mc_block **out) {
    item_at(f, pc);
    mc_registry_classes *registry = f->renderer->parent->classes->registry;
    mc_registry_block_m_view array;
    if (mc_registry_block_m_get(registry, &array, &f->error->detail.registry) != MC_REGISTRY_OK)
        goto registry_failed;
    int32_t id;
    f->error->detail.stack = mc_item_stack_source_id_get(stack, &id);
    if (f->error->detail.stack != MC_ITEM_STACK_OK) {
        f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_STACK;
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, "Item actual Stack ID read");
    }
    if (mc_registry_block_m_at(&array, id, out, &f->error->detail.registry) == MC_REGISTRY_OK)
        return true;
registry_failed:
    f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_REGISTRY;
    ld_registry_classify(f, registry, &f->error->detail.registry);
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, "Item actual Block.m array access");
}

static void inventory_graphics_throwable(ld_frame *f, const mc_client_graphics_error *e) {
    if (e->native_policy)
        return;
    const char *name = NULL;
    switch (e->status) {
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
            f->error->throwable = MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE;
            return;
    }
    f->error->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
    f->error->source_exception = true;
    f->error->exception_class = name;
}

static bool item_inventory(ld_frame *f, mc_client_render_blocks *fields, const mc_block *block,
                           int32_t metadata, float brightness) {
    if (fields == NULL)
        return ld_null(f, "Item actual inventory CV receiver");
    mc_cv_context *c = mc_client_draw_inventory_workspace(f->scope, &f->error->capture_failure);
    if (c == NULL)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, f->error->capture_failure.operation);
    *c = (mc_cv_context){.fields = fields,
                         .registry = f->renderer->parent->classes->registry,
                         .models = f->renderer->parent->classes->models,
                         .graphics = f->scope->model.graphics,
                         .display = f->scope->model.display};
    f->error->detail.inventory = c;
    f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_INVENTORY_BLOCK;
    if (!mc_cv_inventory(c, block, metadata, brightness)) {
        f->error->child =
            (mc_client_graphics_site){c->failure.source_class, c->failure.source_method,
                                      c->failure.source_descriptor, (int32_t)c->failure.source_pc};
        if (!c->failure.native_policy &&
            c->failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE)
            return ld_null(f, c->failure.operation);
        if (!c->failure.native_policy &&
            c->failure.status == MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED)
            inventory_graphics_throwable(f, &c->failure.graphics);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, c->failure.operation);
    }
    f->error->state.item.faces_returned += c->inventory_faces;
    f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_NONE;
    return true;
}

bool mc_client_item_draw_entered(mc_client_draw_scope *scope, mc_client_managed_renderer *renderer,
                                 mc_entity *actor, double x, double y, double z, float yaw,
                                 float partial, mc_client_graphics_site caller,
                                 mc_client_living_draw_error *error) {
    (void)yaw;
    if (error == NULL)
        return false;
    mc_client_living_draw_error_init(error);
    error->caller = caller;
    ld_frame f = {scope, renderer, actor, error};
    error->state.item.entered = true;
    ld_at(&f, "bb", "a", "(Lsn;DDDFF)V", 2);
    if (!ld_cast_actor(&f, MC_ENTITY_CLASS_KIND_ITEM, "Item renderer actual HL checkcast"))
        return false;
    error->parent = ld_site("bb", "a", "(Lsn;DDDFF)V", 14);
    const mc_client_item_renderer_fields *bb = mc_client_item_renderer_fields_get(renderer->bb);
    item_at(&f, 7);
    if (bb == NULL || bb->f == NULL)
        return ld_null(&f, "Item actual Random.setSeed receiver");
    mc_java_random_reseed(bb->f, INT64_C(187));
    item_at(&f, 11);
    if (actor == NULL)
        return ld_null(&f, "Item actual HL Stack field receiver");
    mc_item_stack *stack = actor->stack;
    if (!ld_capture(&f, mc_client_draw_capture_stack(scope, stack, &error->detail.capture)))
        return false;
    error->state.item.stack_identity = (uintptr_t)stack;
    error->state.item.stack_captured = true;
    item_at(&f, 16);
    if (!ld_display(&f,
                    mc_client_display_source_push(scope->model.display, &error->detail.display)))
        return false;
    error->state.item.matrix_pushed = true;
    item_at(&f, 35);
    if (!ld_fp(&f))
        return false;
    const float first =
        at_fadd(at_fdiv(at_fadd((float)actor->item.b, partial), 10.0f), actor->item.d);
    float sine;
    if (!ld_float_math(&f, "a", first, &sine))
        return false;
    const float bob = at_fadd(at_fmul(sine, 0.1f), 0.1f);
    const float rotation = at_fmul(
        at_fadd(at_fdiv(at_fadd((float)actor->item.b, partial), 20.0f), actor->item.d), 57.295776f);
    error->state.item.bob = bob;
    error->state.item.rotation = rotation;
    int32_t copies = 1;
    static const int32_t count_pc[] = {74, 88, 102};
    static const int32_t thresholds[] = {1, 5, 20};
    for (size_t i = 0; i < 3; ++i) {
        item_at(&f, count_pc[i]);
        mc_item_stack *current = actor->stack; /* Fresh actual HL.a every comparison. */
        if (current == NULL)
            return ld_null(&f, "Item current HL Stack count receiver");
        int32_t count;
        error->detail.stack = mc_item_stack_source_count_get(current, &count);
        if (error->detail.stack != MC_ITEM_STACK_OK) {
            error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_STACK;
            return ld_fail(&f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, "Item actual Stack count read");
        }
        if (count > thresholds[i])
            copies = (int32_t)i + 2;
    }
    error->state.item.copies = copies;
    item_at(&f, 124);
    if (!ld_display(&f, mc_client_display_source_translate(scope->model.display, (float)x,
                                                           at_fadd((float)y, bob), (float)z,
                                                           &error->detail.display)))
        return false;
    item_at(&f, 129);
    if (!ld_display(&f, mc_client_display_source_rescale_normal(scope->model.display, true,
                                                                &error->detail.display)))
        return false;
    error->state.item.rescale_enabled = true;
    item_at(&f, 134);
    if (stack == NULL)
        return ld_null(&f, "Item captured Stack ID receiver");
    int32_t id;
    error->detail.stack = mc_item_stack_source_id_get(stack, &id);
    if (error->detail.stack != MC_ITEM_STACK_OK) {
        error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_STACK;
        return ld_fail(&f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, "Item captured Stack ID read");
    }
    if (id >= 256)
        return ld_fail(&f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED, "Item actual billboard branch");
    const mc_block *block;
    if (!block_at(&f, stack, 151, &block))
        return false;
    item_at(&f, 152);
    if (block == NULL)
        return ld_null(&f, "Item Block render-type null receiver");
    mc_block_render_description description;
    if (mc_block_render_describe(block, &description) != MC_REGISTRY_OK)
        return ld_fail(&f, MC_CLIENT_LIVING_DRAW_STATE, "Item actual Block render type");
    if (!mc_cv_inventory_type_3d(description.type))
        return ld_fail(&f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Item actual Block billboard branch");
    item_at(&f, 166);
    if (!ld_display(&f, mc_client_display_source_rotate(scope->model.display, rotation, 0, 1, 0,
                                                        &error->detail.display)))
        return false;
    item_at(&f, 172);
    if (!ld_texture(&f, &terrain, 172))
        return false;
    if (!block_at(&f, stack, 187, &block))
        return false;
    item_at(&f, 188);
    if (block == NULL)
        return ld_null(&f, "Item actual Block opacity receiver");
    float scale = 0.25f;
    if (!mc_block_render_opaque(block))
        return ld_fail(&f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Item nonopaque Block scale singleton/type branch");
    error->state.item.scale = scale;
    item_at(&f, 235);
    if (!ld_display(&f, mc_client_display_source_scale(scope->model.display, scale, scale, scale,
                                                       &error->detail.display)))
        return false;
    for (int32_t copy = 0; copy < copies; ++copy) {
        if (!ld_work(&f))
            return false;
        item_at(&f, 248);
        if (!ld_display(
                &f, mc_client_display_source_push(scope->model.display, &error->detail.display)))
            return false;
        if (copy > 0) {
            float offset[3];
            static const int32_t random_pc[] = {260, 279, 298};
            for (size_t i = 0; i < 3; ++i) {
                item_at(&f, random_pc[i]);
                if (bb->f == NULL)
                    return ld_null(&f, "Item actual copy Random receiver");
                const float value = mc_java_random_next_float(bb->f);
                offset[i] = at_fdiv(at_fmul(at_fsub(at_fmul(value, 2.0f), 1.0f), 0.2f), scale);
            }
            item_at(&f, 319);
            if (!ld_display(&f, mc_client_display_source_translate(scope->model.display, offset[0],
                                                                   offset[1], offset[2],
                                                                   &error->detail.display)))
                return false;
        }
        if (!block_at(&f, stack, 334, &block))
            return false;
        int32_t metadata;
        item_at(&f, 337);
        error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_STACK;
        error->detail.stack = mc_item_stack_source_damage_get(stack, &metadata);
        if (error->detail.stack != MC_ITEM_STACK_OK)
            return ld_fail(&f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, "Item actual Stack metadata");
        item_at(&f, 343);
        float brightness;
        if (!ld_brightness(&f, partial, &brightness))
            return false;
        item_at(&f, 346);
        if (!item_inventory(&f, bb->e, block, metadata, brightness))
            return false;
        item_at(&f, 349);
        if (!ld_display(&f,
                        mc_client_display_source_pop(scope->model.display, &error->detail.display)))
            return false;
        ++error->state.item.copies_returned;
    }
    item_at(&f, 790);
    if (!ld_display(&f, mc_client_display_source_rescale_normal(scope->model.display, false,
                                                                &error->detail.display)))
        return false;
    error->state.item.rescale_enabled = false;
    item_at(&f, 793);
    if (!ld_display(&f, mc_client_display_source_pop(scope->model.display, &error->detail.display)))
        return false;
    error->state.item.matrix_pushed = false;
    item_at(&f, 796);
    error->state.returned = error->state.item.returned = true;
    error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_NONE;
    return true;
}
