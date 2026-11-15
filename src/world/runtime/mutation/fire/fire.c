#include "fire_internal.h"

#include "../../../../block/registry/registry_internal.h"
#include "../../../../block/registry/runtime_internal.h"
#include "../../block_queries_internal.h"
#include "../../collision/blocks_internal.h"
#include "../mutation_internal.h"
#include "../portal_internal.h"
#include "../tnt/tnt_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/environment.h"
#include "mc/world/runtime/movement.h"
#include "mc/world/runtime/scheduling.h"

#include <stdlib.h>

typedef enum fire_method {
    FIRE_ADDED,
    FIRE_NEIGHBOR,
    FIRE_TICK,
    FIRE_CAN_PLACE,
    FIRE_NEIGHBORS,
    FIRE_FLAMMABLE,
    FIRE_ENCOURAGEMENT,
    FIRE_SPREAD,
    FIRE_BURN
} fire_method;

/* Only one synchronous child uses this workspace at a time. Heap storage
 * keeps recursive notifying setters from reserving complete diagnostic graphs
 * in every native stack frame. No Source recursion/work bound follows. */
typedef union fire_scratch {
    mc_world_collision_error value;
    mc_world_solid_support_detail support;
    mc_world_mutation_error mutation;
    mc_world_schedule_error schedule;
    mc_world_movement_error rain;
} fire_scratch;

typedef struct fire_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    fire_method method;
    fire_scratch *scratch;
} fire_frame;

static void fire_at(const fire_frame *f, unsigned int pc) {
    static const char *const methods[] = {
        "c(Lfd;III)V",  "b(Lfd;IIII)V", "a(Lfd;IIILjava/util/Random;)V",
        "a(Lfd;III)Z",  "h(Lfd;III)Z",  "c(Lxp;III)Z",
        "f(Lfd;IIII)I", "i(Lfd;III)I",  "a(Lfd;IIIILjava/util/Random;I)V"};
    unsigned int *const slots[] = {
        &f->error->fire.added,         &f->error->fire.neighbor,  &f->error->fire.tick,
        &f->error->fire.can_place,     &f->error->fire.neighbors, &f->error->fire.flammable,
        &f->error->fire.encouragement, &f->error->fire.spread,    &f->error->fire.burn};
    f->error->hook_class = "yq";
    f->error->hook_method = methods[f->method];
    f->error->hook_bytecode = pc;
    *slots[f->method] = pc;
}

static bool fire_done(const fire_frame *f) {
    f->error->hook_class = f->error->hook_method = NULL;
    f->error->hook_bytecode = 0;
    return true;
}

static bool fire_fail(const fire_frame *f, unsigned int pc, const char *operation) {
    fire_at(f, pc);
    return mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  operation);
}

static bool fire_null(const fire_frame *f, unsigned int pc, const char *operation) {
    f->error->fire_source_null = true;
    return fire_fail(f, pc, operation);
}

static bool fire_storage(const fire_frame *f, unsigned int pc, mc_world_storage_error cause) {
    fire_at(f, pc);
    f->error->status = cause.status;
    f->error->operation = cause.operation;
    f->error->cause = cause;
    if (f->world != NULL)
        f->world->last_error = cause;
    return false;
}

static bool fire_registry(const fire_frame *f, unsigned int pc, const mc_registry_error *cause,
                          const char *operation) {
    fire_at(f, pc);
    if (f->world != NULL)
        return mc_world_mutation_registry_fail(f->world, f->error, cause, operation);
    f->error->registry = *cause;
    return mc_world_mutation_fail(NULL, f->error, mc_world_runtime_registry_status(cause->status),
                                  operation);
}

static bool fire_nested(const fire_frame *f, unsigned int pc,
                        const mc_world_mutation_error *child) {
    if (f->error != child)
        *f->error = *child;
    if (f->error->cause_hook_class == NULL) {
        f->error->cause_hook_class = f->error->hook_class;
        f->error->cause_hook_method = f->error->hook_method;
        f->error->cause_hook_bytecode = f->error->hook_bytecode;
    }
    fire_at(f, pc);
    return false;
}

