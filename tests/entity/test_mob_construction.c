/* Actual Mob owners and GAME World; original scalar replay is labelled separately. */
#include "../support/java_random.h"
#include "core/java/random_profile_internal.h"
#include "entity/entity_internal.h"
#include "entity/mob/internal.h"
#include "entity/player/player_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/entity/mob/construction.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/entities.h"
#include "world/runtime/runtime_internal.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned int checks;
static mc_math_trig trig;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Mob check%u line%d: %s\n", checks, __LINE__, #c);                     \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
static const mc_entity_class_kind kinds[5] = {
    MC_ENTITY_CLASS_KIND_SPIDER, MC_ENTITY_CLASS_KIND_ZOMBIE, MC_ENTITY_CLASS_KIND_SKELETON,
    MC_ENTITY_CLASS_KIND_CREEPER, MC_ENTITY_CLASS_KIND_SLIME};
static const char *const names[5] = {"cn", "uz", "fr", "gb", "uw"};

static uint32_t fbits(float v) {
    uint32_t b;
    memcpy(&b, &v, sizeof(b));
    return b;
}

static uint64_t dbits(double v) {
    uint64_t b;
    memcpy(&b, &v, sizeof(b));
    return b;
}

typedef struct fixture {
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_world_runtime_classes *world_classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_world_info_name *name;
    mc_player_session_classes *sessions;
} fixture;

static bool setup(fixture *f, const char *resources, const char *directory) {
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_statistics(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                          &f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->world_classes, f->registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->world_classes, f->entities) == MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options store = {.session_token = 6381,
                                          .block_map = map,
                                          .block_map_length = 256,
                                          .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, f->regions, &store, &f->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    static const uint16_t name[] = {'P', 'l', 'a', 'y', 'e', 'r'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){name, 6}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimension};
    mc_world_storage_error error;
    const mc_world_storage_status status =
        mc_world_runtime_construct_world(f->world, &args, &error);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "World: %d %s\n", (int)status, error.operation);
    CHECK(status == MC_WORLD_STORAGE_OK);
    CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
          MC_PLAYER_OK);
    return true;
}

