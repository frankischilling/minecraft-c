#include "source_inventory_screen_internal.h"

#include "../../entity/living/fields_internal.h"
#include "../../entity/player/player_internal.h"
#include "../application_state.h"
#include "../render/model_graph_internal.h"
#include "mc/core/java/java.h"
#include "mc/statistics/achievements/achievements.h"

#include <stdlib.h>
#include <string.h>

static bool fail(mc_client_source_ue *o, mc_client_screen_error *e, mc_client_screen_status status,
                 const char *operation, const char *owner, const char *method,
                 const char *descriptor, unsigned int pc, bool native) {
    mc_screen_fail(e, status, operation, owner, method, descriptor, pc);
    e->native_policy = native;
    if (o != NULL) {
        e->inventory = &o->detail;
        o->last_pc = pc;
        o->base.failure = *e;
    }
    return false;
}

static void store(mc_client_source_ue *o, const char *owner, const char *descriptor,
                  unsigned int pc) {
    if (o->base.store_count < sizeof(o->base.stores) / sizeof(o->base.stores[0]))
        o->base.stores[o->base.store_count++] =
            (mc_client_screen_site){owner, "<init>", descriptor, pc};
    o->last_pc = pc;
}

mc_client_screen_status mc_client_screen_inventory_allocate(mc_client_screen_classes *c,
                                                            mc_client_render_classes *models,
                                                            const mc_client_screen_options *options,
                                                            mc_client_source_ue **out,
                                                            mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "UE native output", NULL, NULL, NULL, 0,
             true);
    else if (mc_screen_lock(c, &e)) {
        if (mc_screen_inventory_active_locked(c, models, &e)) {
            if (c->holds == SIZE_MAX ||
                (options != NULL && options->maximum_bytes < sizeof(mc_client_source_ue)))
                fail(NULL, &e, MC_CLIENT_SCREEN_LIMIT, "UE native owner budget", NULL, NULL, NULL,
                     0, true);
            else {
                mc_client_source_ue *o = calloc(1, sizeof(*o));
                if (o == NULL)
                    fail(NULL, &e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "UE RAW owner allocation",
                         NULL, NULL, NULL, 0, true);
                else {
                    o->base.classes = c;
                    o->base.holds = 1;
                    o->base.kind = MC_CLIENT_SCREEN_INVENTORY;
                    mc_screen_memory_init(&o->base.memory, options, sizeof(*o));
                    ++c->holds;
                    *out = o;
                }
            }
        }
        mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

static bool construct(mc_client_source_ue *o, mc_entity *player, mc_client_screen_error *e) {
    if (player == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "UE null Player.d receiver", "ue",
                    "<init>", "(Lgs;)V", 2, false);
    if (o->base.classes->inventory_models == NULL ||
        player->classes != o->base.classes->inventory_models->entities)
        return fail(o, e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "UE actual Player/model namespace",
                    NULL, NULL, NULL, 0, true);
    o->detail.entity = mc_entity_retain(player);
    if (o->detail.entity != MC_ENTITY_OK)
        return fail(o, e, MC_CLIENT_SCREEN_BUSY, "UE native Player occurrence", NULL, NULL, NULL, 0,
                    true);
    o->constructor_player = player;
    o->player_held = true;
    o->detail.entity = mc_player_container_default_get(player, &o->captured_container);
    if (o->detail.entity != MC_ENTITY_OK)
        return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "UE actual Player storage", "ue", "<init>",
                    "(Lgs;)V", 2, true);
    if (o->captured_container != NULL) {
        if (mc_container_retain(o->captured_container, &o->detail.ownership) != MC_INVENTORY_OK)
            return fail(o, e, MC_CLIENT_SCREEN_BUSY, "UE native captured Container occurrence",
                        NULL, NULL, NULL, 0, true);
        o->container_held = true;
    }
    if (!mc_screen_base_construct_locked(&o->base, e)) {
        e->parent_site = (mc_client_screen_site){"id", "<init>", "(Ldw;)V", 1};
        return false;
    }
    o->id.a = 176;
    store(o, "id", "(Ldw;)V", 8);
    o->id.i = 166;
    store(o, "id", "(Ldw;)V", 15);
    o->id.j = o->captured_container;
    store(o, "id", "(Ldw;)V", 20);
    o->base.fields.f = true;
    store(o, "ue", "(Lgs;)V", 10);
    mc_registry_classes *registry = o->base.classes->registry;
    if (registry == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "UE actual EP namespace", "ue",
                    "<init>", "(Lgs;)V", 14, true);
    if (mc_registry_classes_initialize_achievements(registry, &e->registry) != MC_REGISTRY_OK)
        return fail(o, e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "UE actual EP.f active use", "ue",
                    "<init>", "(Lgs;)V", 14, true);
    mc_registry_achievement_view achievements;
    if (mc_registry_classes_inspect_achievements(registry, &achievements, &e->registry) !=
            MC_REGISTRY_OK ||
        achievements.named == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "UE actual EP.f field", "ue", "<init>",
                    "(Lgs;)V", 14, true);
    if (mc_local_player_add_statistic(player, achievements.named[0], 1, &o->detail.statistic) !=
        MC_PLAYER_STATISTIC_OK) {
        o->detail.child = (mc_client_screen_site){
            o->detail.statistic.site.class_name, o->detail.statistic.site.method,
            o->detail.statistic.site.descriptor, o->detail.statistic.site.pc};
        return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "UE actual Player open-Inventory statistic",
                    "ue", "<init>", "(Lgs;)V", 18,
                    o->detail.statistic.status != MC_PLAYER_STATISTIC_NULL_REFERENCE);
    }
    o->last_pc = 21;
    e->source_returned = true;
    e->site = (mc_client_screen_site){"ue", "<init>", "(Lgs;)V", 21};
    return true;
}

