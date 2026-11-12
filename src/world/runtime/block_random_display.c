#include "mc/world/runtime/block_random_display.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../entity/player/tick_math_internal.h"
#include "block_queries_internal.h"
#include "collision/blocks_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mutation/mutation_internal.h"
#include "step_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(MC_BLOCK_KIND_COUNT == 69, "audit new client random-display inheritance");

typedef struct display_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_random_display_block_error *error;
    bool entered;
} display_frame;

static mc_world_random_display_block_site site(const char *cl, const char *method,
                                               const char *descriptor, unsigned int pc) {
    return (mc_world_random_display_block_site){cl, method, descriptor, pc};
}

void mc_world_random_display_block_error_init(mc_world_random_display_block_error *error) {
    if (error == NULL)
        return;
    memset(error, 0, sizeof(*error));
    error->registry = mc_world_mutation_empty_registry();
}

static void at(display_frame *f, unsigned int pc) {
    f->error->site.pc = pc;
    f->error->child_site = (mc_world_random_display_block_site){0};
    f->error->child = MC_WORLD_RANDOM_DISPLAY_BLOCK_NONE;
}

static bool fail(display_frame *f, mc_world_storage_status status, const char *operation) {
    f->error->status = status;
    f->error->operation = operation;
    f->error->native_policy = f->error->exception_class == NULL;
    return false;
}

static bool null_reference(display_frame *f, const char *operation) {
    f->error->exception_class = "java/lang/NullPointerException";
    return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
}

static const char *registry_throwable(mc_registry_status status) {
    if (status == MC_REGISTRY_NULL_REFERENCE)
        return "java/lang/NullPointerException";
    if (status == MC_REGISTRY_INDEX_OUT_OF_RANGE)
        return "java/lang/ArrayIndexOutOfBoundsException";
    if (status == MC_REGISTRY_ILLEGAL_ARGUMENT || status == MC_REGISTRY_DUPLICATE_BLOCK ||
        status == MC_REGISTRY_DUPLICATE_STATISTIC)
        return "java/lang/IllegalArgumentException";
    return NULL;
}

static bool same_registry_failure(const mc_registry_error *a, const mc_registry_error *b,
                                  bool cached) {
    return (a->status == b->status || (cached && a->status == MC_REGISTRY_FAILED_INITIALIZATION)) &&
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

/* Qualify a latched initializer only against this namespace's actual matching
 * immutable first cause. Native LIMIT/ALLOC/dependency latches stay native. */
static void classify_registry(display_frame *f, mc_registry_status returned) {
    mc_world_random_display_block_error *e = f->error;
    const mc_registry_classes *classes = f->block->classes;
    const bool cached = returned == MC_REGISTRY_FAILED_INITIALIZATION ||
                        e->registry.status == MC_REGISTRY_FAILED_INITIALIZATION;
    const mc_registry_error *first = NULL;
    if (classes->blocks_phase == MC_REGISTRY_FAILED &&
        classes->block_states[MC_BLOCK_BASE].phase == MC_REGISTRY_FAILED &&
        same_registry_failure(&classes->block_failure,
                              &classes->block_states[MC_BLOCK_BASE].failure, false) &&
        same_registry_failure(&e->registry, &classes->block_failure, cached))
        first = &classes->block_failure;
    else {
        for (size_t i = 0; i < (size_t)MC_BLOCK_KIND_COUNT; ++i) {
            const mc_registry_class_state *state = &classes->block_states[i];
            if (state->phase == MC_REGISTRY_FAILED &&
                same_registry_failure(&e->registry, &state->failure, cached)) {
                first = &state->failure;
                break;
            }
        }
    }
    if (first != NULL) {
        if (registry_throwable(first->status) != NULL)
            e->exception_class =
                cached ? "java/lang/NoClassDefFoundError" : "java/lang/ExceptionInInitializerError";
    } else if (!cached && e->registry.active_initializer_class == NULL)
        e->exception_class = registry_throwable(returned);
}

static bool registry_failure(display_frame *f, mc_registry_status status, const char *operation) {
    mc_world_random_display_block_error *e = f->error;
    classify_registry(f, status);
    if (e->registry.active_initializer_class != NULL && e->registry.active_initializer_pc >= 0)
        e->child_site = site(e->registry.active_initializer_class, "<clinit>", "()V",
                             (unsigned int)e->registry.active_initializer_pc);
    return fail(f, mc_world_runtime_registry_status(status), operation);
}

static void classify_access(display_frame *f, const mc_world_access_failure *failure) {
    const char *exception = NULL;
    if (mc_world_access_failure_source_exception(failure, &exception))
        f->error->exception_class = exception;
}

static void classify_query(display_frame *f, const mc_world_nearby_error *lower) {
    classify_registry(f, lower->query.registry.status);
    /* The held getter has reached this exact FD.c receiver check, after its
     * original coordinate guards. No method-name/diagnostic-string inference. */
    if (lower->query.provider_bytecode == 6 && lower->query.acquisition == MC_CHUNK_ACQUIRE_OK &&
        f->world->fields.v == NULL) {
        f->error->exception_class = "java/lang/NullPointerException";
        f->error->child_site = site("fd", "c", "(II)Llm;", 6);
    }
    classify_access(f, &lower->batch.publication.world_access_failure);
    classify_access(f, &lower->tile_entity.world_access_failure);
    classify_access(f, &lower->tile_entity.publication.world_access_failure);
    classify_access(f, &lower->tile_entity.batch.publication.world_access_failure);
}

/* Enter only at a reached World dereference. Pure inherited bodies and own
 * false-flag returns do not preflight either nullable source argument. */
static bool world_enter(display_frame *f) {
    if (f->world == NULL)
        return null_reference(f, "Block random-display null World receiver");
    if (f->entered)
        return true;
    const mc_world_storage_status status = mc_world_runtime_call_enter(f->world, &f->error->cause);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(f, status, "Block random-display live GAME World");
    f->entered = true;
    if (mc_world_runtime_registry(f->world) != f->block->classes)
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Block random-display World/code Registry namespace");
    return true;
}

static bool fp(display_frame *f, unsigned int pc) {
    at(f, pc);
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block random-display strict FP controls");
}

static float integer_float(int32_t value) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), value));
}

