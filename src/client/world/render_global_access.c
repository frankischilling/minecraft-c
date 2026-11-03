#include "render_global_access.h"

#include "../../block/registry/runtime_internal.h"
#include "../../entity/entity_internal.h"
#include "../../entity/particles/particle_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../../world/runtime/world_access_internal.h"
#include "../application_state.h"
#include "../render_engine/image_buffer.h"
#include "../render_engine/image_download.h"
#include "../startup/application_owner.h"
#include "mc/block/registry/step_sound.h"
#include "mc/client/particles/effect_renderer.h"
#include "mc/client/sound/sound_manager.h"
#include "mc/entity/appearance.h"
#include "mc/world/runtime/presentation.h"
#include "mc/world/runtime/runtime.h"
#include "render_global_internal.h"
#include "source_chunk_renderer_internal.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

typedef struct access_registration {
    struct access_registration *next;
    mc_world_runtime *world;
    uint64_t identity;
} access_registration;

typedef struct access_particle {
    client_application *application;
    mc_client_effect_renderer *receiver;
    mc_entity *candidate;
    mc_world_runtime *world;
    mc_registry_classes *registry;
    mc_math_classes *math;
    mc_particle_error error;
    mc_effect_renderer_error effects, effects_cleanup;
    mc_client_application_owner_error application_error, application_cleanup;
    mc_math_class_error math_cleanup;
    bool block_work;
} access_particle;

typedef struct access_call {
    struct access_call *next;
    mc_client_render_global_error error;
    mc_world_access_receipt *receipt;
    mc_client_image_buffer *processors[2];
    mc_client_image_download_call *images[2];
    mc_client_image_buffer_error buffer_error, buffer_cleanup;
    mc_client_image_download_error image_error, image_cleanup;
    const char *source_exception_class;
    access_particle *particle;
    mc_step_sound_name effect_name;
    mc_world_mutation_error *world_effect;
    bool auxiliary2001;
    mc_render_global_auxiliary_event auxiliary;
} access_call;

struct mc_render_global_access {
    mc_client_render_global *owner;
    mc_world_access *identity;
    access_registration *registrations;
    access_call *failures;
    size_t registration_count, active_callbacks, failure_count, bytes;
    bool retiring;
    mc_render_global_auxiliary_observation auxiliary;
};

static void auxiliary_count(mc_render_global_auxiliary_observation *observation, uint64_t *value,
                            uint64_t amount) {
    if (amount > UINT64_MAX - *value) {
        *value = UINT64_MAX;
        observation->overflowed = true;
    } else
        *value += amount;
}

static mc_world_storage_status storage_status(mc_client_render_global_status status) {
    switch (status) {
        case MC_CLIENT_RENDER_GLOBAL_OK:
            return MC_WORLD_STORAGE_OK;
        case MC_CLIENT_RENDER_GLOBAL_BUSY:
            return MC_WORLD_STORAGE_BUSY;
        case MC_CLIENT_RENDER_GLOBAL_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        default:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
}

static mc_client_render_global_status owner_status(mc_world_storage_status status) {
    switch (status) {
        case MC_WORLD_STORAGE_OK:
            return MC_CLIENT_RENDER_GLOBAL_OK;
        case MC_WORLD_STORAGE_BUSY:
            return MC_CLIENT_RENDER_GLOBAL_BUSY;
        case MC_WORLD_STORAGE_LIMIT:
            return MC_CLIENT_RENDER_GLOBAL_LIMIT;
        case MC_WORLD_STORAGE_ALLOCATION_FAILED:
            return MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED;
        default:
            return MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED;
    }
}

static mc_client_render_global_status fail(mc_client_render_global_error *error,
                                           mc_client_render_global_status status,
                                           const char *operation, const char *method,
                                           const char *descriptor, unsigned int pc, bool native) {
    return mc_render_global_source_fail(error, status, operation, method == NULL ? NULL : "n",
                                        method, descriptor, pc, native);
}

static bool fp(access_call *call, const char *descriptor, unsigned int pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    fail(&call->error, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
         "RenderGlobal callback strict FP prerequisite", "a", descriptor, pc, true);
    return false;
}

/* The bridge has an independent live-byte ceiling equal to its real owner's
 * configured native constructor byte ceiling. Successful transient callbacks
 * release their frame charge; failed frames remain owned until retirement. */
static bool budget(mc_render_global_access *access, size_t bytes) {
    const size_t limit = access->owner->arena.options.max_constructor_bytes;
    return access->bytes <= limit && bytes <= limit - access->bytes;
}

static mc_client_render_global_status image_status(mc_client_image_download_status status) {
    switch (status) {
        case MC_CLIENT_IMAGE_DOWNLOAD_OK:
            return MC_CLIENT_RENDER_GLOBAL_OK;
        case MC_CLIENT_IMAGE_DOWNLOAD_BUSY:
            return MC_CLIENT_RENDER_GLOBAL_BUSY;
        case MC_CLIENT_IMAGE_DOWNLOAD_LIMIT:
            return MC_CLIENT_RENDER_GLOBAL_LIMIT;
        case MC_CLIENT_IMAGE_DOWNLOAD_ALLOCATION:
            return MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED;
        case MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_NULL:
            return MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE;
        case MC_CLIENT_IMAGE_DOWNLOAD_CLEANUP_FAILED:
            return MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED;
        default:
            return MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY;
    }
}

static void copy_image_error(access_call *call, const mc_client_image_download_error *source,
                             bool cleanup) {
    mc_client_image_download_error *image = cleanup ? &call->image_cleanup : &call->image_error;
    mc_client_image_buffer_error *buffer = cleanup ? &call->buffer_cleanup : &call->buffer_error;
    *image = *source;
    if (image->processor != NULL) {
        *buffer = *image->processor;
        image->processor = buffer;
    }
}

static bool retire_children(mc_render_global_access *access, access_call *call,
                            mc_client_render_global_error *error) {
    access_particle *particle = call->particle;
    if (particle != NULL) {
        if (particle->block_work) {
            const mc_effect_renderer_status status = mc_client_effect_renderer_block_cleanup(
                particle->receiver, &particle->effects_cleanup);
            if (status != MC_EFFECT_RENDERER_OK) {
                error->effects = &particle->effects_cleanup;
                fail(error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                     particle->effects_cleanup.operation, NULL, NULL, 0, true);
                return false;
            }
            particle->block_work = false;
        }
        if (particle->candidate != NULL) {
            mc_entity_release(particle->candidate);
            particle->candidate = NULL;
        }
        if (particle->math != NULL &&
            mc_math_classes_release(&particle->math, &particle->math_cleanup) != MC_MATH_CLASS_OK) {
            error->math = particle->math_cleanup;
            fail(error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                 "N reached particle Math occurrence retirement", NULL, NULL, 0, true);
            return false;
        }
        if (particle->registry != NULL) {
            if (!mc_registry_runtime_borrow_release(particle->registry)) {
                fail(error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                     "N particle Registry occurrence retirement", NULL, NULL, 0, true);
                return false;
            }
            particle->registry = NULL;
        }
        if (particle->world != NULL) {
            if (particle->world->persistent_bindings == 0) {
                fail(error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                     "N particle World occurrence retirement", NULL, NULL, 0, true);
                return false;
            }
            mc_world_runtime_binding_release(particle->world);
            particle->world = NULL;
        }
        if (particle->receiver != NULL) {
            const mc_effect_renderer_status status =
                mc_client_effect_renderer_release(particle->receiver);
            if (status != MC_EFFECT_RENDERER_OK) {
                particle->effects_cleanup.status = status;
                particle->effects_cleanup.operation = "N captured DN occurrence retirement";
                particle->effects_cleanup.native_policy = true;
                error->effects = &particle->effects_cleanup;
                fail(error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                     "N captured DN occurrence retirement", NULL, NULL, 0, true);
                return false;
            }
            particle->receiver = NULL;
        }
        if (particle->application != NULL &&
            mc_client_application_render_release(&particle->application,
                                                 &particle->application_cleanup) !=
                MC_CLIENT_APPLICATION_OWNER_OK) {
            fail(error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                 "N particle App occurrence retirement", NULL, NULL, 0, true);
            return false;
        }
    }
    if (mc_registry_step_sound_name_release(&call->effect_name, &error->registry) !=
        MC_REGISTRY_OK) {
        fail(error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
             "N auxiliary StepSound String occurrence retirement", NULL, NULL, 0, true);
        return false;
    }
    if (call->world_effect != NULL) {
        free(call->world_effect);
        call->world_effect = NULL;
        access->bytes -= sizeof(mc_world_mutation_error);
    }
    error->auxiliary_world = NULL;
    for (size_t i = 0; i < 2; ++i) {
        if (call->images[i] != NULL) {
            const mc_client_image_download_status status =
                mc_client_image_download_call_retire(&call->images[i]);
            if (status != MC_CLIENT_IMAGE_DOWNLOAD_OK) {
                fail(error, image_status(status), "native owned image-factory call retirement",
                     NULL, NULL, 0, true);
                mc_client_image_download_call_state state = {0};
                if (call->images[i] != NULL &&
                    mc_client_image_download_call_inspect(call->images[i], &state) &&
                    state.cleanup != NULL) {
                    copy_image_error(call, state.cleanup, true);
                    error->image_download = &call->image_cleanup;
                }
                return false;
            }
        }
        const mc_client_image_buffer_status status =
            mc_client_image_buffer_release(&call->processors[i], &call->buffer_cleanup);
        if (status != MC_CLIENT_IMAGE_BUFFER_OK) {
            fail(error,
                 status == MC_CLIENT_IMAGE_BUFFER_BUSY ? MC_CLIENT_RENDER_GLOBAL_BUSY
                                                       : MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                 "native owned rr occurrence retirement", NULL, NULL, 0, true);
            error->image_buffer = &call->buffer_cleanup;
            return false;
        }
    }
    return true;
}

static access_call *call_begin(mc_render_global_access *access, const char *descriptor,
                               mc_world_storage_error *error, mc_world_access_failure *failure) {
    *failure = (mc_world_access_failure){0};
    if (access->active_callbacks == SIZE_MAX ||
        !budget(access, sizeof(access_call) + sizeof(mc_world_access_receipt))) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                          "RenderGlobal callback native frame limit", 0};
        return NULL;
    }
    access_call *call = calloc(1, sizeof(*call));
    if (call == NULL) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                          "RenderGlobal callback native frame allocation", 0};
        return NULL;
    }
    mc_world_access_failure unpublished = {0};
    if (mc_world_access_receipt_create(access->identity, &call->error, &call->receipt, &unpublished,
                                       error) != MC_WORLD_STORAGE_OK) {
        free(call);
        return NULL;
    }
    access->bytes += sizeof(*call) + sizeof(*call->receipt);
    ++access->active_callbacks;
    if (access->retiring || access->owner->world_retiring)
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_BUSY, "RenderGlobal native retirement has begun",
             "a", descriptor, 0, true);
    return call;
}

