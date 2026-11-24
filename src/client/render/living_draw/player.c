#include "block/registry/registry_internal.h"
#include "block/registry/runtime_internal.h"
#include "client/render/blocks/internal.h"
#include "entity/player/nbt_internal.h"
#include "internal.h"
#include "inventory/internal.h"
#include "item/registry/registry_internal.h"
#include "item/stack/source_internal.h"
#include "mc/core/java/java.h"
#include "mc/inventory/player_current.h"
#include "mc/item/registry/runtime.h"

#include <stdlib.h>

static void player_at(ld_frame *f, const char *method, const char *descriptor, int32_t pc) {
    ld_at(f, "ds", method, descriptor, pc);
}

static mc_player_fields *player_fields(ld_frame *f) {
    if (f->actor == NULL) {
        ld_null(f, "Player renderer null Player field receiver");
        return NULL;
    }
    if (!mc_entity_is_kind(f->actor, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        f->actor->local_player == NULL) {
        ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED, "Player renderer actual Player storage");
        return NULL;
    }
    return &f->actor->local_player->fields.player;
}

static bool model_owned(ld_frame *f, manager_model *model) {
    if (model == NULL)
        return ld_null(f, "Player renderer null captured FH receiver");
    for (manager_model *p = f->renderer->models; p != NULL; p = p->next)
        if (p == model)
            return true;
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Player renderer FH allocation provenance");
}

static bool flag_stores(ld_frame *f, bool sneaking, bool value, bool clearing) {
    /* DS evaluates g/h/a before the first dup_x1 store; a is stored first. */
    manager_model *g = f->renderer->g, *h = f->renderer->h, *a = f->renderer->own_model;
    manager_model *captured[3] = {a, h, g};
    static const int32_t held[] = {32, 36, 39}, sneak[] = {59, 63, 66};
    static const int32_t clear_sneak[] = {129, 133, 136}, clear_held[] = {153, 157, 160};
    const int32_t *pcs =
        clearing ? (sneaking ? clear_sneak : clear_held) : (sneaking ? sneak : held);
    for (size_t i = 0; i < 3; ++i) {
        player_at(f, "a", "(Lgs;DDDFF)V", pcs[i]);
        if (!model_owned(f, captured[i]))
            return false;
        if (sneaking)
            captured[i]->fields.l = value;
        else
            captured[i]->fields.k = value;
        if (clearing)
            ++f->error->state.player.cleared_flag_stores;
        else if (sneaking)
            ++f->error->state.player.sneak_flag_stores;
        else
            ++f->error->state.player.held_flag_stores;
    }
    return true;
}

static bool current_stack(ld_frame *f, int32_t pc, mc_item_stack **out) {
    const char *descriptor = pc == 660 ? "(Lgs;F)V" : "(Lgs;DDDFF)V";
    player_at(f, "a", descriptor, pc == 660 ? 657 : 1);
    mc_player_fields *p = player_fields(f);
    if (p == NULL)
        return false;
    player_at(f, "a", descriptor, pc);
    if (p->c == NULL)
        return ld_null(f, "Player renderer null Inventory current receiver");
    const mc_item_stack *stack = NULL;
    mc_inventory_current_error error;
    const mc_inventory_status status = mc_inventory_player_current(p->c, &stack, &error);
    if (status != MC_INVENTORY_OK) {
        f->error->child =
            ld_site(error.site.class_name, error.site.method, error.site.descriptor, error.site.pc);
        if (!error.native_policy && status == MC_INVENTORY_NULL_REFERENCE)
            return ld_null(f, error.operation);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, error.operation);
    }
    *out = (mc_item_stack *)stack;
    return ld_capture(f, mc_client_draw_capture_stack(f->scope, *out, &f->error->detail.capture));
}

bool ld_player_prepare(ld_frame *f, double *y) {
    f->error->state.player.entered = true;
    mc_item_stack *stack;
    if (!current_stack(f, 4, &stack))
        return false;
    f->error->state.player.initial_stack_returned = true;
    f->error->state.player.initial_stack_identity = (uintptr_t)stack;
    if (!flag_stores(f, false, stack != NULL, false))
        return false;
    player_at(f, "a", "(Lgs;DDDFF)V", 55);
    bool sneaking;
    if (!ld_entity(f,
                   mc_entity_render_player_sneaking(f->actor, &sneaking, &f->error->detail.entity)))
        return false;
    f->error->state.player.sneaking_returned = true;
    f->error->state.player.sneaking = sneaking;
    if (!flag_stores(f, true, sneaking, false))
        return false;
    player_at(f, "a", "(Lgs;DDDFF)V", 76);
    if (!ld_fp(f))
        return false;
    *y = at_sub(*y, at_widen(f->actor->base.offset));
    player_at(f, "a", "(Lgs;DDDFF)V", 80);
    if (!ld_entity(f,
                   mc_entity_render_player_sneaking(f->actor, &sneaking, &f->error->detail.entity)))
        return false;
    /* This owner implements the canonical concrete DC; source instanceof DC
     * at87 therefore excludes the extra .125 offset for a remote GS. */
    return true;
}

