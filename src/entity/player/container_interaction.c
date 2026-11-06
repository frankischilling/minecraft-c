#include "container_interaction.h"

#include "../../block/registry/registry_internal.h"
#include "../../inventory/internal.h"
#include "../../item/render/internal.h"
#include "../../item/stack/source_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/item/registry/runtime.h"
#include "player_internal.h"
#include "statistics_status_internal.h"

#include <stdlib.h>

struct mc_player_container_call {
    mc_player_container_state state;
    mc_player_container_error error;
    mc_container_interaction_state interaction_state;
    mc_container_interaction_call *interaction;
    mc_registry_classes *registry;
    mc_math_classes *math;
    mc_world_runtime *close_world;
    mc_player_drop_call **drops;
    size_t drop_count, drop_capacity;
    bool player_held;
};

static bool fail(mc_player_container_call *c, mc_entity_status status, const char *operation) {
    c->error.status = status;
    c->error.operation = operation;
    c->error.native_policy = !c->error.source_exception && !c->error.source_error;
    return false;
}

static bool null_reference(mc_player_container_call *c, const char *operation) {
    c->error.source_exception = true;
    c->error.exception_class = "java/lang/NullPointerException";
    return fail(c, MC_ENTITY_STATE_ERROR, operation);
}

static mc_inventory_status host_failure(mc_player_container_call *c,
                                        mc_container_interaction_error *e) {
    e->status = c->error.source_exception ? MC_INVENTORY_NULL_REFERENCE
                                          : MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY;
    e->operation = c->error.operation;
    e->native_policy = c->error.native_policy;
    e->source_exception = c->error.source_exception;
    e->source_error = c->error.source_error;
    e->exception_class = c->error.exception_class;
    e->dependency = (mc_inventory_site){c->error.child.class_name, c->error.child.method,
                                        c->error.child.descriptor, (int32_t)c->error.child.pc};
    return e->status;
}

static bool storage(mc_player_container_call *c, mc_entity *player) {
    return player->local_player != NULL ||
           fail(c, MC_ENTITY_STATE_ERROR, "Container actual LocalPlayer storage");
}

