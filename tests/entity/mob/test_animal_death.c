/* Authored actual-owner fatal damage contracts. The included World/Wolf
 * construction helpers run normally; their earlier main is never executed. */
int mc_animal_death_wolf_fixture_main(int, char **);
#define MC_WOLF_ATTACK_BODY_MAIN_NAME mc_animal_death_wolf_fixture_main
#include "../test_wolf_attack.c"
#undef MC_WOLF_ATTACK_BODY_MAIN_NAME
#undef main

#include "entity/mob/references_internal.h"
#include "mc/item/stack/stack.h"
#include "mc/resources/translation/translation.h"
#include "mc/world/runtime/entity_tick.h"
#include "world/runtime/terminal_inventory_internal.h"
#include "world/storage/generated_internal.h"

#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Animal death check%u line%d: %s\n", checks, __LINE__, #c);            \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct death_fixture {
    random_fixture base;
    mc_entity *wolf, *victim, *item;
    mc_world_access *observer;
    mc_mob_tick_error *attack, *cleanup;
    size_t sounds, added, source_holds, target_holds;
    bool bad_callback;
    int32_t sound_health;
    bool sound_dying, added_dying;
    bool matrix, source_null, source_published;
    int mode, kind, path;
    mc_entity *items[8];
    size_t item_count;

    struct {
        char name[48];
        double position[3];
        float volume, pitch;
    } sound_rows[4];

    struct {
        bool add;
        size_t index;
        bool world_member;
        size_t chunk_members;
    } events[16];

    size_t event_count;
    mc_entity_effect_error *effect, *busy_error;
    uint64_t constructor_actor, constructor_source, constructor_math;
} death_fixture;

static mc_world_storage_status matrix_sound(death_fixture *, mc_world_runtime *,
                                            const mc_registry_string *, double, double, double,
                                            float, float, mc_world_storage_error *);
static mc_world_storage_status matrix_added(death_fixture *, mc_world_runtime *, mc_entity *,
                                            mc_world_storage_error *);

static bool death_callback_guard(death_fixture *f, mc_world_runtime *world) {
    mc_entity *alias = f->victim;
    mc_mob_error error;
    return world == f->base.world && f->victim->holds > f->target_holds &&
           f->wolf->holds > f->source_holds && f->wolf->mob->tick_active &&
           mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_BUSY &&
           mc_mob_retire(&alias, &error) == MC_MOB_BUSY && alias == f->victim;
}

