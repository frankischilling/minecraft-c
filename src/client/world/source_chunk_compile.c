#include "source_chunk_compile_internal.h"

#include "../../entity/player/tick_math_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../application_state.h"
#include "../display/camera_surface.h"
#include "../render/blocks/internal.h"
#include "../render/model_graph_internal.h"
#include "../render_engine/chunk_statistics.h"
#include "../render_engine/internal.h"
#include "mc/block/registry/rendering.h"
#include "mc/block/registry/runtime.h"
#include "mc/client/world/chunk_cache.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mc_source_dk_compile {
    mc_client_chunk_compile_state state;
    mc_client_render_global_error failure;
    mc_client_chunk_cache *cache;
    mc_client_chunk_cache_error cache_error, cache_cleanup;
    mc_world_runtime_classes *world_classes;
    mc_cv_context render_blocks;
    mc_tile_entity **old_tiles;
    size_t old_count;
    mc_tile_entity *tile;
    int32_t open_list;
};

static bool fail(mc_source_dk_compile *c, mc_client_render_global_status status,
                 const char *operation, unsigned int pc, bool native) {
    c->state.source_pc = pc;
    c->state.chunk->source_pc = pc;
    mc_render_global_source_fail(&c->failure, status, operation, "dk", "a", "()V", pc, native);
    return false;
}

static bool gl(mc_source_dk_compile *c, bool ok, unsigned int pc) {
    return ok || fail(c,
                      c->failure.display.status == MC_CLIENT_DISPLAY_BUSY
                          ? MC_CLIENT_RENDER_GLOBAL_BUSY
                          : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                      c->failure.display.operation, pc, true);
}

static bool nw(mc_source_dk_compile *c, bool ok, unsigned int pc) {
    if (ok)
        return true;
    c->failure.child = c->failure.graphics.site;
    return fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.graphics.operation, pc,
                c->failure.graphics.native_policy);
}

static bool cache_result(mc_source_dk_compile *c, mc_client_chunk_cache_status status,
                         unsigned int pc) {
    c->state.cache = c->cache;
    if (status == MC_CLIENT_CHUNK_CACHE_OK)
        return true;
    c->state.cache_failure = &c->cache_error;
    c->failure.child = (mc_client_graphics_site){
        "ew", c->cache_error.method, c->cache_error.descriptor, (int32_t)c->cache_error.bytecode};
    return fail(c,
                status == MC_CLIENT_CHUNK_CACHE_SOURCE_NULL ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                : status == MC_CLIENT_CHUNK_CACHE_ALLOCATION_FAILED
                    ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                : status == MC_CLIENT_CHUNK_CACHE_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                                                        : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                c->cache_error.operation, pc, c->cache_error.native_policy);
}

static bool shape(mc_source_dk_compile *c, mc_client_render_list *list, unsigned int pc) {
    if (list == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual DK TileEntity List", pc,
                    false);
    if (list->count > list->capacity || list->capacity > INT32_MAX ||
        (list->capacity != 0 && list->entries == NULL))
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "DK TileEntity List native extent",
                    pc, true);
    return true;
}

static bool old_set(mc_source_dk_compile *c) {
    mc_client_render_list *b = c->state.chunk->fields.B;
    if (!shape(c, b, 106))
        return false;
    if (b->count > (8u * 1024u * 1024u) / sizeof(*c->old_tiles))
        return fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "DK transient TileEntity Set storage", 98,
                    true);
    if (b->count != 0) {
        c->old_tiles = calloc(b->count, sizeof(*c->old_tiles));
        if (c->old_tiles == NULL)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                        "DK transient TileEntity Set allocation", 98, true);
    }
    for (size_t i = 0; i < b->count; ++i) {
        mc_tile_entity *tile = b->entries[i];
        bool seen = false;
        for (size_t j = 0; j < c->old_count; ++j)
            if (c->old_tiles[j] == tile) {
                seen = true;
                break;
            }
        if (!seen) {
            if (tile != NULL && mc_tile_entity_retain(tile) != MC_TILE_ENTITY_OK)
                return fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "DK old TileEntity native occurrence",
                            106, true);
            c->old_tiles[c->old_count++] = tile;
        }
    }
    /* The source clear is after the old identities have been captured. */
    for (size_t i = 0; i < b->count; ++i)
        b->entries[i] = NULL;
    b->count = 0;
    return true;
}