static bool fire_entry(const fire_frame *f) {
    if (f->error == NULL)
        return false;
    if (f->block == NULL)
        return fire_null(f, 0, "Fire actual Block receiver");
    if (f->block->classes == NULL || f->block->kind != MC_BLOCK_FIRE ||
        f->block->classes->options.side != MC_REGISTRY_CLIENT)
        return fire_fail(f, 0, "Fire actual client concrete receiver");
    return true;
}

static fire_scratch *fire_workspace(fire_frame *f, unsigned int pc) {
    fire_at(f, pc);
    if (f->scratch == NULL)
        f->scratch = malloc(sizeof(*f->scratch));
    if (f->scratch == NULL)
        (void)mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                     "Fire reached child diagnostic allocation");
    return f->scratch;
}

static bool fire_read(fire_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                      bool metadata, int32_t *out) {
    fire_at(f, pc);
    if (f->world == NULL)
        return fire_null(f, pc, "Fire actual World value receiver");
    fire_scratch *work = fire_workspace(f, pc);
    if (work == NULL)
        return false;
    mc_world_collision_error *child = &work->value;
    mc_world_collision_error_init(child);
    if (mc_world_collision_block_value_held(f->world, x, y, z, metadata, out, child) ==
        MC_WORLD_STORAGE_OK)
        return true;
    const mc_world_nearby_error *lower = &child->nearby;
    f->error->nearby = *lower;
    f->error->query = lower->query;
    f->error->population = lower->query.population;
    f->error->entity = lower->entity;
    f->error->tile_entity = lower->tile_entity;
    f->error->batch = lower->batch;
    f->error->load_entity = lower->load_entity;
    f->error->activation_bytecode = lower->activation_bytecode;
    f->error->activation_world_bytecode = lower->activation_world_bytecode;
    f->error->load_bytecode = lower->load_bytecode;
    f->error->load_builder_bytecode = lower->load_builder_bytecode;
    f->error->registry = lower->query.registry;
    f->error->fire_source_null = lower->query.source_null;
    return fire_storage(f, pc, lower->query.cause);
}

mc_registry_status mc_world_fire_array_get(const int32_t *array, int32_t id, int32_t *out,
                                           mc_registry_error *error) {
    *error = mc_world_mutation_empty_registry();
    if (array == NULL) {
        error->status = MC_REGISTRY_NULL_REFERENCE;
        error->message = "Fire captured array receiver";
    } else if (id < 0 || id >= (int32_t)MC_REGISTRY_BLOCK_SLOTS) {
        error->status = MC_REGISTRY_INDEX_OUT_OF_RANGE;
        error->message = "Fire captured array index";
    } else {
        *out = array[(size_t)id];
    }
    return error->status;
}

static bool fire_array(fire_frame *f, unsigned int pc, const int32_t *array, int32_t id,
                       int32_t *out) {
    mc_registry_error cause;
    const mc_registry_status status = mc_world_fire_array_get(array, id, out, &cause);
    if (status == MC_REGISTRY_OK)
        return true;
    f->error->fire_source_null = status == MC_REGISTRY_NULL_REFERENCE;
    f->error->fire_array_index = status == MC_REGISTRY_INDEX_OUT_OF_RANGE;
    return fire_registry(f, pc, &cause, "Fire captured array read");
}

static bool fire_static(fire_frame *f, const char *name, unsigned int load_pc,
                        const mc_block **out) {
    mc_registry_error cause = mc_world_mutation_empty_registry();
    fire_at(f, load_pc);
    if (mc_registry_block_static_get(f->block->classes, name, out, &cause) != MC_REGISTRY_OK)
        return fire_registry(f, load_pc, &cause, "Fire live Block static");
    return true;
}

static bool fire_static_id(fire_frame *f, const char *name, unsigned int load_pc,
                           unsigned int field_pc, int32_t *out) {
    const mc_block *block;
    if (!fire_static(f, name, load_pc, &block))
        return false;
    if (block == NULL)
        return fire_null(f, field_pc, "Fire live Block field receiver");
    *out = block->fields.bn;
    return true;
}

