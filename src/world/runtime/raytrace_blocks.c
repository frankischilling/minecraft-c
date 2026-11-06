#include "../../block/registry/registry_internal.h"
#include "mc/core/java/java.h"
#include "raytrace_internal.h"

#include <emmintrin.h>

static mc_world_raytrace_site site(const char *cl, unsigned int pc) {
    return (mc_world_raytrace_site){cl, "a", "(Lfd;IIILbt;Lbt;)Lvf;", (int)pc};
}

static bool admit(mc_world_raytrace_work *w, const mc_block *b) {
    if (b != NULL && b->classes == w->classes->registry &&
        (unsigned int)b->kind < (unsigned int)MC_BLOCK_KIND_COUNT)
        return true;
    (void)mc_world_raytrace_fail(w, MC_WORLD_RAYTRACE_STATE, "Block ray receiver native provenance",
                                 site("uu", 0), true);
    return false;
}

mc_world_raytrace_status mc_world_raytrace_block_can_collide_held(mc_world_raytrace_work *w,
                                                                  const mc_block *b,
                                                                  int32_t metadata, bool liquid,
                                                                  bool *out) {
    if (!admit(w, b))
        return w->failure.status;
    switch (b->kind) {
        case MC_BLOCK_FLOWING_FLUID:
        case MC_BLOCK_STILL_FLUID:
            *out = liquid && metadata == 0;
            return MC_WORLD_RAYTRACE_OK;
        case MC_BLOCK_FIRE:
            *out = false;
            return MC_WORLD_RAYTRACE_OK;
        case MC_BLOCK_STAIRS:
            if (b->subtype.ss.a == NULL)
                return mc_world_raytrace_null(w, (mc_world_raytrace_site){"ss", "a", "(IZ)Z", 6});
            return mc_world_raytrace_block_can_collide_held(w, b->subtype.ss.a, metadata, liquid,
                                                            out);
        default:
            *out = true; /* All remaining actual classes inherit uu.v_(). */
            return MC_WORLD_RAYTRACE_OK;
    }
}

static bool shape(mc_world_raytrace_work *w, const mc_block *b, int32_t x, int32_t y, int32_t z,
                  const char *cl, unsigned int pc) {
    mc_world_collision_error_init(&w->collision);
    if (mc_world_collision_block_state_shape_held(w->world, b, x, y, z, &w->collision) ==
        MC_WORLD_STORAGE_OK)
        return true;
    (void)mc_world_raytrace_collision_failed(w, site(cl, pc));
    return false;
}

static bool torch(mc_world_raytrace_work *w, const mc_block *block, int32_t x, int32_t y,
                  int32_t z) {
    int32_t value = 0;
    mc_world_collision_error_init(&w->collision);
    if (mc_world_collision_block_value_held(w->world, x, y, z, true, &value, &w->collision) !=
        MC_WORLD_STORAGE_OK) {
        (void)mc_world_raytrace_collision_failed(w, site("vm", 5));
        return false;
    }
    value &= 7;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        (void)mc_world_raytrace_fail(w, MC_WORLD_RAYTRACE_STATE, "Torch ray strict FP admission",
                                     site("vm", value >= 1 && value <= 4 ? 31u : 158u), true);
        return false;
    }
    float r = 0.15f;
    mc_block *b = (mc_block *)block;
    switch (value) {
        case 1:
            mc_registry_block_bounds(b, 0, 0.2f, 0.5f - r, r * 2.0f, 0.8f, 0.5f + r);
            break;
        case 2:
            mc_registry_block_bounds(b, 1.0f - r * 2.0f, 0.2f, 0.5f - r, 1, 0.8f, 0.5f + r);
            break;
        case 3:
            mc_registry_block_bounds(b, 0.5f - r, 0.2f, 0, 0.5f + r, 0.8f, r * 2.0f);
            break;
        case 4:
            mc_registry_block_bounds(b, 0.5f - r, 0.2f, 1.0f - r * 2.0f, 0.5f + r, 0.8f, 1);
            break;
        default:
            r = 0.1f;
            mc_registry_block_bounds(b, 0.5f - r, 0, 0.5f - r, 0.5f + r, 0.6f, 0.5f + r);
            break;
    }
    return true;
}

/* Private uu.a/b/c(bt): positive inclusive comparisons reject NaN. */
static bool on_face(const mc_block *b, const mc_entity_vector *v, size_t axis) {
    if (v == NULL)
        return false;
    const mc_block_fields *f = &b->fields;
    if (axis == 0)
        return v->y >= f->bt && v->y <= f->bw && v->z >= f->bu && v->z <= f->bx;
    if (axis == 1)
        return v->x >= f->bs && v->x <= f->bv && v->z >= f->bu && v->z <= f->bx;
    return v->x >= f->bs && v->x <= f->bv && v->y >= f->bt && v->y <= f->bw;
}

