/* Prospective NATIVE owner component, never an acquisition/projection join.
 * Every nonnull diagnostic below is returned by ONE real source World query
 * over an authored MCR zlib record with non-Compound NBT. No synthetic failure
 * frame, pointer repair, ambient-error reconstruction or callback replacement.
 * Imported main is declared/renamed only and is NEVER executed. */
int nl_unused_source_fixture_main(int, char **);
#define main nl_unused_source_fixture_main
#include "../test_source_world_construction.c"
#undef main

#include "entity/tick/execution_internal.h"

typedef struct nl_fixture {
    fixture source;
    mc_world_runtime *worlds[3];
    mc_world_store *stores[3];
    mc_entity_tick_execution execution;
    mc_world_query_error query[3];
} nl_fixture;

static mc_entity_tick_execution_site nl_site(int pc) {
    return (mc_entity_tick_execution_site){"native.loader.owner", "control", "()V", pc};
}

static bool nl_token_same(const mc_entity_tick_execution_world_token *a,
                          const mc_entity_tick_execution_world_token *b) {
    return a->owner == b->owner && a->slot == b->slot && a->generation == b->generation;
}

static int nl_setup(nl_fixture *s, const char *root) {
    fixture *f = &s->source;
    CHECK(mc_math_trig_init(&trig));
    CHECK(mc_java_files_create(NULL, &f->files, &f->source.file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_logging_create(NULL, &f->logging, &f->source.logging) == MC_JAVA_LOGGING_OK);
    CHECK(mc_world_source_regions_create(f->files, NULL, &f->regions, &f->region) ==
          MC_WORLD_SOURCE_OK);
    CHECK(mc_world_store_source_classes_create(f->files, f->logging, NULL, &f->handlers,
                                               &f->source) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_store_source_classes_bind_regions(f->handlers, f->regions, &f->source) ==
          MC_WORLD_SOURCE_OK);
    mc_world_info_name *path = text(root);
    CHECK(path != NULL);
    CHECK(mc_java_file_allocate(f->files, &f->root, &f->source.file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_string(f->root, path, &f->source.file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(path);
    const uint16_t display[] = {'D', 0xe9, 'm', 'o', 0xd800};
    CHECK(mc_world_info_name_copy((mc_nbt_string){display, 5}, SIZE_MAX, &f->display) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes,
                                          &f->error) == MC_WORLD_STORAGE_OK);
    const char *const names[] = {"primary-a", "primary-b", "compat"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        mc_world_info_name *name = text(names[i]);
        CHECK(name != NULL);
        CHECK(mc_world_store_source_allocate(f->handlers, &s->stores[i], &f->source) ==
              MC_WORLD_SOURCE_OK);
        CHECK(mc_world_store_source_construct(s->stores[i], f->root, name, false, &f->source) ==
              MC_WORLD_SOURCE_OK);
        mc_world_info_name_release(name);
        mc_world_runtime_construction_limits limits =
            mc_world_runtime_default_construction_limits();
        limits.max_provider_chunks = 4;
        CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes,
                                              &limits, &s->worlds[i],
                                              &f->error) == MC_WORLD_STORAGE_OK);
        const mc_world_runtime_world_arguments args = {
            .store = s->stores[i], .name = f->display, .seed = 777};
        CHECK(mc_world_runtime_construct_world(s->worlds[i], &args, &f->error) ==
              MC_WORLD_STORAGE_OK);
        mc_world_runtime_source_construction_state source;
        mc_world_runtime_construction state;
        CHECK(mc_world_runtime_source_construction_inspect(s->worlds[i], &source) &&
              source.loader != NULL && source.store == s->stores[i] && source.store_held &&
              mc_world_runtime_construction_get(s->worlds[i], &state) == MC_WORLD_STORAGE_OK &&
              state.phase == MC_WORLD_RUNTIME_READY);
    }
    mc_entity_tick_execution_init(&s->execution, NULL);
    return 0;
}

static int nl_query_failure(nl_fixture *s, size_t i) {
    fixture *f = &s->source;
    mc_world_runtime_source_construction_state source;
    CHECK(mc_world_runtime_source_construction_inspect(s->worlds[i], &source));
    int32_t value = -731;
    CHECK(mc_world_runtime_plant_light(s->worlds[i], 0, 64, 0, &value, &s->query[i]) ==
          MC_WORLD_STORAGE_INVALID_DATA);
    const mc_world_source_chunk_error *failure = s->query[i].source_chunk;
    CHECK(value == -731 && s->query[i].status == MC_WORLD_STORAGE_INVALID_DATA &&
          s->query[i].bytecode == 23 && s->query[i].provider_bytecode == 6 &&
          s->query[i].acquisition != MC_CHUNK_ACQUIRE_OK && failure != NULL &&
          failure->source.source_exception && failure->source.frame_count == 1 &&
          failure->source.frames[0].pc == 18 && failure->input.source_fault != MC_NBT_SOURCE_NONE);
    mc_world_source_chunk_loader_state loader;
    CHECK(mc_world_source_chunk_loader_inspect(source.loader, &loader) &&
          loader.phase == MC_WORLD_SOURCE_FAILED && loader.last_pc == 18 && loader.reader != NULL &&
          loader.failure == failure);
    /* last_error is NOT used to choose an owner selection. The actual returned
     * query frame above is the sole supplied current diagnostic. */
    CHECK(mc_world_runtime_free(s->worlds[i], &f->error) == MC_WORLD_STORAGE_BUSY &&
          loader.failure == failure);
    return 0;
}

/* Selected-arm comparison for the actually reached ld/as18 non-Compound-root
 * failure. Compare typed active source/input/storage fields, not padding or
 * unrelated inactive file/output/Registry diagnostics. */
static bool nl_site_same(const mc_world_source_site *a, const mc_world_source_site *b) {
    return a->class_name == b->class_name && a->method == b->method &&
           a->descriptor == b->descriptor && a->pc == b->pc;
}

static bool nl_input_same(const mc_nbt_stream_error *a, const mc_nbt_stream_error *b) {
    return a->parse.status == b->parse.status && a->parse.offset == b->parse.offset &&
           a->source_fault == b->source_fault && a->source_class == b->source_class &&
           a->source_method == b->source_method && a->source_pc == b->source_pc &&
           a->utf_error_byte == b->utf_error_byte && a->utf_prefix_units == b->utf_prefix_units &&
           a->utf_partial_character == b->utf_partial_character &&
           a->input.transport_status == b->input.transport_status &&
           a->input.system_error == b->input.system_error &&
           a->input.source_exception == b->input.source_exception &&
           a->input.operation == b->input.operation &&
           a->input.native_written_on_error == b->input.native_written_on_error &&
           strncmp(a->input.message, b->input.message, sizeof(a->input.message)) == 0;
}

static bool nl_error_same(const mc_world_source_chunk_error *a,
                          const mc_world_source_chunk_error *b) {
    if (a == NULL || b == NULL || a->source.frame_count != b->source.frame_count ||
        a->source.frame_count != 1 || a->source.frames[0].pc != 18 ||
        a->source.status != b->source.status || a->source.cause_status != b->source.cause_status ||
        a->source.operation != b->source.operation ||
        a->source.exception_class != b->source.exception_class ||
        a->source.native_policy != b->source.native_policy ||
        a->source.source_exception != b->source.source_exception ||
        a->source.caught_io != b->source.caught_io ||
        a->source.caught_exception != b->source.caught_exception ||
        a->source.diagnostic_frontier != b->source.diagnostic_frontier ||
        a->source.session_mismatch != b->source.session_mismatch ||
        !nl_site_same(&a->source.cause_site, &b->source.cause_site) ||
        !nl_site_same(&a->source.prior_site, &b->source.prior_site) ||
        !nl_site_same(&a->source.frames[0], &b->source.frames[0]) ||
        !nl_input_same(&a->source.input, &b->source.input) || !nl_input_same(&a->input, &b->input))
        return false;
    return a->storage.status == b->storage.status && a->storage.operation == b->storage.operation &&
           a->storage.system_error == b->storage.system_error &&
           a->compression.status == b->compression.status &&
           a->compression.operation == b->compression.operation &&
           a->compression.source_exception == b->compression.source_exception &&
           a->compression.source_class == b->compression.source_class &&
           a->compression.source_method == b->compression.source_method &&
           a->compression.source_pc == b->compression.source_pc;
}

static int nl_tokens(nl_fixture *s) {
    mc_entity_tick_execution *e = &s->execution;
    mc_world_runtime *a = s->worlds[0], *b = s->worlds[1];
    const size_t ab = a->persistent_bindings, bb = b->persistent_bindings;
    mc_entity_tick_execution_world_token outer;
    mc_entity_tick_execution_loader_attempt attempt;
    mc_entity_tick_execution_loader_selection selection;
    mc_entity_tick_execution_loader_view view;
    CHECK(mc_entity_tick_execution_push_world(e, a, nl_site(1), &outer) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_loader_begin(e, &outer, &attempt) == MC_WORLD_STORAGE_OK &&
          a->persistent_bindings == ab + 1u && b->persistent_bindings == bb);
    mc_entity_tick_execution_loader_attempt output = attempt;
    CHECK(mc_entity_tick_execution_loader_begin(e, &outer, &output) == MC_WORLD_STORAGE_BUSY &&
          output.slot == attempt.slot && output.generation == attempt.generation &&
          nl_token_same(&output.world, &attempt.world));
    CHECK(mc_entity_tick_execution_pop_world(e, &outer, true) == MC_WORLD_STORAGE_BUSY &&
          e->current_lease == outer.slot && e->world_count == 1 && e->lease_count == 1);
    /* Exactly ONE source query executes for this attempt. There is no getter
     * after its failure to manufacture an outer/NONE acquisition outcome. */
    CHECK(nl_query_failure(s, 0) == 0);
    const mc_world_source_chunk_error *actual = s->query[0].source_chunk;
    CHECK(mc_entity_tick_execution_loader_finish(e, &attempt, s->query[0].acquisition, actual,
                                                 &selection) == MC_WORLD_STORAGE_OK &&
          e->phase == MC_ENTITY_TICK_EXECUTION_FAILED && e->first.status == MC_WORLD_STORAGE_OK &&
          e->capture.status == MC_WORLD_STORAGE_OK && e->loader_count == 1 &&
          mc_entity_tick_execution_loader_read(e, &selection, &view) &&
          view.status == s->query[0].acquisition && view.failure != actual &&
          view.failure == &e->loaders[attempt.slot] && nl_error_same(view.failure, actual));
    const mc_entity_tick_execution_loader_view sentinel = {MC_CHUNK_ACQUIRE_EMPTY, actual};
    mc_entity_tick_execution_loader_selection wrong = selection;
    ++wrong.generation;
    view = sentinel;
    CHECK(!mc_entity_tick_execution_loader_read(e, &wrong, &view) &&
          view.status == sentinel.status && view.failure == sentinel.failure);
    wrong = selection;
    wrong.status = MC_CHUNK_ACQUIRE_OK;
    CHECK(!mc_entity_tick_execution_loader_read(e, &wrong, &view) &&
          view.status == sentinel.status && view.failure == sentinel.failure &&
          mc_entity_tick_execution_pop_world(e, &outer, true) == MC_WORLD_STORAGE_OK &&
          !e->world_open && e->world_count == 1 && e->lease_count == 0 &&
          mc_entity_tick_execution_loader_read(e, &selection, &view) &&
          view.failure == &e->loaders[attempt.slot] && nl_error_same(view.failure, actual));
    output = attempt;
    CHECK(mc_entity_tick_execution_loader_begin(e, &outer, &output) == MC_WORLD_STORAGE_BUSY &&
          output.slot == attempt.slot && output.generation == attempt.generation &&
          nl_token_same(&output.world, &attempt.world));
    view = (mc_entity_tick_execution_loader_view){0};
    actual = NULL;
    s->query[0].source_chunk = NULL; /* Outer typed borrow only. */
    CHECK(mc_entity_tick_execution_release_receipts(e) == MC_WORLD_STORAGE_OK &&
          e->loader_count == 1 && mc_entity_tick_execution_retire(e) == MC_WORLD_STORAGE_OK &&
          e->loader_count == 0 && a->persistent_bindings == ab && b->persistent_bindings == bb);
    view = sentinel;
    CHECK(!mc_entity_tick_execution_loader_read(e, &selection, &view) &&
          view.status == sentinel.status && view.failure == sentinel.failure);
    return 0;
}

static int nl_compat_push(nl_fixture *s, bool *expected, mc_world_storage_status *observed) {
    mc_entity_tick_execution *e = &s->execution;
    mc_world_runtime *a = s->worlds[2], *b = s->worlds[1];
    const size_t ab = a->persistent_bindings, bb = b->persistent_bindings;
    mc_entity_tick_execution_init(e, NULL);
    mc_entity_tick_execution_world_token outer, current;
    CHECK(mc_entity_tick_execution_push_world(e, a, nl_site(10), &outer) == MC_WORLD_STORAGE_OK &&
          nl_query_failure(s, 2) == 0);
    const mc_world_source_chunk_error *actual = s->query[2].source_chunk;
    const mc_world_source_chunk_error *selected = actual;
    CHECK(mc_entity_tick_execution_copy_loader(e, &selected) == MC_WORLD_STORAGE_OK &&
          selected != actual && selected == &e->loaders[0] && e->loader_count == 1 &&
          e->phase == MC_ENTITY_TICK_EXECUTION_OPEN && nl_error_same(selected, actual));
    const mc_entity_tick_execution_world_token sentinel = {NULL, 493, UINT64_C(394)};
    mc_entity_tick_execution_world_token output = sentinel;
    const mc_entity_tick_execution_site first_site = e->site;
    unsigned char before[sizeof(e->world_slots)];
    memcpy(before, e->world_slots, sizeof(before));
    *observed = mc_entity_tick_execution_push_world(e, b, nl_site(11), &output);
    *expected =
        *observed == MC_WORLD_STORAGE_BUSY && nl_token_same(&output, &sentinel) &&
        e->world_count == 1 && e->lease_count == 1 && e->phase == MC_ENTITY_TICK_EXECUTION_OPEN &&
        e->world == a && e->world_open && mc_entity_tick_execution_current_world(e, &current) &&
        nl_token_same(&current, &outer) && memcmp(before, e->world_slots, sizeof(before)) == 0 &&
        e->site.class_name == first_site.class_name && e->site.method == first_site.method &&
        e->site.descriptor == first_site.descriptor && e->site.pc == first_site.pc &&
        a->persistent_bindings == ab + 1u && b->persistent_bindings == bb &&
        e->first.status == MC_WORLD_STORAGE_OK && e->capture.status == MC_WORLD_STORAGE_OK &&
        s->query[2].source_chunk == actual && s->query[2].status == MC_WORLD_STORAGE_INVALID_DATA &&
        nl_error_same(selected, actual);
    /* Actual wrong admission has a genuine new binding: unwind its exact
     * returned token, then A, by normal checked APIs BEFORE asserting red. */
    if (*observed == MC_WORLD_STORAGE_OK)
        CHECK(mc_entity_tick_execution_pop_world(e, &output, true) == MC_WORLD_STORAGE_OK);
    selected = actual = NULL;
    s->query[2].source_chunk = NULL;
    CHECK(mc_entity_tick_execution_pop_world(e, &outer, true) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(e) == MC_WORLD_STORAGE_OK &&
          a->persistent_bindings == ab && b->persistent_bindings == bb);
    return 0;
}

static int nl_cleanup(nl_fixture *s) {
    fixture *f = &s->source;
    /* Parent source constructors/Store cleanup are the genuine original
     * fixture's checked sequence, after all actual owner bindings are gone. */
    for (size_t i = 0; i < sizeof(s->worlds) / sizeof(s->worlds[0]); ++i) {
        s->query[i].source_chunk = NULL;
        CHECK(mc_world_runtime_retire_world(s->worlds[i], &f->error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_free(s->worlds[i], &f->error) == MC_WORLD_STORAGE_OK);
        s->worlds[i] = NULL;
        mc_world_source_state handler;
        CHECK(mc_world_store_source_inspect(s->stores[i], &handler) && handler.holds == 1);
        CHECK(mc_world_store_source_release(&s->stores[i], &f->source) == MC_WORLD_SOURCE_OK &&
              s->stores[i] == NULL);
    }
    CHECK(mc_world_runtime_classes_free(f->classes, &f->error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_source_classes_retire(&f->handlers, &f->source) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_source_regions_retire(&f->regions, &f->region) == MC_WORLD_SOURCE_OK);
    CHECK(mc_java_logging_retire(&f->logging, &f->source.logging) == MC_JAVA_LOGGING_OK);
    CHECK(mc_java_file_release(&f->root, &f->source.file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_files_release(&f->files, &f->source.file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(f->display);
    mc_java_random_profile_free(f->profile);
    free(s);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    nl_fixture *s = calloc(1, sizeof(*s));
    CHECK(s != NULL && nl_setup(s, argv[1]) == 0);
    puts("NESTED_LOADER_CASE id=0");
    CHECK(nl_tokens(s) == 0);
    puts("NESTED_LOADER_CASE id=1");
    bool guard_expected = false;
    mc_world_storage_status observed = MC_WORLD_STORAGE_INVALID_ARGUMENT;
    CHECK(nl_compat_push(s, &guard_expected, &observed) == 0);
    CHECK(nl_cleanup(s) == 0);
    if (!guard_expected) {
        fprintf(stderr,
                "Nested loader guard assertion AFTER checked source cleanup: actual=%d "
                "expectedBUSY=%d\n",
                observed, MC_WORLD_STORAGE_BUSY);
        return 1;
    }
    printf("PASS: %u actual nested source-loader owner checks;2 groups\n", checks);
    return 0;
}
