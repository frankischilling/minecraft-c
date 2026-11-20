/* Authored actual-reference caller. No original expected values are compiled in. */
#include "item/stack/source_internal.h"
#define MC_PLAYER_STATISTICS_WORLD_SEED INT64_C(23)
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 4
#define main rd_unused_statistics_fixture_main
#include "../../../client/statistics/test_player_dispatch.c"
#undef main
#include "block/registry/redstone_set_internal.h"
#include "block/registry/registry_internal.h"
#include "core/java/random_profile_internal.h"
#include "item/registry/registry_internal.h"
#include "mc/core/java/java.h"
#include "mc/entity/classes/classes.h"
#include "mc/item/use/use.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/lighting.h"
#include "mc/world/runtime/placement.h"
#include "mc/world/runtime/world_access.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/runtime_internal.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum {
    RD_CHUNKS = 4,
    RD_PLANES = 5,
    RD_EVENTS = 64,
    RD_ENTITIES = 64,
    RD_SET_NODES = 256,
    RD_SET_BUCKETS = 1024,
    RD_ALIASES = 4096
};

/* Terminal definitions must precede the teardown adapters that call them. */
// clang-format off
#include "terminal_cleanup.inc"
#include "statistics_setup.inc"
#include "statistics_teardown.inc"
#include "support_setup.inc"
// clang-format on

static const size_t rd_lengths[RD_PLANES] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES,
                                             MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                             MC_CHUNK_HEIGHTMAP_BYTES};

typedef struct rd_alias {
    const void *pointer;
    uint64_t serial;
    unsigned int kind;
} rd_alias;

typedef struct rd_aliases {
    rd_alias values[RD_ALIASES];
    size_t count;
    bool overflow;
} rd_aliases;

typedef struct rd_set {
    mc_registry_redstone_set_view view;
    mc_registry_redstone_node_view nodes[RD_SET_NODES];
    size_t roots[RD_SET_BUCKETS], node_ids[RD_SET_NODES], key_ids[RD_SET_NODES];
    size_t next[RD_SET_NODES], parent[RD_SET_NODES], left[RD_SET_NODES], right[RD_SET_NODES],
        previous[RD_SET_NODES];
    size_t set_id, map_id, table_id;
    bool output;
} rd_set;

typedef struct rd_queue {
    int32_t (*rows)[7];
    size_t count;
} rd_queue;

typedef struct rd_event {
    unsigned int kind;
    int32_t v[6];
    mc_item_stack_fields stack;
} rd_event;

typedef struct rd_unexpected {
    mc_entity_snapshot snapshot;
    mc_entity_class_literal_view literal;
    mc_item_stack_fields stack;
    mc_java_random random;
    uint64_t math_seed, math_gaussian;
    bool math_have, math_ready;
} rd_unexpected;

typedef struct rd_context {
    mc_world_runtime *world;
    mc_registry_classes *registry;
    mc_java_random_profile *profile;
    mc_entity *player;
    mc_client_stats_writer *writer;
    mc_chunk_state *chunks[RD_CHUNKS];
    mc_world_access *access;
    mc_item_stack *stack;
    rd_aliases aliases;
    rd_event events[RD_EVENTS];
    rd_unexpected unexpected[RD_ENTITIES];
    size_t event_count, cells, unexpected_count;
    int32_t mode;
    bool refused, overflow;
} rd_context;

static const char *rd_bool(bool value) {
    return value ? "true" : "false";
}

static uint64_t rd_d_bits(double value) {
    uint64_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static uint32_t rd_f_bits(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static void rd_string(const char *value) {
    if (value == NULL) {
        fputs("null", stdout);
        return;
    }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)value; *p != 0; ++p) {
        if (*p == '"' || *p == '\\')
            putchar('\\');
        if (*p < 32)
            printf("\\u%04x", (unsigned int)*p);
        else
            putchar(*p);
    }
    putchar('"');
}

static void rd_ints(const int32_t *v, size_t count) {
    putchar('[');
    for (size_t i = 0; i < count; ++i)
        printf("%s%" PRId32, i == 0 ? "" : ",", v[i]);
    putchar(']');
}

static void rd_stack(const mc_item_stack_fields *s) {
    if (s == NULL) {
        fputs("null", stdout);
        return;
    }
    const int32_t v[] = {s->count, s->animation, s->id, s->damage};
    rd_ints(v, 4);
}