mc_entity_status mc_player_container_default_get(mc_entity *player, mc_container **out) {
    if (player == NULL || out == NULL || player->local_player == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = player->local_player->fields.player.d;
    return MC_ENTITY_OK;
}

mc_entity_status mc_player_container_active_get(mc_entity *player, mc_container **out) {
    if (player == NULL || out == NULL || player->local_player == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = player->local_player->fields.player.e;
    return MC_ENTITY_OK;
}

mc_entity_status mc_player_container_restore_default(mc_entity *player,
                                                     mc_player_container_error *out) {
    mc_player_container_error e = {.site = {"gs", "r", "()V", 2}};
    if (player == NULL) {
        e.status = MC_ENTITY_STATE_ERROR;
        e.operation = "Player close null actual default Container field receiver";
        e.source_exception = true;
        e.exception_class = "java/lang/NullPointerException";
    } else if (player->local_player == NULL) {
        e.status = MC_ENTITY_STATE_ERROR;
        e.operation = "Player close actual represented Player storage";
        e.native_policy = true;
    } else {
        mc_container *captured = player->local_player->fields.player.d;
        e.site.pc = 5;
        player->local_player->fields.player.e = captured;
        e.site.pc = 8;
    }
    if (out != NULL)
        *out = e;
    return e.status;
}

static mc_inventory_status inventory(void *context, mc_entity *player, mc_inventory_player **out,
                                     mc_container_interaction_error *e) {
    mc_player_container_call *c = context;
    if (player == NULL) {
        (void)null_reference(c, "Container null Player Inventory field receiver");
        return host_failure(c, e);
    }
    if (!storage(c, player))
        return host_failure(c, e);
    *out = player->local_player->fields.player.c;
    return MC_INVENTORY_OK;
}

static mc_inventory_status cursor_changed(void *context, mc_entity *player, mc_item_stack *stack,
                                          mc_container_interaction_error *e) {
    mc_player_container_call *c = context;
    (void)stack;
    if (player == NULL) {
        (void)null_reference(c, "Inventory cursor null GS.b invocation receiver");
        return host_failure(c, e);
    }
    if (!storage(c, player))
        return host_failure(c, e);
    /* Actual GS.b(IZ)V is literal return0; DC does not override it. */
    return MC_INVENTORY_OK;
}

static bool registry_failure(mc_player_container_call *c) {
    mc_item_render_throwable kind;
    mc_item_render_registry_classification(c->registry, &c->error.registry, &kind,
                                           &c->error.exception_class);
    c->error.source_exception = kind == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    c->error.source_error = kind == MC_ITEM_RENDER_SOURCE_ERROR;
    return fail(c, MC_ENTITY_STATE_ERROR, c->error.registry.message);
}

static mc_inventory_status world_remote(void *context, mc_world_runtime *world, bool *out,
                                        mc_container_interaction_error *e) {
    mc_player_container_call *c = context;
    c->error.child = (mc_player_site){"it", "a", "(Lgs;)V", 9};
    if (world == NULL) {
        (void)null_reference(c, "Workbench actual null World.B field receiver");
        return host_failure(c, e);
    }
    if (c->close_world == NULL) {
        if (!mc_world_runtime_binding_retain(world)) {
            (void)fail(c, MC_ENTITY_BUSY, "Workbench reached World occurrence");
            return host_failure(c, e);
        }
        c->close_world = world;
    }
    *out = world->fields.B;
    return MC_INVENTORY_OK;
}

static bool statistic(mc_player_container_call *c, mc_entity *player, const mc_statistic *value,
                      int32_t amount) {
    if (player == NULL)
        return null_reference(c, "Crafting null Player statistic invocation receiver");
    if (mc_local_player_add_statistic(player, value, amount, &c->error.statistic) ==
        MC_PLAYER_STATISTIC_OK)
        return true;
    const mc_player_statistic_error *s = &c->error.statistic;
    c->error.child = s->site;
    if (s->status == MC_PLAYER_STATISTIC_NULL_REFERENCE ||
        (s->status == MC_PLAYER_STATISTIC_WRITER_FAILED &&
         s->writer.status == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
         s->writer.exception == MC_CLIENT_STATS_NULL_REFERENCE)) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/NullPointerException";
    }
    return fail(c, mc_player_statistic_entity_status(s), s->operation);
}

static bool stack_field(mc_player_container_call *c, mc_item_stack *stack, bool count,
                        int32_t *out) {
    if (stack == NULL)
        return null_reference(c, "Crafting null Stack field receiver");
    const mc_item_stack_status s = count ? mc_item_stack_source_count_get(stack, out)
                                         : mc_item_stack_source_id_get(stack, out);
    return s == MC_ITEM_STACK_OK ||
           fail(c, MC_ENTITY_STATE_ERROR, "Crafting actual Stack source field storage");
}

static mc_inventory_status crafted(void *context, mc_entity *player, mc_item_stack *stack,
                                   mc_container_interaction_error *e) {
    mc_player_container_call *c = context;
    c->error.child = (mc_player_site){"yv", "a", "(Liz;)V", 5};
    if (player == NULL) {
        (void)null_reference(c, "Crafting pickup null stored Player World receiver");
        return host_failure(c, e);
    }
    if (!storage(c, player))
        return host_failure(c, e);
    mc_world_runtime *world = player->base.world;
    (void)world; /* Real GM.b bodies below do not read the supplied World. */
    c->error.child.pc = 12;
    if (stack == NULL) {
        (void)null_reference(c, "Crafting pickup null Stack crafted invocation receiver");
        return host_failure(c, e);
    }
    c->error.child = (mc_player_site){"iz", "b", "(Lfd;Lgs;)V", 1};
    if (c->registry == NULL) {
        (void)fail(c, MC_ENTITY_STATE_ERROR, "Crafting actual Registry namespace");
        return host_failure(c, e);
    }
    if (mc_registry_classes_initialize_statistics(c->registry, &c->error.registry) !=
        MC_REGISTRY_OK) {
        (void)registry_failure(c);
        return host_failure(c, e);
    }
    mc_statistic *const *captured = c->registry->statistics.D;
    int32_t id, amount;
    c->error.child.pc = 5;
    if (!stack_field(c, stack, false, &id))
        return host_failure(c, e);
    c->error.child.pc = 8;
    if (captured == NULL) {
        (void)null_reference(c, "Crafting null captured JL.D statistic array");
        return host_failure(c, e);
    }
    if (id < 0 || id >= 32000) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/ArrayIndexOutOfBoundsException";
        (void)fail(c, MC_ENTITY_STATE_ERROR, "Crafting statistic signed Stack id");
        return host_failure(c, e);
    }
    const mc_statistic *stat = captured[id];
    c->error.child.pc = 10;
    if (!stack_field(c, stack, true, &amount))
        return host_failure(c, e);
    c->error.child.pc = 13;
    if (!statistic(c, player, stat, amount))
        return host_failure(c, e);
    c->error.child = (mc_player_site){"iz", "b", "(Lfd;Lgs;)V", 16};
    const mc_registry_item_array *items = NULL;
    if (mc_registry_item_c_get(c->registry, &items, &c->error.registry) != MC_REGISTRY_OK) {
        (void)registry_failure(c);
        return host_failure(c, e);
    }
    c->error.child.pc = 20;
    if (!stack_field(c, stack, false, &id))
        return host_failure(c, e);
    const mc_item *item = NULL;
    c->error.child.pc = 23;
    if (mc_registry_item_c_at(c->registry, items, id, &item, &c->error.registry) !=
        MC_REGISTRY_OK) {
        (void)registry_failure(c);
        return host_failure(c, e);
    }
    c->error.child.pc = 27;
    if (item == NULL) {
        (void)null_reference(c, "Crafting null Item crafted invocation receiver");
        return host_failure(c, e);
    }
    if (item->kind == MC_ITEM_MAP) {
        c->error.child = (mc_player_site){"wr", "b", "(Liz;Lfd;Lgs;)V", 4};
        if (world == NULL)
            (void)null_reference(c, "Map crafted null World map-id lookup receiver");
        else
            (void)fail(c, MC_ENTITY_STATE_ERROR, "Map actual crafted World map-id lookup body");
        return host_failure(c, e);
    }

    /* Other37 actual represented classes inherit GM.b's literal return0. */
    static const struct {
        bool block;
        const char *name;
        unsigned int stack_pc, static_pc, field_pc, achievement_pc, statistic_pc;
        size_t achievement;
    } checks[] = {
        {true, "az", 16, 19, 22, 32, 36, 2},      {false, "r", 43, 46, 49, 59, 63, 3},
        {true, "aC", 70, 73, 76, 86, 90, 4},      {false, "L", 97, 100, 103, 113, 117, 6},
        {false, "S", 124, 127, 130, 140, 144, 7}, {false, "aX", 151, 154, 157, 167, 171, 8},
        {false, "v", 178, 181, 184, 194, 198, 9}, {false, "p", 205, 208, 211, 221, 225, 12}};

    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); ++i) {
        c->error.child = (mc_player_site){"yv", "a", "(Liz;)V", checks[i].stack_pc};
        if (!stack_field(c, stack, false, &id))
            return host_failure(c, e);
        int32_t compared;
        c->error.child.pc = checks[i].static_pc;
        if (checks[i].block) {
            const mc_block *block = NULL;
            if (mc_registry_block_static_get(c->registry, checks[i].name, &block,
                                             &c->error.registry) != MC_REGISTRY_OK) {
                (void)registry_failure(c);
                return host_failure(c, e);
            }
            c->error.child.pc = checks[i].field_pc;
            if (block == NULL) {
                (void)null_reference(c, "Crafting null compared static Block id receiver");
                return host_failure(c, e);
            }
            compared = block->fields.bn;
        } else {
            const mc_item *compared_item = NULL;
            if (mc_registry_item_static_get(c->registry, checks[i].name, &compared_item,
                                            &c->error.registry) != MC_REGISTRY_OK) {
                (void)registry_failure(c);
                return host_failure(c, e);
            }
            c->error.child.pc = checks[i].field_pc;
            if (compared_item == NULL) {
                (void)null_reference(c, "Crafting null compared static Item id receiver");
                return host_failure(c, e);
            }
            compared = compared_item->fields.bf;
        }
        if (id == compared) {
            c->error.child.pc = checks[i].achievement_pc;
            if (mc_registry_classes_initialize_achievements(c->registry, &c->error.registry) !=
                MC_REGISTRY_OK) {
                (void)registry_failure(c);
                return host_failure(c, e);
            }
            const mc_statistic *a = c->registry->achievements.named[checks[i].achievement];
            c->error.child.pc = checks[i].statistic_pc;
            if (!statistic(c, player, a, 1))
                return host_failure(c, e);
            break;
        }
    }
    return MC_INVENTORY_OK;
}

