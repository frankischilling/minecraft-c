/* Authored finite Player-hurt transport over the genuine statistics fixture. */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
int mc_player_hurt_statistics_fixture_main(int, char **);
#define main mc_player_hurt_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/inventory/internal.h"
#include "../../../src/inventory/player/source_internal.h"
#include "../../../src/item/stack/source_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "mc/entity/movement/effects.h"
#include "mc/world/runtime/world_access.h"
#include <inttypes.h>
#include <stdarg.h>

#define PH_BYTES 65536u

typedef struct ph_json {
    char bytes[PH_BYTES];
    size_t length;
    bool good;
} ph_json;

typedef struct ph_sound {
    double p[3];
    float volume, pitch;
} ph_sound;

typedef struct ph_context {
    fixture *f;
    mc_entity *actor;
    mc_inventory_player *inventory;
    mc_item_stack **main_array, **armor_array;
    mc_item_stack *stack;
    mc_java_random *random;
    mc_entity_bounds *box;
    mc_world_access *observer;
    unsigned int profile, sound_count;
    ph_sound sounds[8];
    ph_json checkpoint;
    bool bad_callback;
} ph_context;

static ph_json *ph_out;
static bool first;

static void ph_write(const char *format, ...) {
    if (!ph_out->good)
        return;
    va_list args;
    va_start(args, format);
    const int n =
        vsnprintf(ph_out->bytes + ph_out->length, PH_BYTES - ph_out->length, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= PH_BYTES - ph_out->length)
        ph_out->good = false;
    else
        ph_out->length += (size_t)n;
}

static void key(const char *name) {
    ph_write("%s\"%s\":", first ? "" : ",", name);
    first = false;
}

static void emit_i(const char *name, int32_t value) {
    key(name);
    ph_write("%" PRId32, value);
}

static void emit_z(const char *name, bool value) {
    key(name);
    ph_write("%s", value ? "true" : "false");
}

static void emit_r(const char *name, const void *value) {
    key(name);
    ph_write("%s", value ? "{\"nonnull\":true}" : "null");
}

static uint32_t ph_float(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof out);
    return out;
}

static uint64_t ph_double(double value) {
    uint64_t out;
    memcpy(&out, &value, sizeof out);
    return out;
}

static void emit_f(const char *name, float value) {
    key(name);
    ph_write("{\"F\":\"%08" PRIx32 "\"}", ph_float(value));
}

static void emit_d(const char *name, double value) {
    key(name);
    ph_write("{\"D\":\"%016" PRIx64 "\"}", ph_double(value));
}

static void emit_j(const char *name, int64_t value) {
    key(name);
    ph_write("{\"J\":\"%016" PRIx64 "\"}", (uint64_t)value);
}

static void emit_s(const char *name, const mc_player_string *value) {
    key(name);
    if (!value) {
        ph_write("null");
        return;
    }
    mc_player_string_view view;
    if (!mc_player_string_read(value, &view)) {
        ph_out->good = false;
        return;
    }
    ph_write("{\"utf16\":[");
    for (size_t i = 0; i < view.length; ++i)
        ph_write("%s%u", i ? "," : "", (unsigned int)view.units[i]);
    ph_write("]}");
}

static void ph_random(const mc_java_random *r) {
    if (!r) {
        ph_write("null");
        return;
    }
    ph_write("[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]", r->state,
             r->have_next_next_gaussian ? "true" : "false", ph_double(r->next_next_gaussian));
}

typedef struct ph_entry {
    int32_t id, value;
} ph_entry;

static int ph_entry_compare(const void *left, const void *right) {
    const ph_entry *a = left, *b = right;
    return a->id < b->id ? -1 : a->id > b->id;
}

