/* Actual CURRENT lifecycle caller; no expected Source results are compiled in. */
#include "current_caller.h"

#include "block/registry/registry_internal.h"
#include "core/java/random_profile_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/entities.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/mutation/redstone_torch/redstone_torch_internal.h"
#include "world/runtime/runtime_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(c)                                                                                 \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "DB_CURRENT %s:%d: %s\n", __FILE__, __LINE__, #c);                     \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
static const size_t lengths[DB_PLANES] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES,
                                          MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                          MC_CHUNK_HEIGHTMAP_BYTES};
static const char *const names[DB_WORLDS] = {"zero",     "age100",  "age101",
                                             "wrap-old", "wrap100", "wrap101"};
static const int64_t times[DB_WORLDS] = {
    0, 100, 101, INT64_MAX - 50, INT64_MIN + 49, INT64_MIN + 50};

static uint64_t double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static db_math_state math_state(const db_fixture *f) {
    db_math_state out = {0};
    if (atomic_load(&f->profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_READY &&
        f->profile->math != NULL) {
        const mc_java_math_random_owner *owner = f->profile->math;
        out =
            (db_math_state){true, atomic_load(&owner->state),
                            double_bits(owner->next_next_gaussian), owner->have_next_next_gaussian};
    }
    return out;
}

static uint8_t *plane(mc_chunk_state *chunk, size_t n) {
    switch (n) {
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

static bool inside(int32_t x, int32_t y, int32_t z) {
    return x >= 0 && x < 32 && y >= 0 && y < 128 && z >= 0 && z < 32;
}

static void put(db_world *w, int32_t x, int32_t y, int32_t z, int32_t id, int32_t metadata) {
    if (!inside(x, y, z))
        return;
    mc_chunk_data *data = w->chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)]->data;
    const size_t cell = (((size_t)x & 15u) << 11u) | (((size_t)z & 15u) << 7u) | (size_t)y;
    const unsigned int shift = (unsigned int)(cell & 1u) * 4u;
    data->blocks[cell] = (uint8_t)id;
    data->metadata[cell >> 1u] = (uint8_t)((data->metadata[cell >> 1u] & ~(15u << shift)) |
                                           (((uint32_t)metadata & 15u) << shift));
}

static void cell(const db_world *w, int32_t x, int32_t y, int32_t z, int32_t out[2]) {
    out[0] = out[1] = 0;
    if (!inside(x, y, z))
        return;
    const mc_chunk_data *data = w->chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)]->data;
    const size_t at = (((size_t)x & 15u) << 11u) | (((size_t)z & 15u) << 7u) | (size_t)y;
    out[0] = data->blocks[at];
    out[1] = (data->metadata[at >> 1u] >> ((at & 1u) * 4u)) & 15;
}