bool ld_player_finish(ld_frame *f) {
    if (!flag_stores(f, true, false, true) || !flag_stores(f, false, false, true))
        return false;
    player_at(f, "a", "(Lgs;DDDFF)V", 163);
    f->error->state.player.wrapper_returned = true;
    f->error->state.returned = true;
    return true;
}

static bool armor_stack(ld_frame *f, int32_t slot, int32_t pc, mc_item_stack **out) {
    mc_player_fields *p = player_fields(f);
    if (p == NULL)
        return false;
    f->error->site.pc = pc;
    if (p->c == NULL)
        return ld_null(f, "Player renderer null Inventory armor receiver");
    f->error->child = ld_site("ix", "d", "(I)Liz;", 5);
    if (p->c->side != MC_REGISTRY_CLIENT || p->c->active)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Player renderer actual Inventory owner");
    if (p->c->armor_slots == NULL)
        return ld_null(f, "Player renderer null actual armor array");
    if (slot < 0 || slot >= 4) {
        f->error->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
        f->error->source_exception = true;
        f->error->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_SOURCE_EXCEPTION, "Player renderer armor index");
    }
    *out = p->c->armor_slots[slot];
    return ld_capture(f, mc_client_draw_capture_stack(f->scope, *out, &f->error->detail.capture));
}

static bool stack_item(ld_frame *f, mc_item_stack *stack, const mc_item **out) {
    mc_registry_classes *registry = f->renderer->parent->classes->registry;
    const mc_registry_item_array *array;
    f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_REGISTRY;
    f->error->child = ld_site("iz", "a", "()Lgm;", 0);
    if (mc_registry_item_c_get(registry, &array, &f->error->detail.registry) != MC_REGISTRY_OK)
        goto failed;
    int32_t id;
    f->error->child.pc = 4;
    if (mc_item_stack_source_id_get(stack, &id) != MC_ITEM_STACK_OK)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Player render actual Stack ID");
    f->error->child.pc = 7;
    if (mc_registry_item_c_at(registry, array, id, out, &f->error->detail.registry) !=
        MC_REGISTRY_OK)
        goto failed;
    f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_NONE;
    return true;
failed:
    ld_registry_classify(f, registry, &f->error->detail.registry);
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, f->error->detail.registry.message);
}

bool ld_player_pass(ld_frame *f, int32_t pass, float partial, bool *out) {
    (void)partial;
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site parent = e->parent;
    e->parent = e->site;
    player_at(f, "a", "(Lls;IF)Z", 2);
    player_at(f, "a", "(Lgs;IF)Z", 1);
    mc_item_stack *stack;
    if (!armor_stack(f, mc_java_i32_sub(3, pass), 7, &stack))
        return false;
    if (pass >= 0 && pass < 4) {
        e->state.player.armor_attempted[pass] = true;
        e->state.player.armor_stack_identities[pass] = (uintptr_t)stack;
    }
    *out = false;
    if (stack != NULL) {
        player_at(f, "a", "(Lgs;IF)Z", 19);
        const mc_item *item;
        if (!stack_item(f, stack, &item))
            return false;
        if (item != NULL && item->kind == MC_ITEM_ARMOR) {
            player_at(f, "a", "(Lgs;IF)Z", 60);
            mc_client_render_manager_classes *classes = f->renderer->parent->classes;
            mc_player_string **materials = classes->ds_i;
            const int32_t material = item->subtype.wa.bm;
            if (materials == NULL)
                return ld_null(f, "Player armor null captured DS material array");
            if (material < 0 || material >= 5) {
                e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
                e->source_exception = true;
                e->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
                return ld_fail(f, MC_CLIENT_LIVING_DRAW_SOURCE_EXCEPTION,
                               "Player armor material index");
            }
            mc_player_string_view text;
            static const uint16_t null_text[] = {'n', 'u', 'l', 'l'};
            if (materials[material] == NULL)
                text = (mc_player_string_view){null_text, 4};
            else if (!mc_player_string_read(materials[material], &text))
                return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE,
                               "Player armor actual material String");
            uint16_t *key = malloc((13 + text.length) * sizeof(*key));
            static const uint16_t prefix[] = {'/', 'a', 'r', 'm', 'o', 'r', '/'};
            if (key == NULL)
                return ld_fail(f, MC_CLIENT_LIVING_DRAW_LIMIT,
                               "Player armor texture key native allocation");
            memcpy(key, prefix, sizeof(prefix));
            memcpy(key + 7, text.units, text.length * sizeof(*key));
            const uint16_t suffix[] = {'_', pass == 2 ? '2' : '1', '.', 'p', 'n', 'g'};
            memcpy(key + 7 + text.length, suffix, sizeof(suffix));
            mc_player_string *name = NULL;
            player_at(f, "a", "(Lgs;IF)Z", 87);
            const mc_player_status created = mc_player_string_create(key, 13 + text.length, &name);
            free(key);
            if (created != MC_PLAYER_OK)
                return ld_fail(f, MC_CLIENT_LIVING_DRAW_LIMIT,
                               "Player armor texture String allocation");
            const bool held =
                ld_capture(f, mc_client_draw_capture_string(f->scope, name, &e->detail.capture));
            player_at(f, "a", "(Lgs;IF)Z", 90);
            const bool textured = held && ld_texture(f, name, 90);
            mc_player_string_release(name);
            if (!textured)
                return false;
            manager_model *model = pass == 2 ? f->renderer->h : f->renderer->g;
            player_at(f, "a", "(Lgs;IF)Z", 113);
            if (!model_owned(f, model))
                return false;
            mc_client_model_part *parts[7] = {model->fields.a, model->fields.b, model->fields.c,
                                              model->fields.d, model->fields.e, model->fields.f,
                                              model->fields.g};
            const bool visible[7] = {pass == 0,
                                     pass == 0,
                                     pass == 1 || pass == 2,
                                     pass == 1,
                                     pass == 1,
                                     pass == 2 || pass == 3,
                                     pass == 2 || pass == 3};
            static const int32_t stores[] = {125, 142, 165, 183, 201, 224, 247};
            for (size_t i = 0; i < 7; ++i) {
                player_at(f, "a", "(Lgs;IF)Z", stores[i]);
                if (parts[i] == NULL)
                    return ld_null(f, "Player armor null PS visibility receiver");
                parts[i]->h = visible[i];
            }
            player_at(f, "a", "(Lgs;IF)Z", 253);
            f->renderer->secondary = model;
            e->state.secondary_stored_identity = (uintptr_t)model;
            *out = true;
            if (pass >= 0 && pass < 4)
                e->state.player.armor_selected[pass] = true;
        }
    }
    player_at(f, "a", "(Lgs;IF)Z", *out ? 257 : 259);
    e->parent = parent;
    return true;
}

