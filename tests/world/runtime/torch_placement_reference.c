/* Real constructors, GAME owners and original-declared Chunk inputs. The
 * included Player control main is never executed. No source Random is seeded,
 * replaced or advanced by the observer. */
#define main mc_torch_placement_player_fixture_main
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
    TORCH_PROFILES = 193,
    TORCH_CHUNKS = 4,
    TORCH_PLANES = 5,
    TORCH_ARRAY_BYTES = MC_CHUNK_BLOCKS + 3 * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES,
    TORCH_EVENTS = 4096,
    TORCH_DROPS = 64
};

typedef struct torch_profile {
    char name[96];
    int32_t v[12];
} torch_profile;

enum { ENTRY, X, Y, Z, METADATA, FACE, NEIGHBOR, WALLS, FLOOR, TARGET, NULL_WORLD, NULL_RANDOM };

static const size_t torch_lengths[TORCH_PLANES] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES,
                                                   MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                                   MC_CHUNK_HEIGHTMAP_BYTES};

static bool torch_setup(fixture *f, const char *resources, const char *directory) {
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
    static const uint16_t name[] = {'T', 'o', 'r', 'c', 'h'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){name, 5}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = TORCH_CHUNKS;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
          MC_PLAYER_OK);
    return true;
}

static uint8_t *torch_plane(mc_chunk_state *chunk, size_t plane) {
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

static void torch_cell(mc_chunk_state **chunks, int32_t x, int32_t y, int32_t z, int32_t id) {
    if (x < 0 || x >= 32 || z < 0 || z >= 32 || y < 0 || y >= 128)
        return;
    mc_chunk_state *chunk = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)];
    const size_t index = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
    chunk->data->blocks[index] = (uint8_t)id;
}

static void torch_center(mc_chunk_state **chunks, int32_t x, int32_t y, int32_t z, int32_t out[2]) {
    out[0] = out[1] = 0;
    if (x < 0 || x >= 32 || z < 0 || z >= 32 || y < 0 || y >= 128)
        return;
    const mc_chunk_data *data = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)]->data;
    const size_t index = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
    out[0] = data->blocks[index];
    out[1] = (data->metadata[index >> 1u] >> ((index & 1u) * 4u)) & 15;
}

static void torch_reset(mc_chunk_state **chunks, const torch_profile *p, uint8_t *prior) {
    for (size_t chunk = 0; chunk < TORCH_CHUNKS; ++chunk) {
        for (size_t plane = 0; plane < TORCH_PLANES; ++plane)
            memset(torch_plane(chunks[chunk], plane), plane == 2 ? 15 : 0, torch_lengths[plane]);
        chunks[chunk]->data->minimum_height = 0;
        chunks[chunk]->terrain_populated = true;
    }
    const int32_t x = p->v[X], y = p->v[Y], z = p->v[Z];
    if ((p->v[WALLS] & 1) != 0)
        torch_cell(chunks, mc_java_i32_sub(x, 1), y, z, 1);
    if ((p->v[WALLS] & 2) != 0)
        torch_cell(chunks, mc_java_i32_add(x, 1), y, z, 1);
    if ((p->v[WALLS] & 4) != 0)
        torch_cell(chunks, x, y, mc_java_i32_sub(z, 1), 1);
    if ((p->v[WALLS] & 8) != 0)
        torch_cell(chunks, x, y, mc_java_i32_add(z, 1), 1);
    torch_cell(chunks, x, mc_java_i32_sub(y, 1), z, p->v[FLOOR]);
    torch_cell(chunks, x, y, z, p->v[TARGET]);
    if (x >= 0 && x < 32 && z >= 0 && z < 32 && y >= 0 && y < 128) {
        mc_chunk_data *data = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)]->data;
        const size_t index = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
        const unsigned int shift = (unsigned int)(index & 1u) * 4u;
        data->metadata[index >> 1u] = (uint8_t)((data->metadata[index >> 1u] & ~(15u << shift)) |
                                                (((uint32_t)p->v[METADATA] & 15u) << shift));
    }
    size_t offset = 0;
    for (size_t chunk = 0; chunk < TORCH_CHUNKS; ++chunk)
        for (size_t plane = 0; plane < TORCH_PLANES; ++plane) {
            if (prior != NULL)
                memcpy(prior + offset, torch_plane(chunks[chunk], plane), torch_lengths[plane]);
            offset += torch_lengths[plane];
        }
}

