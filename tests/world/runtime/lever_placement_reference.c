/* Real constructors, GAME owners and original-declared Chunk inputs. The
 * included Player control main is never executed. No source Random is seeded,
 * replaced or advanced by the observer. */
#define main mc_lever_placement_player_fixture_main
#include "../../entity/player/test_construction.c"
#undef main

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/world/chunk/object_state_internal.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../src/world/storage/generated_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/placement.h"
#include "mc/world/runtime/world_access.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum {
    LEVER_PROFILES = 244,
    LEVER_CHUNKS = 4,
    LEVER_PLANES = 5,
    LEVER_ARRAY_BYTES = MC_CHUNK_BLOCKS + 3 * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES,
    LEVER_EVENTS = 4096,
    LEVER_DROPS = 64
};

typedef struct lever_profile {
    char name[96];
    int32_t v[12];
} lever_profile;

enum { ENTRY, X, Y, Z, METADATA, FACE, NEIGHBOR, WALLS, FLOOR, TARGET, NULL_WORLD, REMOTE };

static const size_t lever_lengths[LEVER_PLANES] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES,
                                                   MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                                   MC_CHUNK_HEIGHTMAP_BYTES};

static bool lever_setup(fixture *f, const char *resources, const char *directory) {
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                          &f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->world_classes, f->registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->world_classes, f->entities) == MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options store = {.session_token = 6381,
                                          .block_map = map,
                                          .block_map_length = 256,
                                          .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, f->regions, &store, &f->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    static const uint16_t name[] = {'L', 'e', 'v', 'e', 'r'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){name, 5}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = LEVER_CHUNKS;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
          MC_PLAYER_OK);
    return true;
}

static uint8_t *lever_plane(mc_chunk_state *chunk, size_t plane) {
    switch (plane) {
        case 0:
            return chunk->data->blocks;
        case 1:
            return chunk->data->metadata;
        case 2:
            return chunk->data->skylight;
        case 3:
            return chunk->data->block_light;
        case 4:
            return chunk->data->heightmap;
        default:
            return NULL;
    }
}

static void lever_cell(mc_chunk_state **chunks, int32_t x, int32_t y, int32_t z, int32_t id) {
    if (x < 0 || x >= 32 || z < 0 || z >= 32 || y < 0 || y >= 128)
        return;
    mc_chunk_state *chunk = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)];
    const size_t index = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
    chunk->data->blocks[index] = (uint8_t)id;
}

static void lever_center(mc_chunk_state **chunks, int32_t x, int32_t y, int32_t z, int32_t out[2]) {
    out[0] = out[1] = 0;
    if (x < 0 || x >= 32 || z < 0 || z >= 32 || y < 0 || y >= 128)
        return;
    const mc_chunk_data *data = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)]->data;
    const size_t index = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
    out[0] = data->blocks[index];
    out[1] = (data->metadata[index >> 1u] >> ((index & 1u) * 4u)) & 15;
}