mc_client_screen_status mc_client_screen_inventory_construct(mc_client_source_ue *o,
                                                             mc_entity *player,
                                                             mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "UE native constructor receiver", NULL,
             NULL, NULL, 0, true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        if (o->base.phase != MC_CLIENT_SCREEN_RAW || o->base.kind != MC_CLIENT_SCREEN_INVENTORY ||
            o->base.classes->retiring)
            fail(o, &e, MC_CLIENT_SCREEN_INVALID_PHASE, "UE one-shot constructor", NULL, NULL, NULL,
                 0, true);
        else {
            o->base.phase = MC_CLIENT_SCREEN_CONSTRUCTING;
            const bool returned = construct(o, player, &e);
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

bool mc_screen_inventory_initialize_locked(mc_client_source_ue *o, mc_client_screen_error *e) {
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    if (list == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "UE List.clear null receiver",
                    "ue", "b", "()V", 4, false);
    for (size_t i = 0; i < list->size; ++i) {
        mc_client_source_ke *button = list->entries[i];
        if (button != NULL && (button->classes != o->base.classes || button->holds < 2))
            return fail(o, e, MC_CLIENT_SCREEN_INVALID_PHASE, "UE native List occurrence invariant",
                        "ue", "b", "()V", 4, true);
    }
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    const size_t size = list->size;
    list->size = 0;
    for (size_t i = 0; i < size; ++i) {
        mc_client_source_ke *button = list->entries[i];
        list->entries[i] = NULL;
        if (!mc_screen_button_release_locked(&button, e))
            return false;
    }
    o->last_pc = 9;
    e->source_returned = true;
    return true;
}

static bool cleanup_failure(mc_client_source_ue *o, mc_client_screen_error *e,
                            const char *operation) {
    mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, operation, NULL, NULL, NULL, 0);
    e->inventory = &o->detail;
    return false;
}

static bool retire_call(mc_client_source_ue *o, mc_client_screen_error *e) {
    mc_inventory_screen_call *call = o->calls;
    if (call != NULL) {
        const mc_entity_status status = mc_player_container_retire(&call->call, &call->cleanup);
        /* A checked adapter retirement may discharge earlier children before a
         * later child refuses. Refresh only these borrowed diagnostic views;
         * retain the source exception/site and copied scalar evidence. */
        mc_player_container_state retained = {0};
        const mc_player_drop_error *drop =
            mc_player_container_inspect(call->call, &retained) && retained.error != NULL
                ? &retained.error->drop
                : NULL;
        call->failure.drop.publication = drop == NULL ? NULL : drop->publication;
        call->failure.drop.statistic = drop == NULL ? NULL : drop->statistic;
        o->detail.container.drop.publication = call->failure.drop.publication;
        o->detail.container.drop.statistic = call->failure.drop.statistic;
        o->detail.cleanup_container = call->cleanup;
        if (status != MC_ENTITY_OK)
            return cleanup_failure(o, e, "Inventory source child native retirement");
        if (call->controller != NULL) {
            if (mc_client_singleplayer_controller_release(call->controller) !=
                MC_SINGLEPLAYER_CONTROLLER_OK)
                return cleanup_failure(o, e, "Inventory captured controller native retirement");
            call->controller = NULL;
        }
    }
    return true;
}

bool mc_screen_inventory_cleanup_locked(mc_client_source_ue *o, mc_client_screen_error *e) {
    if (!retire_call(o, e))
        return false;
    if (o->container_held) {
        if (mc_container_release(o->captured_container, &o->detail.cleanup_ownership) !=
            MC_INVENTORY_OK)
            return cleanup_failure(o, e, "UE captured Container native release");
        o->container_held = false;
        o->captured_container = NULL;
    }
    if (o->player_held) {
        mc_entity_release(o->constructor_player);
        o->constructor_player = NULL;
        o->player_held = false;
    }
    return true;
}

bool mc_screen_inventory_terminal_prepare_locked(mc_client_source_ue *o,
                                                 mc_client_screen_error *e) {
    if (o->base.active_calls != 0 || o->base.phase == MC_CLIENT_SCREEN_CONSTRUCTING ||
        o->base.holds != 1)
        return cleanup_failure(o, e, "Inventory terminal active/external Screen occurrence");
    /* Native one-way latch: direct Screen entry must stop before the constructor
     * occurrences end. No source field, close callback or failure is changed. */
    o->base.phase = MC_CLIENT_SCREEN_RETIRING;
    return mc_screen_inventory_cleanup_locked(o, e);
}

mc_client_source_da *mc_client_screen_inventory_base(mc_client_source_ue *o) {
    return o == NULL ? NULL : &o->base;
}

mc_client_screen_status mc_client_screen_inventory_inspect(mc_client_source_ue *o,
                                                           mc_client_inventory_screen_state *out,
                                                           mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "UE native inspection", NULL, NULL, NULL,
             0, true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        *out = (mc_client_inventory_screen_state){.phase = o->base.phase,
                                                  .holds = o->base.holds,
                                                  .source_calls = o->source_calls,
                                                  .id = o->id,
                                                  .ue = o->fields,
                                                  .constructor_player = o->constructor_player,
                                                  .constructor_container = o->captured_container,
                                                  .container_held = o->container_held,
                                                  .player_held = o->player_held,
                                                  .last_pc = o->last_pc,
                                                  .failure = &o->base.failure,
                                                  .detail = &o->detail};
        if (o->base.phase == MC_CLIENT_SCREEN_RETIRING)
            out->id.j = NULL;
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static mc_client_screen_status release(mc_client_source_ue **slot, bool strict,
                                       mc_client_screen_error *e) {
    if (slot == NULL)
        return strict ? mc_client_screen_retire(NULL, e) : mc_client_screen_release(NULL, e);
    mc_client_source_da *base = *slot == NULL ? NULL : &(*slot)->base;
    const mc_client_screen_status status =
        strict ? mc_client_screen_retire(&base, e) : mc_client_screen_release(&base, e);
    if (status == MC_CLIENT_SCREEN_OK)
        *slot = NULL;
    return status;
}

mc_client_screen_status mc_client_screen_inventory_release(mc_client_source_ue **o,
                                                           mc_client_screen_error *e) {
    return release(o, false, e);
}

mc_client_screen_status mc_client_screen_inventory_retire(mc_client_source_ue **o,
                                                          mc_client_screen_error *e) {
    return release(o, true, e);
}

static bool app(mc_client_source_ue *o, const char *method, const char *descriptor, unsigned int pc,
                client_application **out, mc_client_screen_error *e) {
    *out = o->base.fields.b;
    return *out != NULL ||
           fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ID null Minecraft field receiver",
                "id", method, descriptor, pc, false);
}

bool mc_screen_inventory_key_locked(mc_client_source_ue *o, int32_t key, mc_entity **player,
                                    mc_client_screen_error *e) {
    *player = NULL;
    client_application *a;
    if (key != 1) {
        if (!app(o, "a", "(CI)V", 10, &a, e))
            return false;
        if (a->settings == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ID null Settings.r receiver",
                        "id", "a", "(CI)V", 13, false);
        mc_game_settings_state settings;
        if (!mc_game_settings_read_state(a->settings, &settings))
            return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "ID actual Settings fields", "id", "a",
                        "(CI)V", 13, true);
        mc_key_binding *binding = settings.named[5];
        if (binding == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                        "ID null Inventory binding.b receiver", "id", "a", "(CI)V", 16, false);
        mc_game_settings_text name;
        int32_t configured;
        if (!mc_key_binding_read(binding, &name, &configured))
            return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "ID actual Inventory binding", "id", "a",
                        "(CI)V", 16, true);
        if (key != configured) {
            o->last_pc = 32;
            return true;
        }
    }
    if (!app(o, "a", "(CI)V", 26, &a, e))
        return false;
    *player = a->h;
    if (*player == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                    "ID null DC.r invocation receiver", "id", "a", "(CI)V", 29, false);
    o->last_pc = 29;
    return true;
}

