#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"
#include "../../../world/runtime/chunk_cache_internal.h"
#include "../../render_engine/internal.h"
#include "mc/block/registry/runtime.h"

#include <string.h>
#include <xmmintrin.h>

void mc_cv_pc(mc_cv_context *c, unsigned int pc) {
    c->source_pc = pc;
    if (c->depth != 0)
        c->frames[c->depth - 1].pc = pc;
}

bool mc_cv_fail(mc_cv_context *c, mc_client_render_global_status status, const char *operation,
                unsigned int pc, bool native) {
    if (c->failed)
        return false;
    mc_cv_pc(c, pc);
    const mc_cv_frame *f = c->depth == 0 ? NULL : &c->frames[c->depth - 1];
    mc_render_global_source_fail(&c->failure, status, operation, f == NULL ? "cv" : f->class_name,
                                 f == NULL ? NULL : f->method, f == NULL ? NULL : f->descriptor, pc,
                                 native);
    c->failed = true;
    return false;
}

bool mc_cv_push_source(mc_cv_context *c, const char *class_name, const char *method,
                       const char *descriptor) {
    if (c->failed)
        return false;
    if (c->depth == sizeof(c->frames) / sizeof(c->frames[0]))
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "CV native source frame extent",
                          c->source_pc, true);
    c->frames[c->depth++] = (mc_cv_frame){class_name, method, descriptor, 0};
    return true;
}

bool mc_cv_push(mc_cv_context *c, const char *method, const char *descriptor) {
    return mc_cv_push_source(c, "cv", method, descriptor);
}

void mc_cv_pop(mc_cv_context *c) {
    if (!c->failed && c->depth != 0)
        --c->depth;
}

void mc_cv_store(mc_cv_context *c, unsigned int pc) {
    mc_cv_pc(c, pc);
    c->last_store_pc = pc;
}

bool mc_cv_fp(mc_cv_context *c, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
                      "CV reached strict floating-point prerequisite", pc, true);
}

static bool graphics_result(mc_cv_context *c, bool ok, unsigned int pc) {
    if (ok)
        return true;
    const mc_client_graphics_error *e = &c->failure.graphics;
    c->failure.child = e->site;
    return mc_cv_fail(
        c,
        e->status == MC_CLIENT_GRAPHICS_NULL_REFERENCE   ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
        : e->status == MC_CLIENT_GRAPHICS_BUSY           ? MC_CLIENT_RENDER_GLOBAL_BUSY
        : e->status == MC_CLIENT_GRAPHICS_LIMIT          ? MC_CLIENT_RENDER_GLOBAL_LIMIT
        : e->status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
        : e->status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
            ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
            : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
        e->operation, pc, e->native_policy);
}

bool mc_cv_nw(mc_cv_context *c, unsigned int pc) {
    mc_cv_pc(c, pc);
    const mc_cv_frame *f = c->depth == 0 ? NULL : &c->frames[c->depth - 1];
    return graphics_result(
        c,
        mc_client_nw_active_use(c->graphics,
                                (mc_client_graphics_site){"cv", f == NULL ? NULL : f->method,
                                                          f == NULL ? NULL : f->descriptor,
                                                          (int32_t)pc},
                                &c->tessellator, &c->failure.graphics),
        pc);
}

bool mc_cv_color(mc_cv_context *c, float red, float green, float blue, unsigned int pc) {
    mc_cv_pc(c, pc);
    return graphics_result(
        c, mc_client_nw_color_float(c->tessellator, red, green, blue, &c->failure.graphics), pc);
}

bool mc_cv_translation_add(mc_cv_context *c, float x, float y, float z, unsigned int pc) {
    mc_cv_pc(c, pc);
    return graphics_result(
        c, mc_client_nw_translation_add(c->tessellator, x, y, z, &c->failure.graphics), pc);
}