static double float_center(int32_t value, float offset) {
    return at_widen(at_fadd(integer_float(value), offset));
}

static bool draw_int(display_frame *f, unsigned int pc, mc_java_random *random, int32_t bound,
                     int32_t *out) {
    at(f, pc);
    if (random == NULL)
        return null_reference(f, "Block random-display null supplied Random");
    return mc_java_random_next_bounded(random, bound, out) ||
           fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block random-display Random bound");
}

static bool draw_float(display_frame *f, unsigned int pc, mc_java_random *random, float *out) {
    at(f, pc);
    if (random == NULL)
        return null_reference(f, "Block random-display null supplied Random");
    *out = mc_java_random_next_float(random);
    return true;
}

static bool get(display_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z, bool metadata,
                int32_t *out) {
    at(f, pc);
    if (!world_enter(f))
        return false;
    f->error->child = MC_WORLD_RANDOM_DISPLAY_BLOCK_QUERY;
    mc_world_collision_error_init(&f->error->detail.query);
    const mc_world_storage_status status = mc_world_collision_block_value_held(
        f->world, x, y, z, metadata, out, &f->error->detail.query);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    const mc_world_query_error *lower = &f->error->detail.query.nearby.query;
    f->error->registry = lower->registry;
    f->error->cause = lower->cause;
    f->error->child_site = site("fd", metadata ? "e" : "a", "(III)I", lower->bytecode);
    classify_query(f, &f->error->detail.query.nearby);
    return fail(f, status, lower->operation);
}

/* Actual FD.f: ID query before air identity or the separate Block.m capture.
 * Its acquiring child remains complete, rather than reconstructed from World. */
static bool material(display_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                     mc_material **out) {
    at(f, pc);
    if (!world_enter(f))
        return false;
    mc_world_random_display_block_error *e = f->error;
    e->child = MC_WORLD_RANDOM_DISPLAY_BLOCK_MATERIAL;
    memset(&e->detail.material.error, 0, sizeof(e->detail.material.error));
    e->detail.material.error.registry = mc_world_mutation_empty_registry();
    mc_world_collision_error_init(&e->detail.material.access);
    mc_world_material_error *m = &e->detail.material.error;
    int32_t id;
    m->bytecode = 4;
    mc_world_storage_status status = mc_world_collision_block_value_held(
        f->world, x, y, z, false, &id, &e->detail.material.access);
    if (status != MC_WORLD_STORAGE_OK) {
        const mc_world_query_error *q = &e->detail.material.access.nearby.query;
        m->status = status;
        m->operation = q->operation;
        m->cause = e->cause = q->cause;
        m->registry = e->registry = q->registry;
        e->child_site = site("fd", "f", "(III)Lln;", 4);
        classify_query(f, &e->detail.material.access.nearby);
        return fail(f, status, q->operation);
    }
    mc_registry_status rs;
    if (id == 0) {
        m->bytecode = 14;
        rs = mc_registry_material_get(f->block->classes, MC_MATERIAL_a, out, &e->registry);
    } else {
        mc_registry_block_m_view array;
        const mc_block *selected;
        m->bytecode = 18;
        rs = mc_registry_block_m_get(f->block->classes, &array, &e->registry);
        if (rs == MC_REGISTRY_OK) {
            m->bytecode = 23;
            rs = mc_registry_block_m_at(&array, id, &selected, &e->registry);
            if (rs == MC_REGISTRY_OK) {
                m->bytecode = 24;
                if (selected == NULL) {
                    m->status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
                    m->operation = "Display Material null Block field receiver";
                    e->child_site = site("fd", "f", "(III)Lln;", 24);
                    return null_reference(f, m->operation);
                }
                *out = selected->fields.bA;
            }
        }
    }
    if (rs == MC_REGISTRY_OK)
        return true;
    m->registry = e->registry;
    m->status = mc_world_runtime_registry_status(rs);
    m->operation = "Display actual World Material Registry access";
    e->child_site = site("fd", "f", "(III)Lln;", m->bytecode);
    return registry_failure(f, rs, m->operation);
}

static bool material_identity(display_frame *f, unsigned int pc, mc_material_id id,
                              mc_material **out) {
    at(f, pc);
    const mc_registry_status status =
        mc_registry_material_get(f->block->classes, id, out, &f->error->registry);
    return status == MC_REGISTRY_OK ||
           registry_failure(f, status, "Display shared Material identity");
}

