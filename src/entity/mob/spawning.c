#include "mc/entity/mob/spawning.h"

#include "../../world/runtime/chunk_provider_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../entity_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/environment.h"

#include <emmintrin.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Mob spawn arithmetic requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Mob spawn arithmetic requires strict floating transformations"
#endif

typedef struct spawn_frame {
    mc_entity *entity;
    mc_math_classes *math;
    mc_world_runtime_classes *classes;
    mc_mob_spawn_state *state;
    mc_mob_spawn_error *error;
    bool math_held;
} spawn_frame;

static mc_mob_site location(const char *c, const char *m, const char *d, int32_t pc) {
    return (mc_mob_site){c, m, d, pc};
}

static void at(spawn_frame *f, const char *c, const char *m, const char *d, unsigned int pc) {
    f->error->site = location(c, m, d, (int32_t)pc);
    f->error->child = (mc_mob_site){0};
    f->state->source_pc = pc;
    if (m[0] == 'd' && m[1] == '\0') {
        if (strcmp(c, "gz") == 0)
            f->state->monster_pc = pc;
        else if (strcmp(c, "ii") == 0)
            f->state->creature_pc = pc;
        else if (strcmp(c, "ls") == 0)
            f->state->living_pc = pc;
        else if (strcmp(c, "uw") == 0)
            f->state->slime_pc = pc;
        else if (strcmp(c, "bg") == 0)
            f->state->animal_pc = pc;
    }
}

static bool fail(spawn_frame *f, mc_world_storage_status status, const char *operation, bool native,
                 const char *exception) {
    f->error->status = status;
    f->error->operation = operation;
    f->error->native_policy = native;
    f->error->source_exception = !native;
    f->error->exception_class = exception;
    return false;
}

static bool null_receiver(spawn_frame *f, const char *operation) {
    return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation, false,
                "java/lang/NullPointerException");
}

static bool fp(spawn_frame *f) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Mob spawn strict floating controls", true,
                NULL);
}

static bool math_use(spawn_frame *f) {
    if (!f->math_held) {
        if (f->math == NULL)
            return fail(f, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Mob spawn Math class binding", true,
                        NULL);
        const mc_math_class_status status = mc_math_classes_retain(f->math, &f->error->math);
        if (status != MC_MATH_CLASS_OK)
            return fail(f,
                        status == MC_MATH_CLASS_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                      : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        f->error->math.operation, true, NULL);
        f->math_held = true;
    }
    const mc_math_class_status status = mc_math_classes_ensure(f->math, &f->error->math);
    if (status != MC_MATH_CLASS_OK) {
        f->error->child = location("in", "<clinit>", "()V", f->error->math.source_pc);
        return fail(f,
                    status == MC_MATH_CLASS_LIMIT ? MC_WORLD_STORAGE_LIMIT
                    : status == MC_MATH_CLASS_ALLOCATION_FAILED
                        ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    f->error->math.operation, true, NULL);
    }
    if (mc_math_classes_table(f->math) != f->classes->trig)
        return fail(f, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                    "Mob spawn Math/World class table identity", true, NULL);
    return fp(f);
}

static bool floors(spawn_frame *f, const char *c, int32_t *x, int32_t *y, int32_t *z) {
    at(f, c, "d", "()Z", 4);
    const double source_x = f->entity->base.x;
    if (!math_use(f))
        return false;
    *x = mc_math_floor_double(source_x);
    at(f, c, "d", "()Z", 12);
    const mc_entity_bounds *bounds = f->entity->bounds;
    if (bounds == NULL)
        return null_receiver(f, "Mob spawn null bounds receiver");
    const double source_y = bounds->min_y;
    at(f, c, "d", "()Z", 15);
    if (!math_use(f))
        return false;
    *y = mc_math_floor_double(source_y);
    at(f, c, "d", "()Z", 23);
    const double source_z = f->entity->base.z;
    if (!math_use(f))
        return false;
    *z = mc_math_floor_double(source_z);
    f->state->x = *x;
    f->state->y = *y;
    f->state->z = *z;
    return true;
}

static bool world_enter(spawn_frame *f, mc_world_runtime **out) {
    mc_world_runtime *world = f->entity->base.world;
    if (world == NULL)
        return null_receiver(f, "Mob spawn null World receiver");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Mob spawn World phase", true, NULL);
    if (world->classes != f->classes)
        return fail(f, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Mob spawn World class identity", true,
                    NULL);
    const mc_world_storage_status status = mc_world_runtime_call_enter(world, &f->error->world);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(f, status, f->error->world.operation, true, NULL);
    *out = world;
    return true;
}

