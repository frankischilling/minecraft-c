#include "mc/world/runtime/chunk_random.h"

#include "../storage/objects_internal.h"
#include "construction_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/chunk/object.h"
#include "runtime_internal.h"

static mc_world_chunk_random_site site(const char *c, const char *m, const char *d, int32_t pc) {
    return (mc_world_chunk_random_site){c, m, d, pc};
}

static mc_world_storage_status fail(mc_world_chunk_random_error *e, mc_world_storage_status status,
                                    const char *operation, bool native) {
    e->status = status;
    e->operation = operation;
    e->native_policy = native;
    e->source_exception = !native;
    e->exception_class = native ? NULL : "java/lang/NullPointerException";
    return status;
}

mc_world_storage_status mc_world_runtime_chunk_random(mc_world_runtime *world,
                                                      mc_chunk_state *chunk, int64_t salt,
                                                      mc_java_random *out,
                                                      mc_world_chunk_random_state *s,
                                                      mc_world_chunk_random_error *e) {
    if (s == NULL || e == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *s = (mc_world_chunk_random_state){0};
    *e = (mc_world_chunk_random_error){.site = {"lm", "a", "(J)Ljava/util/Random;", -1}};
    if (world == NULL || chunk == NULL)
        return fail(e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Chunk Random arguments", true);
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk Random World phase", true);
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &e->cause);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(e, status, e->cause.operation, true);
    bool leased = false, source_entered = false;
    mc_world_runtime *source_world = NULL;
    mc_world_chunk_fields fields;
    if (chunk == &world->fallback) {
        if (!chunk->blank || chunk->data == NULL) {
            status = fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Chunk Random constructed Blank provenance", true);
            goto done;
        }
        s->blank = true;
        fields = (mc_world_chunk_fields){.d = world, .j = chunk->x, .k = chunk->z};
    } else {
        mc_world_chunk *object = mc_world_store_chunk_object(world->options.store, chunk);
        if (object == NULL || mc_world_chunk_fields_get(object, &fields) != MC_WORLD_STORAGE_OK) {
            status = fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Chunk Random concrete receiver provenance", true);
            goto done;
        }
    }
    e->site = site(s->blank ? "li" : "lm", "a", "(J)Ljava/util/Random;", 0);
    if (world->references.retain == NULL || world->references.release == NULL) {
        status = fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk Random lease binding", true);
        goto done;
    }
    if (!world->references.retain(world->references.owner, chunk)) {
        e->cause = world->last_error;
        status = fail(e, e->cause.status, "Chunk Random captured Chunk lease", true);
        goto done;
    }
    leased = true;
    s->source_pc = 8;
    e->site.pc = 8;
    source_world = fields.d;
    s->source_world = source_world;
    if (source_world == NULL) {
        status =
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk Random null World receiver", false);
        goto done;
    }
    if (source_world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        source_world->classes != world->classes) {
        status = fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Chunk Random source World class provenance", true);
        goto done;
    }
    status = mc_world_runtime_call_enter(source_world, &e->cause);
    if (status != MC_WORLD_STORAGE_OK) {
        status = fail(e, status, e->cause.operation, true);
        goto done;
    }
    source_entered = true;
    e->child = site("fd", "s", "()J", 4);
    mc_world_info *info = source_world->fields.x;
    if (info == NULL) {
        status =
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World seed null Info receiver", false);
        goto done;
    }
    if (mc_world_info_seed_get(info, &s->world_seed) != MC_WORLD_INFO_OK) {
        status = fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World seed Info field", true);
        goto done;
    }
    s->seed_read = true;
    s->x = fields.j;
    s->z = fields.k;
    const int32_t x = fields.j, z = fields.k;
    int64_t seed =
        mc_java_i64_add(s->world_seed, (int64_t)mc_java_i32_mul(mc_java_i32_mul(x, x), 4987142));
    seed = mc_java_i64_add(seed, (int64_t)mc_java_i32_mul(x, 5947611));
    seed = mc_java_i64_add(seed, mc_java_i64_mul((int64_t)mc_java_i32_mul(z, z), 4392871));
    seed = mc_java_i64_add(seed, (int64_t)mc_java_i32_mul(z, 389711));
    const uint64_t bits = (uint64_t)seed ^ (uint64_t)salt;
    s->seed = bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
    s->source_pc = 60;
    e->site.pc = 60;
    mc_java_random value;
    mc_java_random_seed(&value, s->seed);
    s->constructed = true;
    s->source_pc = 63;
    s->returned = true;
    e->site.pc = 63;
    e->child = (mc_world_chunk_random_site){0};
    *out = value;
done:
    if (source_entered)
        mc_world_runtime_call_leave(source_world);
    if (leased)
        world->references.release(world->references.owner, chunk);
    mc_world_runtime_call_leave(world);
    return status;
}