static bool opaque(display_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z, bool *out) {
    at(f, pc);
    if (!world_enter(f))
        return false;
    mc_world_random_display_block_error *e = f->error;
    e->child = MC_WORLD_RANDOM_DISPLAY_BLOCK_STEP;
    mc_world_step_error_init(&e->detail.step);
    mc_world_step_source_failure source;
    if (mc_world_step_opaque_held(f->world, x, y, z, out, &e->detail.step, &source))
        return true;
    e->registry = e->detail.step.registry;
    e->cause = e->detail.step.cause;
    e->child_site = site("fd", "g", "(III)Z", e->detail.step.opaque_bytecode);
    if (source.kind == MC_WORLD_STEP_SOURCE_REGISTRY)
        return registry_failure(f, source.registry_status, e->detail.step.operation);
    if (source.kind == MC_WORLD_STEP_SOURCE_QUERY) {
        e->registry = e->detail.step.block_read.nearby.query.registry;
        e->cause = e->detail.step.block_read.nearby.query.cause;
        classify_query(f, &e->detail.step.block_read.nearby);
    }
    return fail(f, e->detail.step.status, e->detail.step.operation);
}

static bool support(display_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z, bool *out) {
    at(f, pc);
    if (!world_enter(f))
        return false;
    mc_world_solid_support_detail lower;
    const mc_world_storage_status status =
        mc_world_solid_support_detailed_held(f->world, x, y, z, out, &lower);
    f->error->child = MC_WORLD_RANDOM_DISPLAY_BLOCK_SUPPORT;
    f->error->detail.support.error = lower.error;
    f->error->detail.support.access = lower.access;
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    f->error->cause = lower.error.cause;
    f->error->registry = lower.error.registry;
    f->error->child_site = site("fd", "h", "(III)Z", lower.error.bytecode);
    classify_query(f, &lower.access);
    if (lower.error.registry.status != MC_REGISTRY_OK)
        return registry_failure(f, lower.error.registry.status, lower.error.operation);
    return fail(f, status, lower.error.operation);
}

static bool literal(display_frame *f, unsigned int pc, const char *text,
                    const mc_registry_string **out) {
    at(f, pc);
    const mc_registry_status status =
        mc_registry_runtime_literal_ascii(f->block->classes, text, out, &f->error->registry);
    return status == MC_REGISTRY_OK ||
           registry_failure(f, status, "Display actual code String literal");
}

static bool presentation_failure(display_frame *f, bool sound) {
    mc_world_mutation_error *m = &f->error->detail.presentation;
    f->error->registry = m->registry;
    f->error->cause = m->cause;
    f->error->child_site =
        site("fd", "a", sound ? "(DDDLjava/lang/String;FF)V" : "(Ljava/lang/String;DDDDDD)V",
             m->world_access_bytecode);
    classify_registry(f, m->registry.status);
    classify_access(f, &m->world_access_failure);
    return fail(f, m->status, m->operation);
}

static bool particle(display_frame *f, unsigned int literal_pc, unsigned int call_pc,
                     const char *text, double x, double y, double z, double vx, double vy,
                     double vz) {
    const mc_registry_string *name;
    if (!literal(f, literal_pc, text, &name))
        return false;
    at(f, call_pc);
    if (!world_enter(f))
        return false;
    f->error->child = MC_WORLD_RANDOM_DISPLAY_BLOCK_PRESENTATION;
    mc_world_mutation_error_init(&f->error->detail.presentation);
    if (!mc_world_mutation_particle(f->world, name, x, y, z, vx, vy, vz,
                                    &f->error->detail.presentation))
        return presentation_failure(f, false);
    return true;
}

static bool sound(display_frame *f, unsigned int call_pc, const mc_registry_string *name, double x,
                  double y, double z, float volume, float pitch) {
    at(f, call_pc);
    if (!world_enter(f))
        return false;
    f->error->child = MC_WORLD_RANDOM_DISPLAY_BLOCK_PRESENTATION;
    mc_world_mutation_error_init(&f->error->detail.presentation);
    if (!mc_world_mutation_sound(f->world, x, y, z, name, volume, pitch,
                                 &f->error->detail.presentation))
        return presentation_failure(f, true);
    return true;
}

static bool pair(display_frame *f, unsigned int literal_pc, unsigned int smoke_pc,
                 unsigned int flame_literal_pc, unsigned int flame_pc, double x, double y,
                 double z) {
    return particle(f, literal_pc, smoke_pc, "smoke", x, y, z, 0.0, 0.0, 0.0) &&
           particle(f, flame_literal_pc, flame_pc, "flame", x, y, z, 0.0, 0.0, 0.0);
}

static bool fluid(display_frame *f, int32_t x, int32_t y, int32_t z, mc_java_random *random) {
    mc_material *identity;
    mc_material *captured = f->block->fields.bA; /* RP1, reloaded independently at89. */
    if (!material_identity(f, 4, MC_MATERIAL_g, &identity))
        return false;
    int32_t draw;
    if (captured == identity) {
        if (!draw_int(f, 14, random, 64, &draw))
            return false;
        if (draw == 0) {
            int32_t metadata;
            if (!get(f, 25, x, y, z, true, &metadata))
                return false;
            if (metadata > 0 && metadata < 8) {
                if (!fp(f, 44))
                    return false;
                const double px = float_center(x, 0.5f), py = float_center(y, 0.5f),
                             pz = float_center(z, 0.5f);
                const mc_registry_string *name;
                if (!literal(f, 62, "liquid.water", &name))
                    return false;
                float a, b;
                if (!draw_float(f, 66, random, &a))
                    return false;
                const float volume = at_fadd(at_fmul(a, 0.25f), 0.75f);
                if (!draw_float(f, 77, random, &b))
                    return false;
                const float pitch = at_fadd(at_fmul(b, 1.0f), 0.5f);
                if (!sound(f, 85, name, px, py, pz, volume, pitch))
                    return false;
            }
        }
    }
    captured = f->block->fields.bA;
    if (!material_identity(f, 92, MC_MATERIAL_h, &identity))
        return false;
    if (captured == identity) {
        mc_material *above;
        const int32_t above_y = mc_java_i32_add(y, 1);
        if (!material(f, 105, x, above_y, z, &above) ||
            !material_identity(f, 108, MC_MATERIAL_a, &identity))
            return false;
        if (above == identity) {
            bool closed;
            if (!opaque(f, 121, x, above_y, z, &closed))
                return false;
            if (!closed) {
                if (!draw_int(f, 131, random, 100, &draw))
                    return false;
                if (draw == 0) {
                    if (!fp(f, 138))
                        return false;
                    float a;
                    if (!draw_float(f, 141, random, &a))
                        return false;
                    const double px = at_widen(at_fadd(integer_float(x), a));
                    const double py = at_add((double)y, f->block->fields.bw);
                    if (!draw_float(f, 162, random, &a))
                        return false;
                    const double pz = at_widen(at_fadd(integer_float(z), a));
                    if (!particle(f, 170, 181, "lava", px, py, pz, 0.0, 0.0, 0.0))
                        return false;
                }
            }
        }
    }
    at(f, 184);
    return true;
}