static bool own_draw(spawn_frame *f, int32_t bound, int32_t *out) {
    mc_java_random *random = f->entity->random;
    if (random == NULL)
        return null_receiver(f, "Mob spawn null Random receiver");
    if (!mc_java_random_next_bounded(random, bound, out))
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Mob spawn Random bound", true, NULL);
    ++f->state->own_random_calls;
    return true;
}

static bool light(spawn_frame *f, int32_t x, int32_t y, int32_t z, bool plant, int32_t *out) {
    mc_world_runtime *world;
    if (!world_enter(f, &world))
        return false;
    mc_world_query_error *e = &f->error->detail.query;
    const mc_world_storage_status status =
        plant ? mc_world_runtime_plant_light(world, x, y, z, out, e)
              : mc_world_runtime_block_light(world, x, y, z, out, e);
    mc_world_runtime_call_leave(world);
    if (status != MC_WORLD_STORAGE_OK) {
        f->error->child =
            location("fd", plant ? "m" : "a", plant ? "(III)I" : "(IIIZ)I", (int32_t)e->bytecode);
        return fail(f, status, e->operation, true, NULL);
    }
    return true;
}

static bool brightness(spawn_frame *f, int32_t x, int32_t y, int32_t z, float *out) {
    mc_world_runtime *world;
    if (!world_enter(f, &world))
        return false;
    mc_world_brightness_error *e = &f->error->detail.brightness;
    const mc_world_storage_status status = mc_world_runtime_brightness(world, x, y, z, out, e);
    mc_world_runtime_call_leave(world);
    if (status != MC_WORLD_STORAGE_OK) {
        f->error->child = location("fd", "c", "(III)F", (int32_t)e->bytecode);
        return fail(f, status, e->operation, e->native_policy,
                    e->source_exception == MC_WORLD_BRIGHTNESS_SOURCE_INDEX
                        ? "java/lang/ArrayIndexOutOfBoundsException"
                    : e->source_exception != MC_WORLD_BRIGHTNESS_SOURCE_NONE
                        ? "java/lang/NullPointerException"
                        : NULL);
    }
    return true;
}

static bool no_entity(spawn_frame *f, bool *out) {
    mc_world_runtime *world;
    if (!world_enter(f, &world))
        return false;
    mc_world_spawn_collision_error *e = &f->error->detail.no_entity;
    const mc_world_storage_status status =
        mc_world_runtime_no_entity_collision(world, f->entity->bounds, out, e);
    mc_world_runtime_call_leave(world);
    if (status != MC_WORLD_STORAGE_OK) {
        f->error->child = location("fd", "a", "(Leq;)Z", (int32_t)e->bytecode);
        return fail(f, status, e->operation, e->native_policy, e->exception_class);
    }
    return true;
}

static bool living(spawn_frame *f, bool *out) {
    f->state->living_reached = true;
    at(f, "ls", "d", "()Z", 8);
    bool allowed;
    if (!no_entity(f, &allowed))
        return false;
    if (!allowed) {
        *out = false;
        at(f, "ls", "d", "()Z", 53);
        return true;
    }
    at(f, "ls", "d", "()Z", 23);
    mc_world_runtime *world;
    if (!world_enter(f, &world))
        return false;
    mc_world_collision_list_view view = {0};
    mc_world_collision_error *e = &f->error->detail.collision;
    mc_world_storage_status status =
        mc_world_runtime_collision_boxes(world, f->entity, f->entity->bounds, &view, e);
    if (status != MC_WORLD_STORAGE_OK) {
        f->error->child =
            location("fd", "a", "(Lsn;Leq;)Ljava/util/List;", (int32_t)e->world_bytecode);
        mc_world_runtime_call_leave(world);
        return fail(f, status, e->nearby.query.operation, true, NULL);
    }
    at(f, "ls", "d", "()Z", 26);
    status = mc_world_collision_list_view_count(&view, &f->state->collision_count);
    const bool null_list = view.identity == NULL;
    mc_world_collision_list_view_release(&view);
    mc_world_runtime_call_leave(world);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(f, status, "Mob spawn collision List.size", !null_list,
                    null_list ? "java/lang/NullPointerException" : NULL);
    if (f->state->collision_count != 0) {
        *out = false;
        at(f, "ls", "d", "()Z", 53);
        return true;
    }
    at(f, "ls", "d", "()Z", 42);
    if (!world_enter(f, &world))
        return false;
    mc_world_material_movement_error *liquid = &f->error->detail.material;
    status = mc_world_runtime_contains_liquid(world, f->entity->bounds, NULL, &allowed, liquid);
    mc_world_runtime_call_leave(world);
    if (status != MC_WORLD_STORAGE_OK) {
        f->error->child = location("fd", "b", "(Leq;)Z", (int32_t)liquid->bytecode);
        return fail(f, status, liquid->operation, true, NULL);
    }
    *out = !allowed;
    at(f, "ls", "d", "()Z", 53);
    return true;
}

