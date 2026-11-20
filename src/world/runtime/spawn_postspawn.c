#include "spawn_postspawn_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../entity/entity_internal.h"
#include "construction_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/world_access_error.h"
#include "runtime_internal.h"

#include <string.h>

static mc_world_storage_status result(mc_world_storage_error *out, mc_world_storage_status status,
                                      const char *operation) {
    if (out != NULL)
        *out = (mc_world_storage_error){status, operation, 0};
    return status;
}

static bool fail(mc_world_spawn_postspawn_work *s, mc_world_storage_status status,
                 const char *operation, bool native, const char *exception) {
    s->status = status;
    s->operation = operation;
    s->native_policy = native;
    s->exception_class = exception;
    return false;
}

static bool draw(mc_world_spawn_postspawn_work *s, mc_world_runtime *world, int32_t bound,
                 unsigned int world_pc, unsigned int invoke_pc, int32_t *out,
                 mc_java_random **captured) {
    s->child = MC_WORLD_POSTSPAWN_RANDOM;
    s->pc = world_pc;
    if (world == NULL)
        return fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Postspawn null World Random receiver",
                    false, "java/lang/NullPointerException");
    mc_java_random *random = world->fields.r;
    s->pc = invoke_pc;
    if (random == NULL)
        return fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Postspawn null Random receiver", false,
                    "java/lang/NullPointerException");
    if (captured != NULL)
        *captured = random;
    if (!mc_java_random_next_bounded(random, bound, out))
        return fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Postspawn fixed Random bound", true,
                    NULL);
    ++s->random_calls;
    return true;
}

static bool mob_result(mc_world_spawn_postspawn_work *s, mc_mob_status status) {
    if (status == MC_MOB_OK)
        return true;
    return fail(s,
                status == MC_MOB_LIMIT        ? MC_WORLD_STORAGE_LIMIT
                : status == MC_MOB_ALLOCATION ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                              : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                s->mob.operation, s->mob.native_policy, s->mob.exception_class);
}

