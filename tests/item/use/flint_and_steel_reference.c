/* Genuine World/Player/Registry/statistics owners and serialized prepared
 * Chunks are shared with the authored Ore fixture. Its entry is not invoked. */
#define MC_ORE_ENTRY mc_flint_unused_ore_fixture_main
#include "../../world/runtime/test_redstone_ore.c"
#undef MC_ORE_ENTRY

#include "../../../src/client/statistics/internal.h"
#include "../../../src/item/stack/source_internal.h"
#include "../../../src/item/use/flint_and_steel/flint_and_steel_internal.h"
#include "mc/item/combat/combat.h"
#include "mc/item/registry/runtime.h"

typedef struct fs_stat {
    int32_t id, value;
} fs_stat;

typedef struct fs_profile {
    ro_profile owners;
    const char *operation, *sound_action;
    int32_t xyz[3], face, stack_id, count, damage, animation;
    int64_t world_seed, item_seed;
    bool remote, null_world, null_player, null_stack, null_item_random;
    bool null_fire, null_provider, null_access, null_stats_writer;
    bool null_use_stats, null_break_stats, null_item_table, item_random_world;
    int32_t errors[3];
    fs_stat stats[FR_MAX_CELLS];
    size_t stat_count;
    bool stats_dirty;
} fs_profile;

typedef struct fs_observation {
    ro_observation owners;
    const fs_profile *profile;
    const mc_item *flint;
    mc_java_random *prepared_item_random;
    mc_registry_item_array *item_table;
    mc_statistic **use_stats, **break_stats;
    mc_block *fire;
    size_t fire_index, events, kinds[6];
    int32_t fire_id, target[3];
    bool valid;
    mc_world_mutation_error *action;
} fs_observation;

static void fs_shift(const fs_profile *p, int32_t out[3]) {
    memcpy(out, p->xyz, sizeof p->xyz);
    if (p->face == 0)
        out[1] = mc_java_i32_sub(out[1], 1);
    if (p->face == 1)
        out[1] = mc_java_i32_add(out[1], 1);
    if (p->face == 2)
        out[2] = mc_java_i32_sub(out[2], 1);
    if (p->face == 3)
        out[2] = mc_java_i32_add(out[2], 1);
    if (p->face == 4)
        out[0] = mc_java_i32_sub(out[0], 1);
    if (p->face == 5)
        out[0] = mc_java_i32_add(out[0], 1);
}

/* This passive view never calls a World getter or loads an omitted Chunk.
 * NULL means an in-domain coordinate outside the declared loaded footprint. */
static void fs_cell(const fs_observation *o, int32_t x, int32_t y, int32_t z) {
    int32_t cell_value[2];
    if (x < -32000000 || x >= 32000000 || z < -32000000 || z > 32000000 || y < 0 || y >= 128)
        printf("[0,0]");
    else if (fr_cell_read(&o->owners.base, x, y, z, cell_value))
        printf("[%d,%d]", cell_value[0], cell_value[1]);
    else
        printf("null");
}

static bool fs_stack(const fs_observation *o) {
    if (o->owners.stack == NULL) {
        printf("null");
        return true;
    }
    mc_item_stack_fields fields;
    if (mc_item_stack_fields_get(o->owners.stack, &fields) != MC_ITEM_STACK_OK) {
        return false;
    }
    return printf("{\"id\":%d,\"count\":%d,\"damage\":%d,\"animation\":%d}", fields.id,
                  fields.count, fields.damage, fields.animation) >= 0;
}

static int fs_stat_compare(const void *left, const void *right) {
    const fs_stat *a = left, *b = right;
    return a->id < b->id ? -1 : a->id > b->id ? 1 : 0;
}

static bool fs_stat_map(const mc_client_stats_map *map) {
    const size_t count = mc_client_stats_map_size(map);
    fs_stat entries[FR_MAX_CELLS];
    if (count > COUNT(entries))
        return false;
    for (size_t i = 0; i < count; ++i) {
        const mc_statistic *statistic = NULL;
        if (mc_client_stats_map_entry(map, i, &statistic, &entries[i].value, NULL) !=
                MC_CLIENT_STATS_OK ||
            statistic == NULL)
            return false;
        entries[i].id = mc_statistic_fields_get(statistic)->e;
    }
    qsort(entries, count, sizeof entries[0], fs_stat_compare);
    putchar('[');
    for (size_t i = 0; i < count; ++i)
        printf("%s[%d,%d]", i == 0 ? "" : ",", entries[i].id, entries[i].value);
    return putchar(']') != EOF;
}

static bool fs_stats(const fs_observation *o) {
    const ro_observation *r = &o->owners;
    printf("{\"total\":");
    if (!fs_stat_map(r->writer->total))
        return false;
    printf(",\"unsent\":");
    if (!fs_stat_map(r->writer->unsent))
        return false;
    return printf(",\"dirty\":%s,\"writer_null\":%s}", fr_bool(r->writer->dirty),
                  fr_bool(r->application->stats_writer == NULL)) >= 0;
}

