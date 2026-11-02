#include "../../entity/entity_internal.h"
#include "effect_renderer_internal.h"
#include "mc/world/storage/source_chunk_loader.h"

#include <stdlib.h>
#include <string.h>

static void error_init(mc_effect_renderer_error *e) {
    memset(e, 0, sizeof(*e));
    e->layer = e->index = SIZE_MAX;
}

typedef struct mc_effect_error {
    mc_effect_renderer_status status;
    const char *operation, *source_method, *source_descriptor;
    unsigned int source_pc;
    size_t layer, index;
    mc_entity_status entity;
    mc_java_random_profile_status random;
    bool native_policy, source_exception;
    const char *exception_class;
} mc_effect_error;

typedef struct effect_error_scope {
    mc_effect_renderer_error *out;
    mc_effect_error value;
} effect_error_scope;

/* Legacy DN work needs only this compact prefix. Optional diagnostics consume
 * no DN quota and do not allocate a large child that no source call reaches. */
static mc_effect_error *error_begin(mc_client_effect_renderer *r, mc_effect_renderer_error *out,
                                    effect_error_scope *scope) {
    (void)r;
    *scope = (effect_error_scope){.out = out, .value = {.layer = SIZE_MAX, .index = SIZE_MAX}};
    if (out != NULL)
        error_init(out);
    return &scope->value;
}

static mc_effect_renderer_status error_end(mc_effect_error *e, effect_error_scope *scope) {
    mc_effect_renderer_error *out = scope->out;
    if (out != NULL) {
        if (e->status == MC_EFFECT_RENDERER_OK)
            error_init(out);
        out->status = e->status;
        out->operation = e->operation;
        out->source_method = e->source_method;
        out->source_descriptor = e->source_descriptor;
        out->source_pc = e->source_pc;
        out->layer = e->layer;
        out->index = e->index;
        out->entity = e->entity;
        out->random = e->random;
        out->native_policy = e->native_policy;
        out->source_exception = e->source_exception;
        out->exception_class = e->exception_class;
    }
    return e->status;
}

static mc_effect_renderer_status fail(mc_effect_error *e, mc_effect_renderer_status status,
                                      const char *operation, const char *method,
                                      const char *descriptor, unsigned int pc) {
    e->status = status;
    e->operation = operation;
    e->source_method = method;
    e->source_descriptor = descriptor;
    e->source_pc = pc;
    e->native_policy = status != MC_EFFECT_RENDERER_NULL_REFERENCE;
    if (status == MC_EFFECT_RENDERER_NULL_REFERENCE) {
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
    }
    return status;
}

static void full_fail(mc_effect_renderer_error *out, mc_effect_renderer_status status,
                      const char *operation, const char *method, const char *descriptor,
                      unsigned int pc) {
    out->status = status;
    out->operation = operation;
    out->source_method = method;
    out->source_descriptor = descriptor;
    out->source_pc = pc;
    out->native_policy = status != MC_EFFECT_RENDERER_NULL_REFERENCE;
    if (status == MC_EFFECT_RENDERER_NULL_REFERENCE) {
        out->source_exception = true;
        out->exception_class = "java/lang/NullPointerException";
    }
}

static void *allocate(mc_client_effect_renderer *r, size_t bytes, mc_effect_error *e,
                      const char *operation, const char *method, const char *descriptor,
                      unsigned int pc) {
    if (r->allocation_bytes > r->max_allocation_bytes ||
        bytes > r->max_allocation_bytes - r->allocation_bytes) {
        fail(e, MC_EFFECT_RENDERER_LIMIT, operation, method, descriptor, pc);
        return NULL;
    }
    void *p = malloc(bytes);
    if (p == NULL)
        fail(e, MC_EFFECT_RENDERER_ALLOCATION_FAILED, operation, method, descriptor, pc);
    else
        r->allocation_bytes += bytes;
    return p;
}

