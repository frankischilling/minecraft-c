/* Canonical LL setup/retirement, compiled independently of the original test. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
int mc_tile_renderer_frame_core_fixture_main(int argc, char **argv);
#define main mc_tile_renderer_frame_core_fixture_main
#include "test_tile_entity_renderer.c"
#undef main

#include "core/java/random_profile_internal.h"
#include "entity/entity_internal.h"
#include "mc/entity/player/player.h"

#include <xmmintrin.h>

typedef struct frame_input {
    unsigned int id;
    char name[64];
    uint32_t partial, angles[4];
    uint64_t positions[6];
} frame_input;

typedef struct frame_side {
    uint64_t uniquifier, math_state, math_gaussian;
    const mc_java_math_random_owner *math;
    unsigned int math_phase;
    bool math_have;
    const mc_java_random *random;
    uint64_t random_state, random_gaussian;
    bool random_have;
    int32_t next_id;
    uint32_t angles[4];
    uint64_t positions[6], origins[3];
    const mc_entity_watcher *watcher;
    const mc_entity_watch_map *watch_map;
    size_t watch_size;
    const void *map, *engine, *world, *font;
} frame_side;

static int read_frame_input(FILE *file, frame_input *input) {
    return fscanf(
        file,
        "%u %63s %8" SCNx32 " %8" SCNx32 " %8" SCNx32 " %8" SCNx32 " %8" SCNx32 " %16" SCNx64
        " %16" SCNx64 " %16" SCNx64 " %16" SCNx64 " %16" SCNx64 " %16" SCNx64,
        &input->id, input->name, &input->partial, &input->angles[0], &input->angles[1],
        &input->angles[2], &input->angles[3], &input->positions[0], &input->positions[1],
        &input->positions[2], &input->positions[3], &input->positions[4], &input->positions[5]);
}

static void print_frame_input(const char *tag, const frame_input *input) {
    row("%s %u %s %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %016" PRIx64
        " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64,
        tag, input->id, input->name, input->partial, input->angles[0], input->angles[1],
        input->angles[2], input->angles[3], input->positions[0], input->positions[1],
        input->positions[2], input->positions[3], input->positions[4], input->positions[5]);
}

static void pose_words(const mc_entity_base_fields *fields, uint32_t angles[4],
                       uint64_t positions[6]) {
    angles[0] = fb(fields->aU);
    angles[1] = fb(fields->yaw);
    angles[2] = fb(fields->aV);
    angles[3] = fb(fields->aT);
    positions[0] = db(fields->bl);
    positions[1] = db(fields->bm);
    positions[2] = db(fields->bn);
    positions[3] = db(fields->x);
    positions[4] = db(fields->y);
    positions[5] = db(fields->z);
}

static void seed_frame_inputs(mc_entity *actor, mc_client_tile_renderer_dispatcher *manager,
                              const frame_input *input) {
    mc_entity_base_fields *fields = mc_entity_runtime_fields(actor);
    float *angles[4] = {&fields->aU, &fields->yaw, &fields->aV, &fields->aT};
    double *positions[6] = {&fields->bl, &fields->bm, &fields->bn,
                            &fields->x,  &fields->y,  &fields->z};
    for (size_t i = 0; i < 4; ++i)
        memcpy(angles[i], &input->angles[i], sizeof(input->angles[i]));
    for (size_t i = 0; i < 6; ++i)
        memcpy(positions[i], &input->positions[i], sizeof(input->positions[i]));
    const uint32_t h = UINT32_C(0x3f012345), pitch = UINT32_C(0xbf054321);
    const uint64_t j = UINT64_C(0x3ff123456789abcd), k = UINT64_C(0x3ff123456789abce),
                   l = UINT64_C(0x3ff123456789abcf);
    memcpy(&manager->fields.h, &h, sizeof(h));
    memcpy(&manager->fields.i, &pitch, sizeof(pitch));
    memcpy(&manager->fields.j, &j, sizeof(j));
    memcpy(&manager->fields.k, &k, sizeof(k));
    memcpy(&manager->fields.l, &l, sizeof(l));
}

/* Actual native side effects, including absent unconstructed children. The
 * runner marks Random/Math/Watcher absence preservation UNQUALIFIED against
 * the original instantiated objects; no seed or constructor result is copied. */