mc_world_raytrace_status mc_world_raytrace_block_held(mc_world_raytrace_work *w, const mc_block *b,
                                                      int32_t x, int32_t y, int32_t z,
                                                      mc_entity_vector *start,
                                                      mc_entity_vector *end,
                                                      mc_entity_hit_result **out) {
    if (!admit(w, b))
        return w->failure.status;
    if (mc_world_raytrace_block_locals_retire(w) != MC_WORLD_RAYTRACE_OK)
        return w->failure.status;
    const char *wrapper = NULL;
    switch (b->kind) {
        case MC_BLOCK_DOOR:
            wrapper = "le";
            break;
        case MC_BLOCK_TRAPDOOR:
            wrapper = "oq";
            break;
        case MC_BLOCK_RAIL:
        case MC_BLOCK_DETECTOR_RAIL:
            wrapper = "pc";
            break;
        case MC_BLOCK_TORCH:
        case MC_BLOCK_REDSTONE_TORCH:
            if (!torch(w, b, x, y, z))
                return w->failure.status;
            break;
        default:
            break;
    }
    if (wrapper != NULL && !shape(w, b, x, y, z, wrapper, 6))
        return w->failure.status;
    if (wrapper != NULL)
        w->block_parent = site(wrapper, 19);
    else if (b->kind == MC_BLOCK_TORCH || b->kind == MC_BLOCK_REDSTONE_TORCH)
        w->block_parent = site("vm", 190);
    if (!shape(w, b, x, y, z, "uu", 6))
        return w->failure.status;
    if (start == NULL)
        return mc_world_raytrace_null(w, site("uu", 21));
    if (mc_entity_vector_add(w->entities, start, (double)mc_java_i32_neg(x),
                             (double)mc_java_i32_neg(y), (double)mc_java_i32_neg(z), &w->local[0],
                             &w->vector) != MC_ENTITY_OK)
        return mc_world_raytrace_vector_failed(w, site("uu", 21));
    if (end == NULL)
        return mc_world_raytrace_null(w, site("uu", 38));
    if (mc_entity_vector_add(w->entities, end, (double)mc_java_i32_neg(x),
                             (double)mc_java_i32_neg(y), (double)mc_java_i32_neg(z), &w->local[1],
                             &w->vector) != MC_ENTITY_OK)
        return mc_world_raytrace_vector_failed(w, site("uu", 38));
    static const unsigned int plane_pc[] = {51, 64, 77, 90, 103, 116};
    for (size_t i = 0; i < 6; ++i) {
        mc_entity_geometry_status status;
        if (i < 2)
            status = mc_entity_vector_intersect_x(w->entities, w->local[0], w->local[1],
                                                  i == 0 ? b->fields.bs : b->fields.bv,
                                                  &w->generated[i], &w->geometry);
        else if (i < 4)
            status = mc_entity_vector_intersect_y(w->entities, w->local[0], w->local[1],
                                                  i == 2 ? b->fields.bt : b->fields.bw,
                                                  &w->generated[i], &w->geometry);
        else
            status = mc_entity_vector_intersect_z(w->entities, w->local[0], w->local[1],
                                                  i == 4 ? b->fields.bu : b->fields.bx,
                                                  &w->generated[i], &w->geometry);
        if (status != MC_ENTITY_GEOMETRY_OK)
            return mc_world_raytrace_geometry_failed(w, site("uu", plane_pc[i]));
    }
    /* Keep generated holds even after the actual local is filtered to NULL. */
    mc_entity_vector *locals[6];
    for (size_t i = 0; i < 6; ++i)
        locals[i] = on_face(b, w->generated[i], i / 2u) ? w->generated[i] : NULL;
    mc_entity_vector *selected = NULL;
    for (size_t i = 0; i < 6; ++i) {
        if (locals[i] == NULL)
            continue;
        if (selected != NULL) {
            double candidate_distance, previous_distance;
            if (mc_entity_vector_distance(w->entities, w->local[0], locals[i], &candidate_distance,
                                          &w->geometry) != MC_ENTITY_GEOMETRY_OK)
                return mc_world_raytrace_geometry_failed(w,
                                                         site("uu", 210u + (unsigned int)i * 32u));
            if (mc_entity_vector_distance(w->entities, w->local[0], selected, &previous_distance,
                                          &w->geometry) != MC_ENTITY_GEOMETRY_OK)
                return mc_world_raytrace_geometry_failed(w,
                                                         site("uu", 217u + (unsigned int)i * 32u));
            if (!(candidate_distance < previous_distance))
                continue;
        }
        selected = locals[i];
    }
    if (selected == NULL) {
        *out = NULL;
        return MC_WORLD_RAYTRACE_OK;
    }
    static const int32_t faces[] = {4, 5, 0, 1, 2, 3};
    int32_t face = -1;
    for (size_t i = 0; i < 6; ++i)
        if (selected == locals[i])
            face = faces[i];
    if (mc_entity_hit_result_allocate(w->entities, &w->candidate, &w->hit) != MC_ENTITY_HIT_OK)
        return mc_world_raytrace_hit_failed(w, site("uu", 458));
    w->state.pending_result = w->candidate;
    if (mc_entity_vector_add(w->entities, selected, (double)x, (double)y, (double)z, &w->point,
                             &w->vector) != MC_ENTITY_OK)
        return mc_world_raytrace_vector_failed(w, site("uu", 477));
    if (mc_entity_hit_result_construct_block(w->candidate, x, y, z, face, w->point, &w->hit) !=
        MC_ENTITY_HIT_OK)
        return mc_world_raytrace_hit_failed(w, site("uu", 480));
    *out = w->candidate;
    w->candidate = NULL;
    w->state.pending_result = NULL;
    return MC_WORLD_RAYTRACE_OK;
}