bool mc_cv_vertex(mc_cv_context *c, double x, double y, double z, double u, double v,
                  unsigned int pc) {
    mc_cv_pc(c, pc);
    return graphics_result(
        c, mc_client_nw_vertex_uv(c->tessellator, x, y, z, u, v, &c->failure.graphics), pc);
}

bool mc_cv_fields(mc_cv_context *c, const mc_block *block, unsigned int pc,
                  const mc_block_fields **out) {
    if (block == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual Block field receiver",
                          pc, false);
    *out = mc_block_fields_get(block);
    return *out != NULL || mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                                      "actual native Block storage", pc, true);
}

static bool cache_result(mc_cv_context *c, mc_client_chunk_cache_status status, unsigned int pc) {
    if (status == MC_CLIENT_CHUNK_CACHE_OK)
        return true;
    c->failure.child = (mc_client_graphics_site){
        "ew", c->cache_error.method, c->cache_error.descriptor, (int32_t)c->cache_error.bytecode};
    return mc_cv_fail(c,
                      status == MC_CLIENT_CHUNK_CACHE_SOURCE_NULL
                          ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                      : status == MC_CLIENT_CHUNK_CACHE_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                      : status == MC_CLIENT_CHUNK_CACHE_ALLOCATION_FAILED
                          ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                          : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                      c->cache_error.operation, pc, c->cache_error.native_policy);
}

/* c is the live source XP reference. Each call dispatches on the exact bound
 * EW or World identity. A changed/foreign reference never reads an old owner. */
static bool xp(mc_cv_context *c, unsigned int pc) {
    if (c->fields->c == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "actual CV IBlockAccess receiver", pc, false);
    return (c->cache != NULL && c->fields->c == c->cache) ||
           (c->world != NULL && c->fields->c == c->world) ||
           mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                      "actual CV IBlockAccess implementation", pc, true);
}

static bool world_access(const mc_cv_context *c) {
    return c->world != NULL && c->fields->c == c->world;
}

static void world_child(mc_cv_context *c, mc_cv_world_child_kind kind) {
    memset(&c->world_error, 0, sizeof(c->world_error));
    c->world_error.kind = kind;
}

static bool world_result(mc_cv_context *c, mc_world_storage_status status, const char *operation,
                         const char *method, const char *descriptor, unsigned int child_pc,
                         unsigned int pc, bool native, mc_world_storage_error cause) {
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    c->world_error.failed = true;
    c->failure.world = cause;
    c->failure.child = (mc_client_graphics_site){"fd", method, descriptor, (int32_t)child_pc};
    bool source_null = false;
    switch (c->world_error.kind) {
        case MC_CV_WORLD_BRIGHTNESS:
        case MC_CV_WORLD_MIN_BRIGHTNESS:
            source_null = c->world_error.value.brightness.source_exception ==
                              MC_WORLD_BRIGHTNESS_SOURCE_NULL_DIMENSION ||
                          c->world_error.value.brightness.source_exception ==
                              MC_WORLD_BRIGHTNESS_SOURCE_NULL_TABLE;
            break;
        case MC_CV_WORLD_MATERIAL:
            source_null =
                c->world_error.value.material.registry.status == MC_REGISTRY_NULL_REFERENCE;
            break;
        case MC_CV_WORLD_OPAQUE:
            source_null = c->world_error.value.opaque.registry.status == MC_REGISTRY_NULL_REFERENCE;
            break;
        case MC_CV_WORLD_MANAGER:
            /* This bounded getter's only source failure is its null Dimension. */
            source_null = !c->world_error.value.manager.native_policy;
            break;
        default:
            break;
    }
    return mc_cv_fail(c,
                      status == MC_WORLD_STORAGE_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                      : status == MC_WORLD_STORAGE_ALLOCATION_FAILED
                          ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                      : status == MC_WORLD_STORAGE_BUSY ? MC_CLIENT_RENDER_GLOBAL_BUSY
                      : source_null                     ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                                        : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                      operation, pc, native);
}

