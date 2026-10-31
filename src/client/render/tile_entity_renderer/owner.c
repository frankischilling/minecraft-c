#include "internal.h"

#include "../../../world/runtime/construction_internal.h"

#include <stdlib.h>
#include <string.h>

void tile_renderer_error_init(mc_client_tile_renderer_error *e) {
    memset(e, 0, sizeof(*e));
    e->pc = e->ll_pc = e->renderer_pc = e->rf_pc = e->cv_pc = e->frame_pc = -1;
}

mc_client_tile_renderer_status tile_renderer_fail(mc_client_tile_renderer_error *e,
                                                  mc_client_tile_renderer_status status,
                                                  const char *operation, const char *source,
                                                  const char *method, const char *descriptor,
                                                  int32_t pc, bool native_policy) {
    e->status = status;
    e->operation = operation;
    e->source_class = source;
    e->source_method = method;
    e->source_descriptor = descriptor;
    e->pc = pc;
    if (source != NULL && strcmp(source, "ll") == 0)
        e->ll_pc = pc;
    e->native_policy = native_policy;
    return status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_origin_store(mc_client_tile_renderer_classes *c, unsigned int axis,
                                     double value, mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (c == NULL || axis > 2) {
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                           "LL native scalar origin arguments", NULL, NULL, NULL, -1, true);
    } else if (c->active || (c->a != NULL && c->a->active) ||
               (c->candidate != NULL && c->candidate->active)) {
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY, "LL scalar origin active owner", NULL,
                           NULL, NULL, -1, true);
    } else if (c->phase != MC_CLIENT_TILE_RENDERER_READY || c->models == NULL ||
               c->models->ll != c || c->models->ll_retired) {
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE, "LL scalar origin live class",
                           NULL, NULL, NULL, -1, true);
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

mc_client_tile_renderer_options mc_client_tile_renderer_default_options(void) {
    return (mc_client_tile_renderer_options){1048576u, 1048576u};
}

mc_client_tile_renderer_status mc_client_tile_entity_renderer_classes_existing(
    const mc_client_render_classes *models, const mc_registry_classes *expected_registry,
    const mc_world_runtime_classes *expected_world_classes,
    mc_client_tile_renderer_classes **nullable_out, mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (models == NULL || expected_registry == NULL || expected_world_classes == NULL ||
        nullable_out == NULL || expected_world_classes->registry != expected_registry ||
        expected_world_classes->side != MC_WORLD_INFO_CLIENT ||
        expected_world_classes->entities == NULL ||
        expected_world_classes->entities != models->entities ||
        expected_world_classes->profile != models->profile) {
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                           "LL passive existing association provenance", NULL, NULL, NULL, -1,
                           true);
    } else if (models->ll == NULL) {
        *nullable_out = NULL;
    } else {
        mc_client_tile_renderer_classes *c = models->ll;
        if (models->ll_retired || c->models != models ||
            c->world_classes != expected_world_classes || c->profile != models->profile ||
            c->tile_classes == NULL || c->tile_classes != expected_world_classes->tile_entities) {
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                               "LL passive existing owner association", NULL, NULL, NULL, -1, true);
        } else if (c->active || c->phase == MC_CLIENT_TILE_RENDERER_CONSTRUCTING ||
                   (c->candidate != NULL && c->candidate->active)) {
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY, "LL passive existing owner active",
                               NULL, NULL, NULL, -1, true);
        } else {
            *nullable_out = c;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

void *tile_renderer_allocate(mc_client_tile_renderer_classes *c, size_t bytes, const char *source,
                             const char *method, const char *descriptor, int32_t pc,
                             mc_client_tile_renderer_error *e) {
    if (bytes == 0 || c->bytes > c->options.class_bytes ||
        bytes > c->options.class_bytes - c->bytes) {
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_LIMIT, "LL native class allocation bound",
                           source, method, descriptor, pc, true);
        return NULL;
    }
    void *p = calloc(1, bytes);
    if (p == NULL)
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_ALLOCATION_FAILED,
                           "LL native class allocation", source, method, descriptor, pc, true);
    else
        c->bytes += bytes;
    return p;
}