static bool enter(mc_client_effect_renderer *r, mc_effect_error *e) {
    if (r == NULL)
        fail(e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer receiver", NULL, NULL, 0);
    else if (r->entered || r->draw_call != NULL || r->tick_call != NULL || r->block_call != NULL)
        fail(e, MC_EFFECT_RENDERER_BUSY, "EffectRenderer operation entered", NULL, NULL, 0);
    else if (r->phase != MC_EFFECT_RENDERER_READY)
        fail(e, MC_EFFECT_RENDERER_INVALID_PHASE, "EffectRenderer READY receiver", NULL, NULL, 0);
    else {
        r->entered = true;
        return true;
    }
    return false;
}

mc_effect_renderer_status mc_client_effect_renderer_allocate(
    mc_entity_classes *classes, const mc_effect_renderer_options *options,
    mc_client_effect_renderer **out, mc_effect_renderer_error *error) {
    effect_error_scope work;
    mc_effect_error *e = error_begin(NULL, error, &work);
    if (e == NULL)
        return MC_EFFECT_RENDERER_ALLOCATION_FAILED;
    const size_t limit = options == NULL ? 16u * 1024u * 1024u : options->max_allocation_bytes;
    if (classes == NULL || out == NULL || classes->source != MC_ENTITY_CLIENT)
        fail(e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer allocation arguments", NULL,
             NULL, 0);
    else if (limit < sizeof(mc_client_effect_renderer))
        fail(e, MC_EFFECT_RENDERER_LIMIT, "EffectRenderer allocation budget", NULL, NULL, 0);
    else {
        e->entity = mc_entity_classes_retain(classes, MC_ENTITY_CLIENT, classes->profile);
        if (e->entity != MC_ENTITY_OK)
            fail(e, MC_EFFECT_RENDERER_ENTITY_FAILED, "EffectRenderer class binding", NULL, NULL,
                 0);
        else {
            mc_client_effect_renderer *r = malloc(sizeof(*r));
            if (r == NULL) {
                (void)mc_entity_classes_release(classes);
                fail(e, MC_EFFECT_RENDERER_ALLOCATION_FAILED, "EffectRenderer allocation", NULL,
                     NULL, 0);
            } else {
                *r = (mc_client_effect_renderer){.classes = classes,
                                                 .allocation_bytes = sizeof(*r),
                                                 .max_allocation_bytes = limit,
                                                 .holds = 1};
                *out = r;
            }
        }
    }
    return error_end(e, &work);
}

mc_effect_renderer_status
mc_client_effect_renderer_construct(mc_client_effect_renderer *r, mc_world_runtime *world,
                                    mc_client_render_engine *render_engine,
                                    mc_effect_renderer_error *error) {
    effect_error_scope work;
    mc_effect_error *e = error_begin(r, error, &work);
    if (e == NULL)
        return MC_EFFECT_RENDERER_ALLOCATION_FAILED;
    if (r == NULL)
        fail(e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer constructor receiver", NULL,
             NULL, 0);
    else if (r->entered || r->draw_call != NULL || r->tick_call != NULL || r->block_call != NULL)
        fail(e, MC_EFFECT_RENDERER_BUSY, "EffectRenderer constructor entered", NULL, NULL, 0);
    else if (r->phase != MC_EFFECT_RENDERER_RAW)
        fail(e, MC_EFFECT_RENDERER_INVALID_PHASE, "EffectRenderer RAW receiver", NULL, NULL, 0);
    else if (world != NULL && mc_world_runtime_random_profile(world) != r->classes->profile)
        fail(e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer World profile", NULL, NULL, 0);
    else {
        r->entered = true;
        r->phase = MC_EFFECT_RENDERER_CONSTRUCTING;
        r->constructor_pc = 1; /* Object constructor. */
        r->layers = allocate(r, 4 * sizeof(*r->layers), e, "EffectRenderer List array", "<init>",
                             "(Lfd;Lji;)V", 6);
        if (r->layers == NULL)
            goto finished;
        memset(r->layers, 0, 4 * sizeof(*r->layers));
        r->constructor_pc = 9;
        r->random_candidate =
            allocate(r, sizeof(*r->random_candidate), e,
                     "EffectRenderer ordinary Random allocation", "<init>", "(Lfd;Lji;)V", 13);
        if (r->random_candidate == NULL)
            goto finished;
        e->random = mc_java_random_construct_default(r->classes->profile, r->random_candidate);
        if (e->random != MC_JAVA_RANDOM_PROFILE_OK) {
            fail(e, MC_EFFECT_RENDERER_RANDOM_FAILED, "EffectRenderer ordinary Random constructor",
                 "<init>", "(Lfd;Lji;)V", 17);
            goto finished;
        }
        r->random = r->random_candidate;
        r->random_candidate = NULL;
        r->constructor_pc = 20;
        if (world != NULL) {
            if (!mc_world_runtime_binding_retain(world)) {
                fail(e, MC_EFFECT_RENDERER_WORLD_FAILED, "EffectRenderer reached World binding",
                     "<init>", "(Lfd;Lji;)V", 29);
                goto finished;
            }
            r->world = world;
            r->constructor_pc = 29;
        }
        r->render_engine = render_engine;
        r->constructor_pc = 34;
        for (size_t i = 0; i < 4; ++i) {
            mc_effect_particle_list *list =
                allocate(r, sizeof(*list), e, "EffectRenderer distinct ArrayList", "<init>",
                         "(Lfd;Lji;)V", 49);
            if (list == NULL) {
                e->layer = i;
                goto finished;
            }
            *list = (mc_effect_particle_list){0};
            r->layers[i] = list;
            r->constructor_pc = 56;
        }
        r->constructor_pc = 63;
    finished:
        r->phase = e->status == MC_EFFECT_RENDERER_OK ? MC_EFFECT_RENDERER_READY
                                                      : MC_EFFECT_RENDERER_FAILED_CONSTRUCTION;
        r->entered = false;
    }
    return error_end(e, &work);
}

static void remove_at(mc_effect_particle_list *list, size_t index) {
    mc_entity *particle = list->elements[index];
    --list->size;
    memmove(list->elements + index, list->elements + index + 1,
            (list->size - index) * sizeof(*list->elements));
    list->elements[list->size] = NULL;
    mc_entity_release(particle); /* Detach before its actual native retirement. */
}

static bool grow(mc_client_effect_renderer *r, mc_effect_particle_list *list, mc_effect_error *e) {
    if (list->size < list->capacity)
        return true;
    const size_t capacity = list->capacity == 0 ? 10 : list->capacity + list->capacity / 2;
    if (capacity <= list->capacity || capacity > SIZE_MAX / sizeof(*list->elements)) {
        fail(e, MC_EFFECT_RENDERER_LIMIT, "EffectRenderer ArrayList capacity", "a", "(Lxw;)V", 42);
        return false;
    }
    mc_entity **elements = allocate(r, capacity * sizeof(*elements), e,
                                    "EffectRenderer ArrayList growth", "a", "(Lxw;)V", 42);
    if (elements == NULL)
        return false;
    memset(elements, 0, capacity * sizeof(*elements));
    if (list->size != 0)
        memcpy(elements, list->elements, list->size * sizeof(*elements));
    free(list->elements);
    r->allocation_bytes -= list->capacity * sizeof(*elements);
    list->elements = elements;
    list->capacity = capacity;
    return true;
}

static void add_entered(mc_client_effect_renderer *r, mc_entity *particle, mc_effect_error *e) {
    if (particle == NULL)
        fail(e, MC_EFFECT_RENDERER_NULL_REFERENCE, "EffectRenderer null particle", "a", "(Lxw;)V",
             1);
    else if (particle->classes != r->classes)
        fail(e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer particle class owner", NULL,
             NULL, 0);
    else if (!mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE) &&
             !mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_ITEM_PARTICLE) &&
             !mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE) &&
             !mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE))
        fail(e, MC_EFFECT_RENDERER_UNSUPPORTED_PARTICLE, "EffectRenderer concrete c_ closure", "a",
             "(Lxw;)V", 1);
    else if (particle->phase != MC_ENTITY_READY)
        fail(e, MC_EFFECT_RENDERER_INVALID_PHASE, "EffectRenderer READY particle", NULL, NULL, 0);
    else {
        int32_t layer = 0;
        /* The API caller owns the evaluated argument throughout this
         * call, independently of any max4000 List eviction. */
        e->entity = mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE)
                        ? mc_pickup_particle_layer(particle, &layer)
                    : mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE)
                        ? mc_explosion_particle_layer(particle, &layer)
                    : mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE)
                        ? mc_digging_particle_layer(particle, &layer)
                        : mc_item_particle_layer(particle, &layer);
        if (e->entity != MC_ENTITY_OK) {
            if (e->status == MC_EFFECT_RENDERER_OK)
                fail(e, MC_EFFECT_RENDERER_ENTITY_FAILED, "EffectRenderer concrete c_", "a",
                     "(Lxw;)V", 1);
        } else {
            e->layer = (size_t)layer;
            mc_effect_particle_list *list = r->layers[e->layer];
            if (list->size >= 4000)
                remove_at(list, 0);
            if (grow(r, list, e)) {
                e->entity = mc_entity_retain(particle);
                if (e->entity != MC_ENTITY_OK)
                    fail(e, MC_EFFECT_RENDERER_ENTITY_FAILED,
                         "EffectRenderer List occurrence native hold", "a", "(Lxw;)V", 42);
                else
                    list->elements[list->size++] = particle;
            }
        }
    }
}

