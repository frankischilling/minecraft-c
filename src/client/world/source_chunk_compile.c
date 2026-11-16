#include "source_chunk_compile_internal.h"

#include "../../entity/player/tick_math_internal.h"
#include "../../world/runtime/chunk_cache_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../application_state.h"
#include "../display/camera_surface.h"
#include "../render/blocks/internal.h"
#include "../render/model_graph_internal.h"
#include "../render/tile_entity_renderer/internal.h"
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
    mc_tile_entity **new_tiles;
    size_t new_count;
    mc_tile_entity *tile;
    mc_client_tile_renderer_dispatcher *ll;
    mc_client_tile_renderer_error tiles, tiles_cleanup;
    int32_t open_list;
};

static bool shape(mc_source_dk_compile *, mc_client_render_list *, unsigned int);
static bool fail(mc_source_dk_compile *, mc_client_render_global_status, const char *, unsigned int,
                 bool);

static mc_source_dk_tile_hold *b_occurrence(mc_client_source_dk *dk, mc_client_render_list *list,
                                            size_t index) {
    for (mc_source_dk_tile_hold *h = dk->tile_holds; h != NULL; h = h->next)
        if (h->tile != NULL && h->list == list && h->index == index)
            return h;
    return NULL;
}

static bool tile_list(mc_source_dk_compile *c, mc_client_render_list *list,
                      mc_client_render_list *origin, unsigned int pc) {
    if (list != NULL && list != origin)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                    "DK TileEntity List actual object ownership", pc, true);
    return shape(c, list, pc);
}

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

static bool tile_failure(mc_source_dk_compile *c, unsigned int pc) {
    mc_client_render_global_status status;
    switch (c->tiles.status) {
        case MC_CLIENT_TILE_RENDERER_BUSY:
            status = MC_CLIENT_RENDER_GLOBAL_BUSY;
            break;
        case MC_CLIENT_TILE_RENDERER_LIMIT:
            status = MC_CLIENT_RENDER_GLOBAL_LIMIT;
            break;
        case MC_CLIENT_TILE_RENDERER_ALLOCATION_FAILED:
            status = MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED;
            break;
        case MC_CLIENT_TILE_RENDERER_NULL_REFERENCE:
            status = MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE;
            break;
        case MC_CLIENT_TILE_RENDERER_UNSUPPORTED_FP:
        case MC_CLIENT_TILE_RENDERER_UNSUPPORTED_NUMERIC:
            status = MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP;
            break;
        default:
            status = MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED;
            break;
    }
    (void)fail(c, status, c->tiles.operation, pc, c->tiles.native_policy);
    c->failure.tile_renderer = &c->tiles;
    c->failure.child = (mc_client_graphics_site){c->tiles.source_class, c->tiles.source_method,
                                                 c->tiles.source_descriptor, c->tiles.pc};
    return false;
}