static bool fire_support(fire_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                         bool *out) {
    fire_at(f, pc);
    if (f->world == NULL)
        return fire_null(f, pc, "Fire actual World support receiver");
    fire_scratch *work = fire_workspace(f, pc);
    if (work == NULL)
        return false;
    mc_world_solid_support_detail *child = &work->support;
    if (mc_world_solid_support_detailed_held(f->world, x, y, z, out, child) == MC_WORLD_STORAGE_OK)
        return true;
    f->error->support_bytecode = child->error.bytecode;
    f->error->registry = child->error.registry;
    f->error->nearby = child->access;
    f->error->query = child->access.query;
    f->error->fire_source_null = child->error.source_null || child->access.query.source_null;
    return fire_storage(f, pc, child->error.cause);
}

typedef enum fire_write_kind {
    FIRE_WRITE_ID,
    FIRE_WRITE_METADATA,
    FIRE_WRITE_BOTH
} fire_write_kind;

static bool fire_write(fire_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z, int32_t id,
                       int32_t metadata, fire_write_kind kind) {
    if (f->world == NULL)
        return fire_null(f, pc, "Fire actual World setter receiver");
    if (kind == FIRE_WRITE_METADATA) {
        bool ignored;
        mc_world_storage_error cause = {0};
        if (mc_world_runtime_metadata_set_quiet(f->world, x, y, z, metadata, &ignored, &cause) ==
            MC_WORLD_STORAGE_OK)
            return true;
        return fire_storage(f, pc, cause);
    }
    fire_scratch *work = fire_workspace(f, pc);
    if (work == NULL)
        return false;
    mc_world_mutation_error *child = &work->mutation;
    mc_world_mutation_error_init(child);
    bool ignored;
    mc_world_storage_status status;
    if (kind == FIRE_WRITE_ID)
        status = mc_world_runtime_block_set(f->world, x, y, z, id, &ignored, child);
    else
        status =
            mc_world_runtime_block_metadata_set(f->world, x, y, z, id, metadata, &ignored, child);
    return status == MC_WORLD_STORAGE_OK || fire_nested(f, pc, child);
}

static bool fire_schedule(fire_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z) {
    if (f->world == NULL)
        return fire_null(f, pc, "Fire actual World schedule receiver");
    fire_scratch *work = fire_workspace(f, pc);
    if (work == NULL)
        return false;
    mc_world_schedule_error *child = &work->schedule;
    if (mc_world_runtime_schedule(f->world, x, y, z, f->block->fields.bn, 40, child) ==
        MC_WORLD_STORAGE_OK)
        return true;
    mc_world_mutation_error *lower = &child->tick;
    lower->status = child->status;
    lower->operation = child->operation;
    lower->cause = child->cause;
    lower->registry = child->registry;
    lower->schedule_bytecode = child->bytecode;
    lower->record = child->record;
    lower->membership = child->membership;
    lower->ordered = child->ordered;
    lower->presence_bytecode = child->presence_bytecode;
    lower->provider_bytecode = child->provider_bytecode;
    return fire_nested(f, pc, lower);
}

static bool fire_random(fire_frame *f, unsigned int pc, mc_java_random *random, int32_t bound,
                        int32_t *out) {
    fire_at(f, pc);
    if (random == NULL)
        return fire_null(f, pc, "Fire actual Random receiver");
    if (!mc_java_random_next_bounded(random, bound, out))
        return fire_fail(f, pc, "Fire actual Random bound");
    return true;
}

static bool fire_raining(fire_frame *f, unsigned int pc, bool *out) {
    if (f->world == NULL)
        return fire_null(f, pc, "Fire actual World raining receiver");
    float rain;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_rain_strength(f->world, 1.0f, &rain, &cause) != MC_WORLD_STORAGE_OK) {
        f->error->fire.raining_bytecode = 2;
        f->error->fire.rain_strength_bytecode = 12;
        return fire_storage(f, pc, cause);
    }
    *out = (double)rain > 0.2;
    return true;
}