static mc_inventory_status drop(void *context, mc_entity *player, mc_item_stack *stack,
                                mc_container_interaction_error *e) {
    mc_player_container_call *c = context;
    if (c->drop_count == c->drop_capacity) {
        const size_t capacity = c->drop_capacity == 0 ? 8 : c->drop_capacity * 2;
        if (capacity < c->drop_capacity || capacity > SIZE_MAX / sizeof(*c->drops)) {
            (void)fail(c, MC_ENTITY_LIMIT, "Container reached drop owner ledger bound");
            return host_failure(c, e);
        }
        void *allocation = realloc(c->drops, capacity * sizeof(*c->drops));
        if (allocation == NULL) {
            (void)fail(c, MC_ENTITY_ALLOCATION_FAILED, "Container reached drop owner ledger");
            return host_failure(c, e);
        }
        c->drops = allocation;
        c->drop_capacity = capacity;
    }
    mc_player_drop_call *call = NULL;
    const mc_player_drop_status s = mc_local_player_drop_stack_begin(
        c->registry, c->math, player, stack, false, &call, &c->error.drop);
    if (call != NULL)
        c->drops[c->drop_count++] = call;
    c->state.drop_calls = c->drop_count;
    if (s == MC_PLAYER_DROP_OK)
        return MC_INVENTORY_OK;
    c->error.child = c->error.drop.site;
    c->error.source_exception = c->error.drop.source_exception;
    c->error.source_error = c->error.drop.source_error;
    c->error.exception_class = c->error.drop.exception_class;
    (void)fail(c,
               s == MC_PLAYER_DROP_BUSY         ? MC_ENTITY_BUSY
               : s == MC_PLAYER_DROP_LIMIT      ? MC_ENTITY_LIMIT
               : s == MC_PLAYER_DROP_ALLOCATION ? MC_ENTITY_ALLOCATION_FAILED
                                                : MC_ENTITY_STATE_ERROR,
               c->error.drop.operation);
    return host_failure(c, e);
}

