#include "player_internal.h"

#include "mc/block/registry/runtime.h"

#include <stdlib.h>

typedef struct creative_list {
    const mc_block *items[28];
    size_t count;
} creative_list;

struct mc_player_session_classes {
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_player_session_phase phase;
    unsigned int pc;
    creative_list *creative;
    size_t live_sessions;
    mc_player_error failure;
};

mc_player_status mc_player_session_classes_create(mc_entity_classes *entities,
                                                  mc_registry_classes *registry,
                                                  mc_player_session_classes **out,
                                                  mc_player_error *error) {
    mc_player_error_clear(error);
    if (entities == NULL || registry == NULL || out == NULL || entities->source != MC_ENTITY_CLIENT)
        return mc_player_fail(error, MC_PLAYER_INVALID_ARGUMENT, "Session class native arguments",
                              NULL, NULL, NULL, 0);
    mc_entity_status es = mc_entity_classes_retain(entities, MC_ENTITY_CLIENT, entities->profile);
    if (es != MC_ENTITY_OK) {
        if (error != NULL)
            error->entity.status = es;
        return mc_player_fail(error, MC_PLAYER_ENTITY_FAILED, "Session Entity class binding", NULL,
                              NULL, NULL, 0);
    }
    mc_registry_error cause;
    const mc_registry_status rs =
        mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT, entities->profile, &cause);
    if (rs != MC_REGISTRY_OK) {
        (void)mc_entity_classes_release(entities);
        if (error != NULL)
            error->registry = cause;
        return mc_player_fail(error, MC_PLAYER_REGISTRY_FAILED, "Session Registry class binding",
                              NULL, NULL, NULL, 0);
    }
    mc_player_session_classes *classes = calloc(1, sizeof(*classes));
    if (classes == NULL) {
        (void)mc_registry_classes_release(registry, NULL);
        (void)mc_entity_classes_release(entities);
        return mc_player_fail(error, MC_PLAYER_ALLOCATION_FAILED, "Session class allocation", NULL,
                              NULL, NULL, 0);
    }
    classes->entities = entities;
    classes->registry = registry;
    *out = classes;
    return MC_PLAYER_OK;
}