bool mc_cv_block(mc_cv_context *c, int32_t x, int32_t y, int32_t z, unsigned int pc, int32_t *out) {
    mc_cv_pc(c, pc);
    if (!xp(c, pc))
        return false;
    if (world_access(c)) {
        world_child(c, MC_CV_WORLD_BLOCK);
        mc_world_storage_error *e = &c->world_error.value.query.cause;
        const mc_world_storage_status status = mc_world_runtime_block_get_detailed(
            c->world, x, y, z, out, e, &c->world_error.value.query.acquisition);
        return world_result(c, status, e->operation, "a", "(III)I", 0, pc, true, *e);
    }
    return cache_result(
        c, mc_client_chunk_cache_block_failure(c->cache, x, y, z, out, &c->cache_error), pc);
}

bool mc_cv_metadata(mc_cv_context *c, int32_t x, int32_t y, int32_t z, unsigned int pc,
                    int32_t *out) {
    mc_cv_pc(c, pc);
    if (!xp(c, pc))
        return false;
    if (world_access(c)) {
        world_child(c, MC_CV_WORLD_METADATA);
        mc_world_storage_error *e = &c->world_error.value.query.cause;
        const mc_world_storage_status status = mc_world_runtime_metadata_get_detailed(
            c->world, x, y, z, out, e, &c->world_error.value.query.acquisition);
        return world_result(c, status, e->operation, "e", "(III)I", 0, pc, true, *e);
    }
    return cache_result(
        c, mc_client_chunk_cache_metadata_failure(c->cache, x, y, z, out, &c->cache_error), pc);
}

bool mc_cv_material(mc_cv_context *c, int32_t x, int32_t y, int32_t z, unsigned int pc,
                    mc_material **out) {
    mc_cv_pc(c, pc);
    if (!xp(c, pc))
        return false;
    if (world_access(c)) {
        world_child(c, MC_CV_WORLD_MATERIAL);
        mc_world_material_error *e = &c->world_error.value.material;
        const mc_world_storage_status status =
            mc_world_runtime_material_get_detailed(c->world, x, y, z, out, e);
        c->failure.registry = e->registry;
        const bool native = e->registry.status != MC_REGISTRY_NULL_REFERENCE &&
                            e->registry.status != MC_REGISTRY_INDEX_OUT_OF_RANGE;
        return world_result(c, status, e->operation, "f", "(III)Lln;", e->bytecode, pc, native,
                            e->cause);
    }
    return cache_result(c, mc_client_chunk_cache_material(c->cache, x, y, z, out, &c->cache_error),
                        pc);
}

bool mc_cv_manager(mc_cv_context *c, unsigned int pc, mc_world_chunk_manager **out) {
    mc_cv_pc(c, pc);
    if (!xp(c, pc))
        return false;
    if (world_access(c)) {
        world_child(c, MC_CV_WORLD_MANAGER);
        mc_world_manager_error *e = &c->world_error.value.manager;
        const mc_world_storage_status status = mc_world_runtime_chunk_manager(c->world, out, e);
        return world_result(c, status, e->operation, "a", "()Lxv;", e->bytecode, pc,
                            e->native_policy, e->cause);
    }
    return cache_result(c, mc_client_chunk_cache_manager(c->cache, out, &c->cache_error), pc);
}

bool mc_cv_cache_brightness(mc_cv_context *c, int32_t x, int32_t y, int32_t z, unsigned int pc,
                            float *out) {
    mc_cv_pc(c, pc);
    if (!xp(c, pc))
        return false;
    if (world_access(c)) {
        world_child(c, MC_CV_WORLD_BRIGHTNESS);
        mc_world_brightness_error *e = &c->world_error.value.brightness;
        const mc_world_storage_status status =
            mc_world_runtime_brightness(c->world, x, y, z, out, e);
        return world_result(
            c, status, e->operation, "c", "(III)F", e->bytecode, pc,
            e->source_exception == MC_WORLD_BRIGHTNESS_SOURCE_NONE || e->native_policy, e->cause);
    }
    return cache_result(
        c, mc_client_chunk_cache_brightness(c->cache, x, y, z, out, &c->cache_error), pc);
}

