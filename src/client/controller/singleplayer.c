#include "singleplayer_internal.h"

#include "../../entity/entity_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../application_state.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/entities.h"

#include <stdlib.h>
#include <string.h>

#ifdef MC_SINGLEPLAYER_CONTROLLER_TEST_ALLOCATOR
extern void *mc_singleplayer_controller_test_calloc(size_t count, size_t size);
extern void mc_singleplayer_controller_test_free(void *memory);
#define controller_calloc mc_singleplayer_controller_test_calloc
#define controller_free mc_singleplayer_controller_test_free
#else
#define controller_calloc calloc
#define controller_free free
#endif

static mc_singleplayer_controller_status fail(mc_singleplayer_controller_error *e,
                                              mc_singleplayer_controller_status status,
                                              const char *operation, const char *source,
                                              const char *method, const char *descriptor,
                                              unsigned int pc) {
    e->status = status;
    e->operation = operation;
    e->site = (mc_player_site){source, method, descriptor, pc};
    return status;
}

static mc_singleplayer_controller_error *frame(mc_singleplayer_controller_error *out) {
    mc_singleplayer_controller_error *e = controller_calloc(1, sizeof(*e));
    if (e == NULL && out != NULL) {
        memset(out, 0, sizeof(*out));
        out->status = MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED;
        out->operation = "single-player controller diagnostic allocation";
    }
    return e;
}

static mc_singleplayer_controller_status publish(mc_singleplayer_controller_error *e,
                                                 mc_singleplayer_controller_error *out) {
    const mc_singleplayer_controller_status status = e->status;
    if (out != NULL)
        *out = *e;
    controller_free(e);
    return status;
}

static mc_singleplayer_controller_status enter_owner(mc_client_singleplayer_controller *c) {
    if (c == NULL)
        return MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT;
    if (c->fields.entered)
        return MC_SINGLEPLAYER_CONTROLLER_BUSY;
    if (c->fields.phase != MC_SINGLEPLAYER_CONTROLLER_READY)
        return MC_SINGLEPLAYER_CONTROLLER_INVALID_PHASE;
    c->fields.entered = true;
    return MC_SINGLEPLAYER_CONTROLLER_OK;
}

static const char *admission_operation(mc_singleplayer_controller_status status) {
    switch (status) {
        case MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT:
            return "single-player controller owner";
        case MC_SINGLEPLAYER_CONTROLLER_BUSY:
            return "single-player controller entered operation";
        case MC_SINGLEPLAYER_CONTROLLER_INVALID_PHASE:
            return "single-player controller READY phase";
        default:
            return NULL;
    }
}

/* Cleanup has no failing source descendant and needs no temporary frame.
 * Clear caller output directly; no large automatic object or allocation is
 * introduced when releasing the memory that caused a native failure. */
static mc_singleplayer_controller_status simple_result(mc_singleplayer_controller_error *out,
                                                       mc_singleplayer_controller_status status,
                                                       const char *operation) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->status = status;
        out->operation = operation;
    }
    return status;
}

static bool enter(mc_client_singleplayer_controller *c, mc_singleplayer_controller_error *e) {
    const mc_singleplayer_controller_status status = enter_owner(c);
    if (status == MC_SINGLEPLAYER_CONTROLLER_OK)
        return true;
    fail(e, status, admission_operation(status), NULL, NULL, NULL, 0);
    return false;
}

