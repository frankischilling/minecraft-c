#include "../../block/registry/leaf_color_internal.h"
#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../client/render_engine/internal.h"
#include "../../world/runtime/acquisition_internal.h"
#include "../construction_internal.h"
#include "../living/travel_math_internal.h"
#include "digging_color_internal.h"
#include "mc/block/registry/registry.h"
#include "mc/block/registry/rendering.h"
#include "mc/block/registry/runtime.h"
#include "particle_internal.h"
#include <string.h>

static mc_entity_status digging_fail(mc_particle_error *e, mc_entity_status status,
                                     const char *operation, const char *method,
                                     const char *descriptor, unsigned int pc, bool native) {
    e->status = status;
    e->cause = native ? MC_PARTICLE_OWNER : MC_PARTICLE_NULL_REFERENCE;
    e->operation = operation;
    e->source_class = "qm";
    e->source_method = method;
    e->source_descriptor = descriptor;
    e->source_pc = e->item_pc = pc;
    e->native_policy = native;
    if (!native) {
        e->throwable = MC_PARTICLE_SOURCE_EXCEPTION;
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
    }
    return status;
}

mc_entity_status mc_digging_particle_allocate(mc_entity_classes *classes, mc_entity **out,
                                              mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE,
                                        "digging particle allocation", out, error);
}

mc_entity_status mc_digging_particle_construct_entered(
    mc_entity *p, mc_world_runtime *world, mc_registry_classes *registry, mc_math_classes *math,
    double x, double y, double z, double vx, double vy, double vz, const mc_block *block,
    int32_t face, int32_t metadata, mc_math_classes **reached_math, mc_particle_error *out) {
    if (out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_particle_constructor_error e = {.native_policy = true};
    mc_entity_status status = mc_entity_constructor_begin(
        p, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE, world, "digging particle constructor",
        "digging particle World profile", &e.entity);
    if (status != MC_ENTITY_OK)
        (void)mc_particle_constructor_fail(&e, status, MC_PARTICLE_OWNER, e.entity.operation, NULL,
                                           NULL, 0);
    else {
        if (mc_particle_construct_ancestor(p, world, x, y, z, vx, vy, vz, math, true, reached_math,
                                           &e)) {
            p->particle->digging_face = 0;
            p->particle->item_native.item_pc = 18;
            p->particle->digging_block = block;
            p->particle->item_native.item_pc = 24;
            if (block == NULL) {
                (void)mc_particle_constructor_fail(
                    &e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_NULL_REFERENCE,
                    "QM actual Block icon null receiver", "qm", "<init>", 33);
            } else if (block->classes != registry) {
                (void)mc_particle_constructor_fail(
                    &e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                    "QM actual Block Registry provenance", "qm", "<init>", 33);
            } else {
                e.registry.status = mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT,
                                                               p->classes->profile, &e.registry);
                if (e.registry.status == MC_REGISTRY_OK) {
                    p->particle->digging_registry = registry;
                    int32_t icon;
                    e.registry.status = mc_block_render_texture_metadata(block, 0, metadata, &icon);
                    if (e.registry.status == MC_REGISTRY_OK) {
                        mc_particle_fields *f = &p->particle->fields;
                        f->b = icon;
                        p->particle->item_native.item_pc = 36;
                        f->h = block->fields.bz;
                        p->particle->item_native.item_pc = 45;
                        f->k = .6f;
                        p->particle->item_native.item_pc = 54;
                        f->j = .6f;
                        p->particle->item_native.item_pc = 58;
                        f->i = .6f;
                        p->particle->item_native.item_pc = 61;
                        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
                            (void)mc_particle_constructor_fail(
                                &e, MC_ENTITY_UNSUPPORTED_FP, MC_PARTICLE_OWNER,
                                "QM reached size floating controls", "qm", "<init>", 70);
                        else {
                            f->g = mc_travel_fdiv(f->g, 2.0f);
                            p->particle->item_native.item_pc = 71;
                            p->particle->digging_face = face;
                            p->particle->item_native.item_pc = 77;
                            p->particle->item_native.item_pc = 80;
                        }
                    }
                }
                if (e.registry.status != MC_REGISTRY_OK) {
                    (void)mc_particle_constructor_fail(
                        &e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_REGISTRY,
                        "QM actual Block texture child", "qm", "<init>", 33);
                    mc_particle_registry_classification(registry, &e.registry, &e.throwable,
                                                        &e.exception_class);
                    e.native_policy = e.throwable == MC_PARTICLE_NO_THROWABLE;
                    e.source_exception = e.throwable == MC_PARTICLE_SOURCE_EXCEPTION;
                }
            }
        }
        if (e.source_class == NULL || strcmp(e.source_class, "qm") == 0)
            e.source_descriptor = "(Lfd;DDDDDDLuu;II)V";
        (void)mc_entity_constructor_finish(p, e.status, &e.entity);
        if (e.status == MC_ENTITY_OK) {
            e.source_class = "qm";
            e.source_method = "<init>";
            e.source_pc = e.item_pc = 80;
        }
    }
    mc_particle_constructor_error_publish(out, &e);
    return e.status;
}