static uint8_t *rd_plane(mc_chunk_state *chunk, size_t plane) {
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

static void rd_put(rd_context *c, int32_t x, int32_t y, int32_t z, int32_t id) {
    if (x < 0 || x > 31 || y < 0 || y > 127 || z < 0 || z > 31)
        return;
    c->chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)]
        ->data->blocks[(((size_t)x & 15u) << 11u) | (((size_t)z & 15u) << 7u) | (size_t)y] =
        (uint8_t)id;
}

static void rd_cell(rd_context *c, int32_t x, int32_t y, int32_t z, int32_t out[2]) {
    out[0] = out[1] = 0;
    if (x < 0 || x > 31 || y < 0 || y > 127 || z < 0 || z > 31)
        return;
    const mc_chunk_data *d = c->chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)]->data;
    const size_t i = (((size_t)x & 15u) << 11u) | (((size_t)z & 15u) << 7u) | (size_t)y;
    out[0] = d->blocks[i];
    out[1] = (d->metadata[i >> 1u] >> ((i & 1u) * 4u)) & 15;
}

static void rd_prepare(rd_context *c, const int32_t *p, bool use, uint8_t *prior) {
    for (size_t n = 0; n < RD_CHUNKS; ++n) {
        for (size_t k = 0; k < RD_PLANES; ++k)
            memset(rd_plane(c->chunks[n], k), k == 2 ? 15 : 0, rd_lengths[k]);
        c->chunks[n]->data->minimum_height = 0;
        c->chunks[n]->terrain_populated = true;
    }
    if (use) {
        rd_put(c, p[7], mc_java_i32_sub(p[8], 1), p[9], p[11]);
        rd_put(c, p[4], p[5], p[6], p[10]);
        rd_put(c, p[7], p[8], p[9], p[12]);
        c->world->fields.B = p[13] != 0;
    } else {
        rd_put(c, p[0], mc_java_i32_sub(p[1], 1), p[2], p[4]);
        rd_put(c, p[0], p[1], p[2], p[3]);
    }
    for (size_t n = 0; n < RD_CHUNKS; ++n)
        for (size_t k = 0; k < RD_PLANES; ++k) {
            memcpy(prior, rd_plane(c->chunks[n], k), rd_lengths[k]);
            prior += rd_lengths[k];
        }
}

static size_t rd_id(rd_aliases *a, const void *pointer, uint64_t serial, unsigned int kind) {
    if (pointer == NULL && serial == 0)
        return 0;
    for (size_t i = 0; i < a->count; ++i)
        if (a->values[i].pointer == pointer && a->values[i].serial == serial &&
            a->values[i].kind == kind)
            return i + 1;
    if (a->count == RD_ALIASES) {
        a->overflow = true;
        return SIZE_MAX;
    }
    a->values[a->count] = (rd_alias){pointer, serial, kind};
    return ++a->count;
}

static bool rd_set_copy(rd_context *c, const mc_block *block, rd_set *s) {
    memset(s, 0, sizeof(*s));
    const mc_registry_redstone_set *set = block->subtype.sm.b;
    CHECK(set != NULL && mc_registry_redstone_set_inspect(set, &s->view) == MC_REGISTRY_OK &&
          s->view.size <= RD_SET_NODES && s->view.capacity <= RD_SET_BUCKETS);
    for (size_t i = 0; i < s->view.size; ++i)
        CHECK(mc_registry_redstone_set_node_at(set, i, &s->nodes[i]) == MC_REGISTRY_OK);
    for (size_t bucket = 0; bucket < s->view.capacity; ++bucket) {
        for (size_t i = 0; i < s->view.size; ++i)
            if (s->nodes[i].bucket == bucket) {
                const mc_registry_redstone_node_view *n = &s->nodes[i];
                if (s->roots[bucket] == 0)
                    s->roots[bucket] = rd_id(&c->aliases, NULL, n->identity, 1);
                s->node_ids[i] = rd_id(&c->aliases, NULL, n->identity, 1);
                s->key_ids[i] = rd_id(&c->aliases, NULL, n->key.identity, 2);
                s->next[i] = rd_id(&c->aliases, NULL, n->next, 1);
                if (n->tree) {
                    s->parent[i] = rd_id(&c->aliases, NULL, n->parent, 1);
                    s->left[i] = rd_id(&c->aliases, NULL, n->left, 1);
                    s->right[i] = rd_id(&c->aliases, NULL, n->right, 1);
                    s->previous[i] = rd_id(&c->aliases, NULL, n->previous, 1);
                }
            }
    }
    s->set_id = rd_id(&c->aliases, set, 0, 0);
    s->map_id = rd_id(&c->aliases, s->view.map, 0, 0);
    s->table_id = rd_id(&c->aliases, s->view.table, 0, 0);
    s->output = block->subtype.sm.a;
    CHECK(!c->aliases.overflow && s->set_id != SIZE_MAX && s->map_id != SIZE_MAX &&
          s->table_id != SIZE_MAX);
    return true;
}