static bool fire_rain_at(fire_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                         bool *out) {
    if (f->world == NULL)
        return fire_null(f, pc, "Fire actual World rain receiver");
    fire_scratch *work = fire_workspace(f, pc);
    if (work == NULL)
        return false;
    mc_world_movement_error *child = &work->rain;
    if (mc_world_runtime_rain_at(f->world, x, y, z, out, child) == MC_WORLD_STORAGE_OK)
        return true;
    f->error->query = child->query;
    f->error->registry = child->registry;
    f->error->fire_biome = child->biome;
    f->error->fire_dimension = child->dimension;
    f->error->fire.rain_bytecode = child->bytecode;
    f->error->fire.raining_bytecode = child->raining_bytecode;
    f->error->fire.rain_strength_bytecode = child->rain_strength_bytecode;
    f->error->fire.dimension_bytecode = child->dimension_bytecode;
    f->error->fire.biome_bytecode = child->biome_bytecode;
    f->error->fire_source_null = child->source_null;
    return fire_storage(f, pc, child->cause);
}

static void fire_direction(int32_t direction, int32_t x, int32_t y, int32_t z, int32_t *nx,
                           int32_t *ny, int32_t *nz) {
    *nx = x;
    *ny = y;
    *nz = z;
    if (direction == 0)
        *nx = mc_java_i32_add(x, 1);
    if (direction == 1)
        *nx = mc_java_i32_sub(x, 1);
    if (direction == 2)
        *ny = mc_java_i32_sub(y, 1);
    if (direction == 3)
        *ny = mc_java_i32_add(y, 1);
    if (direction == 4)
        *nz = mc_java_i32_sub(z, 1);
    if (direction == 5)
        *nz = mc_java_i32_add(z, 1);
}

static bool fire_flammable(fire_frame *f, int32_t x, int32_t y, int32_t z, bool *out) {
    const fire_method parent = f->method;
    f->method = FIRE_FLAMMABLE;
    const int32_t *const array = f->block->subtype.yq.a; /* getfield1, before getter9. */
    int32_t id = 0, encouragement = 0;
    const bool complete =
        fire_read(f, 9, x, y, z, false, &id) && fire_array(f, 14, array, id, &encouragement);
    if (complete)
        *out = encouragement > 0;
    f->method = parent;
    return complete;
}

static bool fire_neighbors(fire_frame *f, int32_t x, int32_t y, int32_t z, bool *out) {
    static const unsigned int calls[] = {8, 24, 40, 56, 72, 88};
    const fire_method parent = f->method;
    f->method = FIRE_NEIGHBORS;
    *out = false;
    for (int32_t direction = 0; direction < 6; ++direction) {
        int32_t nx, ny, nz;
        fire_direction(direction, x, y, z, &nx, &ny, &nz);
        bool flammable;
        if (!fire_flammable(f, nx, ny, nz, &flammable)) {
            (void)fire_nested(f, calls[direction], f->error);
            f->method = parent;
            return false;
        }
        if (flammable) {
            *out = true;
            break;
        }
    }
    f->method = parent;
    return true;
}

static bool fire_place(fire_frame *f, int32_t x, int32_t y, int32_t z, bool *out) {
    const fire_method parent = f->method;
    f->method = FIRE_CAN_PLACE;
    bool value;
    bool complete = fire_support(f, 7, x, mc_java_i32_sub(y, 1), z, &value);
    if (complete && !value) {
        complete = fire_neighbors(f, x, y, z, &value);
        if (!complete)
            (void)fire_nested(f, 19, f->error);
    }
    if (complete)
        *out = value;
    f->method = parent;
    return complete;
}

static bool fire_spread(fire_frame *f, int32_t x, int32_t y, int32_t z, int32_t *out) {
    const fire_method parent = f->method;
    f->method = FIRE_SPREAD;
    int32_t id;
    if (!fire_read(f, 8, x, y, z, false, &id)) {
        f->method = parent;
        return false;
    }
    int32_t best = 0;
    if (id == 0) {
        static const unsigned int calls[] = {26, 41, 56, 71, 86, 101};
        for (int32_t direction = 0; direction < 6; ++direction) {
            int32_t nx, ny, nz, value;
            fire_direction(direction, x, y, z, &nx, &ny, &nz);
            f->method = FIRE_ENCOURAGEMENT;
            const int32_t *const array = f->block->subtype.yq.a;
            if (!fire_read(f, 9, nx, ny, nz, false, &id) || !fire_array(f, 12, array, id, &value)) {
                f->method = FIRE_SPREAD;
                (void)fire_nested(f, calls[direction], f->error);
                f->method = parent;
                return false;
            }
            best = value > best ? value : best;
            f->method = FIRE_SPREAD;
        }
    }
    *out = best;
    f->method = parent;
    return true;
}