static bool fs_item_fields(const fs_observation *o) {
    const mc_item_fields *f = mc_item_fields_get(o->flint);
    if (f == NULL)
        return false;
    printf("{\"bf\":%d,\"bg\":%d,\"a\":%d,\"bh\":%d,\"bi\":%s,\"bj\":%s,\"bk\":", f->bf, f->bg,
           f->a, f->bh, fr_bool(f->bi), fr_bool(f->bj));
    if (f->bk == NULL)
        printf("null");
    else
        printf("%d", mc_item_fields_get(f->bk)->bf);
    printf(",\"bl\":");
    if (f->bl == NULL)
        printf("null");
    else {
        char text[64];
        if (!fr_name(f->bl, text))
            return false;
        fr_string(text);
    }
    return putchar('}') != EOF;
}

static bool fs_setup(fs_observation *o, const char *root) {
    bool ok = false;
    const fs_profile *p = o->profile;
    ro_observation *r = &o->owners;
    CHECK(fr_setup(&r->base, root) && ro_statistics(r, root));
    fixture *f = r->base.f;
    mc_world_runtime *world = f->worlds[0];
    r->application = calloc(1, sizeof(*r->application));
    CHECK(r->application != NULL);
    r->application->active_world = world;
    r->application->registry = f->registry[0];
    r->application->entities = f->entities[0];
    r->application->profile = f->profile;
    r->application->stats_writer = r->writer;
    CHECK(ro_player(r));
    if (strcmp(p->operation, "ob") == 0)
        CHECK(mc_client_singleplayer_controller_allocate(f->entities[0], f->profile, 0,
                                                         &r->controller,
                                                         NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
              mc_client_singleplayer_controller_construct(r->controller, r->application, NULL) ==
                  MC_SINGLEPLAYER_CONTROLLER_OK);
    for (size_t i = 0; i < p->stat_count; ++i) {
        const mc_statistic *statistic = NULL;
        CHECK(mc_registry_classes_find_statistic(f->registry[0], p->stats[i].id, &statistic) ==
                  MC_REGISTRY_OK &&
              statistic != NULL &&
              mc_client_stats_writer_increment(r->writer, statistic, p->stats[i].value, NULL) ==
                  MC_CLIENT_STATS_OK);
    }
    CHECK(r->writer->dirty == p->stats_dirty);
    if (!p->null_stack)
        CHECK(mc_item_stack_allocate(&r->stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(r->stack, p->stack_id, p->count, p->damage) ==
                  MC_ITEM_STACK_OK &&
              mc_item_stack_source_animation_set(r->stack, p->animation) == MC_ITEM_STACK_OK);
    const mc_registry_item_array *array = NULL;
    CHECK(mc_registry_item_c_get(f->registry[0], &array, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->registry[0], array, 259, &o->flint, NULL) == MC_REGISTRY_OK &&
          o->flint != NULL &&
          mc_registry_item_b_get(f->registry[0], &o->prepared_item_random, NULL) ==
              MC_REGISTRY_OK &&
          o->prepared_item_random != NULL && mc_registry_block_static_index("as", &o->fire_index));
    o->fire = f->registry[0]->blocks.named[o->fire_index];
    CHECK(o->fire != NULL);
    o->fire_id = o->fire->fields.bn;
    o->item_table = f->registry[0]->items.c;
    o->use_stats = f->registry[0]->statistics.E;
    o->break_stats = f->registry[0]->statistics.F;
    mc_java_random_reseed(world->fields.r, p->world_seed);
    mc_java_random_reseed(o->prepared_item_random, p->item_seed);
    mc_java_random seeded;
    mc_java_random_seed(&seeded, 0);
    atomic_store_explicit(&f->profile->math->state, seeded.state, memory_order_relaxed);
    f->profile->math->have_next_next_gaussian = false;
    f->profile->math->next_next_gaussian = 0.0;
    world->fields.B = p->remote;
    if (p->item_random_world)
        f->registry[0]->items.b = world->fields.r;
    if (p->null_item_random)
        f->registry[0]->items.b = NULL;
    if (p->null_fire)
        f->registry[0]->blocks.named[o->fire_index] = NULL;
    if (p->null_provider)
        world->fields.v = NULL;
    if (p->null_stats_writer)
        r->application->stats_writer = NULL;
    if (p->null_use_stats)
        f->registry[0]->statistics.E = NULL;
    if (p->null_break_stats)
        f->registry[0]->statistics.F = NULL;
    if (p->null_item_table)
        f->registry[0]->items.c = NULL;
    fs_shift(p, o->target);
    ok = true;
cleanup:
    return ok;
}

static bool fs_dispose(fs_observation *o) {
    ro_observation *r = &o->owners;
    if (r->base.f != NULL && r->base.f->registry[0] != NULL) {
        mc_registry_classes *registry = r->base.f->registry[0];
        if (o->prepared_item_random != NULL)
            registry->items.b = o->prepared_item_random;
        if (o->item_table != NULL)
            registry->items.c = o->item_table;
        if (o->use_stats != NULL)
            registry->statistics.E = o->use_stats;
        if (o->break_stats != NULL)
            registry->statistics.F = o->break_stats;
        if (o->fire != NULL) {
            o->fire->fields.bn = o->fire_id;
            registry->blocks.named[o->fire_index] = o->fire;
        }
    }
    free(o->action);
    return ro_dispose(r);
}

static bool fs_common(fs_observation *o) {
    fixture *f = o->owners.base.f;
    mc_world_runtime *world = f->worlds[0];
    printf("\"clicked\":");
    fs_cell(o, o->profile->xyz[0], o->profile->xyz[1], o->profile->xyz[2]);
    printf(",\"target_xyz\":[%d,%d,%d],\"target\":", o->target[0], o->target[1], o->target[2]);
    fs_cell(o, o->target[0], o->target[1], o->target[2]);
    printf(",\"world_o\":%s,\"world_random\":", fr_bool(world->fields.o));
    ro_random(world->fields.r);
    printf(",\"math_random\":");
    fr_math(f);
    printf(",\"item_random\":");
    ro_random(f->registry[0]->items.b);
    printf(",\"prepared_item_random\":");
    ro_random(o->prepared_item_random);
    printf(",\"item_random_alias_world\":%s,\"stack\":",
           fr_bool(f->registry[0]->items.b == world->fields.r));
    if (!fs_stack(o))
        return false;
    printf(",\"fire\":");
    ro_block_id(f->registry[0]->blocks.named[o->fire_index]);
    printf(",\"item_table_null\":%s,\"use_stats_null\":%s,\"break_stats_null\":%s,\"stats\":",
           fr_bool(f->registry[0]->items.c == NULL), fr_bool(f->registry[0]->statistics.E == NULL),
           fr_bool(f->registry[0]->statistics.F == NULL));
    return fs_stats(o);
}

static mc_world_storage_status fs_finish(fs_observation *o, int kind, mc_world_storage_error *error,
                                         mc_world_access_failure *failure) {
    puts("}");
    if (!o->valid || ferror(stdout))
        return MC_WORLD_STORAGE_IO_ERROR;
    const size_t ordinal = ++o->kinds[kind];
    if (kind == 2 && ordinal == 1) {
        fixture *f = o->owners.base.f;
        const char *action = o->profile->sound_action;
        if (strcmp(action, "fire-null") == 0)
            f->registry[0]->blocks.named[o->fire_index] = NULL;
        else if (strcmp(action, "fire-id-zero") == 0)
            o->fire->fields.bn = 0;
        else if (strcmp(action, "item-random-world") == 0)
            f->registry[0]->items.b = f->worlds[0]->fields.r;
        else if (strcmp(action, "stack-stick") == 0) {
            if (mc_item_stack_source_id_set(o->owners.stack, 280) != MC_ITEM_STACK_OK)
                return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        } else if (strcmp(action, "stats-writer-null") == 0)
            o->owners.application->stats_writer = NULL;
        else if (strcmp(action, "use-array-null") == 0)
            f->registry[0]->statistics.E = NULL;
        else if (strcmp(action, "break-array-null") == 0)
            f->registry[0]->statistics.F = NULL;
        else if (strcmp(action, "target-stone") == 0) {
            bool value = false;
            o->action = calloc(1, sizeof(*o->action));
            if (o->action == NULL)
                return MC_WORLD_STORAGE_ALLOCATION_FAILED;
            const mc_world_storage_status status = mc_world_runtime_block_set(
                f->worlds[0], o->target[0], o->target[1], o->target[2], 1, &value, o->action);
            if (status != MC_WORLD_STORAGE_OK || !value)
                return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
        }
    }
    if (kind >= 3 || o->profile->errors[kind] == 0 || ordinal != (size_t)o->profile->errors[kind])
        return MC_WORLD_STORAGE_OK;
    fr_observation *b = &o->owners.base;
    if (b->receipt != NULL)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    const mc_world_storage_status status =
        mc_world_access_receipt_create(b->listener, NULL, &b->receipt, failure, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    b->receipt->exception_class = "java/lang/IllegalStateException";
    *error =
        (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                 "Flint and Steel actual observer interruption", (uint32_t)ordinal};
    return error->status;
}

static bool fs_event(fs_observation *o, mc_world_runtime *world, const char *kind) {
    if (world != o->owners.base.f->worlds[0] || ++o->events > FR_MAX_EVENTS)
        return false;
    printf("FLINT_EVENT {\"kind\":\"%s\",", kind);
    return fs_common(o);
}

static mc_world_storage_status fs_cell_event(void *context, mc_world_runtime *world, int32_t x,
                                             int32_t y, int32_t z, mc_world_storage_error *error,
                                             mc_world_access_failure *failure) {
    fs_observation *o = context;
    o->valid &= fs_event(o, world, "cell");
    printf(",\"xyz\":[%d,%d,%d],\"cell\":", x, y, z);
    fs_cell(o, x, y, z);
    return fs_finish(o, 0, error, failure);
}

static mc_world_storage_status fs_range_event(void *context, mc_world_runtime *world, int32_t x0,
                                              int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                                              int32_t z1, mc_world_storage_error *error,
                                              mc_world_access_failure *failure) {
    fs_observation *o = context;
    o->valid &= fs_event(o, world, "range");
    printf(",\"xyz\":[%d,%d,%d,%d,%d,%d]", x0, y0, z0, x1, y1, z1);
    return fs_finish(o, 1, error, failure);
}

static mc_world_storage_status fs_sound_event(void *context, mc_world_runtime *world,
                                              const mc_registry_string *name, double x, double y,
                                              double z, float volume, float pitch,
                                              mc_world_storage_error *error,
                                              mc_world_access_failure *failure) {
    fs_observation *o = context;
    char text[64] = {0};
    o->valid &= fs_event(o, world, "sound") && fr_name(name, text);
    printf(",\"name\":\"%s\",\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\"],\"volume\":\"%08" PRIx32 "\",\"pitch\":\"%08" PRIx32 "\"",
           text, fr_double(x), fr_double(y), fr_double(z), fr_float(volume), fr_float(pitch));
    return fs_finish(o, 2, error, failure);
}

static mc_world_storage_status fs_sky_event(void *context, mc_world_runtime *world,
                                            mc_world_storage_error *error,
                                            mc_world_access_failure *failure) {
    fs_observation *o = context;
    o->valid &= fs_event(o, world, "sky");
    return fs_finish(o, 3, error, failure);
}

static bool fs_state(fs_observation *r, const char *phase) {
    fr_observation *o = &r->owners.base;
    mc_world_runtime *world = o->f->worlds[0];
    const mc_chunk_cache *cache = o->f->providers[0].cache;
    if (!mc_chunk_cache_validate(cache) || cache->count != o->chunk_count ||
        cache->insertion_count != o->chunk_count ||
        mc_world_store_chunk_count(o->f->stores[0]) != o->chunk_count)
        return false;
    for (size_t i = 0; i < o->chunk_count; ++i) {
        size_t j = 0;
        while (j < cache->insertion_count && cache->insertions[j] != o->chunks[i])
            ++j;
        if (j == cache->insertion_count)
            return false;
    }
    int32_t rain_time, thunder_time;
    int64_t time;
    bool raining, thundering;
    if (mc_world_info_time_get(world->fields.x, &time) != MC_WORLD_INFO_OK ||
        mc_world_info_source_rain_time_get(world->fields.x, &rain_time) != MC_WORLD_INFO_OK ||
        mc_world_info_source_thunder_time_get(world->fields.x, &thunder_time) != MC_WORLD_INFO_OK ||
        mc_world_info_source_raining_get(world->fields.x, &raining) != MC_WORLD_INFO_OK ||
        mc_world_info_source_thundering_get(world->fields.x, &thundering) != MC_WORLD_INFO_OK)
        return false;
    printf("FLINT_STATE {\"phase\":\"%s\",", phase);
    if (!fs_common(r))
        return false;
    printf(",\"time\":\"%016" PRIx64 "\",\"sky_seen\":%s,\"remote\":%s,\"provider_null\":%s,"
           "\"world_access_null\":%s,\"scheduled\":",
           (uint64_t)time, fr_bool(world->classes->chunk_a), fr_bool(world->fields.B),
           fr_bool(world->fields.v == NULL), fr_bool(world->fields.u == NULL));
    if (!fr_scheduled(world))
        return false;
    printf(",\"lighting\":");
    if (!fr_lighting(world))
        return false;
    printf(",\"controller\":");
    if (!ro_controller(&r->owners))
        return false;
    printf(",\"flint_fields\":");
    if (!fs_item_fields(r))
        return false;
    printf(",\"entities\":");
    if (!fr_entities(o))
        return false;
    printf(",\"entity_class\":{\"initialized\":%s,\"next_id\":%d},\"entity_count\":%zu,\"player_"
           "count\":%zu,\"players\":",
           fr_bool(o->f->entities[0]->entity_phase == MC_ENTITY_CLASS_READY),
           o->f->entities[0]->next_id, world->fields.b->count, world->fields.d->count);
    if (!fr_players(o))
        return false;
    printf(",\"random_work\":{\"g\":%" PRId32 ",\"cooldown\":%" PRId32 ",\"candidates\":[",
           world->fields.g, world->fields.Q);
    int32_t keys[FR_MAX_CHUNKS];
    size_t key_count = 0;
    if (world->fields.P == NULL ||
        (world->fields.P->order != NULL &&
         mc_java_int_set_copy_order(world->fields.P->order, keys, COUNT(keys), &key_count) !=
             MC_JAVA_INT_SET_OK))
        return false;
    for (size_t i = 0; i < key_count; ++i) {
        size_t j = 0;
        while (j < world->fields.P->count && world->fields.P->entries[j].hash != keys[i])
            ++j;
        if (j == world->fields.P->count)
            return false;
        printf("%s[%d,%d]", i == 0 ? "" : ",", world->fields.P->entries[j].x,
               world->fields.P->entries[j].z);
    }
    printf("]},\"weather\":{\"rain_previous\":\"%08" PRIx32 "\",\"rain_current\":\"%08" PRIx32
           "\",\"thunder_previous\":\"%08" PRIx32 "\",\"thunder_current\":\"%08" PRIx32
           "\",\"rain_time\":%d,\"thunder_time\":%d,\"raining\":%s,\"thundering\":%s},\"chunk_"
           "planes\":[",
           fr_float(world->fields.i), fr_float(world->fields.j), fr_float(world->fields.k),
           fr_float(world->fields.l), rain_time, thunder_time, fr_bool(raining),
           fr_bool(thundering));
    for (size_t i = 0; i < o->chunk_count; ++i) {
        printf("%s{\"xyz\":[%d,%d],\"planes\":", i == 0 ? "" : ",", o->chunks[i]->x,
               o->chunks[i]->z);
        if (!fr_planes(o->chunks[i]->data))
            return false;
        putchar('}');
    }
    return puts("]}") >= 0;
}

static void fs_input(const fs_profile *p) {
    const fr_profile *b = &p->owners.prepared;
    printf("{\"mode\":\"%s\",\"operation\":\"%s\",\"xyz\":[%d,%d,%d],\"face\":%d,"
           "\"stack_id\":%d,\"count\":%d,\"damage\":%d,\"animation\":%d,"
           "\"world_seed\":%" PRId64 ",\"item_seed\":%" PRId64,
           b->mode, p->operation, p->xyz[0], p->xyz[1], p->xyz[2], p->face, p->stack_id, p->count,
           p->damage, p->animation, p->world_seed, p->item_seed);
#define FS_FLAG(field) printf(",\"" #field "\":%s", fr_bool(p->field))
    FS_FLAG(remote);
    FS_FLAG(null_world);
    FS_FLAG(null_player);
    FS_FLAG(null_stack);
    FS_FLAG(null_item_random);
    FS_FLAG(null_fire);
    FS_FLAG(null_provider);
    FS_FLAG(null_access);
    FS_FLAG(null_stats_writer);
    FS_FLAG(null_use_stats);
    FS_FLAG(null_break_stats);
    FS_FLAG(null_item_table);
    FS_FLAG(item_random_world);
#undef FS_FLAG
    printf(",\"sound_action\":\"%s\",\"cell_error_at\":%d,\"range_error_at\":%d,\"sound_error_at\":"
           "%d,\"cells\":[",
           p->sound_action, p->errors[0], p->errors[1], p->errors[2]);
    for (size_t i = 0; i < b->cell_count; ++i) {
        const fr_cell *c = &b->cells[i];
        printf("%s[%d,%d,%d,%d,%d]", i == 0 ? "" : ",", c->x, c->y, c->z, c->id, c->metadata);
    }
    printf("],\"heights\":[");
    for (size_t i = 0; i < b->height_count; ++i)
        printf("%s[%d,%d,%d]", i == 0 ? "" : ",", b->heights[i][0], b->heights[i][1],
               b->heights[i][2]);
    printf("]}");
}

static void fs_prepared(const fs_profile *p) {
    const fr_profile *b = &p->owners.prepared;
    printf("{\"scheduled\":{\"initialized\":%s,\"sequence\":\"%016" PRIx64
           "\"},\"entity\":{\"initialized\":%s,\"next_id\":%d},\"stats\":{\"total\":[",
           fr_bool(b->scheduled_initialized), (uint64_t)b->scheduled_before,
           fr_bool(b->entity_initialized), b->entity_next);
    for (size_t i = 0; i < p->stat_count; ++i)
        printf("%s[%d,%d]", i == 0 ? "" : ",", p->stats[i].id, p->stats[i].value);
    printf("],\"unsent\":[");
    for (size_t i = 0; i < p->stat_count; ++i)
        printf("%s[%d,%d]", i == 0 ? "" : ",", p->stats[i].id, p->stats[i].value);
    printf("],\"dirty\":%s}}", fr_bool(p->stats_dirty));
}

static bool fs_run(const char *, const fs_profile *);

int main(int argc, char **argv) {
    fs_profile p = {0};
    fr_profile *b = &p.owners.prepared;
    char root[8192];
    if (argc != 40 || !decode_path(argv[2], root, sizeof root))
        return 2;
    b->mode = argv[1];
    p.operation = b->operation = argv[3];
    b->biome = 'i';
    b->g = 17;
    b->cooldown = 6001;
    b->prequeue_delay = -1;
    for (const char *c = b->mode; *c != '\0'; ++c)
        if ((*c < 'a' || *c > 'z') && (*c < '0' || *c > '9') && *c != '-')
            return 2;
    if (strcmp(p.operation, "direct") != 0 && strcmp(p.operation, "wrapper") != 0 &&
        strcmp(p.operation, "ob") != 0)
        return 2;
    int64_t values[31] = {0};
    const int arguments[] = {4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18,
                             19, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 34, 35, 36, 38};
    for (size_t i = 0; i < COUNT(arguments); ++i)
        if (!fr_integer(argv[arguments[i]], &values[i]) ||
            ((i != 8 && i != 9) && (values[i] < INT32_MIN || values[i] > INT32_MAX)))
            return 2;
    for (size_t i = 0; i < 3; ++i)
        p.xyz[i] = b->xyz[i] = (int32_t)values[i];
    p.face = (int32_t)values[3];
    p.stack_id = (int32_t)values[4];
    p.count = (int32_t)values[5];
    p.damage = (int32_t)values[6];
    p.animation = (int32_t)values[7];
    p.world_seed = b->seed = values[8];
    p.item_seed = values[9];
    for (size_t i = 10; i <= 22; ++i)
        if (values[i] != 0 && values[i] != 1)
            return 2;
    p.remote = values[10] != 0;
    p.null_world = values[11] != 0;
    p.null_player = values[12] != 0;
    p.null_stack = values[13] != 0;
    p.null_item_random = values[14] != 0;
    p.null_fire = values[15] != 0;
    p.null_provider = values[16] != 0;
    p.null_access = values[17] != 0;
    p.null_stats_writer = values[18] != 0;
    p.null_use_stats = values[19] != 0;
    p.null_break_stats = values[20] != 0;
    p.null_item_table = values[21] != 0;
    p.item_random_world = values[22] != 0;
    p.sound_action = argv[27];
    const char *actions[] = {
        "none",        "fire-null",         "fire-id-zero",   "item-random-world",
        "stack-stick", "stats-writer-null", "use-array-null", "break-array-null",
        "target-stone"};
    bool known = false;
    for (size_t i = 0; i < COUNT(actions); ++i)
        known |= strcmp(p.sound_action, actions[i]) == 0;
    if (!known)
        return 2;
    for (size_t i = 0; i < 3; ++i) {
        if (values[23 + i] < 0)
            return 2;
        p.errors[i] = (int32_t)values[23 + i];
    }
    int32_t records[FR_MAX_CELLS][5];
    if (!fr_records(argv[31], records, 5, &b->cell_count))
        return 2;
    for (size_t i = 0; i < b->cell_count; ++i)
        b->cells[i] =
            (fr_cell){records[i][0], records[i][1], records[i][2], records[i][3], records[i][4]};
    if (!fr_records(argv[32], records, 3, &b->height_count))
        return 2;
    for (size_t i = 0; i < b->height_count; ++i)
        for (size_t j = 0; j < 3; ++j)
            b->heights[i][j] = records[i][j];
    if (!ro_hex16(argv[33], &b->scheduled_before))
        return 2;
    for (size_t i = 26; i <= 27; ++i)
        if (values[i] != 0 && values[i] != 1)
            return 2;
    if (values[29] != 0 && values[29] != 1)
        return 2;
    b->scheduled_initialized = values[26] != 0;
    b->entity_initialized = values[27] != 0;
    b->entity_next = (int32_t)values[28];
    p.stats_dirty = values[29] != 0;
    if (!fr_records(argv[37], records, 2, &p.stat_count))
        return 2;
    for (size_t i = 0; i < p.stat_count; ++i) {
        p.stats[i] = (fs_stat){records[i][0], records[i][1]};
        if (i != 0 && p.stats[i - 1].id >= p.stats[i].id)
            return 2;
    }
    if (strcmp(argv[39], "none") != 0 || !fs_run(root, &p))
        return 1;
    return printf("PASS: %u Flint and Steel checks;mode=%s\n", checks, b->mode) < 0 ? 1 : 0;
}

static void fs_mutation_view(const mc_world_mutation_error *e) {
    if (e == NULL) {
        printf("null");
        return;
    }
    printf("{\"status\":%d,\"access_fault\":%d,\"access_pc\":%u}", (int)e->status,
           (int)e->world_access_source_fault, e->world_access_bytecode);
}

static void fs_progress(const mc_item_use_state *s, const mc_singleplayer_use_state *ob) {
    printf("{\"item\":");
    if (s == NULL)
        printf("null");
    else {
        const mc_item_use_error *e = s->error;
        printf("{\"pc\":%u,\"target_xyz\":[%d,%d,%d],\"item_id\":", s->source_pc, s->target_x,
               s->target_y, s->target_z);
        if (s->item == NULL)
            printf("null");
        else
            printf("%d", mc_item_fields_get(s->item)->bf);
        printf(",\"returned\":%s,\"body_result\":%s,\"sound_entered\":%s,\"sound_returned\":%s,"
               "\"setter_entered\":%s,\"setter_returned\":%s,\"setter_value\":%s,"
               "\"durability_entered\":%s,\"durability_returned\":%s,\"used_stat_returned\":%s,"
               "\"query_provider_null\":%s,\"sound\":",
               fr_bool(s->returned), fr_bool(s->result), fr_bool(e->sound != NULL),
               fr_bool(s->sound_returned), fr_bool(e->mutation != NULL),
               fr_bool(s->setter_returned), fr_bool(s->setter_value),
               fr_bool(e->durability != NULL),
               fr_bool(e->durability != NULL && e->durability->status == MC_ENTITY_OK),
               fr_bool(s->statistic_returned), fr_bool(e->query_provider_null));
        fs_mutation_view(e->sound);
        printf(",\"setter\":");
        fs_mutation_view(e->mutation);
        printf(",\"site\":");
        ro_site(e->site.class_name, e->site.method, e->site.pc);
        printf(",\"child\":");
        ro_site(e->child.class_name, e->child.method, e->child.pc);
        printf("}");
    }
    printf(",\"ob\":");
    if (ob == NULL)
        printf("null");
    else
        printf("{\"pc\":%u,\"activation_entered\":%s,\"activation_returned\":%s,\"activated\":%s,"
               "\"item_entered\":%s,\"item_returned\":%s,\"zero_slot_cleared\":%s,\"selected_slot_"
               "stored\":%s}",
               ob->source_pc, fr_bool(ob->activation_entered), fr_bool(ob->activation_returned),
               fr_bool(ob->activated), fr_bool(ob->item_entered), fr_bool(ob->item_returned),
               fr_bool(ob->zero_slot_cleared), fr_bool(ob->selected_slot_stored));
    printf("}");
}

static bool fs_run(const char *root, const fs_profile *profile) {
    bool ok = false, removed = false, returned = false, result = false, native_policy = false;
    bool source_exception = false, source_error = false, provider_null = false;
    fs_observation o = {.profile = profile,
                        .valid = true,
                        .owners = {.profile = &profile->owners,
                                   .base = {.profile = &profile->owners.prepared, .valid = true}}};
    mc_item_use_call *item_call = NULL;
    mc_item_use_error item_error = {0};
    mc_item_use_state item_state = {0};
    mc_singleplayer_use_call *ob_call = NULL;
    mc_singleplayer_use_error ob_error = {0};
    mc_singleplayer_use_state ob_state = {0};
    const mc_item_use_state *s = NULL;
    const mc_world_access_failure *failure = NULL;
    mc_world_access_receipt *producer = NULL;
    const char *exception = NULL, *site_class = NULL, *site_method = NULL, *child_class = NULL,
               *child_method = NULL;
    unsigned int site_pc = 0, child_pc = 0;
    int raw = 0;
    size_t before_holds = 0, after_busy_holds = 0;
    mc_world_storage_status busy = MC_WORLD_STORAGE_OK;
    bool same_identity = false, selected_source = false;
    char receipt_exception[96] = {0};
    CHECK(fs_setup(&o, root));
    fixture *f = o.owners.base.f;
    mc_world_runtime *world = f->worlds[0];
    const mc_world_access_source_callbacks callbacks = {.cell = fs_cell_event,
                                                        .range = fs_range_event,
                                                        .sound = fs_sound_event,
                                                        .sky_changed = fs_sky_event};
    CHECK(mc_world_access_source_create(&callbacks, &o, &o.owners.base.listener, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, o.owners.base.listener, NULL) ==
              MC_WORLD_STORAGE_OK);
    o.owners.world_accesses = world->fields.u;
    if (profile->null_access)
        world->fields.u = NULL;
    CHECK(fs_state(&o, "before"));
    mc_world_runtime *argument_world = profile->null_world ? NULL : world;
    mc_entity *argument_player = profile->null_player ? NULL : o.owners.base.player;
    const int32_t *xyz = profile->xyz;
    if (strcmp(profile->operation, "ob") == 0) {
        raw = (int)mc_client_singleplayer_use_on_block_begin(
            o.owners.controller, argument_player, argument_world, o.owners.stack, xyz[0], xyz[1],
            xyz[2], profile->face, &ob_call, &ob_error);
        CHECK(ob_call != NULL && mc_client_singleplayer_use_inspect(ob_call, &ob_state));
        s = ob_state.item;
        returned = ob_state.returned;
        result = ob_state.result;
        const mc_singleplayer_use_error *e = ob_state.error;
        exception = e->exception_class;
        native_policy = e->native_policy;
        source_exception = e->source_exception;
        source_error = e->source_error;
        provider_null = e->query_provider_null;
        site_class = e->site.class_name;
        site_method = e->site.method;
        site_pc = e->site.pc;
        child_class = e->child.class_name;
        child_method = e->child.method;
        child_pc = e->child.pc;
    } else {
        raw = (int)(strcmp(profile->operation, "direct") == 0
                        ? mc_item_use_flint_and_steel_begin(f->registry[0], o.flint, o.owners.stack,
                                                            argument_player, argument_world, xyz[0],
                                                            xyz[1], xyz[2], profile->face,
                                                            &item_call, &item_error)
                        : mc_item_use_on_block_begin(
                              f->registry[0], o.owners.stack, argument_player, argument_world,
                              xyz[0], xyz[1], xyz[2], profile->face, &item_call, &item_error));
        CHECK(item_call != NULL && mc_item_use_inspect(item_call, &item_state));
        s = &item_state;
        returned = s->returned;
        result = s->result;
        const mc_item_use_error *e = s->error;
        exception = e->exception_class;
        native_policy = e->native_policy;
        source_exception = e->source_exception;
        source_error = e->source_error;
        provider_null = e->query_provider_null;
        site_class = e->site.class_name;
        site_method = e->site.method;
        site_pc = e->site.pc;
        child_class = e->child.class_name;
        child_method = e->child.method;
        child_pc = e->child.pc;
    }
    CHECK(o.valid && !ferror(stdout));
    /* Select the actual failed producer only for its identity/lifetime proof.
     * The enclosing class/category above is never reconstructed from a child. */
    if (s != NULL) {
        if (s->error->sound != NULL && s->error->sound->status != MC_WORLD_STORAGE_OK)
            failure = &s->error->sound->world_access_failure;
        else if (s->error->mutation != NULL && s->error->mutation->status != MC_WORLD_STORAGE_OK)
            failure = &s->error->mutation->world_access_failure;
    }
    if (o.owners.base.receipt != NULL) {
        producer = o.owners.base.receipt;
        const char *selected_exception = NULL;
        selected_source = failure != NULL &&
                          mc_world_access_failure_source_exception(failure, &selected_exception);
        same_identity = failure != NULL && failure->receipt == producer;
        CHECK(!returned && selected_source && same_identity && exception != NULL &&
              selected_exception != NULL && strcmp(exception, selected_exception) == 0 &&
              strlen(selected_exception) < sizeof receipt_exception);
        memcpy(receipt_exception, selected_exception, strlen(selected_exception) + 1);
        before_holds = producer->holds;
        CHECK(before_holds == 2);
        busy = mc_world_access_receipt_retire(&o.owners.base.receipt, NULL);
        CHECK(busy == MC_WORLD_STORAGE_BUSY && o.owners.base.receipt == producer);
        after_busy_holds = producer->holds;
        CHECK(after_busy_holds == before_holds);
    }
    printf("FLINT_OUTCOME {\"mode\":\"%s\",\"input\":", profile->owners.prepared.mode);
    fs_input(profile);
    printf(",\"prepared_class\":");
    fs_prepared(profile);
    printf(",\"raw\":%d,\"returned\":%s,\"result\":", raw, fr_bool(returned));
    if (returned)
        printf("%s", fr_bool(result));
    else
        printf("null");
    printf(",\"exception\":");
    fr_string(exception);
    printf(",\"native_policy\":%s,\"source_exception\":%s,\"source_error\":%s,\"query_provider_"
           "null\":%s,\"site\":",
           fr_bool(native_policy), fr_bool(source_exception), fr_bool(source_error),
           fr_bool(provider_null));
    ro_site(site_class, site_method, site_pc);
    printf(",\"child\":");
    ro_site(child_class, child_method, child_pc);
    printf(",\"progress\":");
    fs_progress(s, ob_call == NULL ? NULL : &ob_state);
    puts("}");
    CHECK(fs_state(&o, "after"));
    CHECK(mc_client_singleplayer_use_retire(&ob_call, NULL) == MC_SINGLEPLAYER_USE_OK &&
          mc_item_use_retire(&item_call, NULL) == MC_ITEM_USE_OK && fs_state(&o, "retired"));
    if (producer != NULL) {
        CHECK(o.owners.base.receipt == producer && producer->holds == 1);
        const size_t after_call = producer->holds;
        const mc_world_storage_status retired =
            mc_world_access_receipt_retire(&o.owners.base.receipt, NULL);
        CHECK(retired == MC_WORLD_STORAGE_OK && o.owners.base.receipt == NULL);
        printf("FLINT_RECEIPT {\"busy_before\":%d,\"before_holds\":%zu,\"after_busy_holds\":%zu,"
               "\"after_call_holds\":%zu,\"retire\":%d,\"same_identity\":%s,\"selected_source\":%s,"
               "\"exception\":\"%s\",\"attempts\":2}\n",
               (int)busy, before_holds, after_busy_holds, after_call, (int)retired,
               fr_bool(same_identity), fr_bool(selected_source), receipt_exception);
    }
    CHECK(fs_state(&o, "cleanup"));
    world->fields.u = o.owners.world_accesses;
    CHECK(mc_world_runtime_remove_world_access(world, o.owners.base.listener, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(o.owners.base.listener, NULL) == MC_WORLD_STORAGE_OK);
    o.owners.base.listener = NULL;
    ok = true;
cleanup:
    if (mc_client_singleplayer_use_retire(&ob_call, NULL) != MC_SINGLEPLAYER_USE_OK ||
        mc_item_use_retire(&item_call, NULL) != MC_ITEM_USE_OK)
        return false;
    if (o.owners.base.receipt != NULL &&
        mc_world_access_receipt_retire(&o.owners.base.receipt, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (o.owners.base.listener != NULL) {
        if (o.owners.base.f != NULL && o.owners.base.f->worlds[0] != NULL) {
            mc_world_runtime *live = o.owners.base.f->worlds[0];
            if (o.owners.world_accesses != NULL)
                live->fields.u = o.owners.world_accesses;
            if (mc_world_runtime_remove_world_access(live, o.owners.base.listener, &removed,
                                                     NULL) != MC_WORLD_STORAGE_OK)
                return false;
        }
        if (mc_world_access_free(o.owners.base.listener, NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    return fs_dispose(&o) && ok;
}
