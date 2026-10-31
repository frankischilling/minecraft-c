#include "../../block/registry/registry_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../entity_internal.h"
#include "../living/fields_internal.h"
#include "../mob/internal.h"
#include "../player/tick_math_internal.h"
#include "internal.h"
#include "mc/core/java/java.h"
#include <string.h>

static mc_entity_render_site site(const char *c, const char *m, const char *d, int32_t pc) {
    return (mc_entity_render_site){c, m, d, pc};
}

void mc_entity_render_error_init(mc_entity_render_error *e) {
    static const mc_entity_render_error zero = {0};
    if (e == NULL)
        return;
    *e = zero;
    e->site.pc = e->child.pc = -1;
    e->watcher_index = -1;
    e->math.source_pc = -1;
    mc_world_movement_error_init(&e->area);
    e->brightness.query.registry = e->area.registry;
}

mc_entity_status mc_entity_render_worlds_release(mc_entity_render_error *e) {
    if (e == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    for (size_t i = 0; i < 2; ++i) {
        mc_world_runtime *world = e->world_reads[i];
        if (world == NULL)
            continue;
        if (world->persistent_bindings == 0)
            return MC_ENTITY_STATE_ERROR;
        /* This primitive cannot refuse a proved positive native occurrence. */
        mc_world_runtime_binding_release(world);
        e->world_reads[i] = NULL;
    }
    return MC_ENTITY_OK;
}

static bool fail(mc_entity_render_error *e, mc_entity_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    e->native_policy = e->throwable == MC_ENTITY_RENDER_NO_THROWABLE;
    return false;
}

static bool exception(mc_entity_render_error *e, const char *name, const char *operation) {
    e->throwable = MC_ENTITY_RENDER_SOURCE_EXCEPTION;
    e->source_exception = true;
    e->exception_class = name;
    return fail(e, MC_ENTITY_STATE_ERROR, operation);
}

static bool start(mc_entity *p, const void *out, mc_entity_render_error *e, const char *c,
                  const char *m, const char *d) {
    if (e == NULL)
        return false;
    mc_entity_render_error_init(e);
    e->site = site(c, m, d, 0);
    if (p == NULL || out == NULL || p->classes == NULL || p->class_literal == NULL ||
        p->classes->source != MC_ENTITY_CLIENT || !mc_entity_is_kind(p, p->class_literal->kind))
        return fail(e, MC_ENTITY_INVALID_ARGUMENT, "entered Entity render storage/output");
    return true;
}

static bool floating(mc_entity_render_error *e) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_ENTITY_UNSUPPORTED_FP, "Entity render strict floating controls");
}

