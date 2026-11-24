#include "internal.h"

#include "../../../block/registry/horizontal_directions_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../../../entity/player/tick_math_internal.h"
#include "../../../world/runtime/power/power_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

static bool registry_result(mc_cv_context *c, mc_registry_status status, unsigned int pc,
                            const char *operation) {
    if (status == MC_REGISTRY_OK)
        return true;
    return mc_cv_fail(
        c,
        status == MC_REGISTRY_NULL_REFERENCE      ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
        : status == MC_REGISTRY_LIMIT             ? MC_CLIENT_RENDER_GLOBAL_LIMIT
        : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
        : status == MC_REGISTRY_BUSY              ? MC_CLIENT_RENDER_GLOBAL_BUSY
        : status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY
            ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
            : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
        operation, pc,
        status != MC_REGISTRY_NULL_REFERENCE && status != MC_REGISTRY_INDEX_OUT_OF_RANGE);
}

static bool static_id(mc_cv_context *c, const char *name, unsigned int static_pc,
                      unsigned int field_pc, int32_t *out) {
    mc_cv_pc(c, static_pc);
    const mc_block *block = NULL;
    if (!registry_result(
            c, mc_registry_block_static_get(c->registry, name, &block, &c->failure.registry),
            static_pc, "Redstone actual Block static active use"))
        return false;
    const mc_block_fields *fields;
    if (!mc_cv_fields(c, block, field_pc, &fields))
        return false;
    *out = fields->bn;
    return true;
}

/* The held leaf resolves the same actual Block.f overrides as World power.
 * It performs no World read or class active use. A null receiver fails at the
 * invoking SM instruction, before entering a Block method. */
static bool capability(mc_cv_context *c, const mc_block *block, bool *out) {
    mc_cv_pc(c, 37);
    if (block == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "Redstone connectivity Block.f null receiver", 37, false);
    mc_world_power_error error;
    const mc_world_storage_status status = mc_world_block_power_capability_held(block, out, &error);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    c->failure.registry = error.registry;
    c->failure.world = error.cause;
    if (error.capability.entered) {
        if (!mc_cv_push_source(c, error.capability.declaring_class, "f", "()Z"))
            return false;
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, error.operation,
                          error.capability.bytecode, true);
    }
    return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, error.operation, 37, true);
}

/* Actual static SM.e(XP,IIII). Capture each caller's XP argument once; a
 * Registry callback changing CV.c cannot retarget this helper's later read. */
static bool connected(mc_cv_context *c, int32_t x, int32_t y, int32_t z, int32_t side,
                      unsigned int pc, bool *out) {
    const void *receiver = c->fields->c;
    mc_cv_pc(c, pc);
    if (!mc_cv_push_source(c, "sm", "e", "(Lxp;IIII)Z"))
        return false;
    int32_t id, expected;
    if (!mc_cv_block_bound(c, receiver, x, y, z, 4, &id) || !static_id(c, "aw", 13, 16, &expected))
        return false;
    bool value;
    unsigned int returned;
    if (id == expected) {
        value = true;
        returned = 23;
    } else if (id == 0) {
        value = false;
        returned = 30;
    } else {
        mc_registry_block_m_view array;
        mc_cv_pc(c, 31);
        if (!registry_result(c, mc_registry_block_m_get(c->registry, &array, &c->failure.registry),
                             31, "Redstone connectivity captured Block.m"))
            return false;
        const mc_block *block = NULL;
        mc_cv_pc(c, 36);
        if (!registry_result(c, mc_registry_block_m_at(&array, id, &block, &c->failure.registry),
                             36, "Redstone connectivity Block.m element") ||
            !capability(c, block, &value))
            return false;
        returned = 44;
        if (!value) {
            if (!static_id(c, "bi", 47, 50, &expected))
                return false;
            bool diode = id == expected;
            if (!diode) {
                if (!static_id(c, "bj", 58, 61, &expected))
                    return false;
                diode = id == expected;
            }
            returned = 98;
            if (diode) {
                int32_t metadata;
                if (!mc_cv_metadata_bound(c, receiver, x, y, z, 71, &metadata))
                    return false;
                mc_registry_horizontal_view orientation;
                mc_cv_pc(c, 80);
                if (!registry_result(c,
                                     mc_registry_horizontal_b_get(c->registry, &orientation,
                                                                  &c->failure.registry),
                                     80, "Redstone connectivity actual JJ.b active use"))
                    return false;
                mc_cv_pc(c, 87);
                if (!registry_result(c,
                                     mc_registry_horizontal_at(&orientation, metadata & 3,
                                                               &expected, &c->failure.registry),
                                     87, "Redstone connectivity captured JJ.b element"))
                    return false;
                value = side == expected;
                returned = 96;
            }
        }
    }
    mc_cv_pc(c, returned);
    mc_cv_pop(c);
    *out = value;
    return true;
}

