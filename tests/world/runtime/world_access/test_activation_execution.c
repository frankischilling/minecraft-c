/* Native component controls, not an original activation packet. Every Chunk
 * is actually constructed/bound to B. No Source field is rebound/restored. */
int mc_activation_execution_unused_publication_main(int, char **);
#define main mc_activation_execution_unused_publication_main
#include "../test_entity_publication.c"
#undef main

#include "client/render_engine/image_buffer.h"
#include "client/world/render_global_access.h"
#include "client/world/render_global_internal.h"
#include "mc/entity/appearance.h"
#include "mc/entity/player/player.h"
#include "world/chunk/object_internal.h"
#include "world/runtime/activation_execution_internal.h"
#include "world/runtime/entity_publication_execution_internal.h"
#include "world/runtime/world_access_internal.h"

typedef struct ax_observer {
    mc_world_access *self;
    mc_world_access_receipt *receipt;
    mc_entity_tick_execution *execution;
    mc_world_runtime *a, *b;
    mc_entity *expected[3];
    size_t count, prefix, a_bindings, b_bindings, calls;
    size_t saved_receipt_holds, projected_receipt_holds, saved_bindings;
    mc_world_access_failure_lease *node;
    mc_entity_tick_execution_world_token token;
    mc_world_storage_error callback_cause;
    mc_world_storage_status returned, free_during;
    bool remove_self, removed, throwable, saturate, receipt_projected, bind_fault, bind_projected;
    bool observations_ok, armed;
} ax_observer;

typedef struct ax_fixture {
    fixture *base;
    ax_observer observer;
    mc_entity_tick_execution *execution;
    mc_world_activation_error *error;
    mc_entity *items[3];
    mc_client_render_engine_classes *render_classes;
    mc_client_render_global *renderer;
    mc_client_render_global_error *render_error;
    mc_player_session_classes *sessions;
    mc_player_session *session;
    mc_player_string *username;
    mc_entity *player;
    unsigned int cases;
} ax_fixture;

static const mc_entity_tick_execution_site ax_site = {"lm", "e", "()V", 33};

static void ax_marker(const char *name, const char *event) {
    printf("ACTIVATION_OWNER_CASE %s %s\n", name, event);
    fflush(stdout);
}

static mc_world_storage_status ax_added(void *context, mc_world_runtime *world, mc_entity *entity,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    ax_observer *r = context;
    const size_t index = r->calls++;
    bool same = world == r->b && index < r->count && entity == r->expected[index] &&
                world->fields.b->count == r->prefix + r->count &&
                world->entity_membership.count == world->fields.b->count &&
                r->a->persistent_bindings == r->a_bindings &&
                r->b->persistent_bindings == r->b_bindings && world->active_calls == 1 &&
                r->execution->world == world && r->execution->world_open &&
                r->execution->world_held && r->execution->current_lease == 1;
    for (size_t i = 0; i < r->count; ++i)
        same = same && world->fields.b->items[r->prefix + i] == r->expected[i] &&
               world->entity_membership.entries[r->prefix + i] == r->expected[i];
    mc_world_runtime *selected = NULL;
    same = same && mc_entity_tick_execution_current_world(r->execution, &r->token) &&
           r->token.slot == 1 &&
           mc_entity_tick_execution_read_world(r->execution, &r->token, &selected) &&
           selected == world;
    r->observations_ok &= same;
    r->node = mc_entity_tick_execution_world_lease(r->execution);
    const mc_world_access_failure *absent = NULL;
    mc_world_storage_error native = {0};
    r->armed = r->node != NULL && !mc_world_access_failure_lease_inspect(r->node, &absent) &&
               absent == NULL &&
               mc_world_access_failure_lease_release(r->node, &native) == MC_WORLD_STORAGE_BUSY;
    if (r->remove_self) {
        bool removed = false;
        const mc_world_storage_status status =
            mc_world_runtime_remove_world_access(world, r->self, &removed, cause);
        r->removed = status == MC_WORLD_STORAGE_OK && removed;
        if (!r->removed)
            return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
        r->free_during = mc_world_access_free(r->self, NULL);
    }
    if (r->returned != MC_WORLD_STORAGE_OK) {
        const mc_world_storage_status status =
            mc_world_access_receipt_create(r->self, NULL, &r->receipt, failure, cause);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        r->receipt->exception_class = r->throwable ? "java/lang/IllegalStateException" : NULL;
        if (r->saturate) {
            r->saved_receipt_holds = r->receipt->holds;
            r->receipt->holds = SIZE_MAX; /* Actual native capture-counter projection only. */
            r->receipt_projected = true;
        }
        *cause = (mc_world_storage_error){r->returned, "Activation authored callback failure", 91};
        r->callback_cause = *cause;
    }
    if (r->bind_fault) {
        r->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Downward native fault, no owning right is invented. */
        r->bind_projected = true;
    }
    return r->returned;
}