static mc_world_storage_status death_sound(void *context, mc_world_runtime *world,
                                           const mc_registry_string *name, double x, double y,
                                           double z, float volume, float pitch,
                                           mc_world_storage_error *error) {
    death_fixture *f = context;
    if (f->matrix)
        return matrix_sound(f, world, name, x, y, z, volume, pitch, error);
    static const char expected[] = "mob.sheep";
    mc_registry_string_view text = {0};
    if (!death_callback_guard(f, world) || !mc_registry_string_read(name, &text) ||
        text.length != sizeof(expected) - 1 || volume != 1.f || x != f->victim->base.x ||
        y != f->victim->base.y - (double)f->victim->base.offset || z != f->victim->base.z ||
        !(pitch >= .8f && pitch <= 1.2f) || f->added != 0)
        f->bad_callback = true;
    if (!f->bad_callback)
        for (size_t i = 0; i < text.length; ++i)
            if (text.units[i] != (uint16_t)expected[i])
                f->bad_callback = true;
    ++f->sounds;
    f->sound_health = f->victim->mob->living.Y;
    f->sound_dying = f->victim->mob->living.ah;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status death_added(void *context, mc_world_runtime *world, mc_entity *item,
                                           mc_world_storage_error *error) {
    death_fixture *f = context;
    if (f->matrix)
        return matrix_added(f, world, item, error);
    mc_item_stack_fields stack;
    if (!death_callback_guard(f, world) || f->sounds != 1 || item == NULL ||
        !mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) || item->phase != MC_ENTITY_READY ||
        item->base.world != world || !item->base.bF || item->item.c != 10 ||
        mc_item_stack_fields_get(item->stack, &stack) != MC_ITEM_STACK_OK || stack.id != 35 ||
        stack.count != 1 || stack.damage != 7 || world->fields.b->count != 1 ||
        world->fields.b->items[0] != item || !f->victim->mob->living.ah || f->victim->base.be)
        f->bad_callback = true;
    ++f->added;
    f->item = item; /* Borrowed actual World/pending owner, never a constructed answer. */
    f->added_dying = f->victim->mob->living.ah;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool death_prepare_kind(death_fixture *f, const char *resources, const char *directory,
                               mc_entity_class_kind kind) {
    CHECK(setup(&f->base, resources, directory));
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *object = NULL;
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(f->base.store, f->base.world, 0, 0, 0, &blocks,
                                          MC_CHUNK_BLOCKS, &object, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *chunk = mc_world_chunk_state(object);
    CHECK(chunk != NULL && mc_chunk_cache_publish(f->base.provider.cache, 0, 0, chunk));
    f->base.owned[f->base.owned_count++] = chunk;
    CHECK(wolf_actor(&f->base, kind, &f->victim) &&
          wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_WOLF, &f->wolf));
    const mc_world_access_callbacks callbacks = {.sound = death_sound, .entity_added = death_added};
    CHECK(mc_world_access_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->attack = calloc(1, sizeof(*f->attack));
    f->cleanup = calloc(1, sizeof(*f->cleanup));
    f->effect = calloc(1, sizeof(*f->effect));
    f->busy_error = calloc(1, sizeof(*f->busy_error));
    CHECK(f->attack != NULL && f->cleanup != NULL && f->effect != NULL && f->busy_error != NULL);
    f->source_holds = f->wolf->holds;
    f->target_holds = f->victim->holds;
    f->constructor_actor = f->victim->random->state;
    f->constructor_source = f->wolf->random->state;
    f->constructor_math = atomic_load(&f->base.profile->math->state);
    return true;
}

static bool death_prepare(death_fixture *f, const char *resources, const char *directory) {
    return death_prepare_kind(f, resources, directory, MC_ENTITY_CLASS_KIND_SHEEP);
}

/* The first check was written while fatal LS damage still stopped at316.
 * Actual GI195 must now finish the same death body and publish its Wool Item. */
static bool death_wolf_red(death_fixture *f) {
    CHECK(wolf_attack_reset(&f->base, f->wolf, f->victim, 38, true, 1, 0, 0) &&
          mt_wolf_tame_component(f->wolf, f->base.registry, f->base.math, NULL, false, f->attack) ==
              MC_ENTITY_OK);
    mc_entity_watch_record *wool = wolf_attack_flags(f->victim);
    CHECK(wool != NULL && wool->type == 0);
    wool->byte_value = 7; /* Authored unsheared color7 input. */
    f->victim->mob->living.Y = 2;
    mc_java_random_reseed(f->victim->random, 38);
    const mc_entity_status status = mt_wolf_attack_component(
        f->wolf, f->base.registry, f->base.math, NULL, f->victim, 1.f, f->attack);
    if (status != MC_ENTITY_OK) {
        const mc_entity_effect_error *effect = &f->attack->detail.effect;
        fprintf(stderr,
                "Animal death actual GI%u child=%s.%s:%d status=%d LS%u operation=%s "
                "health=%" PRId32 " dying=%d items=%zu\n",
                f->attack->site.pc,
                f->attack->child_site.class_name == NULL ? "none"
                                                         : f->attack->child_site.class_name,
                f->attack->child_site.method == NULL ? "none" : f->attack->child_site.method,
                f->attack->child_site.pc, (int)status, effect->living_damage.living_pc,
                effect->operation == NULL ? "none" : effect->operation, f->victim->mob->living.Y,
                f->victim->mob->living.ah ? 1 : 0, f->base.world->fields.b->count);
    }
    CHECK(status == MC_ENTITY_OK && wolf_returned(f->attack) && f->wolf->mob->living.ae == 20 &&
          f->victim->mob->living.Y == 0 && f->victim->mob->living.ah && !f->victim->base.be &&
          f->sounds == 1 && f->sound_health == 0 && !f->sound_dying && f->added == 1 &&
          f->added_dying && f->item != NULL && !f->bad_callback &&
          f->wolf->holds == f->source_holds && f->victim->holds == f->target_holds &&
          f->base.world->fields.b->count == 1);
    mc_item_stack_fields stack;
    CHECK(mc_item_stack_fields_get(f->item->stack, &stack) == MC_ITEM_STACK_OK && stack.id == 35 &&
          stack.count == 1 && stack.damage == 7 && f->item->item.c == 10 && f->item->base.bF &&
          f->item->base.world == f->base.world && !f->item->base.be);
    const mc_world_chunk *chunk = mc_world_store_chunk_object(f->base.store, f->base.owned[0]);
    CHECK(chunk != NULL && chunk->m != NULL && chunk->m->length == 8 &&
          chunk->m->items[4]->count == 1 && chunk->m->items[4]->items[0] == f->item);
    puts("ANIMAL_DEATH real-wolf-fatal=1 health=0 dying=1 immediate-dead=0 wool=35 count=1 color=7 "
         "published=1");
    return true;
}

static bool death_dispose(death_fixture *f) {
    bool removed = false;
    CHECK(mc_entity_effect_cleanup(f->victim, NULL) == MC_ENTITY_OK &&
          !mc_entity_effect_has_pending(f->victim) &&
          mc_mob_tick_cleanup(f->wolf, f->cleanup) == MC_ENTITY_OK &&
          mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
    if (f->source_published)
        CHECK(mc_world_runtime_retire_entity_containers(f->base.world, NULL) ==
              MC_WORLD_STORAGE_OK);
    mc_mob_error error;
    CHECK(mc_mob_retire(&f->wolf, &error) == MC_MOB_OK &&
          mc_mob_retire(&f->victim, &error) == MC_MOB_OK && teardown(&f->base));
    free(f->cleanup);
    free(f->attack);
    free(f->effect);
    free(f->busy_error);
    return true;
}

static size_t death_members(const death_fixture *f, const mc_entity *item) {
    const mc_world_chunk *chunk = mc_world_store_chunk_object(f->base.store, f->base.owned[0]);
    size_t count = 0;
    for (size_t i = 0; i < chunk->m->length; ++i)
        for (size_t j = 0; j < chunk->m->items[i]->count; ++j)
            if (chunk->m->items[i]->items[j] == item)
                ++count;
    return count;
}

static bool death_world_member(const death_fixture *f, const mc_entity *item) {
    for (size_t i = 0; i < f->base.world->fields.b->count; ++i)
        if (f->base.world->fields.b->items[i] == item)
            return true;
    return false;
}

static bool matrix_guard(death_fixture *f, mc_world_runtime *world, bool pending) {
    mc_entity *alias = f->victim;
    mc_mob_error error;
    if (world != f->base.world || f->victim->holds <= f->target_holds ||
        (!f->source_null && f->wolf->holds <= f->source_holds) ||
        (f->path == 2 && !f->wolf->mob->tick_active) ||
        mc_world_access_free(f->observer, NULL) != MC_WORLD_STORAGE_BUSY ||
        mc_mob_retire(&alias, &error) != MC_MOB_BUSY || alias != f->victim)
        return false;
    if (pending && (!mc_entity_effect_has_pending(f->victim) ||
                    mc_entity_effect_cleanup(f->victim, f->busy_error) != MC_ENTITY_BUSY ||
                    !mc_entity_effect_has_pending(f->victim)))
        return false;
    return true;
}

static mc_world_storage_status matrix_sound(death_fixture *f, mc_world_runtime *world,
                                            const mc_registry_string *name, double x, double y,
                                            double z, float volume, float pitch,
                                            mc_world_storage_error *error) {
    static const char *const names[] = {"mob.sheep", "mob.pigdeath", "mob.cowhurt",
                                        "mob.chickenhurt"};
    mc_registry_string_view text = {0};
    const char *expected = f->kind < 4 ? names[f->kind] : "";
    if (!matrix_guard(f, world, false) || !mc_registry_string_read(name, &text) ||
        text.length != strlen(expected) || text.length >= sizeof(f->sound_rows[0].name) ||
        volume != (f->kind == 2 ? .4f : 1.f) || x != f->victim->base.x ||
        y != f->victim->base.y - (double)f->victim->base.offset || z != f->victim->base.z ||
        f->sounds >= COUNT(f->sound_rows) || f->event_count >= COUNT(f->events))
        f->bad_callback = true;
    if (f->bad_callback) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored animal death sound contract", 0};
        return error->status;
    }
    const size_t row = f->sounds++;
    for (size_t i = 0; i < text.length; ++i) {
        if (text.units[i] != (uint16_t)expected[i])
            f->bad_callback = true;
        f->sound_rows[row].name[i] = (char)text.units[i];
    }
    f->sound_rows[row].name[text.length] = '\0';
    f->sound_rows[row].position[0] = x;
    f->sound_rows[row].position[1] = y;
    f->sound_rows[row].position[2] = z;
    f->sound_rows[row].volume = volume;
    f->sound_rows[row].pitch = pitch;
    f->events[f->event_count].add = false;
    f->events[f->event_count++].index = row;
    if (f->mode == 4)
        f->victim->base.world = NULL; /* Actual observer mutation, after captured sound operands. */
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status matrix_added(death_fixture *f, mc_world_runtime *world,
                                            mc_entity *item, mc_world_storage_error *error) {
    if (!matrix_guard(f, world, true) || item == NULL ||
        !mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) || item->phase != MC_ENTITY_READY ||
        item->base.world != world || !item->base.bF || item->item.c != 10 ||
        !f->victim->mob->living.ah || f->victim->base.be || !death_world_member(f, item) ||
        death_members(f, item) != 1 || f->item_count >= COUNT(f->items) ||
        f->event_count >= COUNT(f->events))
        f->bad_callback = true;
    if (f->bad_callback) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored animal death Item contract", 0};
        return error->status;
    }
    for (size_t i = 0; i < f->item_count; ++i)
        if (f->items[i] == item)
            f->bad_callback = true;
    const size_t row = f->item_count++;
    f->items[row] = item;
    f->events[f->event_count].add = true;
    f->events[f->event_count].index = row;
    f->events[f->event_count].world_member = true;
    f->events[f->event_count++].chunk_members = 1;
    ++f->added;
    if (f->mode == 3 && f->item_count == 1)
        f->victim->base.world = NULL;
    if ((f->mode == 1 && f->item_count == 1) || (f->mode == 2 && f->item_count == 2)) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored animal death post-publication refusal", 0};
        return error->status;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static const int death_tail[][14] = {
    {0, 0, 7, 1, 0, 38, 0, 0, 0, 1, 0, 0, 0, 0},  {1, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0},   {1, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0},
    {1, 0, 0, 0, 1, 2, 0, 0, 0, 1, 0, 0, 0, 0},   {1, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0},
    {2, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 0},   {2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
    {2, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0},   {3, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 0},
    {3, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0},   {3, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0},
    {4, 1, 0, 0, 0, 38, 0, 0, 0, 1, 0, 0, 0, 0},  {1, 1, 0, 0, 0, 3, 1, 0, 0, 1, 0, 0, 0, 0},
    {0, 1, 0, 0, 0, 38, 2, 0, 0, 1, 0, 0, 0, 0},  {0, 0, 9, 0, 0, 38, 0, 1, 0, 1, 0, 0, 0, 0},
    {0, 1, 3, 0, 0, 38, 0, 0, 0, 2, 0, 0, 0, 0},  {0, 0, 7, 0, 0, 38, 0, 0, 1, 1, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 2, 0, 0, 2, 1, 0, 0, 0, 0},   {0, 0, 5, 0, 0, 38, 0, 0, 3, 1, 0, 0, 0, 0},
    {0, 1, 6, 0, 0, 38, 0, 0, 0, 1, -1, 0, 0, 0}, {1, 1, 0, 0, 0, 2, 0, 1, 0, 1, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 38, 0, 0, 0, 1, 0, 1, 0, 0},  {0, 1, 0, 0, 0, 38, 0, 0, 0, 1, 0, 0, 1, 0},
    {0, 2, 7, 0, 0, 38, 0, 0, 0, 1, 0, 0, 0, 0},  {0, 0, 8, 0, 0, 38, 0, 0, 4, 1, 0, 0, 0, 0}};

static void death_profile(size_t index, int n[14]) {
    if (index < 16) {
        const int first[14] = {0, 0, (int)index, 0, 0, 38, 0, 0, 0, 1, 0, 0, 0, 0};
        memcpy(n, first, sizeof(first));
    } else
        memcpy(n, death_tail[index - 16], sizeof(death_tail[0]));
}

static bool matrix_prepare(death_fixture *f, const int n[14]) {
    f->matrix = true;
    f->kind = n[0];
    f->path = n[1];
    f->mode = n[8];
    f->source_null = n[7] != 0;
    CHECK(mc_entity_set_position(f->victim, 8.5, 64.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(f->wolf, 11.5, 64.5, 12.5) == MC_ENTITY_OK &&
          mt_wolf_tame_component(f->wolf, f->base.registry, f->base.math, NULL, false, f->attack) ==
              MC_ENTITY_OK);
    mc_living_entity_fields *v = &f->victim->mob->living;
    v->Y = n[1] == 2 ? 2 : 4;
    v->Z = 17;
    v->E = 20;
    f->victim->base.by = 0;
    v->au = 0;
    v->av = 17;
    v->aa = 3;
    v->ab = 4;
    v->ad = 7;
    v->T = n[10];
    v->ac = 99;
    v->al = .25f;
    v->ah = false;
    f->victim->base.bb = f->victim->base.be = false;
    f->victim->base.yaw = 30;
    f->victim->base.velocity_x = .25;
    f->victim->base.velocity_y = -.375;
    f->victim->base.velocity_z = -.125;
    f->victim->base.offset = .125f;
    f->victim->base.bv = n[4];
    f->wolf->base.yaw = 15;
    f->wolf->base.velocity_x = -.5;
    f->wolf->base.velocity_y = .75;
    f->wolf->base.velocity_z = 1.25;
    f->wolf->mob->living.ae = 123;
    f->wolf->base.aX = true;
    f->wolf->base.bb = f->wolf->base.be = false;
    wolf_reference_set_box(f->victim->bounds, 8, 64, 8, 9, 65, 9);
    wolf_reference_set_box(f->wolf->bounds, 11, 64, 12, 12, 65, 13);
    if (n[0] == 0) {
        mc_entity_watch_record *wool = wolf_attack_flags(f->victim);
        CHECK(wool != NULL && wool->type == 0);
        wool->byte_value = (int8_t)((n[2] & 15) | (n[3] ? 16 : 0));
    }
    mc_java_random_reseed(f->victim->random, n[5]);
    mc_java_random_reseed(f->wolf->random, 12345);
    mc_java_random_reseed(f->base.world->fields.r, 2468);
    f->base.world->fields.B = n[6] == 1;
    if (n[6] == 2)
        f->victim->base.world = NULL;
    return true;
}

static bool death_directory(char *out, size_t capacity, const char *root, const char *prefix,
                            size_t i) {
    const int length = snprintf(out, capacity, "%s/%s%zu", root, prefix, i);
    CHECK(length > 0 && (size_t)length < capacity);
#ifdef _WIN32
    CHECK(_mkdir(out) == 0);
#else
    CHECK(mkdir(out, 0700) == 0);
#endif
    return true;
}

static const char *death_bool(bool value) {
    return value ? "true" : "false";
}

static void death_item_json(FILE *out, const death_fixture *f, const mc_entity *p) {
    mc_item_stack_fields s;
    if (mc_item_stack_fields_get(p->stack, &s) != MC_ITEM_STACK_OK)
        exit(3);
    fprintf(out, "{\"stack\":[%d,%d,%d,%d],\"position\":", s.id, s.count, s.damage, s.animation);
    const double pos[] = {p->base.x, p->base.y, p->base.z};
    const double previous[] = {p->base.aJ, p->base.aK, p->base.aL};
    const double velocity[] = {p->base.velocity_x, p->base.velocity_y, p->base.velocity_z};
    wolf_reference_vector(out, pos, 3);
    fputs(",\"previous_position\":", out);
    wolf_reference_vector(out, previous, 3);
    fputs(",\"velocity\":", out);
    wolf_reference_vector(out, velocity, 3);
    fprintf(out,
            ",\"rotation\":[\"%08" PRIx32 "\",\"%08" PRIx32 "\",\"%08" PRIx32 "\",\"%08" PRIx32
            "\",\"%08" PRIx32 "\"],\"size\":[\"%08" PRIx32 "\",\"%08" PRIx32 "\",\"%08" PRIx32
            "\"],\"bounds\":",
            float_bits(p->base.yaw), float_bits(p->base.aT), float_bits(p->base.aU),
            float_bits(p->base.aV), float_bits(p->item.d), float_bits(p->base.offset),
            float_bits(p->base.width), float_bits(p->base.height));
    wolf_reference_box(out, p->bounds);
    fprintf(out,
            ",\"body\":[%d,%d,%d,%d,%d,%d,%d],\"flags\":[%s,%s,%s,%s,%s,%s,%s,%s,%s,%s],"
            "\"chunk\":[%d,%d,%d],\"private_random\":\"%016" PRIx64 "\",\"world\":\"%s\","
            "\"world_member\":%s,\"chunk_members\":%zu,\"stack_unique\":",
            p->item.b, p->item.c, p->item.e, p->item.f, p->base.bt, p->base.bv, p->base.by,
            death_bool(p->base.aX), death_bool(p->base.aY), death_bool(p->base.aZ),
            death_bool(p->base.ba), death_bool(p->base.bb), death_bool(p->base.bc),
            death_bool(p->base.bd), death_bool(p->base.be), death_bool(p->base.bq),
            death_bool(p->base.bF), p->base.bG, p->base.bH, p->base.bI, p->random->state,
            p->base.world == f->base.world ? "PRIMARY" : "FOREIGN",
            death_bool(death_world_member(f, p)), death_members(f, p));
    bool unique = true;
    for (size_t i = 0; i < f->item_count; ++i)
        if (f->items[i] != p && f->items[i]->stack == p->stack)
            unique = false;
    fprintf(out, "%s}", death_bool(unique));
}

static void death_state_json(FILE *out, const death_fixture *f) {
    const mc_entity *p = f->victim;
    const mc_living_entity_fields *v = &p->mob->living;
    fprintf(out,
            "{\"health\":[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d],\"animation\":[\"%08" PRIx32
            "\",\"%08" PRIx32 "\"],\"velocity\":",
            v->Y, v->Z, v->E, p->base.by, v->au, v->av, v->aa, v->ab, v->ad, v->T,
            float_bits(v->ac), float_bits(v->al));
    const double vel[] = {p->base.velocity_x, p->base.velocity_y, p->base.velocity_z};
    const double pos[] = {p->base.x, p->base.y, p->base.z};
    wolf_reference_vector(out, vel, 3);
    fprintf(out, ",\"attacked\":%s,\"dying\":%s,\"dead\":%s,\"position\":", death_bool(p->base.bb),
            death_bool(v->ah), death_bool(p->base.be));
    wolf_reference_vector(out, pos, 3);
    fprintf(out, ",\"yaw\":\"%08" PRIx32 "\",\"bounds\":", float_bits(p->base.yaw));
    wolf_reference_box(out, p->bounds);
    fputs(",\"random\":", out);
    if (p->random == NULL)
        fputs("null", out);
    else
        fprintf(out, "\"%016" PRIx64 "\"", p->random->state);
    fprintf(out,
            ",\"math_random\":\"%016" PRIx64 "\",\"world_random\":\"%016" PRIx64
            "\",\"source_random\":\"%016" PRIx64
            "\",\"source_cooldown\":%d,\"source_unchanged\":true,\"world\":",
            atomic_load(&f->base.profile->math->state), f->base.world->fields.r->state,
            f->wolf->random->state, f->wolf->mob->living.ae);
    if (p->base.world == NULL)
        fputs("null", out);
    else
        fprintf(out, "\"%s\"", p->base.world == f->base.world ? "PRIMARY" : "FOREIGN");
    fputs(",\"watcher\":", out);
    if (f->kind != 0 || p->watcher == NULL)
        fputs("null", out);
    else {
        const mc_entity_watch_record *record = wolf_attack_flags(f->victim);
        fprintf(out, "%d", (int)record->byte_value);
    }
    fputs(",\"sounds\":[", out);
    for (size_t i = 0; i < f->sounds; ++i) {
        fprintf(out, "%s{\"name\":\"%s\",\"position\":", i ? "," : "", f->sound_rows[i].name);
        wolf_reference_vector(out, f->sound_rows[i].position, 3);
        fprintf(out, ",\"volume\":\"%08" PRIx32 "\",\"pitch\":\"%08" PRIx32 "\"}",
                float_bits(f->sound_rows[i].volume), float_bits(f->sound_rows[i].pitch));
    }
    fputs("],\"items\":[", out);
    for (size_t i = 0; i < f->item_count; ++i) {
        if (i)
            fputc(',', out);
        death_item_json(out, f, f->items[i]);
    }
    const mc_world_chunk *chunk = mc_world_store_chunk_object(f->base.store, f->base.owned[0]);
    fprintf(out, "],\"world_loaded\":%zu,\"chunk_count\":%zu,\"events\":[",
            f->base.world->fields.b->count, chunk->m->items[4]->count);
    for (size_t i = 0; i < f->event_count; ++i) {
        fprintf(out, "%s{\"kind\":\"%s\",\"index\":%zu", i ? "," : "",
                f->events[i].add ? "ADD" : "SOUND", f->events[i].index);
        if (f->events[i].add)
            fprintf(out, ",\"world_member\":%s,\"chunk_members\":%zu",
                    death_bool(f->events[i].world_member), f->events[i].chunk_members);
        fputc('}', out);
    }
    fputs("]}", out);
}

static uint32_t death_next(uint64_t *state, unsigned int bits) {
    *state = (*state * UINT64_C(0x5deece66d) + 11u) & UINT64_C(0xffffffffffff);
    return (uint32_t)(*state >> (48u - bits));
}

static double death_double(uint64_t *state) {
    const uint64_t high = death_next(state, 26), low = death_next(state, 27);
    return (double)((high << 27u) | low) / 9007199254740992.0;
}

static int32_t death_quantity(uint64_t *state) {
    for (;;) {
        const uint32_t bits = death_next(state, 31), value = bits % 3u;
        if (bits - value + 2u <= INT32_MAX)
            return (int32_t)value;
    }
}

static bool death_entropy(death_fixture *f, uint64_t before, uint64_t unique, size_t first_item,
                          bool null_direction) {
    uint64_t state = before;
    if (null_direction)
        (void)death_double(&state);
    for (size_t i = first_item; i < f->item_count; ++i) {
        const double d0 = death_double(&state), d1 = death_double(&state),
                     d2 = death_double(&state), d3 = death_double(&state);
        volatile double phase0 = d0 * 3.141592653589793, phase1 = phase0 * 2;
        volatile float phase = (float)phase1, yaw = (float)(d1 * 360);
        volatile double x0 = d2 * 0.20000000298023224, z0 = d3 * 0.20000000298023224;
        volatile float x = (float)(x0 - 0.10000000149011612), z = (float)(z0 - 0.10000000149011612);
        const mc_entity *item = f->items[i];
        CHECK(float_bits(item->item.d) == float_bits(phase) &&
              float_bits(item->base.yaw) == float_bits(yaw) &&
              double_bits(item->base.velocity_x) == double_bits((double)x) &&
              double_bits(item->base.velocity_y) == double_bits(0.20000000298023224) &&
              double_bits(item->base.velocity_z) == double_bits((double)z) && item->random != NULL);
        unique *= UINT64_C(1181783497276652981);
    }
    CHECK(atomic_load(&f->base.profile->math->state) == state &&
          atomic_load(&f->base.profile->uniquifier) == unique);
    return true;
}

static bool death_matrix(const char *resources, const char *root, FILE *rows) {
    static const mc_entity_class_kind kinds[] = {
        MC_ENTITY_CLASS_KIND_SHEEP, MC_ENTITY_CLASS_KIND_PIG, MC_ENTITY_CLASS_KIND_COW,
        MC_ENTITY_CLASS_KIND_CHICKEN, MC_ENTITY_CLASS_KIND_WOLF};
    size_t row = 0;
    for (size_t profile = 0; profile < 42; ++profile) {
        int n[14];
        death_profile(profile, n);
        char directory[8192];
        CHECK(death_directory(directory, sizeof(directory), root, "case", profile));
        death_fixture *f = calloc(1, sizeof(*f));
        CHECK(f != NULL && death_prepare_kind(f, resources, directory, kinds[n[0]]) &&
              matrix_prepare(f, n));
        mc_java_random *random = f->victim->random;
        mc_entity_watcher *watcher = f->victim->watcher;
        if (n[11])
            f->victim->random = NULL;
        if (n[12])
            f->victim->watcher = NULL;
        const mc_entity_base_fields source = f->wolf->base;
        const mc_entity_bounds source_box = *f->wolf->bounds;
        mc_entity_bounds *const source_box_owner = f->wolf->bounds;
        mc_java_random *const source_random_owner = f->wolf->random;
        const uint64_t source_random = f->wolf->random->state;
        const int32_t source_health = f->wolf->mob->living.Y;
        for (int step = 0; step < n[9]; ++step) {
            const uint64_t math = atomic_load(&f->base.profile->math->state);
            const uint64_t unique = atomic_load(&f->base.profile->uniquifier);
            const uint64_t private_state = f->victim->random ? f->victim->random->state : 0;
            const size_t item_before = f->item_count, sound_before = f->sounds;
            bool answer = false;
            const mc_entity_status status =
                n[1] == 1
                    ? mc_entity_effect_death_in_context(f->victim, f->base.registry,
                                                        f->source_null ? NULL : f->wolf, f->effect)
                : n[1] == 2
                    ? mt_wolf_attack_component(f->wolf, f->base.registry, f->base.math, NULL,
                                               f->victim, 1.f, f->attack)
                    : mc_entity_effect_damage_with_math(f->victim, f->base.registry, f->base.math,
                                                        f->source_null ? NULL : f->wolf, 4, &answer,
                                                        f->effect);
            mc_entity_effect_error *error = n[1] == 2 ? &f->attack->detail.effect : f->effect;
            const char *exception = "NONE", *scope = "FULL";
            if (status != MC_ENTITY_OK) {
                if (n[8] == 1 || n[8] == 2) {
                    CHECK(status == MC_ENTITY_WORLD_FAILED && error->death.pending &&
                          error->death.native_policy && !error->death.source_exception);
                    exception = "IO";
                    scope = "PUBLICATION_CALLBACK";
                } else {
                    CHECK(error->death.pending && error->death.source_exception &&
                          !error->death.native_policy &&
                          strcmp(error->death.exception_class, "java/lang/NullPointerException") ==
                              0);
                    exception = "NPE";
                    scope = "SOURCE_FAILURE";
                }
                CHECK(mc_entity_effect_has_pending(f->victim));
                mc_entity *alias = f->victim;
                mc_mob_error busy;
                CHECK(mc_mob_retire(&alias, &busy) == MC_MOB_BUSY && alias == f->victim);
            } else {
                CHECK(!mc_entity_effect_has_pending(f->victim));
                if (n[1] != 2)
                    CHECK(error->death.returned && error->death.death_pc == 57 &&
                          !error->death.pending && error->death.receiver == NULL &&
                          error->death.drop_error == NULL);
            }
            CHECK(!f->bad_callback && f->victim->mob->living.ad == 7 && !f->victim->base.be &&
                  f->base.world->fields.b->count == f->item_count && f->wolf->base.x == source.x &&
                  f->wolf->base.y == source.y && f->wolf->base.z == source.z &&
                  f->wolf->base.velocity_x == source.velocity_x &&
                  f->wolf->base.velocity_y == source.velocity_y &&
                  f->wolf->base.velocity_z == source.velocity_z &&
                  f->wolf->base.yaw == source.yaw && f->wolf->base.aX == source.aX &&
                  f->wolf->base.be == source.be && f->wolf->base.bb == source.bb &&
                  f->wolf->base.world == f->base.world && f->wolf->random->state == source_random &&
                  f->wolf->random == source_random_owner && f->wolf->bounds == source_box_owner &&
                  f->wolf->mob->living.Y == source_health &&
                  memcmp(f->wolf->bounds, &source_box, sizeof(source_box)) == 0);
            const bool direction = n[1] == 0 && n[7] != 0 && n[6] != 1;
            CHECK(death_entropy(f, math, unique, item_before, direction));
            uint64_t own = private_state;
            if (n[1] != 1) {
                (void)death_next(&own, 24);
                (void)death_next(&own, 24);
            }
            const bool q = n[6] == 0 && n[8] != 4;
            int32_t quantity = n[0] == 0 ? (n[3] ? 0 : 1) : 0;
            if (q && n[0] > 0 && n[0] < 4 && random != NULL)
                quantity = death_quantity(&own);
            if (f->victim->random)
                CHECK(f->victim->random->state == own);
            size_t expected = q ? (size_t)quantity : 0;
            if (n[11] || n[12])
                expected = 0;
            if (n[8] == 1 && expected > 1)
                expected = 1;
            if (n[8] == 2 && expected > 2)
                expected = 2;
            CHECK(f->item_count - item_before == expected &&
                  f->sounds - sound_before == (n[1] == 1 ? 0u : 1u));
            for (size_t i = item_before; i < f->item_count; ++i) {
                mc_item_stack_fields stack;
                CHECK(mc_item_stack_fields_get(f->items[i]->stack, &stack) == MC_ITEM_STACK_OK &&
                      stack.id == (n[0] == 0   ? 35
                                   : n[0] == 1 ? (n[4] > 0 ? 320 : 319)
                                   : n[0] == 2 ? 334
                                               : 288) &&
                      stack.count == 1 && stack.damage == (n[0] == 0 ? n[2] : 0) &&
                      stack.animation == 0 && f->items[i]->item.c == 10 && f->items[i]->base.bF &&
                      death_members(f, f->items[i]) == 1);
            }
            if (rows) {
                fprintf(rows, "{\"case\":%zu,\"profile\":%zu,\"step\":%d,\"input\":[", row, profile,
                        step);
                for (size_t i = 0; i < 14; ++i)
                    fprintf(rows, "%s%d", i ? "," : "", n[i]);
                fprintf(rows,
                        "],\"result\":%s,\"exception\":\"%s\",\"scope\":\"%s\",\"math_before\":\"%"
                        "016" PRIx64 "\",\"uniquifier_before\":\"%016" PRIx64
                        "\",\"uniquifier_after\":\"%016" PRIx64
                        "\",\"constructor\":{\"actor_private_random\":\"%016" PRIx64
                        "\",\"source_private_random\":\"%016" PRIx64
                        "\",\"math_random\":\"%016" PRIx64 "\"},\"state\":",
                        status == MC_ENTITY_OK && n[1] == 0 ? death_bool(answer) : "null",
                        exception, scope, math, unique, atomic_load(&f->base.profile->uniquifier),
                        f->constructor_actor, f->constructor_source, f->constructor_math);
                death_state_json(rows, f);
                fputs("}\n", rows);
                CHECK(fflush(rows) == 0);
            }
            ++row;
            if (status != MC_ENTITY_OK) {
                const mc_entity_status original = error->status;
                const int32_t source_pc = error->death.site.pc;
                const size_t events = f->event_count, items = f->item_count;
                CHECK(mc_entity_effect_error_retire(error) &&
                      !mc_entity_effect_has_pending(f->victim) && error->status == original &&
                      error->death.site.pc == source_pc && !error->death.pending &&
                      error->death.receiver == NULL && error->death.drop_error == NULL &&
                      f->event_count == events && f->item_count == items &&
                      mc_entity_effect_cleanup(f->victim, NULL) == MC_ENTITY_OK);
            }
            CHECK(f->victim->holds == f->target_holds && f->wolf->holds == f->source_holds);
        }
        f->victim->random = random;
        f->victim->watcher = watcher;
        f->victim->base.world = f->base.world;
        CHECK(death_dispose(f));
        free(f);
    }
    CHECK(row == 43);
    return true;
}

typedef struct late_resources {
    const mc_registry_resources *source;
    death_fixture *fixture;
    mc_entity_watcher *watcher;
    bool fired, busy;
} late_resources;

static mc_registry_status late_open(void *context, const char *name,
                                    mc_java_properties_input *out) {
    late_resources *late = context;
    const mc_registry_status status = late->source->open(late->source->context, name, out);
    if (status == MC_REGISTRY_OK && !late->fired) {
        death_fixture *f = late->fixture;
        late->fired = true;
        late->watcher = f->victim->watcher;
        late->busy = mc_entity_effect_has_pending(f->victim) &&
                     mc_entity_effect_cleanup(f->victim, f->busy_error) == MC_ENTITY_BUSY &&
                     mc_entity_effect_has_pending(f->victim);
        /* A reached real resource read changes the actor after s(), before r(). */
        f->victim->watcher = NULL;
    }
    return status;
}

static mc_registry_status late_close(void *context, const mc_java_properties_input *input) {
    late_resources *late = context;
    return late->source->close(late->source->context, input);
}

static bool late_ready(void *context, const mc_java_properties_input *input) {
    late_resources *late = context;
    return late->source->ready(late->source->context, input);
}

static bool death_native_prefixes(const char *resources, const char *root) {
    for (size_t mode = 0; mode < 6; ++mode) {
        char directory[8192];
        CHECK(death_directory(directory, sizeof(directory), root, "native", mode));
        death_fixture *f = calloc(1, sizeof(*f));
        int n[14];
        death_profile(mode >= 4 ? 40 : 0, n);
        CHECK(f != NULL && death_prepare(f, resources, directory) && matrix_prepare(f, n));
        if (mode < 3) {
            mc_registry_classes *cold = NULL;
            late_resources late = {.source = mc_resource_files_source(f->base.files), .fixture = f};
            const mc_registry_resources wrapped = {&late, late_open, late_close, late_ready};
            if (mode == 2) {
                const mc_registry_options options = {MC_REGISTRY_CLIENT, f->base.profile,
                                                     f->base.materials, 0, &wrapped};
                CHECK(mc_registry_classes_allocate(&options, &cold, NULL) == MC_REGISTRY_OK &&
                      mc_registry_classes_bind_entities(cold, f->base.entities, NULL) ==
                          MC_REGISTRY_OK);
            }
            const mc_entity_status status = mc_entity_effect_death_in_context(
                f->victim, cold, f->wolf, mode == 1 ? NULL : f->effect);
            CHECK(status != MC_ENTITY_OK && mc_entity_effect_has_pending(f->victim) &&
                  f->victim->mob->living.ah && !f->victim->base.be && f->item_count == 0 &&
                  f->event_count == 0);
            mc_entity *alias = f->victim;
            mc_mob_error busy;
            CHECK(mc_mob_retire(&alias, &busy) == MC_MOB_BUSY && alias == f->victim);
            if (mode != 1) {
                const mc_mob_death_state *state = &f->effect->death;
                const mc_item_stack *raw = (const mc_item_stack *)state->sheep_stack_identity;
                mc_item_stack_phase phase;
                mc_item_stack_fields fields;
                CHECK(
                    state->pending && state->ah_stored && state->sheared_read && !state->sheared &&
                    state->stack_allocated && !state->stack_constructed && !state->color_read &&
                    raw != NULL && mc_item_stack_phase_get(raw, &phase) == MC_ITEM_STACK_OK &&
                    phase == MC_ITEM_STACK_RAW &&
                    mc_item_stack_fields_get(raw, &fields) == MC_ITEM_STACK_OK && fields.id == 0 &&
                    fields.count == 0 && fields.damage == 0 && fields.animation == 0);
                if (mode == 0)
                    CHECK(state->q_pc == 12 && state->native_policy && !state->source_exception);
                else
                    CHECK(late.fired && late.busy && late.watcher != NULL &&
                          f->victim->watcher == NULL && state->q_pc == 20 &&
                          state->watcher_pc == 6 && state->source_exception &&
                          !state->native_policy &&
                          strcmp(state->exception_class, "java/lang/NullPointerException") == 0);
                CHECK(mc_entity_effect_error_retire(f->effect) && !f->effect->death.pending &&
                      f->effect->death.receiver == NULL && f->effect->death.drop_error == NULL &&
                      f->effect->status == status);
            } else
                CHECK(mc_entity_effect_cleanup(f->victim, NULL) == MC_ENTITY_OK);
            CHECK(!mc_entity_effect_has_pending(f->victim) && f->item_count == 0 &&
                  f->event_count == 0 && f->victim->mob->living.ah &&
                  mc_entity_effect_cleanup(f->victim, NULL) == MC_ENTITY_OK);
            if (mode == 2) {
                f->victim->watcher = late.watcher; /* Restore only terminal allocation ownership. */
                CHECK(mc_registry_classes_free(cold, NULL) == MC_REGISTRY_OK);
            }
        } else if (mode == 3) {
            f->victim->mob->living.Y = 3;
            f->victim->base.by = 11;
            f->victim->mob->living.au = 4;
            const uint64_t own = f->victim->random->state;
            const uint64_t math = atomic_load(&f->base.profile->math->state);
            const uint64_t unique = atomic_load(&f->base.profile->uniquifier);
            bool answer = false;
            CHECK(mc_entity_effect_damage_with_math(f->victim, f->base.registry, NULL, f->wolf, 7,
                                                    &answer, f->effect) == MC_ENTITY_OK &&
                  answer && !f->effect->living_damage.full_hit &&
                  !f->effect->living_damage.direction_entered &&
                  !f->effect->living_damage.source_hold_acquired &&
                  f->effect->living_damage.private_calls == 0 &&
                  f->effect->living_damage.shared_calls == 0 &&
                  f->effect->death.source_hold_acquired && f->effect->death.source_hold_released &&
                  f->victim->mob->living.Y == 0 && f->victim->mob->living.au == 7 &&
                  f->victim->mob->living.ah && !f->victim->base.be &&
                  f->victim->random->state == own && f->sounds == 0 && f->item_count == 1 &&
                  !f->bad_callback && !mc_entity_effect_has_pending(f->victim) &&
                  death_entropy(f, math, unique, 0, false));
        } else {
            /* The unpublished victim is owned only by the actual Wolf target field
             * and then the failed death Work, never by a fabricated World list. */
            CHECK(mc_mob_creature_target_store(f->wolf, f->victim, NULL) == MC_ENTITY_OK);
            mc_entity_release(f->victim);
            CHECK(f->victim->holds == f->target_holds && !death_world_member(f, f->victim));
            if (mode == 5) {
                bool removed = false, inserted = false;
                CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed,
                                                           NULL) == MC_WORLD_STORAGE_OK &&
                      removed &&
                      mc_world_runtime_insert_entity(f->base.world, f->wolf, &inserted, NULL) ==
                          MC_WORLD_STORAGE_OK &&
                      inserted &&
                      mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
                          MC_WORLD_STORAGE_OK &&
                      death_world_member(f, f->wolf) && death_members(f, f->wolf) == 1 &&
                      f->wolf->holds == f->source_holds + 2);
                f->source_holds = f->wolf->holds;
                f->source_published = true;
            }
            f->mode = 1;
            CHECK(mt_wolf_attack_component(f->wolf, f->base.registry, f->base.math, NULL, f->victim,
                                           1.f, f->attack) == MC_ENTITY_WORLD_FAILED &&
                  f->attack->detail.effect.death.pending &&
                  mc_entity_effect_has_pending(f->victim) && f->attack->site.pc == 195 &&
                  f->wolf->mob->living.ae == 20 && f->item_count == 1 &&
                  f->victim->mob->living.ah && !f->victim->base.be && !f->bad_callback &&
                  !death_world_member(f, f->victim));
            mc_entity *target = NULL;
            if (mode == 4)
                CHECK(mc_mob_tick_error_retire(f->attack) &&
                      !f->attack->detail.effect.death.pending);
            else {
                mc_world_storage_error terminal = {0};
                const size_t events = f->event_count, bindings = f->base.world->persistent_bindings;
                CHECK(mc_world_runtime_retire_entity_containers(f->base.world, &terminal) ==
                          MC_WORLD_STORAGE_BUSY &&
                      terminal.operation != NULL &&
                      strcmp(terminal.operation, "World terminal Mob death cleanup pending") == 0 &&
                      f->base.world->construction.phase == MC_WORLD_RUNTIME_READY &&
                      f->base.world->persistent_bindings == bindings && f->event_count == events &&
                      f->base.world->fields.b->count == 2 && death_members(f, f->wolf) == 1 &&
                      mc_entity_effect_has_pending(f->victim));
                mc_world_entity_tick_error *outer = calloc(1, sizeof(*outer));
                CHECK(outer != NULL);
                mc_world_entity_tick_error_init(outer);
                outer->child = MC_WORLD_ENTITY_TICK_CHILD_MOB;
                /* This exercises the retirement interface only. No World tick is
                 * claimed: transfer the live diagnostic of the actual GI call. */
                outer->child_error.mob = *f->attack;
                mc_mob_tick_error_init(f->attack);
                CHECK(outer->child_error.mob.detail.effect.death.pending &&
                      mc_world_entity_tick_error_retire(outer) &&
                      outer->child == MC_WORLD_ENTITY_TICK_CHILD_NONE &&
                      outer->status == MC_WORLD_STORAGE_OK &&
                      mc_world_entity_tick_error_retire(outer) && f->event_count == events &&
                      f->base.world->fields.b->count == 2);
                free(outer);
            }
            CHECK(!mc_entity_effect_has_pending(f->victim) &&
                  mc_mob_creature_target_get(f->wolf, &target, NULL) == MC_ENTITY_OK &&
                  target == f->victim && target->holds == 1 && f->item_count == 1 &&
                  f->event_count == 2 && f->wolf->holds == f->source_holds);
            CHECK(mc_entity_retain(target) == MC_ENTITY_OK &&
                  mc_mob_creature_target_store(f->wolf, NULL, NULL) == MC_ENTITY_OK);
            if (mode == 5) {
                mc_world_terminal_mob_graph *graph = NULL;
                CHECK(mc_world_runtime_terminal_mob_graph_prepare(f->base.world, &graph, NULL) ==
                          MC_WORLD_STORAGE_OK &&
                      graph == NULL &&
                      f->base.world->construction.phase == MC_WORLD_RUNTIME_READY &&
                      f->event_count == 2);
            }
        }
        CHECK(f->victim->holds == f->target_holds && f->wolf->holds == f->source_holds &&
              death_dispose(f));
        free(f);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 && !(argc == 5 && strcmp(argv[3], "--reference") == 0))
        return 2;
    bool passed;
    if (argc == 5) {
        FILE *rows = open_file(argv[4], "wb");
        if (rows == NULL)
            return 2;
        passed = death_matrix(argv[1], argv[2], rows);
        if (fclose(rows) != 0)
            passed = false;
    } else {
        char directory[8192];
        death_fixture *f = calloc(1, sizeof(*f));
        passed = f != NULL && death_directory(directory, sizeof(directory), argv[2], "red", 0) &&
                 death_prepare(f, argv[1], directory) && death_wolf_red(f) && death_dispose(f);
        free(f);
        if (passed)
            passed =
                death_matrix(argv[1], argv[2], NULL) && death_native_prefixes(argv[1], argv[2]);
    }
    if (!passed) {
        fputs("Animal death failure: actual graph retained; no cleanup credit\n", stderr);
        return 1;
    }
    printf("PASS: %u Animal death source-order/ownership checks\n", checks);
    return 0;
}
