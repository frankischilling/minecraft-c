#include "mc/item/use/use.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../registry/registry_internal.h"
#include "../render/internal.h"
#include "../stack/source_internal.h"
#include "mc/block/registry/step_sound.h"
#include "mc/core/java/java.h"
#include "mc/entity/entity.h"
#include "mc/entity/player/statistics.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/placement.h"
#include "mc/world/runtime/presentation.h"
#include "mc/world/storage/source_chunk_loader.h"

#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

enum { USE_RECEIPTS = 16, USE_SOURCES = 5 };

struct mc_item_use_call {
    mc_item_use_state state;
    mc_item_use_error error;
    mc_world_placement_error predicate, face, by;
    mc_player_statistic_error statistic;
    mc_world_runtime_acquisition_error *query;
    mc_world_mutation_error *mutation, *sound;
    mc_step_sound_name name;
    mc_world_access_failure receipts[USE_RECEIPTS];
    mc_world_source_chunk_error *sources[USE_SOURCES];
    const mc_world_source_chunk_error *source_identities[USE_SOURCES];
    size_t receipt_count, source_count;
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
        if (c->receipts[i].receipt == v->receipt) {
            receipt_exception(c, &c->receipts[i], child_failed);
            return true;
        }
    mc_world_storage_status status = MC_WORLD_STORAGE_LIMIT;
    if (c->receipt_count < USE_RECEIPTS)
        status = mc_world_access_failure_retain(v, &c->receipts[c->receipt_count]);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.cleanup =
            (mc_world_storage_error){status, "Item use exact callback producer occurrence", 0};
        return false;
    }
    receipt_exception(c, &c->receipts[c->receipt_count], child_failed);
    c->error.retained_producers = ++c->receipt_count;
    return true;
}

static bool copy_source(mc_item_use_call *c, const mc_world_source_chunk_error **slot) {
    const mc_world_source_chunk_error *source = *slot;
    if (source == NULL)
        return true;
    for (size_t i = 0; i < c->source_count; ++i)
        if (source == c->sources[i] || source == c->source_identities[i]) {
            *slot = c->sources[i];
            return true;
        }
    if (c->source_count == USE_SOURCES) {
        c->error.cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                                    "Item use loader diagnostic extent", 0};
        return false;
    }
    mc_world_source_chunk_error *copy = malloc(sizeof(*copy));
    if (copy == NULL) {
        c->error.cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                                    "Item use loader diagnostic copy", 0};
        return false;
    }
    *copy = *source;
    const size_t i = c->source_count++;
    c->sources[i] = copy;
    c->source_identities[i] = source;
    *slot = copy;
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
           copy_source(c, &m->query.source_chunk) && copy_source(c, &m->nearby.query.source_chunk);
}