static void ax_restore_native_faults(ax_fixture *s) {
    ax_observer *r = &s->observer;
    if (r->receipt_projected) {
        r->projected_receipt_holds = r->receipt->holds;
        r->receipt->holds = r->saved_receipt_holds;
        r->receipt_projected = false;
    }
    if (r->bind_projected) {
        r->b->persistent_bindings = r->saved_bindings;
        r->bind_projected = false;
    }
}

static bool ax_start(ax_fixture *s, mc_world_runtime *a) {
    if (s->execution != NULL)
        return false;
    s->execution = calloc(1, sizeof(*s->execution));
    if (s->execution == NULL)
        return false;
    mc_entity_tick_execution_init(s->execution, NULL);
    return mc_entity_tick_execution_begin_world(
               s->execution, a,
               (mc_entity_tick_execution_site){"native-component", "outer", "()V", 1}) ==
           MC_WORLD_STORAGE_OK;
}

static bool ax_clear_and_retire(ax_fixture *s) {
    if (s->execution == NULL)
        return true;
    s->error->batch.publication.world_access_failure = (mc_world_access_failure){0};
    /* A refused B pop is still the real current scope. Bounded native unwind
     * consumes only exact current tokens, never a Source activation replay. */
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS && s->execution->world_open; ++i) {
        mc_entity_tick_execution_world_token token;
        if (!mc_entity_tick_execution_current_world(s->execution, &token) ||
            mc_entity_tick_execution_pop_world(s->execution, &token, true) != MC_WORLD_STORAGE_OK)
            return false;
    }
    if (s->execution->world_open ||
        mc_entity_tick_execution_retire(s->execution) != MC_WORLD_STORAGE_OK)
        return false;
    s->observer.execution = NULL;
    s->observer.node = NULL;
    free(s->execution);
    s->execution = NULL;
    return true;
}

static bool ax_unsubscribe(ax_fixture *s) {
    if (s->observer.self == NULL || s->base == NULL || s->base->worlds[2] == NULL)
        return true;
    bool removed = false;
    return mc_world_runtime_remove_world_access(s->base->worlds[2], s->observer.self, &removed,
                                                NULL) == MC_WORLD_STORAGE_OK;
}

static bool ax_prefix(ax_fixture *s, size_t start, mc_entity *const *entities, size_t count) {
    mc_world_runtime *b = s->base->worlds[2];
    if (b->fields.b->count != start + count || b->entity_membership.count != start + count ||
        !mc_world_runtime_entity_list_valid(b))
        return false;
    for (size_t i = 0; i < count; ++i)
        if (b->fields.b->items[start + i] != entities[i] ||
            b->entity_membership.entries[start + i] != entities[i])
            return false;
    return true;
}