static mc_world_storage_status call_end(mc_render_global_access *access, access_call *call,
                                        mc_world_storage_error *error,
                                        mc_world_access_failure *failure) {
    if (call->error.status == MC_CLIENT_RENDER_GLOBAL_OK &&
        !retire_children(access, call, &call->error)) {
        /* Preserve this first post-source failure independently of later
         * checked cleanup attempts, which own separate mutable diagnostics. */
        if (call->error.image_download != NULL) {
            copy_image_error(call, call->error.image_download, false);
            call->error.image_download = &call->image_error;
        }
        if (call->error.image_buffer != NULL) {
            call->buffer_error = *call->error.image_buffer;
            call->error.image_buffer = &call->buffer_error;
        }
    }
    if (call->auxiliary2001) {
        call->auxiliary.source_returned = call->error.source_returned;
        call->auxiliary.native_cleanup_returned =
            call->error.source_returned && call->error.status == MC_CLIENT_RENDER_GLOBAL_OK;
        call->auxiliary.source_pc = call->error.source_returned ? 716u : call->error.source_pc;
        call->auxiliary.native_status = call->error.status;
        access->auxiliary.last = call->auxiliary;
    }
    --access->active_callbacks;
    if (call->error.status == MC_CLIENT_RENDER_GLOBAL_OK) {
        (void)mc_world_access_receipt_retire(&call->receipt, NULL);
        access->bytes -= sizeof(*call) + sizeof(mc_world_access_receipt);
        if (call->particle != NULL) {
            access->bytes -= sizeof(*call->particle);
            free(call->particle);
        }
        free(call);
        *failure = (mc_world_access_failure){0};
        *error = (mc_world_storage_error){0};
        return MC_WORLD_STORAGE_OK;
    }
    call->next = access->failures;
    access->failures = call;
    ++access->failure_count;
    *failure = (mc_world_access_failure){access->identity, &call->error, call->receipt};
    if (call->source_exception_class != NULL)
        call->receipt->exception_class = call->source_exception_class;
    else if (call->error.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
             !call->error.native_policy)
        call->receipt->exception_class = "java/lang/NullPointerException";
    const mc_world_storage_status status = storage_status(call->error.status);
    *error = (mc_world_storage_error){status, call->error.operation, 0};
    return status;
}

static mc_world_storage_status cell(void *context, mc_world_runtime *world, int32_t x, int32_t y,
                                    int32_t z, mc_world_storage_error *error,
                                    mc_world_access_failure *failure) {
    (void)world;
    mc_render_global_access *access = context;
    access_call *call = call_begin(access, "(III)V", error, failure);
    if (call == NULL)
        return error->status;
    if (call->error.status == MC_CLIENT_RENDER_GLOBAL_OK)
        (void)mc_render_global_mark_cell(access->owner, x, y, z, &call->error);
    return call_end(access, call, error, failure);
}

