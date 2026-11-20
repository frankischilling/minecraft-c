/* Authored native controls for the retained GS93 NBT join.
 * Inputs below are authored component operands, not an original observation
 * packet or a full Player construction/publication qualification. */
#include "../../../src/entity/construction_internal.h"
#include "../../../src/entity/position_internal.h"
#include "mc/entity/player/sleep.h"
#include "mc/inventory/player.h"
#include "mc/world/runtime/world_access.h"

typedef enum nbt_sleep_case_kind {
    NBT_SLEEP_FULL,
    NBT_SLEEP_ABSENT,
    NBT_SLEEP_PARTIAL,
    NBT_SLEEP_WRONG_SPAWN,
    NBT_SLEEP_WRONG_SCORE,
    NBT_SLEEP_NO_OUTPUT,
    NBT_SLEEP_NO_OUTPUT_AWAKE,
    NBT_SLEEP_NULL_WORLD,
    NBT_SLEEP_NULL_BOUNDS
} nbt_sleep_case_kind;

typedef struct nbt_sleep_observer {
    mc_entity *player;
    mc_entity_bounds *bounds;
    mc_entity_bounds at_callback;
    size_t cells, ranges;
    bool saw_prefix, changed;
} nbt_sleep_observer;

typedef struct nbt_sleep_fixture {
    mc_entity *player;
    mc_entity_bounds *bounds;
    mc_entity_nbt_error error;
    mc_world_access *access;
    nbt_sleep_observer observer;
    bool registered;
} nbt_sleep_fixture;

/* Deliberately mutate the real bounds source field from the reached FD.d
 * cell notification. The observer is enabled only by the explicitly authored
 * mutable t[26] input. No default Bed notification behavior is claimed. */
static mc_world_storage_status nbt_sleep_cell(void *context, mc_world_runtime *world, int32_t x,
                                              int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)world;
    (void)error;
    nbt_sleep_observer *o = context;
    ++o->cells;
    if (x == 8 && y == 64 && z == 8 && !o->changed) {
        const mc_entity *p = o->player;
        const mc_player_fields *fields = &p->local_player->fields.player;
        o->saw_prefix = p->bounds == o->bounds && p->base.x == 8.5 && p->base.y == 64.9375 &&
                        p->base.z == 8.5 && p->base.width == .6f && p->base.height == 1.8f &&
                        p->base.offset == 1.62f && fields->u && fields->private_a == 37 &&
                        fields->v != NULL && fields->v->x == 8 && fields->v->y == 64 &&
                        fields->v->z == 8 && o->bounds->min_x == 8.5 - (double)(.9f / 2.0f) &&
                        o->bounds->min_y == 64.9375 - (double).4f &&
                        o->bounds->max_y == 64.9375 - (double).4f + (double)1.4f;
        o->at_callback = *o->bounds;
        o->player->bounds = NULL;
        o->changed = true;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status nbt_sleep_range(void *context, mc_world_runtime *world,
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
    ++((nbt_sleep_observer *)context)->ranges;
    return MC_WORLD_STORAGE_OK;
}

static bool nbt_sleep_cell_input(mc_chunk_data *data, int32_t x, int32_t y, int32_t z, uint8_t id,
                                 uint8_t metadata) {
    data->blocks[boxes_index(x, y, z)] = id;
    return mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z, metadata);
}

static bool nbt_sleep_scene(fixture *f, int32_t direction) {
    bool ok = false;
    static const int32_t facing[4][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}};
    mc_chunk_data *data = f->states[0]->data;
    memset(data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(data->heightmap, 64, MC_CHUNK_HEIGHTMAP_BYTES);
    data->minimum_height = 64;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z) {
            CHECK(nbt_sleep_cell_input(data, x, 63, z, 2, 0));
            for (int32_t y = 64; y < 128; ++y)
                CHECK(mc_chunk_nibbles_set(data->skylight, MC_CHUNK_NIBBLE_BYTES, x, y, z, 15));
        }
    CHECK(nbt_sleep_cell_input(data, 8, 64, 8, 26, (uint8_t)(direction | 12)) &&
          nbt_sleep_cell_input(data, 8 - facing[direction][0], 64, 8 - facing[direction][1], 26,
                               (uint8_t)direction));
    ok = true;
cleanup:
    return ok;
}

