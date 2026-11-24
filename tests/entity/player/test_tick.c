#define MC_PLAYER_MOVEMENT_MAIN mc_active_movement_fixture_main
#include "../movement/test_player_movement.c"
#undef MC_PLAYER_MOVEMENT_MAIN
#include "../../../src/client/application_state.h"
#include "../../../src/client/input/movement_input_internal.h"
#include "../../../src/client/startup/game_directory.h"
#include "../../../src/client/statistics/application.h"
#include "../../../src/entity/player/active_internal.h"
#include "../../../src/entity/player/container_interaction.h"
#include "../../../src/entity/player/nbt_internal.h"
#include "../../../src/item/stack/source_internal.h"
#include "../../../src/platform/time/millis.h"
#include "mc/entity/nbt/factory.h"
#include "mc/entity/player/coordinates.h"
#include "mc/entity/player/tick.h"
#include "mc/resources/files/files.h"
#include "mc/world/chunk/nibbles.h"
#include <xmmintrin.h>

static bool active_reference;

static mc_client_stats_status active_quiet(void *context, const mc_client_stats_diagnostic *event) {
    (void)context;
    (void)event;
    return MC_CLIENT_STATS_OK;
}

static bool app_setup(fixture *f, client_application *app, const char *root) {
    bool ok = false;
    mc_player_string *username = NULL, *token = NULL;
    app->registry = f->registry[0];
    app->entities = f->entity_classes[0];
    app->profile = f->profile;
    app->world_classes = f->classes[0];
    app->materials = f->materials;
    app->trig = &trig;
    CHECK(mc_client_render_classes_create(app->entities, app->profile, &app->render_classes) ==
          MC_CLIENT_MODEL_OK);
    CHECK(mc_client_achievement_notice_allocate(app->render_classes, app->registry, app->profile,
                                                &app->achievement_notice,
                                                NULL) == MC_CLIENT_NOTICE_OK &&
          mc_client_achievement_notice_construct(app->achievement_notice, app, NULL) ==
              MC_CLIENT_NOTICE_OK);
    CHECK(mc_game_settings_create_defaults(NULL, &app->settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_application_construct_water_effect(app, NULL, NULL) ==
              MC_CLIENT_STATISTICS_OWNER_OK &&
          mc_client_application_statistics_active_use(app, NULL) == MC_CLIENT_STATISTICS_OWNER_OK);
    static const uint16_t user[] = {'A', 'c', 't', 'i', 'v', 'e', 'P', 'l', 'a', 'y', 'e', 'r'},
                          sid[] = {'-'};
    CHECK(mc_player_string_create(user, COUNT(user), &username) == MC_PLAYER_OK &&
          mc_player_string_create(sid, COUNT(sid), &token) == MC_PLAYER_OK &&
          mc_client_application_create_session(app, username, token, NULL) ==
              MC_CLIENT_STATISTICS_OWNER_OK);
    char path[8192];
    CHECK(strlen(root) < sizeof(path) - 32 &&
          snprintf(path, sizeof(path), "%s/application", root) > 0);
    CHECK(mc_client_game_directory_create(path, NULL, &app->diagnostic_game_directory, NULL,
                                          NULL) == MC_CLIENT_GAME_DIRECTORY_OK);
    mc_client_stats_options options = mc_client_stats_default_options();
    options.charset = MC_JAVA_READER_UTF8;
    options.diagnostics.emit = active_quiet;
    CHECK(mc_client_application_construct_statistics(app, &options, NULL, NULL) ==
              MC_CLIENT_STATISTICS_OWNER_OK &&
          app->stats_writer != NULL &&
          mc_client_stats_writer_join(app->stats_writer, NULL) == MC_CLIENT_STATS_OK);
    ok = true;
cleanup:
    mc_player_string_release(username);
    mc_player_string_release(token);
    return ok;
}

static bool reset_tick(fixture *f, mc_entity *p, mc_movement_input *input) {
    bool ok = false;
    movement_air(f, 0);
    movement_floor(f, 0);
    CHECK(movement_input(p, 8.5, 8, 8.5));
    p->base.velocity_x = p->base.velocity_y = p->base.velocity_z = 0;
    p->base.aX = true;
    p->base.yaw = 37;
    p->base.aT = -17;
    p->base.bl = -9;
    p->base.bm = -10;
    p->base.bn = -11;
    p->base.aU = -11;
    p->base.aV = -13;
    p->base.private_c = true;
    p->base.bt = 17;
    p->base.bj = 0;
    p->base.private_b = 1;
    p->base.by = 3;
    p->base.bz = 300;
    mc_living_entity_fields *v = &p->local_player->fields.living;
    v->E = 20;
    v->Y = 20;
    v->V = false;
    v->an = 0;
    v->aw = v->ax = v->ay = 0;
    v->az = false;
    v->ag = .5f;
    v->X = -1;
    v->W = -1;
    v->H = 0;
    v->I = 0;
    v->J = -1;
    v->K = .25f;
    v->L = .5f;
    v->M = -1;
    v->private_a = 0;
    v->ae = v->aa = v->ad = 0;
    v->af = 0;
    v->av = 20;
    v->ak = .125f;
    v->al = .25f;
    v->am = .5f;
    v->ao = 10.5;
    v->ap = 8;
    v->aq = 8.5;
    v->ar = 10;
    v->as = 30;
    mc_player_fields *s = &p->local_player->fields.player;
    s->u = s->j = false;
    s->k = 0;
    s->h = -1;
    s->i = .125f;
    s->private_a = 0;
    s->A = false;
    s->B = 0;
    s->C = -1;
    s->z = 0;
    s->o = s->p = s->q = s->r = s->s = s->t = 0;
    s->e = s->d;
    s->v = NULL;
    f->worlds[0]->fields.B = false;
    f->worlds[0]->fields.q = 1;
    CHECK(mc_local_player_set_movement_input(p, input, NULL) == MC_PLAYER_OK &&
          mc_movement_input_reset(input, NULL) == MC_MOVEMENT_INPUT_OK);
    const mc_movement_input_fields fields = {0};
    CHECK(mc_movement_input_set_fields(input, &fields, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_entity_bounds_pool_reset(p->classes, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(p->classes, NULL) == MC_ENTITY_OK);
    mc_java_random_reseed(p->random, 38);
    mc_java_random_reseed(f->worlds[0]->fields.r, 38);
    ok = true;
cleanup:
    return ok;
}

static bool reset_stats(client_application *app) {
    bool ok = false;
    mc_registry_statistic_view view;
    CHECK(mc_registry_classes_inspect_statistics(app->registry, &view, NULL) == MC_REGISTRY_OK);
    const size_t fields[] = {5, 6, 7, 9, 10, 11, 15};
    for (size_t i = 0; i < COUNT(fields); ++i) {
        int32_t amount;
        const mc_statistic *stat = view.named[fields[i]];
        CHECK(mc_client_stats_writer_value(app->stats_writer, stat, &amount, NULL) ==
                  MC_CLIENT_STATS_OK &&
              mc_client_stats_writer_increment(app->stats_writer, stat, mc_java_i32_neg(amount),
                                               NULL) == MC_CLIENT_STATS_OK);
    }
    ok = true;
cleanup:
    return ok;
}

static bool tick_row(client_application *app, mc_entity *p, int scenario, bool complete,
                     int32_t animation) {
    if (!active_reference)
        return true;
    const mc_living_entity_fields *v = &p->local_player->fields.living;
    const mc_player_fields *s = &p->local_player->fields.player;
    printf("TICK %d %d", scenario, complete ? 1 : 0);
    const double doubles[] = {p->base.x,
                              p->base.y,
                              p->base.z,
                              p->base.velocity_x,
                              p->base.velocity_y,
                              p->base.velocity_z,
                              p->base.aJ,
                              p->base.aK,
                              p->base.aL,
                              p->base.bl,
                              p->base.bm,
                              p->base.bn,
                              s->o,
                              s->p,
                              s->q,
                              s->r,
                              s->s,
                              s->t,
                              p->bounds->min_x,
                              p->bounds->min_y,
                              p->bounds->min_z,
                              p->bounds->max_x,
                              p->bounds->max_y,
                              p->bounds->max_z};
    for (size_t i = 0; i < COUNT(doubles); ++i)
        printf(" %016" PRIx64, movement_double_bits(doubles[i]));
    const float floats[] = {p->base.yaw, p->base.aT, p->base.aU, p->base.aV, p->base.y_size,
                            p->base.bj,  p->base.bk, v->H,       v->I,       v->J,
                            v->K,        v->L,       v->M,       v->W,       v->X,
                            v->aw,       v->ax,      v->ay,      v->ak,      v->al,
                            v->am,       v->ag,      s->h,       s->i,       s->B,
                            s->C};
    for (size_t i = 0; i < COUNT(floats); ++i)
        printf(" %08" PRIx32, movement_float_bits(floats[i]));
    const int32_t ints[] = {p->base.bt, p->base.by,   p->base.bz, p->base.bv, v->Y,
                            v->an,      v->private_a, v->ae,      v->aa,      v->ad,
                            s->k,       s->private_a, s->z,       animation};
    for (size_t i = 0; i < COUNT(ints); ++i)
        printf(" %" PRId32, ints[i]);
    const bool flags[] = {p->base.aX,        p->base.aY, p->base.aZ, p->base.bx, p->base.be,
                          p->base.private_c, s->u,       s->j,       v->az,      s->e == s->d};
    for (size_t i = 0; i < COUNT(flags); ++i)
        printf(" %d", flags[i] ? 1 : 0);
    mc_registry_statistic_view stats;
    if (mc_registry_classes_inspect_statistics(app->registry, &stats, NULL) != MC_REGISTRY_OK)
        return false;
    const size_t named[] = {5, 6, 7, 9, 10, 11, 15};
    for (size_t i = 0; i < COUNT(named); ++i) {
        int32_t amount;
        if (mc_client_stats_writer_value(app->stats_writer, stats.named[named[i]], &amount, NULL) !=
            MC_CLIENT_STATS_OK)
            return false;
        printf(" %" PRId32, amount);
    }
    const mc_client_achievement_notice_fields *notice =
        mc_client_achievement_notice_fields_get(app->achievement_notice);
    const mc_statistic_fields *stat = mc_statistic_fields_get(notice->f);
    printf(" %" PRId32 " %d %d %d %012" PRIx64 "\n", stat == NULL ? -1 : stat->e, notice->i ? 1 : 0,
           notice->e != NULL ? 1 : 0, notice->g != 0 ? 1 : 0, p->random->state);
    return !ferror(stdout);
}

static bool input_fp_cases(mc_entity *p, mc_movement_input *input) {
    bool ok = false;
    mc_movement_input_error error;
    mc_movement_input_state state;
    static const int32_t code[] = {17, 31, 30, 32, 42};
    static const unsigned int pc[] = {25, 44, 63, 82, 122};
    const mc_movement_input_fields initial = {.strafe = .25f, .forward = .5f};
    for (size_t i = 0; i < COUNT(code); ++i) {
        CHECK(mc_movement_input_reset(input, NULL) == MC_MOVEMENT_INPUT_OK &&
              mc_movement_input_set_fields(input, &initial, NULL) == MC_MOVEMENT_INPUT_OK);
        CHECK(mc_local_player_input_key(p, code[i], true, NULL) == MC_PLAYER_OK);
        if (i == 4)
            CHECK(mc_local_player_input_key(p, 57, true, NULL) == MC_PLAYER_OK);
        const unsigned int saved = _mm_getcsr();
        _mm_setcsr((saved & ~0x6000u) | 0x2000u);
        const mc_movement_input_status status = mc_movement_input_update(input, p, &error);
        _mm_setcsr(saved);
        CHECK(status == MC_MOVEMENT_INPUT_UNSUPPORTED_FP && error.bytecode == pc[i] &&
              error.exception == MC_MOVEMENT_INPUT_EXCEPTION_NONE);
        CHECK(mc_movement_input_read_state(input, &state) && state.fields.strafe == 0 &&
              state.fields.forward == 0 && state.fields.jump == (i == 4) &&
              state.fields.sneak == (i == 4));
    }
    CHECK(mc_movement_input_reset(input, NULL) == MC_MOVEMENT_INPUT_OK &&
          mc_local_player_input_key(p, 57, true, NULL) == MC_PLAYER_OK);
    unsigned int saved = _mm_getcsr();
    _mm_setcsr((saved & ~0x6000u) | 0x2000u);
    mc_movement_input_status status = mc_movement_input_update(input, p, &error);
    _mm_setcsr(saved);
    CHECK(status == MC_MOVEMENT_INPUT_OK && mc_movement_input_read_state(input, &state) &&
          state.fields.jump && !state.fields.sneak);
    mc_movement_input *base = NULL;
    CHECK(mc_movement_input_create(&base, NULL) == MC_MOVEMENT_INPUT_OK &&
          mc_movement_input_set_fields(base, &initial, NULL) == MC_MOVEMENT_INPUT_OK);
    saved = _mm_getcsr();
    _mm_setcsr((saved & ~0x6000u) | 0x2000u);
    status = mc_movement_input_update(base, p, &error);
    _mm_setcsr(saved);
    CHECK(status == MC_MOVEMENT_INPUT_OK && mc_movement_input_read_state(base, &state) &&
          state.fields.strafe == .25f && state.fields.forward == .5f);
    CHECK(mc_movement_input_retire(&base, NULL) == MC_MOVEMENT_INPUT_OK);
    ok = true;
cleanup:
    return ok;
}

static bool pushout_cases(fixture *f, mc_entity *p, mc_movement_input *input) {
    bool ok = false;
    mc_player_tick_error error;
    mc_active_scope scope = {.player = p, .registry = f->registry[0]};
    static const int nx[] = {7, 9, 8, 8}, nz[] = {8, 8, 7, 9};
    for (int scenario = 0; scenario < 10; ++scenario) {
        CHECK(reset_tick(f, p, input));
        for (int y = 8; y <= 9; ++y) {
            f->states[0]->data->blocks[boxes_index(8, y, 8)] = 1;
            for (size_t i = 0; i < 4; ++i)
                f->states[0]->data->blocks[boxes_index(nx[i], y, nz[i])] = 1;
        }
        if (scenario == 0)
            for (int y = 8; y <= 9; ++y)
                f->states[0]->data->blocks[boxes_index(8, y, 8)] = 0;
        if (scenario >= 1 && scenario <= 4)
            for (int y = 8; y <= 9; ++y)
                f->states[0]->data->blocks[boxes_index(nx[scenario - 1], y, nz[scenario - 1])] = 0;
        if (scenario == 6) {
            f->states[0]->data->blocks[boxes_index(7, 8, 8)] = 0;
            for (int y = 8; y <= 9; ++y)
                f->states[0]->data->blocks[boxes_index(9, y, 8)] = 0;
        }
        if (scenario == 7 || scenario == 8)
            for (size_t i = 0; i < 4; ++i)
                for (int y = 8; y <= 9; ++y)
                    f->states[0]->data->blocks[boxes_index(nx[i], y, nz[i])] = 0;
        if (scenario == 9) {
            f->states[0]->data->blocks[boxes_index(8, 8, 8)] = 0;
            for (int y = 8; y <= 9; ++y)
                f->states[0]->data->blocks[boxes_index(7, y, 8)] = 0;
        }
        p->base.velocity_x = .125;
        p->base.velocity_y = -.25;
        p->base.velocity_z = .375;
        mc_player_tick_error_init(&error);
        CHECK(mc_active_pushout(&scope, scenario == 8 ? 8.75 : 8.25, 8.5,
                                scenario == 8 ? 8.25 : 8.75, &error) &&
              error.status == MC_ENTITY_OK && f->worlds[0]->active_calls == 0 &&
              f->worlds[0]->local_leases == 0);
        const double vx = scenario == 1 || scenario == 7 || scenario == 9   ? -(double).1f
                          : scenario == 2 || scenario == 6 || scenario == 8 ? (double).1f
                                                                            : .125;
        const double vz = scenario == 3 ? -(double).1f : scenario == 4 ? (double).1f : .375;
        CHECK(p->base.velocity_x == vx && p->base.velocity_y == -.25 && p->base.velocity_z == vz);
        if (active_reference)
            printf("PUSH %d %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n", scenario,
                   movement_double_bits(p->base.velocity_x),
                   movement_double_bits(p->base.velocity_y),
                   movement_double_bits(p->base.velocity_z));
    }
    ok = true;
cleanup:
    return ok;
}

/* Store a real named Item in a separate, not-yet-acquired Chunk. The ordinary
 * region loader, Entity factory and Chunk activation must reach WorldAccess;
 * no provider/getter replacement supplies the callback. */
static bool second_corner_record(fixture *f) {
    bool ok = false, serialized = false;
    fixture_record *r = calloc(1, sizeof(*r));
    mc_item_stack *stack = NULL;
    mc_entity *item = NULL;
    mc_nbt_tree *tree = NULL;
    uint8_t *encoded = NULL;
    size_t length = 0, written = 0;
    const mc_nbt_tag *entities[1];
    const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                 MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    CHECK(r != NULL && mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 1, 1, 0) == MC_ITEM_STACK_OK &&
          mc_item_entity_allocate(f->entity_classes[0], &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(item, f->worlds[0], 8.5, 8, 16.5, stack, NULL) == MC_ENTITY_OK &&
          mc_entity_save_nbt(item, NULL, &serialized, &tree, NULL) == MC_ENTITY_NBT_OK &&
          serialized);
    entities[0] = mc_nbt_tree_root(tree);
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[1].value.int_value = 1; /* Real source Chunk(0,1). */
    r->fields[8].value.byte_value = 1;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3].value.byte_array.data = r->arrays[i];
        r->fields[i + 3].value.byte_array.length = lengths[i];
    }
    r->arrays[0][boxes_index(8, 8, 16)] = 1;
    r->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[9].value.list.items = entities;
    r->fields[9].value.list.length = 1;
    r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[0]), 0, 1, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    mc_nbt_tree_free(tree);
    mc_entity_release(item);
    mc_item_stack_release(stack);
    free(r);
    return ok;
}