mc_effect_renderer_status mc_client_effect_renderer_add(mc_client_effect_renderer *r,
                                                        mc_entity *particle,
                                                        mc_effect_renderer_error *error) {
    effect_error_scope work;
    mc_effect_error *e = error_begin(r, error, &work);
    if (enter(r, e)) {
        add_entered(r, particle, e);
        r->entered = false;
        if (e->status == MC_EFFECT_RENDERER_OK)
            *e = (mc_effect_error){.layer = SIZE_MAX, .index = SIZE_MAX};
    }
    return error_end(e, &work);
}

typedef struct mc_effect_tick_call {
    mc_effect_renderer_error failure, cleanup;
    mc_item_particle_tick_error item;
    mc_entity *current;
    mc_world_access_failure receipts[16];
    size_t receipt_count;
} mc_effect_tick_call;

static bool tick_capture(mc_effect_tick_call *call, const mc_world_access_failure *source) {
    if (source->receipt == NULL)
        return true;
    for (size_t i = 0; i < call->receipt_count; ++i)
        if (call->receipts[i].receipt == source->receipt)
            return true;
    if (call->receipt_count == sizeof(call->receipts) / sizeof(*call->receipts))
        return false;
    if (mc_world_access_failure_retain(source, &call->receipts[call->receipt_count]) !=
        MC_WORLD_STORAGE_OK)
        return false;
    ++call->receipt_count;
    return true;
}