static void lever_reset(mc_chunk_state **chunks, const lever_profile *p, uint8_t *prior) {
    for (size_t chunk = 0; chunk < LEVER_CHUNKS; ++chunk) {
        for (size_t plane = 0; plane < LEVER_PLANES; ++plane)
            memset(lever_plane(chunks[chunk], plane), plane == 2 ? 15 : 0, lever_lengths[plane]);
        chunks[chunk]->data->minimum_height = 0;
        chunks[chunk]->terrain_populated = true;
    }
    const int32_t x = p->v[X], y = p->v[Y], z = p->v[Z];
    if ((p->v[WALLS] & 1) != 0)
        lever_cell(chunks, mc_java_i32_sub(x, 1), y, z, 1);
    if ((p->v[WALLS] & 2) != 0)
        lever_cell(chunks, mc_java_i32_add(x, 1), y, z, 1);
    if ((p->v[WALLS] & 4) != 0)
        lever_cell(chunks, x, y, mc_java_i32_sub(z, 1), 1);
    if ((p->v[WALLS] & 8) != 0)
        lever_cell(chunks, x, y, mc_java_i32_add(z, 1), 1);
    lever_cell(chunks, x, mc_java_i32_sub(y, 1), z, p->v[FLOOR]);
    lever_cell(chunks, x, y, z, p->v[TARGET]);
    if (x >= 0 && x < 32 && z >= 0 && z < 32 && y >= 0 && y < 128) {
        mc_chunk_data *data = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)]->data;
        const size_t index = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
        const unsigned int shift = (unsigned int)(index & 1u) * 4u;
        data->metadata[index >> 1u] = (uint8_t)((data->metadata[index >> 1u] & ~(15u << shift)) |
                                                (((uint32_t)p->v[METADATA] & 15u) << shift));
    }
    size_t offset = 0;
    for (size_t chunk = 0; chunk < LEVER_CHUNKS; ++chunk)
        for (size_t plane = 0; plane < LEVER_PLANES; ++plane) {
            if (prior != NULL)
                memcpy(prior + offset, lever_plane(chunks[chunk], plane), lever_lengths[plane]);
            offset += lever_lengths[plane];
        }
}

typedef struct lever_random_state {
    uint64_t seed, gaussian;
    bool have;
} lever_random_state;

static uint64_t lever_double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t lever_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static lever_random_state lever_random(const mc_java_random *random) {
    return (lever_random_state){random->state, lever_double_bits(random->next_next_gaussian),
                                random->have_next_next_gaussian};
}

/* Quiescent field-only reads. No Math active use or cold-holder initialization. */
static bool lever_math(const fixture *f, lever_random_state *out) {
    if (atomic_load(&f->profile->holder_phase) != MC_JAVA_RANDOM_HOLDER_READY ||
        f->profile->math == NULL)
        return false;
    const mc_java_math_random_owner *math = f->profile->math;
    const uint64_t seed = atomic_load(&math->state);
    *out = (lever_random_state){seed, lever_double_bits(math->next_next_gaussian),
                                math->have_next_next_gaussian};
    return true;
}

static bool lever_random_equal(lever_random_state a, lever_random_state b) {
    return a.seed == b.seed && a.gaussian == b.gaussian && a.have == b.have;
}

static void lever_print_random(lever_random_state value) {
    printf("{\"seed\":\"%012" PRIx64 "\",\"gaussian\":\"%016" PRIx64 "\",\"have\":%s}", value.seed,
           value.gaussian, value.have ? "true" : "false");
}

typedef struct lever_event {
    unsigned int kind;
    int32_t v[6];
    uint64_t position[3];
    uint32_t volume, pitch;
} lever_event;

typedef struct lever_drop {
    mc_entity_snapshot entity;
    mc_item_stack_fields stack;
    lever_random_state random, math;
    mc_entity *identity;
} lever_drop;

typedef struct lever_observer {
    fixture *f;
    mc_chunk_state **chunks;
    lever_event events[LEVER_EVENTS];
    lever_drop drops[LEVER_DROPS];
    size_t event_count, drop_count;
    bool failed, math_seen;
} lever_observer;

