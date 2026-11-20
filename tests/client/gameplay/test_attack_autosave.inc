/* Test-only observations around the actual fd.l177 autosave invocation. */
#include "mc/entity/nbt/nbt.h"
#include "world/storage/objects_internal.h"

enum { ATTACK_AUTOSAVE_LIMIT = 32, ATTACK_AUTOSAVE_CHUNKS = 24 };

typedef struct attack_entity_list_snapshot {
    mc_entity_class_phase phase;
    const void *maps[4];
    size_t counts[4], bytes, live, borrowers;
    unsigned char failure[sizeof(mc_entity_registry_error)];
} attack_entity_list_snapshot;

/* Passive canonical names for this bounded fixture. These are values, not
 * EntityList lookups or a replacement map. Canonical literal identity must
 * agree before a name can be selected. Query-only classes have no name. */
static const char *const attack_entity_names[] = {
    "Arrow",  "Snowball", "Item",   "Painting", "Mob",       "Monster",     "Creeper",  "Skeleton",
    "Spider", "Giant",    "Zombie", "Slime",    "Ghast",     "PigZombie",   "Pig",      "Sheep",
    "Cow",    "Chicken",  "Squid",  "Wolf",     "PrimedTnt", "FallingSand", "Minecart", "Boat"};

static int attack_entity_list_read(const mc_entity_classes *classes,
                                   attack_entity_list_snapshot *out) {
    CHECK(classes != NULL && (classes->registry_phase == MC_ENTITY_CLASS_COLD ||
                              classes->registry_phase == MC_ENTITY_CLASS_READY));
    out->phase = classes->registry_phase;
    out->bytes = classes->allocated_bytes;
    out->live = classes->live_entities;
    out->borrowers = classes->borrowers;
    memcpy(out->failure, &classes->registry_failure, sizeof(out->failure));
    for (size_t i = 0; i < 4; ++i) {
        out->maps[i] = classes->registry_maps[i];
        out->counts[i] = classes->registry_maps[i] == NULL ? 0 : classes->registry_maps[i]->count;
        CHECK(out->phase != MC_ENTITY_CLASS_COLD || (out->maps[i] == NULL && out->counts[i] == 0));
    }
    return 0;
}

static bool attack_entity_list_same(const attack_entity_list_snapshot *a,
                                    const attack_entity_list_snapshot *b) {
    if (a->phase != b->phase || a->bytes != b->bytes || a->live != b->live ||
        a->borrowers != b->borrowers || memcmp(a->failure, b->failure, sizeof(a->failure)) != 0)
        return false;
    for (size_t i = 0; i < 4; ++i)
        if (a->maps[i] != b->maps[i] || a->counts[i] != b->counts[i])
            return false;
    return true;
}

static int attack_entity_list_json(FILE *file, const attack_entity_list_snapshot *state) {
    CHECK(fprintf(file, "{\"phase\":%d,\"bytes\":%zu,\"maps\":[", (int)state->phase, state->bytes) >
          0);
    for (size_t i = 0; i < 4; ++i)
        CHECK(fprintf(file, "%s\"%" PRIxPTR "\"", i == 0 ? "" : ",", (uintptr_t)state->maps[i]) >
              0);
    CHECK(fprintf(file, "],\"counts\":[%zu,%zu,%zu,%zu]}", state->counts[0], state->counts[1],
                  state->counts[2], state->counts[3]) > 0);
    return 0;
}

static int attack_entity_list_ready(const mc_entity_classes *classes) {
    CHECK(classes->registry_phase == MC_ENTITY_CLASS_READY);
    for (size_t map = 0; map < 4; ++map) {
        const mc_entity_registry_map *actual = classes->registry_maps[map];
        CHECK(actual != NULL && actual->count == 24);
        const mc_entity_registry_node *node = actual->head, *last = NULL;
        for (size_t i = 0; i < 24; ++i) {
            CHECK(node != NULL && node->type == &classes->literals[i] && node->record != NULL &&
                  node->record->kind == (mc_entity_class_kind)i &&
                  node->record->name.units != NULL &&
                  node->record->name.length == strlen(attack_entity_names[i]));
            for (size_t j = 0; j < node->record->name.length; ++j)
                CHECK(node->record->name.units[j] == (uint16_t)attack_entity_names[i][j]);
            last = node;
            node = node->next;
        }
        CHECK(node == NULL && actual->tail == last);
    }
    return 0;
}