typedef struct redstone_direction {
    int32_t dx, dz, side;
    unsigned int initial, initial_solid, lower, upper_solid, upper;
    unsigned int wall_solid, wall_id, wall_static, wall_field, color, gray;
    unsigned int vertices[8];
} redstone_direction;

static const redstone_direction directions[4] = {
    {-1,
     0,
     1,
     219,
     235,
     256,
     480,
     501,
     1562,
     1582,
     1587,
     1590,
     1613,
     1728,
     {1641, 1665, 1689, 1717, 1760, 1788, 1816, 1848}},
    {1,
     0,
     3,
     280,
     296,
     317,
     520,
     541,
     1861,
     1881,
     1886,
     1889,
     1912,
     2035,
     {1938, 1968, 1998, 2024, 2065, 2099, 2133, 2163}},
    {0,
     -1,
     2,
     341,
     357,
     378,
     560,
     581,
     2176,
     2196,
     2201,
     2204,
     2227,
     2342,
     {2251, 2279, 2307, 2331, 2370, 2402, 2434, 2462}},
    {0,
     1,
     0,
     402,
     418,
     439,
     600,
     621,
     2475,
     2495,
     2500,
     2503,
     2526,
     2649,
     {2556, 2582, 2608, 2638, 2683, 2713, 2743, 2777}}};

typedef struct redstone_vertex {
    double x, y, z, u, v;
} redstone_vertex;

static bool quad(mc_cv_context *c, const redstone_vertex vertices[4], const unsigned int pc[4],
                 bool overlay) {
    for (size_t i = 0; i < 4; ++i)
        if (!mc_cv_vertex(c, vertices[i].x, vertices[i].y, vertices[i].z, vertices[i].u,
                          overlay ? at_add(vertices[i].v, .0625) : vertices[i].v, pc[i]))
            return false;
    return true;
}

static void uv(int32_t tx, int32_t ty, double *u0, double *u1, double *v0, double *v1) {
    *u0 = at_widen(at_fdiv((float)tx, 256.0f));
    *u1 = at_widen(at_fdiv(at_fadd((float)tx, 15.99f), 256.0f));
    *v0 = at_widen(at_fdiv((float)ty, 256.0f));
    *v1 = at_widen(at_fdiv(at_fadd((float)ty, 15.99f), 256.0f));
}

/* Actual CV.g(Block,III). The caller owns g's frame and the current NW batch.
 * Connection flags affect the floor only; the later upright gates read their
 * support, upper ID and live singleton again in source order. */
