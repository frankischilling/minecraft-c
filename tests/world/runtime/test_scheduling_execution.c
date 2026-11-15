/* Genuine scheduler fixture construction; its original entry is never run. */
int se_unused_schedule_main(int, char **);
#define main se_unused_schedule_main
#include "test_scheduling.c"
#undef main

#include "../../../src/world/runtime/mutation/chunk_execution_internal.h"
#include "../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../src/world/runtime/mutation/world_execution_internal.h"
#include "../../../src/world/runtime/scheduling_execution_internal.h"
#include "mc/world/chunk/nibbles.h"

static const mc_entity_tick_execution_site se_site = {"native.schedule", "run", "()V", 0};
static const mc_world_runtime_execution_code se_code = {{"lm", "e", "()V", 6},
                                                        {"lm", "e", "()V", 33}};

static bool se_call(fixture *f, int32_t x, int32_t y, int32_t z, int32_t block, int32_t delay,
                    mc_world_storage_status raw_expected, mc_world_storage_status owner_expected,
                    unsigned int pc) {
    bool ok = false, open = false;
    mc_world_runtime *world = f->worlds[0];
    mc_entity_tick_execution *execution = calloc(1, sizeof(*execution));
    mc_world_runtime_execution_result *owned = calloc(1, sizeof(*owned));
    mc_world_schedule_execution *frame = calloc(1, sizeof(*frame));
    mc_entity_tick_execution_world_token token = {0};
    mc_world_schedule_class_state before, after;
    CHECK(execution != NULL && owned != NULL && frame != NULL &&
          mc_world_runtime_schedule_class_get(f->classes[0], &before) == MC_WORLD_STORAGE_OK);
    const mc_java_random random = *world->fields.r;
    const size_t cache_count = f->provider[0].cache->count;
    mc_entity_tick_execution_init(execution, NULL);
    mc_world_runtime_execution_result_init(owned);
    CHECK(mc_entity_tick_execution_push_world(execution, world, se_site, &token) ==
          MC_WORLD_STORAGE_OK);
    open = true;
    mc_world_schedule_execution_init(frame, execution, f->classes[0], se_code, token, se_site,
                                     owned);
    mc_world_storage_status raw = MC_WORLD_STORAGE_LIMIT;
    const bool completed =
        mc_world_runtime_schedule_in_execution(frame, world, x, y, z, block, delay, &raw);
    const bool normal =
        raw_expected == MC_WORLD_STORAGE_OK && owner_expected == MC_WORLD_STORAGE_OK;
    CHECK(completed == normal && frame->entered && !frame->active && frame->completed == normal &&
          frame->status == raw_expected && raw == raw_expected && frame->detail.bytecode == pc &&
          owned->ownership_status == owner_expected &&
          frame->native_stopped == (owner_expected != MC_WORLD_STORAGE_OK) &&
          !owned->selection_ready && !owned->root_pending &&
          mc_world_runtime_schedule_class_get(f->classes[0], &after) == MC_WORLD_STORAGE_OK &&
          after.initialized &&
          after.next == mc_java_i64_add(before.initialized ? before.next : 0, 1) &&
          f->provider[0].cache->count == cache_count && world->active_calls == 0 &&
          mc_test_java_random_equal(world->fields.r, &random));
    if (!normal) {
        mc_scheduled_tick_fields fields;
        CHECK(frame->record != NULL &&
              mc_scheduled_tick_fields_get(frame->record, &fields) == MC_SCHEDULED_TICK_OK &&
              fields.x == x && fields.y == y && fields.z == z && fields.block == block &&
              fields.sequence == (before.initialized ? before.next : 0));
        if (owner_expected != MC_WORLD_STORAGE_OK)
            CHECK(frame->lower.entered && frame->lower.completed &&
                  frame->lower.get_status == MC_CHUNK_ACQUIRE_OK &&
                  frame->detail.status == MC_WORLD_STORAGE_OK &&
                  frame->detail.cause.status == MC_WORLD_STORAGE_OK &&
                  owned->ownership_error.site.pc == 142 &&
                  strcmp(owned->ownership_error.site.class_name, "fd") == 0);
    } else
        CHECK(frame->record == NULL && frame->detail.status == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_schedule_execution_retire(&frame) == MC_WORLD_STORAGE_OK && frame == NULL &&
          mc_entity_tick_execution_pop_world(execution, &token, !normal) == MC_WORLD_STORAGE_OK);
    open = false;
    *owned = (mc_world_runtime_execution_result){0};
    CHECK(mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    if (frame != NULL && mc_world_schedule_execution_retire(&frame) != MC_WORLD_STORAGE_OK)
        return false;
    if (open && mc_entity_tick_execution_pop_world(execution, &token, true) != MC_WORLD_STORAGE_OK)
        return false;
    free(owned);
    free(execution);
    return ok;
}

static bool se_canonical(fixture *f) {
    bool ok = false, open = false;
    mc_entity_tick_execution *execution = calloc(1, sizeof(*execution));
    mc_world_runtime_execution_result *owned = calloc(1, sizeof(*owned));
    mc_world_schedule_execution *frame = calloc(1, sizeof(*frame));
    mc_entity_tick_execution_world_token token = {0};
    mc_world_schedule_class_state before, after;
    CHECK(execution != NULL && owned != NULL && frame != NULL &&
          mc_world_runtime_schedule_class_get(f->classes[0], &before) == MC_WORLD_STORAGE_OK);
    mc_entity_tick_execution_init(execution, NULL);
    mc_world_runtime_execution_result_init(owned);
    owned->math_failed = true;
    CHECK(mc_entity_tick_execution_push_world(execution, f->worlds[0], se_site, &token) ==
          MC_WORLD_STORAGE_OK);
    open = true;
    mc_world_schedule_execution_init(frame, execution, f->classes[0], se_code, token, se_site,
                                     owned);
    CHECK(owned->math_failed);
    mc_world_storage_status raw = MC_WORLD_STORAGE_LIMIT;
    CHECK(!mc_world_runtime_schedule_in_execution(frame, f->worlds[0], 8, 64, 8, 1, 3, &raw) &&
          raw == MC_WORLD_STORAGE_LIMIT && !frame->entered && frame->record == NULL &&
          owned->math_failed && owned->ownership_status == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_schedule_class_get(f->classes[0], &after) == MC_WORLD_STORAGE_OK &&
          after.initialized == before.initialized && after.next == before.next &&
          mc_world_schedule_execution_retire(&frame) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_pop_world(execution, &token, true) == MC_WORLD_STORAGE_OK);
    open = false;
    *owned = (mc_world_runtime_execution_result){0};
    CHECK(mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    if (frame != NULL && mc_world_schedule_execution_retire(&frame) != MC_WORLD_STORAGE_OK)
        return false;
    if (open && mc_entity_tick_execution_pop_world(execution, &token, true) != MC_WORLD_STORAGE_OK)
        return false;
    free(owned);
    free(execution);
    return ok;
}

static bool se_metadata(fixture *f) {
    bool ok = false, open = false;
    mc_world_runtime *world = f->worlds[0];
    mc_entity_tick_execution *execution = calloc(1, sizeof(*execution));
    mc_world_runtime_execution_result *owned = calloc(1, sizeof(*owned));
    mc_world_mutation_chunk_execution *frame = calloc(1, sizeof(*frame));
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    mc_world_mutation_execution_call *call = NULL;
    mc_entity_tick_execution_world_token token = {0};
    CHECK(execution != NULL && owned != NULL && frame != NULL && error != NULL);
    mc_entity_tick_execution_init(execution, NULL);
    mc_world_runtime_execution_result_init(owned);
    CHECK(mc_entity_tick_execution_push_world(execution, world, se_site, &token) ==
          MC_WORLD_STORAGE_OK);
    open = true;
    const int32_t values[] = {5, 7, 7, 23};
    const bool changes[] = {true, true, false, true};
    mc_world_chunk *chunk = mc_world_store_chunk_object(f->stores[0], f->center[0]);
    CHECK(chunk != NULL);
    for (size_t i = 0; i < COUNT(values); ++i) {
        mc_world_mutation_error_init(error);
        mc_world_mutation_chunk_execution_init(frame, execution, se_code, f->classes[0],
                                               f->registry[0], se_site, token, owned);
        bool changed = !changes[i];
        mc_world_storage_status raw = MC_WORLD_STORAGE_LIMIT;
        CHECK(mc_world_mutation_chunk_block_metadata_in_execution(
                  frame, world, f->stores[0], f->center[0], chunk, 9, 64, 9, 1, values[i], &changed,
                  error, &raw) &&
              changed == changes[i] && frame->source_returned && frame->changed == changes[i] &&
              frame->completed && frame->setter_body == MC_WORLD_MUTATION_CHUNK_ID_METADATA &&
              !frame->lease_held && !frame->binding_held && raw == MC_WORLD_STORAGE_OK &&
              f->center[0]->data->blocks[cell(9, 64, 9)] == 1);
        uint8_t metadata;
        CHECK(mc_chunk_nibbles_get(f->center[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 9, 64, 9,
                                   &metadata) &&
              metadata == ((uint32_t)values[i] & 15u) &&
              mc_world_mutation_chunk_children_retire(frame) == MC_WORLD_STORAGE_OK);
        if (changes[i])
            CHECK(frame->world_captures >= 3 && f->center[0]->dirty);
        else
            CHECK(frame->world_captures == 0);
    }
    /* Still Water is an actual ordinary neighbor of this notifying ID change.
     * Its same-body quiet conversion must preserve the nonzero level through
     * both metadata stores and the recursively retained FlowingWater addition. */
    f->center[0]->data->blocks[cell(7, 64, 8)] = 9;
    CHECK(mc_chunk_nibbles_set(f->center[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8, 3));
    const mc_world_mutation_execution_context context = {.execution = execution,
                                                         .code_classes = f->classes[0],
                                                         .code = se_code,
                                                         .result = owned,
                                                         .world = token};
    mc_world_mutation_error_init(error);
    bool changed = false;
    mc_world_storage_status raw = MC_WORLD_STORAGE_LIMIT;
    CHECK(mc_world_runtime_block_set_in_execution(&context, world, 8, 64, 8, 0, &changed, &call,
                                                  &raw, error) &&
          changed && call == NULL && raw == MC_WORLD_STORAGE_OK &&
          error->status == MC_WORLD_STORAGE_OK && owned->ownership_status == MC_WORLD_STORAGE_OK &&
          !owned->selection_ready && !world->fields.o &&
          f->center[0]->data->blocks[cell(7, 64, 8)] == 8 &&
          f->center[0]->data->blocks[cell(8, 64, 8)] == 0);
    uint8_t level;
    CHECK(mc_chunk_nibbles_get(f->center[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8,
                               &level) &&
          level == 3 && counts(world, 2, 3) && world->active_calls == 0 &&
          mc_entity_tick_execution_pop_world(execution, &token, false) == MC_WORLD_STORAGE_OK);
    open = false;
    *frame = (mc_world_mutation_chunk_execution){0};
    *owned = (mc_world_runtime_execution_result){0};
    CHECK(mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    if (call != NULL && (mc_world_mutation_execution_native_close(call) != MC_WORLD_STORAGE_OK ||
                         mc_world_mutation_execution_retire(&call) != MC_WORLD_STORAGE_OK))
        return false;
    if (frame != NULL && frame->entered &&
        (mc_world_mutation_chunk_native_close(frame) != MC_WORLD_STORAGE_OK ||
         mc_world_mutation_chunk_children_retire(frame) != MC_WORLD_STORAGE_OK))
        return false;
    if (open && mc_entity_tick_execution_pop_world(execution, &token, true) != MC_WORLD_STORAGE_OK)
        return false;
    free(frame);
    free(error);
    free(owned);
    free(execution);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_world_info *saved_info = NULL;
    mc_scheduled_ordered *saved_ordered = NULL;
    char root[8192];
    const uint16_t title[] = {'S', 'c', 'h'};
    if (argc != 3 || strcmp(argv[1], "retained-scheduling") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK &&
          open_store(f, 0, root) &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK);
    resources *r = &f->resources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = (stream){.bytes = language};
    r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, resource_ready};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK);
    r->registry = f->registry[0];
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          create_world(f, 0, 0, 16));
    mc_world_runtime *world = f->worlds[0];
    saved_info = world->fields.x;
    saved_ordered = world->fields.E;
    CHECK(se_canonical(f) &&
          se_call(f, 8, 64, 8, 12, 3, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_OK, 0) &&
          counts(world, 0, 0) && f->provider[0].cache->count == 0 && r->opens == 0 &&
          load_square(f, 0));
    CHECK(se_call(f, 8, 64, 8, 12, 3, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_OK, 0) &&
          counts(world, 1, 1));
    mc_scheduled_tick *first = NULL;
    mc_scheduled_tick_fields fields;
    int64_t time;
    CHECK(mc_scheduled_ordered_first(world->fields.E, &first) == MC_SCHEDULED_ORDERED_OK);
    const bool read = mc_scheduled_tick_fields_get(first, &fields) == MC_SCHEDULED_TICK_OK;
    mc_scheduled_tick_release(first);
    CHECK(read && mc_world_info_time_get(world->fields.x, &time) == MC_WORLD_INFO_OK &&
          fields.x == 8 && fields.y == 64 && fields.z == 8 && fields.block == 12 &&
          fields.due_time == mc_java_i64_add(time, 3) && fields.sequence == 1);
    CHECK(se_call(f, 8, 64, 8, 12, 99, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_OK, 0) &&
          counts(world, 1, 1));
    world->fields.x = NULL;
    CHECK(
        se_call(f, 8, 64, 8, 12, 3, MC_WORLD_STORAGE_UNSUPPORTED_STATE, MC_WORLD_STORAGE_OK, 191) &&
        counts(world, 1, 1) &&
        se_call(f, 8, 64, 8, 0, INT32_MAX, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_OK, 0) &&
        counts(world, 2, 2));
    world->fields.x = saved_info;
    world->fields.E = NULL;
    CHECK(
        se_call(f, 9, 64, 8, 13, 3, MC_WORLD_STORAGE_UNSUPPORTED_STATE, MC_WORLD_STORAGE_OK, 231));
    world->fields.E = saved_ordered;
    CHECK(counts(world, 2, 3));
    world->fields.a = true;
    CHECK(
        se_call(f, 40, 64, 8, 1, 3, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_OK, 0) &&
        se_call(f, 8, 64, 8, 2, 3, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_OK, 0) &&
        se_call(f, 9, 64, 8, 0, 3, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_OK, 0) && r->opens == 0 &&
        se_call(f, 8, 64, 8, 1, 3, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_UNSUPPORTED_STATE, 142) &&
        r->opens == 3 && counts(world, 2, 3) && !r->bad);
    world->fields.a = false;
    CHECK(se_metadata(f));
    result = 0;
cleanup:
    if (f != NULL && f->worlds[0] != NULL) {
        if (saved_info != NULL)
            f->worlds[0]->fields.x = saved_info;
        if (saved_ordered != NULL)
            f->worlds[0]->fields.E = saved_ordered;
        f->worlds[0]->fields.a = false;
    }
    if (!cleanup_fixture(f)) {
        fputs("retained scheduling cleanup refused\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u retained scheduling checks\n", checks) < 0)
        result = 1;
    return result;
}