static bool fire_burn(fire_frame *f, int32_t x, int32_t y, int32_t z, int32_t chance,
                      mc_java_random *random, int32_t age) {
    const fire_method parent = f->method;
    f->method = FIRE_BURN;
    const int32_t *const array = f->block->subtype.yq.b;
    int32_t id = 0, flammability = 0, draw = 0, tnt = 0;
    bool complete = fire_read(f, 9, x, y, z, false, &id) &&
                    fire_array(f, 12, array, id, &flammability) &&
                    fire_random(f, 19, random, chance, &draw);
    if (complete && draw < flammability) {
        complete = fire_read(f, 32, x, y, z, false, &id) && fire_static_id(f, "an", 35, 38, &tnt);
        if (complete) {
            const bool prime = id == tnt;
            bool wet = false;
            complete = fire_random(f, 58, random, mc_java_i32_add(age, 10), &draw);
            if (complete && draw < 5)
                complete = fire_rain_at(f, 70, x, y, z, &wet);
            if (complete && draw < 5 && !wet) {
                complete = fire_random(f, 81, random, 5, &draw);
                int32_t next_age = mc_java_i32_add(age, draw / 4);
                if (next_age > 15)
                    next_age = 15;
                if (complete)
                    complete =
                        fire_write(f, 111, x, y, z, f->block->fields.bn, next_age, FIRE_WRITE_BOTH);
            } else if (complete)
                complete = fire_write(f, 124, x, y, z, 0, 0, FIRE_WRITE_ID);
            if (complete && prime) {
                const mc_block *selected;
                complete = fire_static(f, "an", 133, &selected);
                if (complete && selected == NULL)
                    complete = fire_null(f, 142, "Fire live TNT activation receiver");
                if (complete) {
                    fire_scratch *work = fire_workspace(f, 142);
                    if (work == NULL)
                        complete = false;
                    else {
                        mc_world_mutation_error_init(&work->mutation);
                        complete = mc_world_mutation_tnt_activate(f->world, selected, x, y, z, 1,
                                                                  &work->mutation);
                        if (!complete)
                            (void)fire_nested(f, 142, &work->mutation);
                    }
                }
            }
        }
    }
    f->method = parent;
    return complete;
}