bool mc_screen_inventory_tick_locked(mc_client_source_ue *o, mc_entity **player,
                                     mc_client_screen_error *e) {
    *player = NULL;
    client_application *a;
    if (!app(o, "a", "()V", 8, &a, e))
        return false;
    mc_entity *first = a->h;
    if (first == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                    "ID null Player.W invocation receiver", "id", "a", "()V", 11, false);
    if (!mc_entity_is_kind(first, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) || first->local_player == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "ID actual concrete Player storage", "id",
                    "a", "()V", 11, true);
    bool alive = !first->base.be;
    if (alive) {
        mc_living_entity_fields *living = mc_entity_living_fields(first);
        if (living == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "ID actual Living Player storage", "id",
                        "a", "()V", 11, true);
        alive = living->Y > 0;
    }
    /* Actual inherited LS.W: !be && Y>0. ID then reads App.h/be afresh. */
    if (alive) {
        if (!app(o, "a", "()V", 21, &a, e))
            return false;
        if (a->h == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ID null Player.be receiver",
                        "id", "a", "()V", 24, false);
        if (!a->h->base.be) {
            o->last_pc = 40;
            return true;
        }
    }
    if (!app(o, "a", "()V", 34, &a, e))
        return false;
    *player = a->h;
    if (*player == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ID null dead Player.r receiver",
                    "id", "a", "()V", 37, false);
    o->last_pc = 37;
    return true;
}

