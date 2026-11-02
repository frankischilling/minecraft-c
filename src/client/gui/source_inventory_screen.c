#include "source_inventory_screen_internal.h"

#include "../../entity/living/fields_internal.h"
#include "../../entity/player/player_internal.h"
#include "../../entity/player/terminal_inventory.h"
#include "../application_state.h"
#include "../render/model_graph_internal.h"
#include "../runtime/bootstrap_terminal_internal.h"
#include "mc/core/java/java.h"
#include "mc/statistics/achievements/achievements.h"
#include "world/runtime/runtime_internal.h"

#include <stdlib.h>
#include <string.h>

static bool fail(mc_client_source_id *o, mc_client_screen_error *e, mc_client_screen_status status,
                 const char *operation, const char *owner, const char *method,
                 const char *descriptor, unsigned int pc, bool native) {
    mc_screen_fail(e, status, operation, owner, method, descriptor, pc);
    e->native_policy = native;
    e->source_exception = !native && (status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE ||
                                      status == MC_CLIENT_SCREEN_SOURCE_INDEX ||
                                      status == MC_CLIENT_SCREEN_SOURCE_ARITHMETIC);
    e->source_error = false;
    e->exception_class =
        !e->source_exception                               ? NULL
        : status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE ? "java/lang/NullPointerException"
        : status == MC_CLIENT_SCREEN_SOURCE_INDEX ? "java/lang/ArrayIndexOutOfBoundsException"
                                                  : "java/lang/ArithmeticException";
    if (o != NULL) {
        e->inventory = &o->detail;
        o->last_pc = pc;
        o->base.failure = *e;
    }
    return false;
}

static void store(mc_client_source_id *o, const char *owner, const char *descriptor,
                  unsigned int pc) {
    if (o->base.store_count < sizeof(o->base.stores) / sizeof(o->base.stores[0]))
        o->base.stores[o->base.store_count++] =
            (mc_client_screen_site){owner, "<init>", descriptor, pc};
    o->last_pc = pc;
}

bool mc_screen_container_construct_locked(mc_client_source_id *o, mc_container *container,
                                          mc_client_screen_error *e) {
    if (!mc_screen_base_construct_locked(&o->base, e)) {
        e->parent_site = (mc_client_screen_site){"id", "<init>", "(Ldw;)V", 1};
        o->detail.child = e->parent_site;
        return false;
    }
    o->id.a = 176;
    store(o, "id", "(Ldw;)V", 8);
    o->id.i = 166;
    store(o, "id", "(Ldw;)V", 15);
    o->id.j = container;
    store(o, "id", "(Ldw;)V", 20);
    o->last_pc = 23;
    return true;
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
                mc_client_source_ue *screen = calloc(1, sizeof(*screen));
                mc_client_source_id *o = screen == NULL ? NULL : &screen->container;
                if (o == NULL)
                    fail(NULL, &e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "UE RAW owner allocation",
                         NULL, NULL, NULL, 0, true);
                else {
                    o->base.classes = c;
                    o->base.holds = 1;
                    o->base.kind = MC_CLIENT_SCREEN_INVENTORY;
                    mc_screen_memory_init(&o->base.memory, options, sizeof(*screen));
                    ++c->holds;
                    *out = screen;
                }
            }
        }
        mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