static bool below_grass(spawn_frame *f, int32_t x, int32_t y, int32_t z, unsigned int query_pc,
                        unsigned int static_pc, unsigned int field_pc, const char *method,
                        const char *descriptor, bool *out) {
    at(f, "bg", method, descriptor, query_pc);
    mc_world_runtime *world;
    if (!world_enter(f, &world))
        return false;
    mc_world_storage_status status =
        mc_world_runtime_block_get_detailed(world, x, mc_java_i32_sub(y, 1), z, &f->state->below_id,
                                            &f->error->world, &f->error->detail.acquisition);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_call_leave(world);
        return fail(f, status, f->error->world.operation, true, NULL);
    }
    at(f, "bg", method, descriptor, static_pc);
    const mc_block *grass = NULL;
    const mc_registry_status registry = mc_registry_block_static_get(
        mc_world_runtime_registry(world), "v", &grass, &f->error->registry);
    mc_world_runtime_call_leave(world);
    if (registry != MC_REGISTRY_OK) {
        f->error->child = location("uu", "<clinit>", "()V", f->error->registry.initializer_pc);
        return fail(f,
                    registry == MC_REGISTRY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                    : registry == MC_REGISTRY_ALLOCATION_FAILED
                        ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    f->error->registry.message, true, NULL);
    }
    at(f, "bg", method, descriptor, field_pc);
    if (grass == NULL)
        return null_receiver(f, "Mob spawn null Grass receiver");
    const mc_block_fields *fields = mc_block_fields_get(grass);
    if (fields == NULL)
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Mob spawn Grass fields", true, NULL);
    *out = f->state->below_id == fields->bn;
    return true;
}

static bool creature(spawn_frame *f, bool animal, bool *out) {
    f->state->creature_reached = true;
    int32_t x, y, z;
    if (!floors(f, "ii", &x, &y, &z))
        return false;
    at(f, "ii", "d", "()Z", 28);
    bool allowed;
    if (!living(f, &allowed))
        return false;
    if (!allowed) {
        *out = false;
        at(f, "ii", "d", "()Z", 51);
        return true;
    }
    at(f, "ii", "d", "()Z", 38);
    f->state->path_weight_reached = true;
    float value;
    if (animal) {
        bool grass;
        if (!below_grass(f, x, y, z, 9, 12, 15, "a", "(III)F", &grass))
            return false;
        if (grass)
            value = 10.0f;
        else {
            at(f, "bg", "a", "(III)F", 31);
            if (!brightness(f, x, y, z, &value))
                return false;
            at(f, "bg", "a", "(III)F", 36);
            if (!fp(f))
                return false;
            value = _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(value), _mm_set_ss(0.5f)));
        }
    } else {
        at(f, "gz", "a", "(III)F", 9);
        if (!brightness(f, x, y, z, &value))
            return false;
        at(f, "gz", "a", "(III)F", 12);
        if (!fp(f))
            return false;
        value = _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(0.5f), _mm_set_ss(value)));
    }
    f->state->path_weight = value;
    *out = value >= 0.0f;
    at(f, "ii", "d", "()Z", 51);
    return true;
}