static mc_player_status initialize(mc_player_session_classes *classes, mc_player_error *out) {
    if (classes->phase == MC_PLAYER_SESSION_READY)
        return MC_PLAYER_OK;
    if (classes->phase == MC_PLAYER_SESSION_FAILED) {
        if (out != NULL)
            *out = classes->failure;
        return classes->failure.status;
    }
    if (classes->phase != MC_PLAYER_SESSION_COLD)
        return mc_player_fail(out, MC_PLAYER_BUSY, "Session class entered initialization", "gr",
                              "<clinit>", "()V", classes->pc);
    classes->phase = MC_PLAYER_SESSION_INITIALIZING;
    mc_player_error error = {0};
    creative_list *list = calloc(1, sizeof(*list));
    if (list == NULL) {
        mc_player_fail(&error, MC_PLAYER_ALLOCATION_FAILED, "Session creative List allocation",
                       "gr", "<clinit>", "()V", 0);
        goto failed;
    }
    classes->creative = list;
    classes->pc = 7;
    static const char *const names[] = {"u",  "x", "am", "w",  "y",  "K",  "L",  "ar", "al", "N",
                                        "ap", "z", "ae", "af", "ag", "ah", "F",  "G",  "M",  "ac",
                                        "J",  "I", "H",  "aj", "ai", "ao", "an", "aq"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        const mc_block *block = NULL;
        if (mc_registry_block_static_get(classes->registry, names[i], &block, &error.registry) !=
            MC_REGISTRY_OK) {
            mc_player_fail(&error, MC_PLAYER_REGISTRY_FAILED, "Session Block static active use",
                           "gr", "<clinit>", "()V", 13u + (unsigned int)i * 12u);
            goto failed;
        }
        list->items[list->count++] = block;
        classes->pc = 21u + (unsigned int)i * 12u;
    }
    classes->pc = 346;
    classes->phase = MC_PLAYER_SESSION_READY;
    return MC_PLAYER_OK;
failed:
    classes->failure = error;
    classes->phase = MC_PLAYER_SESSION_FAILED;
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_player_status mc_player_session_create(mc_player_session_classes *classes,
                                          mc_player_string *username, mc_player_string *session_id,
                                          mc_player_session **out, mc_player_error *error) {
    mc_player_error_clear(error);
    if (classes == NULL || out == NULL)
        return mc_player_fail(error, MC_PLAYER_INVALID_ARGUMENT, "Session native arguments", NULL,
                              NULL, NULL, 0);
    mc_player_status status = initialize(classes, error);
    if (status != MC_PLAYER_OK)
        return status;
    if (classes->live_sessions == SIZE_MAX)
        return mc_player_fail(error, MC_PLAYER_LIMIT, "Session native live count", NULL, NULL, NULL,
                              0);
    mc_player_session *session = calloc(1, sizeof(*session));
    if (session == NULL)
        return mc_player_fail(error, MC_PLAYER_ALLOCATION_FAILED, "Session native allocation", NULL,
                              NULL, NULL, 0);
    status = mc_player_string_retain(username);
    if (status != MC_PLAYER_OK) {
        free(session);
        return mc_player_fail(error, status, "Session username hold", "gr", "<init>",
                              "(Ljava/lang/String;Ljava/lang/String;)V", 6);
    }
    session->username = username;
    status = mc_player_string_retain(session_id);
    if (status != MC_PLAYER_OK) {
        mc_player_string_release(username);
        free(session);
        return mc_player_fail(error, status, "Session ID hold", "gr", "<init>",
                              "(Ljava/lang/String;Ljava/lang/String;)V", 11);
    }
    session->session_id = session_id;
    session->classes = classes;
    ++classes->live_sessions;
    *out = session;
    return MC_PLAYER_OK;
}

void mc_player_session_retire(mc_player_session **owner) {
    if (owner == NULL || *owner == NULL)
        return;
    mc_player_session *session = *owner;
    mc_player_string_release(session->username);
    mc_player_string_release(session->session_id);
    --session->classes->live_sessions;
    free(session);
    *owner = NULL;
}

bool mc_player_session_read(const mc_player_session *session, mc_player_session_state *out) {
    if (session == NULL || out == NULL)
        return false;
    *out = (mc_player_session_state){session->username, session->session_id, NULL};
    return true;
}

bool mc_player_session_matches(const mc_player_session *session, const mc_entity_classes *entities,
                               const mc_registry_classes *registry) {
    return session != NULL && session->classes->entities == entities &&
           (registry == NULL || session->classes->registry == registry);
}

bool mc_player_session_classes_read(const mc_player_session_classes *classes,
                                    mc_player_session_class_state *out) {
    if (classes == NULL || out == NULL)
        return false;
    *out =
        (mc_player_session_class_state){classes->phase,
                                        classes->pc,
                                        classes->creative,
                                        classes->creative == NULL ? NULL : classes->creative->items,
                                        classes->creative == NULL ? 0 : classes->creative->count,
                                        classes->live_sessions};
    return true;
}

mc_player_status mc_player_session_classes_free(mc_player_session_classes **owner,
                                                mc_player_error *error) {
    mc_player_error_clear(error);
    if (owner == NULL)
        return mc_player_fail(error, MC_PLAYER_INVALID_ARGUMENT, "Session class retirement pointer",
                              NULL, NULL, NULL, 0);
    mc_player_session_classes *classes = *owner;
    if (classes == NULL)
        return MC_PLAYER_OK;
    if (classes->live_sessions != 0 || classes->phase == MC_PLAYER_SESSION_INITIALIZING)
        return mc_player_fail(error, MC_PLAYER_BUSY, "Session class live dependents", NULL, NULL,
                              NULL, 0);
    mc_registry_error cause;
    if (mc_registry_classes_release(classes->registry, &cause) != MC_REGISTRY_OK) {
        if (error != NULL)
            error->registry = cause;
        return mc_player_fail(error, MC_PLAYER_REGISTRY_FAILED, "Session Registry release", NULL,
                              NULL, NULL, 0);
    }
    (void)mc_entity_classes_release(classes->entities);
    free(classes->creative);
    free(classes);
    *owner = NULL;
    return MC_PLAYER_OK;
}
