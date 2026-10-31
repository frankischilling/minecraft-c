#include "ge.h"
#include "internal.h"
#include "nw.h"
#include <stdlib.h>
#include <string.h>

static mc_client_render_engine_status fail(mc_client_render_engine_error *e,
                                           mc_client_render_engine_status status,
                                           const char *operation, const char *source, unsigned pc,
                                           unsigned ji_pc, const char *ge_method, unsigned ge_pc) {
    e->status = status;
    e->operation = operation;
    e->source_class = source;
    e->source_pc = pc;
    e->ji_pc = ji_pc;
    e->ge_method = ge_method;
    e->ge_pc = ge_pc;
    return status;
}

mc_client_render_engine_options mc_client_render_engine_default_options(void) {
    return (mc_client_render_engine_options){2u * 1024u * 1024u, SIZE_MAX};
}

static void *allocate(mc_client_render_engine *o, size_t bytes, const char *source, unsigned pc,
                      unsigned ji_pc, const char *ge_method, unsigned ge_pc) {
    mc_client_graphics_error cause = {0};
    const mc_client_graphics_site site = {source, ge_method, NULL, (int32_t)pc};
    void *value = mc_client_render_arena_allocate(&o->arena, bytes, site, &cause);
    if (value == NULL)
        fail(&o->failure,
             cause.status == MC_CLIENT_GRAPHICS_LIMIT ? MC_CLIENT_RENDER_ENGINE_LIMIT
                                                      : MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED,
             cause.status == MC_CLIENT_GRAPHICS_LIMIT ? "RenderEngine native constructor limit"
                                                      : "RenderEngine constructor allocation",
             source, pc, ji_pc, ge_method, ge_pc);
    return value;
}

static bool charge_external(mc_client_render_engine *o, size_t bytes, unsigned pc) {
    mc_client_graphics_error cause = {0};
    if (!mc_client_render_arena_check(
            &o->arena, bytes, (mc_client_graphics_site){"ji", "<init>", "(Lik;Lkv;)V", (int32_t)pc},
            &cause)) {
        fail(&o->failure, MC_CLIENT_RENDER_ENGINE_LIMIT, "RenderEngine image constructor limit",
             "ji", pc, pc, NULL, 0);
        return false;
    }
    return true;
}

static void ge_failure(mc_client_render_engine *o, const mc_client_graphics_error *e,
                       unsigned ji_pc) {
    if (e->status == MC_CLIENT_GRAPHICS_CLASS_FAILED) {
        o->failure = o->classes->failure;
        o->failure.cause_status = o->classes->failure.status;
        o->failure.status = MC_CLIENT_RENDER_ENGINE_CLASS_FAILED;
        o->failure.operation = "RenderEngine previous ge initialization failure";
        o->failure.ji_pc = ji_pc;
        o->failure.source_class = "ji";
        o->failure.source_pc = ji_pc;
        return;
    }
    const mc_client_render_engine_status status = e->status == MC_CLIENT_GRAPHICS_LIMIT
                                                      ? MC_CLIENT_RENDER_ENGINE_LIMIT
                                                      : MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED;
    const bool initializer = e->site.method != NULL && strcmp(e->site.method, "<clinit>") == 0;
    fail(&o->failure, status,
         initializer
             ? (status == MC_CLIENT_RENDER_ENGINE_LIMIT ? "ge List native constructor limit"
                                                        : "ge actual List allocation")
             : (status == MC_CLIENT_RENDER_ENGINE_LIMIT ? "RenderEngine native constructor limit"
                                                        : "RenderEngine constructor allocation"),
         e->site.class_name, (unsigned)e->site.pc, ji_pc, e->site.method, (unsigned)e->site.pc);
    if (initializer)
        o->classes->failure = o->failure;
}

static mc_client_direct_byte_buffer *byte_buffer(mc_client_render_engine *o, size_t bytes,
                                                 unsigned ji_pc) {
    mc_client_graphics_error cause = {0};
    mc_client_direct_byte_buffer *buffer = NULL;
    if (!mc_client_ge_c(o->classes, &o->arena, (int32_t)bytes,
                        (mc_client_graphics_site){"ji", "<init>", "(Lik;Lkv;)V", (int32_t)ji_pc},
                        &buffer, &cause))
        ge_failure(o, &cause, ji_pc);
    return buffer;
}