static mc_world_storage_status observer_failure(db_world *w, mc_world_storage_error *error,
                                                bool declared) {
    if (declared)
        w->fixture->observer_refused = true;
    else
        w->fixture->observer_failed = true;
    if (error != NULL)
        *error = (mc_world_storage_error){
            declared ? MC_WORLD_STORAGE_LIMIT : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            declared ? "declared DB native callback refusal" : "DB native observation boundary", 0};
    return declared ? MC_WORLD_STORAGE_LIMIT : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static db_event *event(db_world *w, mc_world_runtime *world, db_event_kind kind,
                       mc_world_storage_error *error) {
    db_fixture *f = w->fixture;
    if (world != w->world || w->role != f->active_role || f->event_count == DB_EVENTS ||
        world->fields.r == NULL) {
        (void)observer_failure(w, error, false);
        return NULL;
    }
    db_event *e = &f->events[f->event_count++];
    memset(e, 0, sizeof(*e));
    e->kind = kind;
    e->world_role = w->role;
    e->world_random = *world->fields.r;
    e->independent_random = f->independent;
    e->uniquifier = atomic_load(&f->profile->uniquifier);
    return e;
}

static bool refuse(db_world *w, int32_t kind) {
    return w->fixture->observer_mode == kind && !w->fixture->observer_refused;
}

static mc_world_storage_status observe_cell(void *context, mc_world_runtime *world, int32_t x,
                                            int32_t y, int32_t z, mc_world_storage_error *error) {
    db_world *w = context;
    db_event *e = event(w, world, DB_CELL, error);
    if (e == NULL)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    e->integers[0] = x;
    e->integers[1] = y;
    e->integers[2] = z;
    e->cell_captured = inside(x, y, z);
    cell(w, x, y, z, &e->integers[3]);
    return refuse(w, 1) ? observer_failure(w, error, true) : MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status observe_range(void *context, mc_world_runtime *world, int32_t x0,
                                             int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                                             int32_t z1, mc_world_storage_error *error) {
    db_world *w = context;
    db_event *e = event(w, world, DB_RANGE, error);
    if (e == NULL)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    const int32_t v[6] = {x0, y0, z0, x1, y1, z1};
    memcpy(e->integers, v, sizeof(v));
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status observe_sound(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, float volume, float pitch,
                                             mc_world_storage_error *error) {
    db_world *w = context;
    db_event *e = event(w, world, DB_SOUND, error);
    if (e == NULL)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    e->name = name;
    e->position[0] = x;
    e->position[1] = y;
    e->position[2] = z;
    e->volume = volume;
    e->pitch = pitch;
    return refuse(w, 2) ? observer_failure(w, error, true) : MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status observe_particle(void *context, mc_world_runtime *world,
                                                const mc_registry_string *name, double x, double y,
                                                double z, double vx, double vy, double vz,
                                                mc_world_storage_error *error) {
    db_world *w = context;
    db_event *e = event(w, world, DB_PARTICLE, error);
    if (e == NULL)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    e->name = name;
    e->position[0] = x;
    e->position[1] = y;
    e->position[2] = z;
    e->motion[0] = vx;
    e->motion[1] = vy;
    e->motion[2] = vz;
    return refuse(w, 3) ? observer_failure(w, error, true) : MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status observe_item(void *context, mc_world_runtime *world,
                                            mc_entity *entity, mc_world_storage_error *error) {
    db_world *w = context;
    db_event *e = event(w, world, DB_ITEM, error);
    if (e == NULL)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    mc_entity_class_literal_view literal;
    if (mc_entity_get_snapshot(entity, &e->item) != MC_ENTITY_OK ||
        mc_entity_class_literal_inspect(e->item.class_literal, &literal) != MC_ENTITY_OK ||
        literal.kind != MC_ENTITY_CLASS_KIND_ITEM || literal.source != MC_ENTITY_CLIENT ||
        strcmp(literal.client_name, "hl") != 0 || e->item.random == NULL ||
        mc_item_stack_fields_get(e->item.stack, &e->stack) != MC_ITEM_STACK_OK)
        return observer_failure(w, error, false);
    /* Actual published Item remains World-owned throughout the cohort. */
    e->entity = entity;
    e->entity_random = *e->item.random;
    w->fixture->item_seen = true;
    e->math = math_state(w->fixture);
    if (!e->math.observed)
        return observer_failure(w, error, false);
    return MC_WORLD_STORAGE_OK;
}

bool db_fixture_open(db_fixture *f, const char *resources, const char *root, bool aging) {
    REQUIRE(f != NULL && resources != NULL && root != NULL);
    memset(f, 0, sizeof(*f));
    f->world_count = aging ? DB_WORLDS : 1u;
    f->active_role = SIZE_MAX;
    REQUIRE(mc_math_trig_init(&f->trig));
    REQUIRE(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK &&
            mc_materials_create(&f->materials));
    REQUIRE(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    REQUIRE(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
            MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    REQUIRE(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK &&
            mc_registry_classes_initialize_blocks(f->registry, NULL) == MC_REGISTRY_OK);
    REQUIRE(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &f->trig,
                                            &f->world_classes, NULL) == MC_WORLD_STORAGE_OK &&
            mc_world_runtime_classes_bind_registry(f->world_classes, f->registry, NULL) ==
                MC_REGISTRY_OK &&
            mc_world_runtime_classes_bind_entities(f->world_classes, f->entities) == MC_ENTITY_OK);
    static const uint16_t empty[] = {0};
    REQUIRE(mc_world_info_name_copy((mc_nbt_string){empty, 0}, SIZE_MAX, &f->name) ==
            MC_WORLD_INFO_OK);
    /* Fresh declared ordinary Random(31), once; never aliases or overwrites r. */
    mc_java_random_seed(&f->independent, INT64_C(31));
    mc_registry_block_view blocks;
    REQUIRE(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    uint8_t map[256] = {0};
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    for (size_t i = 0; i < 2; ++i) {
        REQUIRE(mc_registry_block_get(f->registry, (int32_t)(75u + i), &f->receivers[i], NULL) ==
                    MC_REGISTRY_OK &&
                f->receivers[i] != NULL && f->receivers[i]->kind == MC_BLOCK_REDSTONE_TORCH &&
                f->receivers[i]->subtype.db.a == (i == 1));
    }
    REQUIRE(mc_registry_block_get(f->registry, 55, &f->redstone, NULL) == MC_REGISTRY_OK &&
            f->redstone != NULL);
    f->history_owner = f->registry->db_b;
    REQUIRE(f->history_owner != NULL);
    for (size_t i = 0; i < f->world_count; ++i) {
        db_world *w = &f->worlds[i];
        w->fixture = f;
        w->role = i;
        w->saved_time = times[i];
        char path[4096];
        const int n = snprintf(path, sizeof(path), "%s/%s", root, names[i]);
        REQUIRE(n > 0 && (size_t)n < sizeof(path));
        REQUIRE(mc_world_regions_create(NULL, &w->regions, NULL) == MC_WORLD_STORAGE_OK);
        const mc_world_store_options store = {.session_token = 6381,
                                              .block_map = map,
                                              .block_map_length = 256,
                                              .limits = mc_world_store_default_limits()};
        REQUIRE(mc_world_store_open(path, w->regions, &store, &w->store, NULL) ==
                MC_WORLD_STORAGE_OK);
        REQUIRE(mc_world_provider_create(0, NULL, &w->dimension, NULL) == MC_WORLD_PROVIDER_OK);
        mc_world_runtime_construction_limits limits =
            mc_world_runtime_default_construction_limits();
        w->constructor_uniquifier_before = atomic_load(&f->profile->uniquifier);
        w->constructor_math_before = math_state(f);
        /* The saved Info read is required. DIRECT_METADATA creates fresh Info. */
        REQUIRE(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE,
                                                f->world_classes, &limits, &w->world,
                                                NULL) == MC_WORLD_STORAGE_OK);
        const mc_world_runtime_world_arguments arguments = {
            .store = w->store, .name = f->name, .seed = 23, .dimension = w->dimension};
        REQUIRE(mc_world_runtime_construct_world(w->world, &arguments, NULL) ==
                MC_WORLD_STORAGE_OK);
        int64_t actual;
        REQUIRE(!w->world->fields.s &&
                mc_world_info_time_get(w->world->fields.x, &actual) == MC_WORLD_INFO_OK &&
                actual == times[i]);
        for (size_t k = 0; k < DB_CHUNKS; ++k) {
            REQUIRE(mc_world_runtime_get(w->world, (int32_t)(k / 2u), (int32_t)(k % 2u),
                                         &w->chunks[k]) == MC_CHUNK_ACQUIRE_OK &&
                    w->chunks[k] != NULL && w->chunks[k]->data != NULL &&
                    w->chunks[k]->x == (int32_t)(k / 2u) && w->chunks[k]->z == (int32_t)(k % 2u));
            for (size_t j = 0; j < k; ++j)
                REQUIRE(w->chunks[k] != w->chunks[j]);
        }
        REQUIRE(mc_world_info_time_get(w->world->fields.x, &actual) == MC_WORLD_INFO_OK &&
                actual == times[i]);
        w->constructor_uniquifier_after = atomic_load(&f->profile->uniquifier);
        w->constructor_math_after = math_state(f);
        const mc_world_access_callbacks callbacks = {.cell = observe_cell,
                                                     .range = observe_range,
                                                     .sound = observe_sound,
                                                     .particle = observe_particle,
                                                     .entity_added = observe_item};
        REQUIRE(mc_world_access_create(&callbacks, w, &w->access, NULL) == MC_WORLD_STORAGE_OK &&
                mc_world_runtime_add_world_access(w->world, w->access, NULL) ==
                    MC_WORLD_STORAGE_OK);
    }
    f->opened_complete = true;
    return true;
}

bool db_profile_read(FILE *file, db_profile *out, bool *eof) {
    REQUIRE(file != NULL && out != NULL && eof != NULL);
    char line[4096];
    *eof = false;
    if (fgets(line, sizeof(line), file) == NULL) {
        REQUIRE(!ferror(file));
        *eof = true;
        return true;
    }
    REQUIRE(strchr(line, '\n') != NULL && strchr(line, '\r') == NULL);
    char *cursor = line, *tab = strchr(cursor, '\t');
    REQUIRE(tab != NULL && tab != cursor && (size_t)(tab - cursor) < sizeof(out->name));
    memcpy(out->name, cursor, (size_t)(tab - cursor));
    out->name[tab - cursor] = 0;
    for (const unsigned char *p = (const unsigned char *)out->name; *p != 0; ++p)
        REQUIRE((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-');
    cursor = tab + 1;
    for (size_t i = 0; i < DB_OPERANDS; ++i) {
        char *end = NULL;
        errno = 0;
        long long value = strtoll(cursor, &end, 10);
        REQUIRE(errno == 0 && end != cursor && value >= INT32_MIN && value <= INT32_MAX);
        REQUIRE(i + 1u == DB_OPERANDS ? *end == '\n' && end[1] == 0 : *end == '\t');
        char canonical[16];
        const int count = snprintf(canonical, sizeof(canonical), "%" PRId32, (int32_t)value);
        REQUIRE(count > 0 && (size_t)count == (size_t)(end - cursor) &&
                memcmp(canonical, cursor, (size_t)count) == 0);
        out->values[i] = (int32_t)value;
        cursor = end + 1;
    }
    return true;
}

bool db_prepare(db_fixture *f, const db_profile *profile) {
    REQUIRE(f != NULL && profile != NULL);
    const int32_t *p = profile->values;
    REQUIRE(p[DB_WORLD] >= 0 && (size_t)p[DB_WORLD] < f->world_count && p[DB_LAYOUT] >= 0 &&
            p[DB_LAYOUT] <= 3 && p[DB_ENTRY] >= 0 && p[DB_ENTRY] <= 5 &&
            (p[DB_RECEIVER] == 75 || p[DB_RECEIVER] == 76) && p[DB_RANDOM_MODE] >= 0 &&
            p[DB_RANDOM_MODE] <= 2 && p[DB_OBSERVER_MODE] >= 0 && p[DB_OBSERVER_MODE] <= 3);
    db_world *w = &f->worlds[p[DB_WORLD]];
    for (size_t k = 0; k < DB_CHUNKS; ++k) {
        memset(plane(w->chunks[k], 0), 0, lengths[0]);
        memset(plane(w->chunks[k], 1), 0, lengths[1]);
    }
    const int32_t x = p[DB_X], y = p[DB_Y], z = p[DB_Z], metadata = p[DB_METADATA];
    int32_t sx = x, sy = y, sz = z;
    if (metadata == 1)
        sx = mc_java_i32_sub(x, 1);
    else if (metadata == 2)
        sx = mc_java_i32_add(x, 1);
    else if (metadata == 3)
        sz = mc_java_i32_sub(z, 1);
    else if (metadata == 4)
        sz = mc_java_i32_add(z, 1);
    else
        sy = mc_java_i32_sub(y, 1);
    if (p[DB_LAYOUT] == 1 || p[DB_LAYOUT] == 2) {
        put(w, sx, sy, sz, 1, 0);
        if (p[DB_LAYOUT] == 2) {
            REQUIRE(metadata >= 1 && metadata <= 4);
            put(w, sx, mc_java_i32_add(sy, 1), sz, 70, 1);
        }
    } else if (p[DB_LAYOUT] == 3) {
        put(w, x, mc_java_i32_sub(y, 2), z, 1, 0);
        put(w, x, mc_java_i32_sub(y, 1), z, 70, 1);
    }
    put(w, x, y, z, p[DB_CENTER_ID], metadata);
    w->world->fields.B = p[DB_REMOTE] != 0;
    f->active_role = (size_t)p[DB_WORLD];
    f->event_count = 0;
    f->observer_refused = f->observer_failed = false;
    f->observer_mode = p[DB_OBSERVER_MODE];
    return true;
}

static bool inventory(mc_world_runtime *world, bool ordered, db_scheduled_inventory *out) {
    size_t count = 0;
    if (ordered) {
        out->owner = world->fields.E;
        REQUIRE(mc_scheduled_ordered_snapshot(world->fields.E, out->records, DB_NODES, &count) ==
                MC_SCHEDULED_ORDERED_OK);
    } else {
        out->owner = world->fields.F;
        REQUIRE(mc_scheduled_membership_snapshot(world->fields.F, out->records, DB_NODES, &count) ==
                MC_SCHEDULED_MEMBERSHIP_OK);
    }
    /* Each copied slot gains its own real hold, including E/F aliases. */
    for (size_t i = 0; i < count; ++i) {
        REQUIRE(mc_scheduled_tick_retain((mc_scheduled_tick *)out->records[i]) ==
                MC_SCHEDULED_TICK_OK);
        ++out->count;
        REQUIRE(mc_scheduled_tick_fields_get(out->records[i], &out->fields[i]) ==
                MC_SCHEDULED_TICK_OK);
    }
    return true;
}

bool db_snapshot_take(db_fixture *f, db_snapshot *out) {
    REQUIRE(f != NULL && out != NULL);
    memset(out, 0, sizeof(*out));
    out->world_count = f->world_count;
    for (size_t i = 0; i < f->world_count; ++i) {
        const db_world *owner = &f->worlds[i];
        mc_world_runtime *world = owner->world;
        db_world_snapshot *w = &out->worlds[i];
        w->identity = world;
        REQUIRE(mc_world_info_time_get(world->fields.x, &w->time) == MC_WORLD_INFO_OK &&
                w->time == owner->saved_time);
        w->a = world->fields.a;
        w->remote = world->fields.B;
        w->o = world->fields.o;
        w->s = world->fields.s;
        w->M = world->fields.M;
        w->random_owner = world->fields.r;
        REQUIRE(w->random_owner != NULL);
        w->random = *w->random_owner;
        w->array_bytes = malloc(DB_CHUNKS * DB_ARRAY_BYTES);
        REQUIRE(w->array_bytes != NULL);
        uint8_t *destination = w->array_bytes;
        for (size_t k = 0; k < DB_CHUNKS; ++k)
            for (size_t n = 0; n < DB_PLANES; ++n) {
                w->array_owners[k][n] = plane(owner->chunks[k], n);
                memcpy(destination, w->array_owners[k][n], lengths[n]);
                destination += lengths[n];
            }
        mc_world_light_list_view light;
        REQUIRE(mc_world_runtime_light_list_get(world, &light, NULL) == MC_WORLD_STORAGE_OK &&
                light.count <= DB_NODES);
        w->light_owner = light.identity;
        w->light_count = light.count;
        w->light_capacity = light.capacity;
        for (size_t k = 0; k < light.count; ++k) {
            REQUIRE(mc_world_runtime_light_at(world, k, &w->light_records[k], NULL) ==
                    MC_WORLD_STORAGE_OK);
            const mc_world_light_region_fields *r =
                mc_world_runtime_light_region_fields_get(w->light_records[k]);
            REQUIRE(r != NULL);
            const mc_world_light_type_fields *type = mc_world_runtime_light_type_fields_get(r->a);
            REQUIRE(type != NULL && (type->kind == MC_LIGHT_SKY || type->kind == MC_LIGHT_BLOCK));
            const int32_t v[7] = {(int32_t)type->kind, r->bounds.min_x, r->bounds.min_y,
                                  r->bounds.min_z,     r->bounds.max_x, r->bounds.max_y,
                                  r->bounds.max_z};
            memcpy(w->light_regions[k], v, sizeof(v));
        }
        REQUIRE(inventory(world, true, &w->ordered) && inventory(world, false, &w->membership));
        const mc_world_reference_list *entities = world->fields.b;
        REQUIRE(entities != NULL && entities->count <= DB_NODES);
        w->entity_list_owner = entities;
        w->entity_capacity = entities->capacity;
        for (size_t k = 0; k < entities->count; ++k) {
            mc_entity *entity = entities->items[k];
            REQUIRE(entity != NULL && mc_entity_retain(entity) == MC_ENTITY_OK);
            w->entities[w->entity_count++] = entity;
            REQUIRE(mc_entity_get_snapshot(entity, &w->entity_fields[k]) == MC_ENTITY_OK);
            mc_entity_class_literal_view literal;
            REQUIRE(mc_entity_class_literal_inspect(w->entity_fields[k].class_literal, &literal) ==
                        MC_ENTITY_OK &&
                    literal.kind == MC_ENTITY_CLASS_KIND_ITEM &&
                    mc_item_stack_fields_get(w->entity_fields[k].stack, &w->entity_stacks[k]) ==
                        MC_ITEM_STACK_OK);
        }
    }
    mc_registry_redstone_torch_list_ref history;
    mc_registry_redstone_torch_error error;
    REQUIRE(mc_registry_redstone_torch_list_capture(f->registry, &history) == MC_REGISTRY_OK &&
            history.object == f->history_owner);
    out->history_owner = history.object;
    REQUIRE(mc_registry_redstone_torch_snapshot_copy(&history, &out->history, &error) ==
                MC_REGISTRY_OK &&
            mc_registry_redstone_torch_snapshot_inspect(out->history, &out->history_view) ==
                MC_REGISTRY_OK &&
            out->history_view.size >= 0 && out->history_view.size <= DB_NODES &&
            out->history_view.unused_slots_null);
    for (int32_t i = 0; i < out->history_view.size; ++i)
        REQUIRE(mc_registry_redstone_torch_snapshot_at(out->history, (size_t)i,
                                                       &out->history_slots[i]) == MC_REGISTRY_OK);
    const mc_registry_redstone_set *set = f->redstone->subtype.sm.b;
    out->position_set_owner = set;
    out->position_output = f->redstone->subtype.sm.a;
    REQUIRE(mc_registry_redstone_set_inspect(set, &out->position_set) == MC_REGISTRY_OK &&
            out->position_set.size <= DB_NODES && out->position_set.capacity <= DB_BUCKETS);
    for (size_t i = 0; i < out->position_set.size; ++i)
        REQUIRE(mc_registry_redstone_set_node_at(set, i, &out->position_nodes[i]) ==
                MC_REGISTRY_OK);
    REQUIRE(mc_world_runtime_schedule_class_get(f->world_classes, &out->sequence) ==
            MC_WORLD_STORAGE_OK);
    out->independent = f->independent;
    out->uniquifier = atomic_load(&f->profile->uniquifier);
    out->math = math_state(f);
    return true;
}

void db_snapshot_retire(db_snapshot *snapshot) {
    if (snapshot == NULL)
        return;
    for (size_t i = 0; i < snapshot->world_count; ++i) {
        db_world_snapshot *w = &snapshot->worlds[i];
        free(w->array_bytes);
        for (size_t k = 0; k < w->ordered.count; ++k)
            mc_scheduled_tick_release((mc_scheduled_tick *)w->ordered.records[k]);
        for (size_t k = 0; k < w->membership.count; ++k)
            mc_scheduled_tick_release((mc_scheduled_tick *)w->membership.records[k]);
        for (size_t k = 0; k < w->entity_count; ++k)
            mc_entity_release((mc_entity *)w->entities[k]);
    }
    mc_registry_redstone_torch_snapshot_release(snapshot->history);
    memset(snapshot, 0, sizeof(*snapshot));
}

bool db_invoke(db_fixture *f, const db_profile *profile, db_outcome *out) {
    REQUIRE(f != NULL && profile != NULL && out != NULL);
    memset(out, 0, sizeof(*out));
    const int32_t *p = profile->values;
    REQUIRE(f->active_role == (size_t)p[DB_WORLD]);
    db_world *owner = &f->worlds[f->active_role];
    mc_world_runtime *actual = p[DB_NULL_WORLD] != 0 ? NULL : owner->world;
    const mc_block *receiver = f->receivers[p[DB_RECEIVER] - 75];
    mc_java_random *random = p[DB_RANDOM_MODE] == 0   ? owner->world->fields.r
                             : p[DB_RANDOM_MODE] == 1 ? &f->independent
                                                      : NULL;
    out->error = malloc(sizeof(*out->error));
    REQUIRE(out->error != NULL);
    mc_world_mutation_error_init(out->error);
    mc_registry_error registry;
    const mc_registry_status retained =
        mc_registry_classes_retain(f->registry, MC_REGISTRY_CLIENT, f->profile, &registry);
    if (retained != MC_REGISTRY_OK) {
        out->native_failure = true;
        out->error->registry = registry;
        return false;
    }
    mc_world_storage_error hold = {0};
    if (actual != NULL && mc_world_runtime_call_enter(actual, &hold) != MC_WORLD_STORAGE_OK) {
        out->native_failure = true;
        out->error->cause = hold;
        out->error->status = hold.status;
        (void)mc_registry_classes_release(f->registry, NULL);
        return false;
    }
    out->entered = true;
    bool complete = false;
    switch (p[DB_ENTRY]) {
        case 0:
            complete = mc_world_mutation_block_added(actual, receiver, p[DB_X], p[DB_Y], p[DB_Z],
                                                     out->error);
            break;
        case 1:
            complete = mc_world_mutation_block_removed(actual, receiver, p[DB_X], p[DB_Y], p[DB_Z],
                                                       out->error);
            break;
        case 2:
            complete = mc_world_mutation_block_neighbor(actual, receiver, p[DB_X], p[DB_Y], p[DB_Z],
                                                        p[DB_NEIGHBOR_ID], out->error);
            break;
        case 3:
            complete = mc_world_mutation_block_tick(actual, receiver, p[DB_X], p[DB_Y], p[DB_Z],
                                                    random, out->error);
            break;
        case 4:
            complete = mc_world_mutation_redstone_torch_powered(actual, receiver, p[DB_X], p[DB_Y],
                                                                p[DB_Z], &out->value, out->error);
            break;
        case 5:
            complete = mc_world_mutation_redstone_torch_burnout(actual, receiver, p[DB_X], p[DB_Y],
                                                                p[DB_Z], p[DB_RECORD] != 0,
                                                                &out->value, out->error);
            break;
    }
    if (actual != NULL)
        mc_world_runtime_call_leave(actual);
    REQUIRE(mc_registry_classes_release(f->registry, NULL) == MC_REGISTRY_OK);
    out->returned = complete;
    out->observer_refused = f->observer_refused;
    out->source_null = out->error->redstone_torch_source_null || out->error->torch_source_null;
    out->native_failure = !complete && !out->source_null && !out->observer_refused;
    REQUIRE(!f->observer_failed && f->registry->db_b == f->history_owner);
    return true;
}

void db_outcome_retire(db_outcome *out) {
    if (out != NULL) {
        free(out->error);
        memset(out, 0, sizeof(*out));
    }
}

/* Complete terminal teardown is deliberately a separate checked operation;
 * it is not run during a Source row and does not process queues or ticks. */
#define RD_CHUNKS DB_CHUNKS
#define CHECK REQUIRE
#include "terminal_current.inc"
#undef CHECK
#undef RD_CHUNKS

bool db_fixture_close(db_fixture *f) {
    REQUIRE(f != NULL && f->opened_complete);
    for (size_t i = 0; i < f->world_count; ++i) {
        db_world *w = &f->worlds[i];
        bool removed = false;
        REQUIRE(mc_world_runtime_remove_world_access(w->world, w->access, &removed, NULL) ==
                    MC_WORLD_STORAGE_OK &&
                removed && mc_world_access_free(w->access, NULL) == MC_WORLD_STORAGE_OK);
        w->access = NULL;
        w->terminal_chunk_count = mc_world_store_chunk_count(w->store);
        REQUIRE(rd_terminal_chunks(w->world, w->store, w->dimension, w->chunks) &&
                rd_terminal_world_free(w->world));
        w->world = NULL;
        mc_world_provider_release(w->dimension);
        w->dimension = NULL;
        REQUIRE(mc_world_store_free_checked(&w->store, NULL) == MC_WORLD_STORAGE_OK);
        mc_world_regions_free(w->regions);
        w->regions = NULL;
    }
    mc_world_info_name_release(f->name);
    f->name = NULL;
    REQUIRE(mc_world_runtime_classes_free(f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    f->world_classes = NULL;
    REQUIRE(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    f->registry = NULL;
    REQUIRE(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    f->entities = NULL;
    mc_java_random_profile_free(f->profile);
    f->profile = NULL;
    mc_materials_destroy(f->materials);
    f->materials = NULL;
    REQUIRE(mc_resource_files_free(f->files) == MC_REGISTRY_OK);
    f->files = NULL;
    f->opened_complete = false;
    return true;
}