static bool tick_capture_nearby(mc_effect_tick_call *c, const mc_world_nearby_error *n) {
    return tick_capture(c, &n->batch.publication.world_access_failure) &&
           tick_capture(c, &n->tile_entity.world_access_failure) &&
           tick_capture(c, &n->tile_entity.publication.world_access_failure) &&
           tick_capture(c, &n->tile_entity.batch.publication.world_access_failure);
}

static bool tick_capture_mutation(mc_effect_tick_call *c, const mc_world_mutation_error *m) {
    return tick_capture(c, &m->world_access_failure) &&
           tick_capture(c, &m->publication.world_access_failure) &&
           tick_capture(c, &m->batch.publication.world_access_failure) &&
           tick_capture(c, &m->tile_entity.world_access_failure) &&
           tick_capture(c, &m->tile_entity.publication.world_access_failure) &&
           tick_capture(c, &m->tile_entity.batch.publication.world_access_failure) &&
           tick_capture_nearby(c, &m->nearby);
}

static bool tick_capture_world(mc_effect_tick_call *c, const mc_world_movement_error *m) {
    return tick_capture(c, &m->world_access_failure) &&
           tick_capture_mutation(c, &m->presentation) && tick_capture_nearby(c, &m->block.nearby);
}

static bool tick_capture_movement(mc_effect_tick_call *c) {
    if (c->failure.movement == NULL)
        return true;
    const mc_entity_move_error *m = &c->item.movement;
    return tick_capture_nearby(c, &m->collision.nearby) &&
           tick_capture_nearby(c, &m->block_read.nearby) && tick_capture_world(c, &m->world) &&
           tick_capture_world(c, &m->effect.world) &&
           tick_capture_nearby(c, &m->contact.metadata.nearby) &&
           tick_capture_world(c, &m->contact.effect.world) &&
           tick_capture_mutation(c, &m->contact.mutation) &&
           tick_capture_nearby(c, &m->step.block_read.nearby) &&
           tick_capture_mutation(c, &m->step.mutation);
}

