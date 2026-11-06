/* Actual source SaveHandler/World/Chunk import with a real Item NBT actor.
 * The imported support program is never executed. The observation receiver is
 * an authored native component producer, not the original World's natural N
 * callback path. Its source Exception is produced by a real RAW N body. */
#include "../../../../src/client/world/render_global_access.h"
#include "../../../../src/client/world/render_global_internal.h"
#include "../../../../src/world/runtime/world_access_internal.h"
#include "../saving/mob_nbt_source_support.inc"
#include "mc/client/controller/use.h"
#include "mc/entity/player/player.h"
#include "mc/item/use/use.h"
#include "mc/world/runtime/placement.h"

typedef struct placement_observer {
    mc_world_placement_error *placement;
    mc_item_use_call **item;
    mc_singleplayer_use_call **use;
    unsigned int action;
    mc_world_access *access, *n_identity;
    mc_world_access_receipt *receipt;
    mc_world_access_failure child;
    mc_client_render_engine_classes *graphics;
    mc_client_render_global *renderer;
    mc_render_global_access *bridge;
    mc_client_render_global_error *n_error;
    size_t calls;
    bool refused, changed;
} placement_observer;

static bool placement_parent_busy(placement_observer *o) {
    if (o->action == 0)
        return !mc_world_placement_error_retire(o->placement) &&
               o->placement->cleanup.status == MC_WORLD_STORAGE_BUSY &&
               o->placement->work != NULL && o->placement->site.pc == 5;
    if (o->action == 1) {
        mc_item_use_error cleanup = {0};
        return mc_item_use_retire(o->item, &cleanup) == MC_ITEM_USE_BUSY && *o->item != NULL;
    }
    mc_singleplayer_use_error cleanup = {0};
    return mc_client_singleplayer_use_retire(o->use, &cleanup) == MC_SINGLEPLAYER_USE_BUSY &&
           *o->use != NULL;
}

static mc_world_storage_status placement_observe_prefix(void *context, mc_world_runtime *world,
                                                        mc_entity *entity,
                                                        mc_world_storage_error *error) {
    placement_observer *o = context;
    ++o->calls;
    /* This is a genuine synchronous source publication, not a manufactured
     * active counter. The exact parent operation must survive checked refusal. */
    o->refused = placement_parent_busy(o);
    const mc_world_storage_status changed =
        mc_world_runtime_metadata_set_quiet(world, 8, 64, 8, 7, &o->changed, error);
    if (entity == NULL || !o->refused || changed != MC_WORLD_STORAGE_OK || !o->changed) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                          "placement callback native contract", 0};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status placement_observe_added(void *context, mc_world_runtime *world,
                                                       mc_entity *entity,
                                                       mc_world_storage_error *error) {
    const mc_world_storage_status status = placement_observe_prefix(context, world, entity, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "authored placement publication refusal after metadata", 0};
    return error->status;
}

static mc_world_storage_status placement_source_failure(placement_observer *o,
                                                        mc_world_runtime *world,
                                                        mc_world_storage_error *error,
                                                        mc_world_access_failure *failure) {
    mc_world_access_failure actual = {0};
    /* Actual N sound reads its real default-null App at22, before any Display
     * or GL operation. No diagnostic, READY flag, status or actor is forged. */
    const mc_world_storage_status source_status = mc_world_access_call_sound(
        o->n_identity, world, NULL, 8.0, 64.0, 8.0, 1.0f, 1.0f, error, &actual);
    if (source_status == MC_WORLD_STORAGE_OK)
        return source_status;
    const char *exception = NULL;
    if (!mc_world_access_failure_source_exception(&actual, &exception) ||
        mc_world_access_failure_retain(&actual, &o->child) != MC_WORLD_STORAGE_OK) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                          "actual N source Exception receipt missing", 0};
        return error->status;
    }
    mc_world_storage_error receipt_error = {0};
    const mc_world_storage_status captured = mc_world_access_receipt_create(
        o->access, actual.render_global, &o->receipt, failure, &receipt_error);
    if (captured != MC_WORLD_STORAGE_OK) {
        *error = receipt_error;
        return captured;
    }
    /* Own outer identity, real inner diagnostic and classification derived
     * exclusively from that inner invocation's public receipt API. */
    o->receipt->exception_class = exception;
    return source_status;
}

