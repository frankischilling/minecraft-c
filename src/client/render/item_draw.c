#include "living_draw/internal.h"

#include "block/registry/runtime_internal.h"
#include "blocks/internal.h"
#include "item/render/internal.h"
#include "item/stack/source_internal.h"
#include "item_billboard.h"
#include "item_renderer_internal.h"
#include "item_scale.h"
#include "mc/core/java/java.h"
#include "world/runtime/block_normal_internal.h"

static const char item_descriptor[] = "(Lhl;DDDFF)V";
static const uint16_t terrain_units[] = {'/', 't', 'e', 'r', 'r', 'a',
                                         'i', 'n', '.', 'p', 'n', 'g'};
static const mc_player_string terrain = {SIZE_MAX, sizeof terrain_units / sizeof *terrain_units,
                                         terrain_units};
static const uint16_t item_units[] = {'/', 'g', 'u', 'i', '/', 'i', 't',
                                      'e', 'm', 's', '.', 'p', 'n', 'g'};
static const mc_player_string item_texture = {SIZE_MAX, sizeof item_units / sizeof *item_units,
                                              item_units};

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

static bool item_registry_failure(ld_frame *f, mc_registry_classes *registry,
                                  const char *operation) {
    f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_REGISTRY;
    ld_registry_classify(f, registry, &f->error->detail.registry);
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, operation);
}

bool mc_client_item_block_scale(mc_registry_classes *registry, mc_item_stack *stack,
                                const mc_block *normal_receiver, float *out,
                                mc_client_living_draw_error *error) {
    if (out == NULL || error == NULL)
        return false;
    ld_frame f = {.error = error};
    item_at(&f, 188);
    if (normal_receiver == NULL)
        return ld_null(&f, "Item actual Block normal predicate receiver");
    float scale = 0.25f;
    if (!mc_world_source_block_normal(normal_receiver)) {
        item_at(&f, 196);
        if (stack == NULL)
            return ld_null(&f, "Item scale captured Stack ID receiver");
        int32_t id;
        error->detail.stack = mc_item_stack_source_id_get(stack, &id);
        if (error->detail.stack != MC_ITEM_STACK_OK) {
            error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_STACK;
            return ld_fail(&f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, "Item scale actual Stack ID");
        }
        item_at(&f, 199);
        const mc_block *singleton = NULL;
        if (mc_registry_block_static_get(registry, "al", &singleton, &error->detail.registry) !=
            MC_REGISTRY_OK)
            return item_registry_failure(&f, registry, "Item scale actual Block.al active use");
        item_at(&f, 202);
        if (singleton == NULL)
            return ld_null(&f, "Item scale current Block.al ID receiver");
        if (id != mc_block_fields_get(singleton)->bn) {
            mc_registry_block_m_view array;
            item_at(&f, 208);
            if (mc_registry_block_m_get(registry, &array, &error->detail.registry) !=
                MC_REGISTRY_OK)
                return item_registry_failure(&f, registry, "Item scale current Block.m active use");
            item_at(&f, 213);
            error->detail.stack = mc_item_stack_source_id_get(stack, &id);
            if (error->detail.stack != MC_ITEM_STACK_OK) {
                error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_STACK;
                return ld_fail(&f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                               "Item scale fresh captured Stack ID");
            }
            item_at(&f, 216);
            const mc_block *current = NULL;
            if (mc_registry_block_m_at(&array, id, &current, &error->detail.registry) !=
                MC_REGISTRY_OK)
                return item_registry_failure(&f, registry, "Item scale current Block.m element");
            item_at(&f, 217);
            if (current == NULL)
                return ld_null(&f, "Item scale current Block render-type receiver");
            mc_block_render_description description;
            if (mc_block_render_describe(current, &description) != MC_REGISTRY_OK)
                return ld_fail(&f, MC_CLIENT_LIVING_DRAW_STATE,
                               "Item scale actual Block render type");
            if (description.type != 16)
                scale = 0.5f;
        }
    }
    item_at(&f, 229);
    error->state.item.scale = scale;
    *out = scale;
    return true;
}