static void entered(mc_player_container_call *c, int32_t slot, int32_t button, bool shift,
                    bool close, bool direct) {
    mc_entity *player = c->state.player;
    const mc_container_interaction_host host = {.context = c,
                                                .inventory = inventory,
                                                .cursor_changed = cursor_changed,
                                                .drop = drop,
                                                .crafted = crafted,
                                                .world_remote = world_remote};
    const mc_inventory_status s =
        close ? mc_container_close_begin(c->state.container, player, &host, &c->interaction,
                                         &c->error.interaction)
              : mc_container_click_begin(c->state.container, player, slot, button, shift, &host,
                                         &c->interaction, &c->error.interaction);
    (void)mc_container_interaction_inspect(c->interaction, &c->interaction_state);
    c->state.result = c->interaction_state.result;
    const mc_container_interaction_error *e = &c->error.interaction;
    if (direct)
        c->error.site = (mc_player_site){e->site.class_name, e->site.method, e->site.descriptor,
                                         (unsigned int)e->site.pc};
    if (s != MC_INVENTORY_OK && c->error.status == MC_ENTITY_OK) {
        c->error.source_exception = e->source_exception;
        c->error.source_error = e->source_error;
        c->error.exception_class = e->exception_class;
        c->error.child = (mc_player_site){e->site.class_name, e->site.method, e->site.descriptor,
                                          (unsigned int)e->site.pc};
        (void)fail(c,
                   s == MC_INVENTORY_BUSY                ? MC_ENTITY_BUSY
                   : s == MC_INVENTORY_LIMIT             ? MC_ENTITY_LIMIT
                   : s == MC_INVENTORY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                                                         : MC_ENTITY_STATE_ERROR,
                   e->operation);
    }
    if (s == MC_INVENTORY_OK) {
        if (!direct) {
            if (close) {
                c->error.site.pc = 13;
                player->local_player->fields.player.e = player->local_player->fields.player.d;
                c->state.active_container_restored = true;
            }
            c->error.site.pc = close ? 16 : 14;
        }
        c->state.returned = true;
    }
}