static mc_world_storage_status range(void *context, mc_world_runtime *world, int32_t x0, int32_t y0,
                                     int32_t z0, int32_t x1, int32_t y1, int32_t z1,
                                     mc_world_storage_error *error,
                                     mc_world_access_failure *failure) {
    (void)world;
    mc_render_global_access *access = context;
    access_call *call = call_begin(access, "(IIIIII)V", error, failure);
    if (call == NULL)
        return error->status;
    if (call->error.status == MC_CLIENT_RENDER_GLOBAL_OK)
        (void)mc_render_global_mark_range(access->owner, x0, y0, z0, x1, y1, z1, &call->error);
    return call_end(access, call, error, failure);
}

static mc_client_source_dk *sky_chunk(mc_client_render_global *owner, access_call *call,
                                      size_t index, unsigned int array_pc, unsigned int field_pc) {
    mc_client_source_dk **array = owner->fields.o; /* Fresh actual n.o at each source read. */
    if (array == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "actual sky-change null DK array", "e", "()V", array_pc, false);
        return NULL;
    }
    if (index >= owner->o_length) {
        call->source_exception_class = "java/lang/ArrayIndexOutOfBoundsException";
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
             "actual sky-change DK array index", "e", "()V", array_pc, false);
        return NULL;
    }
    mc_client_source_dk *chunk = array[index];
    if (chunk == NULL && field_pc != 0) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "actual sky-change null DK receiver", "e", "()V", field_pc, false);
        return NULL;
    }
    if (chunk == NULL)
        return NULL; /* A nullable List.add argument is not dereferenced by aaload. */
    for (mc_client_source_dk *owned = owner->chunk_owners; owned != NULL; owned = owned->next)
        if (owned == chunk)
            return chunk;
    fail(&call->error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
         "sky-change native DK occurrence provenance", "e", "()V", array_pc, true);
    return NULL;
}

static mc_world_storage_status sky_changed(void *context, mc_world_runtime *world,
                                           mc_world_storage_error *error,
                                           mc_world_access_failure *failure) {
    (void)world; /* Actual n.e() has no World argument or World field read. */
    mc_render_global_access *access = context;
    access_call *call = call_begin(access, "()V", error, failure);
    if (call == NULL)
        return error->status;
    if (call->error.status != MC_CLIENT_RENDER_GLOBAL_OK) {
        call->error.source_method = "e";
        return call_end(access, call, error, failure);
    }
    mc_client_render_global *owner = access->owner;
    for (size_t index = 0;; ++index) {
        if (owner->fields.o == NULL) {
            fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                 "actual sky-change null DK array length", "e", "()V", 7, false);
            break;
        }
        if (owner->o_length > INT32_MAX) {
            fail(&call->error, MC_CLIENT_RENDER_GLOBAL_LIMIT, "sky-change native DK array extent",
                 "e", "()V", 7, true);
            break;
        }
        if (index >= owner->o_length) {
            call->error.source_returned = true;
            break;
        }
        mc_client_source_dk *chunk = sky_chunk(owner, call, index, 16, 17);
        if (chunk == NULL)
            break;
        if (!chunk->fields.A)
            continue;
        chunk = sky_chunk(owner, call, index, 28, 29);
        if (chunk == NULL)
            break;
        if (chunk->fields.u)
            continue;
        mc_client_render_list *list = owner->fields.m; /* Receiver captured before o argument. */
        chunk = sky_chunk(owner, call, index, 44, 0);
        if (call->error.status != MC_CLIENT_RENDER_GLOBAL_OK)
            break;
        if (!mc_render_global_pointer_append(
                owner, list, chunk, (mc_client_graphics_site){"n", "e", "()V", 45}, &call->error))
            break;
        chunk = sky_chunk(owner, call, index, 56, 57);
        if (chunk == NULL)
            break;
        mc_source_dk_dirty(chunk); /* Actual dk.f stores u2, returns5; after List.add45. */
    }
    return call_end(access, call, error, failure);
}

static mc_world_storage_status sound(void *context, mc_world_runtime *world,
                                     const mc_registry_string *name, double x, double y, double z,
                                     float volume, float pitch, mc_world_storage_error *error,
                                     mc_world_access_failure *failure) {
    (void)world;
    const char *descriptor = "(Ljava/lang/String;DDDFF)V";
    mc_render_global_access *access = context;
    access_call *call = call_begin(access, descriptor, error, failure);
    if (call == NULL)
        return error->status;
    if (call->error.status != MC_CLIENT_RENDER_GLOBAL_OK || !fp(call, descriptor, 7))
        return call_end(access, call, error, failure);
    volatile float radius = 16.0f;
    if (volume > 1.0f)
        radius = radius * volume;
    client_application *app = access->owner->fields.t;
    if (app == NULL)
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "RenderGlobal sound actual App.i receiver", "a", descriptor, 22, false);
    else if (app->i == NULL)
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "RenderGlobal sound actual LivingEntity distance receiver", "a", descriptor, 30,
             false);
    else {
        const mc_entity_base_fields *camera = mc_entity_runtime_fields(app->i);
        /* Actual inherited sn.g(DDD): the ordered subtractions and products. */
        volatile double dx = camera->x - x, dy = camera->y - y, dz = camera->z - z;
        volatile double xx = dx * dx, yy = dy * dy, zz = dz * dz;
        volatile double xy = xx + yy, distance = xy + zz;
        volatile float radius_squared = radius * radius;
        if (distance < (double)radius_squared) {
            mc_registry_string_view text = {0};
            if (name != NULL && !mc_registry_string_read(name, &text))
                fail(&call->error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                     "RenderGlobal sound actual String metadata", "a", descriptor, 63, true);
            else if (app->B == NULL)
                fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                     "RenderGlobal sound actual SoundManager receiver", "a", descriptor, 63, false);
            else {
                const mc_sound_text sound_name = {text.units, text.length, name == NULL};
                volatile float sx = (float)x, sy = (float)y, sz = (float)z;
                mc_sound_manager_report report = {0};
                const mc_sound_status status = mc_sound_manager_effect(
                    app->B, sound_name, sx, sy, sz, volume, pitch, &report, &call->error.sound);
                if (status != MC_SOUND_OK) {
                    fail(&call->error, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                         call->error.sound.operation, "a", descriptor, 63,
                         status != MC_SOUND_SOURCE_EXCEPTION);
                    call->error.child = (mc_client_graphics_site){
                        "yo", "b", "(Ljava/lang/String;FFFFF)V", report.source_pc};
                }
            }
        }
    }
    if (call->error.status == MC_CLIENT_RENDER_GLOBAL_OK)
        call->error.source_returned = true;
    return call_end(access, call, error, failure);
}

static bool text_equals(const mc_registry_string_view *text, const char *ascii) {
    const size_t length = strlen(ascii);
    if (length != text->length)
        return false;
    for (size_t i = 0; i < length; ++i)
        if (text->units[i] != (unsigned char)ascii[i])
            return false;
    return true;
}

static mc_client_render_global_status particle_status(mc_entity_status status) {
    switch (status) {
        case MC_ENTITY_OK:
            return MC_CLIENT_RENDER_GLOBAL_OK;
        case MC_ENTITY_LIMIT:
            return MC_CLIENT_RENDER_GLOBAL_LIMIT;
        case MC_ENTITY_ALLOCATION_FAILED:
            return MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED;
        case MC_ENTITY_BUSY:
            return MC_CLIENT_RENDER_GLOBAL_BUSY;
        case MC_ENTITY_UNSUPPORTED_FP:
            return MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP;
        default:
            return MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY;
    }
}