static mc_world_storage_status placement_observe_source(void *context, mc_world_runtime *world,
                                                        mc_entity *entity,
                                                        mc_world_storage_error *error,
                                                        mc_world_access_failure *failure) {
    const mc_world_storage_status status = placement_observe_prefix(context, world, entity, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    return placement_source_failure(context, world, error, failure);
}

static int placement_real_producer(placement_observer *o, mc_world_runtime *world) {
    o->n_error = calloc(1, sizeof(*o->n_error));
    VERIFY(o->n_error != NULL &&
           mc_client_render_engine_classes_create(NULL, &o->graphics) ==
               MC_CLIENT_RENDER_ENGINE_OK &&
           mc_client_render_global_allocate(o->graphics, NULL, &o->renderer, o->n_error) ==
               MC_CLIENT_RENDER_GLOBAL_OK &&
           mc_render_global_access_create(o->renderer, &o->bridge, o->n_error) ==
               MC_CLIENT_RENDER_GLOBAL_OK);
    const size_t index = world->fields.u->count;
    VERIFY(mc_render_global_access_source_add(o->bridge, world, o->n_error) ==
               MC_CLIENT_RENDER_GLOBAL_OK &&
           world->fields.u->count == index + 1);
    o->n_identity = world->fields.u->items[index];
    VERIFY(o->n_identity != NULL);
    return 0;
}

static int placement_source_callback(source_save_fixture *s, bool source_exception,
                                     unsigned int action) {
    fixture *f = &s->game;
    mc_world_placement_error e = {0};
    mc_item_use_call *item = NULL;
    mc_item_use_error item_error = {0}, item_cleanup = {0};
    mc_singleplayer_use_call *use = NULL;
    mc_singleplayer_use_error use_error = {0}, use_cleanup = {0};
    placement_observer observer = {.placement = &e, .item = &item, .use = &use, .action = action};
    mc_world_info_name *folder = ss_name("world");
    VERIFY(folder != NULL &&
           mc_world_store_source_allocate(s->handlers, &f->stores[0], &s->source) ==
               MC_WORLD_SOURCE_OK &&
           mc_world_store_source_construct(f->stores[0], s->directory, folder, false, &s->source) ==
               MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(folder);
    VERIFY(create_world(f, 0));
    if (source_exception) {
        VERIFY(placement_real_producer(&observer, f->worlds[0]) == 0);
        const mc_world_access_source_callbacks callbacks = {.entity_added =
                                                                placement_observe_source};
        VERIFY(mc_world_access_source_create(&callbacks, &observer, &observer.access, NULL) ==
               MC_WORLD_STORAGE_OK);
    } else {
        const mc_world_access_callbacks callbacks = {.entity_added = placement_observe_added};
        VERIFY(mc_world_access_create(&callbacks, &observer, &observer.access, NULL) ==
               MC_WORLD_STORAGE_OK);
    }
    VERIFY(mc_world_runtime_add_world_access(f->worlds[0], observer.access, NULL) ==
           MC_WORLD_STORAGE_OK);
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_entity *player = NULL;
    mc_item_stack *stack = NULL;
    mc_client_singleplayer_controller *controller = NULL;
    if (action != 0) {
        VERIFY(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &sessions,
                                                NULL) == MC_PLAYER_OK &&
               mc_player_session_create(sessions, NULL, NULL, &session, NULL) == MC_PLAYER_OK &&
               mc_local_player_allocate(f->entity_classes[0], &player, NULL) == MC_ENTITY_OK &&
               mc_local_player_construct(player, f->worlds[0], NULL, session, 0, NULL, NULL) ==
                   MC_PLAYER_OK &&
               mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
               mc_item_stack_construct(stack, 3, 1, 0) == MC_ITEM_STACK_OK);
        mc_player_session_retire(&session);
    }
    bool value = true, failed = false, native = true, exception = false, source_error = false;
    const char *exception_class = NULL;
    if (action == 0) {
        failed = mc_world_runtime_can_place(f->worlds[0], 3, 8, 64, 8, false, 1, &value, &e) !=
                 MC_WORLD_STORAGE_OK;
        VERIFY(value && !e.returned && !e.existing_read && e.site.pc == 5 && e.acquisition != NULL);
        native = e.native_policy;
        exception = e.source_exception;
        source_error = e.source_error;
        exception_class = e.exception_class;
    } else if (action == 1) {
        failed = mc_item_use_on_block_begin(f->registry[0], stack, player, f->worlds[0], 8, 64, 8,
                                            1, &item, &item_error) != MC_ITEM_USE_OK;
        VERIFY(item != NULL && item_error.site.pc == 7 &&
               strcmp(item_error.site.class_name, "ck") == 0 && item_error.acquisition != NULL);
        native = item_error.native_policy;
        exception = item_error.source_exception;
        source_error = item_error.source_error;
        exception_class = item_error.exception_class;
    } else {
        VERIFY(mc_client_singleplayer_controller_allocate(f->entity_classes[0], f->profile, 0,
                                                          &controller,
                                                          NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
               mc_client_singleplayer_controller_construct(controller, NULL, NULL) ==
                   MC_SINGLEPLAYER_CONTROLLER_OK);
        failed = mc_client_singleplayer_use_on_block_begin(controller, player, f->worlds[0], stack,
                                                           8, 64, 8, 1, &use,
                                                           &use_error) != MC_SINGLEPLAYER_USE_OK;
        VERIFY(use != NULL && use_error.site.pc == 7 &&
               strcmp(use_error.site.class_name, "ob") == 0 && use_error.acquisition != NULL);
        native = use_error.native_policy;
        exception = use_error.source_exception;
        source_error = use_error.source_error;
        exception_class = use_error.exception_class;
    }
    /* Cache publication/activation and metadata effects survive the callback
     * failure. Capture the actual published owner for native teardown. */
    VERIFY(f->providers[0].cache->insertion_count == 1);
    f->states[0] = f->providers[0].cache->insertions[0];
    f->chunks[0] = mc_world_store_chunk_object(f->stores[0], f->states[0]);
    VERIFY(failed && observer.calls == 1 && observer.refused && observer.changed &&
           f->states[0] != NULL && f->states[0]->active && f->chunks[0] != NULL &&
           f->worlds[0]->fields.b->count == 1 &&
           (f->states[0]->data->metadata[((8u << 11u) | (8u << 7u) | 64u) / 2u] & 15u) == 7);
    VERIFY(exception == source_exception && native != source_exception && !source_error);
    if (source_exception) {
        mc_render_global_callback_state actual = {0};
        VERIFY(
            exception_class != NULL &&
            strcmp(exception_class, "java/lang/NullPointerException") == 0 &&
            observer.receipt != NULL && observer.receipt->holds == 2 &&
            mc_world_access_receipt_retire(&observer.receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
            mc_render_global_access_callback_inspect(observer.bridge, &observer.child, &actual) &&
            actual.failure != NULL &&
            actual.failure->status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
            !actual.failure->native_policy && actual.failure->source_pc == 22 &&
            mc_render_global_access_retire(&observer.bridge, observer.n_error) ==
                MC_CLIENT_RENDER_GLOBAL_BUSY &&
            observer.bridge != NULL);
    } else
        VERIFY(exception_class == NULL && observer.receipt == NULL);
    VERIFY(mc_world_placement_error_retire(&e) &&
           mc_item_use_retire(&item, &item_cleanup) == MC_ITEM_USE_OK &&
           mc_client_singleplayer_use_retire(&use, &use_cleanup) == MC_SINGLEPLAYER_USE_OK &&
           observer.calls == 1);
    VERIFY(mc_world_access_receipt_retire(&observer.receipt, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_access_failure_release(&observer.child);
    if (source_exception) {
        VERIFY(mc_render_global_access_retire(&observer.bridge, observer.n_error) ==
                   MC_CLIENT_RENDER_GLOBAL_OK &&
               observer.bridge == NULL &&
               mc_client_render_global_release(&observer.renderer, observer.n_error) ==
                   MC_CLIENT_RENDER_GLOBAL_OK &&
               mc_client_render_engine_classes_release(observer.graphics) ==
                   MC_CLIENT_RENDER_ENGINE_OK);
        free(observer.n_error);
    }
    mc_item_stack_release(stack);
    mc_entity_release(player);
    if (sessions != NULL)
        VERIFY(mc_player_session_classes_free(&sessions, NULL) == MC_PLAYER_OK);
    if (controller != NULL)
        VERIFY(mc_client_singleplayer_controller_free(&controller, NULL) ==
               MC_SINGLEPLAYER_CONTROLLER_OK);
    /* A later independent source action sees the real retained metadata and
     * occupied Chest. Cleanup must not replay the failed activation callback. */
    int32_t metadata = -1;
    VERIFY(mc_world_runtime_metadata_get(f->worlds[0], 8, 64, 8, &metadata, NULL) ==
               MC_WORLD_STORAGE_OK &&
           metadata == 7 &&
           mc_world_runtime_can_place(f->worlds[0], 3, 8, 64, 8, false, 1, &value, &e) ==
               MC_WORLD_STORAGE_OK &&
           !value && e.returned && e.existing_id == 54 && e.collision_checked &&
           !e.predicate_entered && observer.calls == 1 && mc_world_placement_error_retire(&e));
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(f->worlds[0], observer.access, &removed, NULL) ==
               MC_WORLD_STORAGE_OK &&
           removed && mc_world_access_free(observer.access, NULL) == MC_WORLD_STORAGE_OK);
    VERIFY(ss_stop(s) == 0 && ss_finish(s) == 0);
    return 0;
}

#include "torch_callbacks.inc"

int main(int argc, char **argv) {
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "world-placement-callbacks") != 0 ||
        !decode_path(argv[2], root, sizeof(root)))
        return 1;
    for (unsigned int action = 0; action < 3; ++action)
        for (unsigned int mode = 0; mode < 2; ++mode) {
            source_save_fixture *s = calloc(1, sizeof(*s));
            VERIFY(s != NULL && ss_setup(s, root) == 0 &&
                   placement_source_callback(s, mode != 0, action) == 0);
            free(s);
        }
    for (unsigned int mode = 0; mode < 2; ++mode) {
        source_save_fixture *s = calloc(1, sizeof(*s));
        VERIFY(s != NULL && ss_setup(s, root) == 0 && torch_metadata_callback(s, mode != 0) == 0);
        free(s);
    }
    printf("PASS: %u World placement live callback/retirement checks\n", checks);
    return 0;
}