mc_effect_renderer_status mc_client_effect_renderer_tick_in_context(
    mc_client_effect_renderer *r, mc_registry_classes *registry,
    const mc_entity_move_limits *limits, mc_effect_renderer_error *error) {
    effect_error_scope work;
    mc_effect_error *e = error_begin(r, error, &work);
    if (!enter(r, e))
        return error_end(e, &work);
    mc_effect_tick_call *call = NULL;
    for (size_t layer = 0; layer < 4 && e->status == MC_EFFECT_RENDERER_OK; ++layer) {
        mc_effect_particle_list *list = r->layers[layer];
        for (size_t index = 0; index < list->size;) {
            mc_entity *particle = list->elements[index];
            e->layer = layer;
            e->index = index;
            if (error != NULL)
                mc_particle_error_init(&error->particle);
            if (particle == NULL) {
                fail(e, MC_EFFECT_RENDERER_NULL_REFERENCE,
                     "EffectRenderer null concrete tick receiver", "a", "()V", 41);
                break;
            }
            const bool digging = mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE);
            const bool explosion =
                mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE);
            const bool item = digging || explosion ||
                              mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_ITEM_PARTICLE);
            if (particle->classes != r->classes ||
                (!item && !mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE))) {
                fail(e, MC_EFFECT_RENDERER_UNSUPPORTED_PARTICLE,
                     "EffectRenderer concrete tick closure", "a", "()V", 41);
                break;
            }
            if (!item) {
                /* Preserve the old empty/EM-only path, including its quota.
                 * EM has no movement callback and needs no retained PB frame. */
                e->entity =
                    mc_pickup_particle_tick(particle, error == NULL ? NULL : &error->particle);
                if (e->entity != MC_ENTITY_OK) {
                    fail(e, MC_EFFECT_RENDERER_ENTITY_FAILED, "EffectRenderer concrete tick", "a",
                         "()V", 41);
                    break;
                }
                if (particle->base.be)
                    remove_at(list, index);
                else
                    ++index;
                continue;
            }
            if (call == NULL) {
                call = allocate(r, sizeof(*call), e,
                                "EffectRenderer reached PB tick diagnostic frame", "a", "()V", 41);
                if (call == NULL)
                    break;
                memset(call, 0, sizeof(*call));
                r->tick_call = call;
            }
            error_init(&call->failure);
            error_init(&call->cleanup);
            call->failure.layer = layer;
            call->failure.index = index;
            call->failure.entity = mc_entity_retain(particle);
            if (call->failure.entity != MC_ENTITY_OK) {
                full_fail(&call->failure, MC_EFFECT_RENDERER_ENTITY_FAILED,
                          "EffectRenderer tick current native hold", "a", "()V", 41);
            } else {
                call->current = particle;
                call->failure.entity =
                    explosion ? mc_explosion_particle_tick(particle, registry, limits, &call->item)
                    : digging ? mc_digging_particle_tick(particle, registry, limits, &call->item)
                              : mc_item_particle_tick(particle, registry, limits, &call->item);
                call->failure.particle = call->item.particle;
                if (call->item.particle.cause == MC_PARTICLE_MOVEMENT)
                    call->failure.movement = &call->item.movement;
                if (call->failure.entity != MC_ENTITY_OK) {
                    full_fail(&call->failure, MC_EFFECT_RENDERER_ENTITY_FAILED,
                              "EffectRenderer concrete tick", "a", "()V", 41);
                    call->failure.native_policy = call->failure.particle.native_policy;
                    call->failure.source_exception = call->failure.particle.source_exception;
                    call->failure.exception_class = call->failure.particle.exception_class;
                }
            }
            if (call->failure.status != MC_EFFECT_RENDERER_OK) {
                if (!tick_capture_movement(call))
                    full_fail(&call->cleanup, MC_EFFECT_RENDERER_BUSY,
                              "EffectRenderer failed tick producer capture", NULL, NULL, 0);
                if (error != NULL)
                    *error = call->failure;
                r->entered = false;
                return call->failure.status;
            }
            if (particle->base.be)
                remove_at(list, index);
            else
                ++index;
            mc_entity_release(call->current);
            call->current = NULL;
        }
    }
    r->entered = false;
    if (call != NULL) {
        r->tick_call = NULL;
        r->allocation_bytes -= sizeof(*call);
        free(call);
    }
    if (e->status == MC_EFFECT_RENDERER_OK)
        *e = (mc_effect_error){.layer = SIZE_MAX, .index = SIZE_MAX};
    return error_end(e, &work);
}

