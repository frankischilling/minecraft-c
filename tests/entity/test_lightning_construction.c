/* Constructor prefixes on actual C allocations and genuine RAW GAME Worlds.
 * Complete World/fire mutation scenarios belong to the scheduler integration. */
#include "../support/java_random.h"
#include "core/java/random_profile_internal.h"
#include "entity/classes/classes_internal.h"
#include "entity/entity_internal.h"
#include "entity/lightning/internal.h"
#include "mc/entity/lightning/construction.h"
#include "mc/world/runtime/entities.h"
#include "world/runtime/runtime_internal.h"
#include "world/runtime/world_access_internal.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
static mc_math_trig trig;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Lightning check%u line%d: %s\n", checks, __LINE__, #condition);       \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *classes;
    mc_world_runtime_classes *world_classes;
    mc_world_runtime *world;
    mc_lightning_error *error;
    uint64_t u;
    mc_java_random_constructor_input declared;
} fixture;

static bool setup(fixture *f) {
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    f->u = UINT64_C(8682522807148012) * UINT64_C(1181783497276652981);
    f->declared = (mc_java_random_constructor_input){f->u, INT64_C(-987654321)};
    CHECK(mc_java_random_profile_bind_constructor_inputs(f->profile, &f->declared, 1) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->classes) ==
          MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                          &f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->world_classes, f->classes) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          NULL, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    f->error = calloc(1, sizeof(*f->error));
    CHECK(f->error != NULL);
    return true;
}

