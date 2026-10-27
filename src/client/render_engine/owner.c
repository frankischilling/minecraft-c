#include "internal.h"
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
    if (bytes > SIZE_MAX - sizeof(render_allocation) ||
        sizeof(render_allocation) + bytes > o->options.max_constructor_bytes - o->bytes ||
        o->allocations >= o->options.max_constructor_allocations) {
        fail(&o->failure, MC_CLIENT_RENDER_ENGINE_LIMIT, "RenderEngine native constructor limit",
             source, pc, ji_pc, ge_method, ge_pc);
        return NULL;
    }
    render_allocation *a = calloc(1, sizeof(*a) + bytes);
    if (a == NULL) {
        fail(&o->failure, MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED,
             "RenderEngine constructor allocation", source, pc, ji_pc, ge_method, ge_pc);
        return NULL;
    }
    a->next = o->arena;
    o->arena = a;
    o->bytes += sizeof(*a) + bytes;
    ++o->allocations;
    return a + 1;
}

static bool charge_external(mc_client_render_engine *o, size_t bytes, unsigned pc) {
    if (bytes > o->options.max_constructor_bytes - o->bytes ||
        o->allocations >= o->options.max_constructor_allocations) {
        fail(&o->failure, MC_CLIENT_RENDER_ENGINE_LIMIT, "RenderEngine image constructor limit",
             "ji", pc, pc, NULL, 0);
        return false;
    }
    return true;
}

static bool ge_initialize(mc_client_render_engine *o) {
    mc_client_render_engine_classes *c = o->classes;
    if (c->ge_phase == MC_CLIENT_RENDER_ENGINE_READY)
        return true;
    if (c->ge_phase == MC_CLIENT_RENDER_ENGINE_FAILED) {
        o->failure = c->failure;
        o->failure.cause_status = c->failure.status;
        o->failure.status = MC_CLIENT_RENDER_ENGINE_CLASS_FAILED;
        o->failure.operation = "RenderEngine previous ge initialization failure";
        o->failure.ji_pc = 39;
        o->failure.source_class = "ji";
        o->failure.source_pc = 39;
        return false;
    }
    c->ge_phase = MC_CLIENT_RENDER_ENGINE_CONSTRUCTING;
    const unsigned pcs[2] = {0, 10};
    mc_client_render_list **slots[2] = {&c->a, &c->b};
    for (size_t i = 0; i < 2; ++i) {
        if (sizeof(mc_client_render_list) > c->options.max_constructor_bytes - c->bytes ||
            c->allocations >= c->options.max_constructor_allocations) {
            fail(&c->failure, MC_CLIENT_RENDER_ENGINE_LIMIT, "ge List native constructor limit",
                 "ge", pcs[i], 39, "<clinit>", pcs[i]);
            goto failed;
        }
        mc_client_render_list *list = calloc(1, sizeof(*list));
        if (list == NULL) {
            fail(&c->failure, MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED,
                 "ge actual List allocation", "ge", pcs[i], 39, "<clinit>", pcs[i]);
            goto failed;
        }
        c->bytes += sizeof(*list);
        ++c->allocations;
        *slots[i] = list;
    }
    c->ge_phase = MC_CLIENT_RENDER_ENGINE_READY;
    return true;
failed:
    c->ge_phase = MC_CLIENT_RENDER_ENGINE_FAILED;
    o->failure = c->failure;
    return false;
}

static mc_client_direct_byte_buffer *byte_buffer(mc_client_render_engine *o, size_t bytes,
                                                 unsigned ji_pc) {
    mc_client_direct_byte_buffer *b = allocate(o, sizeof(*b) + bytes, "ge", 1, ji_pc, "c", 1);
    if (b != NULL) {
        b->limit = b->capacity = bytes;
        b->data = (uint8_t *)(b + 1);
    }
    return b;
}

static bool construct(mc_client_render_engine *o, mc_client_texture_pack_list *packs,
                      mc_game_settings *settings) {
    if (!o->classes->ji_ready) {
        o->classes->ji_a = false;
        o->classes->ji_ready = true;
    } /* ji literal <clinit>0, once. */
    o->fields.b = allocate(o, sizeof(mc_client_render_map), "ji", 5, 5, NULL, 0);
    if (o->fields.b == NULL)
        return false;
    o->last_store_pc = 12;
    o->fields.c = allocate(o, sizeof(mc_client_render_map), "ji", 16, 16, NULL, 0);
    if (o->fields.c == NULL)
        return false;
    o->last_store_pc = 23;
    o->fields.d = allocate(o, sizeof(mc_client_render_map), "ji", 27, 27, NULL, 0);
    if (o->fields.d == NULL)
        return false;
    o->last_store_pc = 34;
    if (!ge_initialize(o))
        return false;
    mc_client_direct_byte_buffer *underlying = byte_buffer(o, 4, 39);
    if (underlying == NULL)
        return false;
    mc_client_direct_int_buffer *ints = allocate(o, sizeof(*ints), "ge", 6, 39, "d", 6);
    if (ints == NULL)
        return false;
    ints->limit = ints->capacity = 1;
    ints->bytes = underlying;
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
    o->bytes += sizeof(*o->candidate);
    ++o->allocations;
    o->candidate->holds = 1;
    if (!charge_external(o, 64u * 64u * sizeof(uint32_t), 96))
        return false;
    o->candidate->pixels = calloc(64u * 64u, sizeof(uint32_t));
    if (o->candidate->pixels == NULL) {
        fail(&o->failure, MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED,
             "BufferedImage native ARGB plane", "ji", 96, 96, NULL, 0);
        return false;
    }
    o->bytes += 64u * 64u * sizeof(uint32_t);
    ++o->allocations;
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
    if (out == NULL)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    mc_client_render_engine_classes *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED;
    c->holds = 1;
    c->options = options == NULL ? mc_client_render_engine_default_options() : *options;
    *out = c;
    return MC_CLIENT_RENDER_ENGINE_OK;
}