bool ld_player_translate(ld_frame *f, double x, double y, double z) {
    player_at(f, "b", "(Lgs;DDD)V", 1);
    mc_player_fields *p = player_fields(f);
    if (p == NULL)
        return false;
    const mc_living_entity_fields *ls = mc_entity_living_fields(f->actor);
    if (!f->actor->base.be && ls->Y > 0 && p->u) {
        player_at(f, "b", "(Lgs;DDD)V", 22);
        if (!ld_fp(f))
            return false;
        x = at_add(x, at_widen(p->w));
        y = at_add(y, at_widen(p->x));
        z = at_add(z, at_widen(p->y));
    }
    ld_at(f, "gv", "b", "(Lls;DDD)V", 8);
    return ld_display(f, mc_client_display_source_translate(f->scope->model.display, at_narrow(x),
                                                            at_narrow(y), at_narrow(z),
                                                            &f->error->detail.display));
}

static bool orientation_receipt(ld_frame *f, const void *receiver, const char *source,
                                const char *method, const char *descriptor,
                                mc_client_graphics_site caller, const int32_t *args, size_t nargs,
                                mc_client_manager_draw_query_kind kind, size_t *index) {
    mc_client_manager_draw_query query = {.receiver = (uintptr_t)receiver,
                                          .method = {source, method, descriptor, 0},
                                          .caller = caller,
                                          .nargs = nargs,
                                          .kind = kind};
    if (nargs != 0)
        memcpy(query.args, args, nargs * sizeof(*args));
    if (mc_client_draw_query_begin(f->scope, &query, index, &f->error->capture_failure))
        return true;
    return ld_fail(f,
                   f->error->capture_failure.status == MC_CLIENT_MANAGER_LIMIT
                       ? MC_CLIENT_LIVING_DRAW_LIMIT
                       : MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                   f->error->capture_failure.operation);
}

static void orientation_end(ld_frame *f, size_t index, bool invoked, bool returned, uint32_t bits) {
    mc_client_draw_query_end(
        f->scope, index, invoked, returned, bits, !returned && f->error->native_policy,
        !returned && f->error->source_exception, returned ? NULL : f->error->exception_class);
}

static const mc_player_coordinates *orientation_coordinate(ld_frame *f, unsigned int pc,
                                                           size_t index) {
    ld_at(f, "gs", "M", "()F", (int32_t)pc);
    const mc_player_coordinates *value = f->actor->local_player->fields.player.v;
    f->error->state.player.bed_coordinate_identities[index] = (uintptr_t)value;
    if (value == NULL) {
        (void)ld_null(f, "Player orientation current v null coordinate receiver");
        return NULL;
    }
    const struct mc_player_nbt_coordinate *node = f->actor->local_player->nbt_coordinates;
    while (node != NULL && &node->value != value)
        node = node->next;
    if (node == NULL) {
        (void)ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE,
                      "Player orientation actual coordinate provenance");
        return NULL;
    }
    return value;
}

/* Actual GS.M. World and source-producer occurrences belong to this genuine
 * Manager draw scope; no separate source owner or camera service is created. */
