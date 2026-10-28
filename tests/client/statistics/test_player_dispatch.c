#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/client/application_state.h"
#include "../../../src/client/startup/game_directory.h"
#include "../../../src/client/statistics/application.h"
#include "../../../src/client/statistics/internal.h"
#include "../../../src/entity/player/player_internal.h"
#include "mc/entity/player/statistics.h"
#include "mc/world/dimension/world_provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define CD(p) _chdir(p)
#else
#include <unistd.h>
#define CD(p) chdir(p)
#endif

static size_t checks;
static mc_math_trig trig;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Player statistics contract:%d: %s\n", __LINE__, #c);                  \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    client_application app; /* The actual complete production storage. */
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_world_info_name *name;
    mc_entity *player;
    mc_client_achievement_notice *notice_retained;
    const mc_statistic *root, *child, *next, *general;
} fixture;

static mc_client_stats_status quiet(void *context, const mc_client_stats_diagnostic *event) {
    (void)context;
    (void)event;
    return MC_CLIENT_STATS_OK;
}

static bool setup(fixture *f, const char *resources, const char *directory, bool construct_writer) {
    memset(f, 0, sizeof(*f));
    CHECK(mc_resource_files_create(resources, &f->app.files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->app.materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->app.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->app.profile, NULL, &f->app.entities) ==
          MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->app.entities, f->app.profile,
                                          &f->app.render_classes) == MC_CLIENT_MODEL_OK);
    const mc_registry_options ro = {MC_REGISTRY_CLIENT, f->app.profile, f->app.materials, 0,
                                    mc_resource_files_source(f->app.files)};
    CHECK(mc_registry_classes_allocate(&ro, &f->app.registry, NULL) == MC_REGISTRY_OK);
    mc_client_achievement_notice *notice = NULL;
    CHECK(mc_client_achievement_notice_allocate(f->app.render_classes, f->app.registry,
                                                f->app.profile, &notice,
                                                NULL) == MC_CLIENT_NOTICE_OK);
    CHECK(mc_client_achievement_notice_construct(notice, &f->app, NULL) == MC_CLIENT_NOTICE_OK);
    f->app.achievement_notice = notice; /* Actual u publication after real constructor. */
    f->notice_retained = notice;
    mc_client_statistics_owner_error owner;
    CHECK(mc_client_application_create_session(&f->app, NULL, NULL, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE);
    CHECK(mc_client_application_construct_water_effect(&f->app, NULL, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    CHECK(f->app.water_effect != NULL && f->app.water_effect == f->app.water_effect_retained);
    CHECK(mc_client_application_statistics_active_use(&f->app, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    CHECK(f->app.statistics_active_use_completed);
    static const uint16_t user[] = {'P', 'l', 'a', 'y', 'e', 'r', '1', '2', '3'}, token[] = {'-'};
    mc_player_string *username = NULL, *session_id = NULL;
    CHECK(mc_player_string_create(user, 9, &username) == MC_PLAYER_OK);
    CHECK(mc_player_string_create(token, 1, &session_id) == MC_PLAYER_OK);
    CHECK(mc_client_application_create_session(&f->app, username, session_id, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    mc_player_string_release(username);
    mc_player_string_release(session_id);
    CHECK(mc_client_game_directory_create(directory, NULL, &f->app.diagnostic_game_directory, NULL,
                                          NULL) == MC_CLIENT_GAME_DIRECTORY_OK);
    if (construct_writer) {
        mc_client_stats_options options = mc_client_stats_default_options();
        options.charset = MC_JAVA_READER_UTF8;
        options.diagnostics.emit = quiet;
        CHECK(mc_client_application_construct_statistics(&f->app, &options, NULL, &owner) ==
              MC_CLIENT_STATISTICS_OWNER_OK);
        CHECK(f->app.stats_writer == f->app.stats_writer_retained && f->app.stats_writer != NULL);
        mc_client_stats_state state;
        CHECK(mc_client_stats_writer_read_state(f->app.stats_writer, &state));
        CHECK(state.workers_started == 1 && state.workers_joined == 0); /* No startup join. */
        CHECK(mc_client_stats_writer_join(f->app.stats_writer, NULL) ==
              MC_CLIENT_STATS_OK); /* Finite test observation. */
    }
    mc_registry_achievement_view av;
    mc_registry_statistic_view sv;
    CHECK(mc_registry_classes_inspect_achievements(f->app.registry, &av, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_statistics(f->app.registry, &sv, NULL) == MC_REGISTRY_OK);
    f->root = av.named[0];
    f->child = av.named[1];
    f->next = av.named[2];
    f->general = sv.named[0];
    CHECK(mc_statistic_achievement_fields_get(f->root)->c == NULL);
    CHECK(mc_statistic_achievement_fields_get(f->child)->c == f->root);
    CHECK(mc_statistic_achievement_fields_get(f->next)->c == f->child);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->app.profile, &trig,
                                          &f->app.world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->app.world_classes, f->app.registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->app.world_classes, f->app.entities) ==
          MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t blocks[256] = {0};
    mc_registry_block_view bv;
    CHECK(mc_registry_classes_inspect_blocks(f->app.registry, &bv, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        blocks[i] = bv.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options so = {.session_token = 6381,
                                       .block_map = blocks,
                                       .block_map_length = 256,
                                       .limits = mc_world_store_default_limits()};
    char world_path[128];
    CHECK(strlen(directory) < 80);
    CHECK(snprintf(world_path, sizeof(world_path), "%s/world", directory) > 0);
    CHECK(mc_world_store_open(world_path, f->regions, &so, &f->store, NULL) == MC_WORLD_STORAGE_OK);
    static const uint16_t world_name[] = {'S', 't', 'a', 't', 's'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){world_name, 5}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->app.world_classes, &limits, &f->world,
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->store, .name = f->name, .seed = 5143, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_local_player_allocate(f->app.entities, &f->player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(f->player, f->world, &f->app, f->app.session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    CHECK(f->player->phase == MC_ENTITY_READY &&
          f->player->local_player->fields.minecraft == &f->app);
    return true;
}

static bool teardown(fixture *f) {
    mc_entity_release(f->player);
    f->player = NULL;
    CHECK(mc_client_application_retire_statistics(&f->app, NULL) == MC_CLIENT_STATISTICS_OWNER_OK);
    CHECK(mc_client_application_retire_water_effect(&f->app, NULL) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    CHECK(f->app.water_effect == NULL && f->app.water_effect_retained == NULL);
    CHECK(f->app.stats_writer == NULL && f->app.stats_writer_retained == NULL &&
          f->app.session == NULL && f->app.session_classes == NULL);
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_unbind(f->dimension);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(f->dimension);
    CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(f->regions);
    mc_world_info_name_release(f->name);
    CHECK(mc_world_runtime_classes_free(f->app.world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_client_achievement_notice_free(&f->app.achievement_notice, NULL) ==
          MC_CLIENT_NOTICE_OK);
    CHECK(mc_client_render_classes_free(f->app.render_classes) == MC_CLIENT_MODEL_OK);
    CHECK(mc_registry_classes_free(f->app.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f->app.entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->app.profile);
    mc_materials_destroy(f->app.materials);
    mc_resource_files_free(f->app.files);
    mc_client_game_directory_free(f->app.diagnostic_game_directory);
    return true;
}

static bool project(fixture *f, const char *label, mc_player_statistic_status outcome) {
    mc_client_stats_state s;
    CHECK(mc_client_stats_writer_read_state(f->app.stats_writer_retained, &s));
    bool rp, cp, np;
    int32_t rv, cv, nv, gv;
    CHECK(mc_client_stats_writer_contains(f->app.stats_writer_retained, f->root, &rp, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_contains(f->app.stats_writer_retained, f->child, &cp, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_contains(f->app.stats_writer_retained, f->next, &np, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_value(f->app.stats_writer_retained, f->root, &rv, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_value(f->app.stats_writer_retained, f->child, &cv, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_value(f->app.stats_writer_retained, f->next, &nv, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_value(f->app.stats_writer_retained, f->general, &gv, NULL) ==
          MC_CLIENT_STATS_OK);
    const mc_client_achievement_notice_fields *n =
        mc_client_achievement_notice_fields_get(f->notice_retained);
    const mc_statistic_fields *nf = mc_statistic_fields_get(n->f);
    printf("STAT\t%s\t%s\t%zu\t%zu\t%d\t%d\t%ld\t%d\t%ld\t%d\t%ld\t%ld\t%ld\t%d\t%d\n", label,
           outcome == MC_PLAYER_STATISTIC_OK ? "OK" : "NULL", s.total_count, s.unsent_count,
           (int)s.dirty, (int)rp, (long)rv, (int)cp, (long)cv, (int)np, (long)nv, (long)gv,
           nf == NULL ? -1L : (long)nf->e, (int)n->i, (int)(n->g != 0));
    return true;
}

static bool effect_projection(fixture *f) {
    mc_water_texture_state state;
    CHECK(mc_client_water_texture_read(f->app.water_effect, &state));
    const mc_water_texture_fields *p = &state.fields;
    const void *arrays[] = {p->a, p->g, p->h, p->i, p->j};
    bool distinct = true, zero = true;
    for (size_t n = 0; n < 5; ++n) {
        CHECK(arrays[n] != NULL);
        for (size_t j = 0; j < n; ++j)
            distinct = distinct && arrays[n] != arrays[j];
        for (size_t j = 0; j < 1024; ++j)
            zero = zero && ((const unsigned char *)arrays[n])[j] == 0;
    }
    const mc_block *block = NULL;
    CHECK(mc_registry_block_static_get(f->app.registry, "B", &block, NULL) == MC_REGISTRY_OK);
    CHECK(block != NULL && mc_block_fields_get(block)->bn == 8 &&
          mc_block_fields_get(block)->bm == 205);
    CHECK(state.phase == MC_WATER_TEXTURE_READY && state.array_bytes == 5120 && distinct && zero);
    CHECK(p->b == 205 && !p->c && p->d == 0 && p->e == 1 && p->f == 0 && p->k == 0);
    CHECK(p->a_length == 1024 && p->g_length == 256 && p->h_length == 256 && p->i_length == 256 &&
          p->j_length == 256);
    printf("EFFECT\twater\t8\t%ld\t%d\t%ld\t%ld\t%ld\t%ld\t%zu\t%zu\t%zu\t%zu\t%zu\t%d\t%d\n",
           (long)p->b, (int)p->c, (long)p->d, (long)p->e, (long)p->f, (long)p->k, p->a_length,
           p->g_length, p->h_length, p->i_length, p->j_length, (int)distinct, (int)zero);
    return true;
}

static bool water_prefixes(fixture *f) {
    const size_t limits[] = {1, 1024, 2048, 3072, 4096, 5120};
    /* Independently verified aw/vs newarray instruction PCs, not a derived
     * stride. Successful g/h/i/j publications are16/25/34/43. */
    static const int32_t newarray_pcs[] = {8, 14, 23, 32, 41};
    const size_t registry_holds = f->app.registry->runtime_borrows;
    for (size_t n = 0; n < 6; ++n) {
        mc_client_water_texture_effect *owner = NULL;
        const mc_water_texture_options options = {limits[n]};
        mc_water_texture_error error;
        CHECK(mc_client_water_texture_allocate(f->app.registry, f->app.profile, &options, &owner,
                                               &error) == MC_WATER_TEXTURE_OK);
        CHECK(f->app.registry->runtime_borrows == registry_holds + 1);
        CHECK(mc_client_water_texture_construct(owner, &error) ==
              (n == 5 ? MC_WATER_TEXTURE_OK : MC_WATER_TEXTURE_LIMIT));
        mc_water_texture_state state;
        CHECK(mc_client_water_texture_read(owner, &state));
        CHECK(state.array_bytes == (n == 0 ? 0 : n * 1024));
        CHECK(state.fields.a_length == (n == 0 ? 0 : 1024));
        CHECK(state.fields.e == (n == 0 ? 0 : 1) && state.fields.b == (n == 0 ? 0 : 205));
        const size_t lengths[] = {state.fields.g_length, state.fields.h_length,
                                  state.fields.i_length, state.fields.j_length};
        for (size_t a = 0; a < 4; ++a)
            CHECK(lengths[a] == (n > a + 1 ? 256u : 0u));
        CHECK(state.phase == (n == 5 ? MC_WATER_TEXTURE_READY : MC_WATER_TEXTURE_FAILED));
        if (n < 5)
            CHECK(error.source_pc == newarray_pcs[n] &&
                  error.vs_pc == (n == 0 ? 7 : error.source_pc) &&
                  strcmp(error.source_class, n == 0 ? "aw" : "vs") == 0);
        CHECK(mc_client_water_texture_construct(owner, NULL) == MC_WATER_TEXTURE_INVALID_PHASE);
        CHECK(mc_client_water_texture_retire(&owner, NULL) == MC_WATER_TEXTURE_OK && owner == NULL);
        CHECK(f->app.registry->runtime_borrows == registry_holds);
    }
    /* Source-profile fixture mutates the actual native static cell after real
     * class construction. This is a bounded null-getfield prefix, not a claim
     * that ordinary Java final B is mutable or normally null. */
    mc_block *saved = f->app.registry->blocks.named[7];
    f->app.registry->blocks.named[7] = NULL;
    mc_client_water_texture_effect *owner = NULL;
    mc_water_texture_error water;
    CHECK(mc_client_water_texture_allocate(f->app.registry, f->app.profile, NULL, &owner, NULL) ==
          MC_WATER_TEXTURE_OK);
    CHECK(mc_client_water_texture_construct(owner, &water) == MC_WATER_TEXTURE_NULL_REFERENCE &&
          water.source_pc == 4);
    mc_water_texture_state state;
    CHECK(mc_client_water_texture_read(owner, &state) && state.array_bytes == 0 &&
          state.fields.e == 0);
    f->app.registry->blocks.named[7] = saved;
    CHECK(mc_client_water_texture_retire(&owner, NULL) == MC_WATER_TEXTURE_OK);
    client_application *attempt = calloc(1, sizeof(*attempt));
    CHECK(attempt != NULL);
    attempt->registry = f->app.registry;
    attempt->profile = f->app.profile;
    attempt->entities = f->app.entities;
    const mc_water_texture_options quota = {1};
    mc_client_statistics_owner_error error;
    CHECK(mc_client_application_construct_water_effect(attempt, &quota, &error) ==
          MC_CLIENT_STATISTICS_OWNER_WATER_FAILED);
    CHECK(error.site.pc == 119 && error.water.source_pc == 8 && error.water.vs_pc == 7);
    CHECK(attempt->water_effect == NULL && attempt->water_effect_retained != NULL &&
          !attempt->statistics_active_use_completed);
    CHECK(mc_client_application_construct_water_effect(attempt, NULL, NULL) ==
          MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE);
    CHECK(mc_client_application_retire_water_effect(attempt, NULL) ==
              MC_CLIENT_STATISTICS_OWNER_OK &&
          attempt->water_effect_retained == NULL);
    free(attempt);
    return true;
}

static bool award(fixture *f, const char *label, const mc_statistic *stat, int32_t amount,
                  mc_player_statistic_status expected, unsigned int pc) {
    mc_player_statistic_error error;
    CHECK(mc_local_player_add_statistic(f->player, stat, amount, &error) == expected);
    CHECK(error.site.pc == pc);
    if (pc != 0)
        CHECK(strcmp(error.site.class_name, "dc") == 0 &&
              strcmp(error.site.descriptor, "(Lvr;I)V") == 0);
    return project(f, label, expected);
}

static bool dispatch_cases(fixture *f) {
    mc_client_stats_writer *writer = f->app.stats_writer;
    f->player->local_player->fields.minecraft = NULL;
    f->app.stats_writer = NULL;
    f->app.achievement_notice = NULL;
    CHECK(award(f, "null", NULL, INT32_MIN, MC_PLAYER_STATISTIC_OK, 0));
    CHECK(award(f, "general-null-client", f->general, 1, MC_PLAYER_STATISTIC_NULL_REFERENCE, 85));
    CHECK(award(f, "root-null-client", f->root, 1, MC_PLAYER_STATISTIC_NULL_REFERENCE, 45));
    CHECK(award(f, "child-null-client", f->child, 1, MC_PLAYER_STATISTIC_NULL_REFERENCE, 28));
    f->player->local_player->fields.minecraft = &f->app;
    CHECK(award(f, "general-null-I", f->general, 1, MC_PLAYER_STATISTIC_NULL_REFERENCE, 90));
    CHECK(award(f, "root-null-I", f->root, 1, MC_PLAYER_STATISTIC_NULL_REFERENCE, 49));
    CHECK(award(f, "child-null-I", f->child, 1, MC_PLAYER_STATISTIC_NULL_REFERENCE, 35));
    f->app.stats_writer = writer;
    CHECK(award(f, "general-zero", f->general, 0, MC_PLAYER_STATISTIC_OK, 0));
    CHECK(award(f, "general-negative", f->general, -7, MC_PLAYER_STATISTIC_OK, 0));
    CHECK(award(f, "general-large", f->general, INT32_MAX, MC_PLAYER_STATISTIC_OK, 0));
    CHECK(award(f, "general-wrap", f->general, 10, MC_PLAYER_STATISTIC_OK, 0));
    CHECK(award(f, "blocked-parent", f->child, 1, MC_PLAYER_STATISTIC_OK, 0));
    CHECK(award(f, "root-null-u", f->root, 0, MC_PLAYER_STATISTIC_NULL_REFERENCE, 63));
    f->app.achievement_notice = f->notice_retained; /* settings remains genuinely NULL. */
    CHECK(award(f, "root-zero", f->root, 0, MC_PLAYER_STATISTIC_OK, 0));
    f->app.achievement_notice = NULL;
    CHECK(award(f, "root-negative-present", f->root, -3, MC_PLAYER_STATISTIC_OK, 0));
    f->app.achievement_notice = f->notice_retained;
    CHECK(award(f, "child-zero-parent-negative", f->child, 0, MC_PLAYER_STATISTIC_OK, 0));
    f->app.achievement_notice = NULL;
    CHECK(award(f, "child-negative-present", f->child, -2, MC_PLAYER_STATISTIC_OK, 0));
    CHECK(award(f, "next-null-u", f->next, -4, MC_PLAYER_STATISTIC_NULL_REFERENCE, 63));
    f->app.achievement_notice = f->notice_retained;
    CHECK(award(f, "next-negative", f->next, -4, MC_PLAYER_STATISTIC_OK, 0));
    f->app.achievement_notice = NULL;
    CHECK(award(f, "next-zero-present", f->next, 0, MC_PLAYER_STATISTIC_OK, 0));
    f->app.achievement_notice = f->notice_retained;
    return true;
}

static bool native_guards_and_prefixes(fixture *f) {
    mc_player_statistic_error error;
    CHECK(mc_local_player_add_statistic(NULL, NULL, 0, &error) ==
              MC_PLAYER_STATISTIC_INVALID_ARGUMENT &&
          error.site.class_name == NULL);
    mc_entity *raw = NULL;
    CHECK(mc_local_player_allocate(f->app.entities, &raw, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_add_statistic(raw, NULL, 0, &error) ==
              MC_PLAYER_STATISTIC_INVALID_PHASE &&
          error.site.class_name == NULL);
    mc_entity_release(raw);
    mc_client_achievement_notice *raw_notice = NULL;
    CHECK(mc_client_achievement_notice_allocate(f->app.render_classes, f->app.registry,
                                                f->app.profile, &raw_notice,
                                                NULL) == MC_CLIENT_NOTICE_OK);
    const mc_statistic *fresh = NULL;
    mc_registry_achievement_view av;
    CHECK(mc_registry_classes_inspect_achievements(f->app.registry, &av, NULL) == MC_REGISTRY_OK);
    fresh = av.named[3];
    CHECK(mc_client_stats_writer_increment(
              f->app.stats_writer, mc_statistic_achievement_fields_get(fresh)->c, 0, NULL) ==
          MC_CLIENT_STATS_OK); /* labelled fixture input */
    mc_client_stats_state before, after;
    CHECK(mc_client_stats_writer_read_state(f->app.stats_writer, &before));
    f->app.achievement_notice = raw_notice;
    CHECK(mc_local_player_add_statistic(f->player, fresh, 1, &error) ==
          MC_PLAYER_STATISTIC_NOTICE_FAILED);
    CHECK(error.site.pc == 63 && error.notice.status == MC_CLIENT_NOTICE_INVALID_PHASE);
    CHECK(mc_client_stats_writer_read_state(f->app.stats_writer, &after) &&
          after.total_count == before.total_count && after.unsent_count == before.unsent_count);
    f->app.achievement_notice = f->notice_retained;
    CHECK(mc_client_achievement_notice_free(&raw_notice, NULL) == MC_CLIENT_NOTICE_OK);
    /* Actual native per-map limit: notice completes and unsent inserts before
     * total insertion fails. No rollback or invented dirty reset. */
    const size_t limit = f->app.stats_writer->total->options.limits.max_map_entries;
    f->app.stats_writer->total->options.limits.max_map_entries = before.total_count;
    CHECK(mc_local_player_add_statistic(f->player, fresh, 1, &error) ==
          MC_PLAYER_STATISTIC_WRITER_FAILED);
    CHECK(error.site.pc == 75 && error.writer.status == MC_CLIENT_STATS_LIMIT);
    CHECK(mc_client_stats_writer_read_state(f->app.stats_writer, &after) &&
          after.total_count == before.total_count &&
          after.unsent_count == before.unsent_count + 1 && after.dirty == before.dirty);
    CHECK(mc_client_achievement_notice_fields_get(f->notice_retained)->f == fresh);
    f->app.stats_writer->total->options.limits.max_map_entries = limit;
    return true;
}

static bool session_lifetime(fixture *f) {
    mc_player_session *old = f->app.session;
    mc_player_session_state before, after;
    CHECK(mc_player_session_read(old, &before) && before.native_holds == 2);
    static const uint16_t name[] = {'S', 'e', 'c', 'o', 'n', 'd'};
    mc_player_string *username = NULL;
    CHECK(mc_player_string_create(name, 6, &username) == MC_PLAYER_OK);
    CHECK(mc_client_application_create_session(&f->app, username, NULL, NULL) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    mc_player_string_release(username);
    CHECK(f->app.session != old);
    CHECK(mc_player_session_read(old, &after) && after.native_holds == 1);
    mc_client_stats_state state;
    CHECK(mc_client_stats_writer_read_state(f->app.stats_writer, &state) && state.session == old);
    CHECK(mc_client_application_publish_session(&f->app, NULL, NULL) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    CHECK(mc_player_session_classes_free(&f->app.session_classes, NULL) == MC_PLAYER_BUSY);
    CHECK(mc_client_stats_writer_save(f->app.stats_writer, NULL, NULL) == MC_CLIENT_STATS_OK);
    return true;
}

static bool owner_failures(const char *resources) {
    fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    CHECK(setup(f, resources, "failed-owner", false));
    CHECK(mc_client_application_publish_session(&f->app, NULL, NULL) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    mc_client_statistics_owner_error error;
    mc_client_stats_options options = mc_client_stats_default_options();
    options.diagnostics.emit = quiet;
    CHECK(mc_client_application_construct_statistics(&f->app, &options, NULL, &error) ==
          MC_CLIENT_STATISTICS_OWNER_WRITER_FAILED);
    CHECK(error.site.pc == 337 && error.writer.exception == MC_CLIENT_STATS_NULL_REFERENCE &&
          error.writer.bytecode == 48);
    CHECK(f->app.stats_writer == NULL && f->app.stats_writer_retained != NULL);
    mc_client_stats_state state;
    CHECK(mc_client_stats_writer_read_state(f->app.stats_writer_retained, &state) &&
          state.phase == MC_CLIENT_STATS_FAILED_CONSTRUCTION && state.total_identity != NULL &&
          state.unsent_identity != NULL && state.workers_started == 0);
    CHECK(mc_statistic_achievement_fields_get(f->root)->client_callback == NULL);
    CHECK(mc_client_application_construct_statistics(&f->app, &options, NULL, NULL) ==
          MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE);
    CHECK(teardown(f));
    CHECK(setup(f, resources, "kh-failure", false));
    const size_t limit = f->app.registry->options.max_allocation_bytes;
    f->app.registry->options.max_allocation_bytes =
        f->app.registry->allocation_bytes + f->app.registry->managed_current_bytes;
    CHECK(mc_client_application_construct_statistics(&f->app, &options, NULL, &error) ==
          MC_CLIENT_STATISTICS_OWNER_NOTICE_FAILED);
    CHECK(error.site.pc == 351 && error.notice.status == MC_CLIENT_NOTICE_REGISTRY_FAILED);
    CHECK(f->app.stats_writer != NULL && f->app.stats_writer == f->app.stats_writer_retained);
    CHECK(mc_client_stats_writer_read_state(f->app.stats_writer, &state) &&
          state.phase == MC_CLIENT_STATS_READY && state.workers_started == 1 &&
          state.workers_joined == 0);
    CHECK(mc_statistic_achievement_fields_get(f->root)->client_callback == NULL);
    f->app.registry->options.max_allocation_bytes = limit;
    CHECK(teardown(f));
    CHECK(setup(f, resources, "title-failure", true));
    const size_t saved = f->app.registry->options.max_allocation_bytes;
    f->app.registry->options.max_allocation_bytes =
        f->app.registry->allocation_bytes + f->app.registry->managed_current_bytes;
    mc_player_statistic_error dispatch;
    CHECK(mc_local_player_add_statistic(f->player, f->root, 0, &dispatch) ==
          MC_PLAYER_STATISTIC_NOTICE_FAILED);
    CHECK(dispatch.site.pc == 63 && dispatch.notice.status == MC_CLIENT_NOTICE_REGISTRY_FAILED &&
          dispatch.notice.source_pc == 3);
    CHECK(mc_client_achievement_notice_fields_get(f->notice_retained)->d == NULL);
    CHECK(mc_client_stats_writer_read_state(f->app.stats_writer, &state) &&
          state.total_count == 0 && state.unsent_count == 0 && !state.dirty);
    f->app.registry->options.max_allocation_bytes = saved;
    CHECK(teardown(f));
    free(f);
    return true;
}

#ifdef MC_PLAYER_STATISTICS_FAULT_TEST
extern int mc_player_statistics_clock_mode;
extern client_application *mc_player_statistics_clock_app;
extern mc_client_stats_writer *mc_player_statistics_clock_writer;

static bool clock_and_reload_controls(fixture *f) {
    mc_client_stats_writer *ready = f->app.stats_writer, *raw = NULL;
    CHECK(mc_client_stats_writer_allocate(f->app.registry, NULL, &raw, NULL) == MC_CLIENT_STATS_OK);
    mc_player_statistic_error error;
    mc_client_stats_state state;
    mc_player_statistics_clock_mode = 1;
    CHECK(mc_local_player_add_statistic(f->player, f->root, 0, &error) ==
          MC_PLAYER_STATISTIC_NOTICE_FAILED);
    CHECK(error.site.pc == 63 && error.notice.status == MC_CLIENT_NOTICE_CLOCK_FAILED &&
          error.notice.source_pc == 18);
    const mc_client_achievement_notice_fields *notice =
        mc_client_achievement_notice_fields_get(f->notice_retained);
    CHECK(notice->d != NULL && notice->e == mc_statistic_fields_get(f->root)->f &&
          notice->f == NULL && notice->g == 0);
    CHECK(mc_client_stats_writer_read_state(ready, &state) && state.total_count == 0 &&
          state.unsent_count == 0 && !state.dirty);
    mc_player_statistics_clock_app = &f->app;
    mc_player_statistics_clock_mode = 2;
    CHECK(mc_local_player_add_statistic(f->player, f->root, 0, &error) ==
              MC_PLAYER_STATISTIC_NULL_REFERENCE &&
          error.site.pc == 75);
    CHECK(notice->f == f->root && !notice->i && notice->g != 0);
    CHECK(mc_client_stats_writer_read_state(ready, &state) && state.total_count == 0 &&
          state.unsent_count == 0 && !state.dirty);
    f->app.stats_writer = ready;
    mc_player_statistics_clock_writer = raw;
    mc_player_statistics_clock_mode = 3;
    CHECK(mc_local_player_add_statistic(f->player, f->root, 0, &error) ==
          MC_PLAYER_STATISTIC_WRITER_FAILED);
    CHECK(error.site.pc == 75 && error.writer.status == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
          error.writer.bytecode == 2);
    CHECK(notice->f == f->root && !notice->i);
    CHECK(mc_client_stats_writer_read_state(ready, &state) && state.total_count == 0 &&
          state.unsent_count == 0 && !state.dirty);
    f->app.stats_writer = ready;
    mc_player_statistics_clock_mode = 0;
    CHECK(mc_client_stats_writer_retire(&raw, NULL) == MC_CLIENT_STATS_OK);
    return true;
}
#endif

#ifndef MC_PLAYER_STATISTICS_FAULT_TEST
static bool typed_file_owner(const char *resources) {
    mc_java_files *files = NULL;
    mc_client_stats_classes *classes = NULL;
    CHECK(mc_java_files_create(NULL, &files, NULL) == MC_JAVA_FILE_OK);
    CHECK(mc_client_stats_classes_create(0, &classes, NULL) == MC_CLIENT_STATS_OK);
    const char *const paths[3] = {"typed-app", "typed-app-null-session", "typed-app-kh-failure"};
    for (size_t mode = 0; mode < 3; ++mode) {
        fixture *f = calloc(1, sizeof(*f));
        CHECK(f != NULL);
        CHECK(setup(f, resources, paths[mode], false));
        uint16_t units[64];
        const size_t length = strlen(paths[mode]);
        CHECK(length < 64);
        for (size_t i = 0; i < length; ++i)
            units[i] = (uint16_t)(unsigned char)paths[mode][i];
        mc_world_info_name *path = NULL;
        CHECK(mc_world_info_name_copy((mc_nbt_string){units, length}, SIZE_MAX, &path) ==
              MC_WORLD_INFO_OK);
        CHECK(mc_java_file_allocate(files, &f->app.game_directory, NULL) == MC_JAVA_FILE_OK);
        CHECK(mc_java_file_construct_string(f->app.game_directory, path, NULL) == MC_JAVA_FILE_OK);
        mc_world_info_name_release(path);
        if (mode == 1)
            CHECK(mc_client_application_publish_session(&f->app, NULL, NULL) ==
                  MC_CLIENT_STATISTICS_OWNER_OK);
        const size_t quota = f->app.registry->options.max_allocation_bytes;
        if (mode == 2)
            f->app.registry->options.max_allocation_bytes =
                f->app.registry->allocation_bytes + f->app.registry->managed_current_bytes;
        mc_client_stats_options options = mc_client_stats_default_options();
        options.charset = MC_JAVA_READER_UTF8;
        mc_client_statistics_owner_error error;
        const mc_client_statistics_owner_status expected =
            mode == 0   ? MC_CLIENT_STATISTICS_OWNER_OK
            : mode == 1 ? MC_CLIENT_STATISTICS_OWNER_WRITER_FAILED
                        : MC_CLIENT_STATISTICS_OWNER_NOTICE_FAILED;
        CHECK(mc_client_application_construct_statistics_file(&f->app, classes, files, &options,
                                                              NULL, &error) == expected);
        CHECK(f->app.stats_writer_retained != NULL && !f->app.stats_writer_constructing);
        mc_client_stats_state state;
        CHECK(mc_client_stats_writer_read_state(f->app.stats_writer_retained, &state));
        if (mode == 1) {
            CHECK(error.site.pc == 337 && error.writer.bytecode == 48 &&
                  f->app.stats_writer == NULL &&
                  state.phase == MC_CLIENT_STATS_FAILED_CONSTRUCTION &&
                  state.workers_started == 0 && error.source_writer.base.bytecode == 48 &&
                  error.source_writer.caller_count == 1 &&
                  error.source_writer.callers[0].bytecode == 182);
        } else {
            CHECK(f->app.stats_writer == f->app.stats_writer_retained &&
                  state.phase == MC_CLIENT_STATS_READY && state.workers_started == 1 &&
                  state.workers_joined == 0);
            if (mode == 0)
                CHECK(mc_statistic_achievement_fields_get(f->root)->client_callback != NULL);
            else
                CHECK(error.site.pc == 351 &&
                      mc_statistic_achievement_fields_get(f->root)->client_callback == NULL);
            CHECK(mc_client_stats_writer_join(f->app.stats_writer, NULL) == MC_CLIENT_STATS_OK);
        }
        CHECK(mc_client_stats_classes_retire(&classes, NULL) == MC_CLIENT_STATS_BUSY &&
              classes != NULL);
        f->app.registry->options.max_allocation_bytes = quota;
        CHECK(mc_client_application_construct_statistics_file(&f->app, classes, files, &options,
                                                              NULL, NULL) ==
              MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE);
        CHECK(mc_client_application_retire_statistics(&f->app, NULL) ==
              MC_CLIENT_STATISTICS_OWNER_OK);
        CHECK(mc_java_file_release(&f->app.game_directory, NULL) == MC_JAVA_FILE_OK);
        CHECK(teardown(f));
        free(f);
    }
    CHECK(mc_client_stats_classes_retire(&classes, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_java_files_release(&files, NULL) == MC_JAVA_FILE_OK);
    return true;
}
#endif

int main(int argc, char **argv) {
#ifndef MC_PLAYER_STATISTICS_FAULT_TEST
    const bool typed_profile = argc == 4 && strcmp(argv[3], "--typed-files") == 0;
#else
    const bool typed_profile = false;
#endif
    if ((argc != 3 && !typed_profile) || CD(argv[2]) != 0 || !mc_math_trig_init(&trig)) {
        fputs("Player statistics input: resources work-directory\n", stderr);
        return 2;
    }
#ifndef MC_PLAYER_STATISTICS_FAULT_TEST
    if (typed_profile) {
        if (!typed_file_owner(argv[1]))
            return 1;
        printf("Stats source App: %zu checks\n", checks);
        return 0;
    }
#endif
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
#ifdef MC_PLAYER_STATISTICS_FAULT_TEST
    /* Referenced, not executed: retain strict no-unused diagnostics while the
     * separate target executes only its explicitly instrumented fault scope. */
    (void)dispatch_cases;
    (void)native_guards_and_prefixes;
    (void)session_lifetime;
    (void)owner_failures;
    (void)water_prefixes;
    if (!setup(f, argv[1], "clock-controls", true) || !effect_projection(f) ||
        !clock_and_reload_controls(f) || !teardown(f))
        return 1;
#else
    if (!setup(f, argv[1], "normal", true) || !effect_projection(f) || !dispatch_cases(f) ||
        !native_guards_and_prefixes(f) || !water_prefixes(f) || !session_lifetime(f) ||
        !teardown(f) || !owner_failures(argv[1]))
        return 1;
#endif
    free(f);
    printf("Player statistics: %zu checks\n", checks);
    return 0;
}