static mc_world_storage_status lever_observer_fail(lever_observer *o,
                                                   mc_world_storage_error *error) {
    o->failed = true;
    if (error != NULL)
        *error = (mc_world_storage_error){.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          .operation = "Lever observer capacity or actual Item"};
    return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status lever_observe_cell(void *context, mc_world_runtime *world, int32_t x,
                                                  int32_t y, int32_t z,
                                                  mc_world_storage_error *error) {
    lever_observer *o = context;
    if (world != o->f->world || o->event_count == LEVER_EVENTS)
        return lever_observer_fail(o, error);
    int32_t center[2];
    lever_center(o->chunks, x, y, z, center);
    o->events[o->event_count++] = (lever_event){.kind = 0, .v = {x, y, z, center[0], center[1], 0}};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status lever_observe_range(void *context, mc_world_runtime *world,
                                                   int32_t x0, int32_t y0, int32_t z0, int32_t x1,
                                                   int32_t y1, int32_t z1,
                                                   mc_world_storage_error *error) {
    lever_observer *o = context;
    if (world != o->f->world || o->event_count == LEVER_EVENTS)
        return lever_observer_fail(o, error);
    o->events[o->event_count++] = (lever_event){.kind = 1, .v = {x0, y0, z0, x1, y1, z1}};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status lever_observe_add(void *context, mc_world_runtime *world,
                                                 mc_entity *entity, mc_world_storage_error *error) {
    lever_observer *o = context;
    if (world != o->f->world || o->event_count == LEVER_EVENTS || o->drop_count == LEVER_DROPS)
        return lever_observer_fail(o, error);
    lever_drop *drop = &o->drops[o->drop_count];
    mc_entity_class_literal_view literal;
    if (mc_entity_get_snapshot(entity, &drop->entity) != MC_ENTITY_OK ||
        mc_entity_class_literal_inspect(drop->entity.class_literal, &literal) != MC_ENTITY_OK ||
        literal.kind != MC_ENTITY_CLASS_KIND_ITEM || literal.source != MC_ENTITY_CLIENT ||
        strcmp(literal.client_name, "hl") != 0 || drop->entity.random == NULL ||
        mc_item_stack_fields_get(drop->entity.stack, &drop->stack) != MC_ITEM_STACK_OK ||
        !lever_math(o->f, &drop->math))
        return lever_observer_fail(o, error);
    drop->random = lever_random(drop->entity.random);
    drop->identity = entity;
    o->events[o->event_count++] =
        (lever_event){.kind = 2, .v = {(int32_t)o->drop_count, 0, 0, 0, 0, 0}};
    ++o->drop_count;
    o->math_seen = true;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status lever_observe_sound(void *context, mc_world_runtime *world,
                                                   const mc_registry_string *name, double x,
                                                   double y, double z, float volume, float pitch,
                                                   mc_world_storage_error *error) {
    lever_observer *o = context;
    mc_registry_string_view text;
    static const uint16_t expected[] = {'r', 'a', 'n', 'd', 'o', 'm', '.', 'c', 'l', 'i', 'c', 'k'};
    if (world != o->f->world || o->event_count == LEVER_EVENTS ||
        !mc_registry_string_read(name, &text) || text.length != 12 ||
        memcmp(text.units, expected, sizeof(expected)) != 0)
        return lever_observer_fail(o, error);
    lever_event *event = &o->events[o->event_count++];
    *event = (lever_event){.kind = 3};
    event->position[0] = lever_double_bits(x);
    event->position[1] = lever_double_bits(y);
    event->position[2] = lever_double_bits(z);
    event->volume = lever_float_bits(volume);
    event->pitch = lever_float_bits(pitch);
    return MC_WORLD_STORAGE_OK;
}

typedef struct lever_queue {
    int32_t (*rows)[7];
    size_t count;
} lever_queue;

static bool lever_queue_copy(mc_world_runtime *world, lever_queue *out) {
    mc_world_light_list_view list;
    CHECK(mc_world_runtime_light_list_get(world, &list, NULL) == MC_WORLD_STORAGE_OK &&
          list.count <= MC_LIGHT_QUEUE_LIMIT);
    out->count = list.count;
    out->rows = list.count == 0 ? NULL : calloc(list.count, sizeof(*out->rows));
    CHECK(list.count == 0 || out->rows != NULL);
    for (size_t i = 0; i < list.count; ++i) {
        const mc_world_light_region *region = NULL;
        CHECK(mc_world_runtime_light_at(world, i, &region, NULL) == MC_WORLD_STORAGE_OK);
        const mc_world_light_region_fields *r = mc_world_runtime_light_region_fields_get(region);
        CHECK(r != NULL);
        const mc_world_light_type_fields *type = mc_world_runtime_light_type_fields_get(r->a);
        CHECK(type != NULL && (type->kind == MC_LIGHT_SKY || type->kind == MC_LIGHT_BLOCK));
        const mc_light_bounds b = r->bounds;
        const int32_t row[7] = {
            (int32_t)type->kind, b.min_x, b.min_y, b.min_z, b.max_x, b.max_y, b.max_z};
        memcpy(out->rows[i], row, sizeof(row));
    }
    return true;
}

static void lever_print_ints(const int32_t *v, size_t count) {
    putchar('[');
    for (size_t i = 0; i < count; ++i)
        printf("%s%" PRId32, i == 0 ? "" : ",", v[i]);
    putchar(']');
}

static void lever_print_queue(const lever_queue *queue) {
    putchar('[');
    for (size_t i = 0; i < queue->count; ++i) {
        if (i != 0)
            putchar(',');
        lever_print_ints(queue->rows[i], 7);
    }
    putchar(']');
}

static void lever_print_changes(mc_chunk_state **chunks, const uint8_t *prior) {
    size_t offset = 0, count = 0;
    putchar('[');
    for (size_t chunk = 0; chunk < LEVER_CHUNKS; ++chunk)
        for (size_t plane = 0; plane < LEVER_PLANES; ++plane) {
            const uint8_t *after = lever_plane(chunks[chunk], plane);
            for (size_t i = 0; i < lever_lengths[plane]; ++i)
                if (prior[offset + i] != after[i])
                    printf("%s[%zu,%zu,%zu,%u,%u]", count++ == 0 ? "" : ",", chunk, plane, i,
                           (unsigned int)prior[offset + i], (unsigned int)after[i]);
            offset += lever_lengths[plane];
        }
    putchar(']');
}

static void lever_print_events(const lever_observer *o) {
    putchar('[');
    for (size_t i = 0; i < o->event_count; ++i) {
        const lever_event *e = &o->events[i];
        if (i != 0)
            putchar(',');
        if (e->kind == 3) {
            printf("[\"sound\",\"random.click\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                   "\",\"%08" PRIx32 "\",\"%08" PRIx32 "\"]",
                   e->position[0], e->position[1], e->position[2], e->volume, e->pitch);
        } else if (e->kind == 2) {
            printf("[\"add\",%" PRId32 ",\"hl\"]", e->v[0]);
        } else {
            printf("[\"%s\"", e->kind == 0 ? "cell" : "range");
            const size_t length = e->kind == 0 ? 5u : 6u;
            for (size_t j = 0; j < length; ++j)
                printf(",%" PRId32, e->v[j]);
            putchar(']');
        }
    }
    putchar(']');
}

static void lever_print_drops(const lever_observer *o) {
    putchar('[');
    for (size_t i = 0; i < o->drop_count; ++i) {
        const lever_drop *d = &o->drops[i];
        printf("%s[%zu,\"hl\",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
               "]",
               i == 0 ? "" : ",", i, d->stack.id, d->stack.count, d->stack.damage, d->entity.item.c,
               d->entity.item.b, d->entity.item.f);
    }
    putchar(']');
}

static void lever_print_entropy_items(const lever_observer *o) {
    putchar('[');
    for (size_t i = 0; i < o->drop_count; ++i) {
        const lever_drop *d = &o->drops[i];
        const mc_entity_base_fields *b = &d->entity.base;
        printf("%s{\"ordinal\":%zu,\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64
               "\",\"%016" PRIx64 "\"],\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64
               "\",\"%016" PRIx64 "\"],\"yaw\":\"%08" PRIx32 "\",\"bob\":\"%08" PRIx32
               "\",\"entity_random\":",
               i == 0 ? "" : ",", i, lever_double_bits(b->x), lever_double_bits(b->y),
               lever_double_bits(b->z), lever_double_bits(b->velocity_x),
               lever_double_bits(b->velocity_y), lever_double_bits(b->velocity_z),
               lever_float_bits(b->yaw), lever_float_bits(d->entity.item.d));
        lever_print_random(d->random);
        printf(",\"math_post\":");
        lever_print_random(d->math);
        putchar('}');
    }
    putchar(']');
}

static bool lever_call(fixture *f, const lever_profile *p, const mc_block *block,
                       const char **result, const char **exception) {
    mc_world_runtime *world = p->v[NULL_WORLD] != 0 ? NULL : f->world;
    const int32_t x = p->v[X], y = p->v[Y], z = p->v[Z];
    bool value = true;
    *result = "UNRETURNED";
    *exception = "NONE";
    if (p->v[ENTRY] == 0 || p->v[ENTRY] == 2 || p->v[ENTRY] == 3 || p->v[ENTRY] == 4 ||
        p->v[ENTRY] == 5) {
        mc_world_placement_error error = {0};
        mc_world_storage_status status;
        switch (p->v[ENTRY]) {
            case 0:
                status = mc_world_block_can_place_base(f->registry, block, world, x, y, z, &value,
                                                       &error);
                break;
            case 2:
                status = mc_world_block_can_place(f->registry, block, world, x, y, z, p->v[FACE],
                                                  &value, &error);
                break;
            case 3:
                status = mc_world_runtime_can_place(world, 69, x, y, z, false, p->v[FACE], &value,
                                                    &error);
                break;
            case 5:
                status = mc_world_block_activate(f->registry, block, world, x, y, z, NULL, &value,
                                                 &error);
                break;
            default:
                status = mc_world_block_placed_face(f->registry, block, world, x, y, z, p->v[FACE],
                                                    &error);
                break;
        }
        bool qualified = status == MC_WORLD_STORAGE_OK && error.returned;
        if (qualified) {
            *result = p->v[ENTRY] == 4 ? "VOID" : value ? "true" : "false";
        } else if (!error.native_policy && error.source_exception && !error.returned && value &&
                   error.exception_class != NULL &&
                   strcmp(error.exception_class, "java/lang/NullPointerException") == 0) {
            *exception = "java.lang.NullPointerException";
            qualified = true;
        }
        if (!qualified)
            fprintf(stderr, "Lever entry=%" PRId32 " status=%d site=%s pc=%u native=%d op=%s\n",
                    p->v[ENTRY], (int)status,
                    error.site.class_name == NULL ? "-" : error.site.class_name, error.site.pc,
                    (int)error.native_policy, error.operation == NULL ? "-" : error.operation);
        const bool retired = mc_world_placement_error_retire(&error);
        CHECK(qualified && retired);
        return true;
    }
    /* Same held GAME World convention as the existing random-Block controls.
     * A NULL source World deliberately skips native admission and reaches XR's
     * first actual dereference, carrying typed lever_source_null evidence. */
    mc_world_mutation_error *error = malloc(sizeof(*error));
    CHECK(error != NULL);
    mc_world_mutation_error_init(error);
    if (world != NULL && mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK) {
        free(error);
        CHECK(false);
    }
    bool completed = false;
    switch (p->v[ENTRY]) {
        case 1:
            completed = mc_world_mutation_lever_survive(world, block, x, y, z, &value, error);
            break;
        case 7:
            completed =
                mc_world_mutation_block_neighbor(world, block, x, y, z, p->v[NEIGHBOR], error);
            break;
        case 8:
            completed = mc_world_mutation_block_removed(world, block, x, y, z, error);
            break;
        default:
            break;
    }
    if (world != NULL)
        mc_world_runtime_call_leave(world);
    const bool qualified = completed || (error->lever_source_null && value);
    if (completed)
        *result = p->v[ENTRY] == 1 ? (value ? "true" : "false") : "VOID";
    else if (error->lever_source_null)
        *exception = "java.lang.NullPointerException";
    if (!qualified)
        fprintf(stderr, "Lever entry=%" PRId32 " status=%d hook=%s pc=%u op=%s\n", p->v[ENTRY],
                (int)error->status, error->hook_class == NULL ? "-" : error->hook_class,
                error->hook_bytecode, error->operation == NULL ? "-" : error->operation);
    free(error); /* Flat diagnostic; failed producer publications stay with World. */
    CHECK(qualified);
    return true;
}

static bool lever_project(fixture *f, mc_chunk_state **chunks, lever_observer *o, size_t index,
                          const lever_profile *p, uint8_t *prior) {
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->registry, 69, &block, NULL) == MC_REGISTRY_OK && block != NULL &&
          block->kind == MC_BLOCK_LEVER && block->fields.bn == 69);
    f->world->fields.B = p->v[REMOTE] != 0;
    lever_reset(chunks, p, prior);
    o->event_count = o->drop_count = 0;
    o->failed = false;
    int32_t center_before[2], center_after[2];
    lever_center(chunks, p->v[X], p->v[Y], p->v[Z], center_before);
    lever_queue before = {0}, after = {0};
    CHECK(lever_queue_copy(f->world, &before) && f->world->fields.r != NULL &&
          f->world->fields.b != NULL);
    mc_java_random *const random_identity = f->world->fields.r;
    const lever_random_state world_before = lever_random(random_identity);
    lever_random_state math_before = {0}, math_after = {0};
    const bool math_was_seen = o->math_seen;
    CHECK(!math_was_seen || lever_math(f, &math_before));
    const mc_world_reference_list *const entities = f->world->fields.b;
    const size_t entities_before = entities->count;
    const char *result, *exception;
    const bool called = lever_call(f, p, block, &result, &exception);
    if (!called) {
        free(before.rows);
        return false;
    }
    CHECK(!o->failed && f->world->fields.r == random_identity && f->world->fields.b == entities &&
          entities->count == entities_before + o->drop_count);
    for (size_t i = 0; i < o->drop_count; ++i)
        CHECK(entities->items[entities_before + i] == o->drops[i].identity);
    const lever_random_state world_after = lever_random(random_identity);
    CHECK(!o->math_seen || lever_math(f, &math_after));
    CHECK(lever_queue_copy(f->world, &after));
    lever_center(chunks, p->v[X], p->v[Y], p->v[Z], center_after);
    printf("LEVER\t{\"index\":%zu,\"name\":\"%s\",\"values\":", index, p->name);
    lever_print_ints(p->v, 12);
    printf(",\"result\":\"%s\",\"exception\":\"%s\",\"center_before\":", result, exception);
    lever_print_ints(center_before, 2);
    printf(",\"center_after\":");
    lever_print_ints(center_after, 2);
    printf(",\"changes\":");
    lever_print_changes(chunks, prior);
    printf(",\"queues_before\":");
    lever_print_queue(&before);
    printf(",\"queues_after\":");
    lever_print_queue(&after);
    printf(",\"events\":");
    lever_print_events(o);
    printf(",\"drops\":");
    lever_print_drops(o);
    printf(",\"world_rng_unchanged\":%s}\nENTROPY\t{\"index\":%zu,\"name\":\"%s\","
           "\"world_before\":",
           lever_random_equal(world_before, world_after) ? "true" : "false", index, p->name);
    lever_print_random(world_before);
    printf(",\"world_after\":");
    lever_print_random(world_after);
    printf(",\"math_before\":");
    if (math_was_seen)
        lever_print_random(math_before);
    else
        printf("null");
    printf(",\"math_after\":");
    if (o->math_seen)
        lever_print_random(math_after);
    else
        printf("null");
    printf(",\"items\":");
    lever_print_entropy_items(o);
    printf("}\n");
    free(before.rows);
    free(after.rows);
    CHECK(!ferror(stdout));
    return true;
}

static char *lever_token(char **cursor) {
    if (*cursor == NULL)
        return NULL;
    char *result = *cursor;
    char *end = strpbrk(result, "\t\r\n");
    if (end == NULL) {
        *cursor = NULL;
    } else {
        const char delimiter = *end;
        *end = 0;
        *cursor = delimiter == '\t' ? end + 1 : NULL;
    }
    return result;
}

static bool lever_profiles(fixture *f, mc_chunk_state **chunks, lever_observer *observer,
                           const char *path, uint8_t *prior) {
    FILE *input = fopen(path, "rb");
    CHECK(input != NULL);
    char line[1024];
    size_t count = 0;
    while (fgets(line, sizeof(line), input) != NULL) {
        CHECK(strchr(line, '\n') != NULL && count < LEVER_PROFILES);
        lever_profile p = {0};
        char *cursor = line, *token = lever_token(&cursor);
        CHECK(token != NULL && token[0] != 0 && strlen(token) < sizeof(p.name));
        for (const char *c = token; *c != 0; ++c)
            CHECK(isalnum((unsigned char)*c) || *c == '_' || *c == '-' || *c == '.');
        memcpy(p.name, token, strlen(token) + 1);
        for (size_t i = 0; i < 12; ++i) {
            token = lever_token(&cursor);
            CHECK(token != NULL && token[0] != 0);
            char *end = NULL;
            errno = 0;
            const intmax_t value = strtoimax(token, &end, 10);
            CHECK(errno == 0 && *end == 0 && value >= INT32_MIN && value <= INT32_MAX);
            p.v[i] = (int32_t)value;
        }
        CHECK(lever_token(&cursor) == NULL && p.v[ENTRY] >= 0 && p.v[ENTRY] <= 8 &&
              p.v[WALLS] >= 0 && p.v[WALLS] <= 15 && p.v[FLOOR] >= 0 && p.v[FLOOR] <= 255 &&
              p.v[TARGET] >= 0 && p.v[TARGET] <= 255 &&
              (p.v[NULL_WORLD] == 0 || p.v[NULL_WORLD] == 1) &&
              (p.v[REMOTE] == 0 || p.v[REMOTE] == 1));
        if (p.v[ENTRY] != 6)
            CHECK(lever_project(f, chunks, observer, count, &p, prior));
        ++count;
    }
    CHECK(!ferror(input) && fclose(input) == 0 && count == LEVER_PROFILES);
    return true;
}

static bool lever_run(fixture *f, const char *path) {
    mc_chunk_state *chunks[LEVER_CHUNKS] = {0};
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL);
    for (size_t i = 0; i < LEVER_CHUNKS; ++i) {
        uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
        mc_world_chunk *chunk = NULL;
        CHECK(blocks != NULL &&
              mc_world_store_create_generated(f->store, f->world, 0, (int32_t)(i / 2u),
                                              (int32_t)(i % 2u), &blocks, MC_CHUNK_BLOCKS, &chunk,
                                              NULL) == MC_WORLD_STORAGE_OK &&
              blocks == NULL);
        chunks[i] = mc_world_chunk_state(chunk);
        CHECK(chunks[i] != NULL && mc_chunk_cache_publish(acquisition->cache, (int32_t)(i / 2u),
                                                          (int32_t)(i % 2u), chunks[i]));
    }
    lever_observer *observer = calloc(1, sizeof(*observer));
    uint8_t *prior = malloc(LEVER_CHUNKS * LEVER_ARRAY_BYTES);
    CHECK(observer != NULL && prior != NULL);
    observer->f = f;
    observer->chunks = chunks;
    const mc_world_access_callbacks callbacks = {.cell = lever_observe_cell,
                                                 .range = lever_observe_range,
                                                 .entity_added = lever_observe_add,
                                                 .sound = lever_observe_sound};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    const bool profiles = lever_profiles(f, chunks, observer, path, prior);
    free(prior);
    CHECK(profiles);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    free(observer);
    f->world->fields.B = false;
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity));
    /* Actual published drops stay World/Chunk-owned until ordinary retirement. */
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < LEVER_CHUNKS; ++i)
        CHECK(mc_world_store_retire_chunk(f->store, chunks[i], NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || !mc_math_trig_init(&trig))
        return 2;
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 2;
#endif
    fixture f = {0};
    if (!lever_setup(&f, argv[1], argv[2]) || !lever_run(&f, argv[3]) || !teardown(&f))
        return 1;
    if (printf("COMPLETE\t211\n") <= 0 || fflush(stdout) != 0 || ferror(stdout))
        return 1;
    return 0;
}
