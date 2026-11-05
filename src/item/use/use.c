#include "mc/item/use/use.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../entity/entity_internal.h"
#include "../../entity/living/fields_internal.h"
#include "../../world/runtime/acquisition_internal.h"
#include "../../world/runtime/block_queries_internal.h"
#include "../../world/runtime/mutation/plants/plants_internal.h"
#include "../registry/registry_internal.h"
#include "../render/internal.h"
#include "../stack/source_internal.h"
#include "mc/block/registry/step_sound.h"
#include "mc/core/java/java.h"
#include "mc/entity/entity.h"
#include "mc/entity/player/statistics.h"
#include "mc/item/combat/combat.h"
#include "mc/item/registry/runtime.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/placement.h"
#include "mc/world/runtime/presentation.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "metadata_internal.h"

#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

enum { USE_RECEIPTS = 16 };

typedef struct use_source_copy {
    mc_world_source_chunk_error value;
    const mc_world_source_chunk_error *identity;
    size_t epoch;
    struct use_source_copy *next;
} use_source_copy;

struct mc_item_use_call {
    mc_item_use_state state;
    mc_item_use_error error;
    mc_world_placement_error predicate, face, by;
    mc_player_statistic_error statistic;
    mc_world_runtime_acquisition_error *queries[2];
    mc_world_runtime_acquisition_error *dye_query;
    mc_world_solid_support_detail *dye_support;
    mc_world_mutation_error *mutation, *sound;
    mc_item_combat_error *durability;
    mc_step_sound_name name;
    mc_world_access_failure receipts[USE_RECEIPTS];
    mc_world_access_failure *extra_receipts;
    use_source_copy *sources;
    size_t receipt_count, extra_capacity, query_count, source_epoch;
    bool dye_query_initialized;
    bool registry_held, stack_held, world_held, player_held, result_held;
};

static const char *const use_block_descriptor = "(Liz;Lgs;Lfd;IIII)Z";
static const char *const wrapper_block_descriptor = "(Lgs;Lfd;IIII)Z";
static const char *const use_air_descriptor = "(Liz;Lfd;Lgs;)Liz;";

static mc_item_use_site site(const char *owner, const char *method, const char *descriptor,
                             unsigned int pc) {
    return (mc_item_use_site){owner, method, descriptor, pc};
}

static void at(mc_item_use_call *c, const char *owner, const char *descriptor, unsigned int pc) {
    c->error.site = site(owner, "a", descriptor, pc);
    c->state.source_pc = pc;
}

static bool fail(mc_item_use_call *c, mc_item_use_status status, const char *operation,
                 bool native) {
    c->state.status = c->error.status = status;
    c->error.operation = operation;
    c->error.native_policy = native;
    return false;
}

static bool source_null(mc_item_use_call *c, const char *operation) {
    c->error.source_exception = true;
    c->error.exception_class = "java/lang/NullPointerException";
    return fail(c, MC_ITEM_USE_SOURCE_NULL, operation, false);
}

/* The existing shared immutable-cause classifier distinguishes actual Java
 * Exceptions/Errors from native failed-initializer frontiers. */
static bool registry_failed(mc_item_use_call *c) {
    mc_item_use_error *e = &c->error;
    mc_item_render_throwable throwable;
    mc_item_render_registry_classification(c->state.registry, &e->registry, &throwable,
                                           &e->exception_class);
    e->source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    e->source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
    if (e->registry.active_initializer_class != NULL)
        e->child = site(e->registry.active_initializer_class, "<clinit>", "()V",
                        (unsigned int)e->registry.active_initializer_pc);
    return fail(c, MC_ITEM_USE_REGISTRY, e->registry.message,
                throwable == MC_ITEM_RENDER_NO_THROWABLE);
}

static void receipt_exception(mc_item_use_call *c, const mc_world_access_failure *v,
                              bool child_failed) {
    const char *name = NULL;
    if (child_failed && mc_world_access_failure_source_exception(v, &name)) {
        c->error.source_exception = true;
        c->error.exception_class = name;
        c->error.native_policy = false;
    }
}

static bool receipt(mc_item_use_call *c, const mc_world_access_failure *v, bool child_failed) {
    if (v->receipt == NULL)
        return true;
    for (size_t i = 0; i < c->receipt_count; ++i)
        if ((i < USE_RECEIPTS ? c->receipts[i].receipt
                              : c->extra_receipts[i - USE_RECEIPTS].receipt) == v->receipt) {
            receipt_exception(c, v, child_failed);
            return true;
        }
    if (c->receipt_count >= USE_RECEIPTS && c->receipt_count - USE_RECEIPTS == c->extra_capacity) {
        const size_t maximum = SIZE_MAX / sizeof(*c->extra_receipts);
        if (c->extra_capacity >= maximum) {
            c->error.cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                                        "Item use producer capture size", 0};
            return false;
        }
        size_t capacity = c->extra_capacity == 0 ? USE_RECEIPTS : c->extra_capacity;
        capacity = capacity > maximum / 2u ? maximum : capacity * 2u;
        mc_world_access_failure *grown = realloc(c->extra_receipts, capacity * sizeof(*grown));
        if (grown == NULL) {
            c->error.cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                                        "Item use producer capture allocation", 0};
            return false;
        }
        c->extra_receipts = grown;
        c->extra_capacity = capacity;
    }
    mc_world_storage_status status = MC_WORLD_STORAGE_LIMIT;
    mc_world_access_failure *destination =
        c->receipt_count < USE_RECEIPTS ? &c->receipts[c->receipt_count]
                                        : &c->extra_receipts[c->receipt_count - USE_RECEIPTS];
    *destination = (mc_world_access_failure){0};
    status = mc_world_access_failure_retain(v, destination);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.cleanup =
            (mc_world_storage_error){status, "Item use exact callback producer occurrence", 0};
        return false;
    }
    receipt_exception(c, destination, child_failed);
    c->error.retained_producers = ++c->receipt_count;
    return true;
}