static mc_client_render_global_status effect_status(mc_effect_renderer_status status) {
    switch (status) {
        case MC_EFFECT_RENDERER_OK:
            return MC_CLIENT_RENDER_GLOBAL_OK;
        case MC_EFFECT_RENDERER_BUSY:
            return MC_CLIENT_RENDER_GLOBAL_BUSY;
        case MC_EFFECT_RENDERER_LIMIT:
            return MC_CLIENT_RENDER_GLOBAL_LIMIT;
        case MC_EFFECT_RENDERER_ALLOCATION_FAILED:
            return MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED;
        case MC_EFFECT_RENDERER_NULL_REFERENCE:
            return MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE;
        default:
            return MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY;
    }
}

static access_particle *capture_particle(mc_render_global_access *access, access_call *call,
                                         unsigned int receiver_pc) {
    const char *descriptor = "(Ljava/lang/String;DDDDDD)V";
    if (!budget(access, sizeof(access_particle))) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_LIMIT, "N reached particle capture ceiling", "a",
             descriptor, receiver_pc, true);
        return NULL;
    }
    access_particle *p = calloc(1, sizeof(*p));
    if (p == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
             "N reached particle capture allocation", "a", descriptor, receiver_pc, true);
        return NULL;
    }
    call->particle = p;
    access->bytes += sizeof(*p);
    /* Fresh N.t then Minecraft.j. The initial guard is a distinct source read
     * and does not supply the captured receiver for this constructor branch. */
    client_application *app = access->owner->fields.t;
    if (app == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N particle null fresh Minecraft receiver", "a", descriptor, receiver_pc, false);
        return NULL;
    }
    mc_client_effect_renderer *receiver = app->effect_renderer;
    if (mc_client_application_render_borrow(app, &p->application_error) !=
        MC_CLIENT_APPLICATION_OWNER_OK) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_BUSY, "N particle actual App lifetime", "a",
             descriptor, receiver_pc, true);
        return NULL;
    }
    p->application = app;
    if (receiver != NULL) {
        const mc_effect_renderer_status held = mc_client_effect_renderer_retain(receiver);
        if (held != MC_EFFECT_RENDERER_OK) {
            p->effects.status = held;
            p->effects.operation = "N captured particle receiver lifetime";
            p->effects.native_policy = true;
            call->error.effects = &p->effects;
            fail(&call->error, effect_status(held), p->effects.operation, "a", descriptor,
                 receiver_pc, true);
            return NULL;
        }
        p->receiver = receiver;
    }
    return p;
}

static bool slime_particle(mc_render_global_access *access, access_call *call, double x, double y,
                           double z) {
    const char *descriptor = "(Ljava/lang/String;DDDDDD)V";
    access_particle *p = capture_particle(access, call, 683);
    if (p == NULL)
        return false;
    client_application *app = p->application;
    mc_client_effect_renderer *receiver = p->receiver;
    /* Actual RAW allocation686 precedes every constructor argument below. */
    const mc_entity_status allocated =
        mc_item_particle_allocate(app->entities, &p->candidate, &call->error.entity);
    if (allocated != MC_ENTITY_OK) {
        fail(&call->error, particle_status(allocated), call->error.entity.operation, "a",
             descriptor, 686, true);
        return false;
    }
    mc_world_runtime *world = access->owner->fields.k; /* Actual fresh N.k691. */
    if (world != NULL) {
        if (!mc_world_runtime_binding_retain(world)) {
            fail(&call->error,
                 world->persistent_bindings == SIZE_MAX ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                                                        : MC_CLIENT_RENDER_GLOBAL_BUSY,
                 "N captured particle World lifetime", "a", descriptor, 691, true);
            return false;
        }
        p->world = world;
    }
    mc_registry_classes *registry = app->registry; /* Native service, not N.k. */
    const mc_registry_status held = mc_registry_classes_retain(
        registry, MC_REGISTRY_CLIENT, app->entities->profile, &call->error.registry);
    if (held != MC_REGISTRY_OK) {
        fail(&call->error,
             held == MC_REGISTRY_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                                       : MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
             "N reached Item Registry lifetime", "a", descriptor, 699, true);
        return false;
    }
    p->registry = registry;
    const mc_item *item = NULL;
    if (mc_registry_item_static_get(registry, "aK", &item, &call->error.registry) !=
        MC_REGISTRY_OK) {
        mc_particle_throwable throwable;
        const char *exception = NULL;
        mc_particle_registry_classification(registry, &call->error.registry, &throwable,
                                            &exception);
        call->source_exception_class = throwable == MC_PARTICLE_SOURCE_EXCEPTION ? exception : NULL;
        fail(&call->error,
             call->error.registry.status == MC_REGISTRY_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
             : call->error.registry.status == MC_REGISTRY_ALLOCATION_FAILED
                 ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                 : MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
             call->error.registry.message, "a", descriptor, 699,
             throwable == MC_PARTICLE_NO_THROWABLE);
        return false;
    }
    call->error.particle = &p->error;
    /* PB's entered XW body transfers the actual Math occurrence at164,
     * after five prior shared draws, including on initializer failure.
     * No early retain or IN ensure belongs to this N caller. */
    const mc_entity_status constructed =
        mc_item_particle_construct_entered(p->candidate, world, registry, app->source_math_classes,
                                           x, y, z, item, &p->math, &p->error);
    if (constructed != MC_ENTITY_OK) {
        call->source_exception_class =
            p->error.throwable == MC_PARTICLE_SOURCE_EXCEPTION ? p->error.exception_class : NULL;
        const mc_client_render_global_status selected =
            p->error.throwable == MC_PARTICLE_SOURCE_EXCEPTION &&
                    p->error.exception_class != NULL &&
                    strcmp(p->error.exception_class, "java/lang/NullPointerException") == 0
                ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                : particle_status(constructed);
        fail(&call->error, selected, p->error.operation, "a", descriptor, 702,
             p->error.native_policy);
        call->error.child = (mc_client_graphics_site){
            p->error.source_class, p->error.source_method, p->error.source_descriptor,
            p->error.source_class == NULL ? -1 : (int32_t)p->error.source_pc};
        return false;
    }
    if (receiver == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N particle null captured DN enqueue receiver", "a", descriptor, 705, false);
        return false;
    }
    call->error.effects = &p->effects;
    const mc_effect_renderer_status added =
        mc_client_effect_renderer_add(receiver, p->candidate, &p->effects);
    if (added != MC_EFFECT_RENDERER_OK) {
        call->source_exception_class =
            p->effects.source_exception ? p->effects.exception_class : NULL;
        fail(&call->error, effect_status(added), p->effects.operation, "a", descriptor, 705,
             p->effects.native_policy);
        call->error.child =
            (mc_client_graphics_site){"dn", p->effects.source_method, p->effects.source_descriptor,
                                      (int32_t)p->effects.source_pc};
        return false;
    }
    /* DN owns the independently retained List occurrence. End only this
     * constructor-expression local; the actual particle remains in DN. */
    mc_entity_release(p->candidate);
    p->candidate = NULL;
    call->error.source_returned = true;
    return true;
}