static bool ax_case(ax_fixture *s, const char *name, bool alias, size_t slice,
                    mc_world_storage_status callback, bool throwable, bool remove_self,
                    bool null_error, bool saturate, bool bind_fault) {
    bool ok = false;
    ax_marker(name, "BEGIN");
    fixture *f = s->base;
    mc_world_runtime *b = f->worlds[2], *a = alias ? b : f->worlds[0];
    ax_observer *r = &s->observer;
    const size_t a_before = a->persistent_bindings, b_before = b->persistent_bindings;
    const size_t prefix = b->fields.b->count;
    const int32_t mod_before = b->fields.b->mod_count;
    const bool q_before = f->states[2]->dirty;
    const mc_java_random random_before = *b->fields.r;
    size_t count = 0;
    CHECK(mc_world_chunk_entity_count(f->slices[2][slice], &count) == MC_WORLD_STORAGE_OK &&
          count <= 3);
    mc_entity *actual[3] = {0};
    for (size_t i = 0; i < count; ++i)
        CHECK(mc_world_chunk_entity_at(f->slices[2][slice], i, &actual[i]) == MC_WORLD_STORAGE_OK);
    CHECK(r->receipt == NULL && ax_start(s, a));
    r->a = a;
    r->b = b;
    r->execution = s->execution;
    r->count = count;
    r->prefix = prefix;
    r->calls = 0;
    r->node = NULL;
    r->a_bindings = a_before + 1u + (alias ? 1u : 0u);
    r->b_bindings = b_before + 1u + (alias ? 1u : 0u);
    r->returned = callback;
    r->throwable = throwable;
    r->remove_self = remove_self;
    r->removed = false;
    r->observations_ok = true;
    r->armed = false;
    r->saturate = saturate;
    r->bind_fault = bind_fault;
    for (size_t i = 0; i < count; ++i)
        r->expected[i] = actual[i];
    CHECK(mc_world_runtime_add_world_access(b, r->self, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_storage_status status = mc_world_runtime_register_chunk_entities_in_execution(
        s->execution, f->chunks[2], slice, ax_site, null_error ? NULL : s->error);
    ax_restore_native_faults(s); /* BEFORE every assertion after projected Source return. */
    const size_t expected_calls = count == 0                                       ? 0
                                  : callback != MC_WORLD_STORAGE_OK || remove_self ? 1
                                                                                   : count;
    CHECK(r->calls == expected_calls && r->observations_ok && (count == 0 || r->armed));
    CHECK(ax_prefix(s, prefix, actual, count) &&
          b->fields.b->mod_count == mc_java_i32_add(mod_before, 1) &&
          f->states[2]->dirty == q_before &&
          mc_test_java_random_equal(&random_before, b->fields.r) && b->active_calls == 0 &&
          a->active_calls == 0 && files_unchanged(f, 2));
    mc_world_chunk_fields fields;
    CHECK(mc_world_chunk_fields_get(f->chunks[2], &fields) == MC_WORLD_STORAGE_OK && fields.d == b);
    CHECK(!remove_self || (r->removed && r->free_during == MC_WORLD_STORAGE_BUSY));
    if (bind_fault) {
        CHECK(count == 1 && s->execution->world_open && s->execution->world == b &&
              s->execution->current_lease == 1 &&
              s->execution->cleanup.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        if (callback != MC_WORLD_STORAGE_OK) {
            /* The temporary zero native binding count refused VALUE capture.
             * The real in-dispatch handoff still protects this exact receipt.
             * Inspect BEFORE the pending pop retry clears admission. */
            const mc_world_access_failure *protected = NULL;
            CHECK(s->execution->receipt_count == 0 && !s->execution->capture_pending &&
                  s->execution->first.status == MC_WORLD_STORAGE_OK &&
                  s->execution->capture.status == MC_WORLD_STORAGE_OK &&
                  s->execution->admission.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  status == callback && s->error->cause.status == r->callback_cause.status &&
                  s->error->cause.operation == r->callback_cause.operation &&
                  s->error->cause.system_error == r->callback_cause.system_error &&
                  mc_world_access_failure_lease_inspect(r->node, &protected) &&
                  protected->receipt == r->receipt && protected->receiver == r->self &&
                  r->self->active_holds == 1);
            protected = NULL;
        }
        mc_entity_tick_execution_world_token token;
        CHECK(mc_entity_tick_execution_current_world(s->execution, &token));
        CHECK(mc_entity_tick_execution_pop_world(
                  s->execution, &token, callback != MC_WORLD_STORAGE_OK) == MC_WORLD_STORAGE_OK);
    }
    CHECK(s->execution->world_open && s->execution->world == a && s->execution->current_lease == 0);
    if (callback == MC_WORLD_STORAGE_OK) {
        CHECK(status == (bind_fault ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : MC_WORLD_STORAGE_OK) &&
              b->persistent_bindings == b_before + (alias ? 1u : 0u) &&
              a->persistent_bindings == a_before + 1u && r->receipt == NULL &&
              s->execution->world_count == 1 && s->execution->lease_count == 1);
        if (bind_fault)
            CHECK(s->error->bytecode == 0 && s->error->world_bytecode == 0 &&
                  s->error->cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        CHECK(mc_entity_tick_execution_end_world(s->execution, false) == MC_WORLD_STORAGE_OK);
    } else {
        CHECK(status == callback && r->receipt != NULL && r->receipt->holds == 1 &&
              r->self->active_holds == 1 && s->execution->lease_count == 2 &&
              r->node == &s->execution->leases[1]);
        if (!bind_fault)
            CHECK(s->execution->receipt_count == 1 &&
                  s->execution->admission.status == MC_WORLD_STORAGE_OK);
        CHECK(!saturate || r->projected_receipt_holds == SIZE_MAX);
        if (!null_error)
            CHECK(s->error->status == callback && s->error->bytecode == 42 &&
                  s->error->world_bytecode == 34 && s->error->batch.index == 0 &&
                  s->error->batch.publication.world_access_bytecode == 29 &&
                  s->error->batch.publication.world_access_failure.receipt == r->receipt &&
                  s->error->cause.system_error == 91);
        const mc_world_access_failure *view = NULL;
        const char *exception = NULL;
        CHECK(mc_world_access_failure_lease_inspect(r->node, &view) &&
              view->receipt == r->receipt && view->receiver == r->self &&
              view->render_global == NULL &&
              mc_world_access_failure_source_exception(view, &exception) == throwable);
        CHECK(!throwable ||
              (exception != NULL && strcmp(exception, "java/lang/IllegalStateException") == 0));
        CHECK(mc_entity_tick_execution_end_world(s->execution, true) == MC_WORLD_STORAGE_OK &&
              s->execution->lease_count == 1 && s->execution->current_lease == SIZE_MAX &&
              mc_world_access_failure_lease_inspect(r->node, &view) && view->receipt == r->receipt);
        mc_world_runtime *retained = NULL;
        CHECK(s->execution->world_count == 2 &&
              mc_entity_tick_execution_read_world(s->execution, &r->token, &retained) &&
              retained == b && a->persistent_bindings == r->a_bindings &&
              b->persistent_bindings == r->b_bindings);
        CHECK(mc_world_access_receipt_retire(&r->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
              mc_world_access_free(r->self, NULL) == MC_WORLD_STORAGE_BUSY);
        view = NULL;
    }
    const mc_world_storage_error first = null_error ? (mc_world_storage_error){0} : s->error->cause;
    CHECK(ax_unsubscribe(s) && ax_clear_and_retire(s) && a->persistent_bindings == a_before &&
          b->persistent_bindings == b_before &&
          mc_world_access_receipt_retire(&r->receipt, NULL) == MC_WORLD_STORAGE_OK &&
          r->receipt == NULL && r->self->active_holds == 0 && r->calls == expected_calls);
    CHECK(null_error ||
          (s->error->cause.status == first.status && s->error->cause.operation == first.operation &&
           s->error->cause.system_error == first.system_error));
    ++s->cases;
    ax_marker(name, "END");
    ok = true;
cleanup:
    ax_restore_native_faults(s);
    return ok;
}

static bool ax_native_null_context(ax_fixture *s) {
    bool ok = false;
    const char *name = "native-null-context";
    ax_marker(name, "BEGIN");
    mc_world_runtime *b = s->base->worlds[2];
    const size_t count = b->fields.b->count, bindings = b->persistent_bindings;
    const int32_t mod = b->fields.b->mod_count;
    const size_t calls = s->observer.calls;
    CHECK(mc_world_runtime_register_chunk_entities_in_execution(NULL, s->base->chunks[2], 4,
                                                                ax_site, s->error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          s->error->bytecode == 0 && s->error->world_bytecode == 0 && b->fields.b->count == count &&
          b->fields.b->mod_count == mod && b->persistent_bindings == bindings &&
          b->active_calls == 0 && s->observer.calls == calls);
    ++s->cases;
    ax_marker(name, "END");
    ok = true;
cleanup:
    return ok;
}

static bool ax_renderer(ax_fixture *s) {
    bool ok = false;
    const char *name = "actual-renderer-failure";
    ax_marker(name, "BEGIN");
    fixture *f = s->base;
    mc_world_runtime *a = f->worlds[0], *b = f->worlds[2];
    CHECK(b->fields.u->count == 0 &&
          mc_registry_classes_initialize_statistics(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &s->sessions,
                                           NULL) == MC_PLAYER_OK);
    const uint16_t name_units[] = {'A', 'c', 't', 'i', 'v', 'a', 't', 'i', 'o', 'n'};
    CHECK(mc_player_string_create(name_units, COUNT(name_units), &s->username) == MC_PLAYER_OK &&
          mc_player_session_create(s->sessions, s->username, NULL, &s->session, NULL) ==
              MC_PLAYER_OK &&
          mc_local_player_allocate(f->entity_classes[0], &s->player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(s->player, b, NULL, s->session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          mc_entity_set_position(s->player, 6.0, 90.0, 7.0) == MC_ENTITY_OK);
    mc_world_chunk_entity_error inserted = {0};
    CHECK(mc_world_chunk_insert_entity(f->chunks[2], s->player, &inserted) == MC_WORLD_STORAGE_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &s->render_classes) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_global_allocate(s->render_classes, NULL, &s->renderer,
                                           s->render_error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_create(s->renderer, &s->renderer->access, s->render_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    const size_t a_before = a->persistent_bindings, b_before = b->persistent_bindings;
    CHECK(mc_render_global_access_source_add(s->renderer->access, b, s->render_error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    mc_world_access *identity = b->world_accesses.entries[0];
    const size_t prefix = b->fields.b->count;
    CHECK(ax_start(s, a));
    CHECK(mc_world_runtime_register_chunk_entities_in_execution(s->execution, f->chunks[2], 5,
                                                                ax_site, s->error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          s->error->bytecode == 42 && s->error->world_bytecode == 34 &&
          s->error->batch.index == 0 && b->fields.b->count == prefix + 1 &&
          b->fields.b->items[prefix] == s->player && b->fields.d->count == 0 &&
          s->execution->world == a && s->execution->current_lease == 0 &&
          b->persistent_bindings == b_before + 2 && a->persistent_bindings == a_before + 1 &&
          identity->active_holds == 1 && mc_entity_cloak_url(s->player) != NULL);
    const mc_world_access_failure *view = NULL;
    CHECK(mc_world_access_failure_lease_inspect(&s->execution->leases[1], &view) &&
          view->receipt == s->error->batch.publication.world_access_failure.receipt &&
          view->receiver == identity && view->receipt->holds == 1 && view->render_global != NULL &&
          view->render_global->source_pc == 26 &&
          view->render_global->status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE);
    mc_render_global_callback_state callback;
    mc_client_image_buffer_state processor;
    CHECK(mc_render_global_access_callback_inspect(s->renderer->access, view, &callback) &&
          callback.processors[0] != NULL && callback.processors[1] == NULL &&
          callback.images[0] == NULL &&
          mc_client_image_buffer_inspect(callback.processors[0], &processor) &&
          processor.native_holds == 1 && processor.last_source_pc == 4);
    const char *exception = NULL;
    CHECK(mc_world_access_failure_source_exception(view, &exception) && exception != NULL &&
          strcmp(exception, "java/lang/NullPointerException") == 0);
    const mc_world_storage_error first = s->error->cause;
    CHECK(mc_entity_tick_execution_end_world(s->execution, true) == MC_WORLD_STORAGE_OK &&
          s->execution->lease_count == 1 &&
          mc_render_global_access_detach_all(s->renderer->access, s->render_error) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          b->fields.u->count == 1 && identity->active_holds == 1 &&
          b->persistent_bindings == b_before + 2);
    CHECK(mc_render_global_access_callback_inspect(s->renderer->access, view, &callback) &&
          mc_client_image_buffer_inspect(callback.processors[0], &processor) &&
          processor.native_holds == 1);
    s->error->batch.publication.world_access_failure = (mc_world_access_failure){0};
    view = NULL;
    callback = (mc_render_global_callback_state){0};
    processor = (mc_client_image_buffer_state){0};
    CHECK(mc_entity_tick_execution_release_receipts(s->execution) == MC_WORLD_STORAGE_OK &&
          s->execution->lease_count == 0 && identity->active_holds == 0);
    CHECK(mc_render_global_access_detach_all(s->renderer->access, s->render_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          b->fields.u->count == 0 && b->persistent_bindings == b_before + 1);
    identity = NULL;
    CHECK(mc_render_global_access_retire(&s->renderer->access, s->render_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          ax_clear_and_retire(s) && a->persistent_bindings == a_before &&
          b->persistent_bindings == b_before && s->error->cause.status == first.status &&
          s->error->cause.operation == first.operation &&
          s->error->cause.system_error == first.system_error && files_unchanged(f, 2));
    ++s->cases;
    ax_marker(name, "END");
    ok = true;
cleanup:
    return ok;
}

static bool ax_setup(ax_fixture *s, const char *root) {
    bool ok = false;
    char *path = NULL;
    fixture *f = s->base;
    const uint16_t title[] = {'A', 'c', 't', 'i', 'v', 'a', 't', 'i', 'o', 'n'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK &&
          mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK &&
              mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
                  MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
    }
    CHECK(open_store(f, 0, root) && create_world(f, 0) && open_store(f, 2, root) &&
          create_world(f, 2));
    /* Capture the existing authored region bytes without a source save. The
     * inherited comparison/cleanup helpers own this independent file buffer. */
    CHECK(region_path(f, 2, &path) &&
          mc_ws_read_file(path, 1024u * 1024u, &f->saved[2], NULL) == MC_WORLD_STORAGE_OK &&
          f->saved[2].data != NULL);
    free(path);
    path = NULL;
    CHECK(make_entity(f, 0, f->worlds[2], 2, 70, 3, &s->items[0]) &&
          make_entity(f, 0, f->worlds[2], 4, 70, 5, &s->items[1]) &&
          make_entity(f, 0, f->worlds[2], 6, 100, 7, &s->items[2]));
    mc_world_chunk_entity_error inserted = {0};
    CHECK(
        mc_world_chunk_insert_entity(f->chunks[2], s->items[0], &inserted) == MC_WORLD_STORAGE_OK &&
        mc_world_chunk_insert_entity(f->chunks[2], s->items[1], &inserted) == MC_WORLD_STORAGE_OK &&
        mc_world_chunk_insert_entity(f->chunks[2], s->items[0], &inserted) == MC_WORLD_STORAGE_OK &&
        mc_world_chunk_insert_entity(f->chunks[2], s->items[2], &inserted) == MC_WORLD_STORAGE_OK);
    const mc_world_access_source_callbacks callbacks = {.entity_added = ax_added};
    CHECK(mc_world_access_source_create(&callbacks, &s->observer, &s->observer.self, NULL) ==
          MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(path);
    return ok;
}

static bool ax_cleanup(ax_fixture *s) {
    if (s == NULL)
        return true;
    ax_restore_native_faults(s);
    if (!ax_clear_and_retire(s) || !ax_unsubscribe(s) ||
        mc_world_access_receipt_retire(&s->observer.receipt, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (mc_world_access_free(s->observer.self, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    s->observer.self = NULL;
    if (s->renderer != NULL && s->renderer->access != NULL &&
        (mc_render_global_access_detach_all(s->renderer->access, s->render_error) !=
             MC_CLIENT_RENDER_GLOBAL_OK ||
         mc_render_global_access_retire(&s->renderer->access, s->render_error) !=
             MC_CLIENT_RENDER_GLOBAL_OK))
        return false;
    if (s->renderer != NULL && mc_client_render_global_release(&s->renderer, s->render_error) !=
                                   MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    if (s->render_classes != NULL &&
        mc_client_render_engine_classes_release(s->render_classes) != MC_CLIENT_RENDER_ENGINE_OK)
        return false;
    s->render_classes = NULL;
    if (s->base != NULL)
        for (size_t i = 0; i < WORLD_COUNT; ++i)
            if (s->base->worlds[i] != NULL && s->base->bound[i] && !s->base->terminal[i]) {
                if (mc_world_runtime_retire_entity_containers(s->base->worlds[i], NULL) !=
                    MC_WORLD_STORAGE_OK)
                    return false;
                s->base->terminal[i] = true;
            }
    mc_entity_release(s->player);
    s->player = NULL;
    mc_player_session_retire(&s->session);
    mc_player_string_release(s->username);
    s->username = NULL;
    if (mc_player_session_classes_free(&s->sessions, NULL) != MC_PLAYER_OK ||
        !cleanup_fixture(s->base))
        return false;
    free(s->error);
    free(s->render_error);
    free(s);
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    ax_fixture *s = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "activation-owned") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    puts("ACTIVATION_OWNER_BEGIN schema=1");
    fflush(stdout);
    s = calloc(1, sizeof(*s));
    CHECK(s != NULL);
    s->base = calloc(1, sizeof(*s->base));
    s->error = calloc(1, sizeof(*s->error));
    s->render_error = calloc(1, sizeof(*s->render_error));
    CHECK(s->base != NULL && s->error != NULL && s->render_error != NULL && ax_setup(s, root));
    CHECK(ax_case(s, "empty-distinct", false, 0, MC_WORLD_STORAGE_OK, false, false, false, false,
                  false));
    CHECK(ax_case(s, "success-distinct", false, 4, MC_WORLD_STORAGE_OK, false, false, false, false,
                  false));
    CHECK(ax_case(s, "success-alias", true, 4, MC_WORLD_STORAGE_OK, false, false, false, false,
                  false));
    CHECK(ax_case(s, "live-self-remove-success", false, 4, MC_WORLD_STORAGE_OK, false, true, false,
                  false, false));
    CHECK(ax_case(s, "failure-source-distinct", false, 4, MC_WORLD_STORAGE_IO_ERROR, true, true,
                  false, false, false));
    CHECK(ax_case(s, "failure-native-limit", false, 4, MC_WORLD_STORAGE_LIMIT, false, true, false,
                  false, false));
    CHECK(ax_case(s, "failure-native-allocation-alias", true, 4, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                  false, true, false, false, false));
    CHECK(ax_case(s, "failure-nullout-saturated", false, 4, MC_WORLD_STORAGE_IO_ERROR, true, true,
                  true, true, false));
    CHECK(ax_case(s, "failure-pop-refusal", false, 6, MC_WORLD_STORAGE_IO_ERROR, true, true, false,
                  false, true));
    CHECK(ax_case(s, "success-pop-refusal", false, 6, MC_WORLD_STORAGE_OK, false, false, false,
                  false, true));
    CHECK(ax_native_null_context(s) && ax_renderer(s));
    CHECK(s->cases == 12);
    puts("ACTIVATION_OWNER_OPEN null-Chunk.d/slice-order dedicated full-A-query kernel TileEntity "
         "SL");
    fflush(stdout);
    result = 0;
cleanup:
    if (!ax_cleanup(s)) {
        fputs("ACTIVATION_OWNER_CLEANUP_REFUSED remaining real owners retained\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("PASS: %u actual owned activation checks;12 component cases\n", checks);
    return result;
}
