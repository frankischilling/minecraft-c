/* Authored complete hurt calls on genuine client Mob, World and Item owners. */
int mc_remaining_wolf_fixture_main(int, char **);
#define MC_WOLF_ATTACK_BODY_MAIN_NAME mc_remaining_wolf_fixture_main
#include "../test_wolf_attack.c"
#undef MC_WOLF_ATTACK_BODY_MAIN_NAME
#undef main

#include "mc/item/stack/stack.h"
#include "world/storage/generated_internal.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Remaining damage check%u line%d: %s\n", checks, __LINE__, #c);        \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct remaining_fixture {
    random_fixture base;
    mc_entity *actor, *source, *items[4];
    mc_world_access *observer;
    mc_entity_effect_error *error;
    size_t added, sounds;
    const char *name;
    float volume, pitch;
    bool bad_callback;
} remaining_fixture;

static size_t memberships(remaining_fixture *f, const mc_entity *item) {
    size_t count = 0;
    for (size_t n = 0; n < f->base.owned_count; ++n) {
        const mc_world_chunk *chunk = mc_world_store_chunk_object(f->base.store, f->base.owned[n]);
        for (size_t i = 0; i < chunk->m->length; ++i)
            for (size_t j = 0; j < chunk->m->items[i]->count; ++j)
                if (chunk->m->items[i]->items[j] == item)
                    ++count;
    }
    return count;
}

static bool member(remaining_fixture *f, const mc_entity *item) {
    for (size_t i = 0; i < f->base.world->fields.b->count; ++i)
        if (f->base.world->fields.b->items[i] == item)
            return true;
    return false;
}