typedef struct torch_random_state {
    uint64_t seed, gaussian;
    bool have;
} torch_random_state;

static uint64_t torch_double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t torch_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static torch_random_state torch_random(const mc_java_random *random) {
    return (torch_random_state){random->state, torch_double_bits(random->next_next_gaussian),
                                random->have_next_next_gaussian};
}

/* Quiescent field-only reads. No Math active use or cold-holder initialization. */
static bool torch_math(const fixture *f, torch_random_state *out) {
    if (atomic_load(&f->profile->holder_phase) != MC_JAVA_RANDOM_HOLDER_READY ||
        f->profile->math == NULL)
        return false;
    const mc_java_math_random_owner *math = f->profile->math;
    const uint64_t seed = atomic_load(&math->state);
    *out = (torch_random_state){seed, torch_double_bits(math->next_next_gaussian),
                                math->have_next_next_gaussian};
    return true;
}

static bool torch_random_equal(torch_random_state a, torch_random_state b) {
    return a.seed == b.seed && a.gaussian == b.gaussian && a.have == b.have;
}

static void torch_print_random(torch_random_state value) {
    printf("{\"seed\":\"%012" PRIx64 "\",\"gaussian\":\"%016" PRIx64 "\",\"have\":%s}", value.seed,
           value.gaussian, value.have ? "true" : "false");
}

typedef struct torch_event {
    unsigned int kind;
    int32_t v[6];
} torch_event;

typedef struct torch_drop {
    mc_entity_snapshot entity;
    mc_item_stack_fields stack;
    torch_random_state random, math;
    mc_entity *identity;
} torch_drop;

typedef struct torch_observer {
    fixture *f;
    mc_chunk_state **chunks;
    torch_event events[TORCH_EVENTS];
    torch_drop drops[TORCH_DROPS];
    size_t event_count, drop_count;
    bool failed, math_seen;
} torch_observer;