mc_entity_status mc_digging_particle_construct(mc_entity *p, mc_world_runtime *world,
                                               mc_registry_classes *registry, mc_math_classes *math,
                                               double x, double y, double z, double vx, double vy,
                                               double vz, const mc_block *block, int32_t face,
                                               int32_t metadata, mc_particle_error *error) {
    return mc_digging_particle_construct_entered(p, world, registry, math, x, y, z, vx, vy, vz,
                                                 block, face, metadata, NULL, error);
}

static bool digging_ready(mc_entity *p, mc_particle_error *e, const char *method,
                          const char *descriptor) {
    mc_particle_error_init(e);
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE) || p->particle == NULL ||
        p->phase != MC_ENTITY_READY) {
        (void)digging_fail(e, MC_ENTITY_INVALID_ARGUMENT, "QM canonical READY receiver", method,
                           descriptor, 0, true);
        return false;
    }
    return true;
}

typedef struct digging_color_context {
    mc_world_runtime *world; /* Captured QM.aI17 XP argument, not a later field reread. */
    mc_client_render_engine *engine;
    mc_digging_color_error *child;
    mc_particle_error *error;
} digging_color_context;

static bool color_fail(digging_color_context *c, const char *operation, unsigned int pc,
                       bool native, const char *exception) {
    mc_particle_error *e = c->error;
    (void)digging_fail(e, MC_ENTITY_STATE_ERROR, operation, "b", "(Lxp;III)I", pc, native);
    e->source_class = "bk";
    e->item_pc = 23; /* Enclosing actual QM.a invocation. */
    e->exception_class = native ? NULL : exception;
    c->child->failed = true;
    c->child->block_pc = pc;
    return false;
}

static bool color_metadata(void *context, int32_t x, int32_t y, int32_t z, unsigned int pc,
                           int32_t *out) {
    digging_color_context *c = context;
    mc_digging_color_error *d = c->child;
    d->child = MC_DIGGING_COLOR_NONE;
    d->site = (mc_client_graphics_site){"fd", "e", "(III)I", 0};
    if (c->world == NULL)
        return color_fail(c, "BK actual XP metadata null receiver", pc, false,
                          "java/lang/NullPointerException");
    d->child = MC_DIGGING_COLOR_METADATA;
    d->acquisition_reached = true;
    if (mc_world_runtime_metadata_get_failure(c->world, x, y, z, out, &d->world, &d->acquisition) ==
        MC_WORLD_STORAGE_OK)
        return true;
    return color_fail(c, d->world.operation, pc, true, NULL);
}