static mc_world_storage_status remaining_sound(void *context, mc_world_runtime *world,
                                               const mc_registry_string *name, double x, double y,
                                               double z, float volume, float pitch,
                                               mc_world_storage_error *error) {
    remaining_fixture *f = context;
    mc_registry_string_view text = {0};
    if (world != f->base.world || x != f->actor->base.x ||
        y != f->actor->base.y - (double)f->actor->base.offset || z != f->actor->base.z ||
        volume != f->volume || f->sounds != 0 || f->added != 0 || f->actor->mob->living.ah ||
        !(pitch >= .8f && pitch <= 1.2f))
        f->bad_callback = true;
    if (f->name == NULL) {
        if (name != NULL)
            f->bad_callback = true;
    } else if (!mc_registry_string_read(name, &text) || text.length != strlen(f->name))
        f->bad_callback = true;
    else
        for (size_t i = 0; i < text.length; ++i)
            if (text.units[i] != (uint16_t)f->name[i])
                f->bad_callback = true;
    ++f->sounds;
    f->pitch = pitch;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status remaining_added(void *context, mc_world_runtime *world,
                                               mc_entity *item, mc_world_storage_error *error) {
    remaining_fixture *f = context;
    mc_item_stack_fields fields;
    if (world != f->base.world || f->added >= COUNT(f->items) || !f->actor->mob->living.ah ||
        !member(f, item) || memberships(f, item) != 1 || item->item.c != 10 || item->base.be ||
        mc_item_stack_fields_get(item->stack, &fields) != MC_ITEM_STACK_OK || fields.count != 1 ||
        fields.damage != 0)
        f->bad_callback = true;
    if (f->added < COUNT(f->items))
        f->items[f->added++] = item;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool prepare(remaining_fixture *f, const char *resources, const char *directory,
                    mc_entity_class_kind kind, int32_t size) {
    CHECK(setup(&f->base, resources, directory));
    for (int32_t x = -1; x <= 1; ++x)
        for (int32_t z = -1; z <= 1; ++z) {
            uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
            mc_world_chunk *object = NULL;
            CHECK(blocks != NULL && mc_world_store_create_generated(
                                        f->base.store, f->base.world, 0, x, z, &blocks,
                                        MC_CHUNK_BLOCKS, &object, NULL) == MC_WORLD_STORAGE_OK);
            mc_chunk_state *chunk = mc_world_chunk_state(object);
            CHECK(chunk != NULL && mc_chunk_cache_publish(f->base.provider.cache, x, z, chunk));
            f->base.owned[f->base.owned_count++] = chunk;
        }
    CHECK(wolf_actor(&f->base, kind, &f->actor) &&
          wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_COW, &f->source));
    if (kind == MC_ENTITY_CLASS_KIND_SLIME)
        CHECK(mc_mob_slime_set_size(f->actor, size, NULL) == MC_MOB_OK);
    CHECK(mc_entity_set_position(f->actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(f->source, 11.5, 64.5, 12.5) == MC_ENTITY_OK);
    mc_living_entity_fields *v = &f->actor->mob->living;
    v->Y = v->E = 20;
    v->Z = 17;
    v->au = f->actor->base.by = 0;
    v->aa = 3;
    v->ab = 4;
    v->ad = 7;
    v->av = 17;
    v->ah = false;
    f->actor->base.yaw = 30.f;
    f->actor->base.velocity_x = .25;
    f->actor->base.velocity_y = -.375;
    f->actor->base.velocity_z = -.125;
    mc_java_random_reseed(f->actor->random, 38);
    const mc_world_access_callbacks callbacks = {.sound = remaining_sound,
                                                 .entity_added = remaining_added};
    CHECK(mc_world_access_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->error = calloc(1, sizeof(*f->error));
    CHECK(f->error != NULL);
    return true;
}

static bool profile(remaining_fixture *f, const char *name, int32_t size, int32_t amount) {
    const mc_entity_class_kind kind = f->actor->class_literal->kind;
    f->name = kind == MC_ENTITY_CLASS_KIND_SLIME   ? "mob.slime"
              : kind == MC_ENTITY_CLASS_KIND_SQUID ? NULL
              : kind == MC_ENTITY_CLASS_KIND_GHAST
                  ? (amount == 20 ? "mob.ghast.death" : "mob.ghast.scream")
                  : (amount == 40 ? "mob.wolf.death" : "mob.wolf.hurt");
    f->volume = kind == MC_ENTITY_CLASS_KIND_SLIME   ? .6f
                : kind == MC_ENTITY_CLASS_KIND_GHAST ? 10.f
                                                     : .4f;
    bool result = false;
    const mc_entity_status status = mc_entity_effect_damage_with_math(
        f->actor, f->base.registry, f->base.math, f->source, amount, &result, f->error);
    if (status != MC_ENTITY_OK)
        fprintf(stderr, "Remaining damage failed: %s\n", f->error->operation);
    CHECK(status == MC_ENTITY_OK && result && f->sounds == 1 && !f->bad_callback &&
          f->error->living_damage.returned);
    mc_living_entity_fields *v = &f->actor->mob->living;
    CHECK(v->Y == (kind == MC_ENTITY_CLASS_KIND_WOLF ? 20 - (amount + 1) / 2 : 20 - amount));
    printf("REMAINING_DAMAGE {\"profile\":\"%s-size%" PRId32 "-damage%" PRId32
           "\",\"kind\":\"%s\",\"size\":%" PRId32 ",\"amount\":%" PRId32
           ",\"seed\":38,\"result\":true,\"health\":%" PRId32 ",\"previous_health\":%" PRId32
           ",\"resistance\":%" PRId32 ",\"last_damage\":%" PRId32 ",\"hurt\":%" PRId32
           ",\"max_hurt\":%" PRId32 ",\"angle\":\"%08" PRIx32 "\",\"velocity\":[\"%016" PRIx64
           "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"random\":\"%016" PRIx64
           "\",\"dying\":%s,\"dead\":%s,\"sounds\":[{\"name\":",
           name, size, amount, name, size, amount, v->Y, v->Z, f->actor->base.by, v->au, v->aa,
           v->ab, float_bits(v->ac), double_bits(f->actor->base.velocity_x),
           double_bits(f->actor->base.velocity_y), double_bits(f->actor->base.velocity_z),
           f->actor->random->state, v->ah ? "true" : "false", f->actor->base.be ? "true" : "false");
    if (f->name == NULL)
        printf("null");
    else
        printf("\"%s\"", f->name);
    printf(",\"volume\":\"%08" PRIx32 "\",\"pitch\":\"%08" PRIx32 "\"}],\"items\":[",
           float_bits(f->volume), float_bits(f->pitch));
    for (size_t i = 0; i < f->added; ++i) {
        mc_item_stack_fields fields;
        CHECK(mc_item_stack_fields_get(f->items[i]->stack, &fields) == MC_ITEM_STACK_OK);
        printf("%s[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%s,%zu]", i ? "," : "",
               fields.id, fields.count, fields.damage, f->items[i]->item.c,
               member(f, f->items[i]) ? "true" : "false", memberships(f, f->items[i]));
    }
    puts("]}");
    CHECK(fflush(stdout) == 0);
    return true;
}

static bool dispose(remaining_fixture *f) {
    CHECK(mc_entity_effect_cleanup(f->actor, NULL) == MC_ENTITY_OK);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_mob_retire(&f->actor, NULL) == MC_MOB_OK &&
          mc_mob_retire(&f->source, NULL) == MC_MOB_OK && teardown(&f->base));
    free(f->error);
    return true;
}

static bool squid_null_literal(const char *resources, const char *directory) {
    remaining_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL && prepare(f, resources, directory, MC_ENTITY_CLASS_KIND_SQUID, 0));
    f->volume = .4f;
    bool result = false;
    /* XT's literal-null getter never loads a Registry String. A missing
     * caller String namespace must not become an eager failure at LS332. */
    CHECK(mc_entity_effect_damage_with_math(f->actor, NULL, f->base.math, f->source, 4, &result,
                                            f->error) == MC_ENTITY_OK &&
          result && f->actor->mob->living.Y == 16 && f->sounds == 1 && !f->bad_callback &&
          f->error->living_damage.sound_returned &&
          f->error->living_damage.sound_name_identity == 0 &&
          f->error->living_damage.sound_literal == NULL &&
          f->error->living_damage.private_calls == 2);
    CHECK(dispose(f));
    free(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    const mc_entity_class_kind kinds[] = {MC_ENTITY_CLASS_KIND_SLIME, MC_ENTITY_CLASS_KIND_SLIME,
                                          MC_ENTITY_CLASS_KIND_SQUID, MC_ENTITY_CLASS_KIND_GHAST,
                                          MC_ENTITY_CLASS_KIND_WOLF};
    const char *names[] = {"uw", "uw", "xt", "bp", "gi"};
    const int32_t sizes[] = {1, 4, 0, 0, 0};
    for (size_t i = 0; i < COUNT(kinds); ++i)
        for (unsigned int fatal = 0; fatal < 2; ++fatal) {
            remaining_fixture *f = calloc(1, sizeof(*f));
            if (f == NULL || !prepare(f, argv[1], argv[2], kinds[i], sizes[i]) ||
                !profile(f, names[i], sizes[i], fatal ? (i == 4 ? 40 : 20) : 4) || !dispose(f))
                return 1;
            free(f);
        }
    if (!squid_null_literal(argv[1], argv[2]))
        return 1;
    printf("PASS: %u Remaining damage checks\n", checks);
    return 0;
}