static struct {
    fixture *fixture;
    mining_work *work;
    bool failed, active, focused_frames;
    size_t entries, returns, bytes;
    mc_world_runtime *world;
    int64_t next_time, time, first;
    size_t selected;
    size_t entity_list_calls;
    attack_entity_list_snapshot entity_list;
} attack_autosave;

static int attack_autosave_path(size_t index, const char *leaf, char path[8192]) {
    const char *root = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "MC_ATTACK_AUTOSAVES");
    CHECK(root != NULL);
    const int n = snprintf(path, 8192, "%s/save-%zu/%s", root, index, leaf);
    CHECK(n > 0 && n < 8192);
    return 0;
}

static int attack_autosave_bytes(const char *path, const void *data, size_t bytes) {
    CHECK(bytes <= 1048576 && attack_autosave.bytes <= 512u * 1048576u &&
          bytes <= 512u * 1048576u - attack_autosave.bytes && SDL_SaveFile(path, data, bytes));
    attack_autosave.bytes += bytes;
    return 0;
}

static int attack_autosave_disk(size_t index, const char *stage) {
    const char *source = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "MC_ATTACK_SAVES");
    char path[8192];
    CHECK(source != NULL && attack_autosave_path(index, stage, path) == 0);
    reload_snapshot_budget budget = {0};
    CHECK(reload_snapshot_tree(source, path, 0, &budget) && budget.files != 0 &&
          attack_autosave.bytes <= 512u * 1048576u &&
          budget.bytes <= 512u * 1048576u - attack_autosave.bytes);
    attack_autosave.bytes += budget.bytes;
    return 0;
}

static int attack_autosave_tree(const mc_nbt_tag *root, const char *path) {
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_output_bytes = 1048576;
    mc_nbt_error error;
    size_t size = 0, written = 0;
    CHECK(mc_nbt_measure(root, &limits, &size, &error) == MC_NBT_OK && size <= 1048576);
    uint8_t *wire = malloc(size);
    CHECK(wire != NULL);
    const bool encoded =
        mc_nbt_write(root, &limits, wire, size, &written, &error) == MC_NBT_OK && written == size;
    const int saved = encoded ? attack_autosave_bytes(path, wire, size) : 1;
    free(wire);
    CHECK(encoded && saved == 0);
    return 0;
}

typedef struct attack_inventory_snapshot {
    mc_inventory_player *owner;
    mc_item_stack *const *main_slots, *const *armor_slots;
    const mc_item_stack *cursor, *stacks[40];
    mc_item_stack_fields fields[40];
    int32_t selected;
    bool changed;
} attack_inventory_snapshot;

static int attack_inventory_snapshot_read(mc_entity *player, attack_inventory_snapshot *out) {
    mc_local_player_snapshot local;
    mc_inventory_player_state inventory;
    CHECK(mc_local_player_get_snapshot(player, &local) == MC_ENTITY_OK &&
          mc_inventory_player_read_state(local.player.c, &inventory) &&
          inventory.main_slots != NULL && inventory.armor_slots != NULL);
    *out = (attack_inventory_snapshot){.owner = local.player.c,
                                       .main_slots = inventory.main_slots,
                                       .armor_slots = inventory.armor_slots,
                                       .cursor = inventory.cursor,
                                       .selected = inventory.selected,
                                       .changed = inventory.changed};
    for (size_t i = 0; i < 40; ++i) {
        out->stacks[i] = i < 36 ? inventory.main_slots[i] : inventory.armor_slots[i - 36];
        if (out->stacks[i] != NULL)
            CHECK(mc_item_stack_fields_get(out->stacks[i], &out->fields[i]) == MC_ITEM_STACK_OK);
    }
    return 0;
}

static int attack_inventory_snapshot_same(const attack_inventory_snapshot *before,
                                          const attack_inventory_snapshot *after) {
    CHECK(before->owner == after->owner && before->main_slots == after->main_slots &&
          before->armor_slots == after->armor_slots && before->cursor == after->cursor &&
          before->selected == after->selected && before->changed == after->changed);
    for (size_t i = 0; i < 40; ++i)
        CHECK(before->stacks[i] == after->stacks[i] &&
              memcmp(&before->fields[i], &after->fields[i], sizeof(before->fields[i])) == 0);
    return 0;
}