static bool start_list(mc_source_dk_compile *c) {
    mc_client_source_dk *dk = c->state.chunk;
    mc_client_display *d = dk->owner->display;
    c->open_list = mc_java_i32_add(dk->fields.C, c->state.pass);
    if (!gl(c, mc_client_display_lists_begin(d, c->open_list, &c->failure.display), 261))
        return false;
    c->state.list_open = true;
    dk->owner->open_list = c->open_list;
    if (!gl(c, mc_client_display_source_push(d, &c->failure.display), 264))
        return false;
    c->state.matrix_pushed = true;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP, "DK list translation strict FP", 268,
                    true);
    if (!gl(c,
            mc_client_display_source_translate(d, (float)dk->fields.l, (float)dk->fields.m,
                                               (float)dk->fields.n, &c->failure.display),
            268)) {
        c->failure.child = (mc_client_graphics_site){"dk", "g", "()V", 15};
        return false;
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP, "DK negative-center strict FP", 280,
                    true);
    const float h = at_fdiv((float)mc_java_i32_sub(0, dk->fields.h), 2.0f);
    const float g = at_fdiv((float)mc_java_i32_sub(0, dk->fields.g), 2.0f);
    if (!gl(c, mc_client_display_source_translate(d, h, g, h, &c->failure.display), 299) ||
        !gl(c,
            mc_client_display_source_scale(d, (double)1.000001f, (double)1.000001f,
                                           (double)1.000001f, &c->failure.display),
            308))
        return false;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP, "DK positive-center strict FP", 315,
                    true);
    const float ph = at_fdiv((float)dk->fields.h, 2.0f);
    const float pg = at_fdiv((float)dk->fields.g, 2.0f);
    if (!gl(c, mc_client_display_source_translate(d, ph, pg, ph, &c->failure.display), 332))
        return false;
    mc_client_tessellator *t = dk->owner->classes->dk_D;
    if (!nw(c, mc_client_nw_begin_quads(t, &c->failure.graphics), 338))
        return false;
    c->state.tessellator_started = true;
    return nw(c,
              mc_client_nw_translation(t, (double)mc_java_i32_sub(0, dk->fields.c),
                                       (double)mc_java_i32_sub(0, dk->fields.d),
                                       (double)mc_java_i32_sub(0, dk->fields.e),
                                       &c->failure.graphics),
              362);
}

static bool block(mc_source_dk_compile *c) {
    mc_client_source_dk *dk = c->state.chunk;
    mc_registry_classes *registry = c->world_classes->registry;
    if (c->state.pass == 0) {
        if (mc_registry_classes_initialize_blocks(registry, &c->failure.registry) != MC_REGISTRY_OK)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.registry.message,
                        370, true);
        mc_registry_block_view view;
        if (mc_registry_classes_inspect_blocks(registry, &view, &c->failure.registry) !=
            MC_REGISTRY_OK)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "DK canonical uu.p storage", 370,
                        true);
        if (view.p == NULL)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual uu.p array", 375, false);
        if (c->state.block_id < 0 || c->state.block_id >= 256)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "actual uu.p index", 375, false);
        if (view.p[c->state.block_id]) {
            if (!cache_result(c,
                              mc_client_chunk_cache_tile_entity(c->cache, c->state.x, c->state.y,
                                                                c->state.z, &c->tile,
                                                                &c->cache_error),
                              387))
                return false;
            return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                        "actual TileEntity renderer eligibility child is not connected", 399, true);
        }
    }
    const mc_block *b = NULL;
    if (mc_registry_block_get(registry, c->state.block_id, &b, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.registry.message, 422,
                    true);
    if (b == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual Block render-pass receiver",
                    427, false);
    mc_block_render_description description;
    if (mc_block_render_describe(b, &description) != MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, "actual Block render-pass dispatch",
                    427, true);
    if (description.pass != c->state.pass) {
        c->state.needs_later_pass = true;
        return true;
    }
    bool rendered;
    if (!mc_cv_render(&c->render_blocks, b, c->state.x, c->state.y, c->state.z, &rendered)) {
        c->failure = c->render_blocks.failure;
        if (c->render_blocks.depth != 0) {
            const mc_cv_frame *f = &c->render_blocks.frames[0];
            c->failure.child =
                (mc_client_graphics_site){f->class_name, f->method, f->descriptor, (int32_t)f->pc};
        }
        if (c->render_blocks.cache_error.status != MC_CLIENT_CHUNK_CACHE_OK)
            c->state.cache_failure = &c->render_blocks.cache_error;
        return fail(c, c->failure.status, c->failure.operation, 464, c->failure.native_policy);
    }
    c->state.pass_rendered = c->state.pass_rendered | rendered;
    c->state.source_pc = dk->source_pc = 467;
    return true;
}

