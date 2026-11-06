#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"
#include "../../application_state.h"
#include "../model_graph_internal.h"
#include "mc/block/registry/runtime.h"

/* The renderer reaches the genuine pooled BT objects. Native holds protect
 * those same objects through failure cleanup; they do not reset the pool. */
static bool lever_vector(mc_cv_context *c, double x, double y, double z, unsigned int pc,
                         mc_entity_vector **out) {
    mc_cv_pc(c, pc);
    if (c->models == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "Lever actual vector class namespace", pc, true);
    const mc_entity_status status =
        mc_entity_vector_pool_get(c->models->entities, x, y, z, out, &c->fluid_vector_error);
    if (status == MC_ENTITY_OK)
        return true;
    c->failure.child =
        (mc_client_graphics_site){"bt", "b", "(DDD)Lbt;", c->fluid_vector_error.bytecode};
    return mc_cv_fail(c,
                      status == MC_ENTITY_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                      : status == MC_ENTITY_ALLOCATION_FAILED
                          ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                      : status == MC_ENTITY_BUSY ? MC_CLIENT_RENDER_GLOBAL_BUSY
                                                 : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                      c->fluid_vector_error.operation, pc, true);
}

/* Entered BT.a/b(F)V: MathHelper cosine precedes sine, all products widen
 * their binary32 lookup results before binary64 arithmetic, and all three
 * stores follow the complete calculation. Rotation never allocates a Vec3. */
static bool lever_rotate(mc_cv_context *c, mc_entity_vector *v, bool around_y, float angle,
                         unsigned int pc) {
    mc_cv_pc(c, pc);
    if (!mc_cv_push_source(c, "bt", around_y ? "b" : "a", "(F)V"))
        return false;
    if (c->application == NULL || mc_math_classes_ensure(c->application->source_math_classes,
                                                         &c->failure.math) != MC_MATH_CLASS_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          "Lever actual MathHelper active use", 1, true);
    if (!mc_cv_fp(c, 1))
        return false;
    const mc_math_trig *table = mc_math_classes_table(c->application->source_math_classes);
    const double cosine = at_widen(mc_math_cos(table, angle));
    const double sine = at_widen(mc_math_sin(table, angle));
    const double a = around_y ? v->x : v->y;
    const double b = v->z;
    const double first = at_add(at_mul(a, cosine), at_mul(b, sine));
    const double last = at_sub(at_mul(b, cosine), at_mul(a, sine));
    if (around_y)
        v->x = first;
    else
        v->y = first;
    v->z = last;
    mc_cv_pop(c);
    return true;
}

static bool lever_bounds(mc_cv_context *c, const mc_block *block, int32_t orientation) {
    static const float boxes[6][6] = {
        {0, .25f, .3125f, .1875f, .75f, .6875f}, {.8125f, .25f, .3125f, 1, .75f, .6875f},
        {.3125f, .25f, 0, .6875f, .75f, .1875f}, {.3125f, .25f, .8125f, .6875f, .75f, 1},
        {.3125f, 0, .25f, .6875f, .1875f, .75f}, {.25f, 0, .3125f, .75f, .1875f, .6875f}};
    static const unsigned int pcs[6] = {298, 262, 227, 188, 113, 150};
    if (orientation < 1 || orientation > 6)
        return true; /* Metadata0/7 retains the preceding update's bounds. */
    const float *box = boxes[orientation - 1];
    if (mc_block_render_bounds((mc_block *)block, box[0], box[1], box[2], box[3], box[4], box[5]) !=
        MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "Lever actual Block bounds",
                          pcs[orientation - 1], true);
    return true;
}

/* Actual cv.e(Block,III). Source caller owns its frame and begun NW batch.
 * The six-face base uses the ordinary shading body, then the handle appends
 * six unshaded quads. The source leaves base bounds and AO scratch intact. */