static bool fire_tick(fire_frame *f, int32_t x, int32_t y, int32_t z, mc_java_random *random) {
    int32_t below = 0, netherrack = 0, age = 0, draw = 0;
    if (!fire_read(f, 7, x, mc_java_i32_sub(y, 1), z, false, &below) ||
        !fire_static_id(f, "bc", 10, 13, &netherrack))
        return false;
    const bool eternal = below == netherrack;
    bool can_place;
    if (!fire_place(f, x, y, z, &can_place))
        return fire_nested(f, 32, f->error);
    /* Original removal44 deliberately falls through into the remaining tick. */
    if (!can_place && !fire_write(f, 44, x, y, z, 0, 0, FIRE_WRITE_ID))
        return false;
    bool rainy = false;
    if (!eternal) {
        if (!fire_raining(f, 54, &rainy))
            return false;
        if (rainy) {
            static const unsigned int calls[] = {65, 78, 91, 104, 117};
            bool wet = false;
            for (int32_t direction = 0; direction < 5 && !wet; ++direction) {
                int32_t nx = x, nz = z;
                if (direction == 1)
                    nx = mc_java_i32_sub(x, 1);
                if (direction == 2)
                    nx = mc_java_i32_add(x, 1);
                if (direction == 3)
                    nz = mc_java_i32_sub(z, 1);
                if (direction == 4)
                    nz = mc_java_i32_add(z, 1);
                if (!fire_rain_at(f, calls[direction], nx, y, nz, &wet))
                    return false;
            }
            if (wet)
                return fire_write(f, 129, x, y, z, 0, 0, FIRE_WRITE_ID);
        }
    }
    if (!fire_read(f, 139, x, y, z, true, &age))
        return false;
    if (age < 15) {
        if (!fire_random(f, 161, random, 3, &draw) ||
            !fire_write(f, 167, x, y, z, 0, mc_java_i32_add(age, draw / 2), FIRE_WRITE_METADATA))
            return false;
    }
    if (!fire_schedule(f, 184, x, y, z))
        return false;
    if (!eternal) {
        bool neighbors;
        if (!fire_neighbors(f, x, y, z, &neighbors))
            return fire_nested(f, 198, f->error);
        if (!neighbors) {
            bool support;
            if (!fire_support(f, 211, x, mc_java_i32_sub(y, 1), z, &support))
                return false;
            if (!support || age > 3)
                return fire_write(f, 229, x, y, z, 0, 0, FIRE_WRITE_ID);
            return true;
        }
        bool flammable;
        if (!fire_flammable(f, x, mc_java_i32_sub(y, 1), z, &flammable))
            return fire_nested(f, 247, f->error);
        if (!flammable && age == 15) {
            if (!fire_random(f, 263, random, 4, &draw))
                return false;
            if (draw == 0)
                return fire_write(f, 275, x, y, z, 0, 0, FIRE_WRITE_ID);
        }
    }
    static const unsigned int burns[] = {295, 313, 331, 349, 367, 385};
    for (int32_t direction = 0; direction < 6; ++direction) {
        int32_t nx, ny, nz;
        fire_direction(direction, x, y, z, &nx, &ny, &nz);
        if (!fire_burn(f, nx, ny, nz, direction == 2 || direction == 3 ? 250 : 300, random, age))
            return fire_nested(f, burns[direction], f->error);
    }
    /* Source loops use wrapped int endpoints and increments. Do not replace
     * them with fixed offsets: overflow can change whether a loop executes. */
    for (int32_t nx = mc_java_i32_sub(x, 1); nx <= mc_java_i32_add(x, 1);
         nx = mc_java_i32_add(nx, 1))
        for (int32_t nz = mc_java_i32_sub(z, 1); nz <= mc_java_i32_add(z, 1);
             nz = mc_java_i32_add(nz, 1))
            for (int32_t ny = mc_java_i32_sub(y, 1); ny <= mc_java_i32_add(y, 4);
                 ny = mc_java_i32_add(ny, 1)) {
                if (nx == x && ny == y && nz == z)
                    continue;
                int32_t bound = 100;
                const int32_t above = mc_java_i32_add(y, 1);
                if (ny > above)
                    bound =
                        mc_java_i32_add(bound, mc_java_i32_mul(mc_java_i32_sub(ny, above), 100));
                int32_t encouragement;
                if (!fire_spread(f, nx, ny, nz, &encouragement))
                    return fire_nested(f, 485, f->error);
                if (encouragement <= 0)
                    continue;
                int32_t probability;
                if (!mc_java_i32_div(mc_java_i32_add(encouragement, 40), mc_java_i32_add(age, 30),
                                     &probability))
                    return fire_fail(f, 505, "Fire spread actual integer divisor");
                if (probability <= 0)
                    continue;
                if (!fire_random(f, 517, random, bound, &draw))
                    return false;
                if (draw > probability)
                    continue;
                if (!fire_raining(f, 526, &rainy))
                    return false;
                bool wet = false;
                if (rainy && !fire_rain_at(f, 539, nx, ny, nz, &wet))
                    return false;
                if (wet)
                    continue;
                /* This west query uses the ORIGINAL center Z at554. */
                if (!fire_rain_at(f, 554, mc_java_i32_sub(nx, 1), ny, z, &wet))
                    return false;
                if (wet)
                    continue;
                if (!fire_rain_at(f, 569, mc_java_i32_add(nx, 1), ny, nz, &wet))
                    return false;
                if (wet)
                    continue;
                if (!fire_rain_at(f, 584, nx, ny, mc_java_i32_sub(nz, 1), &wet))
                    return false;
                if (wet)
                    continue;
                if (!fire_rain_at(f, 599, nx, ny, mc_java_i32_add(nz, 1), &wet))
                    return false;
                if (wet)
                    continue;
                if (!fire_random(f, 613, random, 5, &draw))
                    return false;
                int32_t next_age = mc_java_i32_add(age, draw / 4);
                if (next_age > 15)
                    next_age = 15;
                if (!fire_write(f, 645, nx, ny, nz, f->block->fields.bn, next_age, FIRE_WRITE_BOTH))
                    return false;
            }
    return true;
}