static mc_entity_status begin(mc_registry_classes *registry, mc_math_classes *math,
                              mc_entity *player, int32_t window, int32_t slot, int32_t button,
                              bool shift, bool close, mc_container *direct_container, bool direct,
                              mc_player_container_call **out, mc_player_container_error *error) {
    if (out == NULL || *out != NULL) {
        if (error != NULL)
            *error = (mc_player_container_error){.status = MC_ENTITY_INVALID_ARGUMENT,
                                                 .operation = "Player Container fresh output",
                                                 .native_policy = true};
        return MC_ENTITY_INVALID_ARGUMENT;
    }
    mc_player_container_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        if (error != NULL)
            *error = (mc_player_container_error){.status = MC_ENTITY_ALLOCATION_FAILED,
                                                 .operation = "Player Container call allocation",
                                                 .native_policy = true};
        return MC_ENTITY_ALLOCATION_FAILED;
    }
    *out = c;
    c->registry = registry;
    c->math = math;
    c->state = (mc_player_container_state){.player = player,
                                           .window_id = window,
                                           .close = close,
                                           .direct_target = direct,
                                           .active = true,
                                           .error = &c->error,
                                           .interaction = &c->interaction_state};
    if (direct) {
        c->state.container = direct_container;
        if (direct_container == NULL)
            (void)null_reference(c, "Direct close null captured Container invocation receiver");
        else {
            c->error.status = player == NULL ? MC_ENTITY_OK : mc_entity_retain(player);
            if (c->error.status != MC_ENTITY_OK)
                (void)fail(c, c->error.status, "Direct close native Player occurrence");
            else {
                c->player_held = player != NULL;
                entered(c, 0, 0, false, true, true);
            }
        }
    } else {
        c->error.site =
            (mc_player_site){"ob", "a", close ? "(ILgs;)V" : "(IIIZLgs;)Liz;", close ? 1 : 2};
        if (player == NULL)
            (void)null_reference(c, "Controller null Player active Container field receiver");
        else if (storage(c, player)) {
            c->state.container = player->local_player->fields.player.e;
            c->error.status = mc_entity_retain(player);
            if (c->error.status != MC_ENTITY_OK)
                (void)fail(c, c->error.status, "Controller native Player occurrence");
            else {
                c->player_held = true;
                c->error.site.pc = close ? 5 : 11;
                if (c->state.container == NULL)
                    (void)null_reference(c,
                                         "Controller null current Container invocation receiver");
                else
                    entered(c, slot, button, shift, close, false);
            }
        }
    }
    c->state.active = false;
    if (error != NULL)
        *error = c->error;
    return c->error.status;
}