/* VM and DB use the same orientation constants, with separately authored
 * metadata/RNG/presentation bodies and source callsites. */
static void torch_position(int32_t metadata, double *x, double *y, double *z) {
    const double vertical = 0.2199999988079071, lateral = 0.27000001072883606;
    if (metadata == 1) {
        *x = at_sub(*x, lateral);
        *y = at_add(*y, vertical);
    } else if (metadata == 2) {
        *x = at_add(*x, lateral);
        *y = at_add(*y, vertical);
    } else if (metadata == 3) {
        *y = at_add(*y, vertical);
        *z = at_sub(*z, lateral);
    } else if (metadata == 4) {
        *y = at_add(*y, vertical);
        *z = at_add(*z, lateral);
    }
}

static bool torch(display_frame *f, int32_t x, int32_t y, int32_t z) {
    int32_t metadata;
    if (!get(f, 5, x, y, z, true, &metadata) || !fp(f, 11))
        return false;
    double px = float_center(x, 0.5f), py = float_center(y, 0.7f), pz = float_center(z, 0.5f);
    torch_position(metadata, &px, &py, &pz);
    unsigned int literal_pc = 250, smoke_pc = 261, flame_literal_pc = 265, flame_pc = 276;
    if (metadata >= 1 && metadata <= 4) {
        const unsigned int offset = (unsigned int)(metadata - 1) * 51u;
        literal_pc = 52u + offset;
        smoke_pc = 69u + offset;
        flame_literal_pc = 73u + offset;
        flame_pc = 90u + offset;
    }
    if (!pair(f, literal_pc, smoke_pc, flame_literal_pc, flame_pc, px, py, pz))
        return false;
    at(f, 279);
    return true;
}

static bool jitter(display_frame *f, mc_java_random *random, int32_t x, int32_t y, int32_t z,
                   float height, double *px, double *py, double *pz) {
    if (!fp(f, 19))
        return false;
    const double center_x = float_center(x, 0.5f);
    float value;
    if (!draw_float(f, 26, random, &value))
        return false;
    *px = at_add(center_x, at_mul(at_widen(at_fsub(value, 0.5f)), 0.2));
    const double center_y = float_center(y, height);
    if (!draw_float(f, 48, random, &value))
        return false;
    *py = at_add(center_y, at_mul(at_widen(at_fsub(value, 0.5f)), 0.2));
    const double center_z = float_center(z, 0.5f);
    if (!draw_float(f, 71, random, &value))
        return false;
    *pz = at_add(center_z, at_mul(at_widen(at_fsub(value, 0.5f)), 0.2));
    return true;
}

static bool redstone_torch(display_frame *f, int32_t x, int32_t y, int32_t z,
                           mc_java_random *random) {
    at(f, 1);
    if (!f->block->subtype.db.a) {
        at(f, 7);
        return true;
    }
    int32_t metadata;
    double px, py, pz;
    if (!get(f, 13, x, y, z, true, &metadata) || !jitter(f, random, x, y, z, 0.7f, &px, &py, &pz))
        return false;
    torch_position(metadata, &px, &py, &pz);
    unsigned int literal_pc = 216, call_pc = 227;
    if (metadata >= 1 && metadata <= 4) {
        const unsigned int offset = (unsigned int)(metadata - 1) * 30u;
        literal_pc = 102u + offset;
        call_pc = 119u + offset;
    }
    if (!particle(f, literal_pc, call_pc, "reddust", px, py, pz, 0.0, 0.0, 0.0))
        return false;
    at(f, 230);
    return true;
}