static bool nbt_sleep_player(client_application *app, mc_world_runtime *world, bool raw,
                             nbt_sleep_fixture *s) {
    bool ok = false;
    mc_inventory_player *inventory = NULL;
    mc_entity_error construction = {0};
    const mc_player_coordinates saved = {42, 66, 44};
    CHECK(mc_local_player_allocate(app->entities, &s->player, NULL) == MC_ENTITY_OK);
    if (raw) {
        /* A canonical RAW allocation, the real base body and a real owned
         * Inventory occurrence are the prepared partial reader operand.
         * There is no phase/hold rewrite or claim of a complete GS/DC ctor. */
        CHECK(s->player->phase == MC_ENTITY_RAW &&
              mc_entity_construct_base(s->player, world, &construction) == MC_ENTITY_OK &&
              mc_inventory_player_create(MC_REGISTRY_CLIENT, s->player, NULL, &inventory, NULL) ==
                  MC_INVENTORY_OK);
        s->player->local_player->fields.player.c = inventory;
        inventory = NULL;
        CHECK(s->player->phase == MC_ENTITY_RAW);
    } else
        CHECK(mc_local_player_construct(s->player, world, app, app->session, 0, NULL, NULL) ==
                  MC_PLAYER_OK &&
              s->player->phase == MC_ENTITY_READY);
    s->bounds = s->player->bounds;
    CHECK(s->bounds != NULL && s->player->random != NULL &&
          mc_player_saved_spawn_set(s->player, &saved, NULL) == MC_PLAYER_OK);
    s->player->local_player->fields.player.g = 900;
    /* Distinct pre-wake dimensions show that source resize5 really ran;
     * base NBT294 rebuilds these bounds before any callback can null them. */
    mc_entity_source_size(s->player, .9f, 1.4f);
    s->player->base.offset = .4f;
    s->player->base.y_size = 0;
    s->observer.player = s->player;
    s->observer.bounds = s->bounds;
    ok = true;
cleanup:
    if (mc_inventory_player_retire(&inventory, NULL) != MC_INVENTORY_OK)
        ok = false;
    return ok;
}

/* Omit presence entries before invocation; the ordinary fresh input stays
 * byte-identical throughout the production reader and every cleanup retry. */
static void nbt_sleep_omit(nbt_input_node *root, const char *name) {
    for (size_t at = 0; at < root->tag.value.compound.length;) {
        const mc_nbt_tag *tag = root->children[at];
        bool match = tag->name.length == strlen(name);
        for (size_t i = 0; match && i < tag->name.length; ++i)
            match = tag->name.units[i] == (uint16_t)(unsigned char)name[i];
        if (!match) {
            ++at;
            continue;
        }
        const size_t tail = --root->tag.value.compound.length - at;
        memmove(&root->children[at], &root->children[at + 1], tail * sizeof(root->children[0]));
        root->children[root->tag.value.compound.length] = NULL;
    }
}

static const mc_nbt_tag *nbt_sleep_input(nbt_input *input, nbt_sleep_case_kind kind) {
    if (nbt_make_input(input, "sleep") == NULL)
        return NULL;
    nbt_input_node *root = &input->nodes[0];
    const double position[] = {8.5, 64.9375, 8.5};
    if (!nbt_vector(input, root, "Pos", MC_NBT_DOUBLE, position, COUNT(position)) ||
        !nbt_scalar(input, root, "Health", MC_NBT_SHORT, 20) ||
        !nbt_scalar(input, root, "Dimension", MC_NBT_INT, 0) ||
        !nbt_scalar(input, root, "SleepTimer", MC_NBT_SHORT, 37))
        return NULL;
    if (kind == NBT_SLEEP_ABSENT) {
        nbt_sleep_omit(root, "SpawnX");
        nbt_sleep_omit(root, "SpawnY");
        nbt_sleep_omit(root, "SpawnZ");
    } else if (kind == NBT_SLEEP_PARTIAL)
        nbt_sleep_omit(root, "SpawnZ");
    else if (kind == NBT_SLEEP_WRONG_SPAWN && !nbt_scalar(input, root, "SpawnY", MC_NBT_SHORT, 66))
        return NULL;
    else if (kind == NBT_SLEEP_WRONG_SCORE && !nbt_scalar(input, root, "Score", MC_NBT_SHORT, 123))
        return NULL;
    else if (kind == NBT_SLEEP_NO_OUTPUT_AWAKE &&
             !nbt_scalar(input, root, "Sleeping", MC_NBT_BYTE, 0))
        return NULL;
    return &root->tag;
}