static bool copy_source(mc_item_use_call *c, const mc_world_source_chunk_error **slot) {
    const mc_world_source_chunk_error *source = *slot;
    if (source == NULL)
        return true;
    for (use_source_copy *copy = c->sources; copy != NULL; copy = copy->next)
        if (source == &copy->value ||
            (copy->epoch == c->source_epoch && source == copy->identity)) {
            *slot = &copy->value;
            return true;
        }
    use_source_copy *copy = malloc(sizeof(*copy));
    if (copy == NULL) {
        c->error.cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                                    "Item use loader diagnostic copy", 0};
        return false;
    }
    *copy = (use_source_copy){
        .value = *source, .identity = source, .epoch = c->source_epoch, .next = c->sources};
    c->sources = copy;
    *slot = &copy->value;
    return true;
}

static bool publication(mc_item_use_call *c, const mc_world_entity_batch_error *b,
                        const mc_live_tile_entity_error *t, bool child_failed) {
    return receipt(c, &b->publication.world_access_failure, child_failed) &&
           receipt(c, &t->world_access_failure, child_failed) &&
           receipt(c, &t->publication.world_access_failure, child_failed) &&
           receipt(c, &t->batch.publication.world_access_failure, child_failed);
}

static bool mutation_capture(mc_item_use_call *c, mc_world_mutation_error *m) {
    if (m == NULL)
        return true;
    const bool failed = m->status != MC_WORLD_STORAGE_OK;
    return receipt(c, &m->world_access_failure, failed) &&
           receipt(c, &m->publication.world_access_failure, failed) &&
           publication(c, &m->batch, &m->tile_entity, failed) &&
           publication(c, &m->nearby.batch, &m->nearby.tile_entity, failed) &&
           publication(c, &m->power.access.batch, &m->power.access.tile_entity, failed) &&
           publication(c, &m->rail_access.nearby.batch, &m->rail_access.nearby.tile_entity,
                       failed) &&
           copy_source(c, &m->query.source_chunk) &&
           copy_source(c, &m->nearby.query.source_chunk) &&
           copy_source(c, &m->plant.query.source_chunk) &&
           copy_source(c, &m->power.access.query.source_chunk) &&
           copy_source(c, &m->rail_access.nearby.query.source_chunk);
}

static bool capture(mc_item_use_call *c) {
    for (size_t i = 0; i < c->query_count; ++i) {
        mc_world_runtime_acquisition_error *q = c->queries[i];
        if (!publication(c, &q->batch, &q->tile_entity, q->status != MC_CHUNK_ACQUIRE_OK) ||
            !copy_source(c, &q->source_chunk))
            return false;
    }
    if (c->dye_query != NULL && (!publication(c, &c->dye_query->batch, &c->dye_query->tile_entity,
                                              c->dye_query->status != MC_CHUNK_ACQUIRE_OK) ||
                                 !copy_source(c, &c->dye_query->source_chunk)))
        return false;
    if (c->dye_support != NULL &&
        (!publication(c, &c->dye_support->access.batch, &c->dye_support->access.tile_entity,
                      c->dye_support->error.status != MC_WORLD_STORAGE_OK) ||
         !copy_source(c, &c->dye_support->access.query.source_chunk)))
        return false;
    return mutation_capture(c, c->mutation) && mutation_capture(c, c->sound);
}

static bool captured(mc_item_use_call *c) {
    if (capture(c))
        return true;
    if (c->state.status == MC_ITEM_USE_OK)
        fail(c, MC_ITEM_USE_LIMIT, c->error.cleanup.operation, true);
    return false;
}

static bool hold_world(mc_item_use_call *c) {
    if (c->state.world == NULL)
        return source_null(c, "Item use actual null World invocation");
    if (c->world_held)
        return true;
    if (!mc_world_runtime_binding_retain(c->state.world))
        return fail(c, MC_ITEM_USE_BUSY, "Item use reached World occurrence", true);
    c->world_held = true;
    return true;
}

/* A reached child may retain Player diagnostics even when its later source
 * operation fails. NULL remains legal for durability; this native occurrence
 * adds no instanceof, constructor or phase gate. The wrapper reuses it. */
static bool hold_player(mc_item_use_call *c, const char *operation) {
    if (c->state.player == NULL || c->player_held)
        return true;
    const mc_entity_status status = mc_entity_retain(c->state.player);
    if (status != MC_ENTITY_OK)
        return fail(c, status == MC_ENTITY_BUSY ? MC_ITEM_USE_BUSY : MC_ITEM_USE_LIMIT, operation,
                    true);
    c->player_held = true;
    return true;
}

static bool query_xyz_at(mc_item_use_call *c, const char *owner, unsigned int pc, int32_t x,
                         int32_t y, int32_t z, int32_t *out) {
    at(c, owner, use_block_descriptor, pc);
    if (!hold_world(c))
        return false;
    if (c->query_count == sizeof(c->queries) / sizeof(c->queries[0]))
        return fail(c, MC_ITEM_USE_LIMIT, "Item use reached query diagnostic extent", true);
    mc_world_runtime_acquisition_error *q = calloc(1, sizeof(*q));
    if (q == NULL)
        return fail(c, MC_ITEM_USE_ALLOCATION, "Item use reached query diagnostic", true);
    c->queries[c->query_count++] = q;
    c->error.acquisition = q;
    ++c->source_epoch;
    mc_world_storage_status status =
        mc_world_runtime_block_get_detailed(c->state.world, x, y, z, out, &c->error.world, q);
    if (status != MC_WORLD_STORAGE_OK)
        fail(c, MC_ITEM_USE_WORLD, c->error.world.operation, true);
    return captured(c) && status == MC_WORLD_STORAGE_OK;
}

static bool query_at(mc_item_use_call *c, const char *owner, unsigned int pc, int32_t y,
                     int32_t *out) {
    return query_xyz_at(c, owner, pc, c->state.x, y, c->state.z, out);
}