mc_effect_renderer_status mc_client_effect_renderer_tick(mc_client_effect_renderer *r,
                                                         mc_effect_renderer_error *error) {
    return mc_client_effect_renderer_tick_in_context(r, NULL, NULL, error);
}

mc_effect_renderer_status mc_client_effect_renderer_tick_cleanup(mc_client_effect_renderer *r,
                                                                 mc_effect_renderer_error *error) {
    if (error != NULL)
        error_init(error);
    if (r == NULL)
        return MC_EFFECT_RENDERER_OK;
    if (r->entered || r->draw_call != NULL) {
        if (error != NULL)
            full_fail(error, MC_EFFECT_RENDERER_BUSY, "EffectRenderer tick cleanup entered", NULL,
                      NULL, 0);
        return MC_EFFECT_RENDERER_BUSY;
    }
    mc_effect_tick_call *call = r->tick_call;
    if (call == NULL)
        return MC_EFFECT_RENDERER_OK;
    /* Failed captures retry only native ownership, preserving first failure. */
    if (!tick_capture_movement(call)) {
        if (error != NULL)
            full_fail(error, MC_EFFECT_RENDERER_BUSY, "EffectRenderer tick receipt cleanup capture",
                      NULL, NULL, 0);
        return MC_EFFECT_RENDERER_BUSY;
    }
    while (call->receipt_count != 0)
        mc_world_access_failure_release(&call->receipts[--call->receipt_count]);
    mc_entity_release(call->current);
    call->current = NULL;
    r->tick_call = NULL;
    r->allocation_bytes -= sizeof(*call);
    free(call);
    return MC_EFFECT_RENDERER_OK;
}

static void clear(mc_effect_particle_list *list) {
    if (list == NULL)
        return;
    const size_t size = list->size;
    list->size = 0;
    for (size_t i = 0; i < size; ++i) {
        mc_entity *particle = list->elements[i];
        list->elements[i] = NULL;
        mc_entity_release(particle);
    }
}

mc_effect_renderer_status mc_client_effect_renderer_change_world(mc_client_effect_renderer *r,
                                                                 mc_world_runtime *world,
                                                                 mc_effect_renderer_error *error) {
    effect_error_scope work;
    mc_effect_error *e = error_begin(r, error, &work);
    if (e == NULL)
        return MC_EFFECT_RENDERER_ALLOCATION_FAILED;
    if (enter(r, e)) {
        if (world != NULL && mc_world_runtime_random_profile(world) != r->classes->profile)
            fail(e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer new World profile", NULL,
                 NULL, 0);
        else if (world != NULL && !mc_world_runtime_binding_retain(world))
            fail(e, MC_EFFECT_RENDERER_WORLD_FAILED, "EffectRenderer new World hold", "a",
                 "(Lfd;)V", 2);
        else {
            mc_world_runtime *old = r->world;
            r->world = world;
            mc_world_runtime_binding_release(old);
            for (size_t layer = 0; layer < 4; ++layer)
                clear(r->layers[layer]);
        }
        r->entered = false;
    }
    return error_end(e, &work);
}