static bool player_bed_orientation(ld_frame *f, float *out) {
    mc_client_living_draw_error *e = f->error;
    ld_at(f, "gs", "M", "()F", 1);
    if (f->actor->local_player->fields.player.v == NULL) {
        ld_at(f, "gs", "M", "()F", 84);
        *out = 0.0f;
        return true;
    }
    mc_world_runtime *world = f->actor->base.world; /* Actual World8, before XYZ. */
    e->state.player.bed_world_identity = (uintptr_t)world;
    const mc_player_coordinates *coordinate = orientation_coordinate(f, 15, 0);
    if (coordinate == NULL)
        return false;
    const int32_t x = coordinate->x;
    coordinate = orientation_coordinate(f, 22, 1);
    if (coordinate == NULL)
        return false;
    const int32_t y = coordinate->y;
    coordinate = orientation_coordinate(f, 29, 2);
    if (coordinate == NULL)
        return false;
    const int32_t z = coordinate->z;
    ld_at(f, "gs", "M", "()F", 32);
    if (!ld_capture(f, mc_client_draw_capture_world(f->scope, world, &e->detail.capture)))
        return false;
    const int32_t args[] = {x, y, z};
    size_t receipt;
    if (!orientation_receipt(f, world, "fd", "e", "(III)I", e->site, args, 3,
                             MC_CLIENT_MANAGER_DRAW_QUERY_INTEGER, &receipt))
        return false;
    if (world == NULL) {
        (void)ld_null(f, "Player orientation actual metadata null World receiver");
        orientation_end(f, receipt, false, false, 0);
        return false;
    }
    mc_client_draw_query_enter(f->scope, receipt);
    memset(&e->detail, 0, sizeof(e->detail));
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_BLOCK;
    int32_t metadata = 0;
    const mc_world_storage_status status = mc_world_runtime_metadata_get_detailed(
        world, x, y, z, &metadata, &e->detail.block.cause, &e->detail.block.acquisition);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_acquisition_error *child = &e->detail.block.acquisition;
        e->child = ld_site("fd", "e", "(III)I", 0);
        ld_registry_classify(f, mc_world_runtime_registry(world), &child->registry);
        ld_access_classify(f, &child->batch.publication.world_access_failure);
        ld_access_classify(f, &child->tile_entity.world_access_failure);
        ld_access_classify(f, &child->tile_entity.publication.world_access_failure);
        ld_access_classify(f, &child->tile_entity.batch.publication.world_access_failure);
        (void)ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, e->detail.block.cause.operation);
    } else {
        e->state.player.sleep_metadata_returned = true;
        e->state.player.bed_metadata = metadata;
    }
    const bool captured = mc_client_draw_capture_acquisition(f->scope, &e->detail.block.acquisition,
                                                             &e->capture_failure);
    if (!captured && status == MC_WORLD_STORAGE_OK)
        (void)ld_fail(f,
                      e->capture_failure.status == MC_CLIENT_MANAGER_LIMIT
                          ? MC_CLIENT_LIVING_DRAW_LIMIT
                          : MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                      e->capture_failure.operation);
    orientation_end(f, receipt, true, status == MC_WORLD_STORAGE_OK, (uint32_t)metadata);
    if (status != MC_WORLD_STORAGE_OK || !captured)
        return false;
    ld_at(f, "gs", "M", "()F", 37);
    mc_registry_classes *registry = f->renderer->parent->classes->registry;
    if (registry == NULL || registry->entities != f->actor->classes)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE,
                       "Player orientation actual defining GS Registry");
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_REGISTRY;
    if (mc_registry_bed_active_use(registry, &e->detail.registry) != MC_REGISTRY_OK) {
        ld_registry_classify(f, registry, &e->detail.registry);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, e->detail.registry.message);
    }
    static const float angles[] = {90.0f, 0.0f, 270.0f, 180.0f};
    *out = angles[(uint32_t)metadata & 3u];
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_NONE;
    return true;
}

bool ld_player_rotation(ld_frame *f, float age, float body, float partial) {
    player_at(f, "a", "(Lgs;FFF)V", 1);
    mc_player_fields *p = player_fields(f);
    if (p == NULL)
        return false;
    const mc_living_entity_fields *ls = mc_entity_living_fields(f->actor);
    if (!f->actor->base.be && ls->Y > 0 && p->u) {
        player_at(f, "a", "(Lgs;FFF)V", 15);
        const mc_client_graphics_site caller = f->error->site;
        const mc_client_graphics_site parent = f->error->parent;
        size_t receipt;
        if (!orientation_receipt(f, f->actor, "gs", "M", "()F", caller, NULL, 0,
                                 MC_CLIENT_MANAGER_DRAW_QUERY_FLOAT_BITS, &receipt))
            return false;
        mc_client_draw_query_enter(f->scope, receipt);
        f->error->parent = caller;
        f->error->state.player.sleep_orientation_entered = true;
        float angle = 0.0f;
        const bool returned = player_bed_orientation(f, &angle);
        uint32_t bits;
        memcpy(&bits, &angle, sizeof(bits));
        orientation_end(f, receipt, true, returned, bits);
        if (!returned)
            return false;
        f->error->state.player.sleep_orientation_returned = true;
        f->error->state.player.bed_orientation = angle;
        player_at(f, "a", "(Lgs;FFF)V", 21);
        if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display, angle, 0.0f,
                                                           1.0f, 0.0f, &f->error->detail.display)))
            return false;
        ++f->error->state.player.sleep_rotations_returned;
        /* Actual inherited GV.a(LS) at26 is the literal90 body. */
        player_at(f, "a", "(Lgs;FFF)V", 32);
        if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display, 90.0f, 0.0f,
                                                           0.0f, 1.0f, &f->error->detail.display)))
            return false;
        ++f->error->state.player.sleep_rotations_returned;
        player_at(f, "a", "(Lgs;FFF)V", 40);
        if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display, 270.0f, 0.0f,
                                                           1.0f, 0.0f, &f->error->detail.display)))
            return false;
        ++f->error->state.player.sleep_rotations_returned;
        player_at(f, "a", "(Lgs;FFF)V", 55);
        f->error->state.rotation_returned = true;
        f->error->parent = parent;
        return true;
    }
    return ld_rotation(f, age, body, partial);
}

