/* Reuse the canonical stored GAME fixture and its checked graph teardown.
 * Its imported contract main is a separate, unexecuted ordinary function. */
#define MC_COLLISION_BOXES_MAIN mc_raytrace_boxes_fixture_main
#include "test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "mc/entity/hit_result.h"
#include "mc/world/runtime/raytrace.h"
#include <inttypes.h>
#include <math.h>

static bool ray(fixture *f, double sx, double sy, double sz, double ex, double ey, double ez,
                bool liquid, bool ignore, int32_t expected_id, int32_t expected_face,
                size_t steps) {
    bool ok = false;
    mc_entity_vector *start = NULL, *end = NULL;
    mc_world_raytrace_call *call = NULL;
    mc_world_raytrace_state state = {0};
    mc_entity_hit_result_view hit = {0};
    mc_entity_classes *classes = f->entity_classes[0];
    CHECK(mc_entity_vector_create(classes, sx, sy, sz, &start, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_create(classes, ex, ey, ez, &end, NULL) == MC_ENTITY_OK);
    const size_t holds = f->worlds[0]->persistent_bindings;
    CHECK(mc_world_raytrace_flags_begin(f->worlds[0], start, end, liquid, ignore, &call) ==
          MC_WORLD_RAYTRACE_OK);
    CHECK(mc_world_raytrace_inspect(call, &state) && state.source_returned && !state.active &&
          state.failure == NULL && state.steps_started == steps && state.world == f->worlds[0] &&
          state.start == start && state.end == end &&
          f->worlds[0]->persistent_bindings == holds + 1);
    if (expected_id == 0)
        CHECK(state.result == NULL);
    else {
        CHECK(state.result != NULL &&
              mc_entity_hit_result_inspect(state.result, &hit) == MC_ENTITY_HIT_OK &&
              hit.phase == MC_ENTITY_HIT_READY && hit.fields.b == 4 && hit.fields.c == 64 &&
              hit.fields.d == 4 && hit.fields.e == expected_face && hit.fields.g == NULL &&
              hit.fields.f != NULL);
        if (steps != 0)
            CHECK(start->x == hit.fields.f->x && start->y == hit.fields.f->y &&
                  start->z == hit.fields.f->z);
    }
    if (steps == 201)
        CHECK(state.cells_started == 202 && state.cells_read == 202 && state.source_pc == 934 &&
              start->x == 201 && start->y == 129.5 && start->z == 0.5);
    if (sx == 3.5 && sy == 63.5)
        CHECK(state.face == (sz == 3.5 ? 2 : 0));
    CHECK(mc_world_raytrace_retire(&call) == MC_WORLD_RAYTRACE_OK && call == NULL &&
          f->worlds[0]->persistent_bindings == holds);
    CHECK(mc_world_raytrace_retire(&call) == MC_WORLD_RAYTRACE_OK);
    ok = true;
cleanup:
    if (call != NULL && mc_world_raytrace_retire(&call) != MC_WORLD_RAYTRACE_OK)
        return false;
    if (start != NULL && mc_entity_vector_release(classes, start) != MC_ENTITY_OK)
        return false;
    if (end != NULL && mc_entity_vector_release(classes, end) != MC_ENTITY_OK)
        return false;
    return ok;
}

static bool basic(fixture *f) {
    bool ok = false;
    boxes_air(f, 0);
    CHECK(boxes_cell(f, 0, 4, 64, 4, 1, 0));
    CHECK(ray(f, 2.5, 64.5, 4.5, 6.5, 64.5, 4.5, false, false, 1, 4, 2));
    CHECK(ray(f, 6.5, 64.5, 4.5, 2.5, 64.5, 4.5, false, false, 1, 5, 2));
    CHECK(ray(f, 4.5, 62.5, 4.5, 4.5, 66.5, 4.5, false, false, 1, 0, 2));
    CHECK(ray(f, 4.5, 66.5, 4.5, 4.5, 62.5, 4.5, false, false, 1, 1, 2));
    CHECK(ray(f, 4.5, 64.5, 2.5, 4.5, 64.5, 6.5, false, false, 1, 2, 2));
    CHECK(ray(f, 4.5, 64.5, 6.5, 4.5, 64.5, 2.5, false, false, 1, 3, 2));
    CHECK(ray(f, 4.5, 64.5, 4.5, 6.5, 64.5, 4.5, false, false, 1, 5, 0));
    CHECK(ray(f, 3.5, 63.5, 4.5, 4.5, 64.5, 4.5, false, false, 1, 4, 1));
    CHECK(ray(f, 3.5, 63.5, 3.5, 4.5, 64.5, 4.5, false, false, 1, 4, 1));
    boxes_air(f, 0);
    CHECK(ray(f, 4.5, 64.5, 4.5, 4.5, 64.5, 4.5, false, false, 0, 0, 1));
    /* Full DDA bound outside height, with real border getter returns and no
     * synthetic provider/World. These are not limited by the picker reach. */
    CHECK(ray(f, 0.5, 129.5, 0.5, 1000.5, 129.5, 0.5, false, false, 0, 0, 201));
    CHECK(ray(f, 0.5, 129.5, 0.5, INFINITY, 129.5, 0.5, false, false, 0, 0, 201));
    CHECK(ray(f, 0.5, 129.5, 0.5, -INFINITY, 129.5, 0.5, false, false, 0, 0, 201));
    ok = true;
cleanup:
    return ok;
}

static bool predicates(fixture *f) {
    bool ok = false;
    boxes_air(f, 0);
    CHECK(boxes_cell(f, 0, 4, 64, 4, 8, 0));
    CHECK(ray(f, 4.5, 64.5, 4.5, 4.9, 64.5, 4.5, false, false, 0, 0, 1));
    CHECK(ray(f, 4.5, 64.5, 4.5, 6.5, 64.5, 4.5, true, false, 8, 5, 0));
    CHECK(ray(f, 4.5, 64.5, 4.5, 4.9, 64.5, 4.5, true, true, 0, 0, 1));
    CHECK(boxes_cell(f, 0, 4, 64, 4, 8, 1));
    CHECK(ray(f, 4.5, 64.5, 4.5, 4.9, 64.5, 4.5, true, false, 0, 0, 1));
    CHECK(boxes_cell(f, 0, 4, 64, 4, 51, 0));
    CHECK(ray(f, 4.5, 64.5, 4.5, 4.9, 64.5, 4.5, false, false, 0, 0, 1));
    CHECK(boxes_cell(f, 0, 4, 64, 4, 53, 0));
    mc_block *stairs = f->registry[0]->blocks.m[53];
    mc_registry_block_bounds(stairs, 0, 0, 0, 0.5f, 0.5f, 0.5f);
    CHECK(ray(f, 4.5, 64.75, 4.75, 6.5, 64.75, 4.75, false, false, 53, 5, 0));
    CHECK(stairs->fields.bv == 1 && stairs->fields.bw == 1 && stairs->fields.bx == 1);
    /* Both Rail subclasses use pc's wrapper; low-height selection remains
     * genuine despite the literal-null movement box. */
    for (size_t i = 0; i < 3; ++i) {
        const uint8_t id = i == 0 ? 66 : i == 1 ? 27 : 28;
        CHECK(boxes_cell(f, 0, 4, 64, 4, id, 0));
        CHECK(ray(f, 2.5, 64.0625, 4.5, 6.5, 64.0625, 4.5, false, false, id, 4, 2));
    }
    ok = true;
cleanup:
    return ok;
}

static bool ownership_and_failure(fixture *f) {
    bool ok = false;
    mc_entity_classes *classes = f->entity_classes[0];
    mc_entity_vector *start = NULL, *end = NULL;
    mc_entity_hit_result *held = NULL;
    mc_world_raytrace_call *call = NULL;
    mc_world_raytrace_state state = {0};
    mc_entity_hit_result_view view = {0};
    CHECK(mc_world_raytrace_begin(f->worlds[0], NULL, NULL, &call) ==
          MC_WORLD_RAYTRACE_SOURCE_EXCEPTION);
    CHECK(mc_world_raytrace_inspect(call, &state) && !state.source_returned &&
          state.failure->source_exception && state.failure->site.pc == 1 &&
          state.failure->caller.pc == 5 && state.cells_started == 0);
    CHECK(mc_world_raytrace_retire(&call) == MC_WORLD_RAYTRACE_OK);
    CHECK(mc_entity_vector_create(classes, NAN, 1, 2, &start, NULL) == MC_ENTITY_OK);
    CHECK(mc_world_raytrace_begin(f->worlds[0], start, NULL, &call) == MC_WORLD_RAYTRACE_OK);
    CHECK(mc_world_raytrace_inspect(call, &state) && state.source_returned &&
          state.source_pc == 30 && state.result == NULL && state.cells_started == 0);
    CHECK(mc_world_raytrace_retire(&call) == MC_WORLD_RAYTRACE_OK);
    start->x = 4.5;
    CHECK(mc_world_raytrace_begin(f->worlds[0], start, NULL, &call) ==
          MC_WORLD_RAYTRACE_SOURCE_EXCEPTION);
    CHECK(mc_world_raytrace_inspect(call, &state) && state.failure->site.pc == 33);
    CHECK(mc_world_raytrace_retire(&call) == MC_WORLD_RAYTRACE_OK);
    start->y = 64.5;
    start->z = 4.5;
    CHECK(mc_entity_vector_create(classes, 6.5, 64.5, 4.5, &end, NULL) == MC_ENTITY_OK);
    boxes_air(f, 0);
    CHECK(boxes_cell(f, 0, 4, 64, 4, 255, 0));
    CHECK(mc_world_raytrace_begin(f->worlds[0], start, end, &call) ==
          MC_WORLD_RAYTRACE_SOURCE_EXCEPTION);
    CHECK(mc_world_raytrace_inspect(call, &state) && state.cells_read == 1 &&
          state.predicate_calls == 1 && state.block_ray_calls == 0 &&
          state.failure->site.pc == 188 && state.failure->source_exception);
    CHECK(mc_world_raytrace_retire(&call) == MC_WORLD_RAYTRACE_OK);
    CHECK(boxes_cell(f, 0, 4, 64, 4, 1, 0));
    CHECK(mc_world_raytrace_begin(f->worlds[0], start, end, &call) == MC_WORLD_RAYTRACE_OK);
    CHECK(mc_world_raytrace_inspect(call, &state) && state.result != NULL);
    held = state.result;
    CHECK(mc_entity_hit_result_retain(held, NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_world_raytrace_retire(&call) == MC_WORLD_RAYTRACE_OK &&
          mc_entity_hit_result_inspect(held, &view) == MC_ENTITY_HIT_OK && view.native_holds == 1 &&
          view.fields.e == 5);
    CHECK(mc_entity_vector_pool_reset(classes, NULL) == MC_ENTITY_OK);
    bool overwritten = false;
    mc_entity_vector_snapshot snapshot = {0};
    CHECK(mc_entity_vector_get_snapshot(classes, &snapshot) == MC_ENTITY_OK);
    for (size_t i = 0; i < snapshot.size; ++i) {
        mc_entity_vector *next = NULL;
        CHECK(mc_entity_vector_pool_get(classes, 91, 92, 93, &next, NULL) == MC_ENTITY_OK);
        overwritten = next == view.fields.f;
        CHECK(mc_entity_vector_release(classes, next) == MC_ENTITY_OK);
        if (overwritten)
            break;
    }
    CHECK(overwritten && view.fields.f->x == 91 && view.fields.f->y == 92 &&
          view.fields.f->z == 93);
    CHECK(mc_entity_vector_pool_clear_client(classes, NULL) == MC_ENTITY_OK &&
          mc_entity_hit_result_inspect(held, &view) == MC_ENTITY_HIT_OK && view.fields.f->x == 91);
    CHECK(mc_entity_hit_result_release(&held, NULL) == MC_ENTITY_HIT_OK);
    /* The identical operand owns two occurrences; retiring never drops the
     * fixture's original occurrence or changes the actual source miss. */
    boxes_air(f, 0);
    CHECK(mc_world_raytrace_begin(f->worlds[0], start, start, &call) == MC_WORLD_RAYTRACE_OK);
    CHECK(mc_world_raytrace_inspect(call, &state) && state.result == NULL &&
          state.steps_started == 1);
    CHECK(mc_world_raytrace_retire(&call) == MC_WORLD_RAYTRACE_OK);
    ok = true;
cleanup:
    if (call != NULL && mc_world_raytrace_retire(&call) != MC_WORLD_RAYTRACE_OK)
        return false;
    if (held != NULL && mc_entity_hit_result_release(&held, NULL) != MC_ENTITY_HIT_OK)
        return false;
    if (start != NULL && mc_entity_vector_release(classes, start) != MC_ENTITY_OK)
        return false;
    if (end != NULL && mc_entity_vector_release(classes, end) != MC_ENTITY_OK)
        return false;
    return ok;
}

static bool late_allocation(fixture *f) {
    bool ok = false;
    mc_entity_classes *classes = f->entity_classes[0];
    const size_t saved_limit = classes->limits.max_allocation_bytes;
    mc_entity_vector *start = NULL, *end = NULL;
    mc_entity_hit_result *raw = NULL;
    mc_world_raytrace_call *call = NULL;
    mc_world_raytrace_state state = {0};
    mc_entity_hit_result_view view = {0};
    CHECK(mc_entity_vector_create(classes, 4.5, 64.75, 4.75, &start, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_create(classes, 6.5, 64.75, 4.75, &end, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_pool_clear_client(classes, NULL) == MC_ENTITY_OK);
    /* Three real reusable slots cover the two local vectors and Xmax plane.
     * The final worldspace point requires a fourth slot after RAW VF NEW458. */
    for (size_t i = 0; i < 3; ++i) {
        mc_entity_vector *slot = NULL;
        CHECK(mc_entity_vector_pool_get(classes, 0, 0, 0, &slot, NULL) == MC_ENTITY_OK);
        CHECK(mc_entity_vector_release(classes, slot) == MC_ENTITY_OK);
    }
    CHECK(mc_entity_vector_pool_reset(classes, NULL) == MC_ENTITY_OK);
    size_t before = classes->allocated_bytes;
    CHECK(mc_entity_hit_result_allocate(classes, &raw, NULL) == MC_ENTITY_HIT_OK);
    const size_t actual_raw_bytes = classes->allocated_bytes - before;
    CHECK(actual_raw_bytes != 0 && mc_entity_hit_result_release(&raw, NULL) == MC_ENTITY_HIT_OK);
    boxes_air(f, 0);
    CHECK(boxes_cell(f, 0, 4, 64, 4, 53, 0));
    mc_block *stairs = f->registry[0]->blocks.m[53];
    mc_registry_block_bounds(stairs, 0, 0, 0, 0.5f, 0.5f, 0.5f);
    classes->limits.max_allocation_bytes = classes->allocated_bytes + actual_raw_bytes;
    const mc_world_raytrace_status status =
        mc_world_raytrace_begin(f->worlds[0], start, end, &call);
    classes->limits.max_allocation_bytes = saved_limit;
    CHECK(status == MC_WORLD_RAYTRACE_LIMIT);
    CHECK(mc_world_raytrace_inspect(call, &state) && !state.source_returned &&
          state.failure->site.pc == 477 && state.failure->vector != NULL &&
          state.failure->vector->status == MC_ENTITY_LIMIT && state.pending_result != NULL &&
          state.result == NULL && stairs->fields.bv == 1 && stairs->fields.bw == 1 &&
          stairs->fields.bx == 1);
    CHECK(mc_entity_hit_result_inspect(state.pending_result, &view) == MC_ENTITY_HIT_OK &&
          view.phase == MC_ENTITY_HIT_RAW && view.fields.a == NULL && view.fields.f == NULL);
    CHECK(mc_world_raytrace_retire(&call) == MC_WORLD_RAYTRACE_OK);
    ok = true;
cleanup:
    classes->limits.max_allocation_bytes = saved_limit;
    if (call != NULL && mc_world_raytrace_retire(&call) != MC_WORLD_RAYTRACE_OK)
        return false;
    if (raw != NULL && mc_entity_hit_result_release(&raw, NULL) != MC_ENTITY_HIT_OK)
        return false;
    if (start != NULL && mc_entity_vector_release(classes, start) != MC_ENTITY_OK)
        return false;
    if (end != NULL && mc_entity_vector_release(classes, end) != MC_ENTITY_OK)
        return false;
    return ok;
}

static void vector_bits(const mc_entity_vector *v) {
    if (v == NULL) {
        fputs("-", stdout);
        return;
    }
    uint64_t bits[3];
    memcpy(&bits[0], &v->x, 8);
    memcpy(&bits[1], &v->y, 8);
    memcpy(&bits[2], &v->z, 8);
    printf("%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64, bits[0], bits[1], bits[2]);
}

/* Authored input wire only: exact binary64, before original-style Vec3
 * construction or the measured ray body. Never normalize measured outputs. */
static bool input_number(const char *token, double *out) {
    if (strlen(token) != 21 || strncmp(token, "bits:", 5) != 0)
        return false;
    uint64_t bits = 0;
    for (size_t i = 5; i < 21; ++i) {
        const unsigned char c = (unsigned char)token[i];
        unsigned int digit;
        if (c >= '0' && c <= '9')
            digit = (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = (unsigned int)(c - 'a') + 10u;
        else if (c >= 'A' && c <= 'F')
            digit = (unsigned int)(c - 'A') + 10u;
        else
            return false;
        bits = (bits << 4u) | digit;
    }
    memcpy(out, &bits, sizeof(bits));
    return true;
}

/* Finite interchange with the untouched-original reflection observer.
 * Preparation restores actual singleton constructor bounds and installs one
 * authored cell in the real stored Chunk. These writes are fixture inputs. */
static bool probe(fixture *f) {
    bool ok = false;
    char line[4096], name[80];
    double initial[97][6];
    mc_entity_vector *start = NULL, *end = NULL;
    mc_world_raytrace_call *call = NULL;
    mc_world_raytrace_state state = {0};
    mc_entity_classes *classes = f->entity_classes[0];
    for (size_t id = 1; id <= 96; ++id) {
        const mc_block_fields *b = &f->registry[0]->blocks.m[id]->fields;
        const double fields[] = {b->bs, b->bt, b->bu, b->bv, b->bw, b->bx};
        memcpy(initial[id], fields, sizeof(fields));
    }
    size_t rows = 0;
    while (fgets(line, sizeof(line), stdin) != NULL) {
        int id = 0, metadata = 0, liquid = 0, ignore = 0, mode = 0;
        char numbers[6][64] = {{0}};
        double sx = 0, sy = 0, sz = 0, ex = 0, ey = 0, ez = 0;
        CHECK(++rows <= 512 &&
              sscanf(line, "%79s %d %d %d %d %d %63s %63s %63s %63s %63s %63s", name, &id,
                     &metadata, &liquid, &ignore, &mode, numbers[0], numbers[1], numbers[2],
                     numbers[3], numbers[4], numbers[5]) == 12 &&
              id >= 0 && id <= 255 && metadata >= 0 && metadata <= 15);
        CHECK(input_number(numbers[0], &sx) && input_number(numbers[1], &sy) &&
              input_number(numbers[2], &sz) && input_number(numbers[3], &ex) &&
              input_number(numbers[4], &ey) && input_number(numbers[5], &ez));
        for (size_t n = 1; n <= 96; ++n) {
            mc_block *b = f->registry[0]->blocks.m[n];
            mc_registry_block_bounds(b, (float)initial[n][0], (float)initial[n][1],
                                     (float)initial[n][2], (float)initial[n][3],
                                     (float)initial[n][4], (float)initial[n][5]);
        }
        boxes_air(f, 0);
        CHECK(boxes_cell(f, 0, 4, 64, 4, (uint8_t)id, metadata));
        CHECK(mc_entity_vector_pool_reset(classes, NULL) == MC_ENTITY_OK);
        CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK);
        if (mode != 2)
            CHECK((mode == 1 ? mc_entity_vector_pool_get(classes, sx, sy, sz, &start, NULL)
                             : mc_entity_vector_create(classes, sx, sy, sz, &start, NULL)) ==
                  MC_ENTITY_OK);
        if (mode != 3)
            CHECK((mode == 1
                       ? mc_entity_vector_pool_get(classes, ex, ey, ez, &end, NULL)
                       : mc_entity_vector_create(classes, ex, ey, ez, &end, NULL)) == MC_ENTITY_OK);
        if (mode == 1)
            CHECK(mc_entity_vector_pool_reset(classes, NULL) == MC_ENTITY_OK);
        const mc_world_raytrace_status status = mc_world_raytrace_flags_begin(
            f->worlds[0], start, end, liquid != 0, ignore != 0, &call);
        CHECK(mc_world_raytrace_inspect(call, &state));
        printf("RAY\t%s\t", name);
        if (status == MC_WORLD_RAYTRACE_OK)
            fputs("OK", stdout);
        else if (state.failure->source_exception)
            fputs(state.failure->exception_class == NULL ? "SOURCE_EXCEPTION"
                                                         : state.failure->exception_class,
                  stdout);
        else
            printf("NATIVE:%d:%s", (int)status, state.failure->operation);
        fputc('\t', stdout);
        if (state.result != NULL) {
            mc_entity_hit_result_view hit = {0};
            CHECK(mc_entity_hit_result_inspect(state.result, &hit) == MC_ENTITY_HIT_OK);
            printf("%d,%d,%d,%d:", hit.fields.b, hit.fields.c, hit.fields.d, hit.fields.e);
            vector_bits(hit.fields.f);
        } else
            fputs("-", stdout);
        fputc('\t', stdout);
        vector_bits(start);
        mc_entity_vector_snapshot pool = {0};
        CHECK(mc_entity_vector_get_snapshot(classes, &pool) == MC_ENTITY_OK);
        mc_entity_class_snapshot class_state = {0};
        CHECK(mc_entity_classes_get_snapshot(classes, &class_state) == MC_ENTITY_OK);
        printf("\t%d\t%d\t", pool.cursor, class_state.bounds_pool_cursor);
        if (id > 0 && id <= 96) {
            mc_block_fields *b = &f->registry[0]->blocks.m[(size_t)id]->fields;
            const double fields[] = {b->bs, b->bt, b->bu, b->bv, b->bw, b->bx};
            for (size_t i = 0; i < 6; ++i) {
                uint64_t bits;
                memcpy(&bits, &fields[i], 8);
                printf(i == 0 ? "%016" PRIx64 : ",%016" PRIx64, bits);
            }
        } else
            fputs("-", stdout);
        fputc('\n', stdout);
        CHECK(mc_world_raytrace_retire(&call) == MC_WORLD_RAYTRACE_OK);
        if (start != NULL)
            CHECK(mc_entity_vector_release(classes, start) == MC_ENTITY_OK);
        if (end != NULL)
            CHECK(mc_entity_vector_release(classes, end) == MC_ENTITY_OK);
        start = end = NULL;
    }
    CHECK(!ferror(stdin) && !ferror(stdout) && rows != 0);
    ok = true;
cleanup:
    if (call != NULL && mc_world_raytrace_retire(&call) != MC_WORLD_RAYTRACE_OK)
        return false;
    if (start != NULL && mc_entity_vector_release(classes, start) != MC_ENTITY_OK)
        return false;
    if (end != NULL && mc_entity_vector_release(classes, end) != MC_ENTITY_OK)
        return false;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'R', 'a', 'y'};
    if (argc != 3 ||
        (strcmp(argv[1], "world-raytrace") != 0 && strcmp(argv[1], "world-raytrace-probe") != 0) ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = (stream){.bytes = language};
    r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(boxes_open_store(f, 0, root) && boxes_create_world(f, 0));
    if (strcmp(argv[1], "world-raytrace-probe") == 0)
        CHECK(probe(f));
    else
        CHECK(basic(f) && predicates(f) && ownership_and_failure(f) && late_allocation(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("raytrace graph cleanup refused\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("PASS: %u World raytrace checks\n", checks);
    return result;
}