static bool furnace(display_frame *f, int32_t x, int32_t y, int32_t z, mc_java_random *random) {
    at(f, 1);
    if (!f->block->subtype.tc.b) {
        at(f, 7);
        return true;
    }
    int32_t metadata;
    if (!get(f, 13, x, y, z, true, &metadata) || !fp(f, 19))
        return false;
    const float center_x = at_fadd(integer_float(x), 0.5f);
    const float center_y = at_fadd(integer_float(y), 0.0f);
    float value;
    if (!draw_float(f, 31, random, &value))
        return false;
    const float py = at_fadd(center_y, at_fdiv(at_fmul(value, 6.0f), 16.0f));
    const float center_z = at_fadd(integer_float(z), 0.5f);
    if (!draw_float(f, 57, random, &value))
        return false;
    const float lateral = at_fsub(at_fmul(value, 0.6f), 0.3f);
    float px = center_x, pz = center_z;
    unsigned int literal_pc, smoke_pc, flame_literal_pc, flame_pc;
    if (metadata == 4) {
        px = at_fsub(center_x, 0.52f);
        pz = at_fadd(center_z, lateral);
        literal_pc = 75;
        smoke_pc = 95;
        flame_literal_pc = 99;
        flame_pc = 119;
    } else if (metadata == 5) {
        px = at_fadd(center_x, 0.52f);
        pz = at_fadd(center_z, lateral);
        literal_pc = 132;
        smoke_pc = 152;
        flame_literal_pc = 156;
        flame_pc = 176;
    } else if (metadata == 2) {
        px = at_fadd(center_x, lateral);
        pz = at_fsub(center_z, 0.52f);
        literal_pc = 189;
        smoke_pc = 209;
        flame_literal_pc = 213;
        flame_pc = 233;
    } else if (metadata == 3) {
        px = at_fadd(center_x, lateral);
        pz = at_fadd(center_z, 0.52f);
        literal_pc = 246;
        smoke_pc = 266;
        flame_literal_pc = 270;
        flame_pc = 290;
    } else {
        at(f, 293);
        return true;
    }
    if (!pair(f, literal_pc, smoke_pc, flame_literal_pc, flame_pc, at_widen(px), at_widen(py),
              at_widen(pz)))
        return false;
    at(f, 293);
    return true;
}

static bool redstone(display_frame *f, int32_t x, int32_t y, int32_t z, mc_java_random *random) {
    int32_t metadata;
    if (!get(f, 5, x, y, z, true, &metadata))
        return false;
    if (metadata > 0) {
        if (!fp(f, 17))
            return false;
        const double center_x = at_add((double)x, 0.5);
        float value;
        if (!draw_float(f, 23, random, &value))
            return false;
        const double px = at_add(center_x, at_mul(at_sub(at_widen(value), 0.5), 0.2));
        const double py = float_center(y, 0.0625f);
        const double center_z = at_add((double)z, 0.5);
        if (!draw_float(f, 55, random, &value))
            return false;
        const double pz = at_add(center_z, at_mul(at_sub(at_widen(value), 0.5), 0.2));
        const float strength = at_fdiv(integer_float(metadata), 15.0f);
        float red = at_fadd(at_fmul(strength, 0.6f), 0.4f);
        if (metadata == 0)
            red = 0.0f;
        float green = at_fsub(at_fmul(at_fmul(strength, strength), 0.7f), 0.5f);
        float blue = at_fsub(at_fmul(at_fmul(strength, strength), 0.6f), 0.7f);
        if (green < 0.0f)
            green = 0.0f;
        if (blue < 0.0f)
            blue = 0.0f;
        if (!particle(f, 143, 160, "reddust", px, py, pz, at_widen(red), at_widen(green),
                      at_widen(blue)))
            return false;
    }
    at(f, 163);
    return true;
}

static bool repeater_offset(display_frame *f, unsigned int static_pc, unsigned int array_pc,
                            int32_t index, double *out) {
    at(f, static_pc);
    mc_registry_classes *classes = f->block->classes;
    if (classes->entered == UINT_MAX)
        return fail(f, MC_WORLD_STORAGE_LIMIT, "Repeater declaring-class entry count");
    ++classes->entered;
    const mc_registry_status status =
        mc_registry_block_class_initialize(classes, MC_BLOCK_REPEATER);
    --classes->entered;
    if (status != MC_REGISTRY_OK) {
        f->error->registry = classes->last_error;
        return registry_failure(f, status, "Repeater actual declaring-class active use");
    }
    const double *const array = classes->wo_a;
    at(f, array_pc);
    if (array == NULL)
        return null_reference(f, "Repeater actual static delay-offset array");
    /* The native source array has the initializer's actual four-double extent.
     * The source index is masked to0..3; no metadata clamp precedes its mask. */
    *out = array[index];
    return true;
}

static bool repeater(display_frame *f, int32_t x, int32_t y, int32_t z, mc_java_random *random) {
    at(f, 1);
    if (!f->block->subtype.wo.c) {
        at(f, 7);
        return true;
    }
    int32_t metadata, draw;
    double px, py, pz;
    if (!get(f, 13, x, y, z, true, &metadata) || !jitter(f, random, x, y, z, 0.4f, &px, &py, &pz) ||
        !draw_int(f, 94, random, 2, &draw))
        return false;
    double ox = 0.0, oz = 0.0;
    const int32_t facing = metadata & 3;
    if (draw == 0) {
        if (facing == 0)
            oz = -0.3125;
        else if (facing == 1)
            ox = 0.3125;
        else if (facing == 2)
            oz = 0.3125;
        else
            ox = -0.3125;
    } else {
        const int32_t delay = (metadata & 12) >> 2;
        double offset;
        static const unsigned int statics[4] = {212, 246, 223, 235};
        static const unsigned int arrays[4] = {217, 251, 228, 240};
        if (!repeater_offset(f, statics[facing], arrays[facing], delay, &offset))
            return false;
        if (facing == 0)
            oz = offset;
        else if (facing == 1)
            ox = -offset;
        else if (facing == 2)
            oz = -offset;
        else
            ox = offset;
    }
    if (!particle(f, 256, 273, "reddust", at_add(px, ox), py, at_add(pz, oz), 0.0, 0.0, 0.0))
        return false;
    at(f, 276);
    return true;
}