static bool tile_eligibility(mc_source_dk_compile *c) {
    mc_client_source_dk *dk = c->state.chunk;
    mc_client_render_global *n = dk->owner;
    client_application *app = n->fields.t;
    c->state.tile_eligibility_returned = false;
    c->state.tile_eligible = false;
    if (mc_client_tile_renderer_dispatcher_release(&c->ll, &c->tiles_cleanup) !=
        MC_CLIENT_TILE_RENDERER_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED, c->tiles_cleanup.operation, 394,
                    true);
    c->state.source_pc = dk->source_pc = 394;
    if (app == NULL || app->render_classes == NULL || app->profile == NULL ||
        c->world_classes == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                    "DK reached actual LL defining namespace", 394, true);
    mc_client_tile_renderer_classes *classes = NULL;
    if (mc_client_tile_entity_renderer_classes_existing(
            app->render_classes, c->world_classes->registry, c->world_classes, &classes,
            &c->tiles) != MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 394);
    if (classes == NULL && mc_client_tile_renderer_classes_bind(
                               app->render_classes, c->world_classes, app->profile, NULL, &classes,
                               &c->tiles) != MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 394);
    if (!n->tile_consumer.attached) {
        if (mc_client_tile_renderer_consumer_attach(classes, &n->tile_consumer, &c->tiles) !=
            MC_CLIENT_TILE_RENDERER_OK)
            return tile_failure(c, 394);
    } else if (n->tile_consumer.classes != classes)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                    "DK actual N LL consumer association changed", 394, true);
    mc_client_tile_renderer_dispatcher *receiver = NULL;
    if (mc_client_tile_renderer_static_get(n->tile_consumer.classes, &receiver, &c->tiles) !=
        MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 394);
    if (receiver != NULL && mc_client_tile_renderer_dispatcher_retain(receiver, &c->tiles) !=
                                MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 394);
    c->ll = receiver; /* Captured fresh actual getstatic, including NULL. */
    ++c->state.tile_eligibility_calls;
    if (receiver == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                    "DK actual null LL eligibility invocation receiver", 399, false);
    bool eligible;
    if (mc_client_tile_renderer_has_renderer(receiver, c->tile, &eligible, &c->tiles) !=
        MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 399);
    c->state.tile_eligibility_returned = true;
    c->state.tile_eligible = eligible;
    if (eligible) {
        if (!tile_list(c, dk->fields.B, dk->tile_list_origin, 411))
            return false;
        const mc_client_graphics_site at = {"dk", "a", "()V", 411};
        mc_source_dk_tile_hold *hold = dk->tile_holds;
        while (hold != NULL && hold->tile != NULL)
            hold = hold->next;
        if (hold == NULL) {
            hold = mc_render_global_allocate(n, sizeof(*hold), at, &c->failure);
            if (hold == NULL)
                return fail(c, c->failure.status, c->failure.operation, 411, true);
            hold->next = dk->tile_holds;
            dk->tile_holds = hold;
        }
        if (c->tile != NULL && mc_tile_entity_retain(c->tile) != MC_TILE_ENTITY_OK)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "DK B native Tile occurrence", 411, true);
        const size_t index = dk->fields.B->count;
        if (!mc_render_global_pointer_append(n, dk->fields.B, c->tile, at, &c->failure)) {
            mc_tile_entity_release(c->tile);
            return fail(c, c->failure.status, c->failure.operation, 411, c->failure.native_policy);
        }
        hold->tile = c->tile;
        hold->list = dk->fields.B;
        hold->index = index;
    }
    c->state.source_pc = dk->source_pc = 417;
    return true;
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
    if (!tile_list(c, b, c->state.chunk->tile_list_origin, 106))
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
        /* HashSet accepts the actual NULL reference without a Tile object or
         * native hold. Every non-NULL object still needs its exact row proof. */
        if (tile != NULL) {
            mc_source_dk_tile_hold *h = b_occurrence(c->state.chunk, b, i);
            if (h == NULL || h->tile != tile)
                return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                            "DK old Set actual Tile occurrence ownership", 106, true);
        }
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
    mc_source_dk_tile_occurrences_release(c->state.chunk, b);
    c->state.old_tile_set_count = c->old_count;
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
            if (!tile_eligibility(c))
                return false;
        }
    }
    const mc_block *b = NULL;
    if (mc_registry_block_get(registry, c->state.block_id, &b, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.registry.message, 422,
                    true);
    /* astore19 at423 overwrites the Tile local after the actual array load. */
    mc_tile_entity_release(c->tile);
    c->tile = NULL;
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

static bool contains(mc_tile_entity *const *tiles, size_t count, const mc_tile_entity *tile) {
    for (size_t i = 0; i < count; ++i)
        if (tiles[i] == tile)
            return true;
    return false;
}

static bool new_set(mc_source_dk_compile *c) {
    mc_client_source_dk *dk = c->state.chunk;
    mc_client_render_list *b = dk->fields.B;
    if (!tile_list(c, b, dk->tile_list_origin, 562))
        return false;
    if (b->count > (8u * 1024u * 1024u) / sizeof(*c->new_tiles))
        return fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "DK new TileEntity Set native extent", 547,
                    true);
    if (b->count != 0) {
        c->new_tiles = calloc(b->count, sizeof(*c->new_tiles));
        if (c->new_tiles == NULL)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                        "DK new TileEntity Set native allocation", 547, true);
    }
    for (size_t i = 0; i < b->count; ++i) {
        mc_source_dk_tile_hold *h = b_occurrence(dk, b, i);
        if (h == NULL || h->tile != b->entries[i])
            return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                        "DK new Set actual Tile occurrence ownership", 562, true);
        if (!contains(c->new_tiles, c->new_count, h->tile)) {
            if (mc_tile_entity_retain(h->tile) != MC_TILE_ENTITY_OK)
                return fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "DK new Set native Tile occurrence",
                            562, true);
            c->new_tiles[c->new_count++] = h->tile;
        }
    }
    /* OW and the supported concrete CY/UK classes inherit Object equality.
     * Set membership is identity; no JVM identity-hash iteration is invented. */
    size_t kept = 0;
    for (size_t i = 0; i < c->new_count; ++i)
        if (contains(c->old_tiles, c->old_count, c->new_tiles[i]))
            mc_tile_entity_release(c->new_tiles[i]);
        else
            c->new_tiles[kept++] = c->new_tiles[i];
    c->new_count = kept;
    c->state.new_tile_set_count = kept;
    return true;
}