static bool explosion_particle(mc_render_global_access *access, access_call *call, double x,
                               double y, double z, double vx, double vy, double vz) {
    const char *descriptor = "(Ljava/lang/String;DDDDDD)V";
    access_particle *p = capture_particle(access, call, 293);
    if (p == NULL)
        return false;
    client_application *app = p->application;
    const mc_entity_status allocated =
        mc_explosion_particle_allocate(app->entities, &p->candidate, &call->error.entity);
    if (allocated != MC_ENTITY_OK) {
        fail(&call->error, particle_status(allocated), call->error.entity.operation, "a",
             descriptor, 296, true);
        return false;
    }
    mc_world_runtime *world = access->owner->fields.k; /* Actual N.k301 after NEW296. */
    if (world != NULL) {
        if (!mc_world_runtime_binding_retain(world)) {
            fail(&call->error,
                 world->persistent_bindings == SIZE_MAX ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                                                        : MC_CLIENT_RENDER_GLOBAL_BUSY,
                 "N captured explosion World lifetime", "a", descriptor, 301, true);
            return false;
        }
        p->world = world;
    }
    call->error.particle = &p->error;
    /* GY enters the actual XW ancestor, which captures Math only at164 after
     * its five shared draws. N introduces no Registry or Item active use. */
    const mc_entity_status constructed = mc_explosion_particle_construct_entered(
        p->candidate, world, app->source_math_classes, x, y, z, vx, vy, vz, &p->math, &p->error);
    if (constructed != MC_ENTITY_OK) {
        call->source_exception_class =
            p->error.throwable == MC_PARTICLE_SOURCE_EXCEPTION ? p->error.exception_class : NULL;
        const mc_client_render_global_status status =
            p->error.throwable == MC_PARTICLE_SOURCE_EXCEPTION &&
                    p->error.exception_class != NULL &&
                    strcmp(p->error.exception_class, "java/lang/NullPointerException") == 0
                ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                : particle_status(constructed);
        fail(&call->error, status, p->error.operation, "a", descriptor, 315,
             p->error.native_policy);
        call->error.child = (mc_client_graphics_site){
            p->error.source_class, p->error.source_method, p->error.source_descriptor,
            p->error.source_class == NULL ? -1 : (int32_t)p->error.source_pc};
        return false;
    }
    if (p->receiver == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N explosion null captured DN enqueue receiver", "a", descriptor, 318, false);
        return false;
    }
    call->error.effects = &p->effects;
    const mc_effect_renderer_status added =
        mc_client_effect_renderer_add(p->receiver, p->candidate, &p->effects);
    if (added != MC_EFFECT_RENDERER_OK) {
        call->source_exception_class =
            p->effects.source_exception ? p->effects.exception_class : NULL;
        fail(&call->error, effect_status(added), p->effects.operation, "a", descriptor, 318,
             p->effects.native_policy);
        call->error.child =
            (mc_client_graphics_site){"dn", p->effects.source_method, p->effects.source_descriptor,
                                      (int32_t)p->effects.source_pc};
        return false;
    }
    /* DN owns its List occurrence. Retire only the constructor-expression local. */
    mc_entity_release(p->candidate);
    p->candidate = NULL;
    call->error.source_returned = true;
    return true;
}

typedef struct sprite_particle_sites {
    unsigned int receiver, allocation, world, constructor, enqueue;
    bool flame, scaled;
} sprite_particle_sites;

static bool sprite_particle(mc_render_global_access *access, access_call *call,
                            const sprite_particle_sites *sites, double x, double y, double z,
                            double vx, double vy, double vz) {
    const char *descriptor = "(Ljava/lang/String;DDDDDD)V";
    access_particle *p = capture_particle(access, call, sites->receiver);
    if (p == NULL)
        return false;
    client_application *app = p->application;
    /* NEW precedes evaluation of the fresh N.k constructor argument. */
    const mc_entity_status allocated =
        sites->flame
            ? mc_flame_particle_allocate(app->entities, &p->candidate, &call->error.entity)
            : mc_smoke_particle_allocate(app->entities, &p->candidate, &call->error.entity);
    if (allocated != MC_ENTITY_OK) {
        fail(&call->error, particle_status(allocated), call->error.entity.operation, "a",
             descriptor, sites->allocation, true);
        return false;
    }
    mc_world_runtime *world = access->owner->fields.k;
    if (world != NULL) {
        if (!mc_world_runtime_binding_retain(world)) {
            fail(&call->error,
                 world->persistent_bindings == SIZE_MAX ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                                                        : MC_CLIENT_RENDER_GLOBAL_BUSY,
                 "N captured sprite World lifetime", "a", descriptor, sites->world, true);
            return false;
        }
        p->world = world;
    }
    call->error.particle = &p->error;
    /* XW alone captures Math at164 after its five shared draws. Default XN
     * retains its actual delegation to the scaled constructor at14. */
    const mc_entity_status constructed =
        sites->flame
            ? mc_flame_particle_construct_entered(p->candidate, world, app->source_math_classes, x,
                                                  y, z, vx, vy, vz, &p->math, &p->error)
        : sites->scaled
            ? mc_smoke_particle_construct_scaled_entered(p->candidate, world,
                                                         app->source_math_classes, x, y, z, vx, vy,
                                                         vz, 2.5f, &p->math, &p->error)
            : mc_smoke_particle_construct_entered(p->candidate, world, app->source_math_classes, x,
                                                  y, z, vx, vy, vz, &p->math, &p->error);
    if (constructed != MC_ENTITY_OK) {
        call->source_exception_class =
            p->error.throwable == MC_PARTICLE_SOURCE_EXCEPTION ? p->error.exception_class : NULL;
        const mc_client_render_global_status status =
            p->error.throwable == MC_PARTICLE_SOURCE_EXCEPTION &&
                    p->error.exception_class != NULL &&
                    strcmp(p->error.exception_class, "java/lang/NullPointerException") == 0
                ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                : particle_status(constructed);
        fail(&call->error, status, p->error.operation, "a", descriptor, sites->constructor,
             p->error.native_policy);
        call->error.child = (mc_client_graphics_site){
            p->error.source_class, p->error.source_method, p->error.source_descriptor,
            p->error.source_class == NULL ? -1 : (int32_t)p->error.source_pc};
        return false;
    }
    if (p->receiver == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N sprite null captured DN enqueue receiver", "a", descriptor, sites->enqueue, false);
        return false;
    }
    call->error.effects = &p->effects;
    const mc_effect_renderer_status added =
        mc_client_effect_renderer_add(p->receiver, p->candidate, &p->effects);
    if (added != MC_EFFECT_RENDERER_OK) {
        call->source_exception_class =
            p->effects.source_exception ? p->effects.exception_class : NULL;
        fail(&call->error, effect_status(added), p->effects.operation, "a", descriptor,
             sites->enqueue, p->effects.native_policy);
        call->error.child =
            (mc_client_graphics_site){"dn", p->effects.source_method, p->effects.source_descriptor,
                                      (int32_t)p->effects.source_pc};
        return false;
    }
    /* The DN List owns its actual independent occurrence after add returns. */
    mc_entity_release(p->candidate);
    p->candidate = NULL;
    call->error.source_returned = true;
    return true;
}