static bool resolve_item(mc_item_use_call *c) {
    const char *d = c->state.in_air ? "(Lfd;Lgs;)Liz;" : wrapper_block_descriptor;
    at(c, "iz", d, 1);
    if (c->state.stack == NULL)
        return source_null(c, "IZ actual null Stack invocation");
    const mc_registry_item_array *array = NULL;
    c->error.child = site("iz", "a", "()Lgm;", 0);
    if (mc_registry_item_c_get(c->state.registry, &array, &c->error.registry) != MC_REGISTRY_OK)
        return registry_failed(c);
    c->error.child.pc = 4;
    c->error.stack = mc_item_stack_source_id_get(c->state.stack, &c->state.id);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "IZ reached live Stack.id field", true);
    c->error.child.pc = 7;
    if (mc_registry_item_c_at(c->state.registry, array, c->state.id, &c->state.item,
                              &c->error.registry) != MC_REGISTRY_OK)
        return registry_failed(c);
    c->state.item_resolved = true;
    c->error.child = (mc_item_use_site){0};
    at(c, "iz", d, c->state.in_air ? 7u : 14u);
    if (c->state.item == NULL)
        return source_null(c, "IZ actual null Item invocation");
    if (c->state.item->classes != c->state.registry ||
        (unsigned int)c->state.item->kind >= MC_ITEM_KIND_COUNT)
        return fail(c, MC_ITEM_USE_ARGUMENT, "Item use genuine code-owner Item storage", true);
    return true;
}

static bool block_item(mc_item_kind k) {
    return k >= MC_ITEM_BLOCK && k <= MC_ITEM_PISTON_BLOCK;
}

static const char *block_override(mc_item_kind k) {
    switch (k) {
        case MC_ITEM_DOOR:
            return "hu";
        case MC_ITEM_REDSTONE:
            return "ie";
        case MC_ITEM_DYE:
            return "km";
        case MC_ITEM_HOE:
            return "la";
        case MC_ITEM_BED:
            return "lp";
        case MC_ITEM_BLOCK_PLACER:
            return "lv";
        case MC_ITEM_SEEDS:
            return "rk";
        case MC_ITEM_MINECART:
            return "rl";
        case MC_ITEM_RECORD:
            return "tr";
        case MC_ITEM_SIGN:
            return "vc";
        case MC_ITEM_FLINT_AND_STEEL:
            return "ye";
        case MC_ITEM_PAINTING:
            return "ym";
        default:
            return NULL;
    }
}

static const char *air_override(mc_item_kind k) {
    switch (k) {
        case MC_ITEM_BUCKET:
            return "bc";
        case MC_ITEM_SOUP:
            return "bz";
        case MC_ITEM_FISHING_ROD:
            return "cs";
        case MC_ITEM_SNOWBALL:
            return "dm";
        case MC_ITEM_EGG:
            return "mo";
        case MC_ITEM_BOW:
            return "qz";
        case MC_ITEM_BOAT:
            return "vd";
        case MC_ITEM_FOOD:
        case MC_ITEM_COOKIE:
            return "yw";
        default:
            return NULL;
    }
}

static bool unsupported(mc_item_use_call *c, const char *owner, const char *method,
                        const char *descriptor) {
    c->error.child = site(owner, method, descriptor, 0);
    return fail(c, MC_ITEM_USE_UNSUPPORTED, "Item use reached distinct virtual override", true);
}

static bool food(mc_item_use_call *c) {
    at(c, "yw", use_air_descriptor, 2);
    c->error.stack = mc_item_stack_source_count_get(c->state.stack, &c->state.count_before);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "Food actual Stack count read", true);
    c->state.count_read = true;
    c->state.count_after = mc_java_i32_sub(c->state.count_before, 1);
    at(c, "yw", use_air_descriptor, 7);
    c->error.stack = mc_item_stack_source_count_set(c->state.stack, c->state.count_after);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "Food actual Stack count store", true);
    c->state.count_stored = true;
    at(c, "yw", use_air_descriptor, 12);
    const int32_t amount = c->state.item->yw.a;
    at(c, "yw", use_air_descriptor, 15);
    if (c->state.player == NULL)
        return source_null(c, "Food actual null Player healing invocation");
    if (!hold_player(c, "Food reached Player healing occurrence"))
        return false;
    if (!mc_entity_is_kind(c->state.player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        return fail(c, MC_ITEM_USE_PLAYER, "Food actual concrete Player storage", true);
    c->state.healing_entered = true;
    if (!mc_entity_living_heal_source(c->state.player, amount))
        return fail(c, MC_ITEM_USE_PLAYER, "Food actual inherited Living healing storage", true);
    c->state.healing_returned = true;
    return true;
}

static bool food_result(mc_item_use_call *c, bool soup) {
    if (!food(c))
        return false;
    if (!soup) {
        at(c, "yw", use_air_descriptor, 19);
        c->error.stack = mc_item_stack_retain(c->state.stack);
        if (c->error.stack != MC_ITEM_STACK_OK)
            return fail(c, MC_ITEM_USE_LIMIT, "Food same-Stack return occurrence", true);
        c->result_held = true;
        c->state.result_stack = c->state.stack;
        return true;
    }
    at(c, "bz", use_air_descriptor, 8);
    c->error.stack = mc_item_stack_allocate(&c->state.allocated_stack);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_ALLOCATION, "Soup reached fresh Stack allocation", true);
    const mc_item *bowl = NULL;
    at(c, "bz", use_air_descriptor, 12);
    if (mc_registry_item_static_get(c->state.registry, "C", &bowl, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    c->state.bowl_captured = true;
    at(c, "bz", use_air_descriptor, 15);
    c->error.child = site("iz", "<init>", "(Lgm;)V", 2);
    if (bowl == NULL)
        return source_null(c, "Soup actual ItemStack constructor null Item bf read");
    int32_t id;
    if (mc_registry_item_source_id(c->state.registry, bowl, &id, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    c->error.child.pc = 7;
    c->error.stack = mc_item_stack_construct(c->state.allocated_stack, id, 1, 0);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "Soup actual ItemStack numeric constructor", true);
    c->error.child = (mc_item_use_site){0};
    c->state.result_stack = c->state.allocated_stack;
    at(c, "bz", use_air_descriptor, 18);
    return true;
}

/* Capture m first, then read this same CK.a at the source field site, then
 * index that captured array. A later active use/table edit cannot substitute
 * another Block for a source local that has already been captured. */
static bool block_get(mc_item_use_call *c, unsigned int get_pc, unsigned int field_pc,
                      unsigned int load_pc, const mc_block **out) {
    mc_registry_block_m_view m;
    at(c, "ck", use_block_descriptor, get_pc);
    if (mc_registry_block_m_get(c->state.registry, &m, &c->error.registry) != MC_REGISTRY_OK)
        return registry_failed(c);
    at(c, "ck", use_block_descriptor, field_pc);
    c->state.block_id = c->state.item->ck_a;
    at(c, "ck", use_block_descriptor, load_pc);
    if (mc_registry_block_m_at(&m, c->state.block_id, out, &c->error.registry) != MC_REGISTRY_OK)
        return registry_failed(c);
    return true;
}

static bool placement_failed(mc_item_use_call *c, mc_world_placement_error *p) {
    c->error.child = site(p->site.class_name, p->site.method, p->site.descriptor, p->site.pc);
    c->error.world = p->cause;
    c->error.source_exception = p->source_exception;
    c->error.source_error = p->source_error;
    c->error.exception_class = p->exception_class;
    return fail(c, MC_ITEM_USE_WORLD, p->operation, p->native_policy);
}

static bool world_failed(mc_item_use_call *c, mc_world_mutation_error *m, const char *method,
                         const char *descriptor) {
    c->error.world = m->cause;
    c->error.child = site("fd", method, descriptor, m->wrapper_bytecode);
    return fail(c, MC_ITEM_USE_WORLD, m->operation, true);
}

static bool metadata(mc_item_use_call *c) {
    at(c, "ck", use_block_descriptor, 156);
    c->error.stack = mc_item_stack_source_damage_get(c->state.stack, &c->state.damage);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "CK reached Stack damage method", true);
    at(c, "ck", use_block_descriptor, 159);
    if (!mc_item_placement_metadata(c->state.item->kind, c->state.damage, &c->state.metadata))
        return fail(c, MC_ITEM_USE_ARGUMENT, "Item placement metadata concrete code owner", true);
    c->state.metadata_returned = true;
    return true;
}

static bool floating_at(mc_item_use_call *c, const char *owner, unsigned int pc) {
    at(c, owner, use_block_descriptor, pc);
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_ITEM_USE_FLOATING,
                owner[0] == 'c' ? "CK placement floating environment"
                                : "Hoe sound floating environment",
                true);
}

static float add(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float divide(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float multiply(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float integer_float(int32_t a) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), a));
}

