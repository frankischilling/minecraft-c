#include "plants_internal.h"

#include "mc/world/runtime/movement.h"

static bool farmland_static(const mc_plant_frame *frame, const char *name, unsigned int capture_pc,
                            unsigned int field_pc, int32_t *out) {
    mc_registry_error registry = mc_world_mutation_empty_registry();
    const mc_block *selected;
    mc_plant_at(frame, capture_pc);
    if (mc_registry_block_static_get(frame->block->classes, name, &selected, &registry) !=
        MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(frame->world, frame->error, &registry,
                                               "Farmland Block static active use");
    mc_plant_at(frame, field_pc);
    if (selected == NULL) {
        registry.status = MC_REGISTRY_NULL_REFERENCE;
        registry.message = "Farmland static Block receiver is null";
        return mc_world_mutation_registry_fail(frame->world, frame->error, &registry,
                                               registry.message);
    }
    *out = selected->fields.bn;
    return true;
}

static bool farmland_material(const mc_plant_frame *frame, unsigned int pc, int32_t x, int32_t y,
                              int32_t z, mc_material **out) {
    mc_plant_at(frame, pc);
    if (mc_world_runtime_material_get_detailed(frame->world, x, y, z, out,
                                               &frame->error->material) == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->registry = frame->error->material.registry;
    return mc_plant_storage_fail(frame, pc, frame->error->material.cause);
}

bool mc_plant_farmland_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                int32_t y, int32_t z, int32_t source_id,
                                mc_world_mutation_error *error) {
    (void)source_id;
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, "vl", "no", "b(Lfd;IIII)V", "b(Ldj;IIII)V");
    /* The actual invokespecial uu/na.b at8 returns without reading arguments. */
    mc_material *above;
    if (!farmland_material(&frame, 18, x, mc_java_i32_add(y, 1), z, &above))
        return false;
    mc_plant_at(&frame, 25);
    if (above == NULL) {
        mc_registry_error registry = mc_world_mutation_empty_registry();
        registry.status = MC_REGISTRY_NULL_REFERENCE;
        registry.message = "Farmland neighbor Material.a receiver is null";
        return mc_world_mutation_registry_fail(world, error, &registry, registry.message);
    }
    if (mc_material_a(above)) {
        int32_t dirt;
        if (!farmland_static(&frame, "w", 36, 39, &dirt) ||
            !mc_plant_set(&frame, 42, x, y, z, dirt))
            return false;
    }
    return mc_plant_done(&frame);
}

/* vl.i/no.h: all three counters and their absolute bounds retain Java int
 * wrapping. A reached water static follows each fresh World material query;
 * nullable material references participate in the ordinary identity compare. */
static bool farmland_water(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                           int32_t z, bool *out, mc_world_mutation_error *error) {
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, "vl", "no", "i(Lfd;III)Z", "h(Ldj;III)Z");
    for (int32_t nx = mc_java_i32_sub(x, 4); nx <= mc_java_i32_add(x, 4);
         nx = mc_java_i32_add(nx, 1)) {
        for (int32_t ny = y; ny <= mc_java_i32_add(y, 1); ny = mc_java_i32_add(ny, 1)) {
            for (int32_t nz = mc_java_i32_sub(z, 4); nz <= mc_java_i32_add(z, 4);
                 nz = mc_java_i32_add(nz, 1)) {
                mc_material *material;
                if (!farmland_material(&frame, 46, nx, ny, nz, &material))
                    return false;
                mc_plant_at(&frame, 49);
                mc_material *water;
                mc_registry_error registry = mc_world_mutation_empty_registry();
                if (mc_registry_material_get(block->classes, MC_MATERIAL_g, &water, &registry) !=
                    MC_REGISTRY_OK)
                    return mc_world_mutation_registry_fail(world, error, &registry,
                                                           "Farmland water Material.g active use");
                if (material == water) {
                    *out = true;
                    return mc_plant_done(&frame);
                }
            }
        }
    }
    *out = false;
    return mc_plant_done(&frame);
}

/* vl.h/no.g uses literal radius0 but still has two absolute, incrementing
 * loops. Preserve their wrapped behavior at INT32_MAX and the late crop ID. */
static bool farmland_crop(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                          int32_t z, bool *out, mc_world_mutation_error *error) {
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, "vl", "no", "h(Lfd;III)Z", "g(Ldj;III)Z");
    for (int32_t nx = x; nx <= x; nx = mc_java_i32_add(nx, 1)) {
        for (int32_t nz = z; nz <= z; nz = mc_java_i32_add(nz, 1)) {
            int32_t id, crop;
            if (!mc_plant_block(&frame, 43, nx, mc_java_i32_add(y, 1), nz, &id) ||
                !farmland_static(&frame, "aA", 46, 49, &crop))
                return false;
            if (id == crop) {
                *out = true;
                return mc_plant_done(&frame);
            }
        }
    }
    *out = false;
    return mc_plant_done(&frame);
}

bool mc_plant_farmland_tick(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                            int32_t z, mc_java_random *random, mc_world_mutation_error *error) {
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, "vl", "no", "a(Lfd;IIILjava/util/Random;)V",
                            "a(Ldj;IIILjava/util/Random;)V");
    int32_t draw;
    if (!mc_plant_random(&frame, 3, random, 5, &draw))
        return false;
    if (draw != 0)
        return mc_plant_done(&frame);
    bool hydrated;
    if (!farmland_water(world, block, x, y, z, &hydrated, error))
        return mc_plant_nested(&frame, 15, error);
    if (!hydrated) {
        mc_plant_at(&frame, 28);
        mc_world_movement_error rain = mc_world_movement_empty_error();
        if (mc_world_runtime_rain_at(world, x, mc_java_i32_add(y, 1), z, &hydrated, &rain) !=
            MC_WORLD_STORAGE_OK) {
            error->query = rain.query;
            error->registry = rain.registry;
            error->population = rain.query.population;
            return mc_plant_storage_fail(&frame, 28, rain.cause);
        }
    }
    if (hydrated) {
        if (!mc_plant_set_metadata(&frame, 41, x, y, z, 7))
            return false;
    } else {
        int32_t moisture;
        if (!mc_plant_metadata(&frame, 52, x, y, z, &moisture))
            return false;
        if (moisture > 0) {
            if (!mc_plant_set_metadata(&frame, 71, x, y, z, mc_java_i32_sub(moisture, 1)))
                return false;
        } else {
            bool crop;
            if (!farmland_crop(world, block, x, y, z, &crop, error))
                return mc_plant_nested(&frame, 83, error);
            if (!crop) {
                int32_t dirt;
                if (!farmland_static(&frame, "w", 94, 97, &dirt) ||
                    !mc_plant_set(&frame, 100, x, y, z, dirt))
                    return false;
            }
        }
    }
    return mc_plant_done(&frame);
}

bool mc_plant_farmland_contact(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                               int32_t z, mc_entity *entity, mc_world_mutation_error *error) {
    (void)entity; /* The actual Entity parameter is never read. */
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, "vl", "no", "b(Lfd;IIILsn;)V", "b(Ldj;IIILlq;)V");
    mc_plant_at(&frame, 1);
    if (world == NULL)
        return mc_plant_fail(&frame, 1, "Farmland step World Random receiver is null");
    int32_t draw;
    if (!mc_plant_random(&frame, 5, world->fields.r, 4, &draw))
        return false;
    if (draw == 0) {
        int32_t dirt;
        if (!farmland_static(&frame, "w", 16, 19, &dirt) ||
            !mc_plant_set(&frame, 22, x, y, z, dirt))
            return false;
    }
    return mc_plant_done(&frame);
}