static bool teardown(fixture *f) {
    CHECK(mc_player_session_classes_free(&f->sessions, NULL) == MC_PLAYER_OK);
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_unbind(f->dimension);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_chunk_count(f->store) == 0);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(f->dimension);
    CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(f->regions);
    mc_world_info_name_release(f->name);
    CHECK(mc_world_runtime_classes_free(f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    mc_resource_files_free(f->files);
    return true;
}

static FILE *output;
static bool first;

static void key(const char *name) {
    fprintf(output, "%s\"%s\":", first ? "" : ",", name);
    first = false;
}

static void emit_i(const char *name, int32_t v) {
    key(name);
    fprintf(output, "%" PRId32, v);
}

static void emit_z(const char *name, bool v) {
    key(name);
    fputs(v ? "true" : "false", output);
}

static void emit_r(const char *name, const void *v) {
    key(name);
    fputs(v ? "{\"nonnull\":true}" : "null", output);
}

static void emit_f(const char *name, float v) {
    key(name);
    fprintf(output, "{\"F\":\"%08" PRIx32 "\"}", fbits(v));
}

static void emit_d(const char *name, double v) {
    key(name);
    fprintf(output, "{\"D\":\"%016" PRIx64 "\"}", dbits(v));
}

static void emit_s(const char *name, const mc_player_string *v) {
    key(name);
    if (!v) {
        fputs("null", output);
        return;
    }
    mc_player_string_view s;
    if (!mc_player_string_read(v, &s))
        exit(3);
    fputs("{\"utf16\":[", output);
    for (size_t i = 0; i < s.length; ++i)
        fprintf(output, "%s%u", i ? "," : "", (unsigned)s.units[i]);
    fputs("]}", output);
}

static void emit_bounds(const mc_entity_bounds *b) {
    first = true;
    emit_d("eq.a", b->min_x);
    emit_d("eq.b", b->min_y);
    emit_d("eq.c", b->min_z);
    emit_d("eq.d", b->max_x);
    emit_d("eq.e", b->max_y);
    emit_d("eq.f", b->max_z);
}

static void emit_fields(mc_entity *entity, const mc_mob_state *value) {
    const mc_mob_state state = *value;
    first = true;
    /* Authored observation transport; original field order from pinned declarations. */
    emit_i("sn.aD", entity->base.id);
    emit_d("sn.aE", entity->base.aE);
    emit_z("sn.aF", entity->base.aF);
    emit_r("sn.aG", entity->base.aG);
    emit_r("sn.aH", entity->base.aH);
    emit_r("sn.aI", entity->base.world);
    emit_d("sn.aJ", entity->base.aJ);
    emit_d("sn.aK", entity->base.aK);
    emit_d("sn.aL", entity->base.aL);
    emit_d("sn.aM", entity->base.x);
    emit_d("sn.aN", entity->base.y);
    emit_d("sn.aO", entity->base.z);
    emit_d("sn.aP", entity->base.velocity_x);
    emit_d("sn.aQ", entity->base.velocity_y);
    emit_d("sn.aR", entity->base.velocity_z);
    emit_f("sn.aS", entity->base.yaw);
    emit_f("sn.aT", entity->base.aT);
    emit_f("sn.aU", entity->base.aU);
    emit_f("sn.aV", entity->base.aV);
    emit_r("sn.aW", entity->bounds);
    emit_z("sn.aX", entity->base.aX);
    emit_z("sn.aY", entity->base.aY);
    emit_z("sn.aZ", entity->base.aZ);
    emit_z("sn.ba", entity->base.ba);
    emit_z("sn.bb", entity->base.bb);
    emit_z("sn.bc", entity->base.bc);
    emit_z("sn.bd", entity->base.bd);
    emit_z("sn.be", entity->base.be);
    emit_f("sn.bf", entity->base.offset);
    emit_f("sn.bg", entity->base.width);
    emit_f("sn.bh", entity->base.height);
    emit_f("sn.bi", entity->base.bi);
    emit_f("sn.bj", entity->base.bj);
    emit_f("sn.bk", entity->base.bk);
    emit_i("sn.b", entity->base.private_b);
    emit_d("sn.bl", entity->base.bl);
    emit_d("sn.bm", entity->base.bm);
    emit_d("sn.bn", entity->base.bn);
    emit_f("sn.bo", entity->base.y_size);
    emit_f("sn.bp", entity->base.bp);
    emit_z("sn.bq", entity->base.bq);
    emit_f("sn.br", entity->base.br);
    emit_r("sn.bs", entity->random);
    emit_i("sn.bt", entity->base.bt);
    emit_i("sn.bu", entity->base.bu);
    emit_i("sn.bv", entity->base.bv);
    emit_i("sn.bw", entity->base.bw);
    emit_z("sn.bx", entity->base.bx);
    emit_i("sn.by", entity->base.by);
    emit_i("sn.bz", entity->base.bz);
    emit_z("sn.c", entity->base.private_c);
    emit_s("sn.bA", (const mc_player_string *)entity->base.client_only.bA);
    emit_s("sn.bB", entity->base.client_only.bB);
    emit_z("sn.bC", entity->base.bC);
    emit_r("sn.bD", entity->watcher);
    emit_f("sn.bE", entity->base.bE);
    emit_d("sn.d", entity->base.private_d);
    emit_d("sn.e", entity->base.private_e);
    emit_z("sn.bF", entity->base.bF);
    emit_i("sn.bG", entity->base.bG);
    emit_i("sn.bH", entity->base.bH);
    emit_i("sn.bI", entity->base.bI);
    emit_i("sn.bJ", entity->base.client_only.bJ);
    emit_i("sn.bK", entity->base.client_only.bK);
    emit_i("sn.bL", entity->base.client_only.bL);
    emit_z("sn.bM", entity->base.bM);
    emit_i("ls.E", state.living.E);
    emit_f("ls.F", state.living.F);
    emit_f("ls.G", state.living.G);
    emit_f("ls.H", state.living.H);
    emit_f("ls.I", state.living.I);
    emit_f("ls.J", state.living.J);
    emit_f("ls.K", state.living.K);
    emit_f("ls.L", state.living.L);
    emit_f("ls.M", state.living.M);
    emit_z("ls.N", state.living.N);
    emit_s("ls.O", state.living.O);
    emit_z("ls.P", state.living.P);
    emit_f("ls.Q", state.living.Q);
    emit_s("ls.R", state.living.R);
    emit_f("ls.S", state.living.S);
    emit_i("ls.T", state.living.T);
    emit_f("ls.U", state.living.U);
    emit_z("ls.V", state.living.V);
    emit_f("ls.W", state.living.W);
    emit_f("ls.X", state.living.X);
    emit_i("ls.Y", state.living.Y);
    emit_i("ls.Z", state.living.Z);
    emit_i("ls.a", state.living.private_a);
    emit_i("ls.aa", state.living.aa);
    emit_i("ls.ab", state.living.ab);
    emit_f("ls.ac", state.living.ac);
    emit_i("ls.ad", state.living.ad);
    emit_i("ls.ae", state.living.ae);
    emit_f("ls.af", state.living.af);
    emit_f("ls.ag", state.living.ag);
    emit_z("ls.ah", state.living.ah);
    emit_i("ls.ai", state.living.ai);
    emit_f("ls.aj", state.living.aj);
    emit_f("ls.ak", state.living.ak);
    emit_f("ls.al", state.living.al);
    emit_f("ls.am", state.living.am);
    emit_i("ls.an", state.living.an);
    emit_d("ls.ao", state.living.ao);
    emit_d("ls.ap", state.living.ap);
    emit_d("ls.aq", state.living.aq);
    emit_d("ls.ar", state.living.ar);
    emit_d("ls.as", state.living.as);
    emit_f("ls.at", state.living.at);
    emit_i("ls.au", state.living.au);
    emit_i("ls.av", state.living.av);
    emit_f("ls.aw", state.living.aw);
    emit_f("ls.ax", state.living.ax);
    emit_f("ls.ay", state.living.ay);
    emit_z("ls.az", state.living.az);
    emit_f("ls.aA", state.living.aA);
    emit_f("ls.aB", state.living.aB);
    emit_r("ls.b", state.living.private_b);
    emit_i("ls.aC", state.living.aC);

    if (state.kind != MC_ENTITY_CLASS_KIND_SLIME) {
        emit_r("ii.a", state.creature_path);
        emit_r("ii.d", state.creature_target);
        emit_z("ii.e", state.creature_attacking);
        emit_i("gz.c", state.monster_damage);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_CREEPER) {
        emit_i("gb.a", state.creeper_a);
        emit_i("gb.b", state.creeper_b);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_SLIME) {
        emit_f("uw.a", state.slime_a);
        emit_f("uw.b", state.slime_b);
        emit_i("uw.c", state.slime_jump_delay);
    }
}

static FILE *open_file(const char *path, const char *mode) {
#ifdef _MSC_VER
    FILE *f = NULL;
    return fopen_s(&f, path, mode) == 0 ? f : NULL;
#else
    return fopen(path, mode);
#endif
}

static bool read_inputs(const char *path, uint64_t values[15][5]) {
    FILE *file = open_file(path, "rb");
    CHECK(file != NULL);
    size_t count = 0, digits = 0, bytes = 0;
    uint64_t word = 0;
    bool valid = true;
    for (;;) {
        const int c = fgetc(file);
        if (c == EOF) {
            if (ferror(file))
                valid = false;
            break;
        }
        if (++bytes > 8192) {
            valid = false;
            break;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (digits) {
                if (count == 75 || digits != (count % 5 == 4 ? 12u : 16u) ||
                    (count % 5 != 4 && word >= UINT64_C(0x3ff0000000000000))) {
                    valid = false;
                    break;
                }
                values[count / 5][count % 5] = word;
                ++count;
                digits = 0;
                word = 0;
            }
            continue;
        }
        const int n = c >= '0' && c <= '9'   ? c - '0'
                      : c >= 'a' && c <= 'f' ? c - 'a' + 10
                      : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                             : -1;
        if (n < 0 || digits == 16) {
            valid = false;
            break;
        }
        word = (word << 4) | (uint64_t)(unsigned int)n;
        ++digits;
    }
    if (digits) {
        if (count == 75 || digits != (count % 5 == 4 ? 12u : 16u) ||
            (count % 5 != 4 && word >= UINT64_C(0x3ff0000000000000)))
            valid = false;
        else {
            values[count / 5][count % 5] = word;
            ++count;
        }
    }
    CHECK(fclose(file) == 0);
    CHECK(valid && count == 75);
    return true;
}

static bool emit_watcher(const mc_entity *entity) {
    CHECK(entity->watcher && entity->watcher->map);
    fprintf(output, "{\"size\":%zu,\"dirty\":%s,\"records\":[", entity->watcher->map->size,
            entity->watcher->dirty ? "true" : "false");
    size_t seen = 0;
    for (int32_t index = 0; index < 32; ++index)
        for (const mc_entity_watch_node *n = entity->watcher->map->head; n; n = n->next)
            if (n->key == index) {
                CHECK(n->record && n->record->index == index && n->record->type == 0);
                fprintf(output, "%s{\"index\":%d,\"type\":%d,\"value\":%d,\"dirty\":%s}",
                        seen++ ? "," : "", index, n->record->type, (int)n->record->byte_value,
                        n->record->dirty ? "true" : "false");
            }
    CHECK(seen == entity->watcher->map->size);
    fputs("]}", output);
    return true;
}

static void replay(const uint64_t values[5], bool slime) {
    double d[4];
    for (size_t i = 0; i < 4; ++i)
        memcpy(&d[i], &values[i], sizeof(d[i]));
    first = true;
    emit_f("ls.aj", mc_local_player_aj_from_draw(d[0]));
    emit_f("ls.G", mc_local_player_g_from_draw(d[1]));
    emit_f("ls.F", mc_local_player_f_from_draw(d[2]));
    emit_f("sn.aS", mc_local_player_yaw_from_draw(d[3]));
    if (slime) {
        mc_java_random random = {.state = values[4]};
        int32_t choice, delay;
        if (!mc_java_random_next_bounded(&random, 3, &choice) ||
            !mc_java_random_next_bounded(&random, 20, &delay))
            exit(3);
        const int32_t size = 1 << choice;
        const float width = .6f * (float)size;
        emit_i("size", size);
        emit_i("uw.c", delay + 10);
        emit_i("ls.Y", size * size);
        emit_f("sn.bg", width);
        emit_f("sn.bh", width);
        key("private_seed_after");
        fprintf(output, "\"%012" PRIx64 "\"", random.state);
        key("bounds");
        fputc('{', output);
        const mc_entity_bounds b = {-(double)width * .5, 0,
                                    -(double)width * .5, (double)width * .5,
                                    (double)width,       (double)width * .5};
        emit_bounds(&b);
        fputc('}', output);
    }
}

static bool success_cases(fixture *f, uint64_t inputs[15][5]) {
    double warm;
    CHECK(mc_java_math_random(f->profile, &warm) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_item_stack *skeleton = NULL;
    const mc_player_string *textures[5] = {0};
    for (unsigned int index = 0; index < 15; ++index) {
        const unsigned int which = index % 5, pass = index / 5;
        f->world->fields.B = pass == 1;
        mc_entity_class_snapshot before, after;
        CHECK(mc_entity_classes_get_snapshot(f->entities, &before) == MC_ENTITY_OK);
        const uint64_t math_before = atomic_load(&f->profile->math->state),
                       u = atomic_load(&f->profile->uniquifier);
        mc_java_random oracle = {.state = math_before};
        double draws[4];
        for (size_t j = 0; j < 4; ++j)
            draws[j] = mc_java_random_next_double(&oracle);
        const mc_entity_class_literal *literal = NULL;
        CHECK(mc_entity_class_literal_get(f->entities, kinds[which], &literal) == MC_ENTITY_OK);
        mc_entity *entity = NULL;
        mc_mob_error error;
        CHECK(mc_mob_allocate(f->entities, f->registry, literal, &entity, &error) == MC_MOB_OK &&
              entity);
        mc_mob_state state;
        CHECK(mc_mob_inspect(entity, &state) && state.phase == MC_ENTITY_RAW);
        CHECK(atomic_load(&f->profile->uniquifier) == u &&
              atomic_load(&f->profile->math->state) == math_before);
        CHECK(mc_mob_construct(entity, pass == 2 ? NULL : f->world, &error) == MC_MOB_OK &&
              mc_mob_inspect(entity, &state) && state.phase == MC_ENTITY_READY);
        CHECK(mc_entity_classes_get_snapshot(f->entities, &after) == MC_ENTITY_OK &&
              after.next_id == before.next_id + 1);
        CHECK(entity->base.id == before.next_id &&
              entity->base.world == (pass == 2 ? NULL : f->world));
        CHECK(atomic_load(&f->profile->uniquifier) == u * UINT64_C(1181783497276652981) &&
              atomic_load(&f->profile->math->state) == oracle.state);
        CHECK(fbits(state.living.aj) == fbits(mc_local_player_aj_from_draw(draws[0])) &&
              fbits(state.living.G) == fbits(mc_local_player_g_from_draw(draws[1])) &&
              fbits(state.living.F) == fbits(mc_local_player_f_from_draw(draws[2])) &&
              fbits(entity->base.yaw) == fbits(mc_local_player_yaw_from_draw(draws[3])));
        CHECK(entity->random && !entity->random->have_next_next_gaussian &&
              dbits(entity->random->next_next_gaussian) == 0);
        if (!pass)
            textures[which] = state.living.O;
        else
            CHECK(textures[which] == state.living.O);
        CHECK(mc_mob_construct(entity, NULL, &error) == MC_MOB_PHASE &&
              atomic_load(&f->profile->math->state) == oracle.state);
        fprintf(output,
                "{\"case\":%u,\"kind\":\"%s\",\"pass\":%u,\"remote\":%s,\"null_world\":%s,\"same_"
                "world\":true,\"entity_counter_before\":%d,\"entity_counter_after\":%d,\"math_seed_"
                "before\":\"%012" PRIx64 "\",\"math_seed_after\":\"%012" PRIx64 "\",\"fields\":{",
                index, names[which], pass, pass == 1 ? "true" : "false",
                pass == 2 ? "true" : "false", before.next_id, after.next_id, math_before,
                oracle.state);
        emit_fields(entity, &state);
        fputs("},\"bounds\":{", output);
        emit_bounds(entity->bounds);
        fputs("},\"watcher\":", output);
        CHECK(emit_watcher(entity));
        fprintf(output,
                ",\"private_random\":{\"seed48\":\"%012" PRIx64 "\",\"gaussian\":\"%016" PRIx64
                "\",\"cached\":false},\"formula_replay\":{",
                entity->random->state, dbits(entity->random->next_next_gaussian));
        replay(inputs[index], which == 4);
        fputc('}', output);
        if (which == 2) {
            mc_mob_class_state cl;
            mc_item_stack_fields stack;
            CHECK(mc_mob_classes_inspect(f->entities, &cl) &&
                  cl.skeleton_phase == MC_ENTITY_CLASS_READY && cl.skeleton_a &&
                  !cl.skeleton_candidate);
            if (!skeleton)
                skeleton = cl.skeleton_a;
            CHECK(skeleton == cl.skeleton_a);
            CHECK(mc_item_stack_fields_get(skeleton, &stack) == MC_ITEM_STACK_OK);
            fprintf(output,
                    ",\"skeleton_stack\":{\"iz.a\":%d,\"iz.b\":%d,\"iz.c\":%d,\"iz.d\":%d},"
                    "\"skeleton_stack_alias\":true,\"skeleton_stack_same\":true",
                    stack.count, stack.animation, stack.id, stack.damage);
        }
        fputs("}\n", output);
        CHECK(mc_entity_retain(entity) == MC_ENTITY_OK);
        mc_entity *held = entity;
        CHECK(mc_mob_retire(&held, &error) == MC_MOB_BUSY && held == entity);
        mc_entity_release(entity);
        CHECK(mc_mob_retire(&held, &error) == MC_MOB_OK && !held);
        CHECK(atomic_load(&f->profile->math->state) == oracle.state);
    }
    return true;
}

static bool quota_controls(fixture *f) {
    for (size_t which = 0; which < 5; ++which) {
        const mc_entity_class_literal *literal = NULL;
        mc_entity *entity = NULL;
        mc_mob_error error;
        CHECK(mc_entity_class_literal_get(f->entities, kinds[which], &literal) == MC_ENTITY_OK);
        CHECK(mc_mob_allocate(f->entities, f->registry, literal, &entity, &error) == MC_MOB_OK);
        const size_t saved = f->entities->limits.max_allocation_bytes;
        const uint64_t random = atomic_load(&f->profile->uniquifier),
                       math = atomic_load(&f->profile->math->state);
        f->entities->limits.max_allocation_bytes = f->entities->allocated_bytes;
        const mc_mob_status status = mc_mob_construct(entity, f->world, &error);
        f->entities->limits.max_allocation_bytes = saved;
        mc_mob_state state;
        CHECK(status != MC_MOB_OK && mc_mob_inspect(entity, &state) &&
              state.phase == MC_ENTITY_FAILED_CONSTRUCTION && state.living_pc == 0);
        CHECK(entity->base_bytecode == 24 && !entity->bounds && !entity->random &&
              !entity->watcher);
        CHECK(atomic_load(&f->profile->uniquifier) == random &&
              atomic_load(&f->profile->math->state) == math);
        CHECK(mc_mob_construct(entity, NULL, &error) == MC_MOB_PHASE);
        CHECK(mc_mob_retire(&entity, &error) == MC_MOB_OK && !entity);
    }
    const mc_entity_class_kind hooks[] = {MC_ENTITY_CLASS_KIND_CREEPER, MC_ENTITY_CLASS_KIND_SLIME};
    for (size_t which = 0; which < 2; ++which) {
        const mc_entity_class_literal *literal = NULL;
        mc_entity *entity = NULL;
        mc_mob_error error;
        CHECK(mc_entity_class_literal_get(f->entities, hooks[which], &literal) == MC_ENTITY_OK);
        CHECK(mc_mob_allocate(f->entities, f->registry, literal, &entity, &error) == MC_MOB_OK);
        const size_t saved = f->entities->limits.max_allocation_bytes;
        const size_t base = sizeof(mc_entity_bounds) + sizeof(mc_java_random) +
                            sizeof(mc_entity_watcher) + sizeof(mc_entity_watch_map) +
                            sizeof(mc_entity_watch_record) + sizeof(mc_entity_watch_node);
        const uint64_t math = atomic_load(&f->profile->math->state);
        f->entities->limits.max_allocation_bytes = f->entities->allocated_bytes + base;
        const mc_mob_status status = mc_mob_construct(entity, NULL, &error);
        f->entities->limits.max_allocation_bytes = saved;
        mc_mob_state state;
        CHECK(status != MC_MOB_OK && mc_mob_inspect(entity, &state));
        CHECK(state.phase == MC_ENTITY_FAILED_CONSTRUCTION && entity->base_bytecode == 224 &&
              state.living_pc == 0 && state.watcher_hook_pc != 0);
        CHECK(entity->watcher && entity->watcher->map->size == 1 && !state.watcher_16_present &&
              !state.watcher_17_present);
        CHECK(atomic_load(&f->profile->math->state) == math);
        CHECK(mc_mob_retire(&entity, &error) == MC_MOB_OK && !entity);
    }
    return true;
}

static bool registry_new(fixture *f, mc_java_random_profile *profile,
                         mc_registry_classes **registry, bool prepared) {
    const mc_registry_options options = {MC_REGISTRY_CLIENT, profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, registry, NULL) == MC_REGISTRY_OK);
    if (prepared)
        CHECK(mc_registry_classes_initialize_statistics(*registry, NULL) == MC_REGISTRY_OK);
    return true;
}

static bool math_failures(fixture *f) {
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_registry_classes *registry = NULL;
    CHECK(mc_java_random_profile_create(sizeof(*profile), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(registry_new(f, profile, &registry, true));
    for (size_t which = 0; which < 5; ++which) {
        const mc_entity_class_literal *literal = NULL;
        mc_entity *entity = NULL;
        mc_mob_error error;
        CHECK(mc_entity_class_literal_get(classes, kinds[which], &literal) == MC_ENTITY_OK);
        CHECK(mc_mob_allocate(classes, registry, literal, &entity, &error) == MC_MOB_OK);
        CHECK(mc_mob_construct(entity, NULL, &error) != MC_MOB_OK);
        mc_mob_state state;
        CHECK(mc_mob_inspect(entity, &state) && state.phase == MC_ENTITY_FAILED_CONSTRUCTION);
        CHECK(state.living_pc == 89 && state.living.E == 20 && state.living.ai == -1 &&
              fbits(state.living.aj) == 0 && !entity->base.aF);
        CHECK(entity->base_bytecode == 231 && entity->watcher && entity->random && !profile->math &&
              atomic_load(&profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_FAILED);
        CHECK(mc_mob_retire(&entity, &error) == MC_MOB_OK && !entity);
    }
    CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    return true;
}

static bool skeleton_controls(fixture *f) {
    mc_entity_classes *classes = NULL;
    const mc_entity_class_literal *literal = NULL;
    mc_entity *entity = NULL;
    mc_mob_error error;
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_entity_class_literal_get(classes, MC_ENTITY_CLASS_KIND_SKELETON, &literal) ==
          MC_ENTITY_OK);
    CHECK(mc_mob_allocate(classes, NULL, literal, &entity, &error) == MC_MOB_ARGUMENT && !entity);
    mc_mob_class_state state;
    CHECK(mc_mob_classes_inspect(classes, &state) && state.skeleton_phase == MC_ENTITY_CLASS_COLD &&
          !state.skeleton_a && !state.skeleton_candidate);
    classes->limits.max_live_entities = 0;
    CHECK(mc_mob_allocate(classes, f->registry, literal, &entity, &error) != MC_MOB_OK && !entity);
    CHECK(mc_mob_classes_inspect(classes, &state) &&
          state.skeleton_phase == MC_ENTITY_CLASS_READY && state.skeleton_a &&
          !state.skeleton_candidate);
    mc_item_stack *stack = (mc_item_stack *)(void *)state.skeleton_a;
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.id == 261 &&
          fields.count == 1 && fields.damage == 0 && fields.animation == 0);
    CHECK(mc_item_stack_retain(stack) == MC_ITEM_STACK_OK);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    classes = NULL;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.id == 261);
    mc_item_stack_release(stack);

    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_entity_class_literal_get(classes, MC_ENTITY_CLASS_KIND_SKELETON, &literal) ==
          MC_ENTITY_OK);
    classes->limits.max_allocation_bytes =
        classes->allocated_bytes + sizeof(mc_mob_class_namespace);
    CHECK(mc_mob_allocate(classes, f->registry, literal, &entity, &error) != MC_MOB_OK && !entity);
    CHECK(mc_mob_classes_inspect(classes, &state) &&
          state.skeleton_phase == MC_ENTITY_CLASS_FAILED && !state.skeleton_a);
    const size_t reached = classes->allocated_bytes;
    classes->limits.max_allocation_bytes = SIZE_MAX;
    CHECK(mc_mob_allocate(classes, f->registry, literal, &entity, &error) ==
              MC_MOB_CLASS_ERRONEOUS &&
          !entity && classes->allocated_bytes == reached);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    classes = NULL;

    mc_registry_classes *cold = NULL;
    CHECK(registry_new(f, f->profile, &cold, false));
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_entity_class_literal_get(classes, MC_ENTITY_CLASS_KIND_SKELETON, &literal) ==
          MC_ENTITY_OK);
    CHECK(mc_mob_allocate(classes, cold, literal, &entity, &error) == MC_MOB_REGISTRY_FAILED &&
          !entity);
    CHECK(error.source_exception && error.exception_class != NULL &&
          strcmp(error.exception_class, "java/lang/ExceptionInInitializerError") == 0);
    CHECK(strcmp(error.site.class_name, "fr") == 0 && error.site.pc == 4 &&
          strcmp(error.site.method, "<clinit>") == 0 && strcmp(error.site.descriptor, "()V") == 0);
    CHECK(error.registry.active_initializer_class == NULL ||
          (error.child.class_name != NULL &&
           strcmp(error.child.class_name, error.registry.active_initializer_class) == 0 &&
           error.child.pc == error.registry.active_initializer_pc &&
           strcmp(error.child.method, "<clinit>") == 0 &&
           strcmp(error.child.descriptor, "()V") == 0));
    CHECK(mc_mob_classes_inspect(classes, &state) &&
          state.skeleton_phase == MC_ENTITY_CLASS_FAILED && !state.skeleton_a &&
          state.skeleton_candidate);
    mc_item_stack_phase phase;
    CHECK(mc_item_stack_phase_get(state.skeleton_candidate, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_RAW);
    const mc_item_stack *candidate = state.skeleton_candidate;
    const mc_mob_error initial_failure = *state.failure;
    const size_t failed_bytes = classes->allocated_bytes;
    const int32_t failed_id = classes->next_id;
    const uint64_t failed_u = atomic_load(&f->profile->uniquifier);
    const uint64_t failed_math = atomic_load(&f->profile->math->state);
    CHECK(mc_mob_allocate(classes, f->registry, literal, &entity, &error) ==
              MC_MOB_CLASS_ERRONEOUS &&
          !entity);
    CHECK(error.source_exception && error.exception_class != NULL &&
          strcmp(error.exception_class, "java/lang/NoClassDefFoundError") == 0);
    CHECK(strcmp(error.site.class_name, "fr") == 0 && error.site.pc == 4 &&
          strcmp(error.site.descriptor, "()V") == 0);
    CHECK(mc_mob_classes_inspect(classes, &state) && state.skeleton_candidate == candidate &&
          state.failure->site.pc == initial_failure.site.pc &&
          state.failure->status == initial_failure.status &&
          state.failure->exception_class == initial_failure.exception_class &&
          classes->allocated_bytes == failed_bytes && classes->next_id == failed_id &&
          atomic_load(&f->profile->uniquifier) == failed_u &&
          atomic_load(&f->profile->math->state) == failed_math);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    CHECK(mc_registry_classes_free(cold, NULL) == MC_REGISTRY_OK);

    cold = NULL;
    CHECK(registry_new(f, f->profile, &cold, false));
    const mc_item *bow = NULL;
    mc_registry_error registry_error;
    CHECK(mc_registry_item_static_get(cold, "i", &bow, &registry_error) != MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_entity_class_literal_get(classes, MC_ENTITY_CLASS_KIND_SKELETON, &literal) ==
          MC_ENTITY_OK);
    const uint64_t previous_u = atomic_load(&f->profile->uniquifier);
    const uint64_t previous_math = atomic_load(&f->profile->math->state);
    CHECK(mc_mob_allocate(classes, cold, literal, &entity, &error) == MC_MOB_REGISTRY_FAILED &&
          !entity);
    CHECK(error.source_exception && error.exception_class != NULL &&
          strcmp(error.exception_class, "java/lang/NoClassDefFoundError") == 0);
    CHECK(strcmp(error.site.class_name, "fr") == 0 && error.site.pc == 4 &&
          strcmp(error.site.descriptor, "()V") == 0);
    CHECK(error.registry.active_initializer_class == NULL ||
          (error.child.class_name != NULL &&
           strcmp(error.child.class_name, error.registry.active_initializer_class) == 0 &&
           error.child.pc == error.registry.active_initializer_pc &&
           strcmp(error.child.method, "<clinit>") == 0 &&
           strcmp(error.child.descriptor, "()V") == 0));
    CHECK(mc_mob_classes_inspect(classes, &state) &&
          state.skeleton_phase == MC_ENTITY_CLASS_FAILED && state.skeleton_candidate &&
          !state.skeleton_a && classes->next_id == 0 &&
          atomic_load(&f->profile->uniquifier) == previous_u &&
          atomic_load(&f->profile->math->state) == previous_math);
    CHECK(mc_item_stack_phase_get(state.skeleton_candidate, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_RAW);
    const mc_mob_error previous_failure = *state.failure;
    const mc_item_stack *previous_candidate = state.skeleton_candidate;
    const size_t previous_bytes = classes->allocated_bytes;
    CHECK(mc_mob_allocate(classes, f->registry, literal, &entity, &error) ==
              MC_MOB_CLASS_ERRONEOUS &&
          !entity);
    CHECK(error.source_exception &&
          strcmp(error.exception_class, "java/lang/NoClassDefFoundError") == 0 &&
          error.first_class_failure.exception_class == previous_failure.exception_class &&
          error.first_class_failure.site.pc == previous_failure.site.pc &&
          strcmp(error.first_class_failure.site.class_name, previous_failure.site.class_name) ==
              0 &&
          strcmp(error.first_class_failure.site.descriptor, previous_failure.site.descriptor) == 0);
    CHECK(mc_mob_classes_inspect(classes, &state) &&
          state.skeleton_candidate == previous_candidate &&
          state.failure->status == previous_failure.status &&
          state.failure->site.pc == previous_failure.site.pc &&
          state.failure->child.pc == previous_failure.child.pc &&
          state.failure->exception_class == previous_failure.exception_class &&
          classes->allocated_bytes == previous_bytes && classes->next_id == 0 &&
          atomic_load(&f->profile->uniquifier) == previous_u &&
          atomic_load(&f->profile->math->state) == previous_math);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    CHECK(mc_registry_classes_free(cold, NULL) == MC_REGISTRY_OK);
    return true;
}

static bool controlled_slimes(void) {
    unsigned int sizes = 0;
    const int64_t samples[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    for (size_t test = 0; test < sizeof(samples) / sizeof(samples[0]) && sizes != 7u; ++test) {
        const uint64_t u1 = UINT64_C(8682522807148012) * UINT64_C(1181783497276652981),
                       u2 = u1 * UINT64_C(1181783497276652981);
        const mc_java_random_constructor_input inputs[] = {{u1, samples[test]},
                                                           {u2, INT64_C(987654321)}};
        mc_java_random_profile *profile = NULL;
        mc_entity_classes *classes = NULL;
        mc_entity *entity = NULL;
        mc_mob_error error;
        CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
        CHECK(mc_java_random_profile_bind_constructor_inputs(profile, inputs, 2) ==
              MC_JAVA_RANDOM_PROFILE_OK);
        CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK);
        const mc_entity_class_literal *literal = NULL;
        CHECK(mc_entity_class_literal_get(classes, MC_ENTITY_CLASS_KIND_SLIME, &literal) ==
              MC_ENTITY_OK);
        CHECK(mc_mob_allocate(classes, NULL, literal, &entity, &error) == MC_MOB_OK);
        mc_java_random own, math;
        mc_java_random_finish_default(u1, samples[test], &own);
        mc_java_random_finish_default(u2, INT64_C(987654321), &math);
        int32_t choice, delay;
        CHECK(mc_java_random_next_bounded(&own, 3, &choice) &&
              mc_java_random_next_bounded(&own, 20, &delay));
        double draws[4];
        for (size_t i = 0; i < 4; ++i)
            draws[i] = mc_java_random_next_double(&math);
        CHECK(mc_mob_construct(entity, NULL, &error) == MC_MOB_OK);
        mc_mob_state state;
        CHECK(mc_mob_inspect(entity, &state));
        const int32_t size = 1 << choice;
        sizes |= 1u << (unsigned int)choice;
        CHECK(state.watcher_16_present && state.watcher_16 == size &&
              state.living.Y == size * size && state.slime_jump_delay == delay + 10);
        CHECK(mc_test_java_random_equal(entity->random, &own) &&
              atomic_load(&profile->math->state) == math.state);
        CHECK(fbits(state.living.aj) == fbits(mc_local_player_aj_from_draw(draws[0])) &&
              fbits(state.living.G) == fbits(mc_local_player_g_from_draw(draws[1])) &&
              fbits(state.living.F) == fbits(mc_local_player_f_from_draw(draws[2])) &&
              fbits(entity->base.yaw) == fbits(mc_local_player_yaw_from_draw(draws[3])));
        CHECK(fbits(entity->base.width) == fbits(.6f * (float)size) &&
              fbits(entity->base.height) == fbits(.6f * (float)size));
        CHECK(dbits(entity->bounds->min_x) == dbits(-(double)entity->base.width * .5) &&
              dbits(entity->bounds->max_x) == dbits((double)entity->base.width * .5) &&
              dbits(entity->bounds->max_y) == dbits((double)entity->base.height));
        CHECK(state.watcher_dirty == (size != 1) && state.watcher_16_dirty);
        CHECK(mc_java_random_profile_constructor_inputs_complete(profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
        CHECK(mc_mob_retire(&entity, &error) == MC_MOB_OK &&
              mc_entity_classes_free(classes) == MC_ENTITY_OK);
        mc_java_random_profile_free(profile);
    }
    CHECK(sizes == 7u);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 5 || !mc_math_trig_init(&trig))
        return 2;
    uint64_t inputs[15][5];
    if (!read_inputs(argv[3], inputs))
        return 2;
    output = open_file(argv[4], "wb");
    if (!output)
        return 2;
    fixture f = {0};
    if (!setup(&f, argv[1], argv[2]) || !success_cases(&f, inputs) || !quota_controls(&f) ||
        !math_failures(&f) || !skeleton_controls(&f) || !controlled_slimes() || !teardown(&f))
        return 1;
    if (fclose(output) != 0)
        return 1;
    printf("Mob construction: %u checks; 15 actual constructors and owned failure prefixes\n",
           checks);
    return 0;
}