typedef struct sound_sites {
    const char *owner;
    unsigned int x, y, z, block, name, volume, volume_math, pitch, pitch_math, call;
} sound_sites;

static bool sound(mc_item_use_call *c, const sound_sites *p) {
    mc_item_use_state *s = &c->state;
    if (!floating_at(c, p->owner, p->x))
        return false;
    s->sound_x = (double)add(integer_float(s->target_x), 0.5f);
    if (!floating_at(c, p->owner, p->y))
        return false;
    s->sound_y = (double)add(integer_float(s->target_y), 0.5f);
    if (!floating_at(c, p->owner, p->z))
        return false;
    s->sound_z = (double)add(integer_float(s->target_z), 0.5f);
    at(c, p->owner, use_block_descriptor, p->block);
    if (s->captured_block == NULL)
        return source_null(c, p->owner[0] == 'c' ? "CK earlier captured Block sound field"
                                                 : "Hoe captured Farmland sound field");
    at(c, p->owner, use_block_descriptor, p->name);
    if (mc_registry_step_sound_name(s->registry, s->captured_block->fields.by, false, &c->name,
                                    &c->error.registry) != MC_REGISTRY_OK)
        return registry_failed(c);
    at(c, p->owner, use_block_descriptor, p->volume);
    if (mc_registry_step_sound_volume(s->captured_block->fields.by, &s->volume,
                                      &c->error.registry) != MC_REGISTRY_OK)
        return registry_failed(c);
    if (!floating_at(c, p->owner, p->volume_math))
        return false;
    s->volume = divide(add(s->volume, 1.0f), 2.0f);
    at(c, p->owner, use_block_descriptor, p->pitch);
    if (mc_registry_step_sound_pitch(s->captured_block->fields.by, &s->pitch, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    if (!floating_at(c, p->owner, p->pitch_math))
        return false;
    s->pitch = multiply(s->pitch, 0.8f);
    at(c, p->owner, use_block_descriptor, p->call);
    c->sound = calloc(1, sizeof(*c->sound));
    if (c->sound == NULL)
        return fail(c, MC_ITEM_USE_ALLOCATION,
                    p->owner[0] == 'c' ? "CK reached sound diagnostic"
                                       : "Hoe reached sound diagnostic",
                    true);
    c->error.sound = c->sound;
    const mc_world_storage_status status =
        mc_world_runtime_sound(s->world, s->sound_x, s->sound_y, s->sound_z, c->name.string,
                               s->volume, s->pitch, c->sound);
    if (status != MC_WORLD_STORAGE_OK)
        world_failed(c, c->sound, "a", "(DDDLjava/lang/String;FF)V");
    else
        s->sound_returned = true;
    return captured(c) && status == MC_WORLD_STORAGE_OK;
}

static bool ck(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    int32_t id;
    if (!query_at(c, "ck", 7, s->y, &id))
        return false;
    const mc_block *snow;
    at(c, "ck", use_block_descriptor, 10);
    if (mc_registry_block_static_get(s->registry, "aT", &snow, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    at(c, "ck", use_block_descriptor, 13);
    if (snow == NULL)
        return source_null(c, "CK actual Snow singleton id field");
    s->snow = id == snow->fields.bn;
    if (s->snow)
        s->target_face = 0;
    else {
        if (s->target_face == 0)
            s->target_y = mc_java_i32_sub(s->target_y, 1);
        if (s->target_face == 1)
            s->target_y = mc_java_i32_add(s->target_y, 1);
        if (s->target_face == 2)
            s->target_z = mc_java_i32_sub(s->target_z, 1);
        if (s->target_face == 3)
            s->target_z = mc_java_i32_add(s->target_z, 1);
        if (s->target_face == 4)
            s->target_x = mc_java_i32_sub(s->target_x, 1);
        if (s->target_face == 5)
            s->target_x = mc_java_i32_add(s->target_x, 1);
    }
    at(c, "ck", use_block_descriptor, 79);
    c->error.stack = mc_item_stack_source_count_get(s->stack, &s->count_before);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "CK reached Stack count field", true);
    s->count_read = true;
    s->count_after = s->count_before;
    if (s->count_before == 0) {
        at(c, "ck", use_block_descriptor, 86);
        s->result = false;
        return true;
    }
    if (s->target_y == 127) {
        const mc_block *block;
        if (!block_get(c, 94, 98, 101, &block))
            return false;
        at(c, "ck", use_block_descriptor, 102);
        if (block == NULL)
            return source_null(c, "CK ceiling Block material field");
        at(c, "ck", use_block_descriptor, 105);
        if (block->fields.bA == NULL)
            return source_null(c, "CK ceiling actual Material invocation");
        s->ceiling_checked = true;
        if (mc_material_a(block->fields.bA)) {
            at(c, "ck", use_block_descriptor, 112);
            s->result = false;
            return true;
        }
    }
    at(c, "ck", use_block_descriptor, 115);
    s->block_id = s->item->ck_a;
    at(c, "ck", use_block_descriptor, 127);
    c->error.placement = &c->predicate;
    s->predicate_entered = true;
    if (mc_world_runtime_can_place(s->world, s->block_id, s->target_x, s->target_y, s->target_z,
                                   false, s->target_face, &s->can_place,
                                   &c->predicate) != MC_WORLD_STORAGE_OK)
        return placement_failed(c, &c->predicate);
    s->predicate_returned = true;
    if (!s->can_place) {
        at(c, "ck", use_block_descriptor, 276);
        s->result = false;
        return true;
    }
    if (!block_get(c, 133, 137, 140, &s->captured_block))
        return false;
    at(c, "ck", use_block_descriptor, 151);
    const int32_t setter_id = s->item->ck_a;
    if (!metadata(c))
        return false;
    at(c, "ck", use_block_descriptor, 162);
    c->mutation = calloc(1, sizeof(*c->mutation));
    if (c->mutation == NULL)
        return fail(c, MC_ITEM_USE_ALLOCATION, "CK reached setter diagnostic", true);
    c->error.mutation = c->mutation;
    const mc_world_storage_status status =
        mc_world_runtime_block_metadata_set(s->world, s->target_x, s->target_y, s->target_z,
                                            setter_id, s->metadata, &s->setter_value, c->mutation);
    if (status != MC_WORLD_STORAGE_OK)
        world_failed(c, c->mutation, "b", "(IIIII)Z");
    else
        s->setter_returned = true;
    if (!captured(c) || status != MC_WORLD_STORAGE_OK)
        return false;
    if (s->setter_value) {
        const mc_block *block;
        if (!block_get(c, 168, 172, 175, &block))
            return false;
        at(c, "ck", use_block_descriptor, 185);
        c->error.placement = &c->face;
        if (mc_world_block_placed_face(s->registry, block, s->world, s->target_x, s->target_y,
                                       s->target_z, s->target_face,
                                       &c->face) != MC_WORLD_STORAGE_OK)
            return placement_failed(c, &c->face);
        s->placed_face_returned = true;
        if (!block_get(c, 188, 192, 195, &block))
            return false;
        at(c, "ck", use_block_descriptor, 204);
        c->error.placement = &c->by;
        if (mc_world_block_placed_by(s->registry, block, s->world, s->target_x, s->target_y,
                                     s->target_z, s->player, &c->by) != MC_WORLD_STORAGE_OK)
            return placement_failed(c, &c->by);
        s->placed_by_returned = true;
        static const sound_sites pcs = {"ck", 210, 217, 224, 231, 234, 242, 246, 254, 259, 260};
        if (!sound(c, &pcs))
            return false;
        at(c, "ck", use_block_descriptor, 265);
        int32_t count;
        c->error.stack = mc_item_stack_source_count_get(s->stack, &count);
        if (c->error.stack != MC_ITEM_STACK_OK)
            return fail(c, MC_ITEM_USE_STACK, "CK late Stack count reload", true);
        s->count_after = mc_java_i32_sub(count, 1);
        at(c, "ck", use_block_descriptor, 270);
        c->error.stack = mc_item_stack_source_count_set(s->stack, s->count_after);
        if (c->error.stack != MC_ITEM_STACK_OK)
            return fail(c, MC_ITEM_USE_STACK, "CK actual Stack count store", true);
        s->count_stored = true;
    }
    at(c, "ck", use_block_descriptor, 274);
    s->result = true;
    return true;
}

static bool singleton_id(mc_item_use_call *c, const char *owner, const char *name,
                         unsigned int get_pc, unsigned int id_pc, int32_t *out) {
    const mc_block *block = NULL;
    at(c, owner, use_block_descriptor, get_pc);
    if (mc_registry_block_static_get(c->state.registry, name, &block, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    at(c, owner, use_block_descriptor, id_pc);
    if (block == NULL)
        return source_null(c, "Item use actual Block singleton id field");
    *out = block->fields.bn;
    return true;
}

/* LA/RK pop the ordinary World.f result. A normal false still reaches the
 * following Stack mutation; a failed native/callback invocation does not. */
static bool farming_set(mc_item_use_call *c, const char *owner, unsigned int pc, int32_t id) {
    mc_item_use_state *s = &c->state;
    at(c, owner, use_block_descriptor, pc);
    c->mutation = calloc(1, sizeof(*c->mutation));
    if (c->mutation == NULL)
        return fail(c, MC_ITEM_USE_ALLOCATION, "Item use reached setter diagnostic", true);
    c->error.mutation = c->mutation;
    const mc_world_storage_status status = mc_world_runtime_block_set(
        s->world, s->target_x, s->target_y, s->target_z, id, &s->setter_value, c->mutation);
    if (status != MC_WORLD_STORAGE_OK)
        world_failed(c, c->mutation, "f", "(IIII)Z");
    else
        s->setter_returned = true;
    return captured(c) && status == MC_WORLD_STORAGE_OK;
}

static bool redstone(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    int32_t id, snow;
    if (!query_at(c, "ie", 7, s->y, &id) || !singleton_id(c, "ie", "aT", 10, 13, &snow))
        return false;
    s->snow = id == snow;
    if (!s->snow) {
        if (s->target_face == 0)
            s->target_y = mc_java_i32_sub(s->target_y, 1);
        if (s->target_face == 1)
            s->target_y = mc_java_i32_add(s->target_y, 1);
        if (s->target_face == 2)
            s->target_z = mc_java_i32_sub(s->target_z, 1);
        if (s->target_face == 3)
            s->target_z = mc_java_i32_add(s->target_z, 1);
        if (s->target_face == 4)
            s->target_x = mc_java_i32_sub(s->target_x, 1);
        if (s->target_face == 5)
            s->target_x = mc_java_i32_add(s->target_x, 1);
        /* Actual FD.d invokes its acquiring FD.a once at child4, using all
         * three shifted coordinates rather than the original clicked x/z. */
        c->error.child = site("fd", "d", "(III)Z", 4);
        if (!query_xyz_at(c, "ie", 79, s->target_x, s->target_y, s->target_z, &id))
            return false;
        c->error.child = (mc_item_use_site){0};
        if (id != 0) {
            at(c, "ie", use_block_descriptor, 86);
            s->result = false;
            return true;
        }
    }
    at(c, "ie", use_block_descriptor, 87);
    if (mc_registry_block_static_get(s->registry, "aw", &s->captured_block, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    at(c, "ie", use_block_descriptor, 97);
    if (s->captured_block == NULL)
        return source_null(c, "Redstone actual null Block can-place invocation");
    c->error.placement = &c->predicate;
    s->predicate_entered = true;
    if (mc_world_block_can_place_base(s->registry, s->captured_block, s->world, s->target_x,
                                      s->target_y, s->target_z, &s->can_place,
                                      &c->predicate) != MC_WORLD_STORAGE_OK)
        return placement_failed(c, &c->predicate);
    s->predicate_returned = true;
    if (s->can_place) {
        at(c, "ie", use_block_descriptor, 105);
        c->error.stack = mc_item_stack_source_count_get(s->stack, &s->count_before);
        if (c->error.stack != MC_ITEM_STACK_OK)
            return fail(c, MC_ITEM_USE_STACK, "Redstone actual Stack count field", true);
        s->count_read = true;
        s->count_after = mc_java_i32_sub(s->count_before, 1);
        at(c, "ie", use_block_descriptor, 110);
        c->error.stack = mc_item_stack_source_count_set(s->stack, s->count_after);
        if (c->error.stack != MC_ITEM_STACK_OK)
            return fail(c, MC_ITEM_USE_STACK, "Redstone actual Stack count store", true);
        s->count_stored = true;
        /* IE reloads aw only after its count store, then discards the
         * notifying World.f result, including an ordinary false return. */
        if (!singleton_id(c, "ie", "aw", 120, 123, &s->block_id) ||
            !farming_set(c, "ie", 126, s->block_id))
            return false;
    }
    at(c, "ie", use_block_descriptor, 131);
    s->result = true;
    return true;
}

static bool hoe_damage(mc_item_use_call *c) {
    at(c, "la", use_block_descriptor, 147);
    if (!hold_player(c, "Hoe reached durability Player occurrence"))
        return false;
    c->durability = malloc(sizeof(*c->durability));
    if (c->durability == NULL)
        return fail(c, MC_ITEM_USE_ALLOCATION, "Hoe reached durability diagnostic", true);
    c->error.durability = c->durability;
    const mc_entity_status status = mc_item_stack_combat_damage(c->state.registry, c->state.stack,
                                                                1, c->state.player, c->durability);
    if (status == MC_ENTITY_OK)
        return true;
    const mc_item_combat_error *d = c->durability;
    c->error.child =
        site(d->site.class_name, d->site.method, d->site.descriptor, (unsigned int)d->site.pc);
    c->error.registry = d->registry;
    c->error.stack = d->stack;
    c->error.source_exception = d->source_exception;
    c->error.source_error = d->source_error;
    c->error.exception_class = d->exception_class;
    mc_item_use_status result = MC_ITEM_USE_STACK;
    if (status == MC_ENTITY_BUSY)
        result = MC_ITEM_USE_BUSY;
    else if (status == MC_ENTITY_LIMIT)
        result = MC_ITEM_USE_LIMIT;
    else if (status == MC_ENTITY_ALLOCATION_FAILED)
        result = MC_ITEM_USE_ALLOCATION;
    else if (d->statistic_entered && !d->statistic_returned)
        result = MC_ITEM_USE_PLAYER;
    else if (d->registry.status != MC_REGISTRY_OK)
        result = MC_ITEM_USE_REGISTRY;
    return fail(c, result, d->operation, d->native_policy);
}

static bool hoe(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    int32_t id, above, singleton;
    if (!query_at(c, "la", 7, s->y, &id) ||
        !query_at(c, "la", 21, mc_java_i32_add(s->y, 1), &above))
        return false;
    bool grass = false;
    if (s->face != 0 && above == 0) {
        if (!singleton_id(c, "la", "v", 38, 41, &singleton))
            return false;
        grass = id == singleton;
    }
    if (!grass) {
        if (!singleton_id(c, "la", "w", 49, 52, &singleton))
            return false;
        if (id != singleton) {
            at(c, "la", use_block_descriptor, 153);
            s->result = false;
            return true;
        }
    }
    at(c, "la", use_block_descriptor, 58);
    if (mc_registry_block_static_get(s->registry, "aB", &s->captured_block, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    static const sound_sites pcs = {"la", 66, 73, 80, 87, 90, 98, 102, 110, 115, 116};
    if (!sound(c, &pcs))
        return false;
    at(c, "la", use_block_descriptor, 120);
    mc_world_runtime_fields fields;
    const mc_world_storage_status status = mc_world_runtime_fields_get(s->world, &fields);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.world = (mc_world_storage_error){status, "Hoe reached World remote field", 0};
        return fail(c, MC_ITEM_USE_WORLD, c->error.world.operation, true);
    }
    if (fields.B) {
        at(c, "la", use_block_descriptor, 127);
        s->result = true;
        return true;
    }
    at(c, "la", use_block_descriptor, 137);
    /* The same captured Farmland survives the sound callback. */
    s->block_id = s->captured_block->fields.bn;
    if (!farming_set(c, "la", 140, s->block_id) || !hoe_damage(c))
        return false;
    at(c, "la", use_block_descriptor, 151);
    s->result = true;
    return true;
}

static bool seeds(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    if (s->face != 1) {
        at(c, "rk", use_block_descriptor, 7);
        s->result = false;
        return true;
    }
    int32_t id, farmland;
    if (!query_at(c, "rk", 15, s->y, &id) || !singleton_id(c, "rk", "aB", 22, 25, &farmland))
        return false;
    if (id == farmland) {
        int32_t above;
        /* FD.d calls the ordinary acquiring FD.a exactly once at child4. */
        c->error.child = site("fd", "d", "(III)Z", 4);
        if (!query_at(c, "rk", 40, mc_java_i32_add(s->y, 1), &above))
            return false;
        c->error.child = (mc_item_use_site){0};
        if (above == 0) {
            at(c, "rk", use_block_descriptor, 56);
            s->block_id = s->item->subtype.rk.a;
            s->target_y = mc_java_i32_add(s->y, 1);
            if (!farming_set(c, "rk", 59, s->block_id))
                return false;
            at(c, "rk", use_block_descriptor, 65);
            c->error.stack = mc_item_stack_source_count_get(s->stack, &s->count_before);
            if (c->error.stack != MC_ITEM_STACK_OK)
                return fail(c, MC_ITEM_USE_STACK, "Seeds late Stack count field", true);
            s->count_read = true;
            s->count_after = mc_java_i32_sub(s->count_before, 1);
            at(c, "rk", use_block_descriptor, 70);
            c->error.stack = mc_item_stack_source_count_set(s->stack, s->count_after);
            if (c->error.stack != MC_ITEM_STACK_OK)
                return fail(c, MC_ITEM_USE_STACK, "Seeds actual Stack count store", true);
            s->count_stored = true;
            at(c, "rk", use_block_descriptor, 74);
            s->result = true;
            return true;
        }
    }
    at(c, "rk", use_block_descriptor, 76);
    s->result = false;
    return true;
}

#include "dye.inc"

static bool used_statistic(mc_item_use_call *c) {
    at(c, "iz", wrapper_block_descriptor, 25);
    if (mc_registry_classes_initialize_statistics(c->state.registry, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    /* Every actual JL.E allocation is newarray32000. Capture that source
     * reference BEFORE reloading the possibly callback-mutated Stack.id29. */
    mc_statistic *const *captured_array = c->state.registry->statistics.E;
    at(c, "iz", wrapper_block_descriptor, 29);
    int32_t id;
    c->error.stack = mc_item_stack_source_id_get(c->state.stack, &id);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "IZ late Stack id field", true);
    at(c, "iz", wrapper_block_descriptor, 32);
    if (captured_array == NULL)
        return source_null(c, "IZ actual null used-statistic array");
    if (id < 0 || id >= 32000) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/ArrayIndexOutOfBoundsException";
        return fail(c, MC_ITEM_USE_SOURCE_INDEX, "IZ actual used-statistic array index", false);
    }
    const mc_statistic *statistic = captured_array[id];
    at(c, "iz", wrapper_block_descriptor, 34);
    if (c->state.player == NULL)
        return source_null(c, "IZ actual null Player statistic invocation");
    if (!hold_player(c, "IZ reached Player occurrence"))
        return false;
    c->error.statistic = &c->statistic;
    c->state.statistic_entered = true;
    if (mc_local_player_add_statistic(c->state.player, statistic, 1, &c->statistic) !=
        MC_PLAYER_STATISTIC_OK) {
        c->error.child = site(c->statistic.site.class_name, c->statistic.site.method,
                              c->statistic.site.descriptor, c->statistic.site.pc);
        if (c->statistic.status == MC_PLAYER_STATISTIC_NULL_REFERENCE) {
            c->error.source_exception = true;
            c->error.exception_class = "java/lang/NullPointerException";
        } else if (c->statistic.status == MC_PLAYER_STATISTIC_WRITER_FAILED &&
                   c->statistic.writer.status == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
                   c->statistic.writer.exception == MC_CLIENT_STATS_NULL_REFERENCE) {
            c->error.source_exception = true;
            c->error.exception_class = "java/lang/NullPointerException";
        }
        return fail(c, MC_ITEM_USE_PLAYER, c->statistic.operation, !c->error.source_exception);
    }
    c->state.statistic_returned = true;
    return true;
}

static bool execute(mc_item_use_call *c) {
    if (!resolve_item(c))
        return false;
    const mc_item_kind k = c->state.item->kind;
    if (c->state.in_air) {
        if (k == MC_ITEM_FOOD || k == MC_ITEM_COOKIE || k == MC_ITEM_SOUP) {
            if (!food_result(c, k == MC_ITEM_SOUP))
                return false;
            at(c, "iz", "(Lfd;Lgs;)Liz;", 10);
            return true;
        }
        const char *owner = air_override(k);
        if (owner != NULL)
            return unsupported(c, owner, "a", use_air_descriptor);
        /* Actual GM.a returns the very same Stack, even count0/RAW/FAILED. */
        c->error.stack = mc_item_stack_retain(c->state.stack);
        if (c->error.stack != MC_ITEM_STACK_OK)
            return fail(c, MC_ITEM_USE_LIMIT, "GM same-Stack return occurrence", true);
        c->result_held = true;
        c->state.result_stack = c->state.stack;
        at(c, "iz", "(Lfd;Lgs;)Liz;", 10);
        return true;
    }
    if (k == MC_ITEM_DYE) {
        if (!dye(c))
            return false;
    } else if (k == MC_ITEM_HOE) {
        if (!hoe(c))
            return false;
    } else if (k == MC_ITEM_SEEDS) {
        if (!seeds(c))
            return false;
    } else if (k == MC_ITEM_REDSTONE) {
        if (!redstone(c))
            return false;
    } else if (block_item(k)) {
        if (!ck(c))
            return false;
    } else {
        const char *owner = block_override(k);
        if (owner != NULL)
            return unsupported(c, owner, "a", use_block_descriptor);
        /* Every other known receiver inherits GM's literal false body. */
        c->state.result = false;
    }
    if (c->state.result && !used_statistic(c))
        return false;
    at(c, "iz", wrapper_block_descriptor, 39);
    return true;
}

static mc_item_use_status admission(mc_item_use_error *e, mc_item_use_status s,
                                    const char *operation) {
    if (e != NULL)
        *e = (mc_item_use_error){.status = s, .operation = operation, .native_policy = true};
    return s;
}

static mc_item_use_status begin(mc_registry_classes *r, mc_item_stack *stack, mc_entity *player,
                                mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                int32_t face, bool in_air, mc_item_use_call **out,
                                mc_item_use_error *error) {
    if (r == NULL || out == NULL || *out != NULL)
        return admission(error, MC_ITEM_USE_ARGUMENT, "Item use actual Registry/empty output");
    mc_item_use_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return admission(error, MC_ITEM_USE_ALLOCATION, "Item use retained receipt allocation");
    if (mc_registry_classes_retain(r, MC_REGISTRY_CLIENT, r->options.random_profile,
                                   &c->error.registry) != MC_REGISTRY_OK) {
        if (error != NULL) {
            *error = c->error;
            error->status = MC_ITEM_USE_REGISTRY;
            error->operation = c->error.registry.message;
            error->native_policy = true;
        }
        free(c);
        return MC_ITEM_USE_REGISTRY;
    }
    c->registry_held = true;
    if (stack != NULL && mc_item_stack_retain(stack) != MC_ITEM_STACK_OK) {
        (void)mc_registry_runtime_borrow_release(r);
        free(c);
        return admission(error, MC_ITEM_USE_LIMIT, "Item use input Stack occurrence");
    }
    c->stack_held = stack != NULL;
    c->state = (mc_item_use_state){.registry = r,
                                   .stack = stack,
                                   .world = world,
                                   .player = player,
                                   .x = x,
                                   .y = y,
                                   .z = z,
                                   .face = face,
                                   .target_x = x,
                                   .target_y = y,
                                   .target_z = z,
                                   .target_face = face,
                                   .in_air = in_air,
                                   .active = true,
                                   .error = &c->error};
    *out = c;
    c->state.returned = execute(c);
    c->state.active = false;
    if (error != NULL)
        *error = c->error;
    return c->state.status;
}

mc_item_use_status mc_item_use_on_block_begin(mc_registry_classes *r, mc_item_stack *stack,
                                              mc_entity *player, mc_world_runtime *world, int32_t x,
                                              int32_t y, int32_t z, int32_t face,
                                              mc_item_use_call **out, mc_item_use_error *error) {
    return begin(r, stack, player, world, x, y, z, face, false, out, error);
}

mc_item_use_status mc_item_use_in_air_begin(mc_registry_classes *r, mc_item_stack *stack,
                                            mc_world_runtime *world, mc_entity *player,
                                            mc_item_use_call **out, mc_item_use_error *error) {
    return begin(r, stack, player, world, 0, 0, 0, 0, true, out, error);
}

bool mc_item_use_inspect(const mc_item_use_call *c, mc_item_use_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

mc_item_use_status mc_item_use_retire(mc_item_use_call **slot, mc_item_use_error *error) {
    if (slot == NULL)
        return admission(error, MC_ITEM_USE_ARGUMENT, "Item use retirement output");
    mc_item_use_call *c = *slot;
    if (c == NULL)
        return admission(error, MC_ITEM_USE_OK, NULL);
    if (c->state.active) {
        c->error.cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_BUSY,
                                                    "Item use active receipt retirement", 0};
        if (error != NULL)
            *error = c->error;
        return MC_ITEM_USE_BUSY;
    }
    if (!capture(c))
        goto busy;
    if (!mc_world_placement_error_retire(&c->predicate) ||
        !mc_world_placement_error_retire(&c->face) || !mc_world_placement_error_retire(&c->by)) {
        c->error.cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_BUSY,
                                                    "Item use placement child retirement", 0};
        goto busy;
    }
    c->error.placement = NULL;
    if (c->name.classes != NULL && mc_registry_step_sound_name_release(
                                       &c->name, &c->error.cleanup_registry) != MC_REGISTRY_OK) {
        c->error.cleanup =
            (mc_world_storage_error){MC_WORLD_STORAGE_BUSY, "Item use sound String retirement", 0};
        goto busy;
    }
    if (c->registry_held && mc_registry_classes_release(
                                c->state.registry, &c->error.cleanup_registry) != MC_REGISTRY_OK) {
        c->error.cleanup =
            (mc_world_storage_error){MC_WORLD_STORAGE_BUSY, "Item use code Registry retirement", 0};
        goto busy;
    }
    c->registry_held = false;
    for (size_t i = 0; i < c->receipt_count; ++i)
        mc_world_access_failure_release(i < USE_RECEIPTS ? &c->receipts[i]
                                                         : &c->extra_receipts[i - USE_RECEIPTS]);
    free(c->extra_receipts);
    while (c->sources != NULL) {
        use_source_copy *next = c->sources->next;
        free(c->sources);
        c->sources = next;
    }
    if (c->result_held)
        mc_item_stack_release(c->state.result_stack);
    mc_item_stack_release(c->state.allocated_stack);
    if (c->stack_held)
        mc_item_stack_release(c->state.stack);
    if (c->player_held)
        mc_entity_release(c->state.player);
    if (c->world_held)
        mc_world_runtime_binding_release(c->state.world);
    for (size_t i = 0; i < c->query_count; ++i)
        free(c->queries[i]);
    free(c->dye_query);
    free(c->dye_support);
    free(c->mutation);
    free(c->sound);
    free(c->durability);
    free(c);
    *slot = NULL;
    return admission(error, MC_ITEM_USE_OK, NULL);
busy:
    if (error != NULL)
        *error = c->error;
    return MC_ITEM_USE_BUSY;
}