static int attack_inventory_snapshot_receipt(size_t event, const attack_inventory_snapshot *raw) {
    char line[2048];
    int length = snprintf(line, sizeof(line), "ATTACK_INVENTORY_SAVE event=%zu slots=", event);
    CHECK(length > 0 && (size_t)length < sizeof(line));
    size_t used = (size_t)length, slots = 0;
    for (size_t i = 0; i < 40; ++i) {
        if (raw->stacks[i] == NULL)
            continue;
        const mc_item_stack_fields *fields = &raw->fields[i];
        length = snprintf(line + used, sizeof(line) - used, "%s%zu:%d:%d:%d", slots == 0 ? "" : ",",
                          i < 36 ? i : i - 36 + 100, fields->id, fields->count, fields->damage);
        CHECK(length > 0 && (size_t)length < sizeof(line) - used);
        used += (size_t)length;
        ++slots;
    }
    CHECK(printf("%s%s\n", line, slots == 0 ? "-" : "") > 0);
    return 0;
}

static int attack_autosave_entity(mc_entity *entity, bool conditional, const char *path,
                                  bool *present) {
    CHECK(entity != NULL && entity->random != NULL);
    attack_entity_list_snapshot before, after;
    CHECK(attack_entity_list_read(entity->classes, &before) == 0);
    const mc_entity_class_literal *literal = entity->class_literal;
    CHECK(literal != NULL && literal->owner == entity->classes &&
          (unsigned int)literal->kind < MC_ENTITY_CLASS_KIND_COUNT &&
          literal == &entity->classes->literals[literal->kind]);
    const char *name = literal->kind < 24 ? attack_entity_names[literal->kind] : NULL;
    bool written = !conditional || (!entity->base.be && name != NULL);
    if (conditional) {
        CHECK(entity->classes == attack_autosave.fixture->app->h->classes);
    } else
        CHECK(literal->kind == MC_ENTITY_CLASS_KIND_LOCAL_PLAYER);
    unsigned char base[sizeof(entity->base)];
    memcpy(base, &entity->base, sizeof(base));
    const mc_java_random random = *entity->random;
    mc_entity_nbt_error *error = calloc(1, sizeof(*error));
    mc_nbt_tree *tree = NULL;
    CHECK(error != NULL);
    const mc_entity_nbt_status status =
        written ? mc_entity_build_nbt(entity, NULL, &tree, error) : MC_ENTITY_NBT_OK;
    int saved = 0;
    if (status == MC_ENTITY_NBT_OK && written) {
        const mc_nbt_tag *body = mc_nbt_tree_root(tree);
        CHECK(body != NULL && body->type == MC_NBT_COMPOUND && body->value.compound.length <= 32);
        if (conditional) {
            static const uint16_t id_units[] = {'i', 'd'};
            uint16_t name_units[12];
            const size_t length = strlen(name);
            CHECK(length <= 12);
            for (size_t i = 0; i < length; ++i)
                name_units[i] = (uint16_t)name[i];
            const mc_nbt_tag id = {
                .type = MC_NBT_STRING, .name = {id_units, 2}, .value.string = {name_units, length}};
            const mc_nbt_tag *items[33] = {&id};
            for (size_t i = 0; i < body->value.compound.length; ++i)
                items[i + 1] = body->value.compound.items[i];
            const mc_nbt_tag wrapper = {
                .type = MC_NBT_COMPOUND,
                .value.compound = {.items = items, .length = body->value.compound.length + 1}};
            saved = attack_autosave_tree(&wrapper, path);
        } else
            saved = attack_autosave_tree(body, path);
    }
    mc_nbt_tree_free(tree);
    free(error);
    CHECK(attack_entity_list_read(entity->classes, &after) == 0 &&
          attack_entity_list_same(&before, &after) && status == MC_ENTITY_NBT_OK && saved == 0 &&
          memcmp(base, &entity->base, sizeof(base)) == 0 && entity->random->state == random.state &&
          entity->random->have_next_next_gaussian == random.have_next_next_gaussian &&
          memcmp(&entity->random->next_next_gaussian, &random.next_next_gaussian,
                 sizeof(random.next_next_gaussian)) == 0);
    *present = written;
    return 0;
}