static bool color_manager(digging_color_context *c, unsigned int pc, mc_world_chunk_manager **out) {
    mc_digging_color_error *d = c->child;
    d->child = MC_DIGGING_COLOR_NONE;
    if (c->world == NULL)
        return color_fail(c, "BK actual XP manager null receiver", pc, false,
                          "java/lang/NullPointerException");
    d->child = MC_DIGGING_COLOR_MANAGER;
    const mc_world_storage_status status =
        mc_world_runtime_chunk_manager(c->world, out, &d->manager);
    d->site = (mc_client_graphics_site){"fd", "a", "()Lxv;", (int32_t)d->manager.bytecode};
    return status == MC_WORLD_STORAGE_OK ||
           color_fail(c, d->manager.operation, pc, d->manager.native_policy,
                      "java/lang/NullPointerException");
}

static bool color_climate(void *context, int32_t x, int32_t z, unsigned int manager_pc,
                          unsigned int sample_pc) {
    digging_color_context *c = context;
    mc_world_chunk_manager *manager = NULL;
    if (!color_manager(c, manager_pc, &manager))
        return false;
    c->child->child = MC_DIGGING_COLOR_NONE;
    if (manager == NULL)
        return color_fail(c, "BK actual cached manager null receiver", sample_pc, false,
                          "java/lang/NullPointerException");
    c->child->child = MC_DIGGING_COLOR_CLIMATE;
    mc_biome_reference_array *ignored = NULL;
    if (mc_world_chunk_manager_cached(manager, x, z, 1, 1, &ignored, &c->child->climate) ==
        MC_WORLD_PROVIDER_OK)
        return true;
    const mc_world_chunk_manager_error *e = &c->child->climate;
    c->child->site = (mc_client_graphics_site){"xv", "a", "(IIII)[Lkd;", (int32_t)e->bytecode};
    c->error->registry = e->sampling.registry;
    const bool native = e->sampling.sampler_pc < 0 ||
                        (e->sampling.registry.status != MC_REGISTRY_NULL_REFERENCE &&
                         e->sampling.registry.status != MC_REGISTRY_INDEX_OUT_OF_RANGE);
    return color_fail(c, e->operation, sample_pc, native,
                      e->sampling.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE
                          ? "java/lang/ArrayIndexOutOfBoundsException"
                          : "java/lang/NullPointerException");
}

static bool color_channel(void *context, bool rainfall, unsigned int manager_pc,
                          unsigned int field_pc, unsigned int load_pc, double *out) {
    digging_color_context *c = context;
    mc_world_chunk_manager *manager = NULL;
    if (!color_manager(c, manager_pc, &manager))
        return false;
    c->child->child = MC_DIGGING_COLOR_CHANNEL;
    if (manager == NULL)
        return color_fail(c, "BK actual climate field null receiver", field_pc, false,
                          "java/lang/NullPointerException");
    mc_world_chunk_manager_fields fields;
    if (mc_world_chunk_manager_fields_get(manager, &fields) != MC_WORLD_PROVIDER_OK)
        return color_fail(c, "BK actual climate native fields", field_pc, true, NULL);
    const mc_biome_double_buffer *array = rainfall ? fields.b : fields.a;
    if (array == NULL || array->data == NULL)
        return color_fail(c, "BK actual climate array", load_pc, false,
                          "java/lang/NullPointerException");
    if (array->length == 0)
        return color_fail(c, "BK actual climate array index0", load_pc, false,
                          "java/lang/ArrayIndexOutOfBoundsException");
    memcpy(out, &array->data[0], sizeof(*out));
    return true;
}