static bool portal(display_frame *f, int32_t x, int32_t y, int32_t z, mc_java_random *random) {
    int32_t draw;
    if (!draw_int(f, 4, random, 100, &draw))
        return false;
    if (draw == 0) {
        if (!fp(f, 16))
            return false;
        const double px = at_add((double)x, 0.5), py = at_add((double)y, 0.5),
                     pz = at_add((double)z, 0.5);
        const mc_registry_string *name;
        if (!literal(f, 30, "portal.portal", &name))
            return false;
        float value;
        if (!draw_float(f, 35, random, &value) ||
            !sound(f, 44, name, px, py, pz, 1.0f, at_fadd(at_fmul(value, 0.4f), 0.8f)))
            return false;
    }
    for (int32_t attempt = 0; attempt < 4; ++attempt) {
        if (!fp(f, 57))
            return false;
        float value;
        if (!draw_float(f, 60, random, &value))
            return false;
        double px = at_widen(at_fadd(integer_float(x), value));
        if (!draw_float(f, 71, random, &value))
            return false;
        const double py = at_widen(at_fadd(integer_float(y), value));
        if (!draw_float(f, 83, random, &value))
            return false;
        double pz = at_widen(at_fadd(integer_float(z), value));
        if (!draw_int(f, 102, random, 2, &draw))
            return false;
        const int32_t sign = mc_java_i32_sub(mc_java_i32_mul(draw, 2), 1);
        if (!draw_float(f, 113, random, &value))
            return false;
        double vx = at_mul(at_sub(at_widen(value), 0.5), 0.5);
        if (!draw_float(f, 129, random, &value))
            return false;
        const double vy = at_mul(at_sub(at_widen(value), 0.5), 0.5);
        if (!draw_float(f, 145, random, &value))
            return false;
        double vz = at_mul(at_sub(at_widen(value), 0.5), 0.5);
        int32_t neighbor;
        if (!get(f, 166, mc_java_i32_sub(x, 1), y, z, false, &neighbor))
            return false;
        bool along_x = neighbor == f->block->fields.bn; /* Late field170. */
        if (!along_x) {
            if (!get(f, 183, mc_java_i32_add(x, 1), y, z, false, &neighbor))
                return false;
            along_x = neighbor == f->block->fields.bn; /* Independent late field187. */
        }
        if (along_x) {
            pz = at_add(at_add((double)z, 0.5), at_mul(0.25, (double)sign));
            if (!draw_float(f, 212, random, &value))
                return false;
            vz = at_widen(at_fmul(at_fmul(value, 2.0f), integer_float(sign)));
        } else {
            px = at_add(at_add((double)x, 0.5), at_mul(0.25, (double)sign));
            if (!draw_float(f, 245, random, &value))
                return false;
            vx = at_widen(at_fmul(at_fmul(value, 2.0f), integer_float(sign)));
        }
        if (!particle(f, 258, 272, "portal", px, py, pz, vx, vy, vz))
            return false;
    }
    at(f, 281);
    return true;
}

/* Each call reloads live UU.as and captures that actual Fire's encouragement
 * array BEFORE the acquiring BlockAccess getter, exactly as YQ.c1/9/14. */
static bool fire_neighbor(display_frame *f, unsigned int static_pc, unsigned int call_pc, int32_t x,
                          int32_t y, int32_t z, bool *out) {
    at(f, static_pc);
    const mc_block *fire;
    const mc_registry_status status =
        mc_registry_block_static_get(f->block->classes, "as", &fire, &f->error->registry);
    if (status != MC_REGISTRY_OK)
        return registry_failure(f, status, "Display actual Fire static active use");
    at(f, call_pc);
    if (fire == NULL)
        return null_reference(f, "Display null Fire c receiver");
    if (fire->kind != MC_BLOCK_FIRE || fire->classes != f->block->classes)
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Display actual Fire c receiver type");
    const int32_t *const encouragement = fire->subtype.yq.a;
    int32_t id;
    if (!get(f, call_pc, x, y, z, false, &id)) {
        f->error->child_site = site("yq", "c", "(Lxp;III)Z", 9);
        return false;
    }
    f->error->child_site = site("yq", "c", "(Lxp;III)Z", 14);
    if (encouragement == NULL)
        return null_reference(f, "Display actual Fire encouragement array");
    if (id < 0 || id >= 256) {
        f->error->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Display Fire encouragement index");
    }
    *out = encouragement[id] > 0;
    return true;
}