static frame_side frame_side_get(const fixture *f, const mc_entity *actor) {
    frame_side result = {0};
    result.uniquifier = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    result.math = f->profile->math;
    result.math_phase = atomic_load_explicit(&f->profile->holder_phase, memory_order_relaxed);
    if (result.math != NULL) {
        result.math_state = atomic_load_explicit(&result.math->state, memory_order_relaxed);
        result.math_gaussian = db(result.math->next_next_gaussian);
        result.math_have = result.math->have_next_next_gaussian;
    }
    result.random = actor->random;
    if (result.random != NULL) {
        result.random_state = result.random->state;
        result.random_gaussian = db(result.random->next_next_gaussian);
        result.random_have = result.random->have_next_next_gaussian;
    }
    result.next_id = actor->classes->next_id;
    pose_words(&actor->base, result.angles, result.positions);
    result.watcher = actor->watcher;
    if (result.watcher != NULL) {
        result.watch_map = result.watcher->map;
        if (result.watch_map != NULL)
            result.watch_size = result.watch_map->size;
    }
    result.map = f->manager->fields.m;
    result.engine = f->manager->fields.e;
    result.world = f->manager->fields.f;
    result.font = f->manager->fields.n;
    result.origins[0] = db(f->classes->b);
    result.origins[1] = db(f->classes->c);
    result.origins[2] = db(f->classes->d);
    return result;
}