static bool teardown(fixture *f) {
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_free(f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_entity_classes_free(f->classes) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    free(f->error);
    return true;
}

static bool entered_case(int32_t difficulty, bool null_world, double y, bool expected_success,
                         unsigned int expected_pc) {
    fixture f = {0};
    CHECK(setup(&f));
    f.world->fields.q = difficulty;
    mc_entity *entity = NULL;
    CHECK(mc_lightning_allocate(f.classes, &entity, NULL) == MC_ENTITY_OK);
    CHECK(entity->lightning != NULL && entity->mob == NULL && entity->local_player == NULL &&
          entity->particle == NULL && entity->phase == MC_ENTITY_RAW);
    CHECK(mc_entity_native_bytes(entity) == sizeof(mc_lightning_entity_object));
    const mc_entity_class_literal *literal = NULL;
    CHECK(mc_entity_class_literal_get(f.classes, MC_ENTITY_CLASS_KIND_C, &literal) ==
              MC_ENTITY_OK &&
          entity->class_literal == literal && f.classes->next_id == 0);
    CHECK(mc_entity_classes_free(f.classes) == MC_ENTITY_BUSY);
    CHECK(atomic_load(&f.profile->uniquifier) == UINT64_C(8682522807148012) &&
          atomic_load(&f.profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD &&
          f.profile->math == NULL);
    const double x = -17.25, z = 31.75;
    const mc_entity_status status =
        mc_lightning_construct(entity, null_world ? NULL : f.world, x, y, z, f.error);
    CHECK((status == MC_ENTITY_OK) == expected_success);
    mc_lightning_state state;
    CHECK(mc_lightning_inspect(entity, &state) && state.ancestor_pc == 5 &&
          state.constructor_pc == expected_pc && state.b == 2 && state.own_random_calls == 2 &&
          state.neighbor_attempts == 0);
    CHECK(state.phase == (expected_success ? MC_ENTITY_READY : MC_ENTITY_FAILED_CONSTRUCTION));
    mc_java_random own;
    mc_java_random_finish_default(f.u, INT64_C(-987654321), &own);
    const int64_t flash = mc_java_random_next_i64(&own);
    int32_t repeats;
    CHECK(mc_java_random_next_bounded(&own, 3, &repeats));
    CHECK(state.a == flash && state.c == repeats + 1 &&
          mc_test_java_random_equal(entity->random, &own));
    CHECK(atomic_load(&f.profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD &&
          f.profile->math == NULL &&
          mc_java_random_profile_constructor_inputs_complete(f.profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(entity->base.id == 0 && f.classes->next_id == 1 && entity->base_bytecode == 231);
    CHECK(entity->base.x == x && entity->base.y == y && entity->base.z == z &&
          entity->base.aJ == x && entity->base.aK == y && entity->base.aL == z &&
          entity->base.bl == x && entity->base.bm == y && entity->base.bn == z &&
          entity->base.yaw == 0.0f && entity->base.aT == 0.0f);
    CHECK(entity->bounds->min_x == x - (double)(entity->base.width / 2.0f) &&
          entity->bounds->max_x == x + (double)(entity->base.width / 2.0f) &&
          entity->bounds->min_y == y && entity->bounds->max_y == y + (double)entity->base.height);
    mc_entity_watcher_snapshot watcher;
    CHECK(mc_entity_watcher_get_snapshot(entity->watcher, &watcher) == MC_ENTITY_OK &&
          watcher.size == 1 && watcher.index == 0 && watcher.type == 0 && watcher.byte_value == 0 &&
          watcher.record_dirty && !watcher.dirty);
    CHECK(entity->base.world == (null_world ? NULL : f.world));
    if (null_world) {
        CHECK(f.error->source_exception && !f.error->native_policy &&
              strcmp(f.error->exception_class, "java/lang/NullPointerException") == 0 &&
              f.error->site.pc == 52 && state.failure->site.pc == 52 && !state.area_reached &&
              state.constructor_pc == 48);
    } else if (difficulty >= 2) {
        CHECK(state.area_reached);
        if (expected_success)
            CHECK(!state.area_present);
        else
            CHECK(f.error->child == MC_LIGHTNING_CHILD_PRESENCE && f.error->site.pc == 76 &&
                  f.error->presence_pc == 69 && f.error->provider_pc == 6 &&
                  !f.error->source_exception);
    } else {
        CHECK(!state.area_reached);
    }
    const uint64_t retained_seed = entity->random->state;
    CHECK(mc_lightning_construct(entity, NULL, 0, 0, 0, f.error) == MC_ENTITY_INVALID_PHASE &&
          entity->random->state == retained_seed && f.classes->next_id == 1);
    CHECK(mc_entity_retain(entity) == MC_ENTITY_OK);
    mc_entity *same = entity;
    CHECK(mc_lightning_retire(&same, NULL) == MC_ENTITY_BUSY && same == entity);
    mc_entity_release(entity);
    CHECK(mc_lightning_retire(&entity, NULL) == MC_ENTITY_OK && entity == NULL);
    CHECK(f.classes->live_entities == 0 && f.world->persistent_bindings == 0);
    CHECK(teardown(&f));
    return true;
}

static bool allocation_and_failed_base(void) {
    fixture f = {0};
    CHECK(setup(&f));
    mc_entity *entity = NULL;
    mc_entity_error error;
    const size_t before = f.classes->allocated_bytes;
    f.classes->limits.max_live_entities = 0;
    CHECK(mc_lightning_allocate(f.classes, &entity, &error) == MC_ENTITY_LIMIT && entity == NULL &&
          f.classes->entity_phase == MC_ENTITY_CLASS_READY && f.classes->next_id == 0 &&
          f.classes->allocated_bytes == before);
    f.classes->limits.max_live_entities = SIZE_MAX;
    f.classes->limits.max_allocation_bytes = before + sizeof(mc_lightning_entity_object) - 1;
    CHECK(mc_lightning_allocate(f.classes, &entity, &error) == MC_ENTITY_LIMIT && entity == NULL &&
          f.classes->live_entities == 0 && f.classes->next_id == 0);
    f.classes->limits.max_allocation_bytes = SIZE_MAX;
    CHECK(mc_lightning_allocate(f.classes, &entity, &error) == MC_ENTITY_OK);
    f.classes->limits.max_allocation_bytes = f.classes->allocated_bytes;
    CHECK(mc_lightning_construct(entity, NULL, 1, 2, 3, f.error) == MC_ENTITY_LIMIT);
    mc_lightning_state state;
    CHECK(mc_lightning_inspect(entity, &state) && state.phase == MC_ENTITY_FAILED_CONSTRUCTION &&
          state.ancestor_pc == 0 && state.constructor_pc == 0 && state.a == 0 && state.b == 0 &&
          state.c == 0 && state.own_random_calls == 0);
    CHECK(entity->base.id == 0 && f.classes->next_id == 1 && entity->random == NULL &&
          entity->watcher == NULL && f.error->child == MC_LIGHTNING_CHILD_ENTITY &&
          strcmp(f.error->site.class_name, "yu") == 0 && f.error->site.pc == 2);
    CHECK(atomic_load(&f.profile->uniquifier) == UINT64_C(8682522807148012) &&
          f.profile->math == NULL);
    mc_entity_release(entity); /* Generic release owns this actual failed allocation. */
    CHECK(f.classes->live_entities == 0 && f.classes->allocated_bytes == before);
    CHECK(teardown(&f));
    return true;
}

static bool admission(void) {
    fixture f = {0};
    CHECK(setup(&f));
    mc_entity *entity = NULL, *item = NULL;
    CHECK(mc_lightning_allocate(f.classes, &entity, NULL) == MC_ENTITY_OK);
    CHECK(mc_item_entity_allocate(f.classes, &item, NULL) == MC_ENTITY_OK);
    mc_lightning_state state;
    CHECK(!mc_lightning_inspect(item, &state) &&
          mc_lightning_construct(item, NULL, 0, 0, 0, f.error) == MC_ENTITY_INVALID_ARGUMENT &&
          item->phase == MC_ENTITY_RAW);
    fixture other = {0};
    CHECK(setup(&other));
    CHECK(mc_lightning_construct(entity, other.world, 0, 0, 0, f.error) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          entity->phase == MC_ENTITY_RAW && f.classes->next_id == 0);
    CHECK(teardown(&other));
    mc_entity_release(item);
    mc_entity_release(entity);
    CHECK(teardown(&f));
    return true;
}

static bool callback_receipt_ownership(void) {
    fixture f = {0};
    CHECK(setup(&f));
    mc_entity *entity = NULL;
    CHECK(mc_lightning_allocate(f.classes, &entity, NULL) == MC_ENTITY_OK);
    CHECK(mc_lightning_construct(entity, NULL, 1, 2, 3, f.error) == MC_ENTITY_STATE_ERROR);
    /* Exercise the native failed-child ownership seam on an actual terminal
     * C allocation. This does not claim an original Fire callback scenario. */
    const mc_world_access_callbacks callbacks = {0};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, NULL, &access, NULL) == MC_WORLD_STORAGE_OK);
    static const unsigned char opaque_cause = 0;
    const mc_client_render_global_error *identity =
        (const mc_client_render_global_error *)(const void *)&opaque_cause;
    mc_world_access_receipt *receipt = NULL;
    mc_world_access_failure borrowed = {0}, external = {0};
    CHECK(mc_world_access_receipt_create(access, identity, &receipt, &borrowed, NULL) ==
          MC_WORLD_STORAGE_OK);
    receipt->exception_class = "java/lang/NullPointerException";
    mc_lightning_error *failure = &entity->lightning->failure;
    failure->status = MC_ENTITY_WORLD_FAILED;
    failure->site = (mc_lightning_site){"c", "<init>", "(Lfd;DDD)V", 144};
    failure->child = MC_LIGHTNING_CHILD_MUTATION;
    failure->detail.mutation.cause =
        (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE, "receipt ownership test", 907};
    failure->detail.mutation.world_access_failure = borrowed;
    CHECK(mc_lightning_capture_failure(entity) == MC_ENTITY_WORLD_FAILED && receipt->holds == 2 &&
          failure->source_exception && !failure->native_policy &&
          strcmp(failure->exception_class, "java/lang/NullPointerException") == 0 &&
          failure->detail.mutation.cause.system_error == 907);
    *f.error = *failure; /* The ordinary public output is a borrowed copy. */
    CHECK(mc_world_access_receipt_retire(&receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_access_failure_retain(&f.error->detail.mutation.world_access_failure,
                                         &external) == MC_WORLD_STORAGE_OK &&
          receipt->holds == 3);
    mc_entity_release(entity); /* Generic final release drops only C's own capture. */
    CHECK(receipt->holds == 2 &&
          mc_world_access_receipt_retire(&receipt, NULL) == MC_WORLD_STORAGE_BUSY);
    const char *exception = NULL;
    CHECK(mc_world_access_failure_source_exception(&external, &exception) &&
          strcmp(exception, "java/lang/NullPointerException") == 0);
    mc_world_access_failure_release(&external);
    CHECK(receipt->holds == 1 &&
          mc_world_access_receipt_retire(&receipt, NULL) == MC_WORLD_STORAGE_OK && receipt == NULL);
    CHECK(mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(teardown(&f));
    return true;
}

static uint32_t probe_fbits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t probe_dbits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static FILE *probe_output;
static bool probe_first;

static void probe_key(const char *name) {
    fprintf(probe_output, "%s\"%s\":", probe_first ? "" : ",", name);
    probe_first = false;
}

static void probe_emit_i(const char *name, int32_t v) {
    probe_key(name);
    fprintf(probe_output, "%" PRId32, v);
}

static void probe_emit_z(const char *name, bool v) {
    probe_key(name);
    fputs(v ? "true" : "false", probe_output);
}

static void probe_emit_r(const char *name, const void *v) {
    probe_key(name);
    fputs(v ? "{\"nonnull\":true}" : "null", probe_output);
}

static void probe_emit_f(const char *name, float v) {
    probe_key(name);
    fprintf(probe_output, "{\"F\":\"%08" PRIx32 "\"}", probe_fbits(v));
}

static void probe_emit_d(const char *name, double v) {
    probe_key(name);
    fprintf(probe_output, "{\"D\":\"%016" PRIx64 "\"}", probe_dbits(v));
}

static void probe_emit_bounds(const mc_entity_bounds *b) {
    probe_first = true;
    probe_emit_d("eq.a", b->min_x);
    probe_emit_d("eq.b", b->min_y);
    probe_emit_d("eq.c", b->min_z);
    probe_emit_d("eq.d", b->max_x);
    probe_emit_d("eq.e", b->max_y);
    probe_emit_d("eq.f", b->max_z);
}

static void probe_emit_fields(mc_entity *entity, const mc_lightning_state *state) {
    probe_first = true;
    probe_emit_i("sn.aD", entity->base.id);
    probe_emit_d("sn.aE", entity->base.aE);
    probe_emit_z("sn.aF", entity->base.aF);
    probe_emit_r("sn.aG", entity->base.aG);
    probe_emit_r("sn.aH", entity->base.aH);
    probe_emit_r("sn.aI", entity->base.world);
    probe_emit_d("sn.aJ", entity->base.aJ);
    probe_emit_d("sn.aK", entity->base.aK);
    probe_emit_d("sn.aL", entity->base.aL);
    probe_emit_d("sn.aM", entity->base.x);
    probe_emit_d("sn.aN", entity->base.y);
    probe_emit_d("sn.aO", entity->base.z);
    probe_emit_d("sn.aP", entity->base.velocity_x);
    probe_emit_d("sn.aQ", entity->base.velocity_y);
    probe_emit_d("sn.aR", entity->base.velocity_z);
    probe_emit_f("sn.aS", entity->base.yaw);
    probe_emit_f("sn.aT", entity->base.aT);
    probe_emit_f("sn.aU", entity->base.aU);
    probe_emit_f("sn.aV", entity->base.aV);
    probe_emit_r("sn.aW", entity->bounds);
    probe_emit_z("sn.aX", entity->base.aX);
    probe_emit_z("sn.aY", entity->base.aY);
    probe_emit_z("sn.aZ", entity->base.aZ);
    probe_emit_z("sn.ba", entity->base.ba);
    probe_emit_z("sn.bb", entity->base.bb);
    probe_emit_z("sn.bc", entity->base.bc);
    probe_emit_z("sn.bd", entity->base.bd);
    probe_emit_z("sn.be", entity->base.be);
    probe_emit_f("sn.bf", entity->base.offset);
    probe_emit_f("sn.bg", entity->base.width);
    probe_emit_f("sn.bh", entity->base.height);
    probe_emit_f("sn.bi", entity->base.bi);
    probe_emit_f("sn.bj", entity->base.bj);
    probe_emit_f("sn.bk", entity->base.bk);
    probe_emit_i("sn.b", entity->base.private_b);
    probe_emit_d("sn.bl", entity->base.bl);
    probe_emit_d("sn.bm", entity->base.bm);
    probe_emit_d("sn.bn", entity->base.bn);
    probe_emit_f("sn.bo", entity->base.y_size);
    probe_emit_f("sn.bp", entity->base.bp);
    probe_emit_z("sn.bq", entity->base.bq);
    probe_emit_f("sn.br", entity->base.br);
    probe_emit_r("sn.bs", entity->random);
    probe_emit_i("sn.bt", entity->base.bt);
    probe_emit_i("sn.bu", entity->base.bu);
    probe_emit_i("sn.bv", entity->base.bv);
    probe_emit_i("sn.bw", entity->base.bw);
    probe_emit_z("sn.bx", entity->base.bx);
    probe_emit_i("sn.by", entity->base.by);
    probe_emit_i("sn.bz", entity->base.bz);
    probe_emit_z("sn.c", entity->base.private_c);
    probe_emit_r("sn.bA", entity->base.client_only.bA);
    probe_emit_r("sn.bB", entity->base.client_only.bB);
    probe_emit_z("sn.bC", entity->base.bC);
    probe_emit_r("sn.bD", entity->watcher);
    probe_emit_f("sn.bE", entity->base.bE);
    probe_emit_d("sn.d", entity->base.private_d);
    probe_emit_d("sn.e", entity->base.private_e);
    probe_emit_z("sn.bF", entity->base.bF);
    probe_emit_i("sn.bG", entity->base.bG);
    probe_emit_i("sn.bH", entity->base.bH);
    probe_emit_i("sn.bI", entity->base.bI);
    probe_emit_i("sn.bJ", entity->base.client_only.bJ);
    probe_emit_i("sn.bK", entity->base.client_only.bK);
    probe_emit_i("sn.bL", entity->base.client_only.bL);
    probe_emit_z("sn.bM", entity->base.bM);
    probe_key("c.a");
    fprintf(probe_output, "{\"J\":\"%016" PRIx64 "\"}", (uint64_t)state->a);
    probe_emit_i("c.b", state->b);
    probe_emit_i("c.c", state->c);
}

static FILE *probe_open(const char *path, const char *mode) {
#ifdef _MSC_VER
    FILE *file = NULL;
    return fopen_s(&file, path, mode) == 0 ? file : NULL;
#else
    return fopen(path, mode);
#endif
}

static bool probe_inputs(const char *path, uint64_t seeds[5]) {
    FILE *input = probe_open(path, "rb");
    CHECK(input != NULL);
    for (size_t i = 0; i < 5; ++i) {
        char line[32];
        CHECK(fgets(line, sizeof(line), input) != NULL && strlen(line) == 13 && line[12] == '\n');
        uint64_t seed = 0;
        for (size_t j = 0; j < 12; ++j) {
            const unsigned char unit = (unsigned char)line[j];
            CHECK((unit >= '0' && unit <= '9') || (unit >= 'a' && unit <= 'f'));
            const unsigned int digit =
                unit <= '9' ? (unsigned int)(unit - '0') : (unsigned int)(unit - 'a') + 10u;
            seed = (seed << 4) | digit;
        }
        seeds[i] = seed;
    }
    CHECK(fgetc(input) == EOF && !ferror(input) && fclose(input) == 0);
    return true;
}

static bool probe_cases(const char *input_path, const char *output_path) {
    uint64_t seeds[5];
    CHECK(probe_inputs(input_path, seeds));
    probe_output = probe_open(output_path, "wb");
    CHECK(probe_output != NULL);
    const int32_t difficulty[5] = {0, 1, 2, 2, 2};
    const double ys[5] = {64.5, -1.5, 140.0, -20.0, 64.5};
    for (size_t i = 0; i < 5; ++i) {
        fixture f = {0};
        CHECK(setup(&f));
        f.world->fields.q = difficulty[i];
        mc_entity *entity = NULL;
        CHECK(mc_lightning_allocate(f.classes, &entity, NULL) == MC_ENTITY_OK);
        const int32_t counter = f.classes->next_id;
        const mc_entity_status status =
            mc_lightning_construct(entity, i == 4 ? NULL : f.world, -17.25, ys[i], 31.75, f.error);
        CHECK((status == MC_ENTITY_OK) == (i != 4));
        mc_lightning_state state;
        CHECK(mc_lightning_inspect(entity, &state));
        CHECK(state.constructor_pc == (i == 4 ? 48u : 262u) && state.own_random_calls == 2 &&
              state.neighbor_attempts == 0 &&
              atomic_load(&f.profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD &&
              f.profile->math == NULL &&
              mc_java_random_profile_constructor_inputs_complete(f.profile) ==
                  MC_JAVA_RANDOM_PROFILE_OK);
        fprintf(probe_output,
                "{\"event\":\"native-constructor\",\"case\":%zu,"
                "\"scope\":\"genuine_RAW_GAME_World_prefix\","
                "\"entropy\":\"fresh_declared_clock\",\"difficulty\":%" PRId32
                ",\"null_world\":%s,\"ready\":%s,\"entity_counter_before\":%" PRId32
                ",\"entity_counter_after\":%" PRId32 ",\"math_holder_cold\":true,"
                "\"math_draws\":0,\"constructor_pc\":%u,\"own_random_calls\":%zu,"
                "\"source_exception\":%s,\"exception_pc\":%u,\"fields\":{",
                i, difficulty[i], i == 4 ? "true" : "false",
                state.phase == MC_ENTITY_READY ? "true" : "false", counter, f.classes->next_id,
                state.constructor_pc, state.own_random_calls,
                f.error->source_exception ? "true" : "false", f.error->site.pc);
        probe_emit_fields(entity, &state);
        fputs("},\"bounds\":{", probe_output);
        probe_emit_bounds(entity->bounds);
        mc_entity_watcher_snapshot watcher;
        CHECK(mc_entity_watcher_get_snapshot(entity->watcher, &watcher) == MC_ENTITY_OK);
        fprintf(probe_output,
                "},\"watcher\":{\"size\":%zu,\"dirty\":%s,\"records\":[{"
                "\"index\":%" PRId32 ",\"type\":%" PRId32 ",\"value\":%d,"
                "\"dirty\":%s}]},\"private_random\":{\"seed48\":\"%012" PRIx64
                "\",\"cached\":%s,\"gaussian\":\"%016" PRIx64 "\"},",
                watcher.size, watcher.dirty ? "true" : "false", watcher.index, watcher.type,
                (int)watcher.byte_value, watcher.record_dirty ? "true" : "false",
                entity->random->state, entity->random->have_next_next_gaussian ? "true" : "false",
                probe_dbits(entity->random->next_next_gaussian));
        /* This plain local value is a scalar replay, never constructor entropy.
         * The actual Entity/profile above are not reseeded or rebound. */
        mc_java_random replay;
        mc_java_random_seed(&replay, (int64_t)(seeds[i] ^ UINT64_C(0x5deece66d)));
        const int64_t word = mc_java_random_next_i64(&replay);
        const uint64_t after_long = replay.state;
        int32_t repeat;
        CHECK(mc_java_random_next_bounded(&replay, 3, &repeat));
        fprintf(probe_output,
                "\"scalar_replay\":{\"label\":\"separate_preseed_scalar_replay\","
                "\"private_seed_before\":\"%012" PRIx64 "\",\"next_long_word\":\"%016" PRIx64
                "\",\"private_seed_after_long\":\"%012" PRIx64 "\",\"next_int3\":%" PRId32
                ",\"private_seed_after\":\"%012" PRIx64 "\"}}\n",
                seeds[i], (uint64_t)word, after_long, repeat, replay.state);
        CHECK(!ferror(probe_output));
        mc_entity_release(entity);
        CHECK(teardown(&f));
    }
    CHECK(fclose(probe_output) == 0);
    probe_output = NULL;
    return true;
}

int main(int argc, char **argv) {
    if (argc != 1 && (argc != 4 || strcmp(argv[1], "--probe") != 0))
        return 2;
    if (!mc_math_trig_init(&trig))
        return 2;
    if (!entered_case(0, false, 70.25, true, 262) || !entered_case(1, false, -1.5, true, 262) ||
        !entered_case(2, false, 140.0, true, 262) || !entered_case(2, false, 60.0, false, 48) ||
        !entered_case(2, true, 60.0, false, 48) || !allocation_and_failed_base() || !admission() ||
        !callback_receipt_ownership())
        return 1;
    if (argc == 4 && !probe_cases(argv[2], argv[3]))
        return 1;
    printf("Lightning construction: %u checks; source prefixes, own RNG and terminal ownership\n",
           checks);
    return 0;
}
