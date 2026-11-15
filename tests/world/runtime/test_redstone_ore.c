/* Reuse genuine Fire fixture owners and serialized prepared Chunks. Its
 * gameplay scenarios and entry point are never invoked by this transport. */
#define MC_FIRE_ENTRY mc_ore_unused_fire_fixture_main
#include "test_fire.c"
#undef MC_FIRE_ENTRY

#include "../../../src/client/application_state.h"
#include "../../../src/world/runtime/step_internal.h"
#include "mc/client/controller/mining.h"
#include "mc/client/controller/use.h"
#include "mc/client/statistics/writer.h"
#include "mc/item/use/use.h"
#include "mc/world/runtime/block_random_display.h"
#include "mc/world/runtime/placement.h"

typedef struct ro_profile {
    fr_profile prepared;
    bool player, null_random, null_tick_random, null_unlit, null_lit, null_world, null_access;
    int32_t particle_error;
    int32_t item_use_stat;
} ro_profile;

typedef struct ro_observation {
    fr_observation base;
    const ro_profile *profile;
    size_t static_indexes[2];
    mc_block *ore_statics[2];
    mc_java_random *world_random;
    mc_world_reference_list *world_accesses;
    client_application *application;
    mc_client_singleplayer_controller *controller;
    mc_item_stack *stack;
    mc_client_stats_writer *writer;
    mc_player_session_classes *stats_sessions;
    size_t particle_count;
} ro_observation;

static void ro_random(const mc_java_random *random) {
    if (random == NULL)
        printf("null");
    else
        fr_random(random);
}

static void ro_block_id(const mc_block *block) {
    if (block == NULL)
        printf("null");
    else
        printf("%d", block->fields.bn);
}

static bool ro_controller(const ro_observation *o) {
    if (o->controller == NULL)
        return printf("null") >= 0;
    mc_singleplayer_controller_snapshot state;
    if (!mc_client_singleplayer_controller_read(o->controller, &state))
        return false;
    return printf("{\"c\":%d,\"d\":%d,\"e\":%d,\"f\":\"%08" PRIx32 "\",\"g\":\"%08" PRIx32
                  "\",\"h\":\"%08" PRIx32 "\",\"i\":%d}",
                  state.c, state.d, state.e, fr_float(state.f), fr_float(state.g),
                  fr_float(state.h), state.i) >= 0;
}

static bool ro_stack(const ro_observation *o) {
    if (o->stack == NULL)
        return printf("null") >= 0;
    mc_item_stack_fields fields;
    return mc_item_stack_fields_get(o->stack, &fields) == MC_ITEM_STACK_OK &&
           printf("{\"id\":%d,\"count\":%d,\"damage\":%d}", fields.id, fields.count,
                  fields.damage) >= 0;
}

static bool ro_event_common(ro_observation *r, mc_world_runtime *world, const char *kind) {
    fr_observation *o = &r->base;
    int32_t target[2];
    if (o->events >= FR_MAX_EVENTS || world != o->f->worlds[0] ||
        !fr_cell_read(o, o->profile->xyz[0], o->profile->xyz[1], o->profile->xyz[2], target))
        return false;
    ++o->events;
    printf("ORE_EVENT {\"kind\":\"%s\",\"target\":[%d,%d],\"world_o\":%s,\"world_random\":", kind,
           target[0], target[1], fr_bool(world->fields.o));
    ro_random(world->fields.r);
    printf(",\"math_random\":");
    fr_math(o->f);
    printf(",\"entities\":");
    return fr_entities(o);
}