static bool finish_empty(mc_source_dk_compile *c) {
    mc_client_source_dk *dk = c->state.chunk;
    /* The real shared F calls still dereference B after geometry. TileEntity
     * renderer dispatch remains a separate child from successful CV faces. */
    if (!shape(c, dk->fields.B, 562) || !shape(c, dk->fields.F, 584))
        return false;
    if (dk->fields.B->count != 0)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                    "actual new TileEntity HashSet iteration", 584, true);
    mc_client_render_list *f = dk->fields.F;
    size_t kept = 0;
    for (size_t i = 0; i < f->count; ++i) {
        bool remove = false;
        for (size_t j = 0; j < c->old_count; ++j)
            if (f->entries[i] == c->old_tiles[j]) {
                remove = true;
                break;
            }
        if (!remove)
            f->entries[kept++] = f->entries[i];
    }
    for (size_t i = kept; i < f->count; ++i)
        f->entries[i] = NULL;
    f->count = kept;
    dk->fields.A = c->world_classes->chunk_a;
    dk->last_store_pc = c->state.last_store_pc = 618;
    dk->fields.E = true;
    dk->last_store_pc = c->state.last_store_pc = 623;
    dk->source_pc = c->state.source_pc = 626;
    c->state.returned = true;
    return true;
}

static bool body(mc_source_dk_compile *c) {
    mc_client_source_dk *dk = c->state.chunk;
    if (!dk->fields.u) {
        dk->source_pc = c->state.source_pc = 7;
        c->state.returned = true;
        return true;
    }
    if (!nw(c,
            mc_client_chunk_statistics_increment(dk->owner->classes,
                                                 (mc_client_graphics_site){"dk", "a", "()V", 8},
                                                 &c->failure.graphics),
            8))
        return false;
    const int32_t x = dk->fields.c, y = dk->fields.d, z = dk->fields.e;
    const int32_t end_x = mc_java_i32_add(x, dk->fields.f);
    const int32_t end_y = mc_java_i32_add(y, dk->fields.g);
    const int32_t end_z = mc_java_i32_add(z, dk->fields.h);
    if (dk->fields.p == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual DK compile pass array", 80,
                    false);
    dk->fields.p[0] = dk->fields.p[1] = true;
    dk->last_store_pc = c->state.last_store_pc = 80;
    c->world_classes = dk->fields.a == NULL ? NULL : dk->fields.a->classes;
    client_application *app = dk->owner->fields.t;
    if (c->world_classes == NULL && app != NULL)
        c->world_classes = app->world_classes;
    if (c->world_classes == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                    "actual shared Chunk class namespace", 88, true);
    c->world_classes->chunk_a = false;
    if (!old_set(c))
        return false;
    if (!cache_result(
            c,
            mc_client_chunk_cache_create(dk->fields.a, mc_java_i32_sub(x, 1), mc_java_i32_sub(y, 1),
                                         mc_java_i32_sub(z, 1), mc_java_i32_add(end_x, 1),
                                         mc_java_i32_add(end_y, 1), mc_java_i32_add(end_z, 1),
                                         &c->cache, &c->cache_error),
            159))
        return false;
    if (app == NULL || app->render_classes == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                    "actual RenderBlocks class namespace", 164, true);
    if (!mc_client_render_blocks_active_use(app->render_classes, &c->failure.model))
        return fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, "actual RenderBlocks active use",
                    164, true);
    mc_client_render_blocks_construct_fields(&c->state.renderer, c->cache, true);
    c->render_blocks = (mc_cv_context){.fields = &c->state.renderer,
                                       .cache = c->cache,
                                       .registry = c->world_classes->registry,
                                       .models = app->render_classes,
                                       .graphics = dk->owner->classes,
                                       .application = app};
    c->state.render_blocks = &c->render_blocks;
    for (c->state.pass = 0; c->state.pass < 2; ++c->state.pass) {
        c->state.needs_later_pass = false;
        c->state.pass_rendered = false;
        bool started = false;
        for (c->state.y = y; c->state.y < end_y; c->state.y = mc_java_i32_add(c->state.y, 1))
            for (c->state.z = z; c->state.z < end_z; c->state.z = mc_java_i32_add(c->state.z, 1))
                for (c->state.x = x; c->state.x < end_x;
                     c->state.x = mc_java_i32_add(c->state.x, 1)) {
                    if (!cache_result(c,
                                      mc_client_chunk_cache_block(c->cache, c->state.x, c->state.y,
                                                                  c->state.z, &c->state.block_id,
                                                                  &c->cache_error),
                                      231))
                        return false;
                    if (c->state.block_id <= 0)
                        continue;
                    if (!started) {
                        started = true;
                        if (!start_list(c))
                            return false;
                    }
                    if (!block(c))
                        return false;
                }
        if (started) {
            mc_client_display *d = dk->owner->display;
            mc_client_tessellator *t = dk->owner->classes->dk_D;
            if (!nw(c, mc_client_nw_draw(t, d, &c->failure.graphics), 496))
                return false;
            c->state.tessellator_started = false;
            if (!gl(c, mc_client_display_source_pop(d, &c->failure.display), 499))
                return false;
            c->state.matrix_pushed = false;
            if (!gl(c, mc_client_display_lists_end(d, &c->failure.display), 502))
                return false;
            c->state.list_open = false;
            dk->owner->open_list = 0;
            if (!nw(c, mc_client_nw_translation(t, 0, 0, 0, &c->failure.graphics), 511))
                return false;
        }
        if (c->state.pass_rendered) {
            dk->fields.p[c->state.pass] = false;
            dk->last_store_pc = c->state.last_store_pc = 532;
        }
        if (!c->state.needs_later_pass)
            break;
    }
    return finish_empty(c);
}