static bool color_read(digging_color_context *c, mc_client_colorizer_read_kind read,
                       double temperature, double rainfall, unsigned int pc, int32_t *out) {
    mc_digging_color_error *d = c->child;
    d->child = MC_DIGGING_COLOR_NONE;
    /* ji is solely the native route to this runtime's actual static namespace.
     * It is consulted only after the reached XP metadata/climate operations. */
    mc_client_render_engine_classes *graphics = c->engine == NULL ? NULL : c->engine->classes;
    if (graphics == NULL || graphics->colorizer_read == NULL)
        return color_fail(c, "BK actual colorizer native namespace", pc, true, NULL);
    d->child = MC_DIGGING_COLOR_COLORIZER;
    if (graphics->colorizer_read(graphics, MC_CLIENT_COLORIZER_FOLIAGE, read, temperature, rainfall,
                                 out, &d->colorizer) == MC_CLIENT_COLORIZER_OK)
        return true;
    const mc_client_colorizer_error *e = &d->colorizer;
    d->site = e->site;
    (void)color_fail(c, e->operation, pc, e->native_policy,
                     e->array.status == MC_CLIENT_COLOR_ARRAY_INDEX_OUT_OF_RANGE
                         ? "java/lang/ArrayIndexOutOfBoundsException"
                         : "java/lang/NullPointerException");
    if (e->site.class_name != NULL && e->site.method != NULL &&
        strcmp(e->site.method, "<clinit>") != 0) {
        c->error->source_class = e->site.class_name;
        c->error->source_method = e->site.method;
        c->error->source_descriptor = e->site.descriptor;
        c->error->source_pc = (unsigned int)e->site.pc;
    }
    if (e->status == MC_CLIENT_COLORIZER_UNSUPPORTED_FP)
        c->error->status = MC_ENTITY_UNSUPPORTED_FP;
    return false;
}

static bool color_constant(void *c, bool pine, unsigned int pc, int32_t *out) {
    return color_read(c, pine ? MC_CLIENT_COLORIZER_PINE : MC_CLIENT_COLORIZER_BIRCH, 0, 0, pc,
                      out);
}

static bool color_lookup(void *c, double temperature, double rainfall, unsigned int pc,
                         int32_t *out) {
    return color_read(c, MC_CLIENT_COLORIZER_SAMPLE, temperature, rainfall, pc, out);
}

static void color_pc(void *context, unsigned int pc) {
    digging_color_context *c = context;
    c->child->block_pc = pc;
}

static const mc_leaf_color_ops color_ops = {color_metadata, color_climate, color_channel,
                                            color_constant, color_lookup,  color_pc};