mc_entity_status mc_player_container_click_begin(mc_registry_classes *r, mc_math_classes *m,
                                                 mc_entity *p, int32_t window, int32_t slot,
                                                 int32_t button, bool shift,
                                                 mc_player_container_call **out,
                                                 mc_player_container_error *error) {
    return begin(r, m, p, window, slot, button, shift, false, NULL, false, out, error);
}

mc_entity_status mc_player_container_close_begin(mc_registry_classes *r, mc_math_classes *m,
                                                 mc_entity *p, int32_t window,
                                                 mc_player_container_call **out,
                                                 mc_player_container_error *error) {
    return begin(r, m, p, window, 0, 0, false, true, NULL, false, out, error);
}

mc_entity_status mc_player_container_direct_close_begin(mc_registry_classes *r, mc_math_classes *m,
                                                        mc_entity *p, mc_container *container,
                                                        mc_player_container_call **out,
                                                        mc_player_container_error *error) {
    return begin(r, m, p, 0, 0, 0, false, true, container, true, out, error);
}

bool mc_player_container_inspect(const mc_player_container_call *c,
                                 mc_player_container_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

bool mc_player_container_drop_inspect(const mc_player_container_call *c, size_t index,
                                      mc_player_drop_state *out) {
    return c != NULL && index < c->drop_count && mc_local_player_drop_inspect(c->drops[index], out);
}

mc_entity_status mc_player_container_take_result(mc_player_container_call *c, mc_item_stack **out) {
    if (c == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    const mc_inventory_status s = mc_container_interaction_take_result(c->interaction, out);
    if (s != MC_INVENTORY_OK)
        return s == MC_INVENTORY_BUSY ? MC_ENTITY_BUSY : MC_ENTITY_INVALID_ARGUMENT;
    c->state.result = NULL;
    (void)mc_container_interaction_inspect(c->interaction, &c->interaction_state);
    return MC_ENTITY_OK;
}

mc_entity_status mc_player_container_retire(mc_player_container_call **out,
                                            mc_player_container_error *error) {
    if (out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_player_container_call *c = *out;
    if (c == NULL) {
        if (error != NULL)
            *error = (mc_player_container_error){0};
        return MC_ENTITY_OK;
    }
    if (c->state.active) {
        if (error != NULL)
            *error = (mc_player_container_error){.status = MC_ENTITY_BUSY,
                                                 .operation = "Player Container active retirement",
                                                 .native_policy = true};
        return MC_ENTITY_BUSY;
    }
    for (size_t i = 0; i < c->drop_count; ++i) {
        mc_player_drop_state before = {0};
        (void)mc_local_player_drop_inspect(c->drops[i], &before);
        const bool publication_expires =
            before.error != NULL && c->error.drop.publication == before.error->publication;
        const bool statistic_expires =
            before.error != NULL && c->error.drop.statistic == before.error->statistic;
        mc_player_drop_error cleanup;
        if (mc_local_player_drop_retire(&c->drops[i], &cleanup) != MC_PLAYER_DROP_OK) {
            if (error != NULL) {
                *error = c->error;
                error->drop = cleanup;
            }
            return MC_ENTITY_BUSY;
        }
        if (publication_expires)
            c->error.drop.publication = NULL;
        if (statistic_expires)
            c->error.drop.statistic = NULL;
    }
    mc_container_interaction_error cleanup;
    if (mc_container_interaction_retire(&c->interaction, &cleanup) != MC_INVENTORY_OK) {
        if (error != NULL) {
            *error = c->error;
            error->interaction = cleanup;
            error->drop.publication = NULL;
            error->drop.statistic = NULL;
        }
        return MC_ENTITY_BUSY;
    }
    mc_world_runtime_binding_release(c->close_world);
    if (c->player_held)
        mc_entity_release(c->state.player);
    free(c->drops);
    free(c);
    *out = NULL;
    if (error != NULL)
        *error = (mc_player_container_error){0};
    return MC_ENTITY_OK;
}