bool mc_client_effect_renderer_read(const mc_client_effect_renderer *r,
                                    mc_effect_renderer_snapshot *out) {
    if (r == NULL || out == NULL)
        return false;
    *out = (mc_effect_renderer_snapshot){.phase = r->phase,
                                         .constructor_pc = r->constructor_pc,
                                         .world = r->world,
                                         .render_engine = r->render_engine,
                                         .layers = r->layers,
                                         .allocation_bytes = r->allocation_bytes,
                                         .random = r->random,
                                         .tick_pending = r->tick_call != NULL,
                                         .tick_current =
                                             r->tick_call == NULL ? NULL : r->tick_call->current};
    if (r->layers != NULL)
        for (size_t i = 0; i < 4; ++i) {
            out->lists[i] = r->layers[i];
            if (r->layers[i] != NULL) {
                out->sizes[i] = r->layers[i]->size;
                out->capacities[i] = r->layers[i]->capacity;
            }
        }
    return true;
}

/* Terminal inspection needs only compact value diagnostics. Do not create the
 * complete particle/movement error on the native stack, even for NULL error. */
static mc_effect_renderer_status terminal_report(mc_effect_renderer_error *error,
                                                 mc_effect_renderer_status status,
                                                 const char *operation, size_t layer) {
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
        error->status = status;
        error->operation = operation;
        error->layer = layer;
        error->index = SIZE_MAX;
        error->native_policy = status != MC_EFFECT_RENDERER_OK;
    }
    return status;
}

mc_effect_renderer_status
mc_client_effect_renderer_terminal_inspect(const mc_client_effect_renderer *r,
                                           mc_effect_renderer_terminal_view *out,
                                           mc_effect_renderer_error *error) {
    if (r == NULL || out == NULL)
        return terminal_report(error, MC_EFFECT_RENDERER_INVALID_ARGUMENT,
                               "EffectRenderer terminal inspection arguments", SIZE_MAX);
    if (r->entered || r->draw_call != NULL || r->tick_call != NULL || r->block_call != NULL)
        return terminal_report(error, MC_EFFECT_RENDERER_BUSY,
                               "EffectRenderer terminal inspection entered", SIZE_MAX);
    if (r->phase != MC_EFFECT_RENDERER_READY)
        return terminal_report(error, MC_EFFECT_RENDERER_INVALID_PHASE,
                               "EffectRenderer terminal inspection READY phase", SIZE_MAX);
    if (r->classes == NULL || r->classes->source != MC_ENTITY_CLIENT || r->holds == 0 ||
        r->layers == NULL || r->random == NULL || r->random_candidate != NULL)
        return terminal_report(error, MC_EFFECT_RENDERER_INVALID_PHASE,
                               "EffectRenderer terminal inspection native ownership", SIZE_MAX);
    mc_effect_renderer_terminal_view view = {.classes = r->classes,
                                             .render_engine = r->render_engine,
                                             .world_slot = &r->world,
                                             .holds = r->holds};
    for (size_t i = 0; i < 4; ++i) {
        const mc_effect_particle_list *list = r->layers[i];
        if (list == NULL || list->size > list->capacity ||
            list->capacity > SIZE_MAX / sizeof(*list->elements) ||
            (list->capacity == 0) != (list->elements == NULL))
            return terminal_report(error, MC_EFFECT_RENDERER_INVALID_PHASE,
                                   "EffectRenderer terminal inspection List storage", i);
        for (size_t j = 0; j < i; ++j)
            if (list == r->layers[j] ||
                (list->elements != NULL && list->elements == r->layers[j]->elements))
                return terminal_report(error, MC_EFFECT_RENDERER_INVALID_PHASE,
                                       "EffectRenderer terminal inspection distinct Lists", i);
        for (size_t j = 0; j < list->size; ++j) {
            const mc_entity *particle = list->elements[j];
            mc_particle_snapshot state;
            const bool digging = mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE);
            const bool explosion =
                mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE);
            if ((explosion ? i != 0
                 : digging ? i != 1
                           : i != 2) ||
                (!digging && !explosion &&
                 !mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_ITEM_PARTICLE)) ||
                particle->classes != r->classes || particle->holds == 0 ||
                particle->base.world != r->world || !mc_particle_read(particle, &state) ||
                state.phase != MC_ENTITY_READY ||
                state.item_pc != (explosion ? 157u
                                  : digging ? 80u
                                            : 58u) ||
                (digging && (state.digging_block == NULL || state.digging_registry == NULL)))
                return terminal_report(error, MC_EFFECT_RENDERER_UNSUPPORTED_PARTICLE,
                                       "EffectRenderer terminal inspection canonical PB closure",
                                       i);
            if (state.tick_active)
                return terminal_report(error, MC_EFFECT_RENDERER_BUSY,
                                       "EffectRenderer terminal inspection PB tick entered", i);
        }
        if (list->size > SIZE_MAX - view.particle_occurrences)
            return terminal_report(error, MC_EFFECT_RENDERER_LIMIT,
                                   "EffectRenderer terminal particle occurrence overflow", i);
        view.lists[i] = list;
        view.particle_occurrences += list->size;
    }
    *out = view;
    return terminal_report(error, MC_EFFECT_RENDERER_OK, NULL, SIZE_MAX);
}