static void ph_map(const mc_client_stats_map *map) {
    if (!map) {
        ph_write("null");
        return;
    }
    ph_entry entries[1024];
    size_t count = 0;
    for (const stats_entry *e = map->head; e; e = e->next) {
        if (count == 1024) {
            ph_out->good = false;
            return;
        }
        entries[count++] = (ph_entry){mc_statistic_fields_get(e->key)->e, e->value};
    }
    if (count != map->count) {
        ph_out->good = false;
        return;
    }
    qsort(entries, count, sizeof entries[0], ph_entry_compare);
    ph_write("[");
    for (size_t i = 0; i < count; ++i) {
        if (i && entries[i - 1].id == entries[i].id)
            ph_out->good = false;
        ph_write("%s[%" PRId32 ",%" PRId32 "]", i ? "," : "", entries[i].id, entries[i].value);
    }
    ph_write("]");
}

static void ph_slots(ph_context *c, mc_item_stack *const *slots, size_t length) {
    if (!slots) {
        ph_write("null");
        return;
    }
    ph_write("[");
    for (size_t i = 0; i < length; ++i) {
        if (slots[i] && slots[i] != c->stack)
            ph_out->good = false;
        ph_write("%s%s", i ? "," : "", slots[i] ? "\"stack\"" : "null");
    }
    ph_write("]");
}

static void ph_world_fields(const mc_world_runtime_fields *v) {
    first = true;
#define WZ(n) emit_z("fd." #n, v->n)
#define WI(n) emit_i("fd." #n, v->n)
#define WF(n) emit_f("fd." #n, v->n)
#define WJ(n) emit_j("fd." #n, v->n)
#define WR(n) emit_r("fd." #n, v->n)
    WZ(a);
    WR(C);
    WR(b);
    WR(D);
    WR(E);
    WR(F);
    WR(c);
    WR(G);
    WR(d);
    WR(e);
    WJ(H);
    WI(f);
    WI(g);
    WI(h);
    WF(i);
    WF(j);
    WF(k);
    WF(l);
    WI(m);
    WI(n);
    WZ(o);
    WJ(I);
    WI(p);
    WI(q);
    WR(r);
    WZ(s);
    WR(t);
    WR(u);
    WR(v);
    WR(w);
    WR(x);
    WZ(y);
    WZ(J);
    WR(z);
    WR(K);
    WZ(L);
    WI(M);
    WZ(N);
    WZ(O);
    WR(P);
    WI(Q);
    WR(R);
    WZ(B);
#undef WZ
#undef WI
#undef WF
#undef WJ
#undef WR
}