static bool monster(spawn_frame *f, bool *out) {
    int32_t x, y, z;
    if (!floors(f, "gz", &x, &y, &z))
        return false;
    at(f, "gz", "d", "()Z", 31);
    mc_world_runtime *world = f->entity->base.world;
    /* The receiver is captured before EB active use, then checked at invoke37. */
    const mc_world_light_type *sky = NULL;
    mc_world_storage_status status =
        mc_world_runtime_light_type_get(f->classes, MC_LIGHT_SKY, &sky, &f->error->world);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(f, status, f->error->world.operation, true, NULL);
    at(f, "gz", "d", "()Z", 37);
    if (world == NULL)
        return null_receiver(f, "Mob spawn null World receiver");
    if (world->classes != f->classes)
        return fail(f, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Mob spawn World class identity", true,
                    NULL);
    status = mc_world_runtime_call_enter(world, &f->error->world);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(f, status, f->error->world.operation, true, NULL);
    status = mc_world_runtime_saved_light(world, sky, x, y, z, &f->state->saved_sky,
                                          &f->error->detail.query);
    mc_world_runtime_call_leave(world);
    f->state->saved_sky_reached = true;
    if (status != MC_WORLD_STORAGE_OK) {
        f->error->child =
            location("fd", "a", "(Leb;III)I", (int32_t)f->error->detail.query.bytecode);
        return fail(f, status, f->error->detail.query.operation, true, NULL);
    }
    at(f, "gz", "d", "()Z", 46);
    if (!own_draw(f, 32, &f->state->draw32))
        return false;
    if (f->state->saved_sky > f->state->draw32) {
        *out = false;
        at(f, "gz", "d", "()Z", 53);
        return true;
    }
    at(f, "gz", "d", "()Z", 61);
    f->state->light_reached = true;
    if (!light(f, x, y, z, false, &f->state->light))
        return false;
    at(f, "gz", "d", "()Z", 70);
    if (!world_enter(f, &world))
        return false;
    f->state->thunder_reached = true;
    status = mc_world_runtime_thunder_strength(world, 1.0f, &f->state->thunder_strength,
                                               &f->error->world);
    mc_world_runtime_call_leave(world);
    if (status != MC_WORLD_STORAGE_OK) {
        f->error->child = location("fd", "B", "()Z", 2);
        return fail(f, status, f->error->world.operation, true, NULL);
    }
    if (!fp(f))
        return false;
    f->state->thundering = (double)f->state->thunder_strength > 0.9;
    if (f->state->thundering) {
        at(f, "gz", "d", "()Z", 80);
        if (!world_enter(f, &world))
            return false;
        f->state->saved_subtraction = world->fields.f;
        mc_world_runtime_call_leave(world);
        at(f, "gz", "d", "()Z", 91);
        if (!world_enter(f, &world))
            return false;
        world->fields.f = 10;
        f->state->temporary_subtraction_stored = true;
        mc_world_runtime_call_leave(world);
        at(f, "gz", "d", "()Z", 101);
        if (!light(f, x, y, z, false, &f->state->light))
            return false;
        at(f, "gz", "d", "()Z", 112);
        if (!world_enter(f, &world))
            return false;
        world->fields.f = f->state->saved_subtraction;
        f->state->subtraction_restored = true;
        mc_world_runtime_call_leave(world);
    }
    at(f, "gz", "d", "()Z", 123);
    if (!own_draw(f, 8, &f->state->draw8))
        return false;
    if (f->state->light > f->state->draw8) {
        *out = false;
        at(f, "gz", "d", "()Z", 141);
        return true;
    }
    at(f, "gz", "d", "()Z", 130);
    if (!creature(f, false, out))
        return false;
    at(f, "gz", "d", "()Z", 141);
    return true;
}

static bool animal(spawn_frame *f, bool *out) {
    int32_t x, y, z;
    if (!floors(f, "bg", &x, &y, &z))
        return false;
    bool grass;
    if (!below_grass(f, x, y, z, 36, 39, 42, "d", "()Z", &grass))
        return false;
    if (!grass) {
        *out = false;
        at(f, "bg", "d", "()Z", 75);
        return true;
    }
    at(f, "bg", "d", "()Z", 55);
    if (!light(f, x, y, z, true, &f->state->plant_light))
        return false;
    if (f->state->plant_light <= 8) {
        *out = false;
        at(f, "bg", "d", "()Z", 75);
        return true;
    }
    at(f, "bg", "d", "()Z", 64);
    if (!creature(f, true, out))
        return false;
    at(f, "bg", "d", "()Z", 75);
    return true;
}