static bool math_floor(mc_math_classes *math, double value, int32_t *out, int32_t pc,
                       mc_entity_render_error *e) {
    e->site.pc = pc;
    e->selected_child = MC_ENTITY_RENDER_CHILD_MATH;
    if (math == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Entity render reached Math namespace");
    const mc_math_class_status status = mc_math_classes_ensure(math, &e->math);
    if (status != MC_MATH_CLASS_OK)
        return fail(e,
                    status == MC_MATH_CLASS_LIMIT               ? MC_ENTITY_LIMIT
                    : status == MC_MATH_CLASS_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                    : status == MC_MATH_CLASS_UNSUPPORTED_FP    ? MC_ENTITY_UNSUPPORTED_FP
                                                                : MC_ENTITY_STATE_ERROR,
                    "Entity render actual Math active use");
    if (!floating(e))
        return false;
    *out = mc_math_floor_double(value);
    e->selected_child = MC_ENTITY_RENDER_CHILD_NONE;
    return true;
}

static bool hold_world(mc_entity_render_error *e, mc_world_runtime *world) {
    if (e->world_read_count == 2)
        return fail(e, MC_ENTITY_LIMIT, "Entity render fixed World occurrence extent");
    const size_t slot = e->world_read_count++;
    e->world_identities[slot] = (uintptr_t)world;
    if (!mc_world_runtime_binding_retain(world))
        return fail(
            e, world->persistent_bindings == SIZE_MAX ? MC_ENTITY_LIMIT : MC_ENTITY_WORLD_FAILED,
            "Entity render reached World native occurrence");
    e->world_reads[slot] = world;
    return true;
}

static const char *registry_exception(mc_registry_status status) {
    switch (status) {
        case MC_REGISTRY_NULL_REFERENCE:
            return "java/lang/NullPointerException";
        case MC_REGISTRY_INDEX_OUT_OF_RANGE:
            return "java/lang/ArrayIndexOutOfBoundsException";
        case MC_REGISTRY_ILLEGAL_ARGUMENT:
        case MC_REGISTRY_DUPLICATE_BLOCK:
        case MC_REGISTRY_DUPLICATE_STATISTIC:
            return "java/lang/IllegalArgumentException";
        default:
            return NULL;
    }
}

static bool same_registry(const mc_registry_error *a, const mc_registry_error *b, bool cached) {
    return (cached ? a->status == MC_REGISTRY_FAILED_INITIALIZATION : a->status == b->status) &&
           a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc && a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

void mc_entity_render_registry_classification(mc_registry_classes *c, const mc_registry_error *r,
                                              mc_entity_render_throwable *kind, const char **name) {
    *kind = MC_ENTITY_RENDER_NO_THROWABLE;
    *name = NULL;
    const bool cached = r->status == MC_REGISTRY_FAILED_INITIALIZATION;
    const mc_registry_error *first = NULL;
    if (c != NULL) {
        if (c->blocks_phase == MC_REGISTRY_FAILED &&
            c->block_states[MC_BLOCK_BASE].phase == MC_REGISTRY_FAILED &&
            same_registry(&c->block_failure, &c->block_states[MC_BLOCK_BASE].failure, false) &&
            same_registry(r, &c->block_failure, cached))
            first = &c->block_failure;
        if (first == NULL) {
            for (size_t i = 0; i < (size_t)MC_BLOCK_KIND_COUNT; ++i)
                if (c->block_states[i].phase == MC_REGISTRY_FAILED &&
                    same_registry(r, &c->block_states[i].failure, cached)) {
                    first = &c->block_states[i].failure;
                    break;
                }
        }
    }
    if (first != NULL && registry_exception(first->status) != NULL) {
        *kind = MC_ENTITY_RENDER_SOURCE_ERROR;
        *name = cached ? "java/lang/NoClassDefFoundError" : "java/lang/ExceptionInInitializerError";
    } else if (!cached && r->active_initializer_class == NULL &&
               registry_exception(r->status) != NULL) {
        *kind = MC_ENTITY_RENDER_SOURCE_EXCEPTION;
        *name = registry_exception(r->status);
    }
}

/* Actual ud.a(I)B over its existing typed map. No boxed Object is fabricated. */
static bool watch(mc_entity *p, int32_t key, int32_t outer_pc, int8_t *out,
                  mc_entity_render_error *e) {
    e->site.pc = outer_pc;
    e->watcher_index = key;
    e->child = site("ud", "a", "(I)B", 8);
    if (p->watcher == NULL) {
        e->child.pc = 0;
        return exception(e, "java/lang/NullPointerException",
                         "Entity render null watcher receiver");
    }
    const mc_entity_watch_map *map = p->watcher->map;
    if (map == NULL) {
        e->watcher_pc = 8;
        return exception(e, "java/lang/NullPointerException", "Entity render null watcher Map");
    }
    const mc_entity_watch_node *node = map->head;
    size_t visited = 0;
    while (node != NULL) {
        if (visited++ == map->size)
            return fail(e, MC_ENTITY_STATE_ERROR, "Entity render watcher map ownership");
        if (node->key == key)
            break;
        node = node->next;
    }
    const mc_entity_watch_record *r = node == NULL ? NULL : node->record;
    if (r == NULL) {
        e->watcher_pc = 16;
        e->child.pc = 16;
        return exception(e, "java/lang/NullPointerException", "Entity render null watcher record");
    }
    if (r->type != 0) {
        e->watcher_pc = 19;
        e->child.pc = 19;
        if (r->type == 4 && r->string_value == NULL) {
            e->watcher_pc = 22;
            e->child.pc = 22;
            return exception(e, "java/lang/NullPointerException",
                             "Entity render null watcher Byte unbox");
        }
        if (r->type == 2 || r->type == 4)
            return exception(e, "java/lang/ClassCastException",
                             "Entity render actual non-Byte watcher value");
        return fail(e, MC_ENTITY_STATE_ERROR, "Entity render unrepresented watcher value");
    }
    *out = r->byte_value;
    return true;
}

bool mc_entity_render_swing(mc_entity *p, float partial, float *out, mc_entity_render_error *e) {
    if (!start(p, out, e, "ls", "d", "(F)F"))
        return false;
    mc_living_entity_fields *ls = mc_entity_living_fields(p);
    if (ls == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Entity render actual Living storage");
    e->site.pc = 8;
    if (!floating(e))
        return false;
    float delta = at_fsub(ls->X, ls->W);
    if (delta < 0.0f)
        delta = at_fadd(delta, 1.0f);
    *out = at_fadd(ls->W, at_fmul(delta, partial));
    e->site.pc = 28;
    return true;
}

bool mc_entity_render_riding(mc_entity *p, bool *out, mc_entity_render_error *e) {
    if (!start(p, out, e, "sn", "al", "()Z"))
        return false;
    if (p->base.aH != NULL) {
        *out = true;
        return true;
    }
    int8_t flags;
    if (!watch(p, 0, 9, &flags, e)) {
        e->child = site("sn", "d", "(I)Z", 5);
        return false;
    }
    *out = ((int32_t)flags & 4) != 0;
    e->site.pc = 20;
    return true;
}

bool mc_entity_render_burning(mc_entity *p, bool *out, mc_entity_render_error *e) {
    if (!start(p, out, e, "sn", "ak", "()Z"))
        return false;
    if (p->base.bv > 0) {
        *out = true;
        return true;
    }
    int8_t flags;
    if (!watch(p, 0, 9, &flags, e)) {
        e->child = site("sn", "d", "(I)Z", 5);
        return false;
    }
    *out = ((int32_t)flags & 1) != 0;
    e->site.pc = 20;
    return true;
}

bool mc_entity_render_half_height(mc_entity *p, float *out, mc_entity_render_error *e) {
    if (!start(p, out, e, "sn", "x_", "()F"))
        return false;
    e->site.pc = 5;
    if (!floating(e))
        return false;
    *out = at_fdiv(p->base.height, 2.0f);
    e->site.pc = 6;
    return true;
}

bool mc_entity_render_texture(mc_entity *p, const mc_player_string **out,
                              mc_entity_render_error *e) {
    if (!start(p, out, e, "ls", "q_", "()Ljava/lang/String;"))
        return false;
    if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_WOLF)) {
        e->site = site("gi", "q_", "()Ljava/lang/String;", 0);
        return fail(e, MC_ENTITY_STATE_ERROR, "Entity render Wolf skin virtual child");
    }
    mc_living_entity_fields *ls = mc_entity_living_fields(p);
    if (ls == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Entity render actual Living texture storage");
    *out = ls->O;
    e->site.pc = 4;
    return true;
}

bool mc_entity_render_sheep_sheared(mc_entity *p, bool *out, mc_entity_render_error *e) {
    if (!start(p, out, e, "dl", "s", "()Z"))
        return false;
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SHEEP))
        return fail(e, MC_ENTITY_STATE_ERROR, "Entity render actual Sheep receiver");
    int8_t value;
    if (!watch(p, 16, 6, &value, e))
        return false;
    *out = ((int32_t)value & 16) != 0;
    e->site.pc = 20;
    return true;
}