bool mc_cv_min_brightness(mc_cv_context *c, int32_t x, int32_t y, int32_t z, int32_t minimum,
                          unsigned int pc, float *out) {
    mc_cv_pc(c, pc);
    if (!xp(c, pc))
        return false;
    if (world_access(c)) {
        world_child(c, MC_CV_WORLD_MIN_BRIGHTNESS);
        mc_world_brightness_error *e = &c->world_error.value.brightness;
        const mc_world_storage_status status =
            mc_world_runtime_min_brightness(c->world, x, y, z, minimum, out, e);
        return world_result(
            c, status, e->operation, "a", "(IIII)F", e->bytecode, pc,
            e->source_exception == MC_WORLD_BRIGHTNESS_SOURCE_NONE || e->native_policy, e->cause);
    }
    return cache_result(
        c, mc_client_chunk_cache_min_brightness(c->cache, x, y, z, minimum, out, &c->cache_error),
        pc);
}

bool mc_cv_opaque(mc_cv_context *c, int32_t x, int32_t y, int32_t z, unsigned int pc, bool *out) {
    mc_cv_pc(c, pc);
    if (!xp(c, pc))
        return false;
    if (world_access(c)) {
        world_child(c, MC_CV_WORLD_OPAQUE);
        mc_world_opaque_error *e = &c->world_error.value.opaque;
        const mc_world_storage_status status =
            mc_world_runtime_opaque_get(c->world, x, y, z, out, e);
        c->failure.registry = e->registry;
        return world_result(c, status, e->operation, "g", "(III)Z", e->bytecode, pc,
                            e->native_policy, e->cause);
    }
    return cache_result(c, mc_client_chunk_cache_opaque(c->cache, x, y, z, out, &c->cache_error),
                        pc);
}

bool mc_cv_registry(mc_cv_context *c, unsigned int pc, mc_registry_block_view *view) {
    if (mc_registry_classes_initialize_blocks(c->registry, &c->failure.registry) != MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.registry.message,
                          pc, true);
    if (mc_registry_classes_inspect_blocks(c->registry, view, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "actual CV Block class storage",
                          pc, true);
    return true;
}

bool mc_cv_grass(mc_cv_context *c, unsigned int pc, const mc_block **out) {
    if (mc_registry_block_static_get(c->registry, "v", out, &c->failure.registry) != MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.registry.message,
                          pc, true);
    return true;
}

bool mc_cv_ao_flag(mc_cv_context *c, int32_t x, int32_t y, int32_t z, unsigned int static_pc,
                   unsigned int query_pc, unsigned int index_pc, bool *out) {
    mc_registry_block_view view;
    if (!mc_cv_registry(c, static_pc, &view))
        return false;
    /* Java captures this exact array before evaluating the xp.a argument. */
    const bool *r = view.r;
    int32_t id;
    if (!mc_cv_block(c, x, y, z, query_pc, &id))
        return false;
    if (r == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual uu.r array", index_pc,
                          false);
    if (id < 0 || id >= 256) {
        c->failure.registry =
            (mc_registry_error){.status = MC_REGISTRY_INDEX_OUT_OF_RANGE, .block_id = id};
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, "actual uu.r array index",
                          index_pc, false);
    }
    *out = r[id];
    return true;
}

bool mc_cv_face(mc_cv_context *c, int32_t side, const mc_block *block, double x, double y, double z,
                int32_t texture, unsigned int pc) {
    static const char *const methods[] = {"a", "b", "c", "d", "e", "f"};
    mc_cv_pc(c, pc);
    if (side < 0 || side >= 6)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT, "CV native face selector",
                          pc, true);
    if (!mc_cv_push(c, methods[side], "(Luu;DDDI)V") ||
        !mc_cv_face_body(c, side, block, x, y, z, texture))
        return false;
    mc_cv_pop(c);
    return true;
}