static mc_world_storage_status particle(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, double vx, double vy, double vz,
                                        mc_world_storage_error *error,
                                        mc_world_access_failure *failure) {
    (void)world;
    const char *descriptor = "(Ljava/lang/String;DDDDDD)V";
    mc_render_global_access *access = context;
    access_call *call = call_begin(access, descriptor, error, failure);
    if (call == NULL)
        return error->status;
    if (call->error.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return call_end(access, call, error, failure);
    client_application *app = access->owner->fields.t;
    /* Actual null guards return27 before String access or FP arithmetic. */
    if (app == NULL || app->i == NULL || app->effect_renderer == NULL)
        return call_end(access, call, error, failure);
    if (!fp(call, descriptor, 39))
        return call_end(access, call, error, failure);
    const mc_entity_base_fields *camera = mc_entity_runtime_fields(app->i);
    volatile double dx = camera->x - x, dy = camera->y - y, dz = camera->z - z;
    volatile double xx = dx * dx, yy = dy * dy, zz = dz * dz;
    volatile double xy = xx + yy, distance = xy + zz;
    if (distance > 256.0) /* dcmpl99; ifle104 also admits NaN. */
        return call_end(access, call, error, failure);
    if (name == NULL)
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "RenderGlobal particle actual String.equals receiver", "a", descriptor, 107, false);
    else {
        mc_registry_string_view text = {0};
        if (!mc_registry_string_read(name, &text))
            fail(&call->error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                 "RenderGlobal particle actual String metadata", "a", descriptor, 107, true);
        else {
            static const struct {
                const char *name, *class_name, *constructor;
                unsigned int allocation_pc;
            } kinds[] = {{"bubble", "cr", "(Lfd;DDDDDD)V", 120},
                         {"smoke", "xn", "(Lfd;DDDDDD)V", 164},
                         {"note", "ae", "(Lfd;DDDDDD)V", 208},
                         {"portal", "op", "(Lfd;DDDDDD)V", 252},
                         {"explode", "gy", "(Lfd;DDDDDD)V", 296},
                         {"flame", "qu", "(Lfd;DDDDDD)V", 340},
                         {"lava", "fg", "(Lfd;DDD)V", 384},
                         {"footstep", "gl", "(Lji;Lfd;DDD)V", 422},
                         {"splash", "sy", "(Lfd;DDDDDD)V", 464},
                         {"largesmoke", "xn", "(Lfd;DDDDDDF)V", 508},
                         {"reddust", "im", "(Lfd;DDDFFF)V", 554},
                         {"snowballpoof", "pb", "(Lfd;DDDLgm;)V", 601},
                         {"snowshovel", "mu", "(Lfd;DDDDDD)V", 642},
                         {"slime", "pb", "(Lfd;DDDLgm;)V", 686},
                         {"heart", "kc", "(Lfd;DDDDDD)V", 727}};

            for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
                if (!text_equals(&text, kinds[i].name))
                    continue;
                if (strcmp(kinds[i].name, "slime") == 0) {
                    (void)slime_particle(access, call, x, y, z);
                    break;
                }
                if (strcmp(kinds[i].name, "explode") == 0) {
                    (void)explosion_particle(access, call, x, y, z, vx, vy, vz);
                    break;
                }
                if (strcmp(kinds[i].name, "smoke") == 0 || strcmp(kinds[i].name, "flame") == 0 ||
                    strcmp(kinds[i].name, "largesmoke") == 0) {
                    const bool flame = strcmp(kinds[i].name, "flame") == 0;
                    const bool scaled = strcmp(kinds[i].name, "largesmoke") == 0;
                    const sprite_particle_sites sites =
                        flame    ? (sprite_particle_sites){337, 340, 345, 359, 362, true, false}
                        : scaled ? (sprite_particle_sites){505, 508, 513, 529, 532, false, true}
                                 : (sprite_particle_sites){161, 164, 169, 183, 186, false, false};
                    (void)sprite_particle(access, call, &sites, x, y, z, vx, vy, vz);
                    break;
                }
                fail(&call->error, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                     "actual concrete particle allocation", "a", descriptor, kinds[i].allocation_pc,
                     false);
                /* The NEW itself is the reached dependency. No constructor
                 * invocation or raw concrete particle owner is invented. */
                call->error.child = (mc_client_graphics_site){kinds[i].class_name, NULL, NULL, 0};
                break;
            }
        }
    }
    if (call->error.status == MC_CLIENT_RENDER_GLOBAL_OK)
        call->error.source_returned = true; /* Actual unmatched-name return752. */
    return call_end(access, call, error, failure);
}

static mc_world_storage_status entity_added(void *context, mc_world_runtime *world,
                                            mc_entity *entity, mc_world_storage_error *error,
                                            mc_world_access_failure *failure) {
    (void)world;
    mc_render_global_access *access = context;
    access_call *call = call_begin(access, "(Lsn;)V", error, failure);
    if (call == NULL)
        return error->status;
    if (call->error.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return call_end(access, call, error, failure);
    if (entity == NULL)
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "RenderGlobal actual Entity.u_ receiver", "a", "(Lsn;)V", 1, false);
    else {
        const mc_player_status status = mc_entity_update_cloak(entity, &call->error.player);
        if (status != MC_PLAYER_OK) {
            fail(&call->error,
                 status == MC_PLAYER_BUSY                ? MC_CLIENT_RENDER_GLOBAL_BUSY
                 : status == MC_PLAYER_NULL_REFERENCE    ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                 : status == MC_PLAYER_ALLOCATION_FAILED ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                 : status == MC_PLAYER_LIMIT             ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                                             : MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                 call->error.player.operation, "a", "(Lsn;)V", 1,
                 status != MC_PLAYER_NULL_REFERENCE);
            call->error.child = (mc_client_graphics_site){
                call->error.player.site.class_name, call->error.player.site.method,
                call->error.player.site.descriptor, (int32_t)call->error.player.site.pc};
        } else {
            for (size_t i = 0; i < 2; ++i) {
                if ((i == 0 ? mc_entity_skin_url(entity) : mc_entity_cloak_url(entity)) == NULL)
                    continue;
                /* Source captures ji12/38, then reloads String16/42, before
                 * the real NEW rr19/45 and Object-only constructor23/49. */
                mc_client_render_engine *engine = access->owner->fields.l;
                const mc_player_string *url =
                    i == 0 ? mc_entity_skin_url(entity) : mc_entity_cloak_url(entity);
                const unsigned int allocation_pc = i == 0 ? 19u : 45u;
                const unsigned int factory_pc = i == 0 ? 26u : 52u;
                const mc_client_image_buffer_status created =
                    mc_client_image_buffer_create(NULL, &call->processors[i], &call->buffer_error);
                if (created != MC_CLIENT_IMAGE_BUFFER_OK) {
                    fail(&call->error,
                         created == MC_CLIENT_IMAGE_BUFFER_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                         : created == MC_CLIENT_IMAGE_BUFFER_ALLOCATION_FAILED
                             ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                             : MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                         call->buffer_error.operation, "a", "(Lsn;)V", allocation_pc, true);
                    call->error.image_buffer = &call->buffer_error;
                    break;
                }
                if (engine == NULL) {
                    fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                         "actual captured ji factory receiver after rr construction", "a",
                         "(Lsn;)V", factory_pc, false);
                    break;
                }
                const mc_client_image_download_status requested =
                    mc_client_image_download_request_begin(engine, (mc_player_string *)url,
                                                           call->processors[i], &call->images[i]);
                mc_client_image_download_call_state state = {0};
                const bool inspected =
                    call->images[i] != NULL &&
                    mc_client_image_download_call_inspect(call->images[i], &state);
                if (requested != MC_CLIENT_IMAGE_DOWNLOAD_OK) {
                    const mc_client_image_download_error *child = inspected ? state.failure : NULL;
                    fail(&call->error, image_status(requested),
                         child == NULL ? "native image-factory invocation admission"
                                       : child->operation,
                         "a", "(Lsn;)V", factory_pc, child == NULL || child->native_policy);
                    if (child != NULL) {
                        copy_image_error(call, child, false);
                        call->error.image_download = &call->image_error;
                        call->error.child = (mc_client_graphics_site){
                            child->site.class_name, child->site.method, child->site.descriptor,
                            (int32_t)child->site.pc};
                        if (child->source_exception)
                            call->source_exception_class = child->exception_class;
                    }
                    break;
                }
                if (!inspected || !state.source_returned) {
                    fail(&call->error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                         "image factory has no actual source return", "a", "(Lsn;)V", factory_pc,
                         true);
                    break;
                }
                /* Source pop29/55 discards the OLD nullable lookup. A newly
                 * started worker's later result/error is not this call result. */
            }
            if (call->error.status == MC_CLIENT_RENDER_GLOBAL_OK)
                call->error.source_returned = true;
        }
    }
    return call_end(access, call, error, failure);
}