bool mc_entity_render_sheep_color(mc_entity *p, int32_t *out, mc_entity_render_error *e) {
    if (!start(p, out, e, "dl", "r", "()I"))
        return false;
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SHEEP))
        return fail(e, MC_ENTITY_STATE_ERROR, "Entity render actual Sheep receiver");
    int8_t value;
    if (!watch(p, 16, 6, &value, e))
        return false;
    *out = (int32_t)value & 15;
    e->site.pc = 12;
    return true;
}

bool mc_entity_render_pig_saddled(mc_entity *p, bool *out, mc_entity_render_error *e) {
    if (!start(p, out, e, "wh", "v", "()Z"))
        return false;
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PIG))
        return fail(e, MC_ENTITY_STATE_ERROR, "Entity render actual Pig receiver");
    int8_t value;
    if (!watch(p, 16, 6, &value, e))
        return false;
    *out = ((int32_t)value & 1) != 0;
    e->site.pc = 19;
    return true;
}

static mc_entity_render_query *query_begin(mc_entity_render_error *e, mc_world_runtime *world,
                                           const char *method, const char *descriptor,
                                           int32_t caller_pc, const int32_t *args, size_t nargs,
                                           mc_entity_render_query_kind kind) {
    mc_entity_render_query *q = &e->queries[e->query_count++];
    *q = (mc_entity_render_query){.receiver = (uintptr_t)world,
                                  .method = {"fd", method, descriptor, 0},
                                  .caller = {"sn", "a", "(F)F", caller_pc},
                                  .nargs = nargs,
                                  .kind = kind};
    memcpy(q->args, args, nargs * sizeof(*args));
    return q;
}