static bool frame_controls(fixture *f, mc_entity *actor) {
    frame_input input = {.partial = UINT32_C(0x3f000000),
                         .angles = {0, UINT32_C(0x40000000), 0, UINT32_C(0x40800000)},
                         .positions = {0, 0, 0, UINT64_C(0x4000000000000000),
                                       UINT64_C(0x4010000000000000), UINT64_C(0x4018000000000000)}};
    seed_frame_inputs(actor, f->manager, &input);
    CHECK(mc_client_tile_renderer_bind_frame(f->manager, NULL, NULL, NULL, actor, 0.5f,
                                             &f->error) == MC_CLIENT_TILE_RENDERER_OK);
    mc_client_tile_renderer_state baseline, state;
    CHECK(mc_client_tile_renderer_inspect(f->classes, &baseline));
    CHECK(fb(baseline.fields.h) != UINT32_C(0x3f012345) &&
          fb(baseline.fields.i) != UINT32_C(0xbf054321));
    /* Clear g through its actual nullable source path, so the following
     * partial failure must publish the nonnull passed occurrence again. */
    CHECK(mc_client_tile_renderer_bind_frame(f->manager, NULL, NULL, NULL, NULL, 0.5f, &f->error) ==
              MC_CLIENT_TILE_RENDERER_NULL_REFERENCE &&
          f->error.frame_pc == 32);
    input.positions[0] = UINT64_C(0x7ff0000000001234);
    input.positions[3] = UINT64_C(0xfff8000000005678);
    seed_frame_inputs(actor, f->manager, &input);
    mc_client_tile_renderer_state sentinels;
    CHECK(mc_client_tile_renderer_inspect(f->classes, &sentinels) && sentinels.fields.g == NULL);
    const mc_client_tile_renderer_status numeric =
        mc_client_tile_renderer_bind_frame(f->manager, NULL, NULL, NULL, actor, 0.5f, &f->error);
    CHECK(mc_client_tile_renderer_inspect(f->classes, &state));
    const bool refs = state.fields.e == NULL && state.fields.g == actor && state.fields.n == NULL;
    const bool hi =
        fb(state.fields.h) == fb(baseline.fields.h) && fb(state.fields.i) == fb(baseline.fields.i);
    const bool jkl = db(state.fields.j) == db(sentinels.fields.j) &&
                     db(state.fields.k) == db(sentinels.fields.k) &&
                     db(state.fields.l) == db(sentinels.fields.l);
    CHECK(numeric == MC_CLIENT_TILE_RENDERER_UNSUPPORTED_NUMERIC && f->error.native_policy &&
          f->error.pc == 93 && f->error.frame_pc == 93 && state.last_frame_pc == 74 && refs && hi &&
          jkl);
    printf("TILE_FRAME_CONTROL numeric_late UNSUPPORTED_NUMERIC %d %d refs=%d completed_hi=%d "
           "preserved_jkl=%d native_policy=%d\n",
           f->error.pc, state.last_frame_pc, refs, hi, jkl, f->error.native_policy);
    CHECK(mc_client_tile_renderer_bind_frame(f->manager, NULL, NULL, NULL, NULL, 0.5f, &f->error) ==
          MC_CLIENT_TILE_RENDERER_NULL_REFERENCE);
    input.positions[0] = 0;
    input.positions[3] = UINT64_C(0x4000000000000000);
    seed_frame_inputs(actor, f->manager, &input);
    CHECK(mc_client_tile_renderer_inspect(f->classes, &sentinels) && sentinels.fields.g == NULL);
    const unsigned int saved = _mm_getcsr();
    const unsigned int unsupported = (saved & ~0x6000u) | 0x6000u;
    _mm_setcsr(unsupported);
    const mc_client_tile_renderer_status fp =
        mc_client_tile_renderer_bind_frame(f->manager, NULL, NULL, NULL, actor, 0.5f, &f->error);
    const bool untouched_control = _mm_getcsr() == unsupported;
    _mm_setcsr(saved); /* Authored caller restores its input; production never repairs it. */
    const bool restored = _mm_getcsr() == saved;
    CHECK(mc_client_tile_renderer_inspect(f->classes, &state));
    const bool fp_refs =
        state.fields.e == NULL && state.fields.g == actor && state.fields.n == NULL;
    const bool scalars = fb(state.fields.h) == fb(sentinels.fields.h) &&
                         fb(state.fields.i) == fb(sentinels.fields.i) &&
                         db(state.fields.j) == db(sentinels.fields.j) &&
                         db(state.fields.k) == db(sentinels.fields.k) &&
                         db(state.fields.l) == db(sentinels.fields.l);
    CHECK(fp == MC_CLIENT_TILE_RENDERER_UNSUPPORTED_FP && f->error.native_policy &&
          f->error.pc == 45 && f->error.frame_pc == 45 && state.last_frame_pc == 26 && fp_refs &&
          scalars && untouched_control && restored);
    printf("TILE_FRAME_CONTROL mxcsr UNSUPPORTED_FP %d %d refs=%d preserved_scalars=%d "
           "unchanged_control=%d caller_restored=%d native_policy=%d\n",
           f->error.pc, state.last_frame_pc, fp_refs, scalars, untouched_control, restored,
           f->error.native_policy);
    return true;
}