static bool ph_state(ph_context *c, ph_json *json) {
    memset(json, 0, sizeof *json);
    json->good = true;
    ph_out = json;
    mc_entity *entity = c->actor;
    mc_local_player_snapshot state;
    if (mc_local_player_get_snapshot(entity, &state) != MC_ENTITY_OK)
        return false;
    ph_write("{\"fields\":{");
    first = true;
#include "trace_fields.inc"
    ph_write("},\"bounds\":[");
    const mc_entity_bounds *b = entity->bounds;
    if (!b)
        return false;
    const double coordinates[] = {b->min_x, b->min_y, b->min_z, b->max_x, b->max_y, b->max_z};
    for (size_t i = 0; i < 6; ++i)
        ph_write("%s{\"D\":\"%016" PRIx64 "\"}", i ? "," : "", ph_double(coordinates[i]));
    ph_write("],\"filters\":[");
    for (size_t i = 0; i < 3; ++i) {
        if (!state.filters[i])
            return false;
        ph_write("%s{", i ? "," : "");
        first = true;
        emit_f("cu.a", state.filters[i]->a);
        emit_f("cu.b", state.filters[i]->b);
        emit_f("cu.c", state.filters[i]->c);
        ph_write("}");
    }
    ph_write("],\"inventory\":{\"visible\":%s,\"main\":",
             state.player.c == c->inventory ? "true" : "false");
    ph_slots(c, c->inventory->main_slots, 36);
    ph_write(",\"armor\":");
    ph_slots(c, c->inventory->armor_slots, 4);
    ph_write(",\"selected\":%" PRId32 ",\"cursor\":%s,\"stack\":", c->inventory->selected,
             c->inventory->cursor ? "{\"nonnull\":true}" : "null");
    if (!c->stack)
        ph_write("null");
    else {
        mc_item_stack_fields s;
        if (mc_item_stack_fields_get(c->stack, &s) != MC_ITEM_STACK_OK)
            return false;
        ph_write("{\"id\":%" PRId32 ",\"count\":%" PRId32 ",\"damage\":%" PRId32
                 ",\"animation\":%" PRId32 ",\"item\":",
                 s.id, s.count, s.damage, s.animation);
        if (s.id < 0 || s.id >= 32000 || !c->f->app.registry->items.c->elements[s.id])
            ph_write("null");
        else {
            const mc_item *item = c->f->app.registry->items.c->elements[s.id];
            ph_write("{\"class\":\"%s\",\"maximum\":%" PRId32 ",\"defense\":",
                     item->kind == MC_ITEM_ARMOR ? "wa" : "other", item->fields.a);
            if (item->kind == MC_ITEM_ARMOR)
                ph_write("%" PRId32, item->subtype.wa.bl);
            else
                ph_write("null");
            ph_write("}");
        }
        ph_write("}");
    }
    ph_write("},\"random\":[");
    ph_random(entity->random);
    ph_write(",");
    ph_random(c->f->world->fields.r);
    ph_write(",");
    mc_java_math_random_owner *math = c->f->app.profile->math;
    if (!math)
        return false;
    ph_write("[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]]", atomic_load(&math->state),
             math->have_next_next_gaussian ? "true" : "false", ph_double(math->next_next_gaussian));
    ph_write(",\"world\":{\"fields\":{");
    ph_world_fields(&c->f->world->fields);
    ph_write("},\"counts\":[%zu,%zu,%zu,%zu]},\"writer\":{\"total\":",
             c->f->world->entity_membership.count, c->f->world->player_membership.count,
             c->f->world->weather_membership.count, c->f->world->entity_deferral.count);
    mc_client_stats_writer *writer = c->f->app.stats_writer_retained;
    ph_map(writer->total);
    ph_write(",\"unsent\":");
    ph_map(writer->unsent);
    ph_write(",\"dirty\":%s},\"aliases\":{", writer->dirty ? "true" : "false");
    first = true;
    emit_z("world", entity->base.world == c->f->world);
    emit_z("app", state.minecraft == &c->f->app);
    emit_z("writer", c->f->app.stats_writer == writer);
    emit_z("inventory", state.player.c == c->inventory);
    emit_z("main_array", c->inventory->main_slots == c->main_array);
    emit_z("armor_array", c->inventory->armor_slots == c->armor_array);
    emit_z("bounds", entity->bounds == c->box);
    emit_z("actor_random", entity->random == c->random);
    emit_z("default_active_container", state.player.d == state.player.e);
    emit_z("filters_distinct", state.filters[0] != state.filters[1] &&
                                   state.filters[0] != state.filters[2] &&
                                   state.filters[1] != state.filters[2]);
    ph_write("},\"sounds\":[");
    for (unsigned int i = 0; i < c->sound_count; ++i) {
        const ph_sound *s = &c->sounds[i];
        ph_write("%s{\"name\":\"random.hurt\",\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64
                 "\",\"%016" PRIx64 "\"],\"volume\":\"%08" PRIx32 "\",\"pitch\":\"%08" PRIx32 "\"}",
                 i ? "," : "", ph_double(s->p[0]), ph_double(s->p[1]), ph_double(s->p[2]),
                 ph_float(s->volume), ph_float(s->pitch));
    }
    ph_write("]}");
    return json->good;
}