static mc_world_storage_status entity_removed(void *context, mc_world_runtime *world,
                                              mc_entity *entity, mc_world_storage_error *error,
                                              mc_world_access_failure *failure) {
    (void)world;
    mc_render_global_access *access = context;
    access_call *call = call_begin(access, "(Lsn;)V", error, failure);
    if (call == NULL)
        return error->status;
    if (call->error.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return call_end(access, call, error, failure);
    if (entity == NULL)
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "RenderGlobal removed Entity appearance receiver", "b", "(Lsn;)V", 1, false);
    else {
        for (size_t i = 0; i < 2; ++i) {
            if ((i == 0 ? mc_entity_skin_url(entity) : mc_entity_cloak_url(entity)) == NULL)
                continue;
            mc_client_render_engine *engine = access->owner->fields.l;
            const mc_player_string *url =
                i == 0 ? mc_entity_skin_url(entity) : mc_entity_cloak_url(entity);
            const unsigned int pc = i == 0 ? 15u : 33u;
            if (engine == NULL) {
                fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                     "actual captured ji downloaded-texture release receiver", "b", "(Lsn;)V", pc,
                     false);
                break;
            }
            const mc_client_image_download_status released =
                mc_client_render_image_download_release(engine, (mc_player_string *)url,
                                                        &call->image_error);
            if (released != MC_CLIENT_IMAGE_DOWNLOAD_OK) {
                copy_image_error(call, &call->image_error, false);
                fail(&call->error, image_status(released), call->image_error.operation, "b",
                     "(Lsn;)V", pc, call->image_error.native_policy);
                call->error.image_download = &call->image_error;
                call->error.child = (mc_client_graphics_site){
                    call->image_error.site.class_name, call->image_error.site.method,
                    call->image_error.site.descriptor, (int32_t)call->image_error.site.pc};
                if (call->image_error.source_exception)
                    call->source_exception_class = call->image_error.exception_class;
                break;
            }
            if (!call->image_error.source_returned) {
                fail(&call->error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                     "downloaded-texture release has no actual source return", "b", "(Lsn;)V", pc,
                     true);
                break;
            }
        }
        if (call->error.status == MC_CLIENT_RENDER_GLOBAL_OK)
            call->error.source_returned = true;
    }
    return call_end(access, call, error, failure);
}

static mc_world_storage_status tile_entity_dirty(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z, mc_tile_entity *tile,
                                                 mc_world_storage_error *error,
                                                 mc_world_access_failure *failure) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)tile;
    mc_render_global_access *access = context;
    access_call *call = call_begin(access, "(IIILow;)V", error, failure);
    if (call == NULL)
        return error->status;
    if (call->error.status == MC_CLIENT_RENDER_GLOBAL_OK)
        call->error.source_returned = true; /* Actual n.a(III,ow) return0. */
    return call_end(access, call, error, failure);
}

#include "render_global_auxiliary.inc"

mc_client_render_global_status
mc_render_global_access_create(mc_client_render_global *owner, mc_render_global_access **out,
                               mc_client_render_global_error *error) {
    *error = (mc_client_render_global_error){0};
    if (owner == NULL || out == NULL)
        return fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                    "RenderGlobal interface creation arguments", NULL, NULL, 0, true);
    if (owner->arena.options.max_constructor_bytes <
        sizeof(mc_render_global_access) + sizeof(mc_world_access))
        return fail(error, MC_CLIENT_RENDER_GLOBAL_LIMIT,
                    "RenderGlobal interface native byte limit", NULL, NULL, 0, true);
    mc_render_global_access *access = calloc(1, sizeof(*access));
    if (access == NULL)
        return fail(error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                    "RenderGlobal interface allocation", NULL, NULL, 0, true);
    const mc_world_access_source_callbacks callbacks = {.cell = cell,
                                                        .range = range,
                                                        .sound = sound,
                                                        .particle = particle,
                                                        .entity_added = entity_added,
                                                        .tile_entity_dirty = tile_entity_dirty,
                                                        .entity_removed = entity_removed,
                                                        .sky_changed = sky_changed,
                                                        .auxiliary = auxiliary};
    mc_world_storage_status status =
        mc_world_access_source_create(&callbacks, access, &access->identity, &error->world);
    if (status != MC_WORLD_STORAGE_OK) {
        free(access);
        return fail(error, owner_status(status), error->world.operation, NULL, NULL, 0, true);
    }
    access->owner = owner;
    access->bytes = sizeof(*access) + sizeof(*access->identity);
    *out = access;
    return MC_CLIENT_RENDER_GLOBAL_OK;
}