mc_singleplayer_controller_status mc_client_singleplayer_controller_allocate(
    mc_entity_classes *classes, mc_java_random_profile *profile, size_t max_bytes,
    mc_client_singleplayer_controller **out, mc_singleplayer_controller_error *out_error) {
    mc_singleplayer_controller_error *e = frame(out_error);
    if (e == NULL)
        return MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED;
    if (classes == NULL || profile == NULL || out == NULL)
        fail(e, MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT, "single-player controller allocation",
             NULL, NULL, NULL, 0);
    else if ((max_bytes == 0 ? 1024u * 1024u : max_bytes) <
             sizeof(mc_client_singleplayer_controller))
        fail(e, MC_SINGLEPLAYER_CONTROLLER_LIMIT, "single-player controller owner budget", NULL,
             NULL, NULL, 0);
    else {
        e->entity.status = mc_entity_classes_retain(classes, MC_ENTITY_CLIENT, profile);
        if (e->entity.status != MC_ENTITY_OK)
            fail(e, MC_SINGLEPLAYER_CONTROLLER_ENTITY_FAILED,
                 "single-player controller exact client class binding", NULL, NULL, NULL, 0);
        else {
            mc_client_singleplayer_controller *c = controller_calloc(1, sizeof(*c));
            if (c == NULL) {
                (void)mc_entity_classes_release(classes);
                fail(e, MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED,
                     "single-player controller owner allocation", NULL, NULL, NULL, 0);
            } else {
                c->classes = classes;
                c->profile = profile;
                c->fields.native_holds = 1;
                *out = c;
            }
        }
    }
    return publish(e, out_error);
}

mc_singleplayer_controller_status
mc_client_singleplayer_controller_construct(mc_client_singleplayer_controller *c,
                                            client_application *app,
                                            mc_singleplayer_controller_error *out) {
    mc_singleplayer_controller_error *e = frame(out);
    if (e == NULL)
        return MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED;
    if (c == NULL)
        fail(e, MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT, "single-player controller owner", NULL,
             NULL, NULL, 0);
    else if (c->fields.entered)
        fail(e, MC_SINGLEPLAYER_CONTROLLER_BUSY, "single-player controller entered constructor",
             NULL, NULL, NULL, 0);
    else if (c->fields.phase != MC_SINGLEPLAYER_CONTROLLER_RAW)
        fail(e, MC_SINGLEPLAYER_CONTROLLER_INVALID_PHASE, "single-player controller RAW phase",
             NULL, NULL, NULL, 0);
    else {
        c->fields.b = false;
        c->fields.base_constructor_pc = 6;
        c->fields.a = app;
        c->fields.base_constructor_pc = 14;
        c->fields.c = -1;
        c->fields.constructor_pc = 7;
        c->fields.d = -1;
        c->fields.constructor_pc = 12;
        c->fields.e = -1;
        c->fields.constructor_pc = 17;
        c->fields.f = 0.0f;
        c->fields.constructor_pc = 22;
        c->fields.g = 0.0f;
        c->fields.constructor_pc = 27;
        c->fields.h = 0.0f;
        c->fields.constructor_pc = 32;
        c->fields.i = 0;
        c->fields.constructor_pc = 40;
        c->fields.phase = MC_SINGLEPLAYER_CONTROLLER_READY;
    }
    return publish(e, out);
}

bool mc_client_singleplayer_controller_read(const mc_client_singleplayer_controller *c,
                                            mc_singleplayer_controller_snapshot *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->fields;
    return true;
}

mc_singleplayer_controller_status
mc_client_singleplayer_controller_change_world(mc_client_singleplayer_controller *c,
                                               mc_world_runtime *world,
                                               mc_singleplayer_controller_error *out) {
    mc_singleplayer_controller_error *e = frame(out);
    if (e == NULL)
        return MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED;
    (void)world; /* Actual os.a(fd) invokes literal-return ob.a(fd). */
    if (enter(c, e))
        c->fields.entered = false;
    return publish(e, out);
}

mc_singleplayer_controller_status
mc_client_singleplayer_controller_finish_player(mc_client_singleplayer_controller *c,
                                                mc_entity *player,
                                                mc_singleplayer_controller_error *out) {
    mc_singleplayer_controller_error *e = frame(out);
    if (e == NULL)
        return MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED;
    (void)player; /* Actual inherited ob.b(gs) is literal return. */
    if (enter(c, e))
        c->fields.entered = false;
    return publish(e, out);
}