static bool body(mc_world_spawn_postspawn_work *s, mc_world_runtime *world,
                 mc_registry_classes *registry, mc_entity *entity, float x, float y, float z) {
    s->pc = 1;
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SPIDER)) {
        int32_t value;
        if (!draw(s, world, 100, 8, 13, &value, NULL))
            return false;
        if (value == 0) {
            s->pc = 19;
            s->child = MC_WORLD_POSTSPAWN_CLASS;
            const mc_entity_class_literal *skeleton = NULL;
            if (registry == NULL || registry->entities == NULL ||
                mc_entity_class_literal_get(registry->entities, MC_ENTITY_CLASS_KIND_SKELETON,
                                            &skeleton) != MC_ENTITY_OK)
                return fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "Postspawn defining Skeleton Class namespace", true, NULL);
            s->child = MC_WORLD_POSTSPAWN_MOB;
            if (!mob_result(s, mc_mob_allocate(registry->entities, registry, skeleton, &s->jockey,
                                               &s->mob)))
                return false;
            s->pc = 24;
            if (!mob_result(s, mc_mob_construct(s->jockey, world, &s->mob)))
                return false;
            s->pc = 43;
            s->child = MC_WORLD_POSTSPAWN_POSITION;
            const mc_entity_status positioned = mc_entity_source_position_rotation(
                s->jockey, (double)x, (double)y, (double)z, entity->base.yaw, 0.0f, &s->position);
            if (positioned != MC_ENTITY_OK)
                return fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, s->position.operation,
                            s->position.native_policy,
                            s->position.source_null ? "java/lang/NullPointerException" : NULL);
            s->pc = 49;
            s->child = MC_WORLD_POSTSPAWN_PUBLICATION;
            bool published;
            const mc_world_storage_status publication =
                mc_world_runtime_insert_entity(world, s->jockey, &published, &s->publication);
            if (publication != MC_WORLD_STORAGE_OK) {
                const char *exception = NULL;
                const bool source = mc_world_access_failure_source_exception(
                    &s->publication.world_access_failure, &exception);
                if (s->publication.world_access_failure.receipt != NULL) {
                    s->capture_pending = true;
                    const mc_world_storage_status captured = mc_world_access_failure_retain(
                        &s->publication.world_access_failure, &s->retained_publication);
                    if (captured == MC_WORLD_STORAGE_OK)
                        s->capture_pending = false;
                    else
                        s->capture_error = (mc_world_storage_error){
                            captured, "Postspawn failed publication receipt capture", 0};
                }
                return fail(s, publication, s->publication.cause.operation, !source, exception);
            }
            if (published)
                ++s->publications;
            /* The returned boolean is discarded before the actual mount. */
            s->pc = 56;
            s->child = MC_WORLD_POSTSPAWN_MOUNT;
            mc_entity *actors[] = {s->jockey, entity};
            if (mc_entity_mount(s->jockey, entity, actors, 2, &s->mount) != MC_ENTITY_OK)
                return fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, s->mount.operation,
                            s->mount.native_policy,
                            s->mount.source_null ? "java/lang/NullPointerException" : NULL);
            s->pc = 83;
            return true;
        }
    }
    s->pc = 63;
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SHEEP)) {
        int32_t value, color;
        mc_java_random *random = NULL;
        /* DL.a(Random) keeps the same evaluated argument for its second draw. */
        if (!draw(s, world, 100, 74, 77, &value, &random))
            return false;
        if (value < 5)
            color = 15;
        else if (value < 10)
            color = 7;
        else if (value < 15)
            color = 8;
        else if (value < 18)
            color = 12;
        else {
            if (!mc_java_random_next_bounded(random, 500, &value))
                return fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Postspawn Sheep fixed bound",
                            true, NULL);
            ++s->random_calls;
            color = value == 0 ? 6 : 0;
        }
        s->pc = 80;
        s->child = MC_WORLD_POSTSPAWN_MOB;
        if (!mob_result(s, mc_mob_sheep_set_color(entity, color, &s->mob)))
            return false;
    }
    s->pc = 83;
    return true;
}

mc_world_storage_status mc_world_spawn_postspawn_begin(mc_world_runtime *world,
                                                       mc_registry_classes *registry,
                                                       mc_entity *entity, float x, float y, float z,
                                                       mc_world_spawn_postspawn_work *s,
                                                       mc_world_storage_error *admission) {
    if (s == NULL)
        return result(admission, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Postspawn work receiver");
    if (s->active || s->entered)
        return result(admission, MC_WORLD_STORAGE_BUSY, "Postspawn existing entered work");
    memset(s, 0, sizeof(*s));
    s->entered = s->active = true;
    s->returned = body(s, world, registry, entity, x, y, z);
    s->active = false;
    (void)result(admission, MC_WORLD_STORAGE_OK, NULL);
    return s->status;
}

mc_world_storage_status mc_world_spawn_postspawn_retire(mc_world_spawn_postspawn_work *s,
                                                        mc_world_storage_error *cleanup) {
    if (s == NULL)
        return result(cleanup, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Postspawn retirement receiver");
    if (s->active)
        return result(cleanup, MC_WORLD_STORAGE_BUSY, "Postspawn active child retirement");
    if (s->jockey != NULL && (s->jockey->classes == NULL || s->jockey->holds == 0))
        return result(cleanup, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Postspawn jockey occurrence");
    if (s->capture_pending) {
        const mc_world_storage_status captured = mc_world_access_failure_retain(
            &s->publication.world_access_failure, &s->retained_publication);
        if (captured != MC_WORLD_STORAGE_OK)
            return result(cleanup, captured, "Postspawn failed publication receipt capture");
        s->capture_pending = false;
    }
    mc_world_access_failure_release(&s->retained_publication);
    s->publication.world_access_failure = (mc_world_access_failure){0};
    mc_entity_release(s->jockey);
    s->jockey = NULL;
    s->entered = false;
    return result(cleanup, MC_WORLD_STORAGE_OK, NULL);
}