static void query_failed(mc_entity_render_query *q, const mc_entity_render_error *e) {
    q->failed = true;
    q->native_policy = e->native_policy;
    q->source_exception = e->source_exception;
    q->exception_class = e->exception_class;
}

bool mc_entity_render_brightness(mc_entity *p, mc_math_classes *math, float partial, float *out,
                                 mc_entity_render_error *e) {
    (void)partial;
    if (!start(p, out, e, "sn", "a", "(F)F"))
        return false;
    int32_t x, y, z, bounds[6];
    if (!math_floor(math, p->base.x, &x, 4, e))
        return false;
    e->site.pc = 12;
    if (p->bounds == NULL)
        return exception(e, "java/lang/NullPointerException",
                         "Entity render brightness null bounds");
    if (!floating(e))
        return false;
    const double h = at_mul(at_sub(p->bounds->max_y, p->bounds->min_y), .66);
    if (!math_floor(math, at_add(at_sub(p->base.y, at_widen(p->base.offset)), h), &y, 40, e) ||
        !math_floor(math, p->base.z, &z, 49, e))
        return false;
    mc_world_runtime *world = p->base.world; /* Captured receiver54, before six arguments. */
    static const int32_t read_pc[6] = {62, 72, 82, 92, 102, 112};
    static const int32_t floor_pc[6] = {65, 75, 85, 95, 105, 115};
    for (size_t i = 0; i < 6; ++i) {
        e->site.pc = read_pc[i];
        if (p->bounds == NULL)
            return exception(e, "java/lang/NullPointerException",
                             "Entity render brightness bounds reload");
        const double v = i == 0   ? p->bounds->min_x
                         : i == 1 ? p->bounds->min_y
                         : i == 2 ? p->bounds->min_z
                         : i == 3 ? p->bounds->max_x
                         : i == 4 ? p->bounds->max_y
                                  : p->bounds->max_z;
        if (!math_floor(math, v, &bounds[i], floor_pc[i], e))
            return false;
    }
    e->site.pc = 118;
    mc_entity_render_query *area_query =
        query_begin(e, world, "a", "(IIIIII)Z", 118, bounds, 6u, MC_ENTITY_RENDER_QUERY_BOOLEAN);
    if (world == NULL) {
        (void)exception(e, "java/lang/NullPointerException",
                        "Entity render brightness null area World");
        query_failed(area_query, e);
        return false;
    }
    if (!hold_world(e, world)) {
        query_failed(area_query, e);
        return false;
    }
    bool loaded;
    e->selected_child = MC_ENTITY_RENDER_CHILD_AREA;
    area_query->invoked = true;
    if (mc_world_runtime_area_loaded(world, bounds[0], bounds[1], bounds[2], bounds[3], bounds[4],
                                     bounds[5], &loaded, &e->area) != MC_WORLD_STORAGE_OK) {
        e->child = site("fd", "a", "(IIIIII)Z", (int32_t)e->area.bytecode);
        (void)fail(e, MC_ENTITY_WORLD_FAILED, "Entity render actual area presence");
        if (e->area.provider_bytecode == 6 && world->fields.v == NULL) {
            e->child = site("fd", "g", "(II)Z", 6);
            (void)exception(e, "java/lang/NullPointerException", e->operation);
        }
        query_failed(area_query, e);
        return false;
    }
    area_query->returned = true;
    area_query->value.boolean = loaded;
    if (!loaded) {
        *out = p->base.bE;
        e->site.pc = 161;
        return true;
    }
    world = p->base.world; /* Source reload125, after the area call returned. */
    e->site.pc = 133;
    const int32_t coordinates[] = {x, y, z};
    mc_entity_render_query *brightness_query = query_begin(
        e, world, "c", "(III)F", 133, coordinates, 3u, MC_ENTITY_RENDER_QUERY_FLOAT_BITS);
    if (world == NULL) {
        (void)exception(e, "java/lang/NullPointerException",
                        "Entity render brightness null fresh World");
        query_failed(brightness_query, e);
        return false;
    }
    if (!hold_world(e, world)) {
        query_failed(brightness_query, e);
        return false;
    }
    e->selected_child = MC_ENTITY_RENDER_CHILD_BRIGHTNESS;
    float value;
    brightness_query->invoked = true;
    if (mc_world_runtime_brightness(world, x, y, z, &value, &e->brightness) !=
        MC_WORLD_STORAGE_OK) {
        e->child = site("fd", "c", "(III)F", (int32_t)e->brightness.bytecode);
        (void)fail(e, MC_ENTITY_WORLD_FAILED, "Entity render actual World brightness");
        if (e->brightness.source_exception == MC_WORLD_BRIGHTNESS_SOURCE_NULL_DIMENSION ||
            e->brightness.source_exception == MC_WORLD_BRIGHTNESS_SOURCE_NULL_TABLE)
            (void)exception(e, "java/lang/NullPointerException", e->operation);
        else if (e->brightness.source_exception == MC_WORLD_BRIGHTNESS_SOURCE_INDEX)
            (void)exception(e, "java/lang/ArrayIndexOutOfBoundsException", e->operation);
        else {
            if (e->brightness.query.provider_bytecode == 6 && world->fields.v == NULL)
                (void)exception(e, "java/lang/NullPointerException", e->operation);
            if (e->throwable == MC_ENTITY_RENDER_NO_THROWABLE)
                mc_entity_render_registry_classification(mc_world_runtime_registry(world),
                                                         &e->brightness.query.registry,
                                                         &e->throwable, &e->exception_class);
            if (e->throwable != MC_ENTITY_RENDER_NO_THROWABLE) {
                e->source_exception = true;
                e->native_policy = false;
            }
        }
        query_failed(brightness_query, e);
        return false;
    }
    brightness_query->returned = true;
    memcpy(&brightness_query->value.float_bits, &value, sizeof(value));
    if (!floating(e))
        return false;
    /* JVM fcmpg treats an unordered result as greater, preserving value. */
    if (value < p->base.bE)
        value = p->base.bE;
    *out = value;
    e->site.pc = 156;
    return true;
}