static mc_inventory_screen_call *source_call(mc_client_source_ue *o, mc_client_screen_error *e) {
    if (o->source_calls == SIZE_MAX) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_LIMIT, "Inventory source invocation count", NULL, NULL,
                       NULL, 0);
        return NULL;
    }
    mc_inventory_screen_call *call = o->calls;
    if (call != NULL) {
        if (!call->returned) {
            /* A later attempt cannot consume or overwrite a failed prefix. */
            mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "Inventory retained failed source invocation",
                           NULL, NULL, NULL, 0);
            e->inventory = &o->detail;
            return NULL;
        }
        if (!retire_call(o, e))
            return NULL;
        *call = (mc_inventory_screen_call){0};
    } else {
        call = mc_screen_allocate(&o->base.memory, sizeof(*call));
        if (call == NULL) {
            fail(o, e, o->base.memory.allocation_status, "Inventory retained source invocation",
                 NULL, NULL, NULL, 0, true);
            return NULL;
        }
        o->calls = call;
    }
    ++o->source_calls;
    return call;
}

static bool container_failure(mc_client_source_ue *o, mc_inventory_screen_call *call,
                              const char *method, const char *descriptor, unsigned int pc,
                              mc_client_screen_error *e) {
    o->detail.container = call->failure;
    o->detail.child =
        (mc_client_screen_site){call->failure.site.class_name, call->failure.site.method,
                                call->failure.site.descriptor, call->failure.site.pc};
    const char *exception = call->failure.exception_class;
    const mc_client_screen_status status =
        exception != NULL && strcmp(exception, "java/lang/NullPointerException") == 0
            ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
        : exception != NULL && strcmp(exception, "java/lang/ArrayIndexOutOfBoundsException") == 0
            ? MC_CLIENT_SCREEN_SOURCE_INDEX
            : MC_CLIENT_SCREEN_APP_FAILED;
    return fail(o, e, status, call->failure.operation, "id", method, descriptor, pc,
                call->failure.native_policy);
}