mc_singleplayer_controller_status mc_client_singleplayer_controller_create_player(
    mc_client_singleplayer_controller *c, mc_world_runtime *world,
    const mc_inventory_limits *inventory, mc_entity **out_player,
    mc_singleplayer_controller_error *out) {
    mc_singleplayer_controller_error *e = frame(out);
    if (e == NULL)
        return MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED;
    if (!enter(c, e))
        return publish(e, out);
    bool world_held = false;
    mc_player_session *held_session = NULL;
    if (out_player == NULL)
        fail(e, MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT, "controller Player output", NULL, NULL,
             NULL, 0);
    else if (c->fields.failed_player != NULL)
        fail(e, MC_SINGLEPLAYER_CONTROLLER_BUSY, "controller failed Player awaits inspection", NULL,
             NULL, NULL, 0);
    else if (world != NULL && (mc_world_runtime_entities(world) != c->classes ||
                               mc_world_runtime_random_profile(world) != c->profile))
        fail(e, MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT, "controller exact Player World", NULL,
             NULL, NULL, 0);
    else if (world != NULL && !(world_held = mc_world_runtime_binding_retain(world)))
        fail(e, MC_SINGLEPLAYER_CONTROLLER_WORLD_FAILED, "controller Player World hold", NULL, NULL,
             NULL, 0);
    else {
        mc_entity *candidate = NULL;
        e->entity.status = mc_local_player_allocate(c->classes, &candidate, &e->entity);
        if (e->entity.status != MC_ENTITY_OK)
            fail(e, MC_SINGLEPLAYER_CONTROLLER_ENTITY_FAILED, "controller new dc allocation", "ob",
                 "b", "(Lfd;)Lgs;", 0);
        else {
            c->fields.failed_player = candidate;
            client_application *app = c->fields.a;
            if (app == NULL)
                fail(e, MC_SINGLEPLAYER_CONTROLLER_NULL_REFERENCE, "controller null App.k", "ob",
                     "b", "(Lfd;)Lgs;", 13);
            else {
                mc_player_session *session = app->session; /* Actual App.k read13. */
                if (session != NULL &&
                    (e->player.status = mc_player_session_retain(session)) != MC_PLAYER_OK)
                    fail(e, MC_SINGLEPLAYER_CONTROLLER_LIMIT, "controller captured Session hold",
                         "ob", "b", "(Lfd;)Lgs;", 13);
                else if (session != NULL)
                    held_session = session;
                if (e->status != MC_SINGLEPLAYER_CONTROLLER_OK) {
                    /* Preserve the allocated RAW prefix after native hold failure. */
                } else if (world == NULL)
                    fail(e, MC_SINGLEPLAYER_CONTROLLER_NULL_REFERENCE, "controller null World.t",
                         "ob", "b", "(Lfd;)Lgs;", 17);
                else {
                    mc_world_runtime_fields fields;
                    e->world = mc_world_runtime_fields_get(world, &fields);
                    if (e->world != MC_WORLD_STORAGE_OK)
                        fail(e, MC_SINGLEPLAYER_CONTROLLER_WORLD_FAILED,
                             "controller actual World.t fields", "ob", "b", "(Lfd;)Lgs;", 17);
                    else if (fields.t == NULL)
                        fail(e, MC_SINGLEPLAYER_CONTROLLER_NULL_REFERENCE,
                             "controller null provider.g", "ob", "b", "(Lfd;)Lgs;", 20);
                    else {
                        mc_world_provider_fields provider;
                        e->provider = mc_world_provider_fields_get(fields.t, &provider);
                        if (e->provider != MC_WORLD_PROVIDER_OK)
                            fail(e, MC_SINGLEPLAYER_CONTROLLER_WORLD_FAILED,
                                 "controller actual provider.g field", "ob", "b", "(Lfd;)Lgs;", 20);
                        else if (mc_local_player_construct(candidate, world, app, session,
                                                           provider.g, inventory,
                                                           &e->player) != MC_PLAYER_OK)
                            fail(e, MC_SINGLEPLAYER_CONTROLLER_PLAYER_FAILED,
                                 "controller actual dc constructor", "ob", "b", "(Lfd;)Lgs;", 23);
                        else {
                            c->fields.failed_player = NULL;
                            *out_player = candidate; /* Actual areturn26, one caller hold. */
                        }
                    }
                }
            }
        }
    }
    mc_player_session_retire(&held_session);
    if (world_held)
        mc_world_runtime_binding_release(world);
    c->fields.entered = false;
    return publish(e, out);
}