static int attack_autosave_chunk(size_t event, size_t index, size_t cache_index,
                                 mc_world_runtime *world, mc_chunk_state *state, FILE *manifest) {
    mc_world_chunk *chunk = mc_world_store_chunk_object(world->fields.w, state);
    mc_world_chunk_fields fields;
    size_t tiles = 0;
    CHECK(chunk != NULL && mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK &&
          fields.d == world &&
          mc_world_chunk_tile_entity_count(fields.l, &tiles) == MC_WORLD_STORAGE_OK && tiles == 0 &&
          fields.b != NULL && fields.h != NULL);
    const mc_world_chunk_nibble *nibbles[] = {fields.e, fields.f, fields.g};
    mc_world_chunk_nibble_fields packed[3];
    for (size_t i = 0; i < 3; ++i)
        CHECK(mc_world_chunk_nibble_fields_get(nibbles[i], &packed[i]) == MC_WORLD_STORAGE_OK &&
              packed[i].a != NULL && packed[i].length == 16384);
    uint8_t *raw = malloc(82176);
    CHECK(raw != NULL);
    memcpy(raw, fields.b, 32768);
    for (size_t i = 0; i < 3; ++i)
        memcpy(raw + 32768 + i * 16384, packed[i].a, 16384);
    memcpy(raw + 81920, fields.h, 256);
    char name[128], path[8192];
    const int n = snprintf(name, sizeof(name), "chunk-%zu.bin", index);
    const int named =
        n > 0 && (size_t)n < sizeof(name) ? attack_autosave_path(event, name, path) : 1;
    const int saved = named == 0 ? attack_autosave_bytes(path, raw, 82176) : 1;
    free(raw);
    CHECK(named == 0 && saved == 0);
    size_t entities = 0;
    for (size_t slice = 0; slice < 8; ++slice) {
        const mc_world_chunk_entity_list *list = NULL;
        size_t count = 0;
        CHECK(mc_world_chunk_slice_at(fields.m, slice, &list) == MC_WORLD_STORAGE_OK &&
              mc_world_chunk_entity_count(list, &count) == MC_WORLD_STORAGE_OK && count <= 1024);
        for (size_t i = 0; i < count; ++i) {
            mc_entity *entity = NULL;
            CHECK(mc_world_chunk_entity_at(list, i, &entity) == MC_WORLD_STORAGE_OK &&
                  entity != NULL && entities < 1024);
            const int length =
                snprintf(name, sizeof(name), "chunk-%zu-entity-%zu.nbt", index, entities);
            bool present = false;
            ++attack_autosave.entity_list_calls;
            CHECK(length > 0 && (size_t)length < sizeof(name) &&
                  attack_autosave_path(event, name, path) == 0 &&
                  attack_autosave_entity(entity, true, path, &present) == 0);
            entities += present;
        }
    }
    CHECK(fprintf(manifest,
                  "%s{\"cache_index\":%zu,\"x\":%d,\"z\":%d,\"populated\":%s,\"entities\":%zu}",
                  index == 0 ? "" : ",", cache_index, fields.j, fields.k,
                  fields.n ? "true" : "false", entities) > 0);
    return 0;
}