typedef struct second_corner_observer {
    mc_entity *player, *added;
    mc_entity_bounds *saved_bounds;
    size_t calls, published_count;
} second_corner_observer;

static mc_world_storage_status second_corner_added(void *context, mc_world_runtime *world,
                                                   mc_entity *entity,
                                                   mc_world_storage_error *error) {
    (void)error;
    second_corner_observer *observer = context;
    ++observer->calls;
    observer->added = entity;
    observer->published_count = world->fields.b->count;
    /* A declared temporary nullable source field fault, after the first
     * corner's arguments were captured. The genuine allocation stays owned. */
    observer->player->bounds = NULL;
    return MC_WORLD_STORAGE_OK;
}

static bool second_corner_case(fixture *f, client_application *app, mc_entity *p,
                               mc_movement_input *input) {
    bool ok = false, registered = false;
    mc_world_access *access = NULL;
    second_corner_observer observer = {.player = p, .saved_bounds = p->bounds};
    mc_player_tick_error error;
    const mc_player_action_context context = {
        .travel = {.code_registry = app->registry, .trig = &trig}};
    CHECK(reset_tick(f, p, input) && second_corner_record(f));
    CHECK(mc_entity_set_size(p, .6f, 1) == MC_ENTITY_OK &&
          mc_entity_set_position(p, 8.5, 8, 16.2) == MC_ENTITY_OK);
    const size_t entities_before = f->worlds[0]->fields.b->count;
    const size_t cached_before = f->providers[0].cache->insertion_count;
    CHECK(cached_before == 1 && mc_world_store_chunk_count(f->stores[0]) == 1);
    const mc_world_access_callbacks callbacks = {.entity_added = second_corner_added};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->worlds[0], access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    const mc_entity_status status = mc_local_player_action_tick(p, &context, NULL, &error);
    p->bounds = observer.saved_bounds; /* Restore before any assertion can exit. */
    const bool expected =
        status == MC_ENTITY_STATE_ERROR && error.local_bytecode == 349 &&
        error.pushout_call_bytecode == 326 && observer.calls == 1 && observer.added != NULL &&
        mc_entity_is_kind(observer.added, MC_ENTITY_CLASS_KIND_ITEM) &&
        observer.published_count == entities_before + 1 &&
        f->worlds[0]->fields.b->count == entities_before + 1 &&
        f->worlds[0]->fields.b->items[entities_before] == observer.added &&
        f->providers[0].cache->insertion_count == cached_before + 1 &&
        mc_world_store_chunk_count(f->stores[0]) == 2 &&
        movement_double_bits(p->base.velocity_x) == movement_double_bits((double)-.1f) &&
        p->base.velocity_y == 0 && p->base.velocity_z == 0 && p->base.bt == 17;
    if (!expected)
        fprintf(stderr, "Second-corner status%d PC%u callbacks%zu published%zu vx%016" PRIx64 "\n",
                (int)status, error.local_bytecode, observer.calls, observer.published_count,
                movement_double_bits(p->base.velocity_x));
    CHECK(expected);
    const mc_chunk_state *loaded = f->providers[0].cache->insertions[cached_before];
    CHECK(loaded != NULL && loaded->x == 0 && loaded->z == 1 && loaded->active);
    ok = true;
cleanup:
    p->bounds = observer.saved_bounds;
    if (registered) {
        bool removed;
        if (mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            ok = false;
    }
    if (access != NULL && mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    /* The imported fixture tracks its original Chunk only. Finish this last
     * native-only scene through its normal terminal teardown, then retire
     * every additional loaded Chunk while the actual World is still alive. */
    mc_chunk_state *extra[4];
    size_t count = 0;
    for (size_t i = 0; i < f->providers[0].cache->insertion_count; ++i)
        if (f->providers[0].cache->insertions[i] != f->states[0] && count < COUNT(extra))
            extra[count++] = f->providers[0].cache->insertions[i];
    if (mc_world_runtime_retire_entity_containers(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    else {
        f->terminal[0] = true;
        if (!retire_descriptors(f, 0) ||
            mc_world_runtime_retire_world(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
            ok = false;
        else {
            f->world_retired[0] = true;
            for (size_t i = 0; i < count; ++i)
                if (mc_world_store_retire_chunk(f->stores[0], extra[i], NULL) !=
                    MC_WORLD_STORAGE_OK)
                    ok = false;
        }
    }
    return ok;
}

typedef struct close_probe {
    mc_entity *player;
    mc_container *active, *original_default, *replacement;
    unsigned int mode;
    size_t calls;
    bool entered_before_prefix, prefix_returned;
} close_probe;

/* Authored whole-child port controls, independent of the real GUI adapter.
 * The actual GS.r primitive supplies its source prefix; declared child
 * completion/failure and a synchronous d mutation exercise GS.w_'s suffix. */
static mc_entity_status tick_close_host(void *context, mc_entity *player,
                                        mc_player_close_error *error) {
    close_probe *probe = context;
    ++probe->calls;
    probe->entered_before_prefix =
        player == probe->player && player->local_player->fields.player.e == probe->active &&
        player->local_player->fields.player.d == probe->original_default &&
        player->local_player->fields.player.r == 0.0;
    if (probe->mode == 6 || probe->mode == 10) {
        *error = (mc_player_close_error){
            .status = probe->mode == 6 ? MC_ENTITY_STATE_ERROR : MC_ENTITY_WORLD_FAILED,
            .operation = "authored whole-close child refusal before GS.r",
            .site = probe->mode == 6 ? (mc_player_site){"fixture/Close", "invoke", "()V", 7}
                                     : (mc_player_site){0},
            .native_policy = probe->mode == 10,
            .source_exception = probe->mode == 6,
            .exception_class = probe->mode == 6 ? "java/lang/NullPointerException" : NULL};
        return error->status;
    }
    mc_player_container_error prefix = {0};
    const mc_entity_status status = mc_player_container_restore_default(player, &prefix);
    probe->prefix_returned = status == MC_ENTITY_OK && prefix.site.pc == 8 &&
                             player->local_player->fields.player.e == probe->original_default;
    if (status != MC_ENTITY_OK) {
        *error = (mc_player_close_error){.status = status,
                                         .operation = prefix.operation,
                                         .site = prefix.site,
                                         .native_policy = prefix.native_policy,
                                         .source_exception = prefix.source_exception,
                                         .source_error = prefix.source_error,
                                         .exception_class = prefix.exception_class};
        return status;
    }
    if (probe->replacement != NULL)
        player->local_player->fields.player.d = probe->replacement;
    if (probe->mode == 8) {
        *error =
            (mc_player_close_error){.status = MC_ENTITY_STATE_ERROR,
                                    .operation = "authored whole-close child Error after GS.r",
                                    .site = {"fixture/Close", "display", "()V", 11},
                                    .parent_site = {"oo", "h", "()V", 15},
                                    .source_error = true,
                                    .exception_class = "java/lang/ExceptionInInitializerError"};
        return error->status;
    }
    *error = (mc_player_close_error){.site = {"dc", "r", "()V", 12}, .source_returned = true};
    return MC_ENTITY_OK;
}

static bool automatic_close_cases(fixture *f, client_application *app, mc_entity *p,
                                  mc_movement_input *input) {
    bool ok = false;
    mc_container *bench = NULL, *replacement = NULL;
    mc_player_fields *v = &p->local_player->fields.player;
    mc_container *original_default = v->d;
    mc_registry_statistic_view statistics;
    CHECK(mc_registry_classes_inspect_statistics(app->registry, &statistics, NULL) ==
          MC_REGISTRY_OK);
    for (unsigned int mode = 0; mode < 11; ++mode) {
        CHECK(reset_tick(f, p, input) && reset_stats(app) && v->d == original_default);
        const bool present = mode == 1 || mode == 2 || mode == 3;
        f->states[0]->data->blocks[boxes_index(8, 0, 8)] = present ? 58 : 0;
        CHECK(mc_container_workbench_create(MC_REGISTRY_CLIENT, app->registry, v->c, f->worlds[0],
                                            8, 0, 8, NULL, &bench, NULL) == MC_INVENTORY_OK);
        v->e = mode == 5 ? NULL : bench;
        if (mode == 2 || mode == 3) {
            const uint64_t bits =
                mode == 2 ? UINT64_C(0x4021000000000000) : UINT64_C(0x4021000000000001);
            double y;
            memcpy(&y, &bits, sizeof(y));
            CHECK(mc_entity_set_position(p, 8.5, y, 8.5) == MC_ENTITY_OK);
        }
        if (mode == 4)
            f->worlds[0]->fields.B = true;
        if (mode == 7 || mode == 8)
            CHECK(mc_container_player_create_default(MC_REGISTRY_CLIENT, app->registry, v->c, NULL,
                                                     &replacement, NULL) == MC_INVENTORY_OK);
        close_probe probe = {.player = p,
                             .active = bench,
                             .original_default = original_default,
                             .replacement = replacement,
                             .mode = mode};
        const mc_player_action_context context = {
            .travel = {.code_registry = app->registry, .trig = &trig},
            .close = {.context = &probe, .close = mode == 9 ? NULL : tick_close_host}};
        mc_player_tick_error error;
        const mc_entity_status status = mc_player_tick(p, &context, NULL, &error);
        const bool no_close = mode == 1 || mode == 2 || mode == 4 || mode == 5;
        const bool failed = mode == 6 || mode == 8 || mode == 9 || mode == 10;
        const mc_entity_status expected_status = mode == 10 ? MC_ENTITY_WORLD_FAILED
                                                 : failed   ? MC_ENTITY_STATE_ERROR
                                                            : MC_ENTITY_OK;
        if (status != expected_status)
            fprintf(stderr, "automatic close mode%u status%d calls%zu gs%u dc%u: %s\n", mode,
                    (int)status, probe.calls, error.player_bytecode, error.close_bytecode,
                    error.operation == NULL ? "none" : error.operation);
        CHECK(status == expected_status && probe.calls == (no_close || mode == 9 ? 0u : 1u) &&
              f->worlds[0]->active_calls == 0 && f->worlds[0]->local_leases == 0);
        if (!no_close && mode != 9)
            CHECK(probe.entered_before_prefix &&
                  probe.prefix_returned == (mode != 6 && mode != 10));
        if (no_close)
            CHECK(v->e == (mode == 5 ? NULL : bench) && v->d == original_default);
        else if (mode == 6 || mode == 10)
            CHECK(v->e == bench && v->d == original_default);
        else if (mode == 8)
            CHECK(v->e == original_default && v->d == replacement);
        else
            CHECK(v->e == (replacement == NULL ? original_default : replacement));
        if (failed) {
            CHECK(error.player_bytecode == 143 && v->r == 0.0 &&
                  error.close_bytecode == (mode == 9 ? 9u : 0u) &&
                  error.close_error.parent_site.pc == (mode == 8 ? 15u : 143u) &&
                  strcmp(error.close_error.parent_site.class_name, mode == 8 ? "oo" : "gs") == 0 &&
                  !error.close_error.source_returned);
            if (mode == 6)
                CHECK(error.close_error.source_exception && !error.close_error.source_error &&
                      !error.close_error.native_policy && error.close_error.site.pc == 7 &&
                      strcmp(error.close_error.exception_class, "java/lang/NullPointerException") ==
                          0);
            else if (mode == 8)
                CHECK(!error.close_error.source_exception && error.close_error.source_error &&
                      !error.close_error.native_policy && error.close_error.site.pc == 11 &&
                      strcmp(error.close_error.exception_class,
                             "java/lang/ExceptionInInitializerError") == 0);
            else
                CHECK(error.close_error.native_policy && !error.close_error.source_exception &&
                      !error.close_error.source_error && error.close_error.exception_class == NULL);
            if (mode == 10)
                CHECK(error.close_error.site.class_name == NULL);
        } else
            CHECK(v->r != 0.0);
        if (mode == 2 || mode == 3) {
            const double dy = p->base.y - .5;
            CHECK((dy * dy > 64.0) == (mode == 3));
        }
        int32_t play_time = -1;
        CHECK(mc_client_stats_writer_value(app->stats_writer, statistics.named[5], &play_time,
                                           NULL) == MC_CLIENT_STATS_OK &&
              play_time == (failed ? 0 : 1));
        /* All source observations are complete before independent fixture
         * ownership is restored for checked native retirement. */
        v->d = original_default;
        v->e = original_default;
        CHECK(mc_container_retire(&replacement, NULL) == MC_INVENTORY_OK &&
              mc_container_retire(&bench, NULL) == MC_INVENTORY_OK);
    }
    ok = true;
cleanup:
    v->d = original_default;
    v->e = original_default;
    if (mc_container_retire(&replacement, NULL) != MC_INVENTORY_OK ||
        mc_container_retire(&bench, NULL) != MC_INVENTORY_OK)
        ok = false;
    return ok;
}

/* Authored native parent controls. These enter the real GS.w_ body, use the
 * actual retained wake module and keep all old reference rows unchanged. */
static bool wake_tick_prepare(fixture *f, client_application *app, mc_entity *p,
                              mc_movement_input *input, int32_t direction, bool missing, bool day) {
    bool ok = false;
    mc_player_sleep_call *call = NULL;
    const mc_player_sleep_context context = {.code_registry = app->registry};
    const int32_t dx[] = {0, -1, 0, 1}, dz[] = {1, 0, -1, 0};
    CHECK(reset_tick(f, p, input) && reset_stats(app));
    mc_world_runtime *world = f->worlds[0];
    mc_chunk_data *data = f->states[0]->data;
    const mc_player_coordinates saved = {42, 10, 44};
    CHECK(mc_player_saved_spawn_set(p, &saved, NULL) == MC_PLAYER_OK);
    world->fields.f = 5;
    data->blocks[boxes_index(8, 8, 8)] = 26;
    data->blocks[boxes_index(8 - dx[direction], 8, 8 - dz[direction])] = 26;
    CHECK(mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 8, 8,
                               (uint8_t)(direction | 8)) &&
          mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8 - dx[direction], 8,
                               8 - dz[direction], (uint8_t)direction));
    CHECK(mc_player_try_sleep_begin(p, 8, 8, 8, &context, &call, NULL) == MC_PLAYER_SLEEP_OK &&
          p->local_player->fields.player.u && world->fields.J &&
          mc_player_sleep_retire(&call, NULL) == MC_PLAYER_SLEEP_OK && call == NULL);
    p->local_player->fields.player.private_a = 99;
    CHECK(mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 8, 8,
                               (uint8_t)(direction | 12)));
    if (missing) {
        data->blocks[boxes_index(8, 8, 8)] = 0;
        data->blocks[boxes_index(8 - dx[direction], 8, 8 - dz[direction])] = 0;
    }
    world->fields.f = day ? 0 : 5;
    ok = true;
cleanup:
    if (mc_player_sleep_retire(&call, NULL) != MC_PLAYER_SLEEP_OK)
        ok = false;
    return ok;
}

typedef struct wake_tick_observer {
    size_t cells, ranges;
} wake_tick_observer;

/* This declared observer refusal is a native callback control, not an original
 * failure oracle or a replacement of the wake/metadata implementation. */
static mc_world_storage_status wake_tick_cell(void *context, mc_world_runtime *world, int32_t x,
                                              int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)world;
    wake_tick_observer *observer = context;
    ++observer->cells;
    if (x == 8 && y == 8 && z == 8) {
        *error =
            (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "authored wake cell refusal", 73};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status wake_tick_range(void *context, mc_world_runtime *world,
                                               int32_t min_x, int32_t min_y, int32_t min_z,
                                               int32_t max_x, int32_t max_y, int32_t max_z,
                                               mc_world_storage_error *error) {
    (void)world;
    (void)min_x;
    (void)min_y;
    (void)min_z;
    (void)max_x;
    (void)max_y;
    (void)max_z;
    (void)error;
    ++((wake_tick_observer *)context)->ranges;
    return MC_WORLD_STORAGE_OK;
}

static bool wake_tick_cases(fixture *f, client_application *app, mc_entity *p,
                            mc_movement_input *input) {
    bool ok = false, registered = false;
    mc_world_access *access = NULL;
    mc_player_tick_error *error = calloc(1, sizeof(*error));
    struct mc_player_nbt_coordinate *node = NULL;
    size_t restored_pins = 0, restored_borrows = 0;
    bool pins_changed = false, borrows_changed = false;
    const unsigned int entered = app->registry->entered;
    const bool bed_observation = app->registry->blocks.t[26];
    const int32_t old_time = f->worlds[0]->fields.f;
    const bool old_sleeping = f->worlds[0]->fields.J;
    const mc_player_action_context context = {
        .travel = {.code_registry = app->registry, .trig = &trig}};
    wake_tick_observer observer = {0};
    CHECK(error != NULL);
    mc_player_tick_error_init(error);
    mc_player_fields *fields = &p->local_player->fields.player;
    mc_world_runtime *world = f->worlds[0];

    /* Four actual day-wake orientations, then missing Bed at night and day.
     * Missing Bed wins over daytime and does not replace savedQ. No observer
     * coordinate pin is held across any call. */
    for (int32_t scenario = 0; scenario < 6; ++scenario) {
        const bool missing = scenario >= 4;
        CHECK(wake_tick_prepare(f, app, p, input, missing ? 0 : scenario, missing, scenario != 4));
        const mc_player_coordinates *bed = fields->v, *saved = fields->private_b;
        const size_t holds = p->holds, borrows = app->registry->runtime_borrows,
                     world_holds = world->persistent_bindings;
        CHECK(mc_player_tick(p, &context, NULL, error) == MC_ENTITY_OK &&
              error->child == MC_PLAYER_TICK_CHILD_NONE && error->retained_registry == NULL &&
              !fields->u && fields->private_a == (missing ? 0 : 100) && !world->fields.J &&
              p->base.bt == 18 && p->holds == holds && app->registry->runtime_borrows == borrows &&
              world->persistent_bindings == world_holds);
        if (missing)
            CHECK(fields->private_b == saved && fields->private_b->x == 42 &&
                  fields->private_b->y == 10 && fields->private_b->z == 44);
        else
            CHECK(fields->private_b != bed && fields->private_b->x == 8 &&
                  fields->private_b->y == 8 && fields->private_b->z == 8 && fields->v == bed);
        CHECK(mc_player_tick_error_retire(error));
    }

    /* Successful wake is retired before the genuine later Living/DC failure.
     * That failure keeps the completed wake and must expose no expired WAKE arm. */
    CHECK(wake_tick_prepare(f, app, p, input, 0, false, true));
    p->local_player->fields.minecraft = NULL;
    const mc_entity_status later = mc_player_tick(p, &context, NULL, error);
    p->local_player->fields.minecraft = app;
    CHECK(later == MC_ENTITY_STATE_ERROR && error->local_bytecode == 4 &&
          error->child != MC_PLAYER_TICK_CHILD_WAKE && error->retained_registry == NULL &&
          !fields->u && fields->private_a == 100 && fields->private_b != fields->v &&
          fields->private_b->x == 8 && p->base.bt == 18 && mc_player_tick_error_retire(error));

    /* Real parent-output availability, independent of its diagnostic scratch.
     * Both refusal forms retain earlier GS timer/am work and stop at53 only. */
    CHECK(wake_tick_prepare(f, app, p, input, 0, true, true));
    const size_t empty_holds = p->holds, empty_borrows = app->registry->runtime_borrows;
    CHECK(mc_player_tick_with_child_output(p, &context, NULL, false, error) ==
              MC_ENTITY_STATE_ERROR &&
          error->player_bytecode == 53 && error->child == MC_PLAYER_TICK_CHILD_NONE &&
          error->retained_registry == NULL && fields->u && fields->private_a == 100 &&
          p->base.bt == 17 && p->base.width == .2f && p->holds == empty_holds &&
          app->registry->runtime_borrows == empty_borrows);
    CHECK(wake_tick_prepare(f, app, p, input, 0, true, true));
    CHECK(mc_player_tick(p, &context, NULL, NULL) == MC_ENTITY_STATE_ERROR && fields->u &&
          fields->private_a == 100 && p->base.bt == 17 && p->holds == empty_holds &&
          app->registry->runtime_borrows == empty_borrows);
    CHECK(reset_tick(f, p, input) && mc_player_tick(p, &context, NULL, NULL) == MC_ENTITY_OK);

    /* A real source wake fails at pin13 only after resize5/offset9. Its call
     * remains owned. After child cleanup, independent parent Registry BUSY
     * must preserve the first LIMIT/site and retain only that parent hold. */
    CHECK(wake_tick_prepare(f, app, p, input, 0, true, false));
    node = p->local_player->nbt_coordinates;
    while (node != NULL && &node->value != fields->v)
        node = node->next;
    CHECK(node != NULL && node->pins == 0);
    restored_pins = node->pins;
    node->pins = SIZE_MAX;
    pins_changed = true;
    const size_t limit_holds = p->holds, limit_borrows = app->registry->runtime_borrows;
    CHECK(mc_player_tick(p, &context, NULL, error) == MC_ENTITY_LIMIT &&
          error->player_bytecode == 53 && error->child == MC_PLAYER_TICK_CHILD_WAKE &&
          error->child_error.wake.call != NULL && error->child_error.wake.error.site.pc == 13 &&
          error->retained_registry == app->registry && fields->u && fields->private_a == 100 &&
          p->base.width == .6f && p->base.height == 1.8f && p->base.offset == 1.62f &&
          p->base.bt == 17 && p->holds == limit_holds + 1 &&
          app->registry->runtime_borrows == limit_borrows + 1);
    node->pins = restored_pins;
    pins_changed = false;
    app->registry->entered = 1; /* Deliberate native retirement refusal. */
    CHECK(!mc_player_tick_error_retire(error) && error->status == MC_ENTITY_LIMIT &&
          error->player_bytecode == 53 && error->child_error.wake.call == NULL &&
          error->child_error.wake.error.site.pc == 13 &&
          error->cleanup.child == MC_PLAYER_TICK_CLEANUP_REGISTRY &&
          error->cleanup.status == MC_ENTITY_BUSY && error->retained_registry == app->registry &&
          p->holds == limit_holds);
    app->registry->entered = entered;
    CHECK(mc_player_tick_error_retire(error) && error->retained_registry == NULL &&
          error->status == MC_ENTITY_LIMIT && app->registry->runtime_borrows == limit_borrows);

    /* Genuine occupancy/WorldAccess failure at wake80 retains the producer.
     * Two checked native cleanup refusals then consume exactly that work,
     * clearing its borrows before the remaining parent Registry is refused. */
    CHECK(wake_tick_prepare(f, app, p, input, 0, false, true));
    /* Actual mutable source t[] input selects FD.d's cell-observation arm;
     * the default Bed flag does not promise that notification. */
    app->registry->blocks.t[26] = true;
    const mc_world_access_callbacks callbacks = {.cell = wake_tick_cell, .range = wake_tick_range};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    const size_t failed_holds = p->holds, failed_borrows = app->registry->runtime_borrows,
                 failed_world_holds = world->persistent_bindings;
    CHECK(mc_player_tick(p, &context, NULL, error) == MC_ENTITY_WORLD_FAILED &&
          error->player_bytecode == 73 && error->child == MC_PLAYER_TICK_CHILD_WAKE &&
          error->child_error.wake.call != NULL && error->child_error.wake.error.site.pc == 80 &&
          error->child_error.wake.error.world.status == MC_WORLD_STORAGE_IO_ERROR &&
          error->child_error.wake.error.occupancy != NULL && fields->u &&
          fields->private_a == 100 && p->base.bt == 17 && observer.cells == 1);
    const size_t cells = observer.cells, ranges = observer.ranges;
    const mc_entity_status source_status = error->status;
    const char *const operation = error->operation;
    node = p->local_player->nbt_coordinates;
    while (node != NULL && &node->value != fields->v)
        node = node->next;
    CHECK(node != NULL && node->pins == 2);
    restored_pins = node->pins;
    node->pins = 1; /* Deliberate native metadata corruption, not source state. */
    pins_changed = true;
    CHECK(!mc_player_tick_error_retire(error) &&
          error->cleanup.child == MC_PLAYER_TICK_CLEANUP_WAKE &&
          error->cleanup.wake.child == MC_PLAYER_SLEEP_CLEANUP_COORDINATE &&
          error->child_error.wake.call != NULL && error->status == source_status &&
          error->operation == operation && error->child_error.wake.error.site.pc == 80);
    node->pins = restored_pins;
    pins_changed = false;
    CHECK(app->registry->runtime_borrows > failed_borrows + 1);
    const size_t child_borrows = app->registry->runtime_borrows - failed_borrows - 1;
    restored_borrows = failed_borrows + 1;
    app->registry->runtime_borrows = child_borrows;
    borrows_changed = true; /* Omit parent/base counts; force only its later refusal. */
    CHECK(!mc_player_tick_error_retire(error) && error->child_error.wake.call == NULL &&
          error->child_error.wake.error.acquisition == NULL &&
          error->child_error.wake.error.occupancy == NULL &&
          error->child_error.wake.error.bed == NULL &&
          error->child_error.wake.error.world_sleep == NULL &&
          error->child_error.wake.error.position == NULL &&
          error->cleanup.child == MC_PLAYER_TICK_CLEANUP_REGISTRY &&
          error->retained_registry == app->registry && error->status == source_status &&
          error->operation == operation && error->player_bytecode == 73 &&
          error->child_error.wake.error.site.pc == 80 && p->holds == failed_holds &&
          world->persistent_bindings == failed_world_holds && observer.cells == cells &&
          observer.ranges == ranges);
    app->registry->runtime_borrows = restored_borrows;
    borrows_changed = false;
    CHECK(mc_player_tick_error_retire(error) && error->retained_registry == NULL &&
          app->registry->runtime_borrows == failed_borrows && observer.cells == cells &&
          observer.ranges == ranges);
    ok = true;
cleanup:
    p->local_player->fields.minecraft = app;
    app->registry->entered = entered;
    app->registry->blocks.t[26] = bed_observation;
    if (pins_changed && node != NULL)
        node->pins = restored_pins;
    if (borrows_changed)
        app->registry->runtime_borrows = restored_borrows;
    if (error != NULL && !mc_player_tick_error_retire(error))
        ok = false;
    if (registered) {
        bool removed;
        if (mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            ok = false;
    }
    if (access != NULL && mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    if (!reset_tick(f, p, input) || mc_player_saved_spawn_set(p, NULL, NULL) != MC_PLAYER_OK)
        ok = false;
    f->worlds[0]->fields.f = old_time;
    f->worlds[0]->fields.J = old_sleeping;
    mc_player_nbt_coordinates_collect_held(p);
    free(error);
    return ok;
}

static bool tick_cases(fixture *f, client_application *app, mc_entity *p,
                       mc_movement_input *input) {
    bool ok = false;
    mc_player_tick_error error;
    mc_container *bench = NULL;
    mc_item_stack *stack = NULL;
    const mc_player_action_context context = {
        .travel = {.code_registry = app->registry, .trig = &trig}};
    mc_registry_achievement_view av;
    CHECK(mc_registry_classes_inspect_achievements(app->registry, &av, NULL) == MC_REGISTRY_OK);
    mc_client_stats_writer *writer = app->stats_writer;
    mc_client_achievement_notice *notice = app->achievement_notice;
    mc_game_settings *settings = app->settings;
    for (int scenario = 0; scenario < 24; ++scenario) {
        CHECK(reset_tick(f, p, input) && reset_stats(app));
        mc_living_entity_fields *v = &p->local_player->fields.living;
        mc_player_fields *s = &p->local_player->fields.player;
        if (scenario == 0)
            p->local_player->fields.minecraft = NULL;
        if (scenario == 1)
            app->stats_writer = NULL;
        if (scenario == 2)
            app->achievement_notice = NULL;
        if (scenario == 4) {
            CHECK(mc_client_stats_writer_increment(writer, av.named[0], 0, NULL) ==
                  MC_CLIENT_STATS_OK);
            CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
                  mc_item_stack_construct(stack, 262, 1, 0) == MC_ITEM_STACK_OK &&
                  mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK &&
                  mc_inventory_player_set(s->c, 0, stack, NULL) == MC_INVENTORY_OK);
        }
        if (scenario == 5)
            CHECK(mc_local_player_input_key(p, 17, true, NULL) == MC_PLAYER_OK);
        if (scenario == 6)
            CHECK(mc_local_player_input_key(p, 57, true, NULL) == MC_PLAYER_OK);
        if (scenario == 7 || scenario == 18)
            CHECK(mc_local_player_input_key(p, 42, true, NULL) == MC_PLAYER_OK);
        if (scenario == 8) {
            s->j = true;
            s->k = 6;
        }
        if (scenario == 9)
            s->private_a = 109;
        if (scenario == 10)
            s->private_a = INT32_MAX;
        if (scenario == 11 || scenario == 12) {
            s->u = true;
            s->private_a = 99;
            if (scenario == 11)
                f->worlds[0]->fields.B = true;
        }
        if (scenario == 13)
            v->Y = 0;
        if (scenario == 14) {
            p->base.width = .6f;
            p->base.height = 1.8f;
            p->base.offset = 1.62f;
            CHECK(mc_entity_set_position(p, 8.5, 9.62, 8.5) == MC_ENTITY_OK);
        }
        if (scenario == 15) {
            const uint64_t xb = UINT64_C(0x7ff8123456789abc), zb = UINT64_C(0xfff8456789abcdef);
            double x, z;
            memcpy(&x, &xb, 8);
            memcpy(&z, &zb, 8);
            CHECK(mc_entity_set_position(p, x, 8, z) == MC_ENTITY_OK);
        }
        if (scenario == 16) {
            s->B = .025f;
            s->z = 1;
        }
        if (scenario == 17) {
            s->B = -0.0f;
            s->z = INT32_MAX;
        }
        if (scenario == 18)
            p->base.y_size = -.1f;
        if (scenario == 19)
            CHECK(mc_local_player_set_movement_input(p, NULL, NULL) == MC_PLAYER_OK);
        if (scenario == 20) {
            v->an = 2;
            p->base.yaw = 350;
            p->base.aT = 10;
        }
        if (scenario == 21 || scenario == 22) {
            if (scenario == 21)
                f->states[0]->data->blocks[boxes_index(8, 7, 8)] = 58;
            CHECK(mc_container_workbench_create(MC_REGISTRY_CLIENT, app->registry, s->c,
                                                f->worlds[0], 8, 7, 8, NULL, &bench,
                                                NULL) == MC_INVENTORY_OK);
            s->e = bench;
        }
        if (scenario == 23)
            app->settings = NULL;
        const bool expected = !(scenario == 0 || scenario == 1 || scenario == 2 || scenario == 12 ||
                                scenario == 19 || scenario == 22);
        int64_t before, after;
        CHECK(mc_platform_current_time_millis(&before) == MC_PLATFORM_MILLIS_OK);
        const mc_entity_status status = mc_player_tick(p, &context, NULL, &error);
        CHECK(mc_platform_current_time_millis(&after) == MC_PLATFORM_MILLIS_OK);
        p->local_player->fields.minecraft = app;
        app->stats_writer = writer;
        app->achievement_notice = notice;
        app->settings = settings;
        if ((status == MC_ENTITY_OK) != expected)
            fprintf(stderr, "tick%d status%d %s gs%u ls%u dc%u child %s\n", scenario, (int)status,
                    error.operation == NULL ? "-" : error.operation, error.player_bytecode,
                    error.living_bytecode, error.local_bytecode,
                    error.child != MC_PLAYER_TICK_CHILD_ACTION ||
                            error.child_error.action.operation == NULL
                        ? "-"
                        : error.child_error.action.operation);
        CHECK((status == MC_ENTITY_OK) == expected && f->worlds[0]->active_calls == 0 &&
              f->worlds[0]->local_leases == 0);
        int32_t animation = -1;
        if (stack != NULL) {
            mc_item_stack_fields fields;
            CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK);
            animation = fields.animation;
        }
        if (scenario != 22)
            CHECK(tick_row(app, p, scenario, status == MC_ENTITY_OK, animation));
        if (scenario == 0)
            CHECK(error.local_bytecode == 4 && p->base.bt == 18);
        if (scenario == 1)
            CHECK(error.local_bytecode == 10);
        if (scenario == 2)
            CHECK(error.local_bytecode == 26);
        if (scenario == 3) {
            const mc_client_achievement_notice_fields *n =
                mc_client_achievement_notice_fields_get(notice);
            CHECK(n->f == av.named[0] && n->i && n->g >= mc_java_i64_sub(before, 2500) &&
                  n->g <= mc_java_i64_sub(after, 2500));
        }
        if (scenario == 4)
            CHECK(animation == 4);
        if (scenario == 9)
            CHECK(s->private_a == 0);
        if (scenario == 10)
            CHECK(s->private_a == INT32_MIN);
        if (scenario == 12)
            CHECK(error.player_bytecode == 43 && error.bed_bytecode == 8 && s->private_a == 100 &&
                  p->base.bt == 17);
        if (scenario == 15)
            CHECK((movement_float_bits(v->L) & UINT32_C(0x7fffffff)) >
                  UINT32_C(0x7f800000)); /* NaN enters source fcmpg branch. */
        if (scenario == 19)
            CHECK(error.local_bytecode == 257);
        if (scenario == 22)
            CHECK(error.player_bytecode == 143 && error.close_bytecode == 9 && s->e == s->d);
        if (stack != NULL) {
            CHECK(mc_inventory_player_set(s->c, 0, NULL, NULL) == MC_INVENTORY_OK);
            mc_item_stack_release(stack);
            stack = NULL;
        }
        if (bench != NULL) {
            s->e = s->d;
            CHECK(mc_container_retire(&bench, NULL) == MC_INVENTORY_OK);
        }
    }
    CHECK(reset_tick(f, p, input) && reset_stats(app) &&
          mc_local_player_input_key(p, 17, true, NULL) == MC_PLAYER_OK);
    for (int tick = 0; tick < 16; ++tick) {
        CHECK(mc_entity_bounds_pool_reset(p->classes, NULL) == MC_ENTITY_OK &&
              mc_entity_vector_pool_reset(p->classes, NULL) == MC_ENTITY_OK);
        CHECK(mc_player_tick(p, &context, NULL, &error) == MC_ENTITY_OK &&
              tick_row(app, p, 100 + tick, true, -1));
    }
    CHECK(input_fp_cases(p, input) && pushout_cases(f, p, input));
    if (!active_reference)
        CHECK(automatic_close_cases(f, app, p, input) && wake_tick_cases(f, app, p, input) &&
              second_corner_case(f, app, p, input));
    ok = true;
cleanup:
    if (stack != NULL) {
        (void)mc_inventory_player_set(p->local_player->fields.player.c, 0, NULL, NULL);
        mc_item_stack_release(stack);
    }
    if (bench != NULL) {
        p->local_player->fields.player.e = p->local_player->fields.player.d;
        (void)mc_container_retire(&bench, NULL);
    }
    return ok;
}
#ifndef MC_PLAYER_ACTIVE_TICK_MAIN
#define MC_PLAYER_ACTIVE_TICK_MAIN main
#endif
#ifndef MC_PLAYER_ACTIVE_TICK_CASES
#define MC_PLAYER_ACTIVE_TICK_CASES tick_cases
#endif
int MC_PLAYER_ACTIVE_TICK_MAIN(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    client_application *app = NULL;
    mc_entity *p = NULL;
    mc_movement_input *input = NULL;
    char root[8192], resources_root[8192];
    if (argc != 4 ||
        (strcmp(argv[1], "player-active") != 0 &&
         strcmp(argv[1], "player-active-reference") != 0) ||
        !decode_path(argv[2], root, sizeof(root)) ||
        !decode_path(argv[3], resources_root, sizeof(resources_root)) || !mc_math_trig_init(&trig))
        return 1;
    active_reference = strcmp(argv[1], "player-active-reference") == 0;
    f = calloc(1, sizeof(*f));
    app = calloc(1, sizeof(*app));
    CHECK(f != NULL && app != NULL && mc_materials_create(&f->materials));
    CHECK(mc_resource_files_create(resources_root, &app->files) == MC_REGISTRY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    static const uint16_t title[] = {'A', 'c', 't', 'i', 'v', 'e'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {
            side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER, f->profile, f->materials, 0,
            side == 0 ? mc_resource_files_source(app->files) : &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
              MC_ENTITY_OK);
        mc_registry_error registry_error;
        const mc_registry_status rs =
            mc_registry_classes_initialize_blocks(f->registry[side], &registry_error);
        if (rs != MC_REGISTRY_OK)
            fprintf(stderr, "Registry side%zu code%d %s\n", side, (int)rs,
                    registry_error.message == NULL ? "-" : registry_error.message);
        CHECK(rs == MC_REGISTRY_OK);
        CHECK(boxes_open_store(f, side, root));
        CHECK(boxes_create_world(f, side));
    }
    CHECK(app_setup(f, app, root) &&
          mc_local_player_allocate(app->entities, &p, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(p, f->worlds[0], app, app->session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          mc_movement_input_create_from_options(app->settings, &input, NULL) ==
              MC_MOVEMENT_INPUT_OK &&
          reset_tick(f, p, input));
    bool inserted;
    CHECK(mc_world_runtime_insert_entity(f->worlds[0], p, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted && p->base.bF);
    CHECK(MC_PLAYER_ACTIVE_TICK_CASES(f, app, p, input));
    result = 0;
cleanup:
    (void)mc_movement_input_retire(&input, NULL);
    mc_entity_release(p);
    if (app != NULL) {
        if (mc_client_application_retire_statistics(app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
            mc_client_application_retire_water_effect(app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
            mc_client_achievement_notice_free(&app->achievement_notice, NULL) !=
                MC_CLIENT_NOTICE_OK ||
            (app->render_classes != NULL &&
             mc_client_render_classes_free(app->render_classes) != MC_CLIENT_MODEL_OK))
            result = 1;
    }
    if (!cleanup_fixture(f))
        result = 1;
    if (app != NULL) {
        if (mc_game_settings_retire(&app->settings, NULL) != MC_GAME_SETTINGS_OK)
            result = 1;
        mc_client_game_directory_free(app->diagnostic_game_directory);
        mc_resource_files_free(app->files);
        free(app);
    }
    if (result == 0 && printf("Player active tick: %u checks\n", checks) < 0)
        return 1;
    return result;
}
