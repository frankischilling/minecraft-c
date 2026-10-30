#include "../../entity/entity_internal.h"
#include "effect_renderer_internal.h"

#include <stdlib.h>
#include <string.h>

static mc_effect_renderer_error fresh(void) {
    return (mc_effect_renderer_error){.layer = SIZE_MAX, .index = SIZE_MAX};
}

static mc_effect_renderer_status fail(mc_effect_renderer_error *e, mc_effect_renderer_status status,
                                      const char *operation, const char *method,
                                      const char *descriptor, unsigned int pc) {
    e->status = status;
    e->operation = operation;
    e->source_method = method;
    e->source_descriptor = descriptor;
    e->source_pc = pc;
    return status;
}

static void *allocate(mc_client_effect_renderer *r, size_t bytes, mc_effect_renderer_error *e,
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

static bool enter(mc_client_effect_renderer *r, mc_effect_renderer_error *e) {
    if (r == NULL)
        fail(e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer receiver", NULL, NULL, 0);
    else if (r->entered)
        fail(e, MC_EFFECT_RENDERER_BUSY, "EffectRenderer operation entered", NULL, NULL, 0);
    else if (r->phase != MC_EFFECT_RENDERER_READY)
        fail(e, MC_EFFECT_RENDERER_INVALID_PHASE, "EffectRenderer READY receiver", NULL, NULL, 0);
    else {
        r->entered = true;
        return true;
    }
    return false;
}

static mc_effect_renderer_status publish(mc_effect_renderer_error *e,
                                         mc_effect_renderer_error *out) {
    if (out != NULL)
        *out = *e;
    return e->status;
}

mc_effect_renderer_status mc_client_effect_renderer_allocate(
    mc_entity_classes *classes, const mc_effect_renderer_options *options,
    mc_client_effect_renderer **out, mc_effect_renderer_error *error) {
    mc_effect_renderer_error e = fresh();
    const size_t limit = options == NULL ? 16u * 1024u * 1024u : options->max_allocation_bytes;
    if (classes == NULL || out == NULL || classes->source != MC_ENTITY_CLIENT)
        fail(&e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer allocation arguments", NULL,
             NULL, 0);
    else if (limit < sizeof(mc_client_effect_renderer))
        fail(&e, MC_EFFECT_RENDERER_LIMIT, "EffectRenderer allocation budget", NULL, NULL, 0);
    else {
        e.entity = mc_entity_classes_retain(classes, MC_ENTITY_CLIENT, classes->profile);
        if (e.entity != MC_ENTITY_OK)
            fail(&e, MC_EFFECT_RENDERER_ENTITY_FAILED, "EffectRenderer class binding", NULL, NULL,
                 0);
        else {
            mc_client_effect_renderer *r = malloc(sizeof(*r));
            if (r == NULL) {
                (void)mc_entity_classes_release(classes);
                fail(&e, MC_EFFECT_RENDERER_ALLOCATION_FAILED, "EffectRenderer allocation", NULL,
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
    return publish(&e, error);
}

mc_effect_renderer_status
mc_client_effect_renderer_construct(mc_client_effect_renderer *r, mc_world_runtime *world,
                                    mc_client_render_engine *render_engine,
                                    mc_effect_renderer_error *error) {
    mc_effect_renderer_error e = fresh();
    if (r == NULL)
        fail(&e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer constructor receiver", NULL,
             NULL, 0);
    else if (r->entered)
        fail(&e, MC_EFFECT_RENDERER_BUSY, "EffectRenderer constructor entered", NULL, NULL, 0);
    else if (r->phase != MC_EFFECT_RENDERER_RAW)
        fail(&e, MC_EFFECT_RENDERER_INVALID_PHASE, "EffectRenderer RAW receiver", NULL, NULL, 0);
    else if (world != NULL && mc_world_runtime_random_profile(world) != r->classes->profile)
        fail(&e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer World profile", NULL, NULL,
             0);
    else {
        r->entered = true;
        r->phase = MC_EFFECT_RENDERER_CONSTRUCTING;
        r->constructor_pc = 1; /* Object constructor. */
        r->layers = allocate(r, 4 * sizeof(*r->layers), &e, "EffectRenderer List array", "<init>",
                             "(Lfd;Lji;)V", 6);
        if (r->layers == NULL)
            goto finished;
        memset(r->layers, 0, 4 * sizeof(*r->layers));
        r->constructor_pc = 9;
        r->random_candidate =
            allocate(r, sizeof(*r->random_candidate), &e,
                     "EffectRenderer ordinary Random allocation", "<init>", "(Lfd;Lji;)V", 13);
        if (r->random_candidate == NULL)
            goto finished;
        e.random = mc_java_random_construct_default(r->classes->profile, r->random_candidate);
        if (e.random != MC_JAVA_RANDOM_PROFILE_OK) {
            fail(&e, MC_EFFECT_RENDERER_RANDOM_FAILED, "EffectRenderer ordinary Random constructor",
                 "<init>", "(Lfd;Lji;)V", 17);
            goto finished;
        }
        r->random = r->random_candidate;
        r->random_candidate = NULL;
        r->constructor_pc = 20;
        if (world != NULL) {
            if (!mc_world_runtime_binding_retain(world)) {
                fail(&e, MC_EFFECT_RENDERER_WORLD_FAILED, "EffectRenderer reached World binding",
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
                allocate(r, sizeof(*list), &e, "EffectRenderer distinct ArrayList", "<init>",
                         "(Lfd;Lji;)V", 49);
            if (list == NULL) {
                e.layer = i;
                goto finished;
            }
            *list = (mc_effect_particle_list){0};
            r->layers[i] = list;
            r->constructor_pc = 56;
        }
        r->constructor_pc = 63;
    finished:
        r->phase = e.status == MC_EFFECT_RENDERER_OK ? MC_EFFECT_RENDERER_READY
                                                     : MC_EFFECT_RENDERER_FAILED_CONSTRUCTION;
        r->entered = false;
    }
    return publish(&e, error);
}

static void remove_at(mc_effect_particle_list *list, size_t index) {
    mc_entity *particle = list->elements[index];
    --list->size;
    memmove(list->elements + index, list->elements + index + 1,
            (list->size - index) * sizeof(*list->elements));
    list->elements[list->size] = NULL;
    mc_entity_release(particle); /* Detach before its actual native retirement. */
}

static bool grow(mc_client_effect_renderer *r, mc_effect_particle_list *list,
                 mc_effect_renderer_error *e) {
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

mc_effect_renderer_status mc_client_effect_renderer_add(mc_client_effect_renderer *r,
                                                        mc_entity *particle,
                                                        mc_effect_renderer_error *error) {
    mc_effect_renderer_error e = fresh();
    if (enter(r, &e)) {
        if (particle == NULL)
            fail(&e, MC_EFFECT_RENDERER_NULL_REFERENCE, "EffectRenderer null particle", "a",
                 "(Lxw;)V", 1);
        else if (particle->classes != r->classes)
            fail(&e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer particle class owner",
                 NULL, NULL, 0);
        else if (!mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE))
            fail(&e, MC_EFFECT_RENDERER_UNSUPPORTED_PARTICLE, "EffectRenderer concrete c_ closure",
                 "a", "(Lxw;)V", 1);
        else if (particle->phase != MC_ENTITY_READY)
            fail(&e, MC_EFFECT_RENDERER_INVALID_PHASE, "EffectRenderer READY particle", NULL, NULL,
                 0);
        else {
            int32_t layer;
            e.entity = mc_pickup_particle_layer(particle, &layer);
            if (e.entity != MC_ENTITY_OK)
                fail(&e, MC_EFFECT_RENDERER_ENTITY_FAILED, "EffectRenderer concrete c_", "a",
                     "(Lxw;)V", 1);
            else {
                e.layer = (size_t)layer;
                mc_effect_particle_list *list = r->layers[e.layer];
                if (list->size >= 4000)
                    remove_at(list, 0);
                if (grow(r, list, &e)) {
                    e.entity = mc_entity_retain(particle);
                    if (e.entity != MC_ENTITY_OK)
                        fail(&e, MC_EFFECT_RENDERER_ENTITY_FAILED,
                             "EffectRenderer List occurrence native hold", "a", "(Lxw;)V", 42);
                    else
                        list->elements[list->size++] = particle;
                }
            }
        }
        r->entered = false;
        if (e.status == MC_EFFECT_RENDERER_OK)
            e = fresh();
    }
    return publish(&e, error);
}

mc_effect_renderer_status mc_client_effect_renderer_tick(mc_client_effect_renderer *r,
                                                         mc_effect_renderer_error *error) {
    mc_effect_renderer_error e = fresh();
    if (enter(r, &e)) {
        for (size_t layer = 0; layer < 4 && e.status == MC_EFFECT_RENDERER_OK; ++layer) {
            mc_effect_particle_list *list = r->layers[layer];
            for (size_t index = 0; index < list->size;) {
                mc_entity *particle = list->elements[index];
                e.layer = layer;
                e.index = index;
                e.entity = mc_pickup_particle_tick(particle, &e.particle);
                if (e.entity != MC_ENTITY_OK) {
                    fail(&e, MC_EFFECT_RENDERER_ENTITY_FAILED, "EffectRenderer concrete tick", "a",
                         "()V", 41);
                    break;
                }
                if (particle->base.be)
                    remove_at(list, index);
                else
                    ++index;
            }
        }
        r->entered = false;
        if (e.status == MC_EFFECT_RENDERER_OK)
            e = fresh();
    }
    return publish(&e, error);
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
    mc_effect_renderer_error e = fresh();
    if (enter(r, &e)) {
        if (world != NULL && mc_world_runtime_random_profile(world) != r->classes->profile)
            fail(&e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer new World profile", NULL,
                 NULL, 0);
        else if (world != NULL && !mc_world_runtime_binding_retain(world))
            fail(&e, MC_EFFECT_RENDERER_WORLD_FAILED, "EffectRenderer new World hold", "a",
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
    return publish(&e, error);
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
                                         .random = r->random};
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

mc_effect_renderer_status
mc_client_effect_renderer_terminal_inspect(const mc_client_effect_renderer *r,
                                           mc_effect_renderer_terminal_view *out,
                                           mc_effect_renderer_error *error) {
    mc_effect_renderer_error e = fresh();
    if (r == NULL || out == NULL)
        fail(&e, MC_EFFECT_RENDERER_INVALID_ARGUMENT,
             "EffectRenderer terminal inspection arguments", NULL, NULL, 0);
    else if (r->entered)
        fail(&e, MC_EFFECT_RENDERER_BUSY, "EffectRenderer terminal inspection entered", NULL, NULL,
             0);
    else if (r->phase != MC_EFFECT_RENDERER_READY)
        fail(&e, MC_EFFECT_RENDERER_INVALID_PHASE, "EffectRenderer terminal inspection READY phase",
             NULL, NULL, 0);
    else if (r->classes == NULL || r->classes->source != MC_ENTITY_CLIENT || r->holds == 0 ||
             r->layers == NULL || r->random == NULL || r->random_candidate != NULL)
        fail(&e, MC_EFFECT_RENDERER_INVALID_PHASE,
             "EffectRenderer terminal inspection native ownership", NULL, NULL, 0);
    else {
        for (size_t i = 0; i < 4; ++i) {
            const mc_effect_particle_list *list = r->layers[i];
            e.layer = i;
            if (list == NULL || list->size > list->capacity ||
                list->capacity > SIZE_MAX / sizeof(*list->elements) ||
                (list->capacity == 0) != (list->elements == NULL)) {
                fail(&e, MC_EFFECT_RENDERER_INVALID_PHASE,
                     "EffectRenderer terminal inspection List storage", NULL, NULL, 0);
                break;
            }
            for (size_t j = 0; j < i; ++j)
                if (list == r->layers[j] ||
                    (list->elements != NULL && list->elements == r->layers[j]->elements)) {
                    fail(&e, MC_EFFECT_RENDERER_INVALID_PHASE,
                         "EffectRenderer terminal inspection distinct Lists", NULL, NULL, 0);
                    break;
                }
            if (e.status != MC_EFFECT_RENDERER_OK)
                break;
            if (list->size != 0) {
                fail(&e, MC_EFFECT_RENDERER_UNSUPPORTED_PARTICLE,
                     "EffectRenderer terminal inspection requires empty Lists", NULL, NULL, 0);
                break;
            }
        }
        if (e.status == MC_EFFECT_RENDERER_OK) {
            *out = (mc_effect_renderer_terminal_view){.classes = r->classes,
                                                      .render_engine = r->render_engine,
                                                      .world_slot = &r->world,
                                                      .holds = r->holds};
            e = fresh();
        }
    }
    return publish(&e, error);
}

mc_effect_renderer_status mc_client_effect_renderer_retire_world(mc_client_effect_renderer *r,
                                                                 mc_world_runtime *expected,
                                                                 mc_effect_renderer_error *error) {
    mc_effect_renderer_error e = fresh();
    if (r == NULL)
        fail(&e, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "EffectRenderer native World owner", NULL,
             NULL, 0);
    else if (r->entered || r->holds != 1)
        fail(&e, MC_EFFECT_RENDERER_BUSY, "EffectRenderer native World occurrence", NULL, NULL, 0);
    else if (r->world != expected)
        fail(&e, MC_EFFECT_RENDERER_INVALID_PHASE, "EffectRenderer expected World changed", NULL,
             NULL, 0);
    else {
        if (r->layers != NULL)
            for (size_t i = 0; i < 4; ++i)
                clear(r->layers[i]);
        r->world = NULL;
        mc_world_runtime_binding_release(expected);
    }
    return publish(&e, error);
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
    if (r->entered || r->holds != 1)
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