static bool fire(display_frame *f, int32_t x, int32_t y, int32_t z, mc_java_random *random) {
    int32_t draw;
    if (!draw_int(f, 4, random, 24, &draw))
        return false;
    if (draw == 0) {
        if (!fp(f, 12))
            return false;
        const double px = float_center(x, 0.5f), py = float_center(y, 0.5f),
                     pz = float_center(z, 0.5f);
        const mc_registry_string *name;
        if (!literal(f, 30, "fire.fire", &name))
            return false;
        float a, b;
        if (!draw_float(f, 35, random, &a))
            return false;
        const float volume = at_fadd(1.0f, a);
        if (!draw_float(f, 41, random, &b) ||
            !sound(f, 50, name, px, py, pz, volume, at_fadd(at_fmul(b, 0.7f), 0.3f)))
            return false;
    }
    bool supported;
    if (!support(f, 60, x, mc_java_i32_sub(y, 1), z, &supported))
        return false;
    if (!supported && !fire_neighbor(f, 66, 76, x, mc_java_i32_sub(y, 1), z, &supported))
        return false;
    if (supported) {
        for (int32_t i = 0; i < 3; ++i) {
            if (!fp(f, 92))
                return false;
            float value;
            if (!draw_float(f, 95, random, &value))
                return false;
            const float px = at_fadd(integer_float(x), value);
            if (!draw_float(f, 105, random, &value))
                return false;
            const float py = at_fadd(at_fadd(integer_float(y), at_fmul(value, 0.5f)), 0.5f);
            if (!draw_float(f, 122, random, &value))
                return false;
            const float pz = at_fadd(integer_float(z), value);
            if (!particle(f, 129, 143, "largesmoke", at_widen(px), at_widen(py), at_widen(pz), 0.0,
                          0.0, 0.0))
                return false;
        }
    } else {
        static const int32_t offsets[5][3] = {
            {-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}};
        static const unsigned int statics[5] = {155, 238, 323, 406, 491};
        static const unsigned int neighbors[5] = {165, 248, 333, 416, 501};
        static const unsigned int draws[5][3] = {
            {184, 197, 208}, {269, 282, 293}, {352, 362, 373}, {435, 445, 458}, {520, 532, 546}};
        static const unsigned int literals[5] = {215, 300, 383, 468, 553};
        static const unsigned int calls[5] = {229, 314, 397, 482, 567};
        for (size_t face = 0; face < 5; ++face) {
            bool burns;
            if (!fire_neighbor(f, statics[face], neighbors[face],
                               mc_java_i32_add(x, offsets[face][0]),
                               mc_java_i32_add(y, offsets[face][1]),
                               mc_java_i32_add(z, offsets[face][2]), &burns))
                return false;
            if (!burns)
                continue;
            for (int32_t i = 0; i < 2; ++i) {
                if (!fp(f, draws[face][0] - 3u))
                    return false;
                float value;
                if (!draw_float(f, draws[face][0], random, &value))
                    return false;
                float px;
                if (face == 0)
                    px = at_fadd(integer_float(x), at_fmul(value, 0.1f));
                else if (face == 1)
                    px = at_fsub(integer_float(mc_java_i32_add(x, 1)), at_fmul(value, 0.1f));
                else
                    px = at_fadd(integer_float(x), value);
                if (!draw_float(f, draws[face][1], random, &value))
                    return false;
                const float py =
                    face == 4 ? at_fsub(integer_float(mc_java_i32_add(y, 1)), at_fmul(value, 0.1f))
                              : at_fadd(integer_float(y), value);
                if (!draw_float(f, draws[face][2], random, &value))
                    return false;
                float pz;
                if (face == 2)
                    pz = at_fadd(integer_float(z), at_fmul(value, 0.1f));
                else if (face == 3)
                    pz = at_fsub(integer_float(mc_java_i32_add(z, 1)), at_fmul(value, 0.1f));
                else
                    pz = at_fadd(integer_float(z), value);
                if (!particle(f, literals[face], calls[face], "largesmoke", at_widen(px),
                              at_widen(py), at_widen(pz), 0.0, 0.0, 0.0))
                    return false;
            }
        }
    }
    at(f, 576);
    return true;
}

static bool redstone_ore(display_frame *f, int32_t x, int32_t y, int32_t z) {
    at(f, 1);
    if (f->block->subtype.bs.a) {
        at(f, 13);
        f->error->child = MC_WORLD_RANDOM_DISPLAY_BLOCK_STEP;
        mc_world_step_error_init(&f->error->detail.step);
        mc_world_step_source_failure source;
        if (!mc_world_step_redstone_particles(f->world, f->block, x, y, z, &f->error->detail.step,
                                              &source)) {
            const mc_world_step_error *lower = &f->error->detail.step;
            f->error->registry = lower->registry;
            f->error->cause = lower->cause;
            f->error->child_site = site("bs", "i", "(Lfd;III)V", lower->particle_bytecode);
            if (source.kind == MC_WORLD_STEP_SOURCE_NULL_WORLD ||
                source.kind == MC_WORLD_STEP_SOURCE_NULL_RANDOM)
                return null_reference(f, lower->operation);
            if (source.kind == MC_WORLD_STEP_SOURCE_REGISTRY)
                return registry_failure(f, source.registry_status, lower->operation);
            if (source.kind == MC_WORLD_STEP_SOURCE_QUERY) {
                f->error->registry = lower->block_read.nearby.query.registry;
                f->error->cause = lower->block_read.nearby.query.cause;
                classify_query(f, &lower->block_read.nearby);
            }
            if (source.kind == MC_WORLD_STEP_SOURCE_PRESENTATION) {
                f->error->registry = lower->mutation.registry;
                f->error->cause = lower->mutation.cause;
                classify_registry(f, lower->mutation.registry.status);
                classify_access(f, &lower->mutation.world_access_failure);
            }
            return fail(f, lower->status, lower->operation);
        }
    }
    at(f, 16);
    return true;
}

