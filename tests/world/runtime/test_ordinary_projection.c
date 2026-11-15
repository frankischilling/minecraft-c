/* Ordinary setter result/diagnostic regression on an authored source World.
 * One physical call per process; all planes, callbacks and owners are checked. */
#define MC_OWNED_CACHE_FIXTURE_MAIN qf_unused_owned_cache_main
#include "saving/test_owned_cache.c"
#undef MC_OWNED_CACHE_FIXTURE_MAIN
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/runtime/lighting_internal.h"
#include "../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../src/world/runtime/world_access_internal.h"

static int qf_layout_sink(const char *format, ...) {
    (void)format;
    return 0;
}

#define QN_LAYOUT_REPORT qf_layout_sink
#include "../../benchmarks/quiet_setter_diagnostics.h"
#undef QN_LAYOUT_REPORT

enum { QF_LIGHTS = 64, QF_EVENTS = 8 };

static const size_t qf_cell = ((size_t)8 * 16u + 8u) * 128u + 64u;

typedef struct qf_event {
    int kind;
    int32_t xyz[6], id, metadata, height;
    size_t calls, leases, lights, receiver_holds;
    uint64_t random_state;
    bool o;
} qf_event;

typedef struct qf_observer {
    mc_world_runtime *world;
    mc_chunk_state *chunk;
    mc_world_access *receiver;
    qf_event events[QF_EVENTS];
    size_t count;
    bool valid;
} qf_observer;

typedef struct qf_snapshot {
    mc_chunk_state chunk;
    mc_chunk_data data;
    uint8_t blocks[MC_CHUNK_BLOCKS];
    mc_java_random random;
    uint64_t math_state, math_gaussian;
    bool math_present, math_cached, o, remote, t1;
    size_t calls, leases, bindings, loaded, lights, events, pending_count;
    int32_t pending_keys[8];
    size_t receiver_memberships, receiver_holds, receiver_receipts;
    int32_t light_depth, ordered, members;
    bool scheduled_initialized;
    int64_t sequence;
    mc_world_light_region_fields regions[QF_LIGHTS];
    mc_light_kind light_kinds[QF_LIGHTS];
    mc_chunk_provider_cache_observation cache;
    mc_world_storage_error last_error;
    mc_registry_error last_registry;
    mc_java_random_profile_status last_random_status;
    uint64_t uniquifier;
    unsigned int holder;
} qf_snapshot;

static uint64_t qf_bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof result);
    return result;
}

static mc_world_storage_status qf_record(qf_observer *s, mc_world_runtime *world, int kind,
                                         const int32_t xyz[6], mc_world_storage_error *error) {
    if (world != s->world || s->chunk == NULL || s->count >= QF_EVENTS) {
        s->valid = false;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                          "qualification observer input/event bound", 0};
        return error->status;
    }
    qf_event *e = &s->events[s->count++];
    *e = (qf_event){.kind = kind,
                    .id = s->chunk->data->blocks[qf_cell],
                    .metadata = s->chunk->data->metadata[qf_cell / 2u] & 15u,
                    .height = s->chunk->data->heightmap[136],
                    .calls = world->active_calls,
                    .leases = world->local_leases,
                    .lights = world->fields.C->count,
                    .receiver_holds = s->receiver->active_holds,
                    .random_state = world->fields.r->state,
                    .o = world->fields.o};
    memcpy(e->xyz, xyz, sizeof e->xyz);
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status qf_cell_event(void *context, mc_world_runtime *world, int32_t x,
                                             int32_t y, int32_t z, mc_world_storage_error *error) {
    const int32_t xyz[6] = {x, y, z, 0, 0, 0};
    return qf_record(context, world, 0, xyz, error);
}

static mc_world_storage_status qf_range_event(void *context, mc_world_runtime *world, int32_t x0,
                                              int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                                              int32_t z1, mc_world_storage_error *error) {
    const int32_t xyz[6] = {x0, y0, z0, x1, y1, z1};
    return qf_record(context, world, 1, xyz, error);
}

static mc_world_storage_status qf_sky_event(void *context, mc_world_runtime *world,
                                            mc_world_storage_error *error) {
    const int32_t xyz[6] = {0};
    return qf_record(context, world, 2, xyz, error);
}