bool mc_cv_redstone(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                    bool *out) {
    if (!mc_cv_nw(c, 0))
        return false;
    int32_t metadata, texture;
    if (!mc_cv_metadata(c, x, y, z, 13, &metadata))
        return false;
    mc_cv_pc(c, 24);
    if (block == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "actual Redstone texture receiver", 24, false);
    const mc_registry_status texture_status =
        mc_block_render_texture_metadata(block, 1, metadata, &texture);
    if (!registry_result(c, texture_status, 24, "actual Redstone metadata texture virtual"))
        return false;
    if (c->fields->d >= 0)
        texture = c->fields->d;
    float brightness;
    if (!mc_cv_brightness(c, block, x, y, z, 51, &brightness) || !mc_cv_fp(c, 58))
        return false;
    const float power = at_fdiv((float)metadata, 15.0f);
    float red = at_fadd(at_fmul(power, .6f), .4f);
    if (metadata == 0)
        red = .3f;
    float green = at_fsub(at_fmul(at_fmul(power, power), .7f), .5f);
    float blue = at_fsub(at_fmul(at_fmul(power, power), .6f), .7f);
    if (green < 0)
        green = 0;
    if (blue < 0)
        blue = 0;
    if (!mc_cv_color(c, at_fmul(brightness, red), at_fmul(brightness, green),
                     at_fmul(brightness, blue), 146) ||
        !mc_cv_fp(c, 168))
        return false;
    const int32_t tx = (texture & 15) << 4, ty = texture & 240;
    double u0, u1, v0, v1;
    uv(tx, ty, &u0, &u1, &v0, &v1);
    bool flags[4];
    const int32_t below = mc_java_i32_sub(y, 1), above = mc_java_i32_add(y, 1);
    for (size_t i = 0; i < 4; ++i) {
        const redstone_direction *d = &directions[i];
        const int32_t nx = mc_java_i32_add(x, d->dx), nz = mc_java_i32_add(z, d->dz);
        if (!connected(c, nx, y, nz, d->side, d->initial, &flags[i]))
            return false;
        if (!flags[i]) {
            bool solid;
            if (!mc_cv_solid(c, nx, y, nz, d->initial_solid, &solid))
                return false;
            if (!solid && !connected(c, nx, below, nz, -1, d->lower, &flags[i]))
                return false;
        }
    }
    bool overhead;
    if (!mc_cv_solid(c, x, above, z, 462, &overhead))
        return false;
    if (!overhead) {
        for (size_t i = 0; i < 4; ++i) {
            const redstone_direction *d = &directions[i];
            const int32_t nx = mc_java_i32_add(x, d->dx), nz = mc_java_i32_add(z, d->dz);
            bool solid;
            if (!mc_cv_solid(c, nx, y, nz, d->upper_solid, &solid))
                return false;
            if (solid) {
                bool upper;
                if (!connected(c, nx, above, nz, -1, d->upper, &upper))
                    return false;
                if (upper)
                    flags[i] = true;
            }
        }
    }
    if (!mc_cv_fp(c, 633))
        return false;
    float x0 = (float)mc_java_i32_add(x, 0), x1 = (float)mc_java_i32_add(x, 1);
    float z0 = (float)mc_java_i32_add(z, 0), z1 = (float)mc_java_i32_add(z, 1);
    int shape = 0;
    if ((flags[0] || flags[1]) && !flags[2] && !flags[3])
        shape = 1;
    if ((flags[2] || flags[3]) && !flags[1] && !flags[0])
        shape = 2;
    if (shape != 0)
        uv(mc_java_i32_add(tx, 16), ty, &u0, &u1, &v0, &v1);
    else if (flags[0] || flags[1] || flags[2] || flags[3]) {
        if (!flags[0]) {
            x0 = at_fadd(x0, .3125f);
            u0 = at_add(u0, .01953125);
        }
        if (!flags[1]) {
            x1 = at_fsub(x1, .3125f);
            u1 = at_sub(u1, .01953125);
        }
        if (!flags[2]) {
            z0 = at_fadd(z0, .3125f);
            v0 = at_add(v0, .01953125);
        }
        if (!flags[3]) {
            z1 = at_fsub(z1, .3125f);
            v1 = at_sub(v1, .01953125);
        }
    }
    const double floor_y = at_widen(at_fadd((float)y, .015625f));
    const redstone_vertex floor_vertices[4] = {
        {at_widen(x1), floor_y, at_widen(z1), u1, v1},
        {at_widen(x1), floor_y, at_widen(z0), shape == 2 ? u0 : u1, shape == 2 ? v1 : v0},
        {at_widen(x0), floor_y, at_widen(z0), u0, v0},
        {at_widen(x0), floor_y, at_widen(z1), shape == 2 ? u1 : u0, shape == 2 ? v0 : v1}};
    static const unsigned int floor_pc[3][8] = {{901, 922, 943, 964, 1000, 1025, 1050, 1075},
                                                {1105, 1126, 1147, 1168, 1204, 1229, 1254, 1279},
                                                {1309, 1330, 1351, 1372, 1408, 1433, 1458, 1483}};
    static const unsigned int floor_gray_pc[3] = {975, 1179, 1383};
    if (!quad(c, floor_vertices, floor_pc[shape], false) ||
        !mc_cv_color(c, brightness, brightness, brightness, floor_gray_pc[shape]) ||
        !quad(c, floor_vertices, floor_pc[shape] + 4, true))
        return false;
    if (!mc_cv_solid(c, x, above, z, 1496, &overhead))
        return false;
    if (!overhead) {
        if (!mc_cv_fp(c, 1509))
            return false;
        uv(mc_java_i32_add(tx, 16), ty, &u0, &u1, &v0, &v1);
        for (size_t i = 0; i < 4; ++i) {
            const redstone_direction *d = &directions[i];
            const int32_t nx = mc_java_i32_add(x, d->dx), nz = mc_java_i32_add(z, d->dz);
            bool solid;
            if (!mc_cv_solid(c, nx, y, nz, d->wall_solid, &solid))
                return false;
            if (!solid)
                continue;
            int32_t id, dust;
            if (!mc_cv_block(c, nx, above, nz, d->wall_id, &id) ||
                !static_id(c, "aw", d->wall_static, d->wall_field, &dust))
                return false;
            if (id != dust)
                continue;
            if (!mc_cv_fp(c, d->color - 11) ||
                !mc_cv_color(c, at_fmul(brightness, red), at_fmul(brightness, green),
                             at_fmul(brightness, blue), d->color))
                return false;
            const double y0 = mc_java_i32_add(y, 0);
            const double y1 = at_widen(at_fadd((float)mc_java_i32_add(y, 1), .021875f));
            const double dx0 = mc_java_i32_add(x, 0), dx1 = mc_java_i32_add(x, 1);
            const double dz0 = mc_java_i32_add(z, 0), dz1 = mc_java_i32_add(z, 1);
            redstone_vertex wall[4];
            if (i == 0) {
                const double wx = at_widen(at_fadd((float)x, .015625f));
                const redstone_vertex values[4] = {{wx, y1, dz1, u1, v0},
                                                   {wx, y0, dz1, u0, v0},
                                                   {wx, y0, dz0, u0, v1},
                                                   {wx, y1, dz0, u1, v1}};
                for (size_t j = 0; j < 4; ++j)
                    wall[j] = values[j];
            } else if (i == 1) {
                const double ex = at_widen(at_fsub((float)mc_java_i32_add(x, 1), .015625f));
                const redstone_vertex values[4] = {{ex, y0, dz1, u0, v1},
                                                   {ex, y1, dz1, u1, v1},
                                                   {ex, y1, dz0, u1, v0},
                                                   {ex, y0, dz0, u0, v0}};
                for (size_t j = 0; j < 4; ++j)
                    wall[j] = values[j];
            } else if (i == 2) {
                const double nz0 = at_widen(at_fadd((float)z, .015625f));
                const redstone_vertex values[4] = {{dx1, y0, nz0, u0, v1},
                                                   {dx1, y1, nz0, u1, v1},
                                                   {dx0, y1, nz0, u1, v0},
                                                   {dx0, y0, nz0, u0, v0}};
                for (size_t j = 0; j < 4; ++j)
                    wall[j] = values[j];
            } else {
                const double sz = at_widen(at_fsub((float)mc_java_i32_add(z, 1), .015625f));
                const redstone_vertex values[4] = {{dx1, y1, sz, u1, v0},
                                                   {dx1, y0, sz, u0, v0},
                                                   {dx0, y0, sz, u0, v1},
                                                   {dx0, y1, sz, u1, v1}};
                for (size_t j = 0; j < 4; ++j)
                    wall[j] = values[j];
            }
            if (!quad(c, wall, d->vertices, false) ||
                !mc_cv_color(c, brightness, brightness, brightness, d->gray) ||
                !quad(c, wall, d->vertices + 4, true))
                return false;
        }
    }
    mc_cv_pc(c, 2781);
    *out = true;
    return true;
}