static mc_render_global_tile_occurrence *f_occurrence(mc_client_render_global *n,
                                                      mc_client_render_list *list, size_t index) {
    for (mc_render_global_tile_occurrence *h = n->tile_occurrences; h != NULL; h = h->next)
        if (h->active && h->list == list && h->index == index)
            return h;
    return NULL;
}

static bool publish_new(mc_source_dk_compile *c) {
    mc_client_source_dk *dk = c->state.chunk;
    mc_client_render_global *n = dk->owner;
    mc_client_render_list *f = dk->fields.F; /* Actual fresh F579. */
    if (!tile_list(c, f, n->tile_list_origin, 584))
        return false;
    if (c->new_count > 1)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                    "DK multiple new Tiles require actual JVM identity-hash Set iteration", 584,
                    true);
    if (c->new_count == 0)
        return true;
    mc_render_global_tile_occurrence *h = n->tile_occurrences;
    while (h != NULL && h->active)
        h = h->next;
    if (h == NULL) {
        h = mc_render_global_allocate(
            n, sizeof(*h), (mc_client_graphics_site){"dk", "a", "()V", 584}, &c->failure);
        if (h == NULL)
            return fail(c, c->failure.status, c->failure.operation, 584, true);
        h->next = n->tile_occurrences;
        n->tile_occurrences = h;
    }
    mc_tile_entity *tile = c->new_tiles[0];
    if (mc_tile_entity_retain(tile) != MC_TILE_ENTITY_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "DK shared List native Tile occurrence", 584,
                    true);
    const size_t index = f->count;
    /* Actual ArrayList.addAll with zero/one Set element publishes no prefix
     * on an allocation refusal. Its independent native right is prepared first. */
    if (!mc_render_global_pointer_append(
            n, f, tile, (mc_client_graphics_site){"dk", "a", "()V", 584}, &c->failure)) {
        mc_tile_entity_release(tile);
        return fail(c, c->failure.status, c->failure.operation, 584, c->failure.native_policy);
    }
    h->tile = tile;
    h->list = f;
    h->index = index;
    h->active = true;
    ++c->state.shared_tiles_added;
    return true;
}

static bool remove_old(mc_source_dk_compile *c) {
    mc_client_source_dk *dk = c->state.chunk;
    mc_client_render_list *b = dk->fields.B; /* Actual fresh B593. */
    if (!tile_list(c, b, dk->tile_list_origin, 596))
        return false;
    for (size_t i = 0; i < b->count; ++i) {
        mc_source_dk_tile_hold *h = b_occurrence(dk, b, i);
        if (h == NULL || h->tile != b->entries[i])
            return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                        "DK old Set subtraction actual Tile ownership", 596, true);
    }
    size_t old_kept = 0;
    for (size_t i = 0; i < c->old_count; ++i) {
        bool found = false;
        for (size_t j = 0; j < b->count; ++j)
            if (b->entries[j] == c->old_tiles[i]) {
                found = true;
                break;
            }
        if (found)
            mc_tile_entity_release(c->old_tiles[i]);
        else
            c->old_tiles[old_kept++] = c->old_tiles[i];
    }
    c->old_count = old_kept;
    c->state.old_tile_set_count = old_kept;
    mc_client_render_list *f = dk->fields.F; /* Actual fresh F603, after Set.removeAll596. */
    if (!tile_list(c, f, dk->owner->tile_list_origin, 608))
        return false;
    for (size_t i = 0; i < f->count; ++i) {
        mc_render_global_tile_occurrence *h = f_occurrence(dk->owner, f, i);
        if (h == NULL || h->tile != f->entries[i])
            return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                        "DK shared List.removeAll actual Tile occurrence ownership", 608, true);
    }
    size_t kept = 0;
    for (size_t i = 0; i < f->count; ++i) {
        mc_render_global_tile_occurrence *h = f_occurrence(dk->owner, f, i);
        if (contains(c->old_tiles, c->old_count, h->tile)) {
            mc_tile_entity_release(h->tile);
            h->tile = NULL;
            h->list = NULL;
            h->index = 0;
            h->active = false;
            ++c->state.shared_tiles_removed;
        } else {
            f->entries[kept++] = f->entries[i];
            h->index = kept - 1u;
        }
    }
    for (size_t i = kept; i < f->count; ++i)
        f->entries[i] = NULL;
    f->count = kept;
    return true;
}