mc_client_render_engine_status
mc_client_render_engine_classes_retain(mc_client_render_engine_classes *c) {
    if (c == NULL)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    if (c->holds == SIZE_MAX)
        return MC_CLIENT_RENDER_ENGINE_LIMIT;
    ++c->holds;
    return MC_CLIENT_RENDER_ENGINE_OK;
}

mc_client_render_engine_status
mc_client_render_engine_classes_release(mc_client_render_engine_classes *c) {
    if (c == NULL)
        return MC_CLIENT_RENDER_ENGINE_OK;
    if (c->holds == 1 && (c->native_resource_count != 0 || c->active_resource_calls != 0))
        return MC_CLIENT_RENDER_ENGINE_BUSY;
    if (--c->holds != 0)
        return MC_CLIENT_RENDER_ENGINE_OK;
    if (c->a != NULL)
        free(c->a->entries);
    if (c->b != NULL)
        free(c->b->entries);
    free(c->a);
    free(c->b);
    free(c->device);
    free(c);
    return MC_CLIENT_RENDER_ENGINE_OK;
}

bool mc_client_render_engine_classes_inspect(const mc_client_render_engine_classes *c,
                                             mc_client_render_engine_class_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = (mc_client_render_engine_class_state){.native_holds = c->holds,
                                                 .ji_ready = c->ji_ready,
                                                 .ji_a = c->ji_a,
                                                 .ge_phase = c->ge_phase,
                                                 .ge_a = c->a,
                                                 .ge_b = c->b,
                                                 .ge_failure = c->failure,
                                                 .native_resource_count = c->native_resource_count,
                                                 .active_resource_calls = c->active_resource_calls,
                                                 .resource_retirement_started =
                                                     c->resource_retirement_started};
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
    o->options = options == NULL ? mc_client_render_engine_default_options() : *options;
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
    else if (o->phase != MC_CLIENT_RENDER_ENGINE_RAW || o->classes->resource_retirement_started)
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

mc_client_render_engine_status mc_client_render_engine_release(mc_client_render_engine *o) {
    if (o == NULL)
        return MC_CLIENT_RENDER_ENGINE_OK;
    if (o->phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING || o->active_resource_calls != 0)
        return MC_CLIENT_RENDER_ENGINE_BUSY;
    if (o->holds == 1 && o->classes->holds == 1 && o->classes->native_resource_count != 0)
        return MC_CLIENT_RENDER_ENGINE_BUSY;
    if (--o->holds != 0)
        return MC_CLIENT_RENDER_ENGINE_OK;
    if (o->graphics != NULL) {
        mc_client_argb_image_release(o->graphics->image);
        (void)mc_client_default_font_release(o->graphics->font);
    }
    mc_client_argb_image_release(o->candidate);
    mc_client_argb_image_release(o->fields.m);
    const mc_client_render_map *maps[4] = {o->fields.b, o->fields.c, o->fields.d, o->fields.h};
    for (size_t i = 0; i < 4; ++i)
        if (maps[i] != NULL) {
            for (size_t j = 0; j < maps[i]->count; ++j)
                mc_client_argb_image_release(maps[i]->entries[j].image);
            free(maps[i]->entries);
        }
    render_allocation *a = o->arena;
    while (a != NULL) {
        render_allocation *next = a->next;
        free(a);
        a = next;
    }
    mc_client_render_engine_classes_release(o->classes);
    (void)mc_client_default_font_release(o->font);
    free(o);
    return MC_CLIENT_RENDER_ENGINE_OK;
}

bool mc_client_render_engine_inspect(const mc_client_render_engine *o,
                                     mc_client_render_engine_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = (mc_client_render_engine_state){
        .phase = o->phase,
        .native_holds = o->holds,
        .constructor_bytes = o->bytes,
        .constructor_allocations = o->allocations,
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
    *out =
        (mc_client_argb_image_state){i->holds,  i->width, i->height,     i->constructed ? 2u : 0u,
                                     i->pixels, i->count, i->constructed};
    return true;
}

mc_client_render_engine_status mc_client_argb_image_retain(mc_client_argb_image *i) {
    if (i == NULL)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    if (i->holds == SIZE_MAX)
        return MC_CLIENT_RENDER_ENGINE_LIMIT;
    ++i->holds;
    return MC_CLIENT_RENDER_ENGINE_OK;
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
    image->holds = 1;
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
    if (i == NULL || --i->holds != 0)
        return;
    free(i->pixels);
    free(i);
}