mc_client_render_global_status
mc_render_global_access_source_add(mc_render_global_access *access, mc_world_runtime *world,
                                   mc_client_render_global_error *error) {
    *error = (mc_client_render_global_error){0};
    if (access == NULL || world == NULL)
        return fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                    "RenderGlobal source subscription arguments", "a", "(Lfd;)V", 66, true);
    if (access->retiring || !budget(access, sizeof(access_registration)))
        return fail(error,
                    access->retiring ? MC_CLIENT_RENDER_GLOBAL_BUSY : MC_CLIENT_RENDER_GLOBAL_LIMIT,
                    "RenderGlobal source subscription native admission", "a", "(Lfd;)V", 66, true);
    access_registration *registration = calloc(1, sizeof(*registration));
    if (registration == NULL)
        return fail(error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                    "RenderGlobal source subscription receipt allocation", "a", "(Lfd;)V", 66,
                    true);
    if (!mc_world_runtime_binding_retain(world)) {
        free(registration);
        return fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                    "native World subscription occurrence was not acquired", "a", "(Lfd;)V", 66,
                    true);
    }
    const mc_world_storage_status status = mc_world_access_add_receipted(
        world, access->identity, &registration->identity, &error->world);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_binding_release(world);
        free(registration);
        return fail(error, owner_status(status), error->world.operation, "a", "(Lfd;)V", 66, true);
    }
    registration->world = world;
    registration->next = access->registrations;
    access->registrations = registration;
    ++access->registration_count;
    access->bytes += sizeof(*registration);
    return MC_CLIENT_RENDER_GLOBAL_OK;
}

mc_client_render_global_status
mc_render_global_access_source_remove(mc_render_global_access *access, mc_world_runtime *world,
                                      mc_client_render_global_error *error) {
    *error = (mc_client_render_global_error){0};
    if (access == NULL || world == NULL)
        return fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                    "RenderGlobal source unsubscription arguments", "a", "(Lfd;)V", 12, true);
    if (access->retiring)
        return fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                    "RenderGlobal source removal after native retirement", "a", "(Lfd;)V", 12,
                    true);
    bool removed = false;
    uint64_t identity = 0;
    const mc_world_storage_status status = mc_world_access_remove_receipted(
        world, access->identity, &removed, &identity, &error->world);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(error, owner_status(status), error->world.operation, "a", "(Lfd;)V", 12, true);
    if (removed) {
        access_registration **cursor = &access->registrations;
        while (*cursor != NULL) {
            access_registration *registration = *cursor;
            if (registration->world == world && registration->identity == identity) {
                *cursor = registration->next;
                mc_world_runtime_binding_release(registration->world);
                free(registration);
                --access->registration_count;
                access->bytes -= sizeof(*registration);
                break;
            }
            cursor = &registration->next;
        }
    }
    return MC_CLIENT_RENDER_GLOBAL_OK;
}

mc_client_render_global_status
mc_render_global_access_detach_all(mc_render_global_access *access,
                                   mc_client_render_global_error *error) {
    *error = (mc_client_render_global_error){0};
    if (access == NULL)
        return MC_CLIENT_RENDER_GLOBAL_OK;
    if (access->owner->holds != 1 || access->active_callbacks != 0 ||
        access->identity->active_holds != 0)
        return fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                    "RenderGlobal external occurrence or entered callback", NULL, NULL, 0, true);
    /* Captures own the full child graph: refuse before consuming World rights. */
    for (access_call *call = access->failures; call != NULL; call = call->next)
        if (call->receipt->holds != 1)
            return fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                        "RenderGlobal captured callback failure", NULL, NULL, 0, true);
    access->retiring = true;
    for (access_call *call = access->failures; call != NULL; call = call->next)
        if (!retire_children(access, call, error))
            return error->status;
    while (access->registrations != NULL) {
        access_registration *registration = access->registrations;
        const mc_world_storage_status status = mc_world_access_detach_occurrence(
            registration->world, access->identity, registration->identity, &error->world);
        if (status != MC_WORLD_STORAGE_OK)
            return fail(error, owner_status(status), error->world.operation, NULL, NULL, 0, true);
        access->registrations = registration->next;
        mc_world_runtime_binding_release(registration->world);
        free(registration);
        --access->registration_count;
        access->bytes -= sizeof(*registration);
    }
    return MC_CLIENT_RENDER_GLOBAL_OK;
}

mc_client_render_global_status
mc_render_global_access_retire(mc_render_global_access **slot,
                               mc_client_render_global_error *error) {
    *error = (mc_client_render_global_error){0};
    if (slot == NULL)
        return fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                    "RenderGlobal interface retirement slot", NULL, NULL, 0, true);
    mc_render_global_access *access = *slot;
    if (access == NULL)
        return MC_CLIENT_RENDER_GLOBAL_OK;
    mc_client_render_global_status status = mc_render_global_access_detach_all(access, error);
    if (status != MC_CLIENT_RENDER_GLOBAL_OK)
        return status;
    if (access->identity->memberships != 0)
        return fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                    "foreign World occurrences retain actual RenderGlobal context", NULL, NULL, 0,
                    true);
    while (access->failures != NULL) {
        access_call *call = access->failures;
        const mc_world_storage_status retired =
            mc_world_access_receipt_retire(&call->receipt, &error->world);
        if (retired != MC_WORLD_STORAGE_OK)
            return fail(error, owner_status(retired), error->world.operation, NULL, NULL, 0, true);
        access->failures = call->next;
        if (call->particle != NULL) {
            access->bytes -= sizeof(*call->particle);
            free(call->particle);
        }
        free(call);
        --access->failure_count;
    }
    const mc_world_storage_status retired = mc_world_access_free(access->identity, &error->world);
    if (retired != MC_WORLD_STORAGE_OK)
        return fail(error, owner_status(retired), error->world.operation, NULL, NULL, 0, true);
    free(access);
    *slot = NULL;
    return MC_CLIENT_RENDER_GLOBAL_OK;
}

bool mc_render_global_access_inspect(const mc_render_global_access *access,
                                     mc_render_global_access_state *out) {
    if (access == NULL || out == NULL)
        return false;
    *out = (mc_render_global_access_state){
        access->registration_count, access->active_callbacks, access->failure_count,
        access->failures == NULL ? NULL : &access->failures->error};
    return true;
}

bool mc_render_global_access_auxiliary_observe(const mc_client_render_global *owner,
                                               mc_render_global_auxiliary_observation *out) {
    if (owner == NULL || owner->access == NULL || out == NULL)
        return false;
    *out = owner->access->auxiliary;
    return true;
}

bool mc_render_global_access_callback_inspect(const mc_render_global_access *access,
                                              const mc_world_access_failure *failure,
                                              mc_render_global_callback_state *out) {
    if (access == NULL || failure == NULL || out == NULL || failure->receiver != access->identity)
        return false;
    for (const access_call *call = access->failures; call != NULL; call = call->next) {
        if (failure->receipt != call->receipt || failure->render_global != &call->error)
            continue;
        const access_particle *p = call->particle;
        *out = (mc_render_global_callback_state){
            .processors = {call->processors[0], call->processors[1]},
            .images = {call->images[0], call->images[1]},
            .failure = &call->error,
            .source_returned = call->error.source_returned,
            .particle_receiver = p == NULL ? NULL : p->receiver,
            .particle_candidate = p == NULL ? NULL : p->candidate,
            .particle_world = p == NULL ? NULL : p->world,
            .particle_math = p == NULL ? NULL : p->math,
            .particle = call->error.particle,
            .effects = call->error.effects};
        return true;
    }
    return false;
}