static bool finish_lists(mc_source_dk_compile *c) {
    mc_client_source_dk *dk = c->state.chunk;
    if (!new_set(c) || !publish_new(c) || !remove_old(c))
        return false;
    if (c->world_classes == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                    "DK actual chunk-statistics defining namespace", 615, true);
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
                                      mc_client_chunk_cache_block_failure(
                                          c->cache, c->state.x, c->state.y, c->state.z,
                                          &c->state.block_id, &c->cache_error),
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
    return finish_lists(c);
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

bool mc_source_dk_tile_eligibility_entered(mc_client_source_dk *dk, mc_tile_entity *tile,
                                           mc_source_dk_compile **out,
                                           mc_client_render_global_error *error) {
    if (dk == NULL || out == NULL || *out != NULL || !dk->owner->active) {
        mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                     "DK Tile eligibility borrowed N operation", NULL, NULL, NULL,
                                     0, true);
        return false;
    }
    mc_source_dk_compile *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                                     "DK Tile eligibility native call", NULL, NULL, NULL, 0, true);
        return false;
    }
    *out = c;
    c->state.chunk = dk;
    c->state.failure = &c->failure;
    c->world_classes =
        dk->fields.a == NULL
            ? (dk->owner->fields.t == NULL ? NULL : dk->owner->fields.t->world_classes)
            : dk->fields.a->classes;
    c->state.active = true;
    bool ok;
    if (tile != NULL && mc_tile_entity_retain(tile) != MC_TILE_ENTITY_OK)
        ok = fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "DK Tile local native occurrence", 392, true);
    else {
        c->tile = tile;
        ok = tile_eligibility(c);
    }
    c->state.active = false;
    *error = c->failure;
    return ok;
}

bool mc_source_dk_tile_lists_entered(mc_client_source_dk *dk, mc_tile_entity *const *tiles,
                                     size_t count, mc_source_dk_compile **out,
                                     mc_client_render_global_error *error) {
    if (dk == NULL || out == NULL || *out != NULL || !dk->owner->active ||
        (count != 0 && tiles == NULL)) {
        mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                     "DK selected Tile Lists borrowed N operation", NULL, NULL,
                                     NULL, 0, true);
        return false;
    }
    mc_source_dk_compile *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                                     "DK selected Tile Lists native call", NULL, NULL, NULL, 0,
                                     true);
        return false;
    }
    *out = c;
    c->state.chunk = dk;
    c->state.failure = &c->failure;
    c->world_classes =
        dk->fields.a == NULL
            ? (dk->owner->fields.t == NULL ? NULL : dk->owner->fields.t->world_classes)
            : dk->fields.a->classes;
    c->state.active = true;
    bool ok = old_set(c);
    for (size_t i = 0; ok && i < count; ++i) {
        mc_tile_entity_release(c->tile);
        c->tile = NULL;
        if (tiles[i] != NULL && mc_tile_entity_retain(tiles[i]) != MC_TILE_ENTITY_OK)
            ok =
                fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "DK Tile local native occurrence", 392, true);
        else {
            c->tile = tiles[i];
            ok = tile_eligibility(c);
        }
    }
    if (ok)
        ok = finish_lists(c);
    c->state.active = false;
    *error = c->failure;
    return ok;
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
    if (mc_client_tile_renderer_dispatcher_release(&c->ll, &c->tiles_cleanup) !=
        MC_CLIENT_TILE_RENDERER_OK) {
        error->tile_renderer = &c->tiles_cleanup;
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                            c->tiles_cleanup.operation, NULL, NULL, NULL, 0, true);
    }
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
    for (size_t i = 0; i < c->new_count; ++i)
        mc_tile_entity_release(c->new_tiles[i]);
    free(c->old_tiles);
    free(c->new_tiles);
    free(c);
    *slot = NULL;
    return MC_CLIENT_RENDER_GLOBAL_OK;
}