static int qf_read(source_save_fixture *source, qf_observer *observer, qf_snapshot *out) {
    mc_world_runtime *world = source->game.worlds[0];
    mc_chunk_state *chunk = observer->chunk;
    VERIFY(chunk != NULL && chunk->data != NULL && world->fields.C != NULL &&
           world->fields.C->count <= QF_LIGHTS &&
           mc_world_store_chunk_count(source->game.stores[0]) == 1);
    out->chunk = *chunk;
    out->data = *chunk->data;
    memcpy(out->blocks, chunk->data->blocks, sizeof out->blocks);
    out->random = *world->fields.r;
    out->math_present = source->game.profile->math != NULL;
    if (out->math_present) {
        out->math_state = atomic_load(&source->game.profile->math->state);
        out->math_cached = source->game.profile->math->have_next_next_gaussian;
        out->math_gaussian = qf_bits(source->game.profile->math->next_next_gaussian);
    }
    out->calls = world->active_calls;
    out->leases = world->local_leases;
    out->bindings = world->persistent_bindings;
    out->loaded = mc_world_store_chunk_count(source->game.stores[0]);
    out->pending_count = world->acquisition.pending->count;
    VERIFY(out->pending_count <= COUNT(out->pending_keys));
    if (out->pending_count != 0)
        memcpy(out->pending_keys, world->acquisition.pending->keys,
               out->pending_count * sizeof(*out->pending_keys));
    out->o = world->fields.o;
    out->remote = world->fields.B;
    out->t1 = source->game.registry[0]->blocks.t[1];
    out->last_error = world->last_error;
    out->last_registry = world->last_registry_error;
    out->last_random_status = world->last_random_status;
    out->uniquifier = atomic_load(&source->game.profile->uniquifier);
    out->holder = atomic_load(&source->game.profile->holder_phase);
    out->lights = world->fields.C->count;
    out->light_depth = world->classes->enqueue_depth;
    out->events = observer->count;
    out->receiver_memberships = observer->receiver->memberships;
    out->receiver_holds = observer->receiver->active_holds;
    out->receiver_receipts = observer->receiver->receipt_holds;
    out->scheduled_initialized = world->classes->scheduled_initialized;
    out->sequence = world->classes->scheduled_sequence.next;
    VERIFY(mc_scheduled_ordered_size(world->fields.E, &out->ordered) == MC_SCHEDULED_ORDERED_OK &&
           mc_scheduled_membership_size(world->fields.F, &out->members) ==
               MC_SCHEDULED_MEMBERSHIP_OK &&
           out->ordered == 0 && out->members == 0 && oc_observe(world, &out->cache) == 0);
    for (size_t i = 0; i < out->lights; ++i) {
        const mc_world_light_region *region = world->fields.C->items[i];
        const mc_world_light_region_fields *fields =
            mc_world_runtime_light_region_fields_get(region);
        const mc_world_light_type_fields *type =
            fields == NULL ? NULL : mc_world_runtime_light_type_fields_get(fields->a);
        VERIFY(fields != NULL && type != NULL);
        out->regions[i] = *fields;
        out->light_kinds[i] = type->kind;
    }
    return 0;
}

static bool qf_equal(const unit_layout *layout, const void *left, const void *right) {
    const unsigned char *a = left, *b = right;
    for (size_t i = 0; i < layout->size; ++i)
        if (layout->mask[i] != UNIT_PADDING && a[i] != b[i])
            return false;
    return true;
}

/* Compare only mutation-carried acquisition children. The source_chunk pointer
 * is not carried by this interface and is not claimed as an observed field. */
static bool qf_neutral(const mc_world_mutation_error *error) {
    mc_world_runtime_acquisition_error *view = calloc(1, sizeof(*view));
    if (view == NULL)
        return false;
    view->tile_entity = error->tile_entity;
    view->registry = error->registry;
    view->batch = error->batch;
    view->activation_bytecode = error->activation_bytecode;
    view->activation_world_bytecode = error->activation_world_bytecode;
    view->load_bytecode = error->load_bytecode;
    view->load_builder_bytecode = error->load_builder_bytecode;
    view->load_entity = error->load_entity;
    view->population = error->population;
    const bool valid = oc_acquisition_empty_children(view);
    free(view);
    return valid;
}