static mc_world_storage_status torch_observer_fail(torch_observer *o,
                                                   mc_world_storage_error *error) {
    o->failed = true;
    if (error != NULL)
        *error = (mc_world_storage_error){.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          .operation = "Torch observer capacity or actual Item"};
    return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status torch_observe_cell(void *context, mc_world_runtime *world, int32_t x,
                                                  int32_t y, int32_t z,
                                                  mc_world_storage_error *error) {
    torch_observer *o = context;
    if (world != o->f->world || o->event_count == TORCH_EVENTS)
        return torch_observer_fail(o, error);
    int32_t center[2];
    torch_center(o->chunks, x, y, z, center);
    o->events[o->event_count++] = (torch_event){0, {x, y, z, center[0], center[1], 0}};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status torch_observe_range(void *context, mc_world_runtime *world,
                                                   int32_t x0, int32_t y0, int32_t z0, int32_t x1,
                                                   int32_t y1, int32_t z1,
                                                   mc_world_storage_error *error) {
    torch_observer *o = context;
    if (world != o->f->world || o->event_count == TORCH_EVENTS)
        return torch_observer_fail(o, error);
    o->events[o->event_count++] = (torch_event){1, {x0, y0, z0, x1, y1, z1}};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status torch_observe_add(void *context, mc_world_runtime *world,
                                                 mc_entity *entity, mc_world_storage_error *error) {
    torch_observer *o = context;
    if (world != o->f->world || o->event_count == TORCH_EVENTS || o->drop_count == TORCH_DROPS)
        return torch_observer_fail(o, error);
    torch_drop *drop = &o->drops[o->drop_count];
    mc_entity_class_literal_view literal;
    if (mc_entity_get_snapshot(entity, &drop->entity) != MC_ENTITY_OK ||
        mc_entity_class_literal_inspect(drop->entity.class_literal, &literal) != MC_ENTITY_OK ||
        literal.kind != MC_ENTITY_CLASS_KIND_ITEM || literal.source != MC_ENTITY_CLIENT ||
        strcmp(literal.client_name, "hl") != 0 || drop->entity.random == NULL ||
        mc_item_stack_fields_get(drop->entity.stack, &drop->stack) != MC_ITEM_STACK_OK ||
        !torch_math(o->f, &drop->math))
        return torch_observer_fail(o, error);
    drop->random = torch_random(drop->entity.random);
    drop->identity = entity;
    o->events[o->event_count++] = (torch_event){2, {(int32_t)o->drop_count, 0, 0, 0, 0, 0}};
    ++o->drop_count;
    o->math_seen = true;
    return MC_WORLD_STORAGE_OK;
}

typedef struct torch_queue {
    int32_t (*rows)[7];
    size_t count;
} torch_queue;

static bool torch_queue_copy(mc_world_runtime *world, torch_queue *out) {
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

static void torch_print_ints(const int32_t *v, size_t count) {
    putchar('[');
    for (size_t i = 0; i < count; ++i)
        printf("%s%" PRId32, i == 0 ? "" : ",", v[i]);
    putchar(']');
}

static void torch_print_queue(const torch_queue *queue) {
    putchar('[');
    for (size_t i = 0; i < queue->count; ++i) {
        if (i != 0)
            putchar(',');
        torch_print_ints(queue->rows[i], 7);
    }
    putchar(']');
}

static void torch_print_changes(mc_chunk_state **chunks, const uint8_t *prior) {
    size_t offset = 0, count = 0;
    putchar('[');
    for (size_t chunk = 0; chunk < TORCH_CHUNKS; ++chunk)
        for (size_t plane = 0; plane < TORCH_PLANES; ++plane) {
            const uint8_t *after = torch_plane(chunks[chunk], plane);
            for (size_t i = 0; i < torch_lengths[plane]; ++i)
                if (prior[offset + i] != after[i])
                    printf("%s[%zu,%zu,%zu,%u,%u]", count++ == 0 ? "" : ",", chunk, plane, i,
                           (unsigned int)prior[offset + i], (unsigned int)after[i]);
            offset += torch_lengths[plane];
        }
    putchar(']');
}

static void torch_print_events(const torch_observer *o) {
    putchar('[');
    for (size_t i = 0; i < o->event_count; ++i) {
        const torch_event *e = &o->events[i];
        if (i != 0)
            putchar(',');
        if (e->kind == 2) {
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

static void torch_print_drops(const torch_observer *o) {
    putchar('[');
    for (size_t i = 0; i < o->drop_count; ++i) {
        const torch_drop *d = &o->drops[i];
        printf("%s[%zu,\"hl\",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
               "]",
               i == 0 ? "" : ",", i, d->stack.id, d->stack.count, d->stack.damage, d->entity.item.c,
               d->entity.item.b, d->entity.item.f);
    }
    putchar(']');
}

static void torch_print_entropy_items(const torch_observer *o) {
    putchar('[');
    for (size_t i = 0; i < o->drop_count; ++i) {
        const torch_drop *d = &o->drops[i];
        const mc_entity_base_fields *b = &d->entity.base;
        printf("%s{\"ordinal\":%zu,\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64
               "\",\"%016" PRIx64 "\"],\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64
               "\",\"%016" PRIx64 "\"],\"yaw\":\"%08" PRIx32 "\",\"bob\":\"%08" PRIx32
               "\",\"entity_random\":",
               i == 0 ? "" : ",", i, torch_double_bits(b->x), torch_double_bits(b->y),
               torch_double_bits(b->z), torch_double_bits(b->velocity_x),
               torch_double_bits(b->velocity_y), torch_double_bits(b->velocity_z),
               torch_float_bits(b->yaw), torch_float_bits(d->entity.item.d));
        torch_print_random(d->random);
        printf(",\"math_post\":");
        torch_print_random(d->math);
        putchar('}');
    }
    putchar(']');
}

static bool torch_call(fixture *f, const torch_profile *p, const mc_block *block,
                       const char **result, const char **exception) {
    mc_world_runtime *world = p->v[NULL_WORLD] != 0 ? NULL : f->world;
    const int32_t x = p->v[X], y = p->v[Y], z = p->v[Z];
    bool value = true;
    *result = "UNRETURNED";
    *exception = "NONE";
    if (p->v[ENTRY] == 0 || p->v[ENTRY] == 2 || p->v[ENTRY] == 3 || p->v[ENTRY] == 4) {
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
                status = mc_world_runtime_can_place(world, 50, x, y, z, false, p->v[FACE], &value,
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
            fprintf(stderr, "Torch entry=%" PRId32 " status=%d site=%s pc=%u native=%d op=%s\n",
                    p->v[ENTRY], (int)status,
                    error.site.class_name == NULL ? "-" : error.site.class_name, error.site.pc,
                    (int)error.native_policy, error.operation == NULL ? "-" : error.operation);
        const bool retired = mc_world_placement_error_retire(&error);
        CHECK(qualified && retired);
        return true;
    }
    /* Same held GAME World convention as the existing random-Block controls.
     * A NULL source World deliberately skips native admission and reaches VM's
     * first actual dereference, carrying typed torch_source_null evidence. */
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
            completed = mc_world_mutation_torch_floor(world, block, x, y, z, &value, error);
            break;
        case 5:
            completed = mc_world_mutation_block_added(world, block, x, y, z, error);
            break;
        case 6:
            completed = mc_world_mutation_torch_survive(world, block, x, y, z, &value, error);
            break;
        case 7:
            completed =
                mc_world_mutation_block_neighbor(world, block, x, y, z, p->v[NEIGHBOR], error);
            break;
        case 8:
            completed = mc_world_mutation_block_tick(
                world, block, x, y, z, p->v[NULL_RANDOM] != 0 ? NULL : f->world->fields.r, error);
            break;
        default:
            break;
    }
    if (world != NULL)
        mc_world_runtime_call_leave(world);
    const bool qualified = completed || (error->torch_source_null && value);
    if (completed)
        *result = p->v[ENTRY] == 1 || p->v[ENTRY] == 6 ? (value ? "true" : "false") : "VOID";
    else if (error->torch_source_null)
        *exception = "java.lang.NullPointerException";
    if (!qualified)
        fprintf(stderr, "Torch entry=%" PRId32 " status=%d hook=%s pc=%u op=%s\n", p->v[ENTRY],
                (int)error->status, error->hook_class == NULL ? "-" : error->hook_class,
                error->hook_bytecode, error->operation == NULL ? "-" : error->operation);
    free(error); /* Flat diagnostic; failed producer publications stay with World. */
    CHECK(qualified);
    return true;
}

static bool torch_project(fixture *f, mc_chunk_state **chunks, torch_observer *o, size_t index,
                          const torch_profile *p, uint8_t *prior) {
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->registry, 50, &block, NULL) == MC_REGISTRY_OK && block != NULL &&
          block->kind == MC_BLOCK_TORCH && block->fields.bn == 50);
    torch_reset(chunks, p, prior);
    o->event_count = o->drop_count = 0;
    o->failed = false;
    int32_t center_before[2], center_after[2];
    torch_center(chunks, p->v[X], p->v[Y], p->v[Z], center_before);
    torch_queue before = {0}, after = {0};
    CHECK(torch_queue_copy(f->world, &before) && f->world->fields.r != NULL &&
          f->world->fields.b != NULL);
    mc_java_random *const random_identity = f->world->fields.r;
    const torch_random_state world_before = torch_random(random_identity);
    torch_random_state math_before = {0}, math_after = {0};
    const bool math_was_seen = o->math_seen;
    CHECK(!math_was_seen || torch_math(f, &math_before));
    const mc_world_reference_list *const entities = f->world->fields.b;
    const size_t entities_before = entities->count;
    const char *result, *exception;
    const bool called = torch_call(f, p, block, &result, &exception);
    if (!called) {
        free(before.rows);
        return false;
    }
    CHECK(!o->failed && f->world->fields.r == random_identity && f->world->fields.b == entities &&
          entities->count == entities_before + o->drop_count);
    for (size_t i = 0; i < o->drop_count; ++i)
        CHECK(entities->items[entities_before + i] == o->drops[i].identity);
    const torch_random_state world_after = torch_random(random_identity);
    CHECK(!o->math_seen || torch_math(f, &math_after));
    CHECK(torch_queue_copy(f->world, &after));
    torch_center(chunks, p->v[X], p->v[Y], p->v[Z], center_after);
    printf("TORCH\t{\"index\":%zu,\"name\":\"%s\",\"values\":", index, p->name);
    torch_print_ints(p->v, 12);
    printf(",\"result\":\"%s\",\"exception\":\"%s\",\"center_before\":", result, exception);
    torch_print_ints(center_before, 2);
    printf(",\"center_after\":");
    torch_print_ints(center_after, 2);
    printf(",\"changes\":");
    torch_print_changes(chunks, prior);
    printf(",\"queues_before\":");
    torch_print_queue(&before);
    printf(",\"queues_after\":");
    torch_print_queue(&after);
    printf(",\"events\":");
    torch_print_events(o);
    printf(",\"drops\":");
    torch_print_drops(o);
    printf(",\"world_rng_unchanged\":%s}\nENTROPY\t{\"index\":%zu,\"name\":\"%s\","
           "\"world_before\":",
           torch_random_equal(world_before, world_after) ? "true" : "false", index, p->name);
    torch_print_random(world_before);
    printf(",\"world_after\":");
    torch_print_random(world_after);
    printf(",\"math_before\":");
    if (math_was_seen)
        torch_print_random(math_before);
    else
        printf("null");
    printf(",\"math_after\":");
    if (o->math_seen)
        torch_print_random(math_after);
    else
        printf("null");
    printf(",\"items\":");
    torch_print_entropy_items(o);
    printf("}\n");
    free(before.rows);
    free(after.rows);
    CHECK(!ferror(stdout));
    return true;
}

static char *torch_token(char **cursor) {
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

static bool torch_profiles(fixture *f, mc_chunk_state **chunks, torch_observer *observer,
                           const char *path, uint8_t *prior) {
    FILE *input = fopen(path, "rb");
    CHECK(input != NULL);
    char line[1024];
    size_t count = 0;
    while (fgets(line, sizeof(line), input) != NULL) {
        CHECK(strchr(line, '\n') != NULL && count < TORCH_PROFILES);
        torch_profile p = {0};
        char *cursor = line, *token = torch_token(&cursor);
        CHECK(token != NULL && token[0] != 0 && strlen(token) < sizeof(p.name));
        for (const char *c = token; *c != 0; ++c)
            CHECK(isalnum((unsigned char)*c) || *c == '_' || *c == '-' || *c == '.');
        memcpy(p.name, token, strlen(token) + 1);
        for (size_t i = 0; i < 12; ++i) {
            token = torch_token(&cursor);
            CHECK(token != NULL && token[0] != 0);
            char *end = NULL;
            errno = 0;
            const intmax_t value = strtoimax(token, &end, 10);
            CHECK(errno == 0 && *end == 0 && value >= INT32_MIN && value <= INT32_MAX);
            p.v[i] = (int32_t)value;
        }
        CHECK(torch_token(&cursor) == NULL && p.v[ENTRY] >= 0 && p.v[ENTRY] <= 8 &&
              p.v[WALLS] >= 0 && p.v[WALLS] <= 15 && p.v[FLOOR] >= 0 && p.v[FLOOR] <= 255 &&
              p.v[TARGET] >= 0 && p.v[TARGET] <= 255 &&
              (p.v[NULL_WORLD] == 0 || p.v[NULL_WORLD] == 1) &&
              (p.v[NULL_RANDOM] == 0 || p.v[NULL_RANDOM] == 1));
        CHECK(torch_project(f, chunks, observer, count++, &p, prior));
    }
    CHECK(!ferror(input) && fclose(input) == 0 && count == TORCH_PROFILES);
    return true;
}

/* Actual live-static identity and retained-frame controls are separate from
 * the 193 original profiles. Restore every declared mutation before checking. */
static bool torch_owner_controls(fixture *f, mc_chunk_state **chunks) {
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->registry, 50, &block, NULL) == MC_REGISTRY_OK && block != NULL);
    size_t fence = SIZE_MAX;
    for (size_t i = 0; i < 96; ++i)
        if (strcmp(mc_registry_block_static_name(i), "ba") == 0)
            fence = i;
    CHECK(fence != SIZE_MAX);
    mc_block *const saved = f->registry->blocks.named[fence];
    const size_t bindings = f->world->persistent_bindings, borrows = f->registry->runtime_borrows;
    torch_profile p = {.v = {0, 8, 64, 8, 0, 1, 0, 1, 0, 0, 0, 0}};
    torch_reset(chunks, &p, NULL);
    f->registry->blocks.named[fence] = NULL;
    bool value = false;
    mc_world_placement_error error = {0};
    const mc_world_storage_status shortcut =
        mc_world_block_can_place_base(f->registry, block, f->world, 8, 64, 8, &value, &error);
    f->registry->blocks.named[fence] = saved;
    CHECK(shortcut == MC_WORLD_STORAGE_OK && value && error.returned &&
          error.support_query_count == 1 && error.support_queries[0] != NULL &&
          error.query_count == 0 && f->world->persistent_bindings == bindings + 1 &&
          f->registry->runtime_borrows == borrows + 1);
    const mc_world_placement_work *const work = error.work;
    const mc_world_solid_support_detail *const support = error.support_queries[0];
    value = false;
    CHECK(mc_world_block_can_place_base(f->registry, block, f->world, 8, 64, 8, &value, &error) ==
              MC_WORLD_STORAGE_BUSY &&
          !value && error.work == work && error.support_queries[0] == support && error.returned);
    CHECK(mc_world_placement_error_retire(&error) && error.work == NULL &&
          error.support_query_count == 0 && f->world->persistent_bindings == bindings &&
          f->registry->runtime_borrows == borrows);
    p.v[WALLS] = 0;
    p.v[FLOOR] = 85;
    torch_reset(chunks, &p, NULL);
    f->registry->blocks.named[fence] = NULL;
    value = true;
    const mc_world_storage_status null_fence =
        mc_world_block_can_place_base(f->registry, block, f->world, 8, 64, 8, &value, &error);
    f->registry->blocks.named[fence] = saved;
    CHECK(null_fence != MC_WORLD_STORAGE_OK && value && !error.returned && !error.native_policy &&
          error.source_exception && error.exception_class != NULL &&
          strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
          error.floor.pc == 22 && error.support_query_count == 5 && error.query_count == 1 &&
          mc_world_placement_error_retire(&error));
    f->registry->blocks.named[fence] = f->registry->blocks.m[1];
    value = true;
    const mc_world_storage_status rebound =
        mc_world_block_can_place_base(f->registry, block, f->world, 8, 64, 8, &value, &error);
    f->registry->blocks.named[fence] = saved;
    CHECK(rebound == MC_WORLD_STORAGE_OK && !value && error.returned && error.floor.pc == 33 &&
          mc_world_placement_error_retire(&error));
    p.v[FLOOR] = 1;
    torch_reset(chunks, &p, NULL);
    f->registry->blocks.named[fence] = NULL;
    value = false;
    const mc_world_storage_status solid =
        mc_world_block_can_place_base(f->registry, block, f->world, 8, 64, 8, &value, &error);
    f->registry->blocks.named[fence] = saved;
    CHECK(solid == MC_WORLD_STORAGE_OK && value && error.query_count == 0 &&
          error.support_query_count == 5 && mc_world_placement_error_retire(&error));
    CHECK(f->world->persistent_bindings == bindings && f->registry->runtime_borrows == borrows);
    return true;
}

static bool torch_run(fixture *f, const char *path) {
    mc_chunk_state *chunks[TORCH_CHUNKS] = {0};
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL);
    for (size_t i = 0; i < TORCH_CHUNKS; ++i) {
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
    torch_observer *observer = calloc(1, sizeof(*observer));
    uint8_t *prior = malloc(TORCH_CHUNKS * TORCH_ARRAY_BYTES);
    CHECK(observer != NULL && prior != NULL);
    observer->f = f;
    observer->chunks = chunks;
    const mc_world_access_callbacks callbacks = {.cell = torch_observe_cell,
                                                 .range = torch_observe_range,
                                                 .entity_added = torch_observe_add};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    const bool profiles = torch_profiles(f, chunks, observer, path, prior);
    free(prior);
    CHECK(profiles);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    free(observer);
    CHECK(torch_owner_controls(f, chunks));
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity));
    /* Actual published drops stay World/Chunk-owned until ordinary retirement. */
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->registry, 50, &block, NULL) == MC_REGISTRY_OK);
    bool value = true;
    mc_world_placement_error error = {0};
    CHECK(mc_world_block_can_place_base(f->registry, block, f->world, 8, 64, 8, &value, &error) !=
              MC_WORLD_STORAGE_OK &&
          value && !error.returned && error.native_policy &&
          mc_world_placement_error_retire(&error));
    for (size_t i = 0; i < TORCH_CHUNKS; ++i)
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
    if (!torch_setup(&f, argv[1], argv[2]) || !torch_run(&f, argv[3]) || !teardown(&f))
        return 1;
    if (printf("COMPLETE\t193\n") <= 0 || fflush(stdout) != 0 || ferror(stdout))
        return 1;
    return 0;
}