mc_entity_status mc_digging_particle_tint_entered(mc_entity *p, mc_client_render_engine *engine,
                                                  int32_t x, int32_t y, int32_t z,
                                                  mc_digging_color_error *color,
                                                  mc_particle_error *e) {
    if (e == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (!digging_ready(p, e, "a", "(III)Lqm;"))
        return e->status;
    const mc_block *grass = NULL;
    if (mc_registry_block_static_get(p->particle->digging_registry, "v", &grass, &e->registry) !=
        MC_REGISTRY_OK) {
        (void)digging_fail(e, MC_ENTITY_STATE_ERROR, "QM actual Grass static child", "a",
                           "(III)Lqm;", 4, true);
        e->cause = MC_PARTICLE_REGISTRY;
        mc_particle_registry_classification(p->particle->digging_registry, &e->registry,
                                            &e->throwable, &e->exception_class);
        e->native_policy = e->throwable == MC_PARTICLE_NO_THROWABLE;
        e->source_exception = e->throwable == MC_PARTICLE_SOURCE_EXCEPTION;
        return e->status;
    }
    const mc_block *block = p->particle->digging_block;
    if (block == grass)
        return MC_ENTITY_OK;
    if (block == NULL)
        return digging_fail(e, MC_ENTITY_STATE_ERROR, "QM actual Block color null receiver", "a",
                            "(III)Lqm;", 23, false);
    int32_t value = 16777215;
    if (block->kind == MC_BLOCK_LEAVES) {
        if (color == NULL)
            return digging_fail(e, MC_ENTITY_STATE_ERROR, "QM native retained color child", "a",
                                "(III)Lqm;", 23, true);
        color->reached = true;
        color->failed = false;
        color->acquisition_reached = false;
        digging_color_context context = {p->base.world, engine, color, e};
        if (!mc_leaf_world_color(&color_ops, &context, x, y, z, &value))
            return e->status;
        color->value = value;
    } else if (block->kind == MC_BLOCK_TALL_GRASS)
        return digging_fail(e, MC_ENTITY_STATE_ERROR, "QM specialized Block World color child", "a",
                            "(III)Lqm;", 23, true);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return digging_fail(e, MC_ENTITY_UNSUPPORTED_FP, "QM tint floating controls", "a",
                            "(III)Lqm;", 45, true);
    const uint32_t bits = (uint32_t)value;
    p->particle->fields.i =
        mc_travel_fmul(p->particle->fields.i, mc_travel_fdiv((float)((bits >> 16u) & 255u), 255.f));
    p->particle->fields.j =
        mc_travel_fmul(p->particle->fields.j, mc_travel_fdiv((float)((bits >> 8u) & 255u), 255.f));
    p->particle->fields.k =
        mc_travel_fmul(p->particle->fields.k, mc_travel_fdiv((float)(bits & 255u), 255.f));
    e->source_class = "qm";
    e->source_method = "a";
    e->source_descriptor = "(III)Lqm;";
    e->source_pc = 92;
    return MC_ENTITY_OK;
}

mc_entity_status mc_digging_particle_tint(mc_entity *p, int32_t x, int32_t y, int32_t z,
                                          mc_particle_error *e) {
    return mc_digging_particle_tint_entered(p, NULL, x, y, z, NULL, e);
}

mc_entity_status mc_digging_particle_velocity_scale(mc_entity *p, float scale,
                                                    mc_particle_error *e) {
    if (e == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (!digging_ready(p, e, "c", "(F)Lxw;"))
        return e->status;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return digging_fail(e, MC_ENTITY_UNSUPPORTED_FP, "XW velocity scale floating controls", "c",
                            "(F)Lxw;", 7, true);
    p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, (double)scale);
    p->base.velocity_y = mc_travel_dadd(
        mc_travel_dmul(mc_travel_dsub(p->base.velocity_y, 0.10000000149011612), (double)scale),
        0.10000000149011612);
    p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, (double)scale);
    e->source_class = "xw";
    e->source_method = "c";
    e->source_descriptor = "(F)Lxw;";
    e->source_pc = 42;
    return MC_ENTITY_OK;
}

mc_entity_status mc_digging_particle_size_scale(mc_entity *p, float scale, mc_particle_error *e) {
    if (e == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (!digging_ready(p, e, "d", "(F)Lxw;"))
        return e->status;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return digging_fail(e, MC_ENTITY_UNSUPPORTED_FP, "XW size scale floating controls", "d",
                            "(F)Lxw;", 4, true);
    const float width = mc_travel_fmul(.2f, scale), height = mc_travel_fmul(.2f, scale);
    const mc_entity_status sized = mc_entity_set_size(p, width, height);
    if (sized != MC_ENTITY_OK)
        return digging_fail(e, sized, "XW actual SN size child", "d", "(F)Lxw;", 9, true);
    p->particle->fields.g = mc_travel_fmul(p->particle->fields.g, scale);
    e->source_class = "xw";
    e->source_method = "d";
    e->source_descriptor = "(F)Lxw;";
    e->source_pc = 23;
    return MC_ENTITY_OK;
}

mc_entity_status mc_digging_particle_tick(mc_entity *p, mc_registry_classes *registry,
                                          const mc_entity_move_limits *limits,
                                          mc_item_particle_tick_error *error) {
    return mc_particle_tick_inherited(p, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE, registry, limits,
                                      error);
}

mc_entity_status mc_digging_particle_layer(const mc_entity *p, int32_t *out) {
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = 1;
    return MC_ENTITY_OK;
}

void mc_digging_particle_dispose(mc_entity *p) {
    if (p->particle->digging_registry != NULL)
        (void)mc_registry_runtime_borrow_release(p->particle->digging_registry);
}