static void rd_set_json(const rd_set *s) {
    printf("{\"set_id\":%zu,\"map_id\":%zu,\"table_id\":%zu,\"size\":%zu,\"capacity\":%" PRIu32
           ",\"threshold\":%" PRId32 ",\"mod_count\":%" PRId32 ",\"load_factor_bits\":\"%08" PRIx32
           "\",\"output_flag\":%s,\"roots\":[",
           s->set_id, s->map_id, s->table_id, s->view.size, s->view.capacity, s->view.threshold,
           s->view.mod_count, rd_f_bits(s->view.load_factor), rd_bool(s->output));
    for (size_t i = 0; i < s->view.capacity; ++i)
        printf("%s%zu", i == 0 ? "" : ",", s->roots[i]);
    fputs("],\"nodes\":[", stdout);
    for (size_t i = 0; i < s->view.size; ++i) {
        const mc_registry_redstone_node_view *n = &s->nodes[i];
        int32_t hash;
        memcpy(&hash, &n->hash, sizeof(hash));
        printf("%s{\"id\":%zu,\"bucket\":%" PRIu32 ",\"hash\":%" PRId32
               ",\"key_id\":%zu,\"xyz\":[%" PRId32 ",%" PRId32 ",%" PRId32
               "],\"next\":%zu,\"tree\":%s",
               i == 0 ? "" : ",", s->node_ids[i], n->bucket, hash, s->key_ids[i], n->key.x,
               n->key.y, n->key.z, s->next[i], rd_bool(n->tree));
        if (n->tree)
            printf(",\"parent\":%zu,\"left\":%zu,\"right\":%zu,\"prev\":%zu,\"red\":%s",
                   s->parent[i], s->left[i], s->right[i], s->previous[i], rd_bool(n->red));
        putchar('}');
    }
    fputs("],\"pending\":[", stdout);
    for (size_t i = 0; i < s->view.size; ++i)
        printf("%s[%" PRId32 ",%" PRId32 ",%" PRId32 "]", i == 0 ? "" : ",", s->nodes[i].key.x,
               s->nodes[i].key.y, s->nodes[i].key.z);
    fputs("]}", stdout);
}

static bool rd_queue_copy(mc_world_runtime *world, rd_queue *q) {
    mc_world_light_list_view list;
    CHECK(mc_world_runtime_light_list_get(world, &list, NULL) == MC_WORLD_STORAGE_OK &&
          list.count <= MC_LIGHT_QUEUE_LIMIT);
    q->count = list.count;
    q->rows = list.count == 0 ? NULL : calloc(list.count, sizeof(*q->rows));
    CHECK(list.count == 0 || q->rows != NULL);
    for (size_t i = 0; i < list.count; ++i) {
        const mc_world_light_region *region = NULL;
        CHECK(mc_world_runtime_light_at(world, i, &region, NULL) == MC_WORLD_STORAGE_OK);
        const mc_world_light_region_fields *r = mc_world_runtime_light_region_fields_get(region);
        CHECK(r != NULL);
        const mc_world_light_type_fields *t = mc_world_runtime_light_type_fields_get(r->a);
        CHECK(t != NULL && (t->kind == MC_LIGHT_SKY || t->kind == MC_LIGHT_BLOCK));
        q->rows[i][0] = (int32_t)t->kind;
        q->rows[i][1] = r->bounds.min_x;
        q->rows[i][2] = r->bounds.min_y;
        q->rows[i][3] = r->bounds.min_z;
        q->rows[i][4] = r->bounds.max_x;
        q->rows[i][5] = r->bounds.max_y;
        q->rows[i][6] = r->bounds.max_z;
    }
    return true;
}

static void rd_queue_json(const rd_queue *q) {
    putchar('[');
    for (size_t i = 0; i < q->count; ++i) {
        if (i != 0)
            putchar(',');
        rd_ints(q->rows[i], 7);
    }
    putchar(']');
}

static void rd_changes(rd_context *c, const uint8_t *prior) {
    size_t count = 0;
    putchar('[');
    for (size_t n = 0; n < RD_CHUNKS; ++n)
        for (size_t k = 0; k < RD_PLANES; ++k) {
            const uint8_t *after = rd_plane(c->chunks[n], k);
            for (size_t i = 0; i < rd_lengths[k]; ++i)
                if (prior[i] != after[i]) {
                    printf("%s[%zu,%zu,%zu,%u,%u]", count++ == 0 ? "" : ",", n, k, i,
                           (unsigned int)prior[i], (unsigned int)after[i]);
                }
            prior += rd_lengths[k];
        }
    putchar(']');
}