static int qf_input(const qf_snapshot *s, bool air, int32_t metadata) {
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        VERIFY(s->blocks[i] == (i == qf_cell && !air ? 1u : 0u));
    for (size_t i = 0; i < MC_CHUNK_NIBBLE_BYTES; ++i) {
        VERIFY(s->data.metadata[i] == (i == qf_cell / 2u ? (uint8_t)metadata : 0u));
        VERIFY(s->data.skylight[i] == 0 && s->data.block_light[i] == 0);
    }
    for (size_t i = 0; i < MC_CHUNK_HEIGHTMAP_BYTES; ++i)
        VERIFY(s->data.heightmap[i] == (i == 136 && !air ? 65u : 64u));
    VERIFY(s->chunk.x == 0 && s->chunk.z == 0 && !s->chunk.blank && s->chunk.active &&
           s->chunk.terrain_populated && !s->chunk.dirty && s->data.minimum_height == 0 &&
           s->lights == 0 && s->pending_count == 0 && s->events == 0 && !s->remote && !s->o &&
           s->calls == 0 && s->leases == 0 && s->cache.owned && s->cache.raw_exports == 0 &&
           s->cache.map_count == 1 && s->cache.insertion_count == 1);
    return 0;
}

/* CLI: root-utf8-hex api(block|block-meta|notify|notify-meta|metadata|metadata-observe)
 * scene(equal|changed|outside) poison(a5|5a|null). One physical call/process. */