mc_singleplayer_controller_status
mc_client_singleplayer_controller_clear_failed_player(mc_client_singleplayer_controller *c,
                                                      mc_singleplayer_controller_error *out) {
    const mc_singleplayer_controller_status status = enter_owner(c);
    if (status == MC_SINGLEPLAYER_CONTROLLER_OK) {
        mc_entity *previous = c->fields.failed_player;
        c->fields.failed_player = NULL;
        mc_entity_release(previous);
        c->fields.entered = false;
    }
    return simple_result(out, status, admission_operation(status));
}

mc_singleplayer_controller_status
mc_client_singleplayer_controller_prepare_player(mc_client_singleplayer_controller *c,
                                                 mc_entity *player,
                                                 mc_singleplayer_controller_error *out) {
    mc_singleplayer_controller_error *e = frame(out);
    if (e == NULL)
        return MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED;
    if (enter(c, e)) {
        if (player == NULL)
            fail(e, MC_SINGLEPLAYER_CONTROLLER_NULL_REFERENCE, "controller null yaw receiver", "os",
                 "a", "(Lgs;)V", 3);
        else if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
                 player->classes != c->classes)
            fail(e, MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT,
                 "controller canonical LocalPlayer yaw receiver", NULL, NULL, NULL, 0);
        else {
            e->entity.status = mc_entity_retain(player);
            if (e->entity.status != MC_ENTITY_OK)
                fail(e, MC_SINGLEPLAYER_CONTROLLER_ENTITY_FAILED, "controller yaw Player hold",
                     NULL, NULL, NULL, 0);
            else {
                player->base.yaw = -180.0f; /* Actual os.a(gs) putfield3, and no aU store. */
                mc_entity_release(player);
            }
        }
        c->fields.entered = false;
    }
    return publish(e, out);
}

mc_singleplayer_controller_status
mc_client_singleplayer_controller_retain(mc_client_singleplayer_controller *c) {
    if (c == NULL)
        return MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT;
    if (c->fields.native_holds == SIZE_MAX)
        return MC_SINGLEPLAYER_CONTROLLER_LIMIT;
    ++c->fields.native_holds;
    return MC_SINGLEPLAYER_CONTROLLER_OK;
}

mc_singleplayer_controller_status
mc_client_singleplayer_controller_release(mc_client_singleplayer_controller *c) {
    if (c == NULL || c->fields.native_holds <= 1)
        return MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT;
    --c->fields.native_holds;
    return MC_SINGLEPLAYER_CONTROLLER_OK;
}

mc_singleplayer_controller_status
mc_client_singleplayer_controller_free(mc_client_singleplayer_controller **owner,
                                       mc_singleplayer_controller_error *out) {
    if (owner == NULL)
        return simple_result(out, MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT,
                             "controller retirement owner");
    if (*owner != NULL) {
        mc_client_singleplayer_controller *c = *owner;
        if (c->fields.entered || c->fields.native_holds != 1)
            return simple_result(out, MC_SINGLEPLAYER_CONTROLLER_BUSY,
                                 "controller retained retirement");
        mc_entity_release(c->fields.failed_player);
        (void)mc_entity_classes_release(c->classes);
        controller_free(c);
        *owner = NULL;
    }
    return simple_result(out, MC_SINGLEPLAYER_CONTROLLER_OK, NULL);
}