bool ld_player_prescale(ld_frame *f, float partial) {
    (void)partial;
    player_at(f, "b", "(Lgs;F)V", 6);
    if (!ld_display(f, mc_client_display_source_scale(f->scope->model.display, .9375, .9375, .9375,
                                                      &f->error->detail.display)))
        return false;
    f->error->state.prescale_returned = true;
    return true;
}

#define PGL(pc, call)                                                                              \
    do {                                                                                           \
        player_at(f, "a", "(Lgs;F)V", pc);                                                         \
        if (!ld_display(f, (call)))                                                                \
            return false;                                                                          \
    } while (0)
#define PD (&f->error->detail.display)
#define DISPLAY (f->scope->model.display)

static bool attachment(ld_frame *f, bool head, int32_t field_pc, int32_t invoke_pc) {
    player_at(f, "a", "(Lgs;F)V", field_pc);
    manager_model *model = f->renderer->own_model;
    if (!model_owned(f, model))
        return false;
    mc_client_model_part *part = head ? model->fields.a : model->fields.d;
    player_at(f, "a", "(Lgs;F)V", invoke_pc);
    if (part == NULL)
        return ld_null(f, "Player attachment null PS invoke receiver");
    return ld_part_result(f, mc_client_model_part_attach(part, f->scope->model.graphics,
                                                         f->scope->model.entities, DISPLAY, .0625f,
                                                         &f->error->detail.part));
}

static bool registry_failed(ld_frame *f, mc_registry_classes *registry) {
    ld_registry_classify(f, registry, &f->error->detail.registry);
    f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_REGISTRY;
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, f->error->detail.registry.message);
}

static bool block_3d(ld_frame *f, mc_item_stack *stack, bool head, bool *out) {
    mc_registry_classes *registry = f->renderer->parent->classes->registry;
    mc_registry_block_m_view blocks;
    player_at(f, "a", "(Lgs;F)V", head ? 41 : 724);
    if (mc_registry_block_m_get(registry, &blocks, &f->error->detail.registry) != MC_REGISTRY_OK)
        return registry_failed(f, registry);
    int32_t id;
    player_at(f, "a", "(Lgs;F)V", head ? 45 : 729);
    if (mc_item_stack_source_id_get(stack, &id) != MC_ITEM_STACK_OK)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Player attachment current Stack ID");
    const mc_block *block;
    player_at(f, "a", "(Lgs;F)V", head ? 48 : 732);
    if (mc_registry_block_m_at(&blocks, id, &block, &f->error->detail.registry) != MC_REGISTRY_OK)
        return registry_failed(f, registry);
    player_at(f, "a", "(Lgs;F)V", head ? 49 : 733);
    if (block == NULL)
        return ld_null(f, "Player attachment null Block render-type receiver");
    mc_block_render_description description;
    if (mc_block_render_describe(block, &description) != MC_REGISTRY_OK)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Player attachment Block declaration");
    *out = mc_cv_inventory_type_3d(description.type);
    return true;
}