bool mc_cv_lever(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                 bool *out) {
    int32_t metadata;
    if (!mc_cv_metadata(c, x, y, z, 8, &metadata) || !mc_cv_nw(c, 37))
        return false;
    mc_client_tessellator *const captured_nw = c->tessellator;
    const int32_t orientation = metadata & 7;
    const bool powered = (metadata & 8) > 0;
    const bool override = c->fields->d >= 0;
    if (!override) {
        const mc_block *cobblestone = NULL;
        mc_cv_pc(c, 62);
        if (mc_registry_block_static_get(c->registry, "x", &cobblestone, &c->failure.registry) !=
            MC_REGISTRY_OK)
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                              "Lever actual Cobblestone static active use", 62, true);
        const mc_block_fields *fields;
        if (!mc_cv_fields(c, cobblestone, 65, &fields))
            return false;
        c->fields->d = fields->bm;
        mc_cv_store(c, 68);
    }
    if (!mc_cv_fp(c, 94) || !lever_bounds(c, block, orientation))
        return false;
    mc_cv_pc(c, 307);
    bool ignored;
    if (!mc_cv_push(c, "l", "(Luu;III)Z") || !mc_cv_standard(c, block, x, y, z, &ignored))
        return false;
    mc_cv_pop(c);
    if (!override) {
        c->fields->d = -1;
        mc_cv_store(c, 318);
    }
    float brightness;
    if (!mc_cv_brightness(c, block, x, y, z, 330, &brightness))
        return false;
    mc_registry_block_view registry;
    const mc_block_fields *fields;
    if (!mc_cv_registry(c, 335, &registry) || !mc_cv_fields(c, block, 339, &fields))
        return false;
    if (registry.s == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "Lever actual emission array",
                          342, false);
    if (fields->bn < 0 || fields->bn >= 256) {
        c->failure.registry =
            (mc_registry_error){.status = MC_REGISTRY_INDEX_OUT_OF_RANGE, .block_id = fields->bn};
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, "Lever actual emission index",
                          342, false);
    }
    if (registry.s[fields->bn] > 0)
        brightness = 1;
    c->tessellator = captured_nw;
    if (!mc_cv_color(c, brightness, brightness, brightness, 357))
        return false;
    int32_t texture;
    if (!mc_cv_side_texture(c, block, 0, 362, &texture))
        return false;
    if (c->fields->d >= 0)
        texture = c->fields->d;
    if (!mc_cv_fp(c, 397))
        return false;
    const int32_t tx = (texture & 15) << 4, ty = texture & 240;
    float u0 = at_fdiv((float)tx, 256), u1 = at_fdiv(at_fadd((float)tx, 15.99f), 256);
    float v0 = at_fdiv((float)ty, 256), v1 = at_fdiv(at_fadd((float)ty, 15.99f), 256);
    static const double corners[8][3] = {
        {-.0625, 0, -.0625},    {.0625, 0, -.0625},    {.0625, 0, .0625},    {-.0625, 0, .0625},
        {-.0625, .625, -.0625}, {.0625, .625, -.0625}, {.0625, .625, .0625}, {-.0625, .625, .0625}};
    static const unsigned int factory_pcs[8] = {466, 481, 495, 510, 528, 545, 562, 580};
    mc_entity_vector *vectors[8] = {0};
    bool completed = false;
    for (size_t i = 0; i < 8; ++i)
        if (!lever_vector(c, corners[i][0], corners[i][1], corners[i][2], factory_pcs[i],
                          &vectors[i]))
            goto cleanup;
    for (size_t i = 0; i < 8; ++i) {
        mc_entity_vector *v = vectors[i];
        v->z = powered ? at_sub(v->z, .0625) : at_add(v->z, .0625);
        if (!lever_rotate(c, v, false, powered ? .69813174f : -.69813174f, powered ? 622u : 651u))
            goto cleanup;
        if (orientation == 6 && !lever_rotate(c, v, true, 1.5707964f, 668))
            goto cleanup;
        if (orientation < 5) {
            v->y = at_sub(v->y, .375);
            if (!lever_rotate(c, v, false, 1.5707964f, 700))
                goto cleanup;
            static const float angles[4] = {-1.5707964f, 1.5707964f, 3.1415927f, 0};
            static const unsigned int pcs[4] = {763, 747, 731, 715};
            if (orientation >= 1 &&
                !lever_rotate(c, v, true, angles[orientation - 1], pcs[orientation - 1]))
                goto cleanup;
        }
        v->x = at_add(v->x, at_add((double)x, .5));
        v->y = at_add(v->y, at_widen(at_fadd((float)y, orientation < 5 ? .5f : .125f)));
        v->z = at_add(v->z, at_add((double)z, .5));
    }
    static const unsigned int faces[6][4] = {{0, 1, 2, 3}, {7, 6, 5, 4}, {1, 0, 4, 5},
                                             {2, 1, 5, 6}, {3, 2, 6, 7}, {0, 3, 7, 4}};
    static const unsigned int vertex_pc[4] = {1250, 1276, 1302, 1328};
    for (size_t face = 0; face < 6; ++face) {
        if (face == 0 || face == 2) {
            u0 = at_fdiv((float)(tx + 7), 256);
            u1 = at_fdiv(at_fsub((float)(tx + 9), .01f), 256);
            v0 = at_fdiv((float)(ty + 6), 256);
            v1 = at_fdiv(at_fsub((float)(ty + (face == 0 ? 8 : 16)), .01f), 256);
        }
        const double uv[4][2] = {{at_widen(u0), at_widen(v1)},
                                 {at_widen(u1), at_widen(v1)},
                                 {at_widen(u1), at_widen(v0)},
                                 {at_widen(u0), at_widen(v0)}};
        for (size_t vertex = 0; vertex < 4; ++vertex) {
            const mc_entity_vector *v = vectors[faces[face][vertex]];
            if (!mc_cv_vertex(c, v->x, v->y, v->z, uv[vertex][0], uv[vertex][1], vertex_pc[vertex]))
                goto cleanup;
        }
    }
    mc_cv_pc(c, 1338);
    *out = true;
    completed = true;
cleanup:
    for (size_t i = 0; i < 8; ++i)
        if (vectors[i] != NULL)
            mc_entity_vector_release(c->models->entities, vectors[i]);
    return completed;
}