static bool slime_size(spawn_frame *f, int32_t *out) {
    if (f->entity->watcher == NULL) {
        f->error->child = location("uw", "v", "()I", 6);
        return null_receiver(f, "Slime size null DataWatcher receiver");
    }
    mc_entity_watch_map *map = f->entity->watcher->map;
    if (map == NULL) {
        f->error->child = location("ud", "a", "(I)B", 8);
        return null_receiver(f, "Slime size null watcher Map receiver");
    }
    mc_entity_watch_node *node = map->head;
    for (size_t i = 0; node != NULL && i < map->size; ++i, node = node->next) {
        if (node->key != 16)
            continue;
        if (node->record == NULL) {
            f->error->child = location("ud", "a", "(I)B", 16);
            return null_receiver(f, "Slime size null watcher record receiver");
        }
        if (node->record->type != 0) {
            f->error->child = location("ud", "a", "(I)B", 19);
            return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Slime size watcher Byte cast",
                        false, "java/lang/ClassCastException");
        }
        *out = node->record->byte_value;
        return true;
    }
    f->error->child = location("ud", "a", "(I)B", 16);
    return null_receiver(f, "Slime size absent watcher record");
}

static bool slime(spawn_frame *f, bool *out) {
    mc_world_runtime *captured = f->entity->base.world;
    at(f, "uw", "d", "()Z", 8);
    const double source_x = f->entity->base.x;
    if (!math_use(f))
        return false;
    const int32_t x = mc_math_floor_double(source_x);
    at(f, "uw", "d", "()Z", 15);
    const double source_z = f->entity->base.z;
    if (!math_use(f))
        return false;
    const int32_t z = mc_math_floor_double(source_z);
    f->state->x = x;
    f->state->z = z;
    at(f, "uw", "d", "()Z", 18);
    if (captured == NULL)
        return null_receiver(f, "Slime spawn null World receiver");
    if (captured->construction.mode != MC_WORLD_RUNTIME_GAME ||
        captured->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Slime spawn World phase", true, NULL);
    if (captured->classes != f->classes)
        return fail(f, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Slime spawn World class identity", true,
                    NULL);
    mc_world_storage_status status = mc_world_runtime_call_enter(captured, &f->error->world);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(f, status, f->error->world.operation, true, NULL);
    bool leased = false, ok = false;
    mc_chunk_state *chunk = NULL;
    /* fd.b shifts block coordinates, then fd.c reads the current actual v. */
    if (captured->fields.v == NULL) {
        f->error->child = location("fd", "c", "(II)Llm;", 6);
        (void)null_receiver(f, "Slime spawn null ChunkProvider receiver");
        goto done;
    }
    mc_chunk_provider_fields provider;
    status = mc_chunk_provider_bind_fields(captured->fields.v, &provider, &f->error->world);
    if (status != MC_WORLD_STORAGE_OK || provider.world != captured ||
        provider.cache != captured->acquisition.cache ||
        provider.pending != captured->acquisition.pending ||
        provider.store != captured->options.store) {
        (void)fail(f, status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status,
                   "Slime spawn concrete ChunkProvider binding", true, NULL);
        goto done;
    }
    mc_world_runtime_acquisition_error *acquisition = &f->error->detail.acquisition;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get_detailed(
        captured, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &chunk, acquisition);
    f->state->chunk_reached = true;
    if (acquired != MC_CHUNK_ACQUIRE_OK) {
        f->error->child = location("fd", "c", "(II)Llm;", 6);
        status = acquisition->cause.status;
        (void)fail(f,
                   status != MC_WORLD_STORAGE_OK ? status
                   : acquired == MC_CHUNK_ACQUIRE_LIMIT ||
                           acquired == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT
                       ? MC_WORLD_STORAGE_LIMIT
                       : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   acquisition->cause.operation == NULL ? "Slime spawn Chunk acquisition"
                                                        : acquisition->cause.operation,
                   true, NULL);
        goto done;
    }
    if (chunk != NULL) {
        if (!captured->references.retain(captured->references.owner, chunk)) {
            f->error->world = captured->last_error;
            (void)fail(f, captured->last_error.status, "Slime spawn captured Chunk lease", true,
                       NULL);
            goto done;
        }
        leased = true;
    }
    at(f, "uw", "d", "()Z", 23);
    if (!slime_size(f, &f->state->slime_size))
        goto done;
    if (f->state->slime_size != 1) {
        at(f, "uw", "d", "()Z", 34);
        mc_world_runtime *world;
        if (!world_enter(f, &world))
            goto done;
        const int32_t difficulty = world->fields.q;
        mc_world_runtime_call_leave(world);
        if (difficulty <= 0) {
            *out = false;
            ok = true;
            goto done;
        }
    }
    at(f, "uw", "d", "()Z", 46);
    if (!own_draw(f, 10, &f->state->draw10))
        goto done;
    if (f->state->draw10 != 0) {
        *out = false;
        ok = true;
        goto done;
    }
    at(f, "uw", "d", "()Z", 56);
    if (chunk == NULL) {
        (void)null_receiver(f, "Slime spawn null Chunk receiver");
        goto done;
    }
    mc_world_chunk_random_error *random = &f->error->detail.chunk_random;
    status = mc_world_runtime_chunk_random(captured, chunk, INT64_C(987234911),
                                           &f->state->chunk_random, &f->state->chunk, random);
    if (status != MC_WORLD_STORAGE_OK) {
        f->error->child = location(random->site.class_name, random->site.method,
                                   random->site.descriptor, random->site.pc);
        (void)fail(f, status, random->operation, random->native_policy, random->exception_class);
        goto done;
    }
    at(f, "uw", "d", "()Z", 61);
    if (!mc_java_random_next_bounded(&f->state->chunk_random, 10, &f->state->chunk_draw10)) {
        (void)fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Slime Chunk Random bound", true, NULL);
        goto done;
    }
    ++f->state->chunk_random_calls;
    if (f->state->chunk_draw10 != 0) {
        *out = false;
        ok = true;
        goto done;
    }
    at(f, "uw", "d", "()Z", 74);
    if (!fp(f))
        goto done;
    *out = f->entity->base.y < 16.0;
    ok = true;
done:
    if (leased)
        captured->references.release(captured->references.owner, chunk);
    mc_world_runtime_call_leave(captured);
    if (ok)
        at(f, "uw", "d", "()Z", *out ? 79 : 81);
    return ok;
}

mc_world_storage_status mc_mob_spawn_eligible(mc_entity *entity, mc_math_classes *math,
                                              mc_world_runtime_classes *classes, bool *out,
                                              mc_mob_spawn_state *s, mc_mob_spawn_error *e) {
    if (s == NULL || e == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *s = (mc_mob_spawn_state){0};
    *e = (mc_mob_spawn_error){0};
    spawn_frame f = {entity, math, classes, s, e, false};
    if (entity == NULL || entity->classes->source != MC_ENTITY_CLIENT || entity->mob == NULL) {
        (void)fail(&f, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Mob spawn concrete client receiver",
                   true, NULL);
        return e->status;
    }
    if (classes == NULL || classes->side != MC_WORLD_INFO_CLIENT ||
        classes->profile != entity->classes->profile || classes->entities != entity->classes) {
        (void)fail(&f, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Mob spawn source class namespace", true,
                   NULL);
        return e->status;
    }
    if (classes->native_borrows == SIZE_MAX) {
        (void)fail(&f, MC_WORLD_STORAGE_LIMIT, "Mob spawn source class native hold", true, NULL);
        return e->status;
    }
    ++classes->native_borrows;
    s->kind = entity->class_literal->kind;
    e->entity = mc_entity_retain(entity);
    if (e->entity != MC_ENTITY_OK) {
        (void)fail(&f,
                   e->entity == MC_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "Mob spawn Entity hold", true, NULL);
        --classes->native_borrows;
        return e->status;
    }
    bool result = false, ok;
    switch (s->kind) {
        case MC_ENTITY_CLASS_KIND_SPIDER:
        case MC_ENTITY_CLASS_KIND_ZOMBIE:
        case MC_ENTITY_CLASS_KIND_SKELETON:
        case MC_ENTITY_CLASS_KIND_CREEPER:
            ok = monster(&f, &result);
            break;
        case MC_ENTITY_CLASS_KIND_SLIME:
            ok = slime(&f, &result);
            break;
        case MC_ENTITY_CLASS_KIND_PIG:
        case MC_ENTITY_CLASS_KIND_SHEEP:
        case MC_ENTITY_CLASS_KIND_COW:
        case MC_ENTITY_CLASS_KIND_CHICKEN:
        case MC_ENTITY_CLASS_KIND_WOLF:
            ok = animal(&f, &result);
            break;
        case MC_ENTITY_CLASS_KIND_SQUID:
            at(&f, "ar", "d", "()Z", 8);
            ok = no_entity(&f, &result);
            if (ok)
                at(&f, "ar", "d", "()Z", 11);
            break;
        default:
            ok = fail(&f, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Mob spawn concrete virtual predicate", true, NULL);
            break;
    }
    if (f.math_held)
        (void)mc_math_classes_release(&f.math, NULL);
    mc_entity_release(entity);
    --classes->native_borrows;
    if (ok) {
        s->returned = true;
        s->eligible = result;
        *out = result;
    }
    return e->status;
}