static mc_world_storage_status ro_event_finish(ro_observation *r, int kind,
                                               mc_world_storage_error *error,
                                               mc_world_access_failure *failure) {
    fr_observation *o = &r->base;
    puts("}");
    if (!o->valid || ferror(stdout))
        return MC_WORLD_STORAGE_IO_ERROR;
    if (kind < 0)
        return MC_WORLD_STORAGE_OK;
    const size_t ordinal = kind == 4 ? ++r->particle_count : ++o->kinds[kind];
    const int32_t wanted = kind == 4 ? r->profile->particle_error : o->profile->errors[kind];
    if (wanted == 0 || ordinal != (size_t)wanted)
        return MC_WORLD_STORAGE_OK;
    if (o->receipt != NULL)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    const mc_world_storage_status status =
        mc_world_access_receipt_create(o->listener, NULL, &o->receipt, failure, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    o->receipt->exception_class = "java/lang/IllegalStateException";
    *error = (mc_world_storage_error){
        MC_WORLD_STORAGE_IO_ERROR, "Redstone Ore actual observer interruption", (uint32_t)ordinal};
    return error->status;
}

static mc_world_storage_status ro_cell_event(void *context, mc_world_runtime *world, int32_t x,
                                             int32_t y, int32_t z, mc_world_storage_error *error,
                                             mc_world_access_failure *failure) {
    ro_observation *o = context;
    int32_t current[2] = {-1, -1};
    o->base.valid &= ro_event_common(o, world, "cell") && fr_cell_read(&o->base, x, y, z, current);
    printf(",\"xyz\":[%d,%d,%d],\"cell\":[%d,%d]", x, y, z, current[0], current[1]);
    return ro_event_finish(o, 0, error, failure);
}

static mc_world_storage_status ro_range_event(void *context, mc_world_runtime *world, int32_t x0,
                                              int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                                              int32_t z1, mc_world_storage_error *error,
                                              mc_world_access_failure *failure) {
    ro_observation *o = context;
    o->base.valid &= ro_event_common(o, world, "range");
    printf(",\"xyz\":[%d,%d,%d,%d,%d,%d]", x0, y0, z0, x1, y1, z1);
    return ro_event_finish(o, 1, error, failure);
}

static mc_world_storage_status ro_particle_event(void *context, mc_world_runtime *world,
                                                 const mc_registry_string *name, double x, double y,
                                                 double z, double vx, double vy, double vz,
                                                 mc_world_storage_error *error,
                                                 mc_world_access_failure *failure) {
    ro_observation *o = context;
    char text[64] = {0};
    o->base.valid &= ro_event_common(o, world, "particle") && fr_name(name, text);
    printf(",\"name\":\"%s\",\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\"],\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]",
           text, fr_double(x), fr_double(y), fr_double(z), fr_double(vx), fr_double(vy),
           fr_double(vz));
    return ro_event_finish(o, 4, error, failure);
}

static mc_world_storage_status ro_sound_event(void *context, mc_world_runtime *world,
                                              const mc_registry_string *name, double x, double y,
                                              double z, float volume, float pitch,
                                              mc_world_storage_error *error,
                                              mc_world_access_failure *failure) {
    ro_observation *o = context;
    char text[64] = {0};
    o->base.valid &= ro_event_common(o, world, "sound") && fr_name(name, text);
    printf(",\"name\":\"%s\",\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\"],\"volume\":\"%08" PRIx32 "\",\"pitch\":\"%08" PRIx32 "\"",
           text, fr_double(x), fr_double(y), fr_double(z), fr_float(volume), fr_float(pitch));
    return ro_event_finish(o, 3, error, failure);
}

static mc_world_storage_status ro_sky_event(void *context, mc_world_runtime *world,
                                            mc_world_storage_error *error,
                                            mc_world_access_failure *failure) {
    ro_observation *o = context;
    o->base.valid &= ro_event_common(o, world, "sky");
    return ro_event_finish(o, -1, error, failure);
}

static bool ro_state(ro_observation *r, const char *phase) {
    fr_observation *o = &r->base;
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
    int32_t target[2], rain_time, thunder_time;
    int64_t time;
    bool raining, thundering;
    if (!fr_cell_read(o, o->profile->xyz[0], o->profile->xyz[1], o->profile->xyz[2], target) ||
        mc_world_info_time_get(world->fields.x, &time) != MC_WORLD_INFO_OK ||
        mc_world_info_source_rain_time_get(world->fields.x, &rain_time) != MC_WORLD_INFO_OK ||
        mc_world_info_source_thunder_time_get(world->fields.x, &thunder_time) != MC_WORLD_INFO_OK ||
        mc_world_info_source_raining_get(world->fields.x, &raining) != MC_WORLD_INFO_OK ||
        mc_world_info_source_thundering_get(world->fields.x, &thundering) != MC_WORLD_INFO_OK)
        return false;
    printf("ORE_STATE {\"phase\":\"%s\",\"target\":[%d,%d],\"world_random\":", phase, target[0],
           target[1]);
    ro_random(world->fields.r);
    printf(",\"math_random\":");
    fr_math(o->f);
    printf(",\"time\":\"%016" PRIx64 "\",\"world_o\":%s,\"sky_seen\":%s,\"scheduled\":",
           (uint64_t)time, fr_bool(world->fields.o), fr_bool(world->classes->chunk_a));
    if (!fr_scheduled(world))
        return false;
    printf(",\"lighting\":");
    if (!fr_lighting(world))
        return false;
    printf(",\"provider_null\":%s,\"world_access_null\":%s,\"ore_statics\":{\"unlit\":",
           fr_bool(world->fields.v == NULL), fr_bool(world->fields.u == NULL));
    ro_block_id(o->f->registry[0]->blocks.named[r->static_indexes[0]]);
    printf(",\"lit\":");
    ro_block_id(o->f->registry[0]->blocks.named[r->static_indexes[1]]);
    printf("},\"controller\":");
    if (!ro_controller(r))
        return false;
    printf(",\"held_stack\":");
    if (!ro_stack(r))
        return false;
    int32_t item_use_stat;
    if (mc_client_stats_writer_value(r->writer, o->f->registry[0]->statistics.E[4], &item_use_stat,
                                     NULL) != MC_CLIENT_STATS_OK)
        return false;
    printf(",\"item_use_stat\":%d", item_use_stat);
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

static bool ro_player(ro_observation *o) {
    bool ok = false, inserted = false;
    fr_observation *b = &o->base;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL, *token = NULL;
    const uint16_t user[] = {'O', 'r', 'e', 'C', 'a', 'l', 'l'}, sid[] = {'-'};
    CHECK(mc_player_session_classes_create(b->f->entities[0], b->f->registry[0], &b->sessions,
                                           NULL) == MC_PLAYER_OK &&
          mc_player_string_create(user, COUNT(user), &username) == MC_PLAYER_OK &&
          mc_player_string_create(sid, COUNT(sid), &token) == MC_PLAYER_OK &&
          mc_player_session_create(b->sessions, username, token, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(b->f->entities[0], &b->player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(b->player, b->f->worlds[0], o->application, session, 0, NULL,
                                    NULL) == MC_PLAYER_OK &&
          mc_entity_set_position(b->player, 8.5, 64.0, 8.5) == MC_ENTITY_OK &&
          mc_world_runtime_insert_entity(b->f->worlds[0], b->player, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    o->application->h = b->player;
    ok = true;
cleanup:
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    mc_player_string_release(token);
    return ok;
}

static mc_client_stats_status ro_quiet_stats(void *context,
                                             const mc_client_stats_diagnostic *event) {
    (void)context;
    (void)event;
    return MC_CLIENT_STATS_OK;
}

static bool ro_statistics(ro_observation *o, const char *root) {
    bool ok = false;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL, *token = NULL;
    const uint16_t user[] = {'O', 'r', 'e', 'S', 't', 'a', 't'}, sid[] = {'-'};
    fixture *f = o->base.f;
    mc_client_stats_options options = mc_client_stats_default_options();
    options.charset = MC_JAVA_READER_UTF8;
    options.diagnostics.emit = ro_quiet_stats;
    CHECK(mc_registry_classes_initialize_statistics(f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_player_session_classes_create(f->entities[0], f->registry[0], &o->stats_sessions,
                                           NULL) == MC_PLAYER_OK &&
          mc_player_string_create(user, COUNT(user), &username) == MC_PLAYER_OK &&
          mc_player_string_create(sid, COUNT(sid), &token) == MC_PLAYER_OK &&
          mc_player_session_create(o->stats_sessions, username, token, &session, NULL) ==
              MC_PLAYER_OK &&
          mc_client_stats_writer_allocate(f->registry[0], &options, &o->writer, NULL) ==
              MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_construct(o->writer, session, root, NULL, NULL) ==
              MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_join(o->writer, NULL) == MC_CLIENT_STATS_OK);
    if (o->profile->item_use_stat != 0)
        CHECK(mc_client_stats_writer_increment(o->writer, f->registry[0]->statistics.E[4],
                                               o->profile->item_use_stat,
                                               NULL) == MC_CLIENT_STATS_OK);
    ok = true;
cleanup:
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    mc_player_string_release(token);
    return ok;
}

static bool ro_setup(ro_observation *o, const char *root) {
    bool ok = false;
    const ro_profile *p = o->profile;
    CHECK(fr_setup(&o->base, root));
    fixture *f = o->base.f;
    mc_world_runtime *world = f->worlds[0];
    CHECK(ro_statistics(o, root));
    if (p->player) {
        /* Declared source App references for the actual OB/OS methods. The
         * App constructor, display and audio are outside this fixture's claim. */
        o->application = calloc(1, sizeof(*o->application));
        CHECK(o->application != NULL);
        o->application->active_world = world;
        o->application->registry = f->registry[0];
        o->application->entities = f->entities[0];
        o->application->profile = f->profile;
        o->application->stats_writer = o->writer;
        CHECK(ro_player(o) &&
              mc_client_singleplayer_controller_allocate(f->entities[0], f->profile, 0,
                                                         &o->controller,
                                                         NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
              mc_client_singleplayer_controller_construct(o->controller, o->application, NULL) ==
                  MC_SINGLEPLAYER_CONTROLLER_OK);
        mc_java_random_reseed(world->fields.r, p->prepared.seed);
        mc_java_random seeded;
        mc_java_random_seed(&seeded, 0);
        atomic_store_explicit(&f->profile->math->state, seeded.state, memory_order_relaxed);
        f->profile->math->have_next_next_gaussian = false;
        f->profile->math->next_next_gaussian = 0.0;
    }
    if (strcmp(p->prepared.operation, "right-use") == 0)
        CHECK(mc_item_stack_allocate(&o->stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(o->stack, 4, 1, 0) == MC_ITEM_STACK_OK);
    CHECK(mc_registry_block_static_index("aO", &o->static_indexes[0]) &&
          mc_registry_block_static_index("aP", &o->static_indexes[1]));
    for (size_t i = 0; i < 2; ++i) {
        o->ore_statics[i] = f->registry[0]->blocks.named[o->static_indexes[i]];
        CHECK(o->ore_statics[i] != NULL);
    }
    if (p->null_unlit)
        f->registry[0]->blocks.named[o->static_indexes[0]] = NULL;
    if (p->null_lit)
        f->registry[0]->blocks.named[o->static_indexes[1]] = NULL;
    o->world_random = world->fields.r;
    if (p->null_random)
        world->fields.r = NULL;
    ok = true;
cleanup:
    return ok;
}

static bool ro_dispose(ro_observation *o) {
    if (o->base.f != NULL) {
        mc_world_runtime *world = o->base.f->worlds[0];
        if (world != NULL && o->world_random != NULL)
            world->fields.r = o->world_random;
        if (o->base.f->registry[0] != NULL)
            for (size_t i = 0; i < 2; ++i)
                if (o->ore_statics[i] != NULL)
                    o->base.f->registry[0]->blocks.named[o->static_indexes[i]] = o->ore_statics[i];
    }
    if (mc_client_singleplayer_controller_free(&o->controller, NULL) !=
        MC_SINGLEPLAYER_CONTROLLER_OK)
        return false;
    mc_item_stack_release(o->stack);
    o->stack = NULL;
    if (mc_client_stats_writer_retire(&o->writer, NULL) != MC_CLIENT_STATS_OK ||
        mc_player_session_classes_free(&o->stats_sessions, NULL) != MC_PLAYER_OK)
        return false;
    /* Player's actual App borrow outlives its final native disposal. */
    const bool result = fr_dispose(&o->base);
    free(o->application);
    o->application = NULL;
    return result;
}

static void ro_input(const ro_profile *p) {
    const fr_profile *b = &p->prepared;
    printf("{\"mode\":\"%s\",\"operation\":\"%s\",\"xyz\":[%d,%d,%d],\"entry_id\":%d,"
           "\"random_seed\":%" PRId64 ",\"cell_error_at\":%d,\"range_error_at\":%d,"
           "\"particle_error_at\":%d,\"sound_error_at\":%d,\"prequeue_delay\":%d,"
           "\"random_player\":%s,\"player\":%s,\"g\":%d,\"cooldown\":%d,\"null_random\":%s,"
           "\"null_tick_random\":%s,\"null_provider\":%s,\"null_unlit\":%s,\"null_lit\":%s,"
           "\"null_world\":%s,\"null_access\":%s,\"cells\":[",
           b->mode, b->operation, b->xyz[0], b->xyz[1], b->xyz[2], b->entry_id, b->seed,
           b->errors[0], b->errors[1], p->particle_error, b->errors[3], b->prequeue_delay,
           fr_bool(b->random_player), fr_bool(p->player), b->g, b->cooldown,
           fr_bool(p->null_random), fr_bool(p->null_tick_random), fr_bool(b->null_provider),
           fr_bool(p->null_unlit), fr_bool(p->null_lit), fr_bool(p->null_world),
           fr_bool(p->null_access));
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

static bool ro_hex16(const char *text, int64_t *out) {
    if (strlen(text) != 16)
        return false;
    uint64_t value = 0;
    for (size_t i = 0; i < 16; ++i) {
        const unsigned char c = (unsigned char)text[i];
        const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                   : c >= 'a' && c <= 'f' ? c - 'a' + 10u
                                                          : 16u;
        if (digit > 15)
            return false;
        value = value * 16u + digit;
    }
    memcpy(out, &value, sizeof value);
    return true;
}

static const char *ro_mutation_exception(const mc_world_mutation_error *e) {
    const char *exception = NULL;
    if (mc_world_access_failure_source_exception(&e->world_access_failure, &exception))
        return exception;
    if (e->redstone_ore_source_null || e->query.source_null ||
        e->world_access_source_fault != MC_WORLD_ACCESS_SOURCE_FAULT_NONE)
        return "java/lang/NullPointerException";
    return NULL;
}

static const char *ro_step_exception(const mc_world_step_error *e) {
    switch (e->source.kind) {
        case MC_WORLD_STEP_SOURCE_NULL_WORLD:
        case MC_WORLD_STEP_SOURCE_NULL_RANDOM:
        case MC_WORLD_STEP_SOURCE_NULL_STATIC:
            return "java/lang/NullPointerException";
        case MC_WORLD_STEP_SOURCE_QUERY:
            return e->block_read.nearby.query.source_null ? "java/lang/NullPointerException" : NULL;
        case MC_WORLD_STEP_SOURCE_PRESENTATION:
        case MC_WORLD_STEP_SOURCE_MUTATION:
            return ro_mutation_exception(&e->mutation);
        default:
            return NULL;
    }
}

static void ro_site(const char *class_name, const char *method, unsigned int pc) {
    printf("{\"class\":");
    fr_string(class_name);
    printf(",\"method\":");
    fr_string(method);
    printf(",\"pc\":%u}", pc);
}

static void ro_source_sites(const mc_world_step_error *step,
                            const mc_world_mutation_error *mutation) {
    printf("{\"step\":");
    if (step == NULL)
        printf("null");
    else
        printf("{\"source\":%d,\"registry_status\":%d,\"caller_pc\":%u,\"activation_pc\":%u,"
               "\"particle_pc\":%u,\"opaque_pc\":%u,\"face\":%d,\"query_null\":%s,"
               "\"query_pc\":%u,\"provider_pc\":%u}",
               (int)step->source.kind, (int)step->source.registry_status, step->bytecode,
               step->activation_bytecode, step->particle_bytecode, step->opaque_bytecode,
               step->particle_face, fr_bool(step->block_read.nearby.query.source_null),
               step->block_read.nearby.query.bytecode,
               step->block_read.nearby.query.provider_bytecode);
    printf(",\"mutation\":");
    if (mutation == NULL)
        printf("null");
    else
        printf("{\"ore_tick_pc\":%u,\"source_null\":%s,\"query_null\":%s,\"query_pc\":%u,"
               "\"provider_pc\":%u,\"world_access_pc\":%u,\"access_fault\":%d}",
               mutation->redstone_ore_tick_bytecode, fr_bool(mutation->redstone_ore_source_null),
               fr_bool(mutation->query.source_null), mutation->query.bytecode,
               mutation->query.provider_bytecode, mutation->world_access_bytecode,
               (int)mutation->world_access_source_fault);
    printf("}");
}

static bool ro_run(const char *root, const ro_profile *profile) {
    bool ok = false, removed = false, returned = false, result = false, has_result = false;
    ro_observation o = {.profile = profile, .base = {.profile = &profile->prepared, .valid = true}};
    mc_world_placement_error placement = {0};
    mc_world_step_error walk;
    mc_world_step_error_init(&walk);
    mc_world_mutation_error tick;
    mc_world_mutation_error_init(&tick);
    mc_world_random_display_block_error *display = calloc(1, sizeof(*display));
    mc_world_tick_call *world_call = NULL;
    mc_world_tick_error world_admission = {0};
    mc_world_tick_state world_state = {0};
    mc_singleplayer_mining_call *mining_call = NULL;
    mc_singleplayer_mining_error mining_error = {0};
    mc_singleplayer_mining_state mining_state = {0};
    mc_singleplayer_use_call *use_call = NULL;
    mc_singleplayer_use_error use_error = {0};
    mc_singleplayer_use_state use_state = {0};
    const mc_world_placement_error *selected_placement = NULL;
    const mc_world_step_error *selected_step = NULL;
    const mc_world_mutation_error *selected_mutation = NULL;
    const mc_world_access_failure *selected_receipt = NULL;
    mc_world_access_receipt *producer = NULL;
    mc_world_storage_status receipt_busy = MC_WORLD_STORAGE_OK;
    size_t receipt_before = 0, receipt_after_busy = 0;
    const char *exception = NULL, *site_class = NULL, *site_method = NULL;
    unsigned int site_pc = 0;
    int raw = 0;
    bool native_policy = false, retained_failure = false, same_receipt = false,
         selected_source = false;
    char receipt_exception[96] = {0};
    CHECK(display != NULL && ro_setup(&o, root));
    mc_world_runtime *world = o.base.f->worlds[0];
    mc_world_runtime *argument_world = profile->null_world ? NULL : world;
    const mc_world_access_source_callbacks callbacks = {.cell = ro_cell_event,
                                                        .range = ro_range_event,
                                                        .sound = ro_sound_event,
                                                        .particle = ro_particle_event,
                                                        .sky_changed = ro_sky_event};
    CHECK(mc_world_access_source_create(&callbacks, &o, &o.base.listener, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, o.base.listener, NULL) == MC_WORLD_STORAGE_OK);
    o.world_accesses = world->fields.u;
    if (profile->null_access)
        world->fields.u = NULL;
    CHECK(ro_state(&o, "before"));
    const fr_profile *p = &profile->prepared;
    const int32_t x = p->xyz[0], y = p->xyz[1], z = p->xyz[2];
    mc_registry_block_m_view view;
    const mc_block *block = NULL;
    CHECK(mc_registry_block_m_get(o.base.f->registry[0], &view, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_m_at(&view, p->entry_id, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL);
    if (strcmp(p->operation, "left") == 0 || strcmp(p->operation, "right") == 0) {
        if (strcmp(p->operation, "left") == 0)
            raw = (int)mc_world_block_clicked(o.base.f->registry[0], block, argument_world, x, y, z,
                                              NULL, &placement);
        else {
            has_result = true;
            raw = (int)mc_world_block_activate(o.base.f->registry[0], block, argument_world, x, y,
                                               z, NULL, &result, &placement);
        }
        selected_placement = &placement;
        returned = placement.returned;
    } else if (strcmp(p->operation, "walk") == 0) {
        raw = (int)mc_world_runtime_entity_step(argument_world, block, x, y, z, NULL, NULL, &walk);
        returned = raw == (int)MC_WORLD_STORAGE_OK;
        selected_step = &walk;
        exception = returned ? NULL : ro_step_exception(&walk);
        native_policy = !returned && exception == NULL;
        site_class = walk.block_class;
        site_method = "b";
        site_pc = walk.bytecode;
    } else if (strcmp(p->operation, "display") == 0) {
        raw = (int)mc_world_runtime_block_random_display(
            argument_world, block, x, y, z, profile->null_tick_random ? NULL : world->fields.r,
            display);
        returned = raw == (int)MC_WORLD_STORAGE_OK;
        exception = display->exception_class;
        native_policy = display->native_policy;
        site_class = display->site.class_name;
        site_method = display->site.method;
        site_pc = display->site.pc;
        if (display->child == MC_WORLD_RANDOM_DISPLAY_BLOCK_STEP)
            selected_step = &display->detail.step;
    } else if (strcmp(p->operation, "tick") == 0) {
        returned =
            mc_world_mutation_block_tick(argument_world, block, x, y, z,
                                         profile->null_tick_random ? NULL : world->fields.r, &tick);
        raw = (int)(returned ? MC_WORLD_STORAGE_OK : tick.status);
        selected_mutation = &tick;
        exception = returned ? NULL : ro_mutation_exception(&tick);
        native_policy = !returned && exception == NULL;
        site_class = tick.hook_class;
        site_method = tick.hook_method;
        site_pc = tick.hook_bytecode;
    } else if (strcmp(p->operation, "world-tick") == 0) {
        raw = (int)mc_world_runtime_tick_begin(world, o.base.math, &world_call, &world_admission);
        CHECK(world_call != NULL && mc_world_runtime_tick_inspect(world_call, &world_state));
        returned = world_state.returned;
        exception = world_state.error->exception_class;
        native_policy = world_state.error->native_policy;
        site_class = world_state.error->source_class;
        site_method = world_state.error->source_method;
        site_pc = world_state.error->bytecode;
        selected_receipt = &world_state.error->world_access;
        if (world_state.error->scheduled != NULL)
            selected_mutation = &world_state.error->scheduled->call.tick;
        else if (world_state.error->random != NULL)
            selected_mutation = world_state.error->random->mutation;
    } else if (strcmp(p->operation, "mining") == 0) {
        raw = (int)mc_client_singleplayer_mining_begin(o.controller, MC_SINGLEPLAYER_MINING_START,
                                                       x, y, z, 1, &mining_call, &mining_error);
        CHECK(mining_call != NULL &&
              mc_client_singleplayer_mining_inspect(mining_call, &mining_state));
        returned = mining_state.returned;
        selected_placement = mining_state.error->activation;
        native_policy = mining_state.error->native_policy;
        site_class = mining_state.error->site.class_name;
        site_method = mining_state.error->site.method;
        site_pc = mining_state.error->site.pc;
    } else if (strcmp(p->operation, "right-use") == 0) {
        const mc_singleplayer_use_context sources = {.math = o.base.math};
        raw = (int)mc_client_singleplayer_use_on_block_with_sources_begin(
            o.controller, o.base.player, world, o.stack, x, y, z, 1, &sources, &use_call,
            &use_error);
        CHECK(use_call != NULL && mc_client_singleplayer_use_inspect(use_call, &use_state));
        returned = use_state.returned;
        result = use_state.result;
        has_result = true;
        exception = use_state.error->exception_class;
        native_policy = use_state.error->native_policy;
        site_class = use_state.error->site.class_name;
        site_method = use_state.error->site.method;
        site_pc = use_state.error->site.pc;
        if (use_state.error->activation != NULL &&
            use_state.error->activation->status != MC_WORLD_STORAGE_OK)
            selected_placement = use_state.error->activation;
        if (use_state.error->item != NULL) {
            const mc_item_use_error *item = use_state.error->item;
            selected_mutation = item->sound != NULL ? item->sound : item->mutation;
        }
    } else
        CHECK(false);
    if (selected_placement != NULL) {
        if (!returned && selected_placement->status != MC_WORLD_STORAGE_OK) {
            /* OS exposes this actual selected child instead of a duplicate
             * class slot. Keep its enclosing native-policy category intact;
             * OB already supplies both its own class and category. */
            if (mining_call != NULL)
                exception = selected_placement->exception_class;
            else if (use_call == NULL) {
                exception = selected_placement->exception_class;
                native_policy = selected_placement->native_policy;
            }
        }
        if (strcmp(p->operation, "mining") != 0 && strcmp(p->operation, "right-use") != 0) {
            site_class = selected_placement->site.class_name;
            site_method = selected_placement->site.method;
            site_pc = selected_placement->site.pc;
        }
        selected_step = selected_placement->step;
        if (selected_placement->mutation != NULL)
            selected_mutation = selected_placement->mutation;
    }
    if (selected_step != NULL && selected_step->status != MC_WORLD_STORAGE_OK &&
        (selected_step->source.kind == MC_WORLD_STEP_SOURCE_PRESENTATION ||
         selected_step->source.kind == MC_WORLD_STEP_SOURCE_MUTATION))
        selected_mutation = &selected_step->mutation;
    if (selected_receipt == NULL && selected_mutation != NULL)
        selected_receipt = &selected_mutation->world_access_failure;
    CHECK(o.base.valid && !ferror(stdout));
    retained_failure =
        o.base.receipt != NULL && (world_call != NULL || selected_placement != NULL ||
                                   mining_call != NULL || use_call != NULL);
    if (retained_failure) {
        producer = o.base.receipt;
        const char *selected_exception = NULL;
        selected_source = selected_receipt != NULL && mc_world_access_failure_source_exception(
                                                          selected_receipt, &selected_exception);
        same_receipt = selected_receipt != NULL && selected_receipt->receipt == producer;
        CHECK(!returned && selected_source && same_receipt && exception != NULL &&
              selected_exception != NULL && strcmp(exception, selected_exception) == 0 &&
              strlen(selected_exception) < sizeof receipt_exception);
        memcpy(receipt_exception, selected_exception, strlen(selected_exception) + 1);
        receipt_before = producer->holds;
        CHECK(receipt_before == 2);
        receipt_busy = mc_world_access_receipt_retire(&o.base.receipt, NULL);
        CHECK(receipt_busy == MC_WORLD_STORAGE_BUSY && o.base.receipt == producer);
        receipt_after_busy = producer->holds;
        CHECK(receipt_after_busy == receipt_before);
    }
    printf("ORE_OUTCOME {\"mode\":\"%s\",\"input\":", p->mode);
    ro_input(profile);
    printf(",\"prepared_class\":{\"scheduled\":{\"initialized\":%s,\"sequence\":\"%016" PRIx64
           "\"},\"entity\":{\"initialized\":%s,\"next_id\":%d},\"item_use_stat\":%d},\"raw\":%d,"
           "\"returned\":%s,\"result\":",
           fr_bool(p->scheduled_initialized), (uint64_t)p->scheduled_before,
           fr_bool(p->entity_initialized), p->entity_next, profile->item_use_stat, raw,
           fr_bool(returned));
    if (has_result && returned)
        printf("%s", fr_bool(result));
    else
        printf("null");
    printf(",\"exception\":");
    fr_string(exception);
    printf(",\"native_policy\":%s,\"site\":", fr_bool(native_policy));
    ro_site(site_class, site_method, site_pc);
    printf(",\"source_sites\":");
    ro_source_sites(selected_step, selected_mutation);
    printf(",\"continuation\":");
    if (mining_call != NULL)
        printf("{\"kind\":\"mining\",\"pc\":%u,\"block_id\":%d,\"metadata\":%d,"
               "\"initial_hook_returned\":%s,\"strength\":\"%08" PRIx32 "\","
               "\"progress_stored\":%s,\"removal_returned\":%s}",
               mining_state.source_pc, mining_state.block_id, mining_state.metadata,
               fr_bool(mining_state.initial_hook_returned), fr_float(mining_state.strength),
               fr_bool(mining_state.progress_stored), fr_bool(mining_state.removal_returned));
    else if (use_call != NULL)
        printf(
            "{\"kind\":\"right-use\",\"pc\":%u,\"clicked_id\":%d,\"activation_entered\":%s,"
            "\"activation_returned\":%s,\"activated\":%s,\"item_entered\":%s,\"item_returned\":%s,"
            "\"count_before\":%d,\"count_after\":%d}",
            use_state.source_pc, use_state.clicked_id, fr_bool(use_state.activation_entered),
            fr_bool(use_state.activation_returned), fr_bool(use_state.activated),
            fr_bool(use_state.item_entered), fr_bool(use_state.item_returned),
            use_state.count_before, use_state.count_after);
    else
        printf("null");
    printf(",\"world_tick\":");
    if (world_call == NULL)
        printf("null");
    else
        printf("{\"pc\":%u,\"scheduled_returned\":%s,\"random_returned\":%s,\"chunks\":%zu,"
               "\"samples\":%zu,\"block_ticks\":%zu}",
               world_state.source_pc, fr_bool(world_state.scheduled_returned),
               fr_bool(world_state.random_returned),
               world_state.random == NULL ? 0 : world_state.random->chunks_returned,
               world_state.random == NULL ? 0 : world_state.random->samples_returned,
               world_state.random == NULL ? 0 : world_state.random->block_ticks);
    puts("}");
    CHECK(ro_state(&o, "after"));
    CHECK(mc_world_runtime_tick_retire(&world_call, NULL) == MC_WORLD_STORAGE_OK &&
          mc_client_singleplayer_mining_retire(&mining_call, NULL) == MC_SINGLEPLAYER_MINING_OK &&
          mc_client_singleplayer_use_retire(&use_call, NULL) == MC_SINGLEPLAYER_USE_OK &&
          mc_world_placement_error_retire(&placement) && ro_state(&o, "retired"));
    if (retained_failure) {
        CHECK(o.base.receipt == producer && producer->holds == 1);
        const size_t after_call_holds = producer->holds;
        const mc_world_storage_status retired =
            mc_world_access_receipt_retire(&o.base.receipt, NULL);
        CHECK(retired == MC_WORLD_STORAGE_OK && o.base.receipt == NULL);
        printf("ORE_RECEIPT {\"busy_before\":%d,\"before_holds\":%zu,\"after_busy_holds\":%zu,"
               "\"after_call_holds\":%zu,\"retire\":%d,\"same_identity\":%s,\"selected_source\":%s,"
               "\"exception\":\"%s\",\"attempts\":2}\n",
               (int)receipt_busy, receipt_before, receipt_after_busy, after_call_holds,
               (int)retired, fr_bool(same_receipt), fr_bool(selected_source), receipt_exception);
    } else if (o.base.receipt != NULL)
        CHECK(mc_world_access_receipt_retire(&o.base.receipt, NULL) == MC_WORLD_STORAGE_OK);
    /* Deliberately null source inputs stay null through the final snapshot.
     * Restoring their prepared native owners happens only for final disposal. */
    CHECK(ro_state(&o, "cleanup"));
    world->fields.u = o.world_accesses;
    CHECK(mc_world_runtime_remove_world_access(world, o.base.listener, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(o.base.listener, NULL) == MC_WORLD_STORAGE_OK);
    o.base.listener = NULL;
    ok = true;
cleanup:
    if (mc_world_runtime_tick_retire(&world_call, NULL) != MC_WORLD_STORAGE_OK ||
        mc_client_singleplayer_mining_retire(&mining_call, NULL) != MC_SINGLEPLAYER_MINING_OK ||
        mc_client_singleplayer_use_retire(&use_call, NULL) != MC_SINGLEPLAYER_USE_OK ||
        !mc_world_placement_error_retire(&placement))
        return false;
    if (o.base.receipt != NULL &&
        mc_world_access_receipt_retire(&o.base.receipt, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (o.base.listener != NULL) {
        if (o.base.f != NULL && o.base.f->worlds[0] != NULL) {
            mc_world_runtime *live_world = o.base.f->worlds[0];
            if (o.world_accesses != NULL)
                live_world->fields.u = o.world_accesses;
            if (mc_world_runtime_remove_world_access(live_world, o.base.listener, &removed, NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
        }
        if (mc_world_access_free(o.base.listener, NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    free(display);
    return ro_dispose(&o) && ok;
}

#ifndef MC_ORE_ENTRY
#define MC_ORE_ENTRY main
#endif
int MC_ORE_ENTRY(int argc, char **argv) {
    ro_profile p = {0};
    fr_profile *b = &p.prepared;
    char root[8192];
    if (argc != 33 || !decode_path(argv[2], root, sizeof root))
        return 2;
    b->mode = argv[1];
    b->operation = argv[3];
    b->biome = 'i';
    for (const char *c = b->mode; *c != '\0'; ++c)
        if ((*c < 'a' || *c > 'z') && (*c < '0' || *c > '9') && *c != '-')
            return 2;
    if (strcmp(b->operation, "left") != 0 && strcmp(b->operation, "walk") != 0 &&
        strcmp(b->operation, "right") != 0 && strcmp(b->operation, "tick") != 0 &&
        strcmp(b->operation, "display") != 0 && strcmp(b->operation, "world-tick") != 0 &&
        strcmp(b->operation, "mining") != 0 && strcmp(b->operation, "right-use") != 0)
        return 2;
    int64_t values[25] = {0};
    const int arguments[] = {4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 17, 18, 19,
                             20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    for (size_t i = 0; i < COUNT(arguments); ++i)
        if (!fr_integer(argv[arguments[i]], &values[i]) ||
            (i != 4 && (values[i] < INT32_MIN || values[i] > INT32_MAX)))
            return 2;
    for (size_t i = 0; i < 3; ++i)
        b->xyz[i] = (int32_t)values[i];
    b->entry_id = (int32_t)values[3];
    b->seed = values[4];
    b->errors[0] = (int32_t)values[5];
    b->errors[1] = (int32_t)values[6];
    p.particle_error = (int32_t)values[7];
    b->errors[3] = (int32_t)values[8];
    b->prequeue_delay = (int32_t)values[9];
    for (size_t i = 5; i <= 8; ++i)
        if (values[i] < 0)
            return 2;
    if (b->prequeue_delay < -1 || (b->entry_id != 73 && b->entry_id != 74))
        return 2;
    int32_t records[FR_MAX_CELLS][5];
    if (!fr_records(argv[14], records, 5, &b->cell_count))
        return 2;
    for (size_t i = 0; i < b->cell_count; ++i)
        b->cells[i] =
            (fr_cell){records[i][0], records[i][1], records[i][2], records[i][3], records[i][4]};
    if (!fr_records(argv[15], records, 3, &b->height_count) ||
        !ro_hex16(argv[16], &b->scheduled_before))
        return 2;
    for (size_t i = 0; i < b->height_count; ++i)
        for (size_t j = 0; j < 3; ++j)
            b->heights[i][j] = records[i][j];
    const size_t booleans[] = {10, 11, 12, 15, 16, 17, 18, 19, 20, 21, 22};
    for (size_t i = 0; i < COUNT(booleans); ++i)
        if (values[booleans[i]] != 0 && values[booleans[i]] != 1)
            return 2;
    b->scheduled_initialized = values[10] != 0;
    b->random_player = values[11] != 0;
    p.player = values[12] != 0;
    b->g = (int32_t)values[13];
    b->cooldown = (int32_t)values[14];
    p.null_random = values[15] != 0;
    p.null_tick_random = values[16] != 0;
    b->null_provider = values[17] != 0;
    p.null_unlit = values[18] != 0;
    p.null_lit = values[19] != 0;
    p.null_world = values[20] != 0;
    p.null_access = values[21] != 0;
    b->entity_initialized = values[22] != 0;
    b->entity_next = (int32_t)values[23];
    p.item_use_stat = (int32_t)values[24];
    if (strcmp(argv[32], "none") != 0 || !ro_run(root, &p))
        return 1;
    return printf("PASS: %u World Redstone Ore checks;mode=%s\n", checks, b->mode) < 0 ? 1 : 0;
}