bool mc_screen_inventory_closed_locked(mc_client_source_ue *o, mc_client_screen_error *e) {
    client_application *a;
    if (!app(o, "h", "()V", 4, &a, e))
        return false;
    if (a->h == NULL) {
        e->source_returned = true;
        o->last_pc = 10;
        return true;
    }
    if (!app(o, "h", "()V", 15, &a, e))
        return false;
    mc_client_singleplayer_controller *controller = a->controller;
    mc_container *container = o->id.j;
    if (container == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                    "ID null captured Container.f receiver", "id", "h", "()V", 22, false);
    int32_t window;
    if (mc_container_window_id(container, &window, &o->detail.ownership) != MC_INVENTORY_OK)
        return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "ID actual captured Container window ID",
                    "id", "h", "()V", 22, true);
    if (!app(o, "h", "()V", 29, &a, e))
        return false;
    mc_entity *player = a->h;
    if (controller == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ID null OB close receiver", "id",
                    "h", "()V", 32, false);
    mc_inventory_screen_call *call = source_call(o, e);
    if (call == NULL)
        return false;
    if (mc_client_singleplayer_controller_retain(controller) != MC_SINGLEPLAYER_CONTROLLER_OK)
        return fail(o, e, MC_CLIENT_SCREEN_BUSY, "ID native captured Controller occurrence", NULL,
                    NULL, NULL, 0, true);
    call->controller = controller;
    if (mc_player_container_close_begin(o->base.classes->registry, a->source_math_classes, player,
                                        window, &call->call, &call->failure) != MC_ENTITY_OK)
        return container_failure(o, call, "h", "()V", 32, e);
    call->returned = true;
    o->last_pc = 35;
    e->source_returned = true;
    return true;
}

static bool slot_failure(mc_client_source_ue *o, const mc_container_interaction_error *child,
                         const char *descriptor, unsigned int pc, mc_client_screen_error *e) {
    o->detail.container.interaction = *child;
    o->detail.child =
        (mc_client_screen_site){child->site.class_name, child->site.method, child->site.descriptor,
                                child->site.pc < 0 ? 0u : (unsigned int)child->site.pc};
    return fail(o, e,
                child->status == MC_INVENTORY_NULL_REFERENCE
                    ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                : child->status == MC_INVENTORY_INDEX_OUT_OF_BOUNDS ? MC_CLIENT_SCREEN_SOURCE_INDEX
                                                                    : MC_CLIENT_SCREEN_APP_FAILED,
                child->operation, "id", "a", descriptor, pc, child->native_policy);
}

static bool slot_hit(mc_client_source_ue *o, mc_inventory_slot *slot, int32_t x, int32_t y,
                     bool *hit, mc_client_screen_error *e) {
    const int32_t left = mc_java_i32_sub(o->base.fields.c, o->id.a) / 2;
    const int32_t top = mc_java_i32_sub(o->base.fields.d, o->id.i) / 2;
    x = mc_java_i32_sub(x, left);
    y = mc_java_i32_sub(y, top);
    if (slot == NULL) {
        fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ID hit null Slot.b receiver", "id", "a",
             "(Lgp;II)Z", 38, false);
        e->parent_site = (mc_client_screen_site){"id", "a", "(II)Lgp;", 41};
        return false;
    }
    mc_container_slot_state state;
    if (!mc_inventory_slot_source_state(slot, &state))
        return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "ID actual Slot storage", "id", "a",
                    "(Lgp;II)Z", 38, true);
    *hit = x >= mc_java_i32_sub(state.x, 1) &&
           x < mc_java_i32_add(mc_java_i32_add(state.x, 16), 1) &&
           y >= mc_java_i32_sub(state.y, 1) && y < mc_java_i32_add(mc_java_i32_add(state.y, 16), 1);
    return true;
}