static int attack_autosave_entry(mc_world_runtime *world, int64_t next_time) {
    fixture *f = attack_autosave.fixture;
    CHECK(f != NULL && f->app != NULL && f->app->active_world == world && f->app->h != NULL &&
          world->fields.d != NULL && world->fields.d->count != 0 &&
          world->fields.d->items != NULL && world->fields.d->items[0] == f->app->h &&
          !attack_autosave.active && attack_autosave.entries < ATTACK_AUTOSAVE_LIMIT &&
          repeat_focus_state(f, "autosave-entry") == 0);
    const size_t event = attack_autosave.entries++;
    attack_autosave.active = true;
    attack_autosave.world = world;
    attack_autosave.next_time = next_time;
    attack_autosave.entity_list_calls = 0;
    CHECK(attack_entity_list_read(f->app->h->classes, &attack_autosave.entity_list) == 0);
    mc_world_info_fields info;
    CHECK(mc_world_info_fields_get(world->fields.x, &info) == MC_WORLD_INFO_OK &&
          mc_platform_current_time_millis(&attack_autosave.first) == MC_PLATFORM_MILLIS_OK);
    attack_autosave.time = info.value.time;
    CHECK(next_time == mc_java_i64_add(info.value.time, 1));
    char path[8192];
    CHECK(attack_autosave_path(event, "", path) == 0 && SDL_CreateDirectory(path) &&
          attack_autosave_disk(event, "before") == 0);
    bool present = false;
    attack_inventory_snapshot inventory_before, inventory_after;
    CHECK(attack_inventory_snapshot_read(f->app->h, &inventory_before) == 0);
    CHECK(attack_autosave_path(event, "player.nbt", path) == 0 &&
          attack_autosave_entity(f->app->h, false, path, &present) == 0 && present);
    CHECK(attack_inventory_snapshot_read(f->app->h, &inventory_after) == 0 &&
          attack_inventory_snapshot_same(&inventory_before, &inventory_after) == 0 &&
          attack_inventory_snapshot_receipt(event, &inventory_before) == 0);
    /* Exercise the formerly active Boolean-save admission with the genuine
     * unnamed Player, even if this partial save selects no Entity-bearing
     * Chunk. No record is expected; a cold EntityList must remain cold. This
     * observer control is excluded from the actual source slice-call count. */
    CHECK(attack_autosave_entity(f->app->h, true, NULL, &present) == 0 && !present);
    CHECK(attack_autosave_path(event, "entry.json", path) == 0);
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    mc_chunk_cache *cache = world->lifecycle.cache;
    const mc_nbt_string level_name = mc_world_info_name_text(info.name);
    mc_chunk_state *selected[ATTACK_AUTOSAVE_CHUNKS];
    size_t selected_indices[ATTACK_AUTOSAVE_CHUNKS];
    size_t selected_count = 0;
    bool good = cache != NULL && cache->insertion_count <= 4096 &&
                (cache->insertion_count == 0 || cache->insertions != NULL);
    if (good)
        good = fprintf(file,
                       "{\"time\":%" PRId64 ",\"next_time\":%" PRId64 ",\"period\":%d,"
                       "\"first_millis\":%" PRId64 ",\"seed\":%" PRId64 ",\"size\":%" PRId64
                       ",\"spawn\":[%d,%d,%d],\"rain_time\":%d,\"thunder_time\":%d,"
                       "\"raining\":%s,\"thundering\":%s,\"name_units\":[",
                       info.value.time, next_time, world->fields.p, attack_autosave.first,
                       info.value.seed, info.value.size_on_disk, info.value.spawn_x,
                       info.value.spawn_y, info.value.spawn_z, info.value.rain_time,
                       info.value.thunder_time, info.value.raining ? "true" : "false",
                       info.value.thundering ? "true" : "false") > 0;
    for (size_t i = 0; good && i < level_name.length; ++i)
        good = fprintf(file, "%s%u", i == 0 ? "" : ",", (unsigned)level_name.units[i]) > 0;
    if (good)
        good = fprintf(file, "],\"cache\":[") > 0;
    for (size_t i = 0; good && i < cache->insertion_count; ++i) {
        mc_chunk_state *state = cache->insertions[i];
        if (state == NULL) {
            good = false;
            break;
        }
        good =
            fprintf(file,
                    "%s{\"identity\":\"%" PRIxPTR "\",\"x\":%d,\"z\":%d,\"blank\":%s,"
                    "\"never_save\":%s,\"dirty\":%s,\"has_entities\":%s,\"last_save\":%" PRId64 "}",
                    i == 0 ? "" : ",", (uintptr_t)state, state->x, state->z,
                    state->blank ? "true" : "false", state->never_save ? "true" : "false",
                    state->dirty ? "true" : "false", state->has_entities ? "true" : "false",
                    state->last_save_time) > 0;
        const bool eligible =
            !state->blank && !state->never_save &&
            (state->dirty || (state->has_entities &&
                              info.value.time >= mc_java_i64_add(state->last_save_time, 600)));
        bool repeated = false;
        for (size_t j = 0; j < selected_count; ++j)
            repeated |= selected[j] == state;
        if (eligible && !repeated && selected_count < ATTACK_AUTOSAVE_CHUNKS) {
            selected_indices[selected_count] = i;
            selected[selected_count++] = state;
        }
    }
    if (good)
        good = fprintf(file, "],\"selected\":[") > 0;
    for (size_t i = 0; good && i < selected_count; ++i)
        good = attack_autosave_chunk(event, i, selected_indices[i], world, selected[i], file) == 0;
    attack_entity_list_snapshot after;
    CHECK(attack_entity_list_read(f->app->h->classes, &after) == 0 &&
          attack_entity_list_same(&attack_autosave.entity_list, &after));
    if (good)
        good = fprintf(file, "],\"entity_list\":") > 0 &&
               attack_entity_list_json(file, &attack_autosave.entity_list) == 0 &&
               fprintf(file,
                       ",\"entity_list_calls\":%zu,\"observer_unchanged\":true,"
                       "\"unnamed_player_excluded\":true}\n",
                       attack_autosave.entity_list_calls) > 0;
    const bool closed = fclose(file) == 0;
    CHECK(good && closed);
    attack_autosave.selected = selected_count;
    printf("ATTACK_AUTOSAVE_ENTRY event=%zu time=%" PRId64 " next=%" PRId64
           " period=%d selected=%zu source=fd.l:177 focused=1\n",
           event, info.value.time, next_time, world->fields.p, selected_count);
    return 0;
}