int main(int argc, char **argv) {
    char root[8192];
    VERIFY(argc == 5 && decode_path(argv[1], root, sizeof(root)));
    const char *api = argv[2], *scene = argv[3], *poison = argv[4];
    const bool id_only = strcmp(api, "block") == 0;
    const bool id_meta = strcmp(api, "block-meta") == 0;
    const bool notify = strcmp(api, "notify") == 0;
    const bool notify_meta = strcmp(api, "notify-meta") == 0;
    const bool metadata = strcmp(api, "metadata") == 0 || strcmp(api, "metadata-observe") == 0;
    const bool observe = strcmp(api, "metadata-observe") == 0;
    const bool equal = strcmp(scene, "equal") == 0, changed_scene = strcmp(scene, "changed") == 0;
    const bool outside = strcmp(scene, "outside") == 0;
    const bool nullable = strcmp(poison, "null") == 0;
    VERIFY((id_only || id_meta || notify || notify_meta || metadata) &&
           (equal || changed_scene || outside) && (!observe || !outside) &&
           (nullable || strcmp(poison, "a5") == 0 || strcmp(poison, "5a") == 0));
    const unsigned char fill = strcmp(poison, "a5") == 0 ? 0xa5u : 0x5au;
    source_save_fixture *source = calloc(1, sizeof(*source));
    qf_snapshot *before = calloc(1, sizeof(*before)), *after = calloc(1, sizeof(*after));
    mc_world_mutation_error *error = malloc(sizeof(*error)),
                            *canonical = malloc(sizeof(*canonical));
    VERIFY(source != NULL && before != NULL && after != NULL && error != NULL &&
           canonical != NULL && ss_setup(source, root) == 0 && oc_open(source, SIZE_MAX) == 0);
    mc_world_runtime *world = source->game.worlds[0];
    mc_chunk_state *chunk = NULL;
    VERIFY(mc_world_runtime_get_detailed(world, 0, 0, &chunk, NULL) == MC_CHUNK_ACQUIRE_OK &&
           chunk != NULL && mc_world_store_owns_chunk(source->game.stores[0], chunk));
    mc_java_random_seed(world->fields.r, 1);
    double ignored;
    VERIFY(mc_java_math_random(source->game.profile, &ignored) == MC_JAVA_RANDOM_PROFILE_OK &&
           source->game.profile->math != NULL);
    mc_java_random seeded;
    mc_java_random_seed(&seeded, 0);
    atomic_store(&source->game.profile->math->state, seeded.state);
    source->game.profile->math->have_next_next_gaussian = false;
    source->game.profile->math->next_next_gaussian = 0.0;
    VERIFY(source->game.registry[0]->blocks.t != NULL && !source->game.registry[0]->blocks.t[1] &&
           source->game.registry[0]->blocks.q != NULL &&
           source->game.registry[0]->blocks.q[1] == 255);
    const bool original_t1 = source->game.registry[0]->blocks.t[1];
    if (observe)
        source->game.registry[0]->blocks.t[1] = true;
    qf_observer observer = {.world = world, .chunk = chunk, .valid = true};
    const mc_world_access_callbacks callbacks = {
        .cell = qf_cell_event, .range = qf_range_event, .sky_changed = qf_sky_event};
    VERIFY(mc_world_access_create(&callbacks, &observer, &observer.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(world, observer.receiver, NULL) ==
               MC_WORLD_STORAGE_OK);
    const bool air = !metadata && !equal;
    const int32_t initial_metadata = !air && equal ? 3 : 0;
    VERIFY(qf_read(source, &observer, before) == 0 &&
           qf_input(before, air, initial_metadata) == 0 && before->t1 == observe);
    unit_layout layout = {.size = sizeof(*error), .valid = 1};
    layout.mask = calloc(layout.size, sizeof(*layout.mask));
    layout.context = calloc(layout.size, sizeof(*layout.context));
    layout.member = calloc(layout.size, sizeof(*layout.member));
    VERIFY(layout.mask != NULL && layout.context != NULL && layout.member != NULL);
    unit_prepare_layout(&layout);
    size_t named = 0, padding = 0, unknown = 0;
    for (size_t i = 0; i < layout.size; ++i) {
        named += layout.mask[i] == UNIT_NAMED;
        padding += layout.mask[i] == UNIT_PADDING;
        unknown += layout.mask[i] == UNIT_UNKNOWN;
    }
    /* Compiler-derived V3 mask: never discard an UNKNOWN byte.
     * No ABI-specific size, member count, padding count or fingerprint. */
    VERIFY(layout.valid && named != 0 && named + padding + unknown == sizeof(*error));
    memset(canonical, 0xa5, sizeof(*canonical));
    memset(error, 0x5a, sizeof(*error));
    mc_world_mutation_error_init(canonical);
    mc_world_mutation_error_init(error);
    VERIFY(qf_equal(&layout, canonical, error));
    memset(canonical, (unsigned char)~fill, sizeof(*canonical));
    mc_world_mutation_error_init(canonical);
    memset(error, fill, sizeof(*error));
    bool changed = !changed_scene;
    const int32_t x = outside ? 32000000 : 8;
    mc_world_mutation_error *argument = nullable ? NULL : error;
    mc_world_storage_status status;
    if (id_only)
        status = mc_world_runtime_block_set_quiet(world, x, 64, 8, 1, &changed, argument);
    else if (id_meta)
        status =
            mc_world_runtime_block_metadata_set_quiet(world, x, 64, 8, 1, 3, &changed, argument);
    else if (notify)
        status = mc_world_runtime_block_set(world, x, 64, 8, 1, &changed, argument);
    else if (notify_meta)
        status = mc_world_runtime_block_metadata_set(world, x, 64, 8, 1, 3, &changed, argument);
    else
        status = mc_world_runtime_metadata_set(world, x, 64, 8, 3, argument);
    VERIFY(qf_read(source, &observer, after) == 0);
    VERIFY(
        status == MC_WORLD_STORAGE_OK && observer.valid && (metadata || changed == changed_scene) &&
        (nullable || (qf_equal(&layout, error, canonical) && oc_registry_empty(&error->registry) &&
                      oc_load_empty(&error->load_entity) && qf_neutral(error))) &&
        after->calls == before->calls && after->leases == before->leases &&
        after->bindings == before->bindings && after->loaded == before->loaded &&
        after->receiver_memberships == before->receiver_memberships &&
        after->receiver_holds == before->receiver_holds &&
        after->receiver_receipts == before->receiver_receipts &&
        after->random.state == before->random.state &&
        after->random.have_next_next_gaussian == before->random.have_next_next_gaussian &&
        qf_bits(after->random.next_next_gaussian) == qf_bits(before->random.next_next_gaussian) &&
        after->math_state == before->math_state && after->math_present == before->math_present &&
        after->math_cached == before->math_cached &&
        after->math_gaussian == before->math_gaussian && after->ordered == 0 &&
        after->members == 0 && after->sequence == before->sequence &&
        after->scheduled_initialized == before->scheduled_initialized && !after->o &&
        after->remote == before->remote && after->t1 == before->t1 &&
        after->uniquifier == before->uniquifier && after->holder == before->holder &&
        after->last_random_status == before->last_random_status &&
        after->pending_count == before->pending_count && after->cache.owned &&
        after->cache.map_count == before->cache.map_count &&
        after->cache.insertion_count == before->cache.insertion_count &&
        after->cache.mutation_depth == before->cache.mutation_depth &&
        after->cache.raw_exports == before->cache.raw_exports &&
        after->cache.misses == before->cache.misses &&
        after->cache.rebuilds == before->cache.rebuilds);
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        VERIFY(after->blocks[i] == (i == qf_cell && !outside ? 1u : before->blocks[i]));
    const int32_t final_metadata = outside               ? initial_metadata
                                   : (id_only || notify) ? initial_metadata
                                                         : 3;
    for (size_t i = 0; i < MC_CHUNK_NIBBLE_BYTES; ++i) {
        VERIFY(after->data.metadata[i] == (i == qf_cell / 2u ? (uint8_t)final_metadata : 0u));
    }
    VERIFY(memcmp(before->data.skylight, after->data.skylight, MC_CHUNK_NIBBLE_BYTES) == 0 &&
           memcmp(before->data.block_light, after->data.block_light, MC_CHUNK_NIBBLE_BYTES) == 0);
    for (size_t i = 0; i < MC_CHUNK_HEIGHTMAP_BYTES; ++i)
        VERIFY(after->data.heightmap[i] ==
               (i == 136 && changed_scene && !metadata ? 65u : before->data.heightmap[i]));
    VERIFY(after->chunk.dirty == (!outside && (changed_scene || metadata)) &&
           after->chunk.active == before->chunk.active &&
           after->chunk.terrain_populated == before->chunk.terrain_populated &&
           after->chunk.never_save == before->chunk.never_save &&
           after->chunk.has_entities == before->chunk.has_entities &&
           after->chunk.last_save_time == before->chunk.last_save_time &&
           after->data.minimum_height ==
               (changed_scene && !metadata ? 64 : before->data.minimum_height));
    const size_t expected_events = outside || (equal && !metadata) ? 0u
                                   : metadata                      ? (observe ? 1u : 0u)
                                   : (notify || notify_meta)       ? 2u
                                                                   : 1u;
    VERIFY(observer.count == expected_events);
    if (changed_scene && !metadata) {
        VERIFY(observer.events[0].kind == 1 && observer.events[0].xyz[0] == 8 &&
               observer.events[0].xyz[1] == 64 && observer.events[0].xyz[2] == 8 &&
               observer.events[0].xyz[3] == 8 && observer.events[0].xyz[4] == 65 &&
               observer.events[0].xyz[5] == 8 && observer.events[0].height == 64 &&
               after->lights > 0);
    } else {
        VERIFY(after->lights == before->lights);
    }
    if (expected_events != 0 && (metadata || notify || notify_meta)) {
        const qf_event *last = &observer.events[expected_events - 1u];
        VERIFY(last->kind == 0 && last->xyz[0] == 8 && last->xyz[1] == 64 && last->xyz[2] == 8 &&
               last->id == 1 && last->metadata == final_metadata && last->height == 65);
    }
    if (metadata && !outside)
        VERIFY(after->cache.hits == before->cache.hits + 8);
    if (outside || (equal && !metadata))
        VERIFY(memcmp(before->blocks, after->blocks, MC_CHUNK_BLOCKS) == 0 &&
               memcmp(before->data.metadata, after->data.metadata, MC_CHUNK_NIBBLE_BYTES) == 0 &&
               memcmp(before->data.heightmap, after->data.heightmap, MC_CHUNK_HEIGHTMAP_BYTES) ==
                   0);
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(world, observer.receiver, &removed, NULL) ==
               MC_WORLD_STORAGE_OK &&
           removed && mc_world_access_free(observer.receiver, NULL) == MC_WORLD_STORAGE_OK);
    observer.receiver = NULL;
    source->game.registry[0]->blocks.t[1] = original_t1;
    VERIFY(source->game.registry[0]->blocks.t[1] == original_t1 && oc_stop(source) == 0 &&
           source->game.worlds[0] == NULL && source->game.stores[0] == NULL &&
           ss_finish(source) == 0);
    printf("ORDINARY_PROJECTION_FINISH {\"api\":\"%s\",\"scene\":\"%s\",\"poison\":\"%s\","
           "\"checks\":%u,\"physical_calls\":1,\"canonical_checked\":%u,"
           "\"cleanup\":1,\"t1_restored\":1,\"layout\":[%zu,%zu,%zu,%zu]}\n",
           api, scene, poison, checks, nullable ? 0u : 1u, layout.size, named, padding, unknown);
    free(layout.member);
    free(layout.context);
    free(layout.mask);
    free(canonical);
    free(error);
    free(after);
    free(before);
    free(source);
    return 0;
}