bool mc_world_mutation_fire_can_place(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, bool *out,
                                      mc_world_mutation_error *error) {
    fire_frame f = {world, block, error, FIRE_CAN_PLACE, NULL};
    const bool complete = fire_entry(&f) && fire_place(&f, x, y, z, out);
    if (complete)
        (void)fire_done(&f);
    free(f.scratch);
    return complete;
}

bool mc_world_mutation_fire_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, int32_t source_id,
                                     mc_world_mutation_error *error) {
    (void)source_id;
    fire_frame f = {world, block, error, FIRE_NEIGHBOR, NULL};
    bool support = false, neighbors = false;
    bool complete = fire_entry(&f) && fire_support(&f, 7, x, mc_java_i32_sub(y, 1), z, &support);
    if (complete && !support) {
        complete = fire_neighbors(&f, x, y, z, &neighbors);
        if (!complete)
            (void)fire_nested(&f, 19, error);
    }
    if (complete && !support && !neighbors)
        complete = fire_write(&f, 31, x, y, z, 0, 0, FIRE_WRITE_ID);
    if (complete)
        (void)fire_done(&f);
    free(f.scratch);
    return complete;
}

bool mc_world_mutation_fire_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_world_mutation_error *error) {
    fire_frame f = {world, block, error, FIRE_ADDED, NULL};
    int32_t below = 0, obsidian = 0;
    bool complete = fire_entry(&f) &&
                    fire_read(&f, 7, x, mc_java_i32_sub(y, 1), z, false, &below) &&
                    fire_static_id(&f, "aq", 10, 13, &obsidian);
    bool portal = false;
    if (complete && below == obsidian) {
        const mc_block *selected;
        complete = fire_static(&f, "bf", 19, &selected);
        if (complete && selected == NULL)
            complete = fire_null(&f, 27, "Fire actual Portal creation receiver");
        if (complete) {
            fire_scratch *work = fire_workspace(&f, 27);
            if (work == NULL)
                complete = false;
            else {
                mc_world_mutation_error_init(&work->mutation);
                complete = mc_world_mutation_portal_create(world, selected, x, y, z, &portal,
                                                           &work->mutation);
                if (!complete)
                    (void)fire_nested(&f, 27, &work->mutation);
            }
        }
    }
    if (complete && !portal) {
        bool support = false, neighbors = false;
        complete = fire_support(&f, 41, x, mc_java_i32_sub(y, 1), z, &support);
        if (complete && !support) {
            complete = fire_neighbors(&f, x, y, z, &neighbors);
            if (!complete)
                (void)fire_nested(&f, 53, error);
        }
        if (complete)
            complete = !support && !neighbors ? fire_write(&f, 65, x, y, z, 0, 0, FIRE_WRITE_ID)
                                              : fire_schedule(&f, 83, x, y, z);
    }
    if (complete)
        (void)fire_done(&f);
    free(f.scratch);
    return complete;
}

bool mc_world_mutation_fire_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                 int32_t y, int32_t z, mc_java_random *random,
                                 mc_world_mutation_error *error) {
    fire_frame f = {world, block, error, FIRE_TICK, NULL};
    const bool complete = fire_entry(&f) && fire_tick(&f, x, y, z, random);
    if (complete)
        (void)fire_done(&f);
    free(f.scratch);
    return complete;
}