static bool capture(mc_item_use_call *c) {
    return (c->query == NULL || (publication(c, &c->query->batch, &c->query->tile_entity,
                                             c->query->status != MC_CHUNK_ACQUIRE_OK) &&
                                 copy_source(c, &c->query->source_chunk))) &&
           mutation_capture(c, c->mutation) && mutation_capture(c, c->sound);
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

static bool query(mc_item_use_call *c, int32_t *out) {
    at(c, "ck", use_block_descriptor, 7);
    if (!hold_world(c))
        return false;
    c->query = calloc(1, sizeof(*c->query));
    if (c->query == NULL)
        return fail(c, MC_ITEM_USE_ALLOCATION, "Item use reached query diagnostic", true);
    c->error.acquisition = c->query;
    mc_world_storage_status status = mc_world_runtime_block_get_detailed(
        c->state.world, c->state.x, c->state.y, c->state.z, out, &c->error.world, c->query);
    if (status != MC_WORLD_STORAGE_OK)
        fail(c, MC_ITEM_USE_WORLD, c->error.world.operation, true);
    return captured(c) && status == MC_WORLD_STORAGE_OK;
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
    const char *owner = NULL;
    switch (c->state.item->kind) {
        case MC_ITEM_WOOL_BLOCK:
            owner = "bi";
            break;
        case MC_ITEM_SLAB_BLOCK:
            owner = "en";
            break;
        case MC_ITEM_LOG_BLOCK:
            owner = "go";
            break;
        case MC_ITEM_SAPLING_BLOCK:
            owner = "mr";
            break;
        case MC_ITEM_PISTON_BLOCK:
            owner = "qr";
            break;
        case MC_ITEM_LEAVES_BLOCK:
            owner = "uy";
            break;
        default:
            break;
    }
    if (owner != NULL)
        return unsupported(c, owner, "b", "(I)I");
    /* GM.b(int) is literal zero, independent of the argument. */
    c->state.metadata = 0;
    c->state.metadata_returned = true;
    return true;
}

static bool floating(mc_item_use_call *c, unsigned int pc) {
    at(c, "ck", use_block_descriptor, pc);
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_ITEM_USE_FLOATING, "CK placement floating environment", true);
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

static bool sound(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    if (!floating(c, 210))
        return false;
    s->sound_x = (double)add(integer_float(s->target_x), 0.5f);
    if (!floating(c, 217))
        return false;
    s->sound_y = (double)add(integer_float(s->target_y), 0.5f);
    if (!floating(c, 224))
        return false;
    s->sound_z = (double)add(integer_float(s->target_z), 0.5f);
    at(c, "ck", use_block_descriptor, 231);
    if (s->captured_block == NULL)
        return source_null(c, "CK earlier captured Block sound field");
    at(c, "ck", use_block_descriptor, 234);
    if (mc_registry_step_sound_name(s->registry, s->captured_block->fields.by, false, &c->name,
                                    &c->error.registry) != MC_REGISTRY_OK)
        return registry_failed(c);
    at(c, "ck", use_block_descriptor, 242);
    if (mc_registry_step_sound_volume(s->captured_block->fields.by, &s->volume,
                                      &c->error.registry) != MC_REGISTRY_OK)
        return registry_failed(c);
    if (!floating(c, 246))
        return false;
    s->volume = divide(add(s->volume, 1.0f), 2.0f);
    at(c, "ck", use_block_descriptor, 254);
    if (mc_registry_step_sound_pitch(s->captured_block->fields.by, &s->pitch, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    if (!floating(c, 259))
        return false;
    s->pitch = multiply(s->pitch, 0.8f);
    at(c, "ck", use_block_descriptor, 260);
    c->sound = calloc(1, sizeof(*c->sound));
    if (c->sound == NULL)
        return fail(c, MC_ITEM_USE_ALLOCATION, "CK reached sound diagnostic", true);
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
    if (!query(c, &id))
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
        if (!sound(c))
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
    if (mc_entity_retain(c->state.player) != MC_ENTITY_OK)
        return fail(c, MC_ITEM_USE_LIMIT, "IZ reached Player occurrence", true);
    c->player_held = true;
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
    const char *owner = block_override(k);
    if (owner != NULL)
        return unsupported(c, owner, "a", use_block_descriptor);
    if (block_item(k)) {
        if (!ck(c))
            return false;
    } else {
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
        mc_world_access_failure_release(&c->receipts[i]);
    for (size_t i = 0; i < c->source_count; ++i)
        free(c->sources[i]);
    if (c->result_held)
        mc_item_stack_release(c->state.result_stack);
    if (c->stack_held)
        mc_item_stack_release(c->state.stack);
    if (c->player_held)
        mc_entity_release(c->state.player);
    if (c->world_held)
        mc_world_runtime_binding_release(c->state.world);
    free(c->query);
    free(c->mutation);
    free(c->sound);
    free(c);
    *slot = NULL;
    return admission(error, MC_ITEM_USE_OK, NULL);
busy:
    if (error != NULL)
        *error = c->error;
    return MC_ITEM_USE_BUSY;
}