static bool find_slot(mc_client_source_ue *o, int32_t x, int32_t y, mc_inventory_slot **out,
                      mc_client_screen_error *e) {
    *out = NULL;
    for (int32_t index = 0;; index = mc_java_i32_add(index, 1)) {
        mc_container *container = o->id.j;
        if (container == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                        "ID null Container.e receiver", "id", "a", "(II)Lgp;", 7, false);
        int32_t count;
        mc_container_interaction_error child;
        if (mc_container_source_slots_size(container, &count, &child) != MC_INVENTORY_OK)
            return slot_failure(o, &child, "(II)Lgp;", 10, e);
        if (index >= count)
            return true;
        container = o->id.j;
        if (container == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                        "ID fresh null Container.e receiver", "id", "a", "(II)Lgp;", 22, false);
        mc_inventory_slot *slot;
        if (mc_container_source_slot(container, index, &slot, &child) != MC_INVENTORY_OK)
            return slot_failure(o, &child, "(II)Lgp;", 26, e);
        bool hit;
        if (!slot_hit(o, slot, x, y, &hit, e))
            return false;
        if (hit) {
            *out = slot;
            return true;
        }
    }
}

bool mc_screen_inventory_mouse_after_base_locked(mc_client_source_ue *o, int32_t x, int32_t y,
                                                 int32_t button, mc_client_sdk_input *input,
                                                 mc_client_screen_error *e) {
    if (button != 0 && button != 1)
        return true;
    mc_inventory_slot *slot;
    if (!find_slot(o, x, y, &slot, e)) {
        o->detail.caller = (mc_client_screen_site){"id", "a", "(III)V", 19};
        return false;
    }
    const int32_t left = mc_java_i32_sub(o->base.fields.c, o->id.a) / 2;
    const int32_t top = mc_java_i32_sub(o->base.fields.d, o->id.i) / 2;
    const bool outside = x < left || y < top || x >= mc_java_i32_add(left, o->id.a) ||
                         y >= mc_java_i32_add(top, o->id.i);
    int32_t index = -1;
    if (slot != NULL) {
        mc_container_slot_state fields;
        if (!mc_inventory_slot_source_state(slot, &fields))
            return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "ID captured Slot.a field", "id", "a",
                        "(III)V", 101, true);
        index = fields.container_index;
    }
    if (outside)
        index = -999;
    if (index == -1)
        return true;
    bool shift = false;
    if (index != -999) {
        if (mc_client_sdk_keyboard_key_down(input, 42, &shift, &o->detail.input) !=
            MC_CLIENT_SDK_INPUT_OK)
            return fail(o, e, MC_CLIENT_SCREEN_INPUT_FAILED, o->detail.input.operation, "id", "a",
                        "(III)V", 132, o->detail.input.native_policy);
        if (!shift && mc_client_sdk_keyboard_key_down(input, 54, &shift, &o->detail.input) !=
                          MC_CLIENT_SDK_INPUT_OK)
            return fail(o, e, MC_CLIENT_SCREEN_INPUT_FAILED, o->detail.input.operation, "id", "a",
                        "(III)V", 140, o->detail.input.native_policy);
    }
    client_application *a;
    if (!app(o, "a", "(III)V", 157, &a, e))
        return false;
    mc_client_singleplayer_controller *controller = a->controller;
    mc_container *container = o->id.j;
    if (container == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                    "ID click null Container.f receiver", "id", "a", "(III)V", 164, false);
    int32_t window;
    if (mc_container_window_id(container, &window, &o->detail.ownership) != MC_INVENTORY_OK)
        return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "ID click actual window ID", "id", "a",
                    "(III)V", 164, true);
    if (!app(o, "a", "(III)V", 176, &a, e))
        return false;
    mc_entity *player = a->h;
    if (controller == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ID null OB slot click receiver",
                    "id", "a", "(III)V", 179, false);
    mc_inventory_screen_call *call = source_call(o, e);
    if (call == NULL)
        return false;
    if (mc_client_singleplayer_controller_retain(controller) != MC_SINGLEPLAYER_CONTROLLER_OK)
        return fail(o, e, MC_CLIENT_SCREEN_BUSY, "ID click native Controller occurrence", NULL,
                    NULL, NULL, 0, true);
    call->controller = controller;
    if (mc_player_container_click_begin(o->base.classes->registry, a->source_math_classes, player,
                                        window, index, button, shift, &call->call,
                                        &call->failure) != MC_ENTITY_OK)
        return container_failure(o, call, "a", "(III)V", 179, e);
    call->returned = true;
    o->last_pc = 183;
    e->source_returned = true;
    return true;
}