mc_effect_renderer_status mc_client_effect_renderer_retire_world(mc_client_effect_renderer *r,
                                                                 mc_world_runtime *expected,
                                                                 mc_effect_renderer_error *error) {
    effect_error_scope work;
    mc_effect_error *e = error_begin(r, error, &work);
    if (e == NULL)
        return MC_EFFECT_RENDERER_ALLOCATION_FAILED;
    if (r == NULL)
        fail(e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer native World owner", NULL,
             NULL, 0);
    else if (r->entered || r->draw_call != NULL || r->tick_call != NULL || r->block_call != NULL ||
             r->holds != 1)
        fail(e, MC_EFFECT_RENDERER_BUSY, "EffectRenderer native World occurrence", NULL, NULL, 0);
    else if (r->world != expected)
        fail(e, MC_EFFECT_RENDERER_INVALID_PHASE, "EffectRenderer expected World changed", NULL,
             NULL, 0);
    else {
        if (r->layers != NULL)
            for (size_t i = 0; i < 4; ++i)
                clear(r->layers[i]);
        r->world = NULL;
        mc_world_runtime_binding_release(expected);
    }
    return error_end(e, &work);
}

mc_entity *mc_client_effect_renderer_particle(const mc_client_effect_renderer *r, size_t layer,
                                              size_t index) {
    return r == NULL || r->phase != MC_EFFECT_RENDERER_READY || layer >= 4 ||
                   index >= r->layers[layer]->size
               ? NULL
               : r->layers[layer]->elements[index];
}

mc_effect_renderer_status mc_client_effect_renderer_retain(mc_client_effect_renderer *r) {
    if (r == NULL)
        return MC_EFFECT_RENDERER_INVALID_ARGUMENT;
    if (r->entered)
        return MC_EFFECT_RENDERER_BUSY;
    if (r->holds == SIZE_MAX)
        return MC_EFFECT_RENDERER_LIMIT;
    ++r->holds;
    return MC_EFFECT_RENDERER_OK;
}

mc_effect_renderer_status mc_client_effect_renderer_release(mc_client_effect_renderer *r) {
    if (r == NULL)
        return MC_EFFECT_RENDERER_INVALID_ARGUMENT;
    if (r->entered)
        return MC_EFFECT_RENDERER_BUSY;
    if (r->holds <= 1)
        return MC_EFFECT_RENDERER_INVALID_PHASE;
    --r->holds;
    return MC_EFFECT_RENDERER_OK;
}

mc_effect_renderer_status mc_client_effect_renderer_free(mc_client_effect_renderer **owner) {
    if (owner == NULL || *owner == NULL)
        return MC_EFFECT_RENDERER_OK;
    mc_client_effect_renderer *r = *owner;
    if (r->entered || r->draw_call != NULL || r->tick_call != NULL || r->block_call != NULL ||
        r->holds != 1)
        return MC_EFFECT_RENDERER_BUSY;
    if (r->layers != NULL) {
        for (size_t i = 0; i < 4; ++i) {
            mc_effect_particle_list *list = r->layers[i];
            if (list != NULL) {
                clear(list);
                free(list->elements);
                free(list);
            }
        }
        free(r->layers);
    }
    mc_world_runtime_binding_release(r->world);
    free(r->random);
    free(r->random_candidate);
    (void)mc_entity_classes_release(r->classes);
    free(r);
    *owner = NULL;
    return MC_EFFECT_RENDERER_OK;
}

#include "block_effects.inc"