static int attack_autosave_return(mc_world_runtime *world, int64_t next_time, bool returned) {
    CHECK(attack_autosave.active && world == attack_autosave.world &&
          next_time == attack_autosave.next_time && returned &&
          attack_autosave.returns + 1 == attack_autosave.entries);
    const size_t event = attack_autosave.entries - 1;
    mc_world_info_fields info;
    int64_t last;
    CHECK(mc_world_info_fields_get(world->fields.x, &info) == MC_WORLD_INFO_OK &&
          info.value.time == attack_autosave.time &&
          mc_platform_current_time_millis(&last) == MC_PLATFORM_MILLIS_OK &&
          attack_autosave_disk(event, "after") == 0);
    attack_entity_list_snapshot after;
    CHECK(attack_entity_list_read(attack_autosave.fixture->app->h->classes, &after) == 0);
    if (attack_autosave.entity_list_calls != 0) {
        CHECK(attack_entity_list_ready(attack_autosave.fixture->app->h->classes) == 0);
        if (attack_autosave.entity_list.phase == MC_ENTITY_CLASS_COLD)
            CHECK(after.bytes == attack_autosave.entity_list.bytes +
                                     4 * sizeof(mc_entity_registry_map) +
                                     96 * sizeof(mc_entity_registry_node));
        else
            CHECK(attack_entity_list_same(&attack_autosave.entity_list, &after));
    } else
        CHECK(attack_entity_list_same(&attack_autosave.entity_list, &after));
    char path[8192];
    CHECK(attack_autosave_path(event, "exit.json", path) == 0);
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    const bool good = fprintf(file,
                              "{\"time\":%" PRId64 ",\"size\":%" PRId64 ",\"last_millis\":%" PRId64
                              ",\"source_returned\":true,\"entity_list\":",
                              info.value.time, info.value.size_on_disk, last) > 0 &&
                      attack_entity_list_json(file, &after) == 0 && fprintf(file, "}\n") > 0;
    const bool closed = fclose(file) == 0;
    CHECK(good && closed);
    ++attack_autosave.returns;
    attack_autosave.active = false;
    printf("ATTACK_AUTOSAVE_RETURN event=%zu time=%" PRId64
           " selected=%zu source-returned=1 time-store-not-entered=1\n",
           event, info.value.time, attack_autosave.selected);
    return 0;
}

/* This symbol exists only in the attack executable's privately compiled tick
 * body. A failed observation records failure; it never changes source control
 * flow, suppresses a save, restores a clock or alters a game field. */
void mc_test_attack_autosave_observe(mc_world_runtime *world, int64_t next_time, bool before,
                                     bool returned) {
    if (attack_autosave.failed)
        return;
    const int status = before ? attack_autosave_entry(world, next_time)
                              : attack_autosave_return(world, next_time, returned);
    if (status != 0)
        attack_autosave.failed = true;
}

static int attack_autosave_frame_before(fixture *f) {
    CHECK(!attack_autosave.failed);
    return attack_autosave.focused_frames ? repeat_focus_state(f, "attack-frame-entry") : 0;
}

static void attack_autosave_frame_after(fixture *f, bool returned) {
    if (attack_autosave.focused_frames && returned &&
        repeat_focus_state(f, "attack-frame-return") != 0)
        attack_autosave.failed = true;
}