static mc_client_direct_int_buffer *int_buffer(mc_client_render_engine *o, unsigned ji_pc) {
    mc_client_graphics_error cause = {0};
    mc_client_direct_int_buffer *buffer = NULL;
    if (!mc_client_ge_d(o->classes, &o->arena, 1,
                        (mc_client_graphics_site){"ji", "<init>", "(Lik;Lkv;)V", (int32_t)ji_pc},
                        &buffer, &cause))
        ge_failure(o, &cause, ji_pc);
    return buffer;
}

mc_client_render_engine_status
mc_client_render_engine_classes_active_use(mc_client_render_engine_classes *c,
                                           mc_client_render_engine_error *error) {
    mc_client_render_engine_error e = {0};
    if (c == NULL || c->resource_retirement_started)
        fail(&e,
             c == NULL ? MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT
                       : MC_CLIENT_RENDER_ENGINE_INVALID_PHASE,
             "ji class native active-use admission", NULL, 0, 0, NULL, 0);
    else if (!c->ji_ready) {
        c->ji_a = false; /* Actual ji.<clinit> putstatic1. */
        c->ji_ready = true;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

static bool construct(mc_client_render_engine *o, mc_client_texture_pack_list *packs,
                      mc_game_settings *settings) {
    if (mc_client_render_engine_classes_active_use(o->classes, &o->failure) !=
        MC_CLIENT_RENDER_ENGINE_OK)
        return false;
    o->fields.b = allocate(o, sizeof(mc_client_render_map), "ji", 5, 5, NULL, 0);
    if (o->fields.b == NULL)
        return false;
    ((mc_client_render_map *)(void *)o->fields.b)->string_ids = NULL; /* Native typed b role. */
    o->last_store_pc = 12;
    o->fields.c = allocate(o, sizeof(mc_client_render_map), "ji", 16, 16, NULL, 0);
    if (o->fields.c == NULL)
        return false;
    ((mc_client_render_map *)(void *)o->fields.c)->color_arrays = NULL; /* Actual c int[] role. */
    o->last_store_pc = 23;
    o->fields.d = allocate(o, sizeof(mc_client_render_map), "ji", 27, 27, NULL, 0);
    if (o->fields.d == NULL)
        return false;
    ((mc_client_render_map *)(void *)o->fields.d)->entries = NULL; /* Existing image role. */
    o->last_store_pc = 34;
    mc_client_direct_int_buffer *ints = int_buffer(o, 39);
    if (ints == NULL)
        return false;
    o->fields.e = ints;
    o->last_store_pc = 42;
    o->fields.f = byte_buffer(o, 1048576, 48);
    if (o->fields.f == NULL)
        return false;
    o->last_store_pc = 51;
    o->fields.g = allocate(o, sizeof(mc_client_render_list), "ji", 55, 55, NULL, 0);
    if (o->fields.g == NULL)
        return false;
    o->last_store_pc = 62;
    o->fields.h = allocate(o, sizeof(mc_client_render_map), "ji", 66, 66, NULL, 0);
    if (o->fields.h == NULL)
        return false;
    ((mc_client_render_map *)(void *)o->fields.h)->entries = NULL; /* Empty h role. */
    o->last_store_pc = 73;
    o->fields.j = false;
    o->last_store_pc = 78;
    o->fields.k = false;
    o->last_store_pc = 83;
    if (!charge_external(o, sizeof(mc_client_argb_image), 87))
        return false;
    o->candidate = calloc(1, sizeof(*o->candidate));
    if (o->candidate == NULL) {
        fail(&o->failure, MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED, "BufferedImage native object",
             "ji", 87, 87, NULL, 0);
        return false;
    }
    o->arena.bytes += sizeof(*o->candidate);
    ++o->arena.allocations;
    atomic_init(&o->candidate->holds, 1);
    if (!charge_external(o, 64u * 64u * sizeof(uint32_t), 96))
        return false;
    o->candidate->pixels = calloc(64u * 64u, sizeof(uint32_t));
    if (o->candidate->pixels == NULL) {
        fail(&o->failure, MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED,
             "BufferedImage native ARGB plane", "ji", 96, 96, NULL, 0);
        return false;
    }
    o->arena.bytes += 64u * 64u * sizeof(uint32_t);
    ++o->arena.allocations;
    o->candidate->width = o->candidate->height = 64;
    o->candidate->count = 4096;
    o->candidate->constructed = true;
    o->fields.m = o->candidate;
    o->candidate = NULL;
    o->last_store_pc = 99;
    o->fields.l = packs;
    o->last_store_pc = 104;
    o->fields.i = settings;
    o->last_store_pc = 109;
    o->graphics = allocate(o, sizeof(render_graphics), "ji", 116, 116, NULL, 0);
    if (o->graphics == NULL)
        return false;
    if (mc_client_argb_image_retain(o->fields.m) != MC_CLIENT_RENDER_ENGINE_OK) {
        fail(&o->failure, MC_CLIENT_RENDER_ENGINE_LIMIT, "Graphics image native hold", "ji", 116,
             116, NULL, 0);
        return false;
    }
    o->graphics->image = o->fields.m;
    if (mc_client_default_font_retain(o->font) != MC_CLIENT_FONT_OK) {
        fail(&o->failure, MC_CLIENT_RENDER_ENGINE_LIMIT, "Graphics dependency native hold", "ji",
             116, 116, NULL, 0);
        return false;
    }
    o->graphics->font = o->font;
    o->graphics->color = UINT32_C(0xffffffff); /* Actual setColor124. */
    for (size_t i = 0; i < o->fields.m->count; ++i)
        o->fields.m->pixels[i] = UINT32_C(0xffffffff); /* fillRect134 */
    o->graphics->color = UINT32_C(0xff000000);         /* setColor141 */
    const mc_client_font_argb_view view = {o->fields.m->pixels, 64, 64, 64};
    if (mc_client_default_font_draw_missingtex(o->font, view, &o->failure.font) !=
        MC_CLIENT_FONT_OK) {
        fail(&o->failure, MC_CLIENT_RENDER_ENGINE_FONT_FAILED,
             "RenderEngine actual default-font drawString", "ji", 150, 150, NULL, 0);
        return false;
    }
    o->failure.font = (mc_client_font_error){0};
    o->graphics->disposed = true; /* Source dispose154, separate from native teardown. */
    mc_client_argb_image_release(o->graphics->image);
    o->graphics->image = NULL;
    (void)mc_client_default_font_release(o->graphics->font);
    o->graphics->font = NULL;
    return true;
}

mc_client_render_engine_status
mc_client_render_engine_classes_create(const mc_client_render_engine_options *options,
                                       mc_client_render_engine_classes **out) {
    return mc_client_render_engine_classes_allocate_context(options, NULL, NULL, out);
}

static bool engine_colors_can_retire(const mc_client_render_engine *, bool last_classes);

mc_client_render_engine_status
mc_client_render_engine_classes_retain(mc_client_render_engine_classes *c) {
    if (c == NULL)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    size_t observed = atomic_load_explicit(&c->holds, memory_order_acquire);
    for (;;) {
        if (observed == 0)
            return MC_CLIENT_RENDER_ENGINE_INVALID_PHASE;
        if (observed == SIZE_MAX)
            return MC_CLIENT_RENDER_ENGINE_LIMIT;
        if (atomic_compare_exchange_weak_explicit(&c->holds, &observed, observed + 1,
                                                  memory_order_acq_rel, memory_order_acquire))
            return MC_CLIENT_RENDER_ENGINE_OK;
    }
}

/* Returns a unique final-storage right only for a protected 1->0 drop. A
 * successful >1 drop can never finalize this shared namespace. No source or
 * device activity is inferred from this one native occurrence account. */
static mc_client_render_engine_status classes_drop(mc_client_render_engine_classes *c,
                                                   bool legacy_policy,
                                                   const mc_client_render_engine *engine,
                                                   bool *final) {
    size_t observed = atomic_load_explicit(&c->holds, memory_order_acquire);
    for (;;) {
        if (observed == 0)
            return MC_CLIENT_RENDER_ENGINE_INVALID_PHASE;
        if (legacy_policy &&
            (c->active_source_calls != 0 || c->nw.phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING))
            return MC_CLIENT_RENDER_ENGINE_BUSY;
        if (observed == 1) {
            if (c->active_source_calls != 0 ||
                c->nw.phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING ||
                c->native_resource_count != 0 || c->active_resource_calls != 0 ||
                c->command_range_count != 0 || c->font_lease_count != 0 ||
                c->context_lease != NULL || !mc_client_colorizer_can_retire(c))
                return MC_CLIENT_RENDER_ENGINE_BUSY;
        }
        if (engine != NULL && !engine_colors_can_retire(engine, observed == 1))
            return MC_CLIENT_RENDER_ENGINE_BUSY;
        const bool last = observed == 1;
        if (atomic_compare_exchange_weak_explicit(&c->holds, &observed, observed - 1,
                                                  memory_order_acq_rel, memory_order_acquire)) {
            *final = last;
            return MC_CLIENT_RENDER_ENGINE_OK;
        }
    }
}

static void destroy_classes(mc_client_render_engine_classes *c) {
    if (c->a != NULL)
        free(c->a->entries);
    if (c->b != NULL)
        free(c->b->entries);
    free(c->a);
    free(c->b);
    free(c->device);
    mc_client_colorizer_free_namespace(c);
    mc_client_nw_free_namespace(c);
    mc_client_render_arena_free(&c->arena);
    free(c);
}

mc_client_render_engine_status
mc_client_render_engine_classes_release(mc_client_render_engine_classes *c) {
    if (c == NULL)
        return MC_CLIENT_RENDER_ENGINE_OK;
    bool final = false;
    const mc_client_render_engine_status status = classes_drop(c, true, NULL, &final);
    if (status == MC_CLIENT_RENDER_ENGINE_OK && final)
        destroy_classes(c);
    return status;
}

bool mc_client_render_engine_classes_inspect(const mc_client_render_engine_classes *c,
                                             mc_client_render_engine_class_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = (mc_client_render_engine_class_state){
        .native_holds = atomic_load_explicit(&c->holds, memory_order_acquire),
        .ji_ready = c->ji_ready,
        .ji_a = c->ji_a,
        .ge_phase = c->ge_phase,
        .ge_a = c->a,
        .ge_b = c->b,
        .ge_failure = c->failure,
        .native_resource_count = c->native_resource_count,
        .active_resource_calls = c->active_resource_calls,
        .resource_retirement_started = c->resource_retirement_started};
    return true;
}

mc_client_render_engine_status
mc_client_render_engine_allocate(mc_client_render_engine_classes *c, mc_client_default_font *font,
                                 const mc_client_render_engine_options *options,
                                 mc_client_render_engine **out) {
    if (c == NULL || font == NULL || out == NULL)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    mc_client_render_engine_status status = mc_client_render_engine_classes_retain(c);
    if (status != MC_CLIENT_RENDER_ENGINE_OK)
        return status;
    if (mc_client_default_font_retain(font) != MC_CLIENT_FONT_OK) {
        mc_client_render_engine_classes_release(c);
        return MC_CLIENT_RENDER_ENGINE_LIMIT;
    }
    mc_client_render_engine *o = calloc(1, sizeof(*o));
    if (o == NULL) {
        mc_client_render_engine_classes_release(c);
        (void)mc_client_default_font_release(font);
        return MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED;
    }
    o->holds = 1;
    o->classes = c;
    o->font = font;
    o->arena.options = options == NULL ? mc_client_render_engine_default_options() : *options;
    *out = o;
    return MC_CLIENT_RENDER_ENGINE_OK;
}

mc_client_render_engine_status
mc_client_render_engine_construct(mc_client_render_engine *o, mc_client_texture_pack_list *packs,
                                  mc_game_settings *settings,
                                  mc_client_render_engine_error *error) {
    mc_client_render_engine_error e = {0};
    if (o == NULL)
        fail(&e, MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT, "RenderEngine receiver", "ji", 0, 0,
             NULL, 0);
    else if (o->phase != MC_CLIENT_RENDER_ENGINE_RAW || o->classes->resource_retirement_started ||
             o->lookup_retired)
        fail(&e, MC_CLIENT_RENDER_ENGINE_INVALID_PHASE, "RenderEngine RAW constructor admission",
             "ji", 0, 0, NULL, 0);
    else {
        o->phase = MC_CLIENT_RENDER_ENGINE_CONSTRUCTING;
        const bool ok = construct(o, packs, settings);
        o->phase = ok ? MC_CLIENT_RENDER_ENGINE_READY : MC_CLIENT_RENDER_ENGINE_FAILED;
        e = o->failure;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_render_engine_status mc_client_render_engine_retain(mc_client_render_engine *o) {
    if (o == NULL)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    if (o->holds == SIZE_MAX)
        return MC_CLIENT_RENDER_ENGINE_LIMIT;
    ++o->holds;
    return MC_CLIENT_RENDER_ENGINE_OK;
}

static size_t color_map_occurrences(const mc_client_render_map *map,
                                    const mc_client_color_array *array) {
    size_t count = 0;
    if (map != NULL && map->color_arrays != NULL && array != NULL)
        for (size_t i = 0; i < map->count; ++i)
            count += map->color_arrays[i].array == array ? 1u : 0u;
    return count;
}

static bool engine_colors_can_retire(const mc_client_render_engine *o, bool last_classes) {
    const mc_client_render_map *map = o->fields.c;
    if (last_classes && !mc_client_colorizer_can_retire(o->classes))
        return false;
    if (map != NULL && map->color_arrays != NULL)
        for (size_t i = 0; i < map->count; ++i) {
            const mc_client_color_array *array = map->color_arrays[i].array;
            size_t count = color_map_occurrences(map, array);
            if (last_classes)
                count += mc_client_colorizer_occurrences(o->classes, array);
            if (!mc_client_color_array_can_release_occurrences(array, count))
                return false;
        }
    return true;
}

static void destroy_engine(mc_client_render_engine *o, bool drop_class) {
    if (o->graphics != NULL) {
        mc_client_argb_image_release(o->graphics->image);
        (void)mc_client_default_font_release(o->graphics->font);
    }
    mc_client_argb_image_release(o->candidate);
    mc_client_argb_image_release(o->fields.m);
    const mc_client_render_map *maps[3] = {o->fields.b, o->fields.c, o->fields.d};
    for (size_t i = 0; i < 3; ++i)
        if (maps[i] != NULL) {
            if (i > 1 && maps[i]->entries != NULL)
                for (size_t j = 0; j < maps[i]->count; ++j)
                    mc_client_argb_image_release(maps[i]->entries[j].image);
            if (i == 0 && maps[i]->string_ids != NULL)
                for (size_t j = 0; j < maps[i]->count; ++j)
                    free(maps[i]->string_ids[j].units);
            if (i == 1 && maps[i]->color_arrays != NULL)
                for (size_t j = 0; j < maps[i]->count; ++j) {
                    free(maps[i]->color_arrays[j].units);
                    (void)mc_client_color_array_release(&maps[i]->color_arrays[j].array, NULL);
                }
            free(i == 0   ? (void *)maps[i]->string_ids
                 : i == 1 ? (void *)maps[i]->color_arrays
                          : (void *)maps[i]->entries);
        }
    /* h contains ek owners, never BufferedImages. The client layer retires its
     * workers and values before this pure CPU destructor is admitted. */
    if (o->fields.h != NULL)
        free(o->fields.h->downloads);
    if (o->fields.g != NULL)
        free(((mc_client_render_list *)o->fields.g)->entries);
    mc_client_render_arena_free(&o->arena);
    if (drop_class)
        (void)mc_client_render_engine_classes_release(o->classes);
    (void)mc_client_default_font_release(o->font);
    free(o);
}

mc_client_render_engine_status mc_client_render_engine_release(mc_client_render_engine *o) {
    if (o == NULL)
        return MC_CLIENT_RENDER_ENGINE_OK;
    if (o->phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING || o->active_resource_calls != 0 ||
        o->classes->active_source_calls != 0)
        return MC_CLIENT_RENDER_ENGINE_BUSY;
    /* Opaque client-device prefix/policy storage is retired explicitly in its
     * own layer. CPU ownership never calls Pack/Display or a cleanup callback. */
    if (o->holds == 1 && o->fields.g != NULL && o->fields.g->count != 0 && !o->effects_retired)
        return MC_CLIENT_RENDER_ENGINE_BUSY;
    if (o->holds == 1 && o->lookup != NULL)
        return MC_CLIENT_RENDER_ENGINE_BUSY;
    if (o->holds == 1 &&
        (o->image_downloads != NULL || (o->fields.h != NULL && o->fields.h->count != 0)))
        return MC_CLIENT_RENDER_ENGINE_BUSY;
    if (o->holds == 1 && atomic_load_explicit(&o->classes->holds, memory_order_acquire) == 1 &&
        (o->classes->native_resource_count != 0 || o->classes->command_range_count != 0 ||
         o->classes->font_lease_count != 0 || o->classes->context_lease != NULL))
        return MC_CLIENT_RENDER_ENGINE_BUSY;
    if (o->holds == 1) {
        const size_t class_holds = atomic_load_explicit(&o->classes->holds, memory_order_acquire);
        if (!engine_colors_can_retire(o, class_holds == 1))
            return MC_CLIENT_RENDER_ENGINE_BUSY;
    }
    if (--o->holds != 0)
        return MC_CLIENT_RENDER_ENGINE_OK;
    destroy_engine(o, true);
    return MC_CLIENT_RENDER_ENGINE_OK;
}

mc_client_render_engine_status
mc_client_render_engine_retire(mc_client_render_engine **slot,
                               mc_client_render_engine_error *error) {
    mc_client_render_engine_error e = {0};
    if (slot == NULL)
        fail(&e, MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT, "Engine final native occurrence output",
             NULL, 0, 0, NULL, 0);
    else if (*slot != NULL) {
        mc_client_render_engine *o = *slot;
        /* Constructor is an actual source-call domain independent of resource
         * counters. Every retained lookup prefix also prevents final release;
         * registration/lookup calls and font leases hold the actual Engine. */
        if (o->holds != 1 || o->phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING ||
            o->active_resource_calls != 0 || o->lookup != NULL || o->image_downloads != NULL ||
            (o->fields.h != NULL && o->fields.h->count != 0) ||
            (o->fields.g != NULL && o->fields.g->count != 0 && !o->effects_retired))
            fail(&e, MC_CLIENT_RENDER_ENGINE_BUSY, "Engine own operation/hold/pending dependency",
                 NULL, 0, 0, NULL, 0);
        else {
            mc_client_render_engine_classes *classes = o->classes;
            bool final_classes = false;
            const mc_client_render_engine_status status =
                classes_drop(classes, false, o, &final_classes);
            if (status != MC_CLIENT_RENDER_ENGINE_OK)
                fail(&e, status, "Engine own arrays or actual final class dependency", NULL, 0, 0,
                     NULL, 0);
            else {
                /* Our canonical class occurrence is already dropped. The CPU
                 * body never touches the namespace; a final right remains
                 * locally owned until its dependent CPU data is destroyed. */
                o->holds = 0; /* Consume this exclusive final Engine occurrence once. */
                destroy_engine(o, false);
                if (final_classes)
                    destroy_classes(classes);
                *slot = NULL;
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_render_engine_inspect(const mc_client_render_engine *o,
                                     mc_client_render_engine_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = (mc_client_render_engine_state){
        .phase = o->phase,
        .native_holds = o->holds,
        .constructor_bytes = o->arena.bytes,
        .constructor_allocations = o->arena.allocations,
        .last_store_pc = o->last_store_pc,
        .graphics_created = o->graphics != NULL,
        .graphics_disposed = o->graphics != NULL && o->graphics->disposed,
        .graphics_color = o->graphics == NULL ? 0 : o->graphics->color,
        .failed_image = o->candidate,
        .fields = o->fields,
        .failure = o->failure};
    return true;
}

bool mc_client_render_map_count(const mc_client_render_map *o, size_t *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = o->count;
    return true;
}

bool mc_client_render_list_count(const mc_client_render_list *o, size_t *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = o->count;
    return true;
}

bool mc_client_direct_byte_buffer_inspect(const mc_client_direct_byte_buffer *b,
                                          mc_client_direct_buffer_state *out) {
    if (b == NULL || out == NULL)
        return false;
    *out = (mc_client_direct_buffer_state){b,           b->data,     NULL, b->position, b->limit,
                                           b->capacity, b->capacity, true, true};
    return true;
}

bool mc_client_direct_int_buffer_inspect(const mc_client_direct_int_buffer *b,
                                         mc_client_direct_buffer_state *out) {
    if (b == NULL || out == NULL)
        return false;
    *out = (mc_client_direct_buffer_state){
        b,           b->bytes->data,     b->bytes, b->position, b->limit,
        b->capacity, b->bytes->capacity, true,     true};
    return true;
}

bool mc_client_argb_image_inspect(const mc_client_argb_image *i, mc_client_argb_image_state *out) {
    if (i == NULL || out == NULL)
        return false;
    *out = (mc_client_argb_image_state){atomic_load_explicit(&i->holds, memory_order_acquire),
                                        i->width,
                                        i->height,
                                        i->constructed ? 2u : 0u,
                                        i->pixels,
                                        i->count,
                                        i->constructed};
    return true;
}

mc_client_render_engine_status mc_client_argb_image_retain(mc_client_argb_image *i) {
    if (i == NULL)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    size_t observed = atomic_load_explicit(&i->holds, memory_order_acquire);
    for (;;) {
        if (observed == 0)
            return MC_CLIENT_RENDER_ENGINE_INVALID_PHASE;
        if (observed == SIZE_MAX)
            return MC_CLIENT_RENDER_ENGINE_LIMIT;
        if (atomic_compare_exchange_weak_explicit(&i->holds, &observed, observed + 1,
                                                  memory_order_acq_rel, memory_order_acquire))
            return MC_CLIENT_RENDER_ENGINE_OK;
    }
}

mc_client_render_engine_status mc_client_argb_image_create(int32_t width, int32_t height,
                                                           size_t max_bytes,
                                                           mc_client_argb_image **out) {
    if (out == NULL || width <= 0 || height <= 0)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    const size_t w = (size_t)width, h = (size_t)height;
    if (w > SIZE_MAX / h || w * h > (SIZE_MAX - sizeof(mc_client_argb_image)) / sizeof(uint32_t))
        return MC_CLIENT_RENDER_ENGINE_LIMIT;
    const size_t count = w * h, bytes = count * sizeof(uint32_t);
    if (width > 8192 || height > 8192 || bytes > 64u * 1024u * 1024u)
        return MC_CLIENT_RENDER_ENGINE_LIMIT;
    if (sizeof(mc_client_argb_image) + bytes > max_bytes)
        return MC_CLIENT_RENDER_ENGINE_LIMIT;
    mc_client_argb_image *image = calloc(1, sizeof(*image));
    if (image == NULL)
        return MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED;
    image->pixels = calloc(count, sizeof(uint32_t));
    if (image->pixels == NULL) {
        free(image);
        return MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED;
    }
    atomic_init(&image->holds, 1);
    image->width = (uint32_t)width;
    image->height = (uint32_t)height;
    image->count = count;
    image->constructed = true;
    *out = image;
    return MC_CLIENT_RENDER_ENGINE_OK;
}

mc_client_render_engine_status mc_client_argb_image_set_pixel(mc_client_argb_image *image,
                                                              int32_t x, int32_t y, uint32_t argb) {
    if (image == NULL)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    if (!image->constructed)
        return MC_CLIENT_RENDER_ENGINE_INVALID_PHASE;
    if (x < 0 || y < 0 || (uint32_t)x >= image->width || (uint32_t)y >= image->height)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    image->pixels[(size_t)(uint32_t)y * image->width + (uint32_t)x] = argb;
    return MC_CLIENT_RENDER_ENGINE_OK;
}

void mc_client_argb_image_release(mc_client_argb_image *i) {
    if (i == NULL)
        return;
    size_t observed = atomic_load_explicit(&i->holds, memory_order_acquire);
    while (observed != 0) {
        if (atomic_compare_exchange_weak_explicit(&i->holds, &observed, observed - 1,
                                                  memory_order_acq_rel, memory_order_acquire)) {
            if (observed == 1) {
                free(i->pixels);
                free(i);
            }
            return;
        }
    }
}