static bool player_item(ld_frame *f, mc_item_stack *stack, int32_t active_pc, int32_t id_pc,
                        int32_t at_pc, const mc_item **out) {
    mc_registry_classes *registry = f->renderer->parent->classes->registry;
    const mc_registry_item_array *array;
    player_at(f, "a", "(Lgs;F)V", active_pc);
    if (mc_registry_item_c_get(registry, &array, &f->error->detail.registry) != MC_REGISTRY_OK)
        return registry_failed(f, registry);
    player_at(f, "a", "(Lgs;F)V", id_pc);
    int32_t id;
    if (mc_item_stack_source_id_get(stack, &id) != MC_ITEM_STACK_OK)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Player attachment late Item ID");
    player_at(f, "a", "(Lgs;F)V", at_pc);
    if (mc_registry_item_c_at(registry, array, id, out, &f->error->detail.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(f, registry);
    return true;
}

static bool held_item(ld_frame *f, mc_item_stack *stack) {
    PGL(670, mc_client_display_source_push(DISPLAY, PD));
    if (!attachment(f, false, 677, 682))
        return false;
    PGL(691, mc_client_display_source_translate(DISPLAY, -.0625f, .4375f, .0625f, PD));
    player_at(f, "a", "(Lgs;F)V", 695);
    mc_player_fields *p = player_fields(f);
    if (p == NULL)
        return false;
    if (p->D != NULL) {
        player_at(f, "a", "(Lgs;F)V", 701);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Player actual fishing-hook replacement Stack");
    }
    player_at(f, "a", "(Lgs;F)V", 715);
    int32_t id;
    if (mc_item_stack_source_id_get(stack, &id) != MC_ITEM_STACK_OK)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Player held actual Stack ID");
    bool three_d = false;
    if (id < 256 && !block_3d(f, stack, false, &three_d))
        return false;
    if (three_d) {
        PGL(751, mc_client_display_source_translate(DISPLAY, 0, .1875f, -.3125f, PD));
        PGL(766, mc_client_display_source_rotate(DISPLAY, 20, 1, 0, 0, PD));
        PGL(774, mc_client_display_source_rotate(DISPLAY, 45, 0, 1, 0, PD));
        PGL(784, mc_client_display_source_scale(DISPLAY, .375, -.375, .375, PD));
    } else {
        const mc_item *item;
        if (!player_item(f, stack, 790, 795, 798, &item))
            return false;
        player_at(f, "a", "(Lgs;F)V", 799);
        if (item == NULL)
            return ld_null(f, "Player held null Item full3D invoke receiver");
        bool full;
        if (!ld_item_result(f, mc_item_render_full_3d(f->renderer->parent->classes->registry, item,
                                                      &full, &f->error->detail.item)))
            return false;
        if (full) {
            if (!player_item(f, stack, 809, 814, 817, &item))
                return false;
            player_at(f, "a", "(Lgs;F)V", 818);
            if (item == NULL)
                return ld_null(f, "Player held null Item rotate invoke receiver");
            bool rotate;
            if (!ld_item_result(f, mc_item_render_rotate(f->renderer->parent->classes->registry,
                                                         item, &rotate, &f->error->detail.item)))
                return false;
            if (rotate) {
                PGL(829, mc_client_display_source_rotate(DISPLAY, 180, 0, 0, 1, PD));
                PGL(836, mc_client_display_source_translate(DISPLAY, 0, -.125f, 0, PD));
            }
            PGL(843, mc_client_display_source_translate(DISPLAY, 0, .1875f, 0, PD));
            PGL(853, mc_client_display_source_scale(DISPLAY, .625, -.625, .625, PD));
            PGL(861, mc_client_display_source_rotate(DISPLAY, -100, 1, 0, 0, PD));
            PGL(869, mc_client_display_source_rotate(DISPLAY, 45, 0, 1, 0, PD));
        } else {
            PGL(885, mc_client_display_source_translate(DISPLAY, .25f, .1875f, -.1875f, PD));
            PGL(894, mc_client_display_source_scale(DISPLAY, .375, .375, .375, PD));
            PGL(902, mc_client_display_source_rotate(DISPLAY, 60, 0, 0, 1, PD));
            PGL(910, mc_client_display_source_rotate(DISPLAY, -90, 1, 0, 0, PD));
            PGL(918, mc_client_display_source_rotate(DISPLAY, 20, 0, 0, 1, PD));
        }
    }
    if (!ld_held_item(f, stack, 925, 931))
        return false;
    PGL(934, mc_client_display_source_pop(DISPLAY, PD));
    return true;
}

static bool auxiliary_model(ld_frame *f, bool ears) {
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site caller = e->site;
    const mc_client_graphics_site parent = e->parent;
    manager_model *model = f->renderer->own_model;
    if (!model_owned(f, model))
        return false;
    e->parent = caller;
    const char *method = ears ? "a" : "b";
    mc_client_model_part *part;
    if (ears) {
        ld_at(f, "fh", method, "(F)V", 8);
        part = model->fields.h;
        mc_client_model_part *head = model->fields.a;
        if (head == NULL)
            return ld_null(f, "Player ear model null head yaw field receiver");
        const float yaw = head->e;
        ld_at(f, "fh", method, "(F)V", 11);
        if (part == NULL)
            return ld_null(f, "Player ear model null captured ear yaw store receiver");
        part->e = yaw;
        ld_at(f, "fh", method, "(F)V", 22);
        part = model->fields.h;
        head = model->fields.a;
        if (head == NULL)
            return ld_null(f, "Player ear model null head pitch field receiver");
        const float pitch = head->d;
        ld_at(f, "fh", method, "(F)V", 25);
        if (part == NULL)
            return ld_null(f, "Player ear model null captured ear pitch store receiver");
        part->d = pitch;
        ld_at(f, "fh", method, "(F)V", 33);
        if (model->fields.h == NULL)
            return ld_null(f, "Player ear model null x pivot store receiver");
        model->fields.h->a = 0;
        ld_at(f, "fh", method, "(F)V", 41);
        if (model->fields.h == NULL)
            return ld_null(f, "Player ear model null y pivot store receiver");
        model->fields.h->b = 0;
        part = model->fields.h;
    } else
        part = model->fields.i;
    ld_at(f, "fh", method, "(F)V", ears ? 49 : 5);
    if (part == NULL)
        return ld_null(f, "Player auxiliary null PS draw invocation receiver");
    if (!ld_part_result(f, mc_client_model_part_draw(part, f->scope->model.graphics,
                                                     f->scope->model.entities, DISPLAY, .0625f,
                                                     &e->detail.part)))
        return false;
    e->site = caller;
    e->parent = parent;
    return true;
}

static bool ears_layer(ld_frame *f, float partial) {
    mc_client_living_draw_error *e = f->error;
    const mc_player_string *skin = (const mc_player_string *)f->actor->base.client_only.bA;
    if (!ld_capture(f, mc_client_draw_capture_string(f->scope, skin, &e->detail.capture)))
        return false;
    bool textured;
    if (!ld_download(f, skin, NULL, ld_site("ds", "a", "(Lgs;F)V", 120), &textured))
        return false;
    if (!textured)
        return true;
    for (int32_t i = 0; i < 2; ++i) {
        player_at(f, "a", "(Lgs;F)V", 135);
        if (!ld_work(f) || !ld_fp(f))
            return false;
        const mc_living_entity_fields *ls = mc_entity_living_fields(f->actor);
        const float yaw =
            at_fsub(at_fadd(f->actor->base.aU,
                            at_fmul(at_fsub(f->actor->base.yaw, f->actor->base.aU), partial)),
                    at_fadd(ls->I, at_fmul(at_fsub(ls->H, ls->I), partial)));
        const float pitch = at_fadd(
            f->actor->base.aV, at_fmul(at_fsub(f->actor->base.aT, f->actor->base.aV), partial));
        PGL(188, mc_client_display_source_push(DISPLAY, PD));
        PGL(196, mc_client_display_source_rotate(DISPLAY, yaw, 0, 1, 0, PD));
        PGL(204, mc_client_display_source_rotate(DISPLAY, pitch, 1, 0, 0, PD));
        PGL(219, mc_client_display_source_translate(DISPLAY, at_fmul(.375f, (float)(i * 2 - 1)), 0,
                                                    0, PD));
        PGL(226, mc_client_display_source_translate(DISPLAY, 0, -.375f, 0, PD));
        PGL(235, mc_client_display_source_rotate(DISPLAY, at_fneg(pitch), 1, 0, 0, PD));
        PGL(244, mc_client_display_source_rotate(DISPLAY, at_fneg(yaw), 0, 1, 0, PD));
        PGL(257, mc_client_display_source_scale(DISPLAY, at_widen(1.3333334f), at_widen(1.3333334f),
                                                at_widen(1.3333334f), PD));
        player_at(f, "a", "(Lgs;F)V", 266);
        if (!auxiliary_model(f, true))
            return false;
        PGL(269, mc_client_display_source_pop(DISPLAY, PD));
    }
    return true;
}

static bool cape_layer(ld_frame *f, float partial) {
    PGL(290, mc_client_display_source_push(DISPLAY, PD));
    PGL(297, mc_client_display_source_translate(DISPLAY, 0, 0, .125f, PD));
    player_at(f, "a", "(Lgs;F)V", 300);
    if (!ld_fp(f))
        return false;
    const mc_player_fields *p = player_fields(f);
    const mc_entity_base_fields *b = &f->actor->base;
    const double t = at_widen(partial);
    const double dx = at_sub(at_add(p->o, at_mul(at_sub(p->r, p->o), t)),
                             at_add(b->aJ, at_mul(at_sub(b->x, b->aJ), t)));
    const double dy = at_sub(at_add(p->p, at_mul(at_sub(p->s, p->p), t)),
                             at_add(b->aK, at_mul(at_sub(b->y, b->aK), t)));
    const double dz = at_sub(at_add(p->q, at_mul(at_sub(p->t, p->q), t)),
                             at_add(b->aL, at_mul(at_sub(b->z, b->aL), t)));
    const mc_living_entity_fields *ls = mc_entity_living_fields(f->actor);
    const float body = at_fadd(ls->I, at_fmul(at_fsub(ls->H, ls->I), partial));
    float sine, cosine;
    player_at(f, "a", "(Lgs;F)V", 437);
    if (!ld_float_math(f, "a", at_fdiv(at_fmul(body, 3.1415927f), 180), &sine))
        return false;
    player_at(f, "a", "(Lgs;F)V", 451);
    if (!ld_float_math(f, "b", at_fdiv(at_fmul(body, 3.1415927f), 180), &cosine))
        return false;
    const double sin_d = at_widen(sine), cos_d = at_widen(at_fneg(cosine));
    float lift = at_fmul(at_narrow(dy), 10);
    if (lift < -6)
        lift = -6;
    if (lift > 32)
        lift = 32;
    float forward = at_fmul(at_narrow(at_add(at_mul(dx, sin_d), at_mul(dz, cos_d))), 100);
    const float sideways = at_fmul(at_narrow(at_sub(at_mul(dx, cos_d), at_mul(dz, sin_d))), 100);
    if (forward < 0)
        forward = 0;
    const float camera = at_fadd(p->h, at_fmul(at_fsub(p->i, p->h), partial));
    float wave;
    player_at(f, "a", "(Lgs;F)V", 573);
    if (!ld_float_math(f, "a", at_fmul(at_fadd(b->bi, at_fmul(at_fsub(b->bj, b->bi), partial)), 6),
                       &wave))
        return false;
    lift = at_fadd(lift, at_fmul(at_fmul(wave, 32), camera));
    player_at(f, "a", "(Lgs;F)V", 586);
    bool sneaking;
    if (!ld_entity(f,
                   mc_entity_render_player_sneaking(f->actor, &sneaking, &f->error->detail.entity)))
        return false;
    if (sneaking)
        lift = at_fadd(lift, 25);
    PGL(612, mc_client_display_source_rotate(
                 DISPLAY, at_fadd(at_fadd(6, at_fdiv(forward, 2)), lift), 1, 0, 0, PD));
    PGL(622, mc_client_display_source_rotate(DISPLAY, at_fdiv(sideways, 2), 0, 0, 1, PD));
    PGL(633, mc_client_display_source_rotate(DISPLAY, at_fdiv(at_fneg(sideways), 2), 0, 1, 0, PD));
    PGL(641, mc_client_display_source_rotate(DISPLAY, 180, 0, 1, 0, PD));
    player_at(f, "a", "(Lgs;F)V", 650);
    if (!auxiliary_model(f, false))
        return false;
    PGL(653, mc_client_display_source_pop(DISPLAY, PD));
    return true;
}

bool ld_player_layers(ld_frame *f, float partial) {
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site parent = e->parent;
    e->parent = ld_site("gv", "a", "(Lls;DDDFF)V", 365);
    e->state.player.layers_entered = true;
    player_at(f, "a", "(Lgs;F)V", 1);
    mc_item_stack *helmet;
    if (!armor_stack(f, 3, 5, &helmet))
        return false;
    if (helmet != NULL) {
        player_at(f, "a", "(Lgs;F)V", 14);
        const mc_item *item;
        if (!stack_item(f, helmet, &item))
            return false;
        player_at(f, "a", "(Lgs;F)V", 17);
        if (item == NULL)
            return ld_null(f, "Player helmet null Item ID receiver");
        if (item->fields.bf < 256) {
            PGL(26, mc_client_display_source_push(DISPLAY, PD));
            if (!attachment(f, true, 33, 38))
                return false;
            bool three_d;
            if (!block_3d(f, helmet, true, &three_d))
                return false;
            if (three_d) {
                PGL(66, mc_client_display_source_translate(DISPLAY, 0, -.25f, 0, PD));
                PGL(74, mc_client_display_source_rotate(DISPLAY, 180, 0, 1, 0, PD));
                PGL(84, mc_client_display_source_scale(DISPLAY, .625, -.625, .625, PD));
            }
            if (!ld_held_item(f, helmet, 91, 96))
                return false;
            PGL(99, mc_client_display_source_pop(DISPLAY, PD));
        }
    }
    player_at(f, "a", "(Lgs;F)V", 108);
    mc_player_fields *p = player_fields(f);
    if (p == NULL)
        return false;
    const mc_player_string *name = p->l;
    if (name == NULL)
        return ld_null(f, "Player ears null name equals receiver");
    mc_player_string_view text;
    if (!mc_player_string_read(name, &text))
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Player actual name String");
    static const uint16_t special[] = {'d', 'e', 'a', 'd', 'm', 'a', 'u', '5'};
    if (text.length == 8 && memcmp(text.units, special, sizeof(special)) == 0 &&
        !ears_layer(f, partial))
        return false;
    player_at(f, "a", "(Lgs;F)V", 280);
    const mc_player_string *cape = p->n;
    if (!ld_capture(f, mc_client_draw_capture_string(f->scope, cape, &e->detail.capture)))
        return false;
    e->state.player.cape_attempted = true;
    bool textured;
    if (!ld_download(f, cape, NULL, ld_site("ds", "a", "(Lgs;F)V", 284), &textured))
        return false;
    e->state.player.cape_textured = textured;
    if (textured && !cape_layer(f, partial))
        return false;
    e->state.player.cape_returned = true;
    mc_item_stack *stack;
    if (!current_stack(f, 660, &stack))
        return false;
    if (stack != NULL && !held_item(f, stack))
        return false;
    player_at(f, "a", "(Lgs;F)V", 937);
    e->state.player.layers_returned = true;
    e->parent = parent;
    return true;
}

bool ld_player_name(ld_frame *f, double x, double y, double z) {
    (void)x;
    (void)y;
    (void)z;
    mc_client_living_draw_error *e = f->error;
    player_at(f, "a", "(Lgs;DDD)V", 0);
    bool enabled;
    if (!mc_client_draw_name_flag(f->scope, &enabled, &e->detail.capture)) {
        e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_CAPTURE;
        e->child = ld_site(e->detail.capture.source_class, e->detail.capture.source_method,
                           e->detail.capture.source_descriptor, e->detail.capture.pc);
        if (!e->detail.capture.native_policy &&
            e->detail.capture.status == MC_CLIENT_MANAGER_NULL_REFERENCE)
            return ld_null(f, e->detail.capture.operation);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, e->detail.capture.operation);
    }
    e->state.player.name_checked = true;
    e->state.player.name_enabled = enabled;
    if (enabled) {
        mc_client_render_manager *manager = ld_manager(f, 11);
        if (manager == NULL)
            return false;
        e->state.player.name_self = f->actor == manager->fields.h;
        if (!e->state.player.name_self) {
            player_at(f, "a", "(Lgs;DDD)V", 36);
            return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                           "Player actual nonself name distance and label");
        }
    }
    player_at(f, "a", "(Lgs;DDD)V", 379);
    e->state.player.name_returned = true;
    return true;
}