static mc_world_storage_status ph_sound_call(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, float volume, float pitch,
                                             mc_world_storage_error *error) {
    ph_context *c = context;
    mc_registry_string_view text = {0};
    static const char expected[] = "random.hurt";
    if (world != c->f->world || c->sound_count == 8 || !mc_registry_string_read(name, &text) ||
        text.length != sizeof(expected) - 1 || c->actor->holds <= 1)
        c->bad_callback = true;
    if (!c->bad_callback)
        for (size_t i = 0; i < text.length; ++i)
            if (text.units[i] != (uint16_t)expected[i])
                c->bad_callback = true;
    if (c->bad_callback) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Player hurt observer admission", 0};
        return error->status;
    }
    c->sounds[c->sound_count++] = (ph_sound){{x, y, z}, volume, pitch};
    if (c->profile == 21 && !ph_state(c, &c->checkpoint))
        c->bad_callback = true;
    if (c->profile == 19)
        c->actor->base.world = NULL;
    *error = (mc_world_storage_error){0};
    if (c->profile == 18)
        *error =
            (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "authored Player sound refusal", 0};
    return error->status;
}

static bool ph_prepare(ph_context *c, const int32_t n[13]) {
    fixture *f = c->f;
    f->world->fields.B = false;
    f->world->fields.q = 2;
    f->app.stats_writer = f->app.stats_writer_retained;
    CHECK(mc_local_player_allocate(f->app.entities, &f->player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(f->player, f->world, &f->app, f->app.session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    c->actor = f->player;
    c->inventory = c->actor->local_player->fields.player.c;
    c->main_array = c->inventory->main_slots;
    c->armor_array = c->inventory->armor_slots;
    c->random = c->actor->random;
    c->box = c->actor->bounds;
    c->stack = NULL;
    c->sound_count = 0;
    c->bad_callback = false;
    memset(&c->checkpoint, 0, sizeof c->checkpoint);
    CHECK(mc_entity_set_position(c->actor, 8.5, 200.5, 8.5) == MC_ENTITY_OK);
    mc_entity *p = c->actor;
    mc_living_entity_fields *v = &p->local_player->fields.living;
    p->base.yaw = 30;
    p->base.velocity_x = .25;
    p->base.velocity_y = -.375;
    p->base.velocity_z = -.125;
    v->E = 20;
    v->Y = n[1];
    v->Z = 17;
    p->base.by = n[2];
    v->au = n[3];
    v->av = 17;
    v->aa = 3;
    v->ab = 4;
    v->ad = 0;
    v->ac = 99;
    v->al = .25f;
    v->ah = false;
    p->base.bb = false;
    p->base.be = false;
    p->local_player->fields.player.private_bO = n[4];
    p->local_player->fields.player.u = false;
    if (n[5]) {
        CHECK(mc_item_stack_allocate(&c->stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(c->stack, n[5], 1, n[6]) == MC_ITEM_STACK_OK);
        CHECK(mc_inventory_player_source_set(c->inventory, 39, c->stack, NULL) == MC_INVENTORY_OK);
    }
    ph_json *constructor = malloc(sizeof *constructor);
    CHECK(constructor != NULL && ph_state(c, constructor));
    printf("PLAYER_HURT_CONSTRUCTOR {\"profile\":%u,\"state\":%s}\n", c->profile,
           constructor->bytes);
    free(constructor);
    mc_java_random_reseed(p->random, 38);
    mc_java_random_reseed(f->world->fields.r, 2468);
    mc_java_random math;
    mc_java_random_seed(&math, 0);
    CHECK(f->app.profile->math != NULL);
    atomic_store(&f->app.profile->math->state, math.state);
    f->app.profile->math->have_next_next_gaussian = false;
    if (n[7] == 1)
        f->world->fields.B = true;
    if (n[7] == 2)
        p->local_player->fields.player.c = NULL;
    if (n[7] == 3)
        c->inventory->armor_slots = NULL;
    if (n[7] == 4)
        CHECK(mc_item_stack_source_id_set(c->stack, -1) == MC_ITEM_STACK_OK);
    if (n[7] == 5)
        p->local_player->fields.minecraft = NULL;
    if (n[7] == 6)
        f->app.stats_writer = NULL;
    if (n[7] == 7)
        p->base.world = NULL;
    if (n[7] == 8)
        p->random = NULL;
    return true;
}

static bool ph_release_actor(ph_context *c) {
    /* Fault-field restoration is native teardown after all selected snapshots,
     * never a repair before a reused Source call. Every allocation stays owned. */
    c->actor->base.world = c->f->world;
    c->actor->random = c->random;
    c->actor->local_player->fields.minecraft = &c->f->app;
    c->actor->local_player->fields.player.c = c->inventory;
    c->inventory->armor_slots = c->armor_array;
    c->f->app.stats_writer = c->f->app.stats_writer_retained;
    mc_entity_release(c->actor);
    c->f->player = NULL;
    c->actor = NULL;
    mc_item_stack_release(c->stack);
    c->stack = NULL;
    return true;
}

static FILE *ph_open(const char *name, const char *mode) {
#ifdef _MSC_VER
    FILE *file = NULL;
    if (fopen_s(&file, name, mode) != 0)
        return NULL;
    return file;
#else
    return fopen(name, mode);
#endif
}

static bool ph_run(ph_context *c, FILE *inputs, FILE *output) {
    ph_json *before = malloc(sizeof *before), *after = malloc(sizeof *after);
    mc_entity_effect_error *error = malloc(sizeof *error);
    CHECK(before && after && error);
    unsigned int row = 0;
    for (unsigned int profile = 0; profile < 22; ++profile) {
        int32_t n[13] = {0};
        for (size_t i = 0; i < 9; ++i)
            CHECK(fscanf(inputs, "%" SCNd32, &n[i]) == 1);
        CHECK(n[0] == (int32_t)profile && n[8] > 0 && n[8] <= 4);
        for (int32_t i = 0; i < n[8]; ++i)
            CHECK(fscanf(inputs, "%" SCNd32, &n[9 + i]) == 1);
        c->profile = profile;
        CHECK(ph_prepare(c, n));
        for (int32_t step = 0; step < n[8]; ++step) {
            CHECK(ph_state(c, before));
            const size_t holds = c->actor->holds;
            const mc_world_runtime_fields world_before = c->f->world->fields;
            bool result = true;
            const mc_entity_status status = mc_entity_effect_damage_in_context(
                c->actor, c->f->app.registry, NULL, n[9 + step], &result, error);
            CHECK(ph_state(c, after) && !c->bad_callback && c->actor->holds == holds &&
                  c->f->world->active_calls == 0 && c->f->world->local_leases == 0 &&
                  memcmp(&world_before, &c->f->world->fields, sizeof world_before) == 0);
            const char *exception = "NONE", *scope = "RETURN";
            if (status != MC_ENTITY_OK) {
                scope = "EXCEPTION";
                if (error->cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE ||
                    error->statistic_error.status == MC_PLAYER_STATISTIC_NULL_REFERENCE ||
                    (error->armor.source_exception && error->armor.exception_class &&
                     strcmp(error->armor.exception_class, "java/lang/NullPointerException") == 0) ||
                    (error->living_damage.source_exception &&
                     error->living_damage.exception_class &&
                     strcmp(error->living_damage.exception_class,
                            "java/lang/NullPointerException") == 0))
                    exception = "NPE";
                else if (error->armor.source_exception && error->armor.exception_class &&
                         strcmp(error->armor.exception_class,
                                "java/lang/ArrayIndexOutOfBoundsException") == 0)
                    exception = "INDEX";
                else if (error->cause == MC_ENTITY_EFFECT_CAUSE_WORLD &&
                         error->world.status == MC_WORLD_STORAGE_IO_ERROR)
                    exception = "IO";
                else if (profile == 21 &&
                         error->cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL &&
                         error->living_bytecode == 316 &&
                         error->living_damage.death_invoke_reached) {
                    exception = "DEPENDENCY";
                    scope = "FATAL_PREFIX";
                } else {
                    fprintf(stderr,
                            "Player hurt unexpected producer: profile%u status%d cause%d %s\n",
                            profile, (int)status, (int)error->cause,
                            error->operation ? error->operation : "-");
                    return false;
                }
            }
            fprintf(output,
                    "{\"schema\":\"PLAYER_HURT_V1\",\"case\":%u,\"profile\":%u,\"step\":%" PRId32
                    ",\"input\":[",
                    row, profile, step);
            for (int32_t i = 0; i < 9 + n[8]; ++i)
                fprintf(output, "%s%" PRId32, i ? "," : "", n[i]);
            fprintf(output,
                    "],\"amount\":%" PRId32 ",\"scope\":\"%s\",\"result\":%s,\"exception\":\"%s\","
                    "\"before\":%s,\"state\":%s,\"completion\":null}\n",
                    n[9 + step], scope,
                    status == MC_ENTITY_OK ? (result ? "true" : "false") : "null", exception,
                    before->bytes, after->bytes);
            fprintf(stdout,
                    "PLAYER_HURT_NATIVE "
                    "{\"case\":%u,\"status\":%d,\"cause\":%d,\"source_pc\":%u,\"living_pc\":%u,"
                    "\"player_pc\":%u,\"armor_pc\":%d,\"holds_unchanged\":true,\"world_unchanged\":"
                    "true,\"calls\":0,\"leases\":0}\n",
                    row, (int)status, (int)error->cause, (unsigned int)error->source_bytecode,
                    (unsigned int)error->living_bytecode, (unsigned int)error->player_bytecode,
                    error->armor.site.pc);
            if (profile == 21)
                CHECK(c->checkpoint.good && strcmp(c->checkpoint.bytes, after->bytes) == 0);
            ++row;
        }
        CHECK(ph_release_actor(c));
    }
    int byte;
    do {
        byte = fgetc(inputs);
    } while (byte == ' ' || byte == '\r' || byte == '\n' || byte == '\t');
    CHECK(byte == EOF && !ferror(inputs) && row == 28 && !ferror(output));
    free(error);
    free(after);
    free(before);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 5 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig)) {
        fputs("Player hurt: resources work-directory profiles output\n", stderr);
        return 2;
    }
    FILE *inputs = ph_open(argv[3], "rb"), *output = ph_open(argv[4], "wb");
    if (!inputs || !output)
        return 2;
    fixture *f = calloc(1, sizeof *f);
    ph_context *c = calloc(1, sizeof *c);
    if (!f || !c)
        return 1;
    c->f = f;
    if (!setup(f, argv[1], "player-hurt", true))
        return 1;
    mc_entity_release(f->player);
    f->player = NULL;
    if (mc_world_info_spawn_set(mc_world_runtime_info(f->world), 0, 64, 0) != MC_WORLD_INFO_OK)
        return 1;
    const mc_world_access_callbacks callbacks = {.sound = ph_sound_call};
    if (mc_world_access_create(&callbacks, c, &c->observer, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_add_world_access(f->world, c->observer, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    const bool passed = ph_run(c, inputs, output);
    bool removed = false;
    if (!passed || fclose(inputs) != 0 || fclose(output) != 0 ||
        mc_world_runtime_remove_world_access(f->world, c->observer, &removed, NULL) !=
            MC_WORLD_STORAGE_OK ||
        !removed || mc_world_access_free(c->observer, NULL) != MC_WORLD_STORAGE_OK || !teardown(f))
        return 1;
    free(c);
    free(f);
    printf("Player hurt: 28 rows; %zu checks\n", checks);
    return 0;
}
