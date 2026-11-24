/* Actual source World/Chunk factory/save owners on authored disposable records.
 * The support program is renamed and never executed. No flags, constructors,
 * witnesses, holds, clocks or Random seeds are manufactured by this fixture. */
#include "../../../../src/entity/mob/nbt_internal.h"
#include "../../../../src/entity/mob/tick/internal.h"
#include "mc/entity/movement/effects.h"
#include "mc/entity/nbt/factory.h"
#include "mc/entity/player/player.h"
#include "mob_nbt_source_support.inc"
#include <inttypes.h>

static const char *const mn_names[] = {"Spider", "Zombie", "Skeleton", "Creeper", "Slime", "Pig",
                                       "Sheep",  "Cow",    "Chicken",  "Wolf",    "Squid"};
static const mc_entity_class_kind mn_kinds[] = {
    MC_ENTITY_CLASS_KIND_SPIDER,  MC_ENTITY_CLASS_KIND_ZOMBIE, MC_ENTITY_CLASS_KIND_SKELETON,
    MC_ENTITY_CLASS_KIND_CREEPER, MC_ENTITY_CLASS_KIND_SLIME,  MC_ENTITY_CLASS_KIND_PIG,
    MC_ENTITY_CLASS_KIND_SHEEP,   MC_ENTITY_CLASS_KIND_COW,    MC_ENTITY_CLASS_KIND_CHICKEN,
    MC_ENTITY_CLASS_KIND_WOLF,    MC_ENTITY_CLASS_KIND_SQUID};
static unsigned mn_sequence;