static bool nbt_sleep_no_producers(const mc_player_sleep_error *error) {
    return error != NULL && error->acquisition == NULL && error->occupancy == NULL &&
           error->bed == NULL && error->world_sleep == NULL && error->position == NULL;
}

static bool nbt_sleep_no_pins(const mc_entity *player) {
    for (const struct mc_player_nbt_coordinate *node = player->local_player->nbt_coordinates;
         node != NULL; node = node->next)
        if (node->pins != 0)
            return false;
    return true;
}

static bool nbt_sleep_case(fixture *f, client_application *app, bool raw, int32_t direction,
                           nbt_sleep_case_kind kind) {
    bool ok = false;
    nbt_sleep_fixture *s = calloc(1, sizeof(*s));
    nbt_input *input = calloc(1, sizeof(*input));
    nbt_input *unchanged = calloc(1, sizeof(*unchanged));
    mc_world_runtime *world = f->worlds[0];
    const unsigned int entered = app->registry->entered;
    const bool observation = app->registry->blocks.t[26];
    const size_t outer_borrows = app->registry->runtime_borrows;
    const size_t outer_bindings = world->persistent_bindings;
    CHECK(s != NULL);
    mc_entity_nbt_error_init(&s->error);
    CHECK(input != NULL && unchanged != NULL && nbt_sleep_scene(f, direction) &&
          nbt_sleep_player(app, world, raw, s));
    mc_entity *p = s->player;
    mc_player_fields *fields = &p->local_player->fields.player;
    const mc_entity_phase phase = p->phase;
    const uintptr_t old_saved = (uintptr_t)fields->private_b;
    const size_t holds = p->holds, borrows = app->registry->runtime_borrows,
                 bindings = world->persistent_bindings;
    const mc_java_random player_random = *p->random, world_random = *world->fields.r;
    const mc_nbt_tag *tag = nbt_sleep_input(input, kind);
    CHECK(tag != NULL);
    *unchanged = *input;
    if (kind == NBT_SLEEP_NULL_WORLD)
        p->base.world = NULL; /* Authored nullable source World, binding restored at cleanup. */
    if (kind == NBT_SLEEP_NULL_BOUNDS) {
        app->registry->blocks.t[26] = true;
        const mc_world_access_callbacks callbacks = {.cell = nbt_sleep_cell,
                                                     .range = nbt_sleep_range};
        CHECK(mc_world_access_create(&callbacks, &s->observer, &s->access, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(world, s->access, NULL) == MC_WORLD_STORAGE_OK);
        s->registered = true;
    }
    const bool no_output = kind == NBT_SLEEP_NO_OUTPUT || kind == NBT_SLEEP_NO_OUTPUT_AWAKE;
    const mc_entity_nbt_status status =
        mc_entity_read_nbt_in_context(p, app->registry, tag, NULL, no_output ? NULL : &s->error);
    CHECK(memcmp(input, unchanged, sizeof(*input)) == 0 && p->phase == phase &&
          nbt_random_equal(p->random, &player_random) &&
          nbt_random_equal(world->fields.r, &world_random));
    uint8_t metadata = 0;
    CHECK(mc_chunk_nibbles_get(f->states[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8,
                               &metadata));
    if (no_output) {
        CHECK(p->holds == holds && app->registry->runtime_borrows == borrows &&
              world->persistent_bindings == bindings && s->error.sleep_work == NULL &&
              p->base.width == .9f && p->base.height == 1.4f && p->base.offset == .4f &&
              p->base.x == 8.5 && p->base.y == 64.9375 && p->base.z == 8.5 &&
              metadata == (uint8_t)(direction | 12));
        if (kind == NBT_SLEEP_NO_OUTPUT)
            CHECK(status == MC_ENTITY_NBT_UNSUPPORTED_TYPE && fields->u &&
                  fields->private_a == 37 && fields->v != NULL && fields->v->x == 8 &&
                  fields->v->y == 64 && fields->v->z == 8 &&
                  (uintptr_t)fields->private_b == old_saved && fields->g == 900);
        else
            CHECK(status == MC_ENTITY_NBT_OK && !fields->u && fields->private_a == 37 &&
                  fields->v == NULL && fields->private_b != NULL && fields->private_b->x == 5 &&
                  fields->private_b->y == 66 && fields->private_b->z == -7 && fields->g == 123);
    } else if (kind == NBT_SLEEP_NULL_WORLD || kind == NBT_SLEEP_NULL_BOUNDS) {
        CHECK(status == MC_ENTITY_NBT_NULL_REFERENCE && s->error.status == status &&
              s->error.base_bytecode == 311 && s->error.local_player_bytecode == 2 &&
              s->error.player_bytecode == 93 && s->error.sleep_work != NULL &&
              s->error.sleep != NULL && s->error.sleep->status == MC_PLAYER_SLEEP_SOURCE_NULL &&
              !s->error.sleep->native_policy && s->error.sleep->source_exception &&
              !s->error.sleep->source_error && fields->u && fields->private_a == 37 &&
              (uintptr_t)fields->private_b == old_saved && fields->g == 900 &&
              p->base.width == .6f && p->base.height == 1.8f && p->base.offset == 1.62f &&
              fields->v != NULL && fields->v->x == 8 && fields->v->y == 64 && fields->v->z == 8 &&
              p->holds == holds + 1);
        if (kind == NBT_SLEEP_NULL_WORLD) {
            CHECK(s->error.sleep->site.pc == 48 && nbt_sleep_no_producers(s->error.sleep) &&
                  metadata == (uint8_t)(direction | 12) && p->base.x == 8.5 &&
                  p->base.y == 64.9375 && p->base.z == 8.5 &&
                  app->registry->runtime_borrows == borrows + 1 &&
                  world->persistent_bindings == bindings);
            struct mc_player_nbt_coordinate *node = p->local_player->nbt_coordinates;
            while (node != NULL && &node->value != fields->v)
                node = node->next;
            CHECK(node != NULL && node->pins == 2); /* Passive observation, no test pin. */
        } else {
            CHECK(s->observer.changed && s->observer.saw_prefix && s->observer.cells == 1 &&
                  p->bounds == NULL && s->error.sleep->site.pc == 175 &&
                  s->error.sleep->child.pc == 86 && s->error.sleep->position != NULL &&
                  s->error.sleep->position->source_null &&
                  !s->error.sleep->position->native_policy &&
                  s->error.sleep->position->bytecode == 86 && s->error.sleep->occupancy != NULL &&
                  s->error.sleep->bed != NULL && metadata == (uint8_t)(direction | 8) &&
                  p->base.x == 7.5 && p->base.y == (double)((64.0f + 1.62f) + .1f) &&
                  p->base.z == 7.5 &&
                  memcmp(s->bounds, &s->observer.at_callback, sizeof(*s->bounds)) == 0);
        }
        const unsigned int wake_pc = s->error.sleep->site.pc;
        const char *const operation = s->error.operation;
        const size_t cells = s->observer.cells, ranges = s->observer.ranges;
        /* Deliberate native cleanup input, authored only AFTER source failed.
         * No mutable pin counters, binding counters or borrows are fabricated. */
        app->registry->entered = 1;
        CHECK(!mc_entity_nbt_error_retire(&s->error) && s->error.status == status &&
              s->error.operation == operation && s->error.player_bytecode == 93 &&
              s->error.sleep != NULL && s->error.sleep->site.pc == wake_pc &&
              s->error.sleep_work != NULL && s->error.sleep_cleanup != NULL &&
              s->error.sleep_cleanup->status == MC_ENTITY_NBT_CLEANUP_FAILED &&
              s->error.sleep_cleanup->cause.status == MC_WORLD_STORAGE_BUSY &&
              s->observer.cells == cells && s->observer.ranges == ranges);
        if (kind == NBT_SLEEP_NULL_WORLD)
            CHECK(s->error.sleep_cleanup->child == MC_ENTITY_NBT_SLEEP_CLEANUP_REGISTRY &&
                  s->error.sleep_cleanup->registry.status == MC_REGISTRY_BUSY &&
                  nbt_sleep_no_producers(s->error.sleep) && p->holds == holds &&
                  app->registry->runtime_borrows == borrows + 1);
        else
            CHECK(s->error.sleep_cleanup->child == MC_ENTITY_NBT_SLEEP_CLEANUP_WAKE &&
                  s->error.sleep_cleanup->wake != NULL &&
                  s->error.sleep_cleanup->wake->child == MC_PLAYER_SLEEP_CLEANUP_REGISTRY &&
                  s->error.sleep_cleanup->wake->registry.status == MC_REGISTRY_BUSY &&
                  s->error.sleep->position != NULL && s->error.sleep->position->source_null &&
                  p->holds == holds + 1);
        app->registry->entered = entered;
        p->bounds = s->bounds;
        CHECK(mc_entity_nbt_error_retire(&s->error) && s->error.status == status &&
              s->error.operation == operation && s->error.player_bytecode == 93 &&
              s->error.sleep_work == NULL && s->error.sleep == NULL &&
              s->error.sleep_cleanup == NULL && p->holds == holds &&
              app->registry->runtime_borrows == borrows && world->persistent_bindings == bindings &&
              s->observer.cells == cells && s->observer.ranges == ranges && fields->u &&
              fields->private_a == 37 && (uintptr_t)fields->private_b == old_saved &&
              fields->g == 900);
    } else {
        CHECK(!fields->u && fields->private_a == 0 && p->base.width == .6f &&
              p->base.height == 1.8f && p->base.offset == 1.62f && p->base.x == 7.5 &&
              p->base.y == (double)((64.0f + 1.62f) + .1f) && p->base.z == 7.5 &&
              metadata == (uint8_t)(direction | 8) && fields->v != NULL && fields->v->x == 8 &&
              fields->v->y == 64 && fields->v->z == 8 && !world->fields.J && p->holds == holds &&
              world->persistent_bindings == bindings);
        if (kind == NBT_SLEEP_ABSENT || kind == NBT_SLEEP_PARTIAL || kind == NBT_SLEEP_WRONG_SPAWN)
            CHECK((uintptr_t)fields->private_b == old_saved && fields->private_b->x == 42 &&
                  fields->private_b->y == 66 && fields->private_b->z == 44);
        else
            CHECK(fields->private_b != NULL && (uintptr_t)fields->private_b != old_saved &&
                  fields->private_b != fields->v && fields->private_b->x == 5 &&
                  fields->private_b->y == 66 && fields->private_b->z == -7);
        if (kind == NBT_SLEEP_WRONG_SPAWN || kind == NBT_SLEEP_WRONG_SCORE) {
            const unsigned int gs_pc = kind == NBT_SLEEP_WRONG_SPAWN ? 137u : 152u;
            const unsigned int dc_pc = kind == NBT_SLEEP_WRONG_SPAWN ? 2u : 9u;
            const char *const field = kind == NBT_SLEEP_WRONG_SPAWN ? "SpawnY" : "Score";
            CHECK(status == MC_ENTITY_NBT_WRONG_TAG && s->error.status == status &&
                  s->error.base_bytecode == 311 && s->error.player_bytecode == gs_pc &&
                  s->error.local_player_bytecode == dc_pc && s->error.field != NULL &&
                  strcmp(s->error.field, field) == 0 && fields->g == 900 &&
                  s->error.sleep_work != NULL && s->error.sleep == NULL &&
                  s->error.sleep_cleanup == NULL && app->registry->runtime_borrows == borrows + 1);
            const char *const operation = s->error.operation;
            app->registry->entered = 1;
            CHECK(!mc_entity_nbt_error_retire(&s->error) && s->error.status == status &&
                  s->error.operation == operation && s->error.player_bytecode == gs_pc &&
                  s->error.local_player_bytecode == dc_pc && strcmp(s->error.field, field) == 0 &&
                  s->error.sleep == NULL && s->error.sleep_cleanup != NULL &&
                  s->error.sleep_cleanup->status == MC_ENTITY_NBT_CLEANUP_FAILED &&
                  s->error.sleep_cleanup->cause.status == MC_WORLD_STORAGE_BUSY &&
                  s->error.sleep_cleanup->child == MC_ENTITY_NBT_SLEEP_CLEANUP_REGISTRY &&
                  s->error.sleep_cleanup->registry.status == MC_REGISTRY_BUSY &&
                  p->holds == holds && !fields->u && fields->private_a == 0);
            app->registry->entered = entered;
            CHECK(mc_entity_nbt_error_retire(&s->error) && s->error.status == status &&
                  s->error.operation == operation && s->error.player_bytecode == gs_pc &&
                  s->error.local_player_bytecode == dc_pc && s->error.sleep_work == NULL &&
                  s->error.sleep == NULL && s->error.sleep_cleanup == NULL &&
                  app->registry->runtime_borrows == borrows && !fields->u &&
                  fields->private_a == 0 && fields->g == 900);
        } else
            CHECK(status == MC_ENTITY_NBT_OK && s->error.status == MC_ENTITY_NBT_OK &&
                  fields->g == 123 && s->error.sleep_work == NULL && s->error.sleep == NULL &&
                  s->error.sleep_cleanup == NULL && app->registry->runtime_borrows == borrows);
    }
    CHECK(memcmp(input, unchanged, sizeof(*input)) == 0 && nbt_sleep_no_pins(p) &&
          p->phase == phase && p->holds == holds && nbt_random_equal(p->random, &player_random) &&
          nbt_random_equal(world->fields.r, &world_random));
    ok = true;
cleanup:
    /* Restore every deliberately nullable source projection and mutable input
     * even when an assertion fails, before checked work or graph teardown. */
    app->registry->entered = entered;
    app->registry->blocks.t[26] = observation;
    if (s != NULL) {
        if (s->player != NULL) {
            s->player->base.world = world;
            if (s->bounds != NULL)
                s->player->bounds = s->bounds;
        }
        if (!mc_entity_nbt_error_retire(&s->error))
            ok = false;
        if (s->error.sleep_work == NULL) {
            if (s->registered) {
                bool removed = false;
                if (mc_world_runtime_remove_world_access(world, s->access, &removed, NULL) !=
                        MC_WORLD_STORAGE_OK ||
                    !removed)
                    ok = false;
                else
                    s->registered = false;
            }
            if (!s->registered && mc_world_access_free(s->access, NULL) == MC_WORLD_STORAGE_OK) {
                mc_entity_release(s->player);
                if (app->registry->runtime_borrows != outer_borrows ||
                    world->persistent_bindings != outer_bindings)
                    ok = false;
                free(s);
            } else
                ok = false;
        } else
            ok = false;
        /* An unexpected checked refusal keeps the heap fixture, Player and
         * callback context alive on the failing-process path. It is never a
         * source retry or permission to tear down a borrowed child owner. */
    }
    free(unchanged);
    free(input);
    if (!ok)
        fprintf(stderr, "Player NBT sleep control raw=%d direction=%" PRId32 " kind=%d failed\n",
                raw ? 1 : 0, direction, (int)kind);
    return ok;
}

static bool nbt_sleep_cases(fixture *f, client_application *app) {
    bool ok = false, saved = false;
    mc_chunk_data *data = f->states[0]->data;
    mc_chunk_data *old_data = malloc(sizeof(*old_data));
    uint8_t *old_blocks = malloc(MC_CHUNK_BLOCKS);
    const bool sleeping = f->worlds[0]->fields.J;
    CHECK(old_data != NULL && old_blocks != NULL);
    *old_data = *data;
    memcpy(old_blocks, data->blocks, MC_CHUNK_BLOCKS);
    saved = true;
    for (int raw = 0; raw <= 1; ++raw)
        for (int32_t direction = 0; direction < 4; ++direction)
            CHECK(nbt_sleep_case(f, app, raw != 0, direction, NBT_SLEEP_FULL));
    const nbt_sleep_case_kind cases[] = {NBT_SLEEP_ABSENT,      NBT_SLEEP_PARTIAL,
                                         NBT_SLEEP_WRONG_SPAWN, NBT_SLEEP_WRONG_SCORE,
                                         NBT_SLEEP_NO_OUTPUT,   NBT_SLEEP_NO_OUTPUT_AWAKE,
                                         NBT_SLEEP_NULL_WORLD,  NBT_SLEEP_NULL_BOUNDS};
    for (int raw = 0; raw <= 1; ++raw)
        for (size_t i = 0; i < COUNT(cases); ++i)
            CHECK(nbt_sleep_case(f, app, raw != 0, 0, cases[i]));
    ok = !ferror(stdout);
cleanup:
    if (saved) {
        memcpy(data->blocks, old_blocks, MC_CHUNK_BLOCKS);
        *data = *old_data;
    }
    f->worlds[0]->fields.J = sleeping;
    free(old_blocks);
    free(old_data);
    return ok;
}