static mc_world_storage_status rd_refuse(rd_context *c, mc_world_storage_error *error,
                                         bool declared) {
    c->refused = declared;
    if (!declared)
        c->overflow = true;
    if (error != NULL)
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                          "declared Redstone observer native refusal", 0};
    return MC_WORLD_STORAGE_LIMIT;
}

static mc_world_storage_status rd_cell_event(void *opaque, mc_world_runtime *world, int32_t x,
                                             int32_t y, int32_t z, mc_world_storage_error *error) {
    rd_context *c = opaque;
    if (world != c->world || c->event_count == RD_EVENTS || c->stack == NULL)
        return rd_refuse(c, error, false);
    rd_event *e = &c->events[c->event_count++];
    memset(e, 0, sizeof(*e));
    e->v[0] = x;
    e->v[1] = y;
    e->v[2] = z;
    rd_cell(c, x, y, z, &e->v[3]);
    if (mc_item_stack_fields_get(c->stack, &e->stack) != MC_ITEM_STACK_OK)
        return rd_refuse(c, error, false);
    ++c->cells;
    if (c->cells == 1 && c->mode == 1)
        return rd_refuse(c, error, true);
    if (c->cells == 1 && (c->mode == 2 || c->mode == 3)) {
        const mc_item_stack_status status = c->mode == 2
                                                ? mc_item_stack_source_count_set(c->stack, 99)
                                                : mc_item_stack_source_id_set(c->stack, 332);
        if (status != MC_ITEM_STACK_OK || c->event_count == RD_EVENTS)
            return rd_refuse(c, error, false);
        e = &c->events[c->event_count++];
        memset(e, 0, sizeof(*e));
        e->kind = c->mode == 2 ? 2u : 3u;
        e->v[0] = c->mode == 2 ? 99 : 332;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status rd_range_event(void *opaque, mc_world_runtime *world, int32_t a,
                                              int32_t b, int32_t d, int32_t e, int32_t f, int32_t g,
                                              mc_world_storage_error *error) {
    rd_context *c = opaque;
    if (world != c->world || c->event_count == RD_EVENTS)
        return rd_refuse(c, error, false);
    c->events[c->event_count++] = (rd_event){.kind = 1, .v = {a, b, d, e, f, g}};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status rd_add_event(void *opaque, mc_world_runtime *world,
                                            mc_entity *entity, mc_world_storage_error *error) {
    rd_context *c = opaque;
    if (world != c->world || c->unexpected_count == RD_ENTITIES)
        return rd_refuse(c, error, false);
    rd_unexpected *u = &c->unexpected[c->unexpected_count++];
    memset(u, 0, sizeof(*u));
    if (mc_entity_get_snapshot(entity, &u->snapshot) != MC_ENTITY_OK ||
        mc_entity_class_literal_inspect(u->snapshot.class_literal, &u->literal) != MC_ENTITY_OK ||
        u->snapshot.random == NULL)
        return rd_refuse(c, error, false);
    u->random = *u->snapshot.random;
    if (u->literal.kind == MC_ENTITY_CLASS_KIND_ITEM) {
        if (mc_item_stack_fields_get(u->snapshot.stack, &u->stack) != MC_ITEM_STACK_OK)
            return rd_refuse(c, error, false);
        if (atomic_load(&c->profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_READY &&
            c->profile->math != NULL) {
            const mc_java_math_random_owner *m = c->profile->math;
            u->math_ready = true;
            u->math_seed = atomic_load(&m->state);
            u->math_gaussian = rd_d_bits(m->next_next_gaussian);
            u->math_have = m->have_next_next_gaussian;
        }
    }
    return MC_WORLD_STORAGE_OK;
}

static void rd_events(const rd_context *c) {
    putchar('[');
    for (size_t i = 0; i < c->event_count; ++i) {
        const rd_event *e = &c->events[i];
        if (i != 0)
            putchar(',');
        if (e->kind == 0) {
            fputs("{\"event\":\"cell\",\"xyz\":", stdout);
            rd_ints(e->v, 3);
            fputs(",\"cell\":", stdout);
            rd_ints(&e->v[3], 2);
            fputs(",\"stack\":", stdout);
            rd_stack(&e->stack);
            putchar('}');
        } else if (e->kind == 1) {
            fputs("[\"range\",", stdout);
            for (size_t j = 0; j < 6; ++j)
                printf("%s%" PRId32, j == 0 ? "" : ",", e->v[j]);
            putchar(']');
        } else
            printf("{\"event\":\"%s\",\"value\":%" PRId32 "}",
                   e->kind == 2 ? "count-store" : "id-store", e->v[0]);
    }
    putchar(']');
}

static void rd_random(const mc_java_random *r) {
    if (r == NULL) {
        fputs("null", stdout);
        return;
    }
    printf("{\"seed\":\"%012" PRIx64 "\",\"gaussian\":\"%016" PRIx64 "\",\"have\":%s}", r->state,
           rd_d_bits(r->next_next_gaussian), rd_bool(r->have_next_next_gaussian));
}

static bool rd_random_same(const mc_java_random *a, const mc_java_random *b) {
    return a->state == b->state && a->have_next_next_gaussian == b->have_next_next_gaussian &&
           rd_d_bits(a->next_next_gaussian) == rd_d_bits(b->next_next_gaussian);
}

static void rd_unexpected_json(const rd_context *c) {
    putchar('[');
    for (size_t i = 0; i < c->unexpected_count; ++i) {
        const rd_unexpected *u = &c->unexpected[i];
        const mc_entity_base_fields *b = &u->snapshot.base;
        printf("%s{\"event\":\"unexpected-add\",\"class\":", i == 0 ? "" : ",");
        rd_string(u->literal.client_name);
        printf(",\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
               "\"],\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
               "\"],\"yaw\":\"%08" PRIx32 "\",\"entity_random\":",
               rd_d_bits(b->x), rd_d_bits(b->y), rd_d_bits(b->z), rd_d_bits(b->velocity_x),
               rd_d_bits(b->velocity_y), rd_d_bits(b->velocity_z), rd_f_bits(b->yaw));
        rd_random(&u->random);
        if (u->literal.kind == MC_ENTITY_CLASS_KIND_ITEM) {
            fputs(",\"stack\":", stdout);
            rd_stack(&u->stack);
            printf(",\"bob\":\"%08" PRIx32 "\",\"math_post\":", rd_f_bits(u->snapshot.item.d));
            if (u->math_ready)
                printf("{\"seed\":\"%012" PRIx64 "\",\"gaussian\":\"%016" PRIx64 "\",\"have\":%s}",
                       u->math_seed, u->math_gaussian, rd_bool(u->math_have));
            else
                fputs("null", stdout);
        }
        putchar('}');
    }
    putchar(']');
}

static bool rd_stats(rd_context *c, int32_t out[2]) {
    CHECK(c->writer != NULL && c->registry->statistics.E[331] != NULL &&
          c->registry->statistics.E[332] != NULL);
    CHECK(mc_client_stats_writer_value(c->writer, c->registry->statistics.E[331], &out[0], NULL) ==
              MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(c->writer, c->registry->statistics.E[332], &out[1], NULL) ==
              MC_CLIENT_STATS_OK);
    return true;
}

static const char *rd_exception(bool source, const char *name, bool refused) {
    if (refused)
        return "NATIVE_CALLBACK_REFUSAL";
    if (!source)
        return "NATIVE_ERROR";
    if (name != NULL && strcmp(name, "java/lang/NullPointerException") == 0)
        return "java.lang.NullPointerException";
    return name == NULL ? "NATIVE_UNCLASSIFIED_SOURCE_EXCEPTION" : name;
}

static bool rd_row(rd_context *c, size_t index, const char *name, const int32_t *p, bool use,
                   uint8_t *prior, rd_set *sets) {
    const mc_block *block = NULL;
    const mc_item *item = NULL;
    const mc_registry_item_array *items = NULL;
    CHECK(mc_registry_block_get(c->registry, 55, &block, NULL) == MC_REGISTRY_OK && block != NULL &&
          block->kind == MC_BLOCK_REDSTONE && block->classes == c->registry);
    if (use)
        CHECK(mc_registry_item_c_get(c->registry, &items, NULL) == MC_REGISTRY_OK &&
              mc_registry_item_c_at(c->registry, items, 331, &item, NULL) == MC_REGISTRY_OK &&
              item != NULL && item->kind == MC_ITEM_REDSTONE && item->classes == c->registry);
    rd_prepare(c, p, use, prior);
    c->event_count = c->cells = c->unexpected_count = 0;
    c->refused = c->overflow = false;
    c->mode = use ? p[15] : 0;
    mc_item_stack *stack = NULL;
    mc_item_stack_fields before = {0}, after = {0};
    if (use)
        CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(stack, p[0], p[1], p[2]) == MC_ITEM_STACK_OK &&
              mc_item_stack_fields_get(stack, &before) == MC_ITEM_STACK_OK);
    c->stack = stack;
    int32_t clicked_before[2] = {0}, clicked_after[2] = {0}, target_before[2], target_after[2];
    if (use)
        rd_cell(c, p[4], p[5], p[6], clicked_before);
    rd_cell(c, p[use ? 7 : 0], p[use ? 8 : 1], p[use ? 9 : 2], target_before);
    rd_queue queues[2] = {{0}};
    CHECK(rd_queue_copy(c->world, &queues[0]) && rd_set_copy(c, block, &sets[0]) &&
          c->world->fields.r != NULL);
    mc_java_random *const random_owner = c->world->fields.r;
    const mc_java_random random_before = *random_owner;
    int32_t stats_before[2] = {0}, stats_after[2] = {0};
    if (use)
        CHECK(rd_stats(c, stats_before) && c->player != NULL && c->player->local_player != NULL);
    const int32_t health_before = use ? c->player->local_player->fields.living.Y : 0;
    mc_item_use_call *call = NULL;
    mc_item_use_error error = {0};
    mc_item_use_state state = {0};
    mc_world_placement_error predicate = {0};
    int status;
    bool returned = false, value = false, source_exception = false, source_error = false,
         native_policy = false;
    unsigned int source_pc = 0;
    const char *source_class = NULL, *source_method = NULL, *descriptor = NULL, *operation = NULL,
               *exception = "NONE";
    mc_world_storage_status world_status = MC_WORLD_STORAGE_OK;
    size_t support_count = 0, query_count = 0, retained = 0;
    if (use) {
        status = (int)mc_item_use_on_block_begin(
            c->registry, (p[14] & 2) != 0 ? NULL : stack, (p[14] & 4) != 0 ? NULL : c->player,
            (p[14] & 1) != 0 ? NULL : c->world, p[4], p[5], p[6], p[3], &call, &error);
        CHECK(call != NULL && mc_item_use_inspect(call, &state) && !state.active);
        returned = state.returned;
        value = state.result;
        source_pc = state.source_pc;
        source_exception = error.source_exception;
        source_error = error.source_error;
        native_policy = error.native_policy;
        source_class = error.site.class_name;
        source_method = error.site.method;
        descriptor = error.site.descriptor;
        operation = error.operation;
        world_status = error.world.status;
        retained = error.retained_producers;
        if (error.placement != NULL) {
            support_count = error.placement->support_query_count;
            query_count = error.placement->query_count;
            CHECK(support_count == 0 || error.placement->support_queries[0] != NULL);
        }
        if (status != MC_ITEM_USE_OK)
            exception = rd_exception(source_exception, error.exception_class, c->refused);
    } else {
        value = true;
        status = (int)mc_world_block_can_place_base(c->registry, block, p[5] != 0 ? NULL : c->world,
                                                    p[0], p[1], p[2], &value, &predicate);
        returned = predicate.returned;
        source_exception = predicate.source_exception;
        source_error = predicate.source_error;
        native_policy = predicate.native_policy;
        source_pc = predicate.site.pc;
        source_class = predicate.site.class_name;
        source_method = predicate.site.method;
        descriptor = predicate.site.descriptor;
        operation = predicate.operation;
        world_status = predicate.cause.status;
        support_count = predicate.support_query_count;
        query_count = predicate.query_count;
        retained = predicate.retained_producers;
        CHECK(support_count == 0 || predicate.support_queries[0] != NULL);
        if (status != MC_WORLD_STORAGE_OK)
            exception = rd_exception(source_exception, predicate.exception_class, false);
    }
    CHECK(!c->overflow && rd_queue_copy(c->world, &queues[1]) && rd_set_copy(c, block, &sets[1]));
    if (use)
        CHECK(mc_item_stack_fields_get(stack, &after) == MC_ITEM_STACK_OK &&
              rd_stats(c, stats_after));
    if (use)
        rd_cell(c, p[4], p[5], p[6], clicked_after);
    rd_cell(c, p[use ? 7 : 0], p[use ? 8 : 1], p[use ? 9 : 2], target_after);
    const bool owner_same = c->world->fields.r == random_owner;
    const mc_java_random *random_after = c->world->fields.r;
    const bool random_unchanged =
        owner_same && random_after != NULL && rd_random_same(&random_before, random_after);
    printf("%s\t{\"index\":%zu,\"name\":", use ? "USE" : "SUPPORT", index);
    rd_string(name);
    fputs(",\"values\":", stdout);
    rd_ints(p, use ? 16u : 6u);
    printf(",\"result\":\"%s\",\"exception\":",
           returned ? (value ? "true" : "false") : "UNRETURNED");
    rd_string(exception);
    fputs(",\"stack_before\":", stdout);
    rd_stack(use ? &before : NULL);
    fputs(",\"stack_after\":", stdout);
    rd_stack(use ? &after : NULL);
    fputs(",\"clicked_before\":", stdout);
    if (use)
        rd_ints(clicked_before, 2);
    else
        fputs("null", stdout);
    fputs(",\"clicked_after\":", stdout);
    if (use)
        rd_ints(clicked_after, 2);
    else
        fputs("null", stdout);
    fputs(",\"target_before\":", stdout);
    rd_ints(target_before, 2);
    fputs(",\"target_after\":", stdout);
    rd_ints(target_after, 2);
    fputs(",\"changes\":", stdout);
    rd_changes(c, prior);
    fputs(",\"queues_before\":", stdout);
    rd_queue_json(&queues[0]);
    fputs(",\"queues_after\":", stdout);
    rd_queue_json(&queues[1]);
    fputs(",\"set_before\":", stdout);
    rd_set_json(&sets[0]);
    fputs(",\"set_after\":", stdout);
    rd_set_json(&sets[1]);
    fputs(",\"events\":", stdout);
    rd_events(c);
    printf(",\"unexpected_adds\":%zu,\"remote\":%s,\"world_o\":%s,\"registered_block_class\":"
           "\"sm\",\"registered_block_id\":%" PRId32
           ",\"registered_item_class\":%s,\"registered_item_slot\":%s,\"registered_owners_same\":%"
           "s,\"used331_delta\":",
           c->unexpected_count, rd_bool(c->world->fields.B), rd_bool(c->world->fields.o),
           block->fields.bn, use ? "\"ie\"" : "null", use ? "331" : "null",
           rd_bool(c->registry->blocks.m[55] == block &&
                   (!use || (items != NULL && item != NULL && item->classes == c->registry))));
    if (use)
        printf("%" PRId32, mc_java_i32_sub(stats_after[0], stats_before[0]));
    else
        fputs("null", stdout);
    fputs(",\"used332_delta\":", stdout);
    if (use)
        printf("%" PRId32, mc_java_i32_sub(stats_after[1], stats_before[1]));
    else
        fputs("null", stdout);
    fputs(",\"health_before\":", stdout);
    if (use)
        printf("%" PRId32, health_before);
    else
        fputs("null", stdout);
    fputs(",\"health_after\":", stdout);
    if (use)
        printf("%" PRId32, c->player->local_player->fields.living.Y);
    else
        fputs("null", stdout);
    printf(",\"world_random_same\":%s,\"world_rng_unchanged\":%s}\n", rd_bool(owner_same),
           rd_bool(random_unchanged));
    printf("OWN_ENTROPY\t{\"index\":%zu,\"name\":", index);
    rd_string(name);
    fputs(",\"world_before\":", stdout);
    rd_random(&random_before);
    fputs(",\"world_after\":", stdout);
    rd_random(random_after);
    printf(",\"world_random_same\":%s,\"unexpected_entities\":", rd_bool(owner_same));
    rd_unexpected_json(c);
    fputs("}\n", stdout);
    printf("REDSTONE_NATIVE\t{\"index\":%zu,\"domain\":\"%s\",\"status\":%d,\"native_policy\":%s,"
           "\"source_exception\":%s,\"source_error\":%s,\"observer_refused\":%s,\"source_pc\":%u,"
           "\"class\":",
           index, use ? "item" : "placement", status, rd_bool(native_policy),
           rd_bool(source_exception), rd_bool(source_error), rd_bool(c->refused), source_pc);
    rd_string(source_class);
    fputs(",\"method\":", stdout);
    rd_string(source_method);
    fputs(",\"descriptor\":", stdout);
    rd_string(descriptor);
    fputs(",\"operation\":", stdout);
    rd_string(operation);
    printf(",\"world_status\":%d,\"support_count\":%zu,\"predicate_query_count\":%zu,\"retained_"
           "producers\":%zu}\n",
           (int)world_status, support_count, query_count, retained);
    CHECK(fflush(stdout) == 0 && !ferror(stdout));
    if (use)
        CHECK(mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK && call == NULL);
    else
        CHECK(mc_world_placement_error_retire(&predicate));
    c->stack = NULL;
    mc_item_stack_release(stack);
    free(queues[0].rows);
    free(queues[1].rows);
    return true;
}

static char *rd_next(char **cursor) {
    if (*cursor == NULL)
        return NULL;
    char *out = *cursor;
    size_t n = strcspn(out, "\t\r\n");
    char end = out[n];
    if (end == 0)
        *cursor = NULL;
    else {
        out[n] = 0;
        *cursor = out + n + 1;
        if (end != '\t') {
            *cursor += strspn(*cursor, "\r\n");
            if (**cursor == 0)
                *cursor = NULL;
        }
    }
    return out;
}

static bool rd_run(rd_context *c, const char *profiles, bool use) {
    for (size_t i = 0; i < RD_CHUNKS; ++i) {
        if (c->chunks[i] == NULL)
            CHECK(mc_world_runtime_get(c->world, (int32_t)(i / 2u), (int32_t)(i % 2u),
                                       &c->chunks[i]) == MC_CHUNK_ACQUIRE_OK);
        CHECK(c->chunks[i] != NULL && c->chunks[i]->data != NULL);
        for (size_t j = 0; j < i; ++j)
            CHECK(c->chunks[i] != c->chunks[j] && c->chunks[i]->data != c->chunks[j]->data);
    }
    const mc_world_access_callbacks callbacks = {
        .cell = rd_cell_event, .range = rd_range_event, .entity_added = rd_add_event};
    CHECK(mc_world_access_create(&callbacks, c, &c->access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(c->world, c->access, NULL) == MC_WORLD_STORAGE_OK);
    size_t bytes = 0;
    for (size_t i = 0; i < RD_PLANES; ++i)
        bytes += RD_CHUNKS * rd_lengths[i];
    uint8_t *prior = malloc(bytes);
    rd_set *sets = calloc(2, sizeof(*sets));
    CHECK(prior != NULL && sets != NULL);
    FILE *input = fopen(profiles, "rb");
    CHECK(input != NULL);
    char line[4096];
    size_t rows = 0;
    while (fgets(line, sizeof(line), input) != NULL) {
        CHECK(strchr(line, '\n') != NULL);
        char *cursor = line, *token = rd_next(&cursor);
        CHECK(token != NULL && token[0] != 0 && strlen(token) < 128);
        char name[128];
        memcpy(name, token, strlen(token) + 1);
        int32_t p[16] = {0};
        for (size_t i = 0; i < (use ? 16u : 6u); ++i) {
            token = rd_next(&cursor);
            CHECK(token != NULL && token[0] != 0);
            char *end = NULL;
            errno = 0;
            long long value = strtoll(token, &end, 10);
            CHECK(errno == 0 && *end == 0 && value >= INT32_MIN && value <= INT32_MAX);
            p[i] = (int32_t)value;
        }
        CHECK(rd_next(&cursor) == NULL && rd_row(c, rows, name, p, use, prior, sets));
        ++rows;
    }
    CHECK(!ferror(input) && fclose(input) == 0 && rows == (use ? 42u : 12u));
    free(prior);
    free(sets);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(c->world, c->access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(c->access, NULL) == MC_WORLD_STORAGE_OK);
    c->access = NULL;
    return true;
}

int main(int argc, char **argv) {
    if (argc != 5 || (strcmp(argv[4], "use") != 0 && strcmp(argv[4], "support") != 0) ||
        !mc_math_trig_init(&trig))
        return 2;
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 2;
#endif
    const bool use = strcmp(argv[4], "use") == 0;
    rd_context *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return 1;
    bool ok = false;
    if (use) {
        fixture *f = calloc(1, sizeof(*f));
        if (f == NULL)
            return 1;
        if (rd_use_setup(f, argv[1], argv[2], true, c->chunks)) {
            c->world = f->world;
            c->registry = f->app.registry;
            c->profile = f->app.profile;
            c->player = f->player;
            c->writer = f->app.stats_writer;
            ok = rd_run(c, argv[3], true) && rd_use_teardown(f, c->chunks);
        }
        free(f);
    } else {
        rd_support_fixture *f = calloc(1, sizeof(*f));
        if (f == NULL)
            return 1;
        if (rd_support_setup(f, argv[1], argv[2])) {
            c->world = f->world;
            c->registry = f->registry;
            c->profile = f->profile;
            ok = rd_run(c, argv[3], false) && rd_support_teardown(f, c->chunks);
        }
        free(f);
    }
    free(c);
    if (!ok)
        return 1;
    printf("COMPLETE\t%s\t%s\nREDSTONE_NATIVE_COMPLETE\t%s\t%zu\n", argv[4], use ? "42" : "12",
           argv[4], checks);
    return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
}
