#include "internal.h"
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(mc_client_manager_error) <= 32768u,
               "manager diagnostics need bounded native stack storage");

void manager_error_init(mc_client_manager_error *e) {
    memset(e, 0, sizeof(*e));
    e->pc = e->th_pc = e->renderer_pc = e->frame_pc = -1;
}

mc_client_manager_status manager_fail(mc_client_manager_error *e, mc_client_manager_status status,
                                      const char *op, const char *cl, const char *method,
                                      const char *descriptor, int32_t pc, bool native) {
    e->status = status;
    e->registry_delegated = false;
    e->operation = op;
    e->source_class = cl;
    e->source_method = method;
    e->source_descriptor = descriptor;
    e->pc = pc;
    e->native_policy = native;
    return status;
}

mc_client_manager_status mc_client_render_manager_origin_store(mc_client_render_manager_classes *c,
                                                               unsigned int axis, double value,
                                                               mc_client_manager_error *error) {
    mc_client_manager_error e;
    manager_error_init(&e);
    if (c == NULL || axis > 2) {
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "TH native scalar origin arguments",
                     NULL, NULL, NULL, -1, true);
    } else if (c->active || (c->a != NULL && c->a->active) ||
               (c->candidate != NULL && c->candidate->active)) {
        manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "TH scalar origin active owner", NULL, NULL, NULL,
                     -1, true);
    } else if (c->phase != MC_CLIENT_MANAGER_READY || c->models == NULL || c->models->th != c ||
               c->models->th_retired) {
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_PHASE, "TH scalar origin live class", NULL, NULL,
                     NULL, -1, true);
    } else if (axis == 0) {
        c->b = value;
    } else if (axis == 1) {
        c->c = value;
    } else {
        c->d = value;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_manager_options mc_client_render_manager_default_options(void) {
    return (mc_client_manager_options){2u * 1048576u, 2u * 1048576u, 128};
}

void *manager_allocate_native(mc_client_render_manager_classes *c, size_t bytes, const char *source,
                              int32_t pc, mc_client_manager_error *error) {
    if (c->bytes > c->options.class_bytes || bytes > c->options.class_bytes - c->bytes) {
        manager_fail(error, MC_CLIENT_MANAGER_LIMIT, "manager native class allocation bound",
                     source, "<init>", "()V", pc, true);
        return NULL;
    }
    void *p = calloc(1, bytes);
    if (p == NULL)
        manager_fail(error, MC_CLIENT_MANAGER_ALLOCATION_FAILED, "manager native class allocation",
                     source, "<init>", "()V", pc, true);
    else
        c->bytes += bytes;
    return p;
}

void *manager_allocate(mc_client_render_manager_classes *c, size_t bytes, const char *source,
                       int32_t pc) {
    return manager_allocate_native(c, bytes, source, pc, &c->failure);
}

mc_client_manager_status mc_client_render_manager_classes_bind(
    mc_client_render_classes *models, mc_registry_classes *registry,
    mc_java_random_profile *profile, const mc_client_manager_options *requested,
    mc_client_render_manager_classes **out, mc_client_manager_error *error) {
    mc_client_manager_error e;
    manager_error_init(&e);
    const mc_client_manager_options options =
        requested == NULL ? mc_client_render_manager_default_options() : *requested;
    if (models == NULL || registry == NULL || profile == NULL || out == NULL ||
        models->profile != profile) {
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "manager same real namespace/services/output", NULL, NULL, NULL, -1, true);
    } else if (models->th_retired) {
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_PHASE, "manager native namespace ended", NULL,
                     NULL, NULL, -1, true);
    } else if (models->th != NULL) {
        mc_client_render_manager_classes *c = models->th;
        if (c->registry != registry || c->profile != profile ||
            c->options.class_bytes != options.class_bytes ||
            c->options.renderer_bytes != options.renderer_bytes ||
            c->options.maximum_displaced_hands != options.maximum_displaced_hands)
            manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                         "manager immutable associated namespace", NULL, NULL, NULL, -1, true);
        else if (c->phase == MC_CLIENT_MANAGER_RETIRING)
            manager_fail(&e, MC_CLIENT_MANAGER_INVALID_PHASE, "manager terminal namespace", NULL,
                         NULL, NULL, -1, true);
        else
            *out = c;
    } else if (options.class_bytes < sizeof(mc_client_render_manager_classes)) {
        manager_fail(&e, MC_CLIENT_MANAGER_LIMIT, "manager cold native class bound", NULL, NULL,
                     NULL, -1, true);
    } else {
        mc_client_render_manager_classes *c = calloc(1, sizeof(*c));
        if (c == NULL)
            manager_fail(&e, MC_CLIENT_MANAGER_ALLOCATION_FAILED, "manager cold native namespace",
                         NULL, NULL, NULL, -1, true);
        else {
            e.registry.status =
                mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT, profile, &e.registry);
            if (e.registry.status != MC_REGISTRY_OK) {
                free(c);
                manager_fail(&e, MC_CLIENT_MANAGER_REGISTRY_FAILED, "manager real Registry hold",
                             NULL, NULL, NULL, -1, true);
            } else {
                c->models = models;
                c->registry = registry;
                c->profile = profile;
                c->options = options;
                c->registry_held = true;
                c->bytes = sizeof(*c);
                manager_error_init(&c->failure);
                manager_error_init(&c->retirement);
                models->th = c;
                *out = c;
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_manager_status
mc_client_render_manager_consumer_attach(mc_client_render_manager_classes *c,
                                         mc_client_manager_consumer *consumer,
                                         mc_client_manager_error *error) {
    mc_client_manager_error e;
    manager_error_init(&e);
    if (c == NULL || consumer == NULL || consumer->attached || consumer->classes != NULL)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "manager unique native consumer", NULL,
                     NULL, NULL, -1, true);
    else if (c->active)
        manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "manager consumer while active", NULL, NULL, NULL,
                     -1, true);
    else if (c->phase == MC_CLIENT_MANAGER_RETIRING)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_PHASE, "manager terminal consumer", NULL, NULL,
                     NULL, -1, true);
    else if (c->consumers == SIZE_MAX)
        manager_fail(&e, MC_CLIENT_MANAGER_LIMIT, "manager consumer bound", NULL, NULL, NULL, -1,
                     true);
    else {
        ++c->consumers;
        consumer->classes = c;
        consumer->attached = true;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_manager_status
mc_client_render_manager_consumer_detach(mc_client_manager_consumer *consumer,
                                         mc_client_manager_error *error) {
    mc_client_manager_error e;
    manager_error_init(&e);
    if (consumer == NULL)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "manager consumer slot", NULL, NULL,
                     NULL, -1, true);
    else if (consumer->attached) {
        mc_client_render_manager_classes *c = consumer->classes;
        if (c == NULL || c->consumers == 0)
            manager_fail(&e, MC_CLIENT_MANAGER_INVALID_PHASE, "manager consumer accounting", NULL,
                         NULL, NULL, -1, true);
        else if (c->active)
            manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "manager active consumer detach", NULL, NULL,
                         NULL, -1, true);
        else {
            --c->consumers;
            consumer->attached = false;
            consumer->classes = NULL;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool manager_ds_active_use(mc_client_render_manager_classes *c) {
    if (c->ds_ready)
        return true;
    if (c->ds_erroneous)
        return false;
    c->ds_candidate = manager_allocate(c, 5 * sizeof(*c->ds_candidate), "ds", 1);
    if (c->ds_candidate == NULL)
        goto failed;
    static const uint16_t cloth[] = {'c', 'l', 'o', 't', 'h'};
    static const uint16_t chain[] = {'c', 'h', 'a', 'i', 'n'};
    static const uint16_t iron[] = {'i', 'r', 'o', 'n'};
    static const uint16_t diamond[] = {'d', 'i', 'a', 'm', 'o', 'n', 'd'};
    static const uint16_t gold[] = {'g', 'o', 'l', 'd'};
    const uint16_t *const units[] = {cloth, chain, iron, diamond, gold};
    static const size_t lengths[] = {5, 5, 4, 7, 4};
    static const int32_t pcs[] = {6, 11, 16, 21, 26};
    for (size_t i = 0; i < 5; ++i) {
        c->failure.string = mc_player_string_create(units[i], lengths[i], &c->ds_candidate[i]);
        if (c->failure.string != MC_PLAYER_OK) {
            manager_fail(&c->failure, MC_CLIENT_MANAGER_STRING_FAILED,
                         "actual DS literal resolution", "ds", "<clinit>", "()V", pcs[i], true);
            goto failed;
        }
    }
    c->ds_i = c->ds_candidate; /* Actual DS.i29 only after all array elements. */
    c->ds_ready = true;
    return true;
failed:
    c->ds_erroneous = true;
    c->failure.source_method = "<clinit>";
    return false;
}

mc_client_manager_status mc_client_render_manager_static_get(mc_client_render_manager_classes *c,
                                                             mc_client_render_manager **out,
                                                             mc_client_manager_error *error) {
    mc_client_manager_error e;
    manager_error_init(&e);
    if (c == NULL || out == NULL)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "actual TH static get/output", NULL,
                     NULL, NULL, -1, true);
    else if (c->active)
        manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "actual TH entered class initialization", NULL,
                     NULL, NULL, -1, true);
    else if (c->phase == MC_CLIENT_MANAGER_RETIRING)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_PHASE, "actual TH terminal namespace", NULL,
                     NULL, NULL, -1, true);
    else if (c->phase == MC_CLIENT_MANAGER_FAILED) {
        e = c->failure;
        e.initial_status = e.status;
        e.status = MC_CLIENT_MANAGER_CLASS_ERRONEOUS;
    } else {
        if (c->phase == MC_CLIENT_MANAGER_COLD) {
            c->active = true;
            c->phase = MC_CLIENT_MANAGER_CONSTRUCTING;
            c->candidate = manager_allocate(c, sizeof(*c->candidate), "th", 0);
            if (c->candidate == NULL)
                c->failure.source_method = "<clinit>";
            else {
                c->candidate->classes = c;
                c->candidate->holds = 1;
                c->candidate->last_put_pc = c->candidate->last_constructor_pc = -1;
                c->candidate->last_frame_pc = -1;
                if (manager_construct(c->candidate)) {
                    c->a = c->candidate; /* TH.<clinit> a7 after completed TH ctor730. */
                    c->phase = MC_CLIENT_MANAGER_READY;
                }
            }
            if (c->phase != MC_CLIENT_MANAGER_READY)
                c->phase = MC_CLIENT_MANAGER_FAILED;
            c->active = false;
        }
        if (c->phase == MC_CLIENT_MANAGER_READY)
            *out = c->a;
        else
            e = c->failure;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_render_manager_inspect(const mc_client_render_manager_classes *c,
                                      mc_client_manager_state *s) {
    if (c == NULL || s == NULL || c->active)
        return false;
    *s = (mc_client_manager_state){.phase = c->phase,
                                   .b = c->b,
                                   .c = c->c,
                                   .d = c->d,
                                   .native_consumers = c->consumers,
                                   .class_bytes = c->bytes,
                                   .ds_ready = c->ds_ready,
                                   .last_put_pc = -1,
                                   .last_constructor_pc = -1,
                                   .last_frame_pc = -1,
                                   .failure = &c->failure,
                                   .retirement = &c->retirement};
    if (c->candidate != NULL) {
        s->last_frame_pc = c->candidate->last_frame_pc;
        s->retained_engines = c->candidate->retained_engines;
        s->retained_fonts = c->candidate->retained_fonts;
        s->retained_settings = c->candidate->retained_settings;
    }
    if (c->phase != MC_CLIENT_MANAGER_RETIRING) {
        s->source_views_available = true;
        s->a = c->a;
        s->candidate = c->candidate;
        s->ds_i = c->ds_i;
        if (c->candidate != NULL) {
            const mc_client_render_manager *m = c->candidate;
            s->fields = m->fields;
            s->entries = m->map == NULL ? 0 : m->map->size;
            s->completed_bw_stores = m->bw_count;
            s->last_put_pc = m->last_put_pc;
            s->last_constructor_pc = m->last_constructor_pc;
            s->manager_holds = m->holds;
            s->displaced_hands = m->displaced_count;
            s->hand_occurrence = m->hand;
            s->living_occurrence = m->living_occurrence;
        }
    }
    return true;
}

mc_client_manager_status mc_client_render_manager_retain(mc_client_render_manager *m,
                                                         mc_client_manager_error *error) {
    mc_client_manager_error e;
    manager_error_init(&e);
    if (m == NULL)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "manager occurrence receiver", NULL,
                     NULL, NULL, -1, true);
    else if (m->active || m->classes->active)
        manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "manager active occurrence", NULL, NULL, NULL, -1,
                     true);
    else if (m->classes->phase == MC_CLIENT_MANAGER_RETIRING)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_PHASE, "manager terminal occurrence", NULL, NULL,
                     NULL, -1, true);
    else if (m->holds == SIZE_MAX)
        manager_fail(&e, MC_CLIENT_MANAGER_LIMIT, "manager occurrence bound", NULL, NULL, NULL, -1,
                     true);
    else
        ++m->holds;
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_manager_status mc_client_render_manager_release(mc_client_render_manager **slot,
                                                          mc_client_manager_error *error) {
    mc_client_manager_error e;
    manager_error_init(&e);
    if (slot == NULL)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "manager occurrence slot", NULL, NULL,
                     NULL, -1, true);
    else if (*slot != NULL) {
        mc_client_render_manager *m = *slot;
        if (m->active || m->classes->active || m->holds <= 1)
            manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "manager class owns final occurrence", NULL,
                         NULL, NULL, -1, true);
        else {
            --m->holds;
            *slot = NULL;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool manager_terminal_admit(mc_client_render_manager_classes *c, mc_client_manager_error *e) {
    if (c->active || c->consumers != 0)
        goto busy;
    mc_client_render_manager *m = c->candidate;
    if (m != NULL) {
        if (m->active || m->holds != 1)
            goto busy;
        for (size_t i = 0; i < m->allocated_renderers; ++i) {
            const mc_client_managed_renderer *r = m->renderers[i];
            if (r != NULL &&
                (r->holds != 1 || r->active || r->phase == MC_CLIENT_MODEL_CONSTRUCTING))
                goto busy;
        }
        if (!manager_frame_terminal_admit(m, e))
            return false;
    }
    c->phase = MC_CLIENT_MANAGER_RETIRING;
    return true;
busy:
    manager_fail(e, MC_CLIENT_MANAGER_BUSY, "manager actual consumer/renderer/operation gate", NULL,
                 NULL, NULL, -1, true);
    return false;
}

mc_client_manager_status
mc_client_render_manager_classes_retire(mc_client_render_manager_classes **slot,
                                        mc_client_manager_error *error) {
    mc_client_manager_error e;
    manager_error_init(&e);
    if (slot == NULL)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "outer manager class slot", NULL, NULL,
                     NULL, -1, true);
    else if (*slot != NULL) {
        mc_client_render_manager_classes *c = *slot;
        if (!manager_terminal_admit(c, &e))
            goto done;
        mc_client_render_manager *m = c->candidate;
        if (m != NULL) {
            if (m->hand != NULL || m->displaced != NULL) {
                manager_fail(
                    &e, MC_CLIENT_MANAGER_BUSY,
                    "outer authority must retire actual current/displaced RA with Display alive",
                    NULL, NULL, NULL, -1, true);
                goto retry;
            }
            m->active = c->active = true;
            const bool frame_retired = manager_frame_retire(m, &e);
            m->active = c->active = false;
            if (!frame_retired)
                goto retry;
            for (size_t i = 0; i < m->allocated_renderers; ++i)
                if (!manager_renderer_retire(&m->renderers[i], &e))
                    goto retry;
            if (m->map_candidate != NULL) {
                manager_map *map = m->map_candidate;
                for (size_t i = 0; i < map->capacity; ++i) {
                    manager_map_entry *p = map->buckets[i];
                    while (p != NULL) {
                        manager_map_entry *next = p->next;
                        free(p);
                        p = next;
                    }
                }
                free(map->buckets);
                free(map);
            }
            /* Source views end with native storage; no Source o/a/f reset. */
            mc_world_runtime_binding_release(m->world_occurrence);
            manager_frame_destroy(m);
            free(m);
            c->candidate = NULL;
        }
        if (c->ds_candidate != NULL) {
            for (size_t i = 0; i < 5; ++i)
                mc_player_string_release(c->ds_candidate[i]);
            free(c->ds_candidate);
            c->ds_candidate = NULL;
        }
        if (c->registry_held) {
            e.registry.status = mc_registry_classes_release(c->registry, &e.registry);
            if (e.registry.status != MC_REGISTRY_OK) {
                manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "manager Registry occurrence cleanup",
                             NULL, NULL, NULL, -1, true);
                goto retry;
            }
            c->registry_held = false;
        }
        c->models->th = NULL;
        c->models->th_retired = true;
        free(c);
        *slot = NULL;
        goto done;
    retry:
        c->retirement = e;
    }
done:
    if (error != NULL)
        *error = e;
    return e.status;
}
