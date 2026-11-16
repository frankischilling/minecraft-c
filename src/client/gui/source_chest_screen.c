#include "source_chest_screen_internal.h"

#include "entity/entity_internal.h"
#include "item/render/internal.h"
#include "mc/core/java/java.h"
#include "world/runtime/runtime_internal.h"

#include <stdlib.h>

static bool fail(mc_client_source_id *o, mc_client_screen_error *e, mc_client_screen_status status,
                 const char *operation, unsigned int pc, bool native) {
    mc_screen_fail(e, status, operation, "hp", "<init>", "(Llw;Llw;)V", pc);
    e->native_policy = native;
    if (o != NULL) {
        e->inventory = &o->detail;
        o->last_pc = pc;
        o->base.failure = *e;
    }
    return false;
}

mc_client_screen_status mc_client_screen_chest_allocate(mc_client_screen_classes *classes,
                                                        mc_client_render_classes *models,
                                                        const mc_client_screen_options *options,
                                                        mc_client_source_hp **out,
                                                        mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL || *out != NULL)
        (void)fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "HP fresh native output", 0, true);
    else if (mc_screen_lock(classes, &e)) {
        if (mc_screen_inventory_active_locked(classes, models, &e)) {
            if (classes->holds == SIZE_MAX ||
                (options != NULL && options->maximum_bytes < sizeof(mc_client_source_hp)))
                (void)fail(NULL, &e, MC_CLIENT_SCREEN_LIMIT, "HP native owner budget", 0, true);
            else {
                mc_client_source_hp *screen = calloc(1, sizeof(*screen));
                if (screen == NULL)
                    (void)fail(NULL, &e, MC_CLIENT_SCREEN_ALLOCATION_FAILED,
                               "HP RAW owner allocation", 0, true);
                else {
                    mc_client_source_id *o = &screen->container;
                    o->base.classes = classes;
                    o->base.holds = 1;
                    o->base.kind = MC_CLIENT_SCREEN_CHEST;
                    mc_screen_memory_init(&o->base.memory, options, sizeof(*screen));
                    ++classes->holds;
                    *out = screen;
                }
            }
        }
        mtx_unlock(&classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static bool container_failure(mc_client_source_id *o, mc_client_screen_error *e,
                              mc_inventory_status status, unsigned int pc) {
    const mc_inventory_error *v = &o->detail.ownership;
    mc_client_screen_status mapped =
        status == MC_INVENTORY_LIMIT                 ? MC_CLIENT_SCREEN_LIMIT
        : status == MC_INVENTORY_ALLOCATION_FAILED   ? MC_CLIENT_SCREEN_ALLOCATION_FAILED
        : status == MC_INVENTORY_NULL_REFERENCE      ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
        : status == MC_INVENTORY_INDEX_OUT_OF_BOUNDS ? MC_CLIENT_SCREEN_SOURCE_INDEX
                                                     : MC_CLIENT_SCREEN_APP_FAILED;
    (void)fail(o, e, mapped, v->operation, pc, true);
    if (v->frame_count != 0) {
        o->detail.child = (mc_client_screen_site){
            v->frames[0].class_name, v->frames[0].method, v->frames[0].descriptor,
            v->frames[0].pc < 0 ? 0u : (unsigned int)v->frames[0].pc};
        if (status == MC_INVENTORY_NULL_REFERENCE || status == MC_INVENTORY_INDEX_OUT_OF_BOUNDS) {
            e->native_policy = false;
            e->source_exception = true;
            e->exception_class = status == MC_INVENTORY_NULL_REFERENCE
                                     ? "java/lang/NullPointerException"
                                     : "java/lang/ArrayIndexOutOfBoundsException";
        }
    }
    if (v->source_exception) {
        e->native_policy = false;
        e->source_exception = true;
        e->exception_class = v->exception_class;
    }
    if (status == MC_INVENTORY_REGISTRY_FAILED) {
        mc_item_render_throwable kind;
        mc_item_render_registry_classification(o->base.classes->registry, &v->registry, &kind,
                                               &e->exception_class);
        e->source_exception = kind == MC_ITEM_RENDER_SOURCE_EXCEPTION;
        e->source_error = kind == MC_ITEM_RENDER_SOURCE_ERROR;
        e->native_policy = kind == MC_ITEM_RENDER_NO_THROWABLE;
        e->registry = v->registry;
    }
    o->base.failure = *e;
    return false;
}

static void store(mc_client_source_id *o, unsigned int pc) {
    if (o->base.store_count < sizeof(o->base.stores) / sizeof(o->base.stores[0]))
        o->base.stores[o->base.store_count++] =
            (mc_client_screen_site){"hp", "<init>", "(Llw;Llw;)V", pc};
    o->last_pc = pc;
}

static bool construct(mc_client_source_hp *screen, mc_inventory_player *inventory,
                      mc_inventory_chest *chest, mc_client_screen_error *e) {
    mc_client_source_id *o = &screen->container;
    /* Preserve the two actual nullable arguments independently. These native
     * occurrences do not invoke LW or replace any HP/CD source field. */
    o->constructor_inventory = inventory;
    o->constructor_chest = chest;
    if (chest != NULL) {
        if (mc_inventory_chest_retain(chest, &o->detail.ownership) != MC_INVENTORY_OK)
            return container_failure(o, e, o->detail.ownership.status, 0);
        o->chest_held = true;
    }
    if (inventory != NULL) {
        mc_inventory_player_state state;
        if (!mc_inventory_player_read_state(inventory, &state))
            return fail(o, e, MC_CLIENT_SCREEN_INVALID_ARGUMENT,
                        "HP actual Inventory native storage", 0, true);
        if (state.player != NULL) {
            o->detail.entity = mc_entity_retain(state.player);
            if (o->detail.entity != MC_ENTITY_OK)
                return fail(o, e, MC_CLIENT_SCREEN_BUSY, "HP Inventory owner Player occurrence", 0,
                            true);
            o->constructor_player = state.player;
            o->player_held = true;
            /* This native World occurrence protects the retained constructor
             * Player graph; HP has no source World field or World invocation. */
            o->constructor_world = state.player->base.world;
            if (o->constructor_world != NULL) {
                if (!mc_world_runtime_binding_retain(o->constructor_world))
                    return fail(o, e, MC_CLIENT_SCREEN_BUSY,
                                "HP constructor Player World native occurrence", 0, true);
                o->world_held = true;
            }
        }
    }
    mc_inventory_status status =
        mc_container_chest_allocate(MC_REGISTRY_CLIENT, o->base.classes->registry, NULL,
                                    &o->captured_container, &o->detail.ownership);
    if (status != MC_INVENTORY_OK)
        return container_failure(o, e, status, 1);
    o->container_owned = true;
    status =
        mc_container_chest_construct(o->captured_container, inventory, chest, &o->detail.ownership);
    if (status != MC_INVENTORY_OK)
        return container_failure(o, e, status, 7);
    o->detail.caller = (mc_client_screen_site){"hp", "<init>", "(Llw;Llw;)V", 10};
    if (!mc_screen_container_construct_locked(o, o->captured_container, e))
        return false;
    screen->fields.n = 0;
    store(o, 15);
    screen->fields.l = inventory;
    store(o, 20);
    screen->fields.m = chest;
    store(o, 25);
    o->base.fields.f = false;
    store(o, 30);
    int32_t rows;
    status = mc_inventory_chest_size(chest, &rows, &o->detail.ownership);
    if (status != MC_INVENTORY_OK)
        return container_failure(o, e, status, 45);
    screen->fields.n = rows / 9;
    store(o, 53);
    o->id.i = mc_java_i32_add(114, mc_java_i32_mul(screen->fields.n, 18));
    store(o, 67);
    o->last_pc = 70;
    e->source_returned = true;
    e->site = (mc_client_screen_site){"hp", "<init>", "(Llw;Llw;)V", 70};
    return true;
}

mc_client_screen_status mc_client_screen_chest_construct(mc_client_source_hp *screen,
                                                         mc_inventory_player *inventory,
                                                         mc_inventory_chest *chest,
                                                         mc_client_screen_error *error) {
    mc_client_source_id *o = screen == NULL ? NULL : &screen->container;
    mc_client_screen_error e = {0};
    if (o == NULL)
        (void)fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "HP constructor native receiver", 0,
                   true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        if (o->base.phase != MC_CLIENT_SCREEN_RAW || o->base.kind != MC_CLIENT_SCREEN_CHEST ||
            o->base.classes->retiring)
            (void)fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_PHASE, "HP one-shot constructor", 0,
                       true);
        else {
            o->base.phase = MC_CLIENT_SCREEN_CONSTRUCTING;
            const bool returned = construct(screen, inventory, chest, &e);
            o->base.phase = returned ? MC_CLIENT_SCREEN_READY : MC_CLIENT_SCREEN_FAILED;
            if (!returned) {
                e.inventory = &o->detail;
                o->base.failure = e;
            }
        }
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_source_da *mc_client_screen_chest_base(mc_client_source_hp *screen) {
    return screen == NULL ? NULL : &screen->container.base;
}

mc_client_screen_status mc_client_screen_chest_inspect(mc_client_source_hp *screen,
                                                       mc_client_chest_screen_state *out,
                                                       mc_client_screen_error *error) {
    mc_client_source_id *o = screen == NULL ? NULL : &screen->container;
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL)
        (void)fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "HP inspection native inputs", 0,
                   true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        *out = (mc_client_chest_screen_state){.phase = o->base.phase,
                                              .holds = o->base.holds,
                                              .source_calls = o->source_calls,
                                              .id = o->id,
                                              .constructor_inventory = o->constructor_inventory,
                                              .hp = screen->fields,
                                              .constructor_chest = o->constructor_chest,
                                              .chest_held = o->chest_held,
                                              .constructor_container = o->captured_container,
                                              .container_owned = o->container_owned,
                                              .inherited_closed_returned =
                                                  o->inherited_closed_returned,
                                              .last_pc = o->last_pc,
                                              .failure = &o->base.failure,
                                              .detail = &o->detail};
        if (o->base.phase == MC_CLIENT_SCREEN_RETIRING) {
            out->id.j = NULL;
            out->hp.l = NULL;
            out->hp.m = NULL;
        }
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static mc_client_screen_status release(mc_client_source_hp **slot, bool strict,
                                       mc_client_screen_error *error) {
    if (slot == NULL)
        return strict ? mc_client_screen_retire(NULL, error)
                      : mc_client_screen_release(NULL, error);
    mc_client_source_da *base = mc_client_screen_chest_base(*slot);
    const mc_client_screen_status status =
        strict ? mc_client_screen_retire(&base, error) : mc_client_screen_release(&base, error);
    if (status == MC_CLIENT_SCREEN_OK)
        *slot = NULL;
    return status;
}

mc_client_screen_status mc_client_screen_chest_release(mc_client_source_hp **slot,
                                                       mc_client_screen_error *error) {
    return release(slot, false, error);
}

mc_client_screen_status mc_client_screen_chest_retire(mc_client_source_hp **slot,
                                                      mc_client_screen_error *error) {
    return release(slot, true, error);
}