mc_client_tile_renderer_status mc_client_tile_renderer_classes_bind(
    mc_client_render_classes *models, mc_world_runtime_classes *world_classes,
    mc_java_random_profile *profile, const mc_client_tile_renderer_options *requested,
    mc_client_tile_renderer_classes **out, mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    const mc_client_tile_renderer_options options =
        requested == NULL ? mc_client_tile_renderer_default_options() : *requested;
    if (models == NULL || world_classes == NULL || profile == NULL || out == NULL ||
        models->profile != profile || world_classes->profile != profile ||
        world_classes->side != MC_WORLD_INFO_CLIENT || world_classes->registry == NULL ||
        world_classes->entities == NULL || world_classes->entities != models->entities) {
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                           "LL same CLIENT World/Entity/profile namespace", NULL, NULL, NULL, -1,
                           true);
    } else if (models->ll_retired) {
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE, "LL native namespace ended",
                           NULL, NULL, NULL, -1, true);
    } else if (models->ll != NULL) {
        mc_client_tile_renderer_classes *c = models->ll;
        if (c->world_classes != world_classes || c->profile != profile ||
            c->options.class_bytes != options.class_bytes ||
            c->options.renderer_bytes != options.renderer_bytes)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                               "LL immutable associated namespace", NULL, NULL, NULL, -1, true);
        else if (c->phase == MC_CLIENT_TILE_RENDERER_RETIRING)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE, "LL terminal namespace",
                               NULL, NULL, NULL, -1, true);
        else
            *out = c;
    } else if (options.class_bytes < sizeof(mc_client_tile_renderer_classes)) {
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_LIMIT, "LL cold native namespace bound",
                           NULL, NULL, NULL, -1, true);
    } else {
        mc_client_tile_renderer_classes *c = calloc(1, sizeof(*c));
        if (c == NULL) {
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_ALLOCATION_FAILED,
                               "LL cold native namespace allocation", NULL, NULL, NULL, -1, true);
        } else {
            e.tile_entity = mc_tile_entity_classes_borrow(world_classes, &c->tile_classes);
            if (e.tile_entity != MC_TILE_ENTITY_OK) {
                free(c);
                tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                                   "LL canonical TileEntity namespace borrow", NULL, NULL, NULL, -1,
                                   true);
            } else {
                /* This valid selector/output on the just-borrowed live owner
                 * cannot initialize or fail a source class. */
                e.tile_entity = mc_tile_entity_class_literal_get(
                    c->tile_classes, MC_TILE_ENTITY_CLASS_KIND_TILE_ENTITY, &c->base);
                if (e.tile_entity != MC_TILE_ENTITY_OK) {
                    (void)mc_tile_entity_classes_release(&c->tile_classes);
                    free(c);
                    tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                                       "LL canonical base Class cell", NULL, NULL, NULL, -1, true);
                } else {
                    c->models = models;
                    c->world_classes = world_classes;
                    c->profile = profile;
                    c->options = options;
                    c->bytes = sizeof(*c);
                    tile_renderer_error_init(&c->failure);
                    tile_renderer_error_init(&c->retirement);
                    models->ll = c;
                    *out = c;
                }
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_consumer_attach(mc_client_tile_renderer_classes *c,
                                        mc_client_tile_renderer_consumer *consumer,
                                        mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (c == NULL || consumer == NULL || consumer->attached || consumer->classes != NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                           "LL unique native consumer", NULL, NULL, NULL, -1, true);
    else if (c->active)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY, "LL active consumer attach", NULL,
                           NULL, NULL, -1, true);
    else if (c->phase == MC_CLIENT_TILE_RENDERER_RETIRING)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE, "LL terminal consumer", NULL,
                           NULL, NULL, -1, true);
    else if (c->consumers == SIZE_MAX)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_LIMIT, "LL native consumer bound", NULL,
                           NULL, NULL, -1, true);
    else {
        ++c->consumers;
        consumer->classes = c;
        consumer->attached = true;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_consumer_detach(mc_client_tile_renderer_consumer *consumer,
                                        mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (consumer == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, "LL native consumer slot",
                           NULL, NULL, NULL, -1, true);
    else if (consumer->attached) {
        mc_client_tile_renderer_classes *c = consumer->classes;
        if (c == NULL || c->consumers == 0)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE,
                               "LL native consumer accounting", NULL, NULL, NULL, -1, true);
        else if (c->active)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY, "LL active consumer detach", NULL,
                               NULL, NULL, -1, true);
        else {
            --c->consumers;
            consumer->classes = NULL;
            consumer->attached = false;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_static_get(mc_client_tile_renderer_classes *c,
                                   mc_client_tile_renderer_dispatcher **out,
                                   mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (c == NULL || out == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, "LL static get/output",
                           NULL, NULL, NULL, -1, true);
    else if (c->active)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY,
                           "LL entered source class initialization", NULL, NULL, NULL, -1, true);
    else if (c->phase == MC_CLIENT_TILE_RENDERER_RETIRING)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE, "LL terminal static get",
                           NULL, NULL, NULL, -1, true);
    else if (c->phase == MC_CLIENT_TILE_RENDERER_FAILED) {
        e = c->failure;
        e.initial_status = e.status;
        e.status = MC_CLIENT_TILE_RENDERER_CLASS_ERRONEOUS;
    } else {
        if (c->phase == MC_CLIENT_TILE_RENDERER_COLD) {
            c->active = true;
            c->phase = MC_CLIENT_TILE_RENDERER_CONSTRUCTING;
            c->candidate = tile_renderer_allocate(c, sizeof(*c->candidate), "ll", "<clinit>", "()V",
                                                  0, &c->failure);
            if (c->candidate != NULL) {
                c->candidate->classes = c;
                c->candidate->holds = 1;
                c->candidate->last_put_pc = c->candidate->last_constructor_pc =
                    c->candidate->last_frame_pc = -1;
                if (tile_renderer_construct(c->candidate)) {
                    c->a = c->candidate; /* Actual LL.<clinit> a7 after ctor114. */
                    c->phase = MC_CLIENT_TILE_RENDERER_READY;
                }
            }
            if (c->phase != MC_CLIENT_TILE_RENDERER_READY)
                c->phase = MC_CLIENT_TILE_RENDERER_FAILED;
            c->active = false;
        }
        if (c->phase == MC_CLIENT_TILE_RENDERER_READY)
            *out = c->a;
        else
            e = c->failure;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool tile_renderer_passive_admit(mc_client_tile_renderer_dispatcher *m,
                                 mc_client_tile_renderer_error *e) {
    if (m == NULL)
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, "LL actual receiver", NULL,
                           NULL, NULL, -1, true);
    else if (m->active || m->classes->active)
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_BUSY, "LL active receiver", NULL, NULL, NULL,
                           -1, true);
    else if (m->classes->phase == MC_CLIENT_TILE_RENDERER_RETIRING)
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE, "LL terminal receiver", NULL,
                           NULL, NULL, -1, true);
    return e->status == MC_CLIENT_TILE_RENDERER_OK;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_dispatcher_retain(mc_client_tile_renderer_dispatcher *m,
                                          mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (tile_renderer_passive_admit(m, &e)) {
        if (m->holds == SIZE_MAX)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_LIMIT, "LL occurrence bound", NULL, NULL,
                               NULL, -1, true);
        else
            ++m->holds;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_dispatcher_release(mc_client_tile_renderer_dispatcher **slot,
                                           mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (slot == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, "LL occurrence slot", NULL,
                           NULL, NULL, -1, true);
    else if (*slot != NULL && tile_renderer_passive_admit(*slot, &e)) {
        if ((*slot)->holds <= 1)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY, "LL class owns final occurrence",
                               NULL, NULL, NULL, -1, true);
        else {
            --(*slot)->holds;
            *slot = NULL;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_retain(mc_client_tile_renderer *r, mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (r == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, "JE occurrence receiver",
                           NULL, NULL, NULL, -1, true);
    else if (r->active || !tile_renderer_passive_admit(r->parent, &e)) {
        if (e.status == MC_CLIENT_TILE_RENDERER_OK)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY, "JE active occurrence", NULL, NULL,
                               NULL, -1, true);
    } else if (r->holds == SIZE_MAX)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_LIMIT, "JE occurrence bound", NULL, NULL,
                           NULL, -1, true);
    else
        ++r->holds;
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_release(mc_client_tile_renderer **slot,
                                mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (slot == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, "JE occurrence slot", NULL,
                           NULL, NULL, -1, true);
    else if (*slot != NULL) {
        mc_client_tile_renderer *r = *slot;
        if (r->active || !tile_renderer_passive_admit(r->parent, &e)) {
            if (e.status == MC_CLIENT_TILE_RENDERER_OK)
                tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY, "JE active occurrence", NULL,
                                   NULL, NULL, -1, true);
        } else if (r->holds <= 1)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY, "LL owns final JE occurrence",
                               NULL, NULL, NULL, -1, true);
        else {
            --r->holds;
            *slot = NULL;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_tile_renderer_inspect(const mc_client_tile_renderer_classes *c,
                                     mc_client_tile_renderer_state *s) {
    if (c == NULL || s == NULL || c->active)
        return false;
    *s = (mc_client_tile_renderer_state){.phase = c->phase,
                                         .b = c->b,
                                         .c = c->c,
                                         .d = c->d,
                                         .native_consumers = c->consumers,
                                         .class_bytes = c->bytes,
                                         .last_put_pc = -1,
                                         .last_constructor_pc = -1,
                                         .last_frame_pc = -1,
                                         .failure = &c->failure,
                                         .retirement = &c->retirement};
    if (c->phase != MC_CLIENT_TILE_RENDERER_RETIRING) {
        s->source_views_available = true;
        s->a = c->a;
        s->candidate = c->candidate;
        s->tile_classes = c->tile_classes;
        s->world_classes = c->world_classes;
        const mc_client_tile_renderer_dispatcher *m = c->candidate;
        if (m != NULL) {
            s->fields = m->fields;
            s->dispatcher_holds = m->holds;
            s->allocated_renderers = m->allocated_renderers;
            s->put_calls = m->put_calls;
            s->completed_backpointers = m->backpointers;
            s->last_put_pc = m->last_put_pc;
            s->last_constructor_pc = m->last_constructor_pc;
            s->last_frame_pc = m->last_frame_pc;
            s->retained_engines = m->retained_engines;
            s->retained_fonts = m->retained_fonts;
            s->living_retained = m->living_occurrence != NULL;
            if (m->map != NULL) {
                s->entries = m->map->size;
                s->capacity = m->map->capacity;
                s->threshold = m->map->threshold;
                s->map_mod_count = m->map->mod_count;
                for (size_t i = 0; i < m->map->capacity; ++i)
                    for (tile_renderer_map_node *p = m->map->buckets[i]; p != NULL; p = p->next)
                        if (p->value == NULL)
                            ++s->null_values;
            }
        }
    }
    return true;
}

bool mc_client_tile_renderer_object_inspect(const mc_client_tile_renderer *r,
                                            mc_client_tile_renderer_object_state *s) {
    if (r == NULL || s == NULL || r->active || r->parent->active || r->parent->classes->active ||
        r->parent->classes->phase == MC_CLIENT_TILE_RENDERER_RETIRING)
        return false;
    *s = (mc_client_tile_renderer_object_state){.source_class = r->source_class,
                                                .identity = r,
                                                .phase = r->phase,
                                                .holds = r->holds,
                                                .allocation_bytes = r->bytes,
                                                .manager = r->manager,
                                                .sign_model = r->sign_model,
                                                .sign_candidate = r->sign_candidate,
                                                .sign_phase = r->sign_phase,
                                                .spawner_map = r->spawner_map,
                                                .spawner_map_candidate = r->spawner_map_candidate,
                                                .piston_blocks = r->piston_blocks,
                                                .piston_cv = r->current_cv,
                                                .live_cvs = r->live_cvs,
                                                .retained_cv_holds = r->retained_cv_holds,
                                                .failure = &r->failure};
    for (model_allocation *a = r->allocations; a != NULL; a = a->next) {
        ++s->graph_allocations;
        if (a->vector != NULL)
            ++s->vectors;
    }
    if (r->spawner_map != NULL) {
        s->spawner_entries = r->spawner_map->size;
        s->spawner_capacity = r->spawner_map->capacity;
        s->spawner_threshold = r->spawner_map->threshold;
    }
    return true;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_allocated(mc_client_tile_renderer_dispatcher *m, size_t index,
                                  mc_client_tile_renderer **out,
                                  mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (out == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                           "LL allocated renderer output", NULL, NULL, NULL, -1, true);
    else if (tile_renderer_passive_admit(m, &e)) {
        if (index >= m->allocated_renderers)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                               "LL allocated renderer index", NULL, NULL, NULL, -1, true);
        else
            *out = m->renderers[index];
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

static bool terminal_admit(mc_client_tile_renderer_classes *c, mc_client_tile_renderer_error *e) {
    if (c->active || c->consumers != 0)
        goto busy;
    mc_client_tile_renderer_dispatcher *m = c->candidate;
    if (m != NULL) {
        if (m->active || m->holds != 1)
            goto busy;
        for (size_t i = 0; i < m->allocated_renderers; ++i) {
            const mc_client_tile_renderer *r = m->renderers[i];
            if (r != NULL &&
                (r->holds != 1 || r->active || r->phase == MC_CLIENT_MODEL_CONSTRUCTING))
                goto busy;
        }
        if (!tile_renderer_world_terminal_admit(m, e))
            return false;
        if (!tile_renderer_frame_terminal_admit(m, e))
            return false;
    }
    c->phase = MC_CLIENT_TILE_RENDERER_RETIRING;
    return true;
busy:
    tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_BUSY,
                       "LL native consumer/renderer/operation retirement gate", NULL, NULL, NULL,
                       -1, true);
    return false;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_classes_retire(mc_client_tile_renderer_classes **slot,
                                       mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (slot == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                           "LL outer class retirement slot", NULL, NULL, NULL, -1, true);
    else if (*slot != NULL) {
        mc_client_tile_renderer_classes *c = *slot;
        if (!terminal_admit(c, &e))
            goto done;
        mc_client_tile_renderer_dispatcher *m = c->candidate;
        if (m != NULL) {
            m->active = c->active = true;
            const bool frame_retired = tile_renderer_frame_retire(m, &e);
            m->active = c->active = false;
            if (!frame_retired)
                goto retry;
            if (!tile_renderer_world_retire(m, &e))
                goto retry;
            for (size_t i = 0; i < m->allocated_renderers; ++i)
                if (!tile_renderer_object_retire(&m->renderers[i], &e))
                    goto retry;
            tile_renderer_map_retire(c, &m->map_candidate);
            c->bytes -= sizeof(*m);
            free(m);
            c->candidate = NULL;
        }
        e.tile_entity = mc_tile_entity_classes_release(&c->tile_classes);
        if (e.tile_entity != MC_TILE_ENTITY_OK) {
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY,
                               "LL canonical TileEntity namespace release", NULL, NULL, NULL, -1,
                               true);
            goto retry;
        }
        c->models->ll = NULL;
        c->models->ll_retired = true;
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