static bool dispatch(display_frame *f, int32_t x, int32_t y, int32_t z, mc_java_random *random) {
    for (;;) {
        const mc_block *block = f->block;
        f->error->block = block;
        if (block == NULL)
            return null_reference(f, "Block random-display null virtual receiver");
        if (block->classes == NULL ||
            (unsigned int)block->kind >= (unsigned int)MC_BLOCK_KIND_COUNT)
            return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Block random-display concrete receiver provenance");
        if (block->classes->options.side != MC_REGISTRY_CLIENT)
            return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Block random-display client source qualification");
        f->error->site = site("uu", "b", "(Lfd;IIILjava/util/Random;)V", 0);
        switch (block->kind) {
            case MC_BLOCK_STAIRS:
                f->error->site.class_name = "ss";
                at(f, 11);
                if (f->error->wrapped_calls == (size_t)1048576)
                    return fail(f, MC_WORLD_STORAGE_LIMIT,
                                "Stairs display pending wrapped-call ceiling");
                ++f->error->wrapped_calls;
                f->block = block->subtype.ss.a;
                continue;
            case MC_BLOCK_REDSTONE_ORE:
                f->error->site.class_name = "bs";
                return redstone_ore(f, x, y, z);
            case MC_BLOCK_FLOWING_FLUID:
            case MC_BLOCK_STILL_FLUID:
                f->error->site.class_name = "rp";
                return fluid(f, x, y, z, random);
            case MC_BLOCK_TORCH:
                f->error->site.class_name = "vm";
                return torch(f, x, y, z);
            case MC_BLOCK_REDSTONE_TORCH:
                f->error->site.class_name = "db";
                return redstone_torch(f, x, y, z, random);
            case MC_BLOCK_FURNACE:
                f->error->site.class_name = "tc";
                return furnace(f, x, y, z, random);
            case MC_BLOCK_REDSTONE:
                f->error->site.class_name = "sm";
                return redstone(f, x, y, z, random);
            case MC_BLOCK_REPEATER:
                f->error->site.class_name = "wo";
                return repeater(f, x, y, z, random);
            case MC_BLOCK_PORTAL:
                f->error->site.class_name = "ak";
                return portal(f, x, y, z, random);
            case MC_BLOCK_FIRE:
                f->error->site.class_name = "yq";
                return fire(f, x, y, z, random);
            /* These concrete parent chains resolve this EXACT overload to
             * UU.b's literal return0, regardless of their simulation ticks. */
            case MC_BLOCK_BASE:
            case MC_BLOCK_STONE:
            case MC_BLOCK_GRASS:
            case MC_BLOCK_DIRT:
            case MC_BLOCK_SAPLING:
            case MC_BLOCK_SAND:
            case MC_BLOCK_GRAVEL:
            case MC_BLOCK_ORE:
            case MC_BLOCK_LOG:
            case MC_BLOCK_LEAVES:
            case MC_BLOCK_SPONGE:
            case MC_BLOCK_GLASS:
            case MC_BLOCK_DISPENSER:
            case MC_BLOCK_SANDSTONE:
            case MC_BLOCK_NOTE_BLOCK:
            case MC_BLOCK_BED:
            case MC_BLOCK_RAIL:
            case MC_BLOCK_DETECTOR_RAIL:
            case MC_BLOCK_PISTON_BASE:
            case MC_BLOCK_COBWEB:
            case MC_BLOCK_TALL_GRASS:
            case MC_BLOCK_DEAD_BUSH:
            case MC_BLOCK_PISTON_HEAD:
            case MC_BLOCK_WOOL:
            case MC_BLOCK_MOVING_PISTON:
            case MC_BLOCK_FLOWER:
            case MC_BLOCK_MUSHROOM:
            case MC_BLOCK_MINERAL_STORAGE:
            case MC_BLOCK_SLAB:
            case MC_BLOCK_TNT:
            case MC_BLOCK_BOOKSHELF:
            case MC_BLOCK_OBSIDIAN:
            case MC_BLOCK_MOB_SPAWNER:
            case MC_BLOCK_CHEST:
            case MC_BLOCK_CRAFTING_TABLE:
            case MC_BLOCK_CROPS:
            case MC_BLOCK_FARMLAND:
            case MC_BLOCK_SIGN:
            case MC_BLOCK_DOOR:
            case MC_BLOCK_LADDER:
            case MC_BLOCK_LEVER:
            case MC_BLOCK_PRESSURE_PLATE:
            case MC_BLOCK_BUTTON:
            case MC_BLOCK_SNOW_LAYER:
            case MC_BLOCK_ICE:
            case MC_BLOCK_SNOW_BLOCK:
            case MC_BLOCK_CACTUS:
            case MC_BLOCK_CLAY:
            case MC_BLOCK_SUGAR_CANE:
            case MC_BLOCK_JUKEBOX:
            case MC_BLOCK_FENCE:
            case MC_BLOCK_PUMPKIN:
            case MC_BLOCK_NETHERRACK:
            case MC_BLOCK_SOUL_SAND:
            case MC_BLOCK_GLOWSTONE:
            case MC_BLOCK_CAKE:
            case MC_BLOCK_LOCKED_CHEST:
            case MC_BLOCK_TRAPDOOR:
                return true;
            case MC_BLOCK_KIND_COUNT:
                break;
        }
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Block random-display unaudited concrete inheritance");
    }
}

mc_world_storage_status
mc_world_runtime_block_random_display(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, mc_java_random *random,
                                      mc_world_random_display_block_error *out) {
    mc_world_random_display_block_error *error = out;
    if (error == NULL) {
        error = malloc(sizeof(*error));
        if (error == NULL)
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    }
    mc_world_random_display_block_error_init(error);
    display_frame frame = {world, block, error, false};
    (void)dispatch(&frame, x, y, z, random);
    if (frame.entered)
        mc_world_runtime_call_leave(world);
    const mc_world_storage_status status = error->status;
    if (status == MC_WORLD_STORAGE_OK) {
        error->child = MC_WORLD_RANDOM_DISPLAY_BLOCK_NONE;
        error->child_site = (mc_world_random_display_block_site){0};
    }
    if (out == NULL)
        free(error);
    return status;
}