static bool billboard_prefix_result(ld_frame *f, const mc_client_item_billboard_context *c,
                                    bool returned) {
    if (returned)
        return true;
    item_at(f, (int32_t)c->source_pc);
    if (c->source_exception)
        return ld_null(f, c->error.operation);
    f->error->detail.graphics = c->error;
    return ld_graphics(f, false);
}

static bool item_billboard(ld_frame *f, const mc_client_item_renderer_fields *bb,
                           mc_item_stack *stack, int32_t copies, float partial) {
    mc_client_living_draw_error *e = f->error;
    e->state.item.billboard = true;
    e->state.item.scale = .5f;
    item_at(f, 367);
    if (!ld_display(f, mc_client_display_source_scale(f->scope->model.display, .5f, .5f, .5f,
                                                      &e->detail.display)))
        return false;
    item_at(f, 372);
    if (stack == NULL)
        return ld_null(f, "Item billboard null captured Stack icon receiver");
    int32_t icon;
    if (!ld_item_result(f, mc_item_stack_render_icon(f->renderer->parent->classes->registry, stack,
                                                     &icon, &e->detail.item)))
        return false;
    e->state.item.icon_returned = true;
    e->state.item.icon = icon;
    item_at(f, 379);
    int32_t id;
    e->detail.stack = mc_item_stack_source_id_get(stack, &id);
    if (e->detail.stack != MC_ITEM_STACK_OK) {
        e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_STACK;
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, "Item billboard current Stack ID");
    }
    item_at(f, id < 256 ? 391 : 400);
    if (!ld_texture(f, id < 256 ? &terrain : &item_texture, id < 256 ? 391 : 400))
        return false;
    item_at(f, 403);
    mc_client_item_billboard_context prefix = {0};
    if (!ld_graphics(f, mc_client_nw_active_use(f->scope->model.graphics, e->site, &prefix.nw,
                                                &e->detail.graphics)))
        return false;
    e->state.item.nw_identity = (uintptr_t)prefix.nw;
    if (!billboard_prefix_result(f, &prefix, mc_client_item_billboard_uv_capture(&prefix, icon)))
        return false;
    memcpy(e->state.item.uv, prefix.uv, sizeof(prefix.uv));
    if (bb->a) { /* Fresh actual BB.a485, after all four captured UV locals. */
        mc_registry_classes *registry = f->renderer->parent->classes->registry;
        const mc_registry_item_array *array = NULL;
        const mc_item *color_item = NULL;
        item_at(f, 492);
        if (mc_registry_item_c_get(registry, &array, &e->detail.registry) != MC_REGISTRY_OK)
            return item_registry_failure(f, registry, "Item billboard actual GM.c active use");
        item_at(f, 497);
        e->detail.stack = mc_item_stack_source_id_get(stack, &id);
        if (e->detail.stack != MC_ITEM_STACK_OK) {
            e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_STACK;
            return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                           "Item billboard color current Stack ID");
        }
        item_at(f, 500);
        if (mc_registry_item_c_at(registry, array, id, &color_item, &e->detail.registry) !=
            MC_REGISTRY_OK)
            return item_registry_failure(f, registry, "Item billboard actual GM.c aaload");
        item_at(f, 503);
        int32_t metadata;
        e->detail.stack = mc_item_stack_source_damage_get(stack, &metadata);
        if (e->detail.stack != MC_ITEM_STACK_OK) {
            e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_STACK;
            return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                           "Item billboard actual Stack metadata");
        }
        item_at(f, 506);
        if (color_item == NULL)
            return ld_null(f, "Item billboard null captured color Item receiver");
        int32_t color;
        const mc_item_render_color_context colors = {f->scope->model.graphics,
                                                     mc_client_colorizer_read};
        if (!ld_item_result(f, mc_item_render_color_with_context(registry, color_item, metadata,
                                                                 &colors, &color, &e->detail.item)))
            return false;
        item_at(f, 523);
        if (!ld_fp(f))
            return false;
        const float red = at_fdiv((float)(((uint32_t)color >> 16u) & 255u), 255.0f);
        const float green = at_fdiv((float)(((uint32_t)color >> 8u) & 255u), 255.0f);
        const float blue = at_fdiv((float)((uint32_t)color & 255u), 255.0f);
        item_at(f, 556);
        float brightness;
        if (!ld_brightness(f, partial, &brightness))
            return false;
        e->state.item.color = color;
        e->state.item.brightness = brightness;
        item_at(f, 565);
        if (!ld_fp(f))
            return false;
        item_at(f, 577);
        if (!ld_display(f, mc_client_display_source_color(
                               f->scope->model.display, at_fmul(red, brightness),
                               at_fmul(green, brightness), at_fmul(blue, brightness), 1.0f,
                               &e->detail.display)))
            return false;
        e->state.item.color_returned = true;
    }
    for (int32_t copy = 0; copy < copies; ++copy) {
        if (!ld_work(f))
            return false;
        item_at(f, 590);
        if (!ld_display(f,
                        mc_client_display_source_push(f->scope->model.display, &e->detail.display)))
            return false;
        if (copy > 0) {
            float offset[3];
            static const int32_t random_pc[] = {602, 618, 634};
            for (size_t i = 0; i < 3; ++i) {
                item_at(f, random_pc[i]);
                if (bb->f == NULL)
                    return ld_null(f, "Item billboard actual copy Random receiver");
                const float value = mc_java_random_next_float(bb->f);
                item_at(f, random_pc[i] + 3);
                if (!ld_fp(f))
                    return false;
                offset[i] = at_fmul(at_fsub(at_fmul(value, 2.0f), 1.0f), .3f);
            }
            item_at(f, 652);
            if (!ld_display(f, mc_client_display_source_translate(f->scope->model.display,
                                                                  offset[0], offset[1], offset[2],
                                                                  &e->detail.display)))
                return false;
        }
        item_at(f, 661);
        mc_client_render_manager *manager = ld_manager(f, 661);
        if (manager == NULL)
            return false;
        item_at(f, 664);
        if (!ld_fp(f))
            return false;
        const float angle = at_fsub(180.0f, manager->fields.i);
        item_at(f, 668);
        if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display, angle, 0, 1, 0,
                                                           &e->detail.display)))
            return false;
        const bool prefix_returned = mc_client_item_billboard_entered(&prefix);
        e->state.item.billboard_vertices_returned += prefix.vertices;
        if (!billboard_prefix_result(f, &prefix, prefix_returned))
            return false;
        item_at(f, 776);
        if (!ld_graphics(
                f, mc_client_nw_draw(prefix.nw, f->scope->model.display, &e->detail.graphics)))
            return false;
        ++e->state.item.billboard_quads_returned;
        item_at(f, 779);
        if (!ld_display(f,
                        mc_client_display_source_pop(f->scope->model.display, &e->detail.display)))
            return false;
        ++e->state.item.copies_returned;
    }
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
    bool billboard = id >= 256;
    const mc_block *block;
    if (!billboard) {
        if (!block_at(&f, stack, 151, &block))
            return false;
        item_at(&f, 152);
        if (block == NULL)
            return ld_null(&f, "Item Block render-type null receiver");
        mc_block_render_description description;
        if (mc_block_render_describe(block, &description) != MC_REGISTRY_OK)
            return ld_fail(&f, MC_CLIENT_LIVING_DRAW_STATE, "Item actual Block render type");
        billboard = !mc_cv_inventory_type_3d(description.type);
    }
    if (billboard) {
        if (!item_billboard(&f, bb, stack, copies, partial))
            return false;
        goto suffix;
    }
    item_at(&f, 166);
    if (!ld_display(&f, mc_client_display_source_rotate(scope->model.display, rotation, 0, 1, 0,
                                                        &error->detail.display)))
        return false;
    item_at(&f, 172);
    if (!ld_texture(&f, &terrain, 172))
        return false;
    if (!block_at(&f, stack, 187, &block))
        return false;
    float scale;
    if (!mc_client_item_block_scale(renderer->parent->classes->registry, stack, block, &scale,
                                    error))
        return false;
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
suffix:
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