static uint32_t mn_float(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int mn_collect(source_save_fixture *s, mc_entity *out[11]) {
    memset(out, 0, 11 * sizeof(*out));
    mc_world_chunk *chunk = s->game.chunks[0];
    VERIFY(chunk->m != NULL && chunk->m->length == CHUNK_ENTITY_SLICES);
    size_t count = 0;
    for (size_t slice = 0; slice < CHUNK_ENTITY_SLICES; ++slice) {
        const mc_world_chunk_entity_list *list = chunk->m->items[slice];
        VERIFY(list != NULL);
        for (size_t j = 0; j < list->count; ++j) {
            mc_entity *entity = list->items[j];
            VERIFY(entity != NULL && entity->base.world == s->game.worlds[0] &&
                   entity->classes == s->game.entity_classes[0] && entity->storage_source != NULL &&
                   entity->base.bF && entity->base.bG == 0 && entity->base.bH == (int32_t)slice &&
                   entity->base.bI == 0 && entity->phase == MC_ENTITY_READY);
            size_t i = 0;
            while (i < 11 && entity->class_literal->kind != mn_kinds[i])
                ++i;
            VERIFY(i < 11 && out[i] == NULL);
            out[i] = entity; /* borrow the actual Chunk-owned occurrence */
            ++count;
        }
    }
    VERIFY(count == 11 && chunk->state->has_entities);
    return 0;
}

static int mn_construct(source_save_fixture *s, size_t i, mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    VERIFY(mc_entity_class_literal_get(s->game.entity_classes[0], mn_kinds[i], &literal) ==
               MC_ENTITY_OK &&
           mc_mob_allocate(s->game.entity_classes[0], s->game.registry[0], literal, out, &error) ==
               MC_MOB_OK &&
           mc_mob_construct(*out, s->game.worlds[0], &error) == MC_MOB_OK);
    return 0;
}

static int mn_emit(source_save_fixture *s, mc_entity *entity, size_t i, const char *scenario,
                   const char *operation) {
    mc_entity_nbt_error error = mc_entity_nbt_empty_error();
    mc_nbt_tree *tree = NULL;
    bool gate = false;
    if (strcmp(operation, "own") == 0)
        VERIFY(mc_mob_nbt_build_fields_held(entity, NULL, &tree, &error));
    else if (strcmp(operation, "direct") == 0)
        VERIFY(mc_entity_build_nbt(entity, NULL, &tree, &error) == MC_ENTITY_NBT_OK);
    else
        VERIFY(mc_entity_save_nbt(entity, NULL, &gate, &tree, &error) == MC_ENTITY_NBT_OK);
    const bool serialized = tree != NULL;
    size_t length = 0, written = 0;
    char file[160], path[8192];
    const unsigned sequence = mn_sequence++;
    int n = snprintf(file, sizeof(file), "%03u-%s-%s-%s.nbt", sequence, mn_names[i], scenario,
                     operation);
    VERIFY(n > 0 && (size_t)n < sizeof(file));
    if (serialized) {
        const mc_nbt_tag *root = mc_nbt_tree_root(tree);
        VERIFY(mc_nbt_measure(root, NULL, &length, NULL) == MC_NBT_OK && length <= 65536);
        uint8_t *bytes = malloc(length);
        VERIFY(bytes != NULL &&
               mc_nbt_write(root, NULL, bytes, length, &written, NULL) == MC_NBT_OK &&
               written == length);
        n = snprintf(path, sizeof(path), "%s/nbt/%s", s->root, file);
        VERIFY(n > 0 && (size_t)n < sizeof(path));
        FILE *output = ss_fopen(path, "wb");
        VERIFY(output != NULL && fwrite(bytes, 1, length, output) == length && fclose(output) == 0);
        free(bytes);
    }
    mc_mob_state state;
    VERIFY(mc_mob_inspect(entity, &state));
    printf("{\"event\":\"mob\",\"sequence\":%u,\"kind\":\"%s\",\"scenario\":\"%s\","
           "\"operation\":\"%s\",\"serialized\":%d,\"gate\":%d,\"file\":\"%s\",\"bytes\":%zu,"
           "\"health\":%d,\"hurt\":%d,\"death\":%d,\"attack\":%d,\"watch16\":%d,\"watch17\":%d,"
           "\"watch16type\":%d,\"watch17type\":%d,\"chickenEggTimer\":%d,\"removed\":%d,"
           "\"widthBits\":\"%08" PRIx32 "\",\"heightBits\":\"%08" PRIx32 "\","
           "\"entity\":\"%p\",\"world\":\"%p\",\"nonnullWorld\":%d,\"source\":%d}\n",
           sequence, mn_names[i], scenario, operation, serialized, gate, serialized ? file : "",
           length, state.living.Y, state.living.aa, state.living.ad, state.living.ae,
           (int)state.watcher_16, (int)state.watcher_17, state.watcher_16_type,
           state.watcher_17_type, state.chicken.i, entity->base.be, mn_float(entity->base.width),
           mn_float(entity->base.height), (void *)entity, (void *)entity->base.world,
           entity->base.world == s->game.worlds[0], entity->storage_source != NULL);
    VERIFY(fflush(stdout) == 0);
    mc_nbt_tree_free(tree);
    return 0;
}

static int mn_save(source_save_fixture *s, const char *label) {
    VERIFY(s->game.states[0]->has_entities); /* real imported q drives forced save */
    VERIFY(mc_platform_current_time_millis(&s->first) == MC_PLATFORM_MILLIS_OK);
    const mc_chunk_lifecycle_status result =
        mc_world_runtime_save_detailed(s->game.worlds[0], true, &s->lifecycle);
    s->save = s->lifecycle.save;
    VERIFY(mc_platform_current_time_millis(&s->last) == MC_PLATFORM_MILLIS_OK);
    if (result != MC_CHUNK_LIFECYCLE_OK)
        fprintf(stderr, "Mob actual World save failed status%d operation%s\n", (int)result,
                s->lifecycle.cause.operation == NULL ? "none" : s->lifecycle.cause.operation);
    VERIFY(result == MC_CHUNK_LIFECYCLE_OK && ss_event(s, label) == 0);
    return 0;
}

static int mn_extras(source_save_fixture *s) {
    mc_entity *entity = NULL;
    mc_entity_effect_error effect;
    VERIFY(mn_construct(s, 5, &entity) == 0 &&
           mc_entity_effect_dead(entity, &effect) == MC_ENTITY_OK && entity->base.be &&
           mn_emit(s, entity, 5, "removed", "storage") == 0 &&
           mn_emit(s, entity, 5, "removed", "direct") == 0);
    mc_entity_release(entity);
    entity = NULL;
    VERIFY(mn_construct(s, 4, &entity) == 0);
    const int32_t values[] = {-1, 127, 255};
    for (size_t i = 0; i < COUNT(values); ++i) {
        uint16_t names[2][6];
        mc_nbt_tag fields[2] = {{.type = MC_NBT_SHORT, .value.short_value = -123},
                                {.type = MC_NBT_INT, .value.int_value = values[i]}};
        fields[0].name = ascii("Health", names[0]);
        fields[1].name = ascii("Size", names[1]);
        const mc_nbt_tag *items[2] = {fields, fields + 1};
        const mc_nbt_tag input = {.type = MC_NBT_COMPOUND, .value.compound = {items, 2}};
        mc_entity_nbt_error error = mc_entity_nbt_empty_error();
        char label[40];
        const int n = snprintf(label, sizeof(label), "size%d", values[i]);
        VERIFY(n > 0 && (size_t)n < sizeof(label) &&
               mc_mob_nbt_read_held(entity, &input, NULL, &error) &&
               mn_emit(s, entity, 4, label, "own") == 0);
    }
    mc_entity_release(entity);
    entity = NULL;
    VERIFY(mn_construct(s, 5, &entity) == 0);
    uint16_t name[6];
    const mc_nbt_tag bad_health = {
        .type = MC_NBT_BYTE, .name = ascii("Health", name), .value.byte_value = 1};
    const mc_nbt_tag *bad_item = &bad_health;
    const mc_nbt_tag bad = {.type = MC_NBT_COMPOUND, .value.compound = {&bad_item, 1}};
    mc_entity_nbt_error error = mc_entity_nbt_empty_error();
    const int32_t before = entity->mob->living.Y;
    VERIFY(!mc_mob_nbt_read_held(entity, &bad, NULL, &error) &&
           error.status == MC_ENTITY_NBT_WRONG_TAG && error.late_hook == MC_ENTITY_NBT_LATE_MOB &&
           error.mob.living_bytecode == 4 && error.mob.source_exception &&
           entity->mob->living.Y == before);
    printf("{\"event\":\"wrong-tag\",\"health\":%d,\"living_pc\":%u,\"source_exception\":%d,"
           "\"exception\":\"%s\"}\n",
           before, error.mob.living_bytecode, error.mob.source_exception,
           error.mob.exception_class);
    mc_entity_release(entity);
    entity = NULL;
    VERIFY(mn_construct(s, 3, &entity) == 0);
    mc_mob_tick_error tick_error = {0};
    mc_mob_tick_scope scope = {.entity = entity, .error = &tick_error};
    VERIFY(mt_watch_byte_set(&scope, 17, 2) && mn_emit(s, entity, 3, "watcher2", "own") == 0);
    mc_entity_release(entity);
    return 0;
}

/* This native String ownership profile emits no Mob comparison rows. */
static bool mn_wolf_read(mc_entity *entity, const uint16_t *owner, size_t length, int16_t health,
                         bool angry, bool sitting, const mc_nbt_limits *limits,
                         mc_entity_nbt_error *error) {
    static const char *const keys[] = {"Health", "HurtTime", "DeathTime", "AttackTime",
                                       "Angry",  "Sitting",  "Owner"};
    uint16_t names[7][16];
    mc_nbt_tag fields[7] = {0};
    const mc_nbt_tag *items[7];
    for (size_t i = 0; i < COUNT(fields); ++i) {
        fields[i].name = ascii(keys[i], names[i]);
        items[i] = fields + i;
        if (i < 4) {
            fields[i].type = MC_NBT_SHORT;
            fields[i].value.short_value = (int16_t)(health + (int)i);
        }
    }
    fields[4].type = fields[5].type = MC_NBT_BYTE;
    fields[4].value.byte_value = (int8_t)(angry ? 1 : 0);
    fields[5].value.byte_value = (int8_t)(sitting ? 1 : 0);
    fields[6].type = MC_NBT_STRING;
    fields[6].value.string = (mc_nbt_string){owner, length};
    const mc_nbt_tag input = {.type = MC_NBT_COMPOUND, .value.compound = {items, COUNT(items)}};
    return mc_mob_nbt_read_held(entity, &input, limits, error);
}

static const mc_player_string *mn_wolf_owner(mc_entity *entity) {
    const mc_player_string *owner = NULL;
    mc_mob_tick_error error = {0};
    mc_mob_tick_scope scope = {.entity = entity, .error = &error};
    return mt_watch_string(&scope, 17, &owner) ? owner : NULL;
}

static bool mn_owner_equal(const mc_player_string *owner, const uint16_t *units, size_t length) {
    mc_player_string_view view = {0};
    return mc_player_string_read(owner, &view) && view.length == length &&
           memcmp(view.units, units, length * sizeof(*units)) == 0;
}

static void mn_owner_units(const mc_player_string *owner) {
    mc_player_string_view view = {0};
    (void)mc_player_string_read(owner, &view); /* Caller has already checked this SAME hold. */
    putchar('[');
    for (size_t i = 0; i < view.length; ++i)
        printf("%s%u", i == 0 ? "" : ",", (unsigned int)view.units[i]);
    putchar(']');
}

static int mn_wolf_ownership(source_save_fixture *s) {
    static const uint16_t old_units[] = {'O', 'l', 'd', 0, 937, 0xd83d, 0xdc3a, 0xd800, 0xdfff};
    static const uint16_t middle_units[] = {'M', 'i', 'd', 0, 937, 0xd83d, 0xdc3a, 0xd800, 0xdfff};
    static const uint16_t current_units[] = {'N', 'e', 'w', 0, 937, 0xd83d, 0xdc3a, 0xd800, 0xdfff};
    mc_entity *wolf = NULL;
    mc_entity_nbt_error error = mc_entity_nbt_empty_error();
    VERIFY(mn_construct(s, 9, &wolf) == 0 && mc_mob_nbt_owner_bytes(wolf) == 0 &&
           mn_wolf_owner(wolf) == mc_player_empty_literal());
    VERIFY(mn_wolf_read(wolf, old_units, COUNT(old_units), 17, false, true, NULL, &error));
    const mc_player_string *old = mn_wolf_owner(wolf);
    const size_t owner_bytes = mc_mob_nbt_owner_bytes(wolf);
    VERIFY(old != NULL && old != mc_player_empty_literal() &&
           mn_owner_equal(old, old_units, COUNT(old_units)) &&
           owner_bytes > COUNT(old_units) * sizeof(*old_units) && owner_bytes <= SIZE_MAX / 2 &&
           mc_player_string_retain((mc_player_string *)old) == MC_PLAYER_OK);
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = 0;
    error = mc_entity_nbt_empty_error();
    VERIFY(mn_wolf_read(wolf, old_units, COUNT(old_units), 17, false, true, &limits, &error) &&
           mn_wolf_owner(wolf) == old && mc_mob_nbt_owner_bytes(wolf) == owner_bytes &&
           mn_owner_equal(old, old_units, COUNT(old_units)));
    error = mc_entity_nbt_empty_error();
    VERIFY(mn_wolf_read(wolf, middle_units, COUNT(middle_units), 23, false, true, NULL, &error));
    const mc_player_string *middle = mn_wolf_owner(wolf);
    VERIFY(middle != NULL && middle != old && mc_mob_nbt_owner_bytes(wolf) == owner_bytes &&
           mn_owner_equal(middle, middle_units, COUNT(middle_units)) &&
           mn_owner_equal(old, old_units, COUNT(old_units)));
    mc_mob_state before, rejected;
    VERIFY(mc_mob_inspect(wolf, &before) && before.living.Y == 23 && before.watcher_16 == 5);
    /* Equal UTF16 extents give the same independently measured native owner
     * bytes. Both old and staged new owners must fit before publication. */
    const size_t peak_bytes = owner_bytes * 2;
    limits.max_allocation_bytes = peak_bytes - 1;
    error = mc_entity_nbt_empty_error();
    VERIFY(!mn_wolf_read(wolf, current_units, COUNT(current_units), -27, true, false, &limits,
                         &error) &&
           error.status == MC_ENTITY_NBT_LIMIT && error.late_hook == MC_ENTITY_NBT_LATE_MOB &&
           error.mob.native_policy && !error.mob.source_exception &&
           error.mob.exception_class == NULL && error.mob.concrete.pc == 41 &&
           error.mob.child.pc == 7 && error.operation != NULL &&
           strcmp(error.operation, "Wolf NBT overlapping native String owner bound") == 0 &&
           mn_wolf_owner(wolf) == middle && mc_mob_nbt_owner_bytes(wolf) == owner_bytes &&
           mn_owner_equal(middle, middle_units, COUNT(middle_units)) &&
           mn_owner_equal(old, old_units, COUNT(old_units)) && mc_mob_inspect(wolf, &rejected) &&
           rejected.living.Y == -27 && rejected.living.aa == -26 && rejected.living.ad == -25 &&
           rejected.living.ae == -24 && rejected.watcher_16 == 6);
    limits.max_allocation_bytes = peak_bytes;
    error = mc_entity_nbt_empty_error();
    const uintptr_t middle_identity = (uintptr_t)middle;
    VERIFY(
        mn_wolf_read(wolf, current_units, COUNT(current_units), -27, true, false, &limits, &error));
    const mc_player_string *current = mn_wolf_owner(wolf);
    VERIFY(current != NULL && (uintptr_t)current != middle_identity && current != old &&
           mc_mob_nbt_owner_bytes(wolf) == owner_bytes &&
           mn_owner_equal(current, current_units, COUNT(current_units)) &&
           mc_player_string_retain((mc_player_string *)current) == MC_PLAYER_OK);
    mc_entity_release(wolf); /* Actual Mob destructor releases its current String occurrence. */
    wolf = NULL;
    VERIFY(mn_owner_equal(old, old_units, COUNT(old_units)) &&
           mn_owner_equal(current, current_units, COUNT(current_units)));
    VERIFY(ss_stop(s) == 0 && ss_finish(s) == 0 && s->game.worlds[0] == NULL &&
           mn_owner_equal(old, old_units, COUNT(old_units)) &&
           mn_owner_equal(current, current_units, COUNT(current_units)));
    printf("{\"event\":\"wolf-owner-complete\",\"kind\":\"Wolf\",\"ownerBytes\":%zu,"
           "\"equalBudget\":0,\"rejectedBudget\":%zu,\"acceptedBudget\":%zu,"
           "\"rejectedNativePolicy\":1,\"sourceException\":0,\"concretePc\":41,\"childPc\":7,"
           "\"health\":%d,\"hurt\":%d,\"death\":%d,\"attack\":%d,\"watch16\":%d,"
           "\"identityReuse\":1,\"replacementDistinct\":1,\"rejectionRetainsIdentity\":1,"
           "\"postEntity\":1,\"postWorld\":1,\"oldUnits\":",
           owner_bytes, peak_bytes - 1, peak_bytes, rejected.living.Y, rejected.living.aa,
           rejected.living.ad, rejected.living.ae, (int)rejected.watcher_16);
    mn_owner_units(old);
    printf(",\"currentUnits\":");
    mn_owner_units(current);
    mc_player_string_release((mc_player_string *)old);
    mc_player_string_release((mc_player_string *)current);
    printf(",\"externalReleases\":2,\"actualWorldLoads\":1,\"actualWorldSaves\":0,"
           "\"cleanup\":1,\"checks\":%u}\n",
           checks);
    VERIFY(fflush(stdout) == 0);
    free(s);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 2 || (argc == 3 && strcmp(argv[2], "wolf-owner") == 0));
    source_save_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL && ss_setup(s, argv[1]) == 0 && ss_open(s) == 0);
    if (argc == 3)
        return mn_wolf_ownership(s);
    mc_entity *loaded[11];
    VERIFY(mn_collect(s, loaded) == 0 && ss_event(s, "loaded") == 0);
    for (size_t i = 0; i < 11; ++i) {
        mc_entity *fresh = NULL;
        VERIFY(mn_construct(s, i, &fresh) == 0 && mn_emit(s, fresh, i, "constructor", "own") == 0);
        mc_entity_release(fresh);
        VERIFY(mn_emit(s, loaded[i], i, "authored", "own") == 0 &&
               mn_emit(s, loaded[i], i, "authored", "direct") == 0 &&
               mn_emit(s, loaded[i], i, "authored", "storage") == 0);
    }
    VERIFY(mc_world_info_time_set(s->game.worlds[0]->fields.x, 20000) == MC_WORLD_INFO_OK &&
           mn_save(s, "authored-saved") == 0 && ss_stop(s) == 0 && ss_open(s) == 0 &&
           mn_collect(s, loaded) == 0 && ss_event(s, "authored-reloaded") == 0);
    const mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    for (size_t i = 0; i < 11; ++i) {
        mc_entity_nbt_error error = mc_entity_nbt_empty_error();
        VERIFY(mn_emit(s, loaded[i], i, "authored-reloaded", "own") == 0 &&
               mc_mob_nbt_read_held(loaded[i], &empty, NULL, &error) &&
               mn_emit(s, loaded[i], i, "missing", "own") == 0);
    }
    VERIFY(mc_world_info_time_set(s->game.worlds[0]->fields.x, 20001) == MC_WORLD_INFO_OK &&
           mn_save(s, "missing-saved") == 0 && ss_stop(s) == 0 && ss_open(s) == 0 &&
           mn_collect(s, loaded) == 0 && ss_event(s, "missing-reloaded") == 0);
    for (size_t i = 0; i < 11; ++i)
        VERIFY(mn_emit(s, loaded[i], i, "missing-reloaded", "own") == 0);
    VERIFY(mn_extras(s) == 0 && ss_stop(s) == 0 && ss_finish(s) == 0);
    printf("{\"event\":\"complete\",\"kinds\":11,\"writes\":%u,\"checks\":%u,\"cleanup\":1,"
           "\"actualWorldLoads\":3,\"actualWorldSaves\":2,\"originalOnlyExistingPowered\":1}\n",
           mn_sequence, checks);
    free(s);
    return 0;
}