static bool observe_frames(const char *path) {
    fixture f = {0};
    CHECK(fixture_open(&f));
    CHECK(mc_client_tile_renderer_static_get(f.classes, &f.manager, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    mc_entity *actor = NULL;
    CHECK(mc_local_player_allocate(f.entities, &actor, NULL) == MC_ENTITY_OK && actor != NULL);
    mc_entity_class_literal_view type;
    CHECK(mc_entity_class_literal_inspect(actor->class_literal, &type) == MC_ENTITY_OK &&
          type.owner == f.entities && type.kind == MC_ENTITY_CLASS_KIND_LOCAL_PLAYER &&
          actor->phase == MC_ENTITY_RAW);
    printf("TILE_FRAME_NATIVE_OPERAND %s RAW original=wh constructor=uncredited random=%d math=%d "
           "watcher=%d\n",
           type.client_name, actor->random != NULL, f.profile->math != NULL,
           actor->watcher != NULL);
    mc_client_tile_renderer_map_entry piston_entry;
    mc_client_tile_renderer_object_state piston_before;
    CHECK(mc_client_tile_renderer_map_find(f.manager, f.literals[7], &piston_entry, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_object_inspect(piston_entry.value, &piston_before));
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    for (unsigned int index = 0; index < 24; ++index) {
        frame_input declared = {0}, stored = {0};
        CHECK(read_frame_input(file, &declared) == 13 && read_frame_input(file, &stored) == 13 &&
              declared.id == index && stored.id == index &&
              strcmp(declared.name, stored.name) == 0);
        seed_frame_inputs(actor, f.manager, &stored);
        const frame_side before = frame_side_get(&f, actor);
        print_frame_input("TILE_FRAME_DECLARED", &declared);
        frame_input actual = stored;
        pose_words(&actor->base, actual.angles, actual.positions);
        print_frame_input("TILE_FRAME_INPUT", &actual);
        float partial;
        memcpy(&partial, &stored.partial, sizeof(partial));
        const mc_client_tile_renderer_status status = mc_client_tile_renderer_bind_frame(
            f.manager, NULL, NULL, NULL, actor, partial, &f.error);
        mc_client_tile_renderer_state state;
        mc_client_tile_renderer_object_state piston_after;
        CHECK(mc_client_tile_renderer_inspect(f.classes, &state));
        CHECK(mc_client_tile_renderer_object_inspect(piston_entry.value, &piston_after));
        row("TILE_FRAME_RESULT %u %d %d %d %d %d %d %d %d %08" PRIx32 " %08" PRIx32 " %016" PRIx64
            " %016" PRIx64 " %016" PRIx64,
            index, status == MC_CLIENT_TILE_RENDERER_OK, state.fields.e == NULL,
            state.fields.f == NULL, state.fields.g == actor, state.fields.n == NULL,
            state.fields.m == before.map, state.a == f.manager,
            piston_after.piston_blocks == piston_before.piston_blocks, fb(state.fields.h),
            fb(state.fields.i), db(state.fields.j), db(state.fields.k), db(state.fields.l));
        const frame_side after = frame_side_get(&f, actor);
        row("TILE_FRAME_SIDE %u %d %d %d %d %d %d %d", index, after.uniquifier == before.uniquifier,
            after.random == before.random && after.random_state == before.random_state &&
                after.random_gaussian == before.random_gaussian &&
                after.random_have == before.random_have,
            after.math == before.math && after.math_phase == before.math_phase &&
                after.math_state == before.math_state &&
                after.math_gaussian == before.math_gaussian && after.math_have == before.math_have,
            after.next_id == before.next_id,
            memcmp(after.angles, before.angles, sizeof(before.angles)) == 0 &&
                memcmp(after.positions, before.positions, sizeof(before.positions)) == 0,
            after.map == before.map && after.engine == before.engine &&
                after.world == before.world && after.font == before.font &&
                memcmp(after.origins, before.origins, sizeof(before.origins)) == 0,
            after.watcher == before.watcher && after.watch_map == before.watch_map &&
                after.watch_size == before.watch_size);
        if (status != MC_CLIENT_TILE_RENDERER_OK)
            fprintf(stderr, "LL frame case%u status%d pc%d frame%d\n", index, (int)status,
                    f.error.pc, f.error.frame_pc);
        CHECK(status == MC_CLIENT_TILE_RENDERER_OK && state.last_frame_pc == 152 &&
              f.error.frame_pc == 152 && state.phase == MC_CLIENT_TILE_RENDERER_READY &&
              actor->phase == MC_ENTITY_RAW);
    }
    frame_input extra;
    CHECK(read_frame_input(file, &extra) == EOF && ferror(file) == 0);
    CHECK(fclose(file) == 0);
    CHECK(frame_controls(&f, actor));
    /* Caller drops only its own actual hold; LL's source occurrence persists
     * until the same canonical class retirement releases it. */
    mc_entity_release(actor);
    actor = NULL;
    CHECK(fixture_close(&f));
    CHECK(ferror(stdout) == 0);
    printf("Source TileEntityRenderer frame: cases=24; rows=%u; controls=2; checks=%u; cleanup=1\n",
           rows, checks);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "TileEntityRenderer frame requires an input-only TSV path\n");
        return 1;
    }
    return observe_frames(argv[1]) ? 0 : 1;
}