static bool construct(mc_client_source_id *o, mc_entity *player, mc_client_screen_error *e) {
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
    o->detail.caller = (mc_client_screen_site){"ue", "<init>", "(Lgs;)V", 5};
    if (!mc_screen_container_construct_locked(o, o->captured_container, e))
        return false;
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

mc_client_screen_status mc_client_screen_inventory_construct(mc_client_source_ue *screen,
                                                             mc_entity *player,
                                                             mc_client_screen_error *error) {
    mc_client_source_id *o = screen == NULL ? NULL : &screen->container;
    mc_client_screen_error e = {0};
    if (o == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "UE native constructor receiver", NULL,
             NULL, NULL, 0, true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        if (o->base.phase != MC_CLIENT_SCREEN_RAW || o->base.kind != MC_CLIENT_SCREEN_INVENTORY ||
            o->base.classes->retiring)
            fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_PHASE, "UE one-shot constructor", NULL, NULL,
                 NULL, 0, true);
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

bool mc_screen_container_initialize_locked(mc_client_source_id *o, mc_client_screen_error *e) {
    if (o->base.kind == MC_CLIENT_SCREEN_WORKBENCH) {
        /* OO inherits actual ID.b: DA.b returns before these fresh reads. */
        client_application *a = o->base.fields.b;
        if (a == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                        "ID initialize null Minecraft.h receiver", "id", "b", "()V", 8, false);
        mc_entity *player = a->h;
        mc_container *container = o->id.j;
        if (player == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                        "ID initialize null Player.e store receiver", "id", "b", "()V", 15, false);
        if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
            player->local_player == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED,
                        "ID initialize actual Player source storage", "id", "b", "()V", 15, true);
        mc_screen_id_player_hold *hold = o->published_players;
        while (hold != NULL && hold->player != player)
            hold = hold->next;
        if (hold == NULL) {
            hold = mc_screen_allocate(&o->base.memory, sizeof(*hold));
            if (hold == NULL)
                return fail(o, e, o->base.memory.allocation_status,
                            "ID native Player publication occurrence", "id", "b", "()V", 15, true);
            o->detail.entity = mc_entity_retain(player);
            if (o->detail.entity != MC_ENTITY_OK)
                return fail(o, e, MC_CLIENT_SCREEN_BUSY,
                            "ID native captured Player publication occurrence", "id", "b", "()V",
                            15, true);
            hold->player = player;
            hold->next = o->published_players;
            o->published_players = hold;
        }
        player->local_player->fields.player.e = container;
        o->last_pc = 18;
        e->source_returned = true;
        e->site = (mc_client_screen_site){"id", "b", "()V", 18};
        return true;
    }
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

static bool cleanup_failure(mc_client_source_id *o, mc_client_screen_error *e,
                            const char *operation) {
    mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, operation, NULL, NULL, NULL, 0);
    e->inventory = &o->detail;
    return false;
}

static bool retire_one_call(mc_client_source_id *o, mc_inventory_screen_call *call,
                            mc_client_screen_error *e) {
    if (call != NULL) {
        const void *publication = call->failure.drop.publication;
        const void *statistic = call->failure.drop.statistic;
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
        if (o->detail.container.drop.publication == publication)
            o->detail.container.drop.publication = call->failure.drop.publication;
        if (o->detail.container.drop.statistic == statistic)
            o->detail.container.drop.statistic = call->failure.drop.statistic;
        mc_player_container_error *receipt = call == o->closed_parent   ? &o->detail.inherited_close
                                             : call == o->closed_direct ? &o->detail.direct_close
                                                                        : NULL;
        if (receipt != NULL) {
            receipt->drop.publication = call->failure.drop.publication;
            receipt->drop.statistic = call->failure.drop.statistic;
        }
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

bool mc_screen_container_retirement_admit_locked(mc_client_source_id *o,
                                                 mc_client_screen_error *e) {
    if (!o->container_owned || o->captured_container == NULL)
        return true;
    if (o->constructor_player != NULL &&
        (o->constructor_player->local_player == NULL ||
         o->constructor_player->local_player->fields.player.e == o->captured_container))
        return cleanup_failure(o, e, "OO owned Container still published in Player.e");
    for (mc_screen_id_player_hold *hold = o->published_players; hold != NULL; hold = hold->next)
        if (hold->player->local_player == NULL ||
            hold->player->local_player->fields.player.e == o->captured_container)
            return cleanup_failure(o, e, "OO owned Container still published in ID Player.e");
    return true;
}

bool mc_screen_container_cleanup_locked(mc_client_source_id *o, mc_client_screen_error *e) {
    if (!mc_screen_container_retirement_admit_locked(o, e))
        return false;
    if (!retire_one_call(o, o->calls, e) || !retire_one_call(o, o->closed_direct, e) ||
        !retire_one_call(o, o->closed_parent, e))
        return false;
    if (o->container_owned) {
        if (mc_container_retire(&o->captured_container, &o->detail.cleanup_ownership) !=
            MC_INVENTORY_OK)
            return cleanup_failure(o, e, "OO owned Container native retirement");
        o->container_owned = false;
    }
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
    if (o->world_held) {
        mc_world_runtime_binding_release(o->constructor_world);
        o->constructor_world = NULL;
        o->world_held = false;
    }
    o->constructor_inventory = NULL;
    while (o->published_players != NULL) {
        mc_screen_id_player_hold *hold = o->published_players;
        o->published_players = hold->next;
        mc_entity_release(hold->player);
        hold->player = NULL;
    }
    return true;
}

bool mc_screen_container_terminal_prepare_locked(mc_client_source_id *o,
                                                 mc_client_screen_error *e) {
    if (o->base.active_calls != 0 || o->base.phase == MC_CLIENT_SCREEN_CONSTRUCTING ||
        o->base.holds != 1)
        return cleanup_failure(o, e, "Inventory terminal active/external Screen occurrence");
    if (!mc_screen_container_retirement_admit_locked(o, e))
        return false;
    /* Native one-way latch: direct Screen entry must stop before the constructor
     * occurrences end. No source field, close callback or failure is changed. */
    o->base.phase = MC_CLIENT_SCREEN_RETIRING;
    return mc_screen_container_cleanup_locked(o, e);
}

static bool handoff_failure(mc_client_source_id *o, mc_client_screen_error *e,
                            const char *operation) {
    (void)cleanup_failure(o, e, operation);
    e->native_policy = true;
    return false;
}

bool mc_screen_container_terminal_handoff_locked(mc_client_source_id *o,
                                                 const mc_client_bootstrap_terminal_scope *scope,
                                                 mc_entity *player, mc_registry_classes *registry,
                                                 mc_world_runtime *world,
                                                 mc_client_screen_error *e) {
    if (!mc_client_bootstrap_terminal_scope_matches(scope, o->base.fields.b))
        return handoff_failure(o, e, "OO native selected terminal root scope");
    if (o->base.kind != MC_CLIENT_SCREEN_WORKBENCH || o->base.active_calls != 0 ||
        o->base.holds != 1 || o->base.phase == MC_CLIENT_SCREEN_CONSTRUCTING)
        return handoff_failure(o, e, "OO terminal handoff active/external Screen occurrence");
    if (o->terminal_handoff_done) {
        /* The actual Player may already have been consumed by a later native
         * tail. Never revisit ended constructor/publication borrows on retry. */
        if (o->base.phase != MC_CLIENT_SCREEN_RETIRING || o->container_owned ||
            o->captured_container != NULL || o->container_held)
            return handoff_failure(o, e, "OO terminal completed handoff owner slots");
        return mc_screen_container_cleanup_locked(o, e);
    }
    if (player == NULL || player->local_player == NULL || registry != o->base.classes->registry ||
        !o->container_owned || o->container_held || o->captured_container == NULL ||
        o->id.j != o->captured_container || !o->player_held || o->constructor_player != player ||
        !o->world_held || o->constructor_world != world || player->base.world != world ||
        player->local_player->fields.minecraft != o->base.fields.b ||
        o->constructor_inventory != player->local_player->fields.player.c ||
        player->local_player->fields.player.e != o->captured_container ||
        o->published_players == NULL)
        return handoff_failure(o, e, "OO terminal exact constructor and published Player owner");
    size_t publications = 0;
    for (const mc_screen_id_player_hold *hold = o->published_players; hold != NULL;
         hold = hold->next) {
        if (publications == o->base.memory.allocations || hold->player != player)
            return handoff_failure(o, e, "OO terminal exact Player publication ledger");
        ++publications;
    }
    /* The genuine root has ended Source production. Revoke this Screen's
     * direct entry before checked child cleanup; a later BUSY keeps the same
     * still-owned IT and native terminal progress, without Source replay. */
    o->base.phase = MC_CLIENT_SCREEN_RETIRING;
    if (!retire_one_call(o, o->calls, e) || !retire_one_call(o, o->closed_direct, e) ||
        !retire_one_call(o, o->closed_parent, e)) {
        e->native_policy = true;
        return false;
    }
    mc_player_terminal_inventory_error adopted = {0};
    const mc_entity_status status = mc_player_terminal_inventory_adopt(
        player, registry, world, &o->captured_container, &adopted);
    o->detail.entity = status;
    o->detail.cleanup_ownership = adopted.inventory;
    if (status != MC_ENTITY_OK) {
        (void)handoff_failure(o, e, adopted.operation);
        if (status == MC_ENTITY_LIMIT)
            e->status = MC_CLIENT_SCREEN_LIMIT;
        else if (status == MC_ENTITY_ALLOCATION_FAILED)
            e->status = MC_CLIENT_SCREEN_ALLOCATION_FAILED;
        return false;
    }
    o->container_owned = false;
    o->terminal_handoff_done = true;
    /* Only native owner slots moved. Source j/e/c/d/r remain untouched, and
     * the exact Load Player occurrence protects the sealed sidecar while the
     * constructor/publication Player and World occurrences end below. */
    return mc_screen_container_cleanup_locked(o, e);
}

mc_client_source_da *mc_client_screen_inventory_base(mc_client_source_ue *screen) {
    return screen == NULL ? NULL : &screen->container.base;
}

mc_client_screen_status mc_client_screen_inventory_inspect(mc_client_source_ue *screen,
                                                           mc_client_inventory_screen_state *out,
                                                           mc_client_screen_error *error) {
    mc_client_source_id *o = screen == NULL ? NULL : &screen->container;
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "UE native inspection", NULL, NULL, NULL,
             0, true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        *out = (mc_client_inventory_screen_state){.phase = o->base.phase,
                                                  .holds = o->base.holds,
                                                  .source_calls = o->source_calls,
                                                  .id = o->id,
                                                  .ue = screen->fields,
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
    mc_client_source_da *base = *slot == NULL ? NULL : &(*slot)->container.base;
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

static bool app(mc_client_source_id *o, const char *method, const char *descriptor, unsigned int pc,
                client_application **out, mc_client_screen_error *e) {
    *out = o->base.fields.b;
    return *out != NULL ||
           fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ID null Minecraft field receiver",
                "id", method, descriptor, pc, false);
}

bool mc_screen_container_key_locked(mc_client_source_id *o, int32_t key, mc_entity **player,
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

bool mc_screen_container_tick_locked(mc_client_source_id *o, mc_entity **player,
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

static mc_inventory_screen_call *source_call(mc_client_source_id *o, mc_client_screen_error *e) {
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
        if (!retire_one_call(o, call, e))
            return NULL;
        *call = (mc_inventory_screen_call){0};
    } else {
        if (o->spare_calls != NULL) {
            call = o->spare_calls;
            o->spare_calls = call->next;
            *call = (mc_inventory_screen_call){0};
        } else
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

static bool container_failure(mc_client_source_id *o, mc_inventory_screen_call *call,
                              const char *owner, const char *method, const char *descriptor,
                              unsigned int pc, mc_client_screen_error *e) {
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
    (void)fail(o, e, status, call->failure.operation, owner, method, descriptor, pc,
               call->failure.native_policy);
    e->source_exception = call->failure.source_exception;
    e->source_error = call->failure.source_error;
    e->exception_class = call->failure.exception_class;
    o->base.failure = *e;
    return false;
}

bool mc_screen_container_closed_locked(mc_client_source_id *o, mc_client_screen_error *e) {
    if (o->base.kind == MC_CLIENT_SCREEN_WORKBENCH) {
        if ((o->closed_parent != NULL && !o->closed_parent->returned) ||
            (o->closed_direct != NULL && !o->closed_direct->returned))
            return cleanup_failure(o, e, "OO retained failed close invocation");
        if (!retire_one_call(o, o->closed_direct, e) || !retire_one_call(o, o->closed_parent, e))
            return false;
        /* Reuse the same two bounded records for a later genuine h call. */
        if (o->closed_direct != NULL) {
            o->closed_direct->next = o->spare_calls;
            o->spare_calls = o->closed_direct;
            o->closed_direct = NULL;
        }
        if (o->closed_parent != NULL) {
            o->closed_parent->next = o->spare_calls;
            o->spare_calls = o->closed_parent;
            o->closed_parent = NULL;
        }
        o->inherited_closed_returned = o->direct_closed_returned = false;
    }
    client_application *a;
    if (!app(o, "h", "()V", 4, &a, e))
        return false;
    if (a->h == NULL) {
        e->source_returned = true;
        o->last_pc = 10;
        o->inherited_closed_returned = true;
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
    if (o->base.kind == MC_CLIENT_SCREEN_WORKBENCH) {
        o->closed_parent = call;
        o->calls = NULL;
    }
    if (mc_player_container_close_begin(o->base.classes->registry, a->source_math_classes, player,
                                        window, &call->call, &call->failure) != MC_ENTITY_OK) {
        if (o->base.kind == MC_CLIENT_SCREEN_WORKBENCH)
            o->detail.inherited_close = call->failure;
        return container_failure(o, call, "id", "h", "()V", 32, e);
    }
    call->returned = true;
    if (o->base.kind == MC_CLIENT_SCREEN_WORKBENCH) {
        o->detail.inherited_close = call->failure;
    }
    o->inherited_closed_returned = true;
    o->last_pc = 35;
    e->source_returned = true;
    return true;
}

bool mc_screen_container_direct_close_locked(mc_client_source_id *o, mc_client_screen_error *e) {
    if (o->base.kind != MC_CLIENT_SCREEN_WORKBENCH || !o->inherited_closed_returned)
        return cleanup_failure(o, e, "OO direct close requires returned inherited child");
    /* OO.h evaluates j before fresh b/h, independently of the earlier ID.h. */
    mc_container *container = o->id.j;
    client_application *a = o->base.fields.b;
    if (a == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                    "OO close null Minecraft.h receiver", "oo", "h", "()V", 12, false);
    mc_entity *player = a->h;
    mc_inventory_screen_call *call = source_call(o, e);
    if (call == NULL)
        return false;
    o->closed_direct = call;
    o->calls = NULL;
    e->source_returned = false;
    if (mc_player_container_direct_close_begin(o->base.classes->registry, a->source_math_classes,
                                               player, container, &call->call,
                                               &call->failure) != MC_ENTITY_OK) {
        o->detail.direct_close = call->failure;
        return container_failure(o, call, "oo", "h", "()V", 15, e);
    }
    call->returned = true;
    o->detail.direct_close = call->failure;
    o->direct_closed_returned = true;
    o->last_pc = 18;
    e->site = (mc_client_screen_site){"oo", "h", "()V", 18};
    e->source_returned = true;
    return true;
}

static bool slot_failure(mc_client_source_id *o, const mc_container_interaction_error *child,
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

static bool slot_hit(mc_client_source_id *o, mc_inventory_slot *slot, int32_t x, int32_t y,
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

static bool find_slot(mc_client_source_id *o, int32_t x, int32_t y, mc_inventory_slot **out,
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

bool mc_screen_container_mouse_after_base_locked(mc_client_source_id *o, int32_t x, int32_t y,
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
        return container_failure(o, call, "id", "a", "(III)V", 179, e);
    call->returned = true;
    o->last_pc = 183;
    e->source_returned = true;
    return true;
}