bool mc_source_dk_compile_entered(mc_client_source_dk *dk, mc_source_dk_compile **out,
                                  mc_client_render_global_error *error) {
    if (dk == NULL || out == NULL || *out != NULL || !dk->owner->active) {
        mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                     "DK compile borrowed N operation", NULL, NULL, NULL, 0, true);
        return false;
    }
    mc_source_dk_compile *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                                     "DK compile native call allocation", NULL, NULL, NULL, 0,
                                     true);
        return false;
    }
    c->state.chunk = dk;
    c->state.failure = &c->failure;
    c->state.cache_cleanup = &c->cache_cleanup;
    *out = c;
    c->state.active = true;
    const bool ok = body(c);
    c->state.active = false;
    *error = c->failure;
    return ok;
}

bool mc_source_dk_compile_inspect(const mc_source_dk_compile *c,
                                  mc_client_chunk_compile_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

mc_client_render_global_status mc_source_dk_compile_retire(mc_source_dk_compile **slot,
                                                           mc_client_render_global_error *error) {
    *error = (mc_client_render_global_error){0};
    if (slot == NULL)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                            "DK compile native retirement slot", NULL, NULL, NULL,
                                            0, true);
    mc_source_dk_compile *c = *slot;
    if (c == NULL)
        return MC_CLIENT_RENDER_GLOBAL_OK;
    if (c->state.active)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "DK compile active native retirement", NULL, NULL, NULL,
                                            0, true);
    if (c->state.list_open) {
        if (!mc_client_display_lists_abort(c->state.chunk->owner->display, c->open_list,
                                           &error->cleanup_display))
            return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                                error->cleanup_display.operation, NULL, NULL, NULL,
                                                0, true);
        c->state.list_open = false;
        c->state.chunk->owner->open_list = 0;
    }
    if (mc_client_chunk_cache_retire(&c->cache, &c->cache_cleanup) != MC_CLIENT_CHUNK_CACHE_OK)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                            c->cache_cleanup.operation, NULL, NULL, NULL, 0, true);
    if (c->tile != NULL)
        mc_tile_entity_release(c->tile);
    for (size_t i = 0; i < c->old_count; ++i)
        if (c->old_tiles[i] != NULL)
            mc_tile_entity_release(c->old_tiles[i]);
    free(c->old_tiles);
    free(c);
    *slot = NULL;
    return MC_CLIENT_RENDER_GLOBAL_OK;
}
