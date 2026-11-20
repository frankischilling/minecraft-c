/* Genuine Registry initialization failures and passive value diagnostics.
 * Real missing class-loader resources cause the failed/partial namespace;
 * no phase, Source class, initializer outcome or returned getter is replaced. */
int mc_dispenser_registry_unused_boxes_main(int, char **);
#define MC_COLLISION_BOXES_MAIN mc_dispenser_registry_unused_boxes_main
#include "../collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "block/registry/runtime_internal.h"
#include "item/render/internal.h"
#include "mc/world/runtime/mutations.h"
#include "world/runtime/mutation/dispenser/dispenser_internal.h"
#include "world/runtime/mutation/mutation_internal.h"

static mc_registry_status registry_missing_stream(void *context, const char *name,
                                                  mc_java_properties_input *out) {
    resources *r = context;
    if (name == NULL || out == NULL || strcmp(name, "/lang/en_US.lang") != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++r->opens;
    return MC_REGISTRY_NULL_REFERENCE; /* Actual missing resource, not a callee stub. */
}

static bool registry_world(fixture *f, const char *root, size_t slot, bool cold) {
    bool ok = false;
    const uint16_t title[] = {'R', 'e', 'g', 'i', 's', 't', 'r', 'y'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = cold ? (mc_registry_resources){r, registry_missing_stream, NULL, NULL}
                  : (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK);
    if (!cold)
        CHECK(mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(boxes_open_store(f, slot, root) && boxes_create_world(f, slot));
    ok = true;
cleanup:
    return ok;
}

static bool defining_namespace(fixture *f) {
    bool ok = false;
    mc_registry_classes *defining = NULL, *native_owner = NULL;
    resources missing = {0};
    missing.api = (mc_registry_resources){&missing, registry_missing_stream, NULL, NULL};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         &missing.api};
    mc_registry_error first = {0}, cached = {0};
    mc_registry_block_view published = {0};
    mc_world_mutation_error *error = malloc(sizeof(*error));
    mc_world_registry_failure saved = {0};
    mc_registry_error saved_cause = {0};
    CHECK(error != NULL &&
          mc_registry_classes_allocate(&options, &defining, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_blocks(defining, &first) == MC_REGISTRY_NULL_REFERENCE &&
          mc_registry_classes_inspect_blocks(defining, &published, NULL) == MC_REGISTRY_OK &&
          published.phase == MC_REGISTRY_FAILED && published.m != NULL && published.m[23] != NULL &&
          published.m[23]->classes == defining && f->registry[0] != defining &&
          f->registry[0]->blocks_phase == MC_REGISTRY_READY && !missing.bad && missing.opens == 1);
    mc_world_mutation_error_init(error);
    CHECK(
        !mc_world_mutation_registry_fail_in(f->worlds[0], defining, error, &first, first.message) &&
        error->registry_failure.kind == MC_WORLD_REGISTRY_FAILURE_SOURCE_ERROR &&
        strcmp(error->registry_failure.exception_class, "java/lang/ExceptionInInitializerError") ==
            0 &&
        mc_world_registry_failure_matches(&error->registry_failure, &error->registry));
    /* The healthy evaluated World cannot classify a foreign initializer's
     * immutable cause. The producer value must carry that distinction. */
    mc_item_render_throwable wrong = MC_ITEM_RENDER_SOURCE_ERROR;
    const char *wrong_name = NULL;
    mc_item_render_registry_classification(f->registry[0], &error->registry, &wrong, &wrong_name);
    CHECK(wrong == MC_ITEM_RENDER_NO_THROWABLE && wrong_name == NULL);
    boxes_air(f, 0);
    CHECK(boxes_cell(f, 0, 8, 64, 8, 23, 0) &&
          mc_world_runtime_call_enter(f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    mc_world_mutation_error_init(error);
    const bool oriented =
        mc_world_mutation_dispenser_orient(f->worlds[0], published.m[23], 8, 64, 8, error, NULL);
    mc_world_runtime_call_leave(f->worlds[0]);
    CHECK(!oriented && error->registry.status == MC_REGISTRY_FAILED_INITIALIZATION &&
          error->registry_failure.kind == MC_WORLD_REGISTRY_FAILURE_SOURCE_ERROR &&
          strcmp(error->registry_failure.exception_class, "java/lang/NoClassDefFoundError") == 0 &&
          mc_world_registry_failure_matches(&error->registry_failure, &error->registry) &&
          strcmp(error->hook_class, "xq") == 0 && error->hook_bytecode == 15 &&
          error->cause_hook_bytecode == 59 && f->registry[0]->blocks_phase == MC_REGISTRY_READY);
    saved = error->registry_failure;
    saved_cause = error->registry;
    /* These are explicit diagnostic corruption controls, never Source input
     * repairs. A stale classification cannot be joined to a changed cause. */
    mc_registry_error changed = saved_cause;
    changed.block_id ^= 1;
    CHECK(!mc_world_registry_failure_matches(&saved, &changed));
    changed = saved_cause;
    changed.format_error.offset ^= 1u;
    CHECK(!mc_world_registry_failure_matches(&saved, &changed));
    CHECK(!mc_world_mutation_registry_fail(f->worlds[0], error, &first, first.message) &&
          error->registry_failure.kind == MC_WORLD_REGISTRY_FAILURE_UNCLASSIFIED &&
          !mc_world_registry_failure_matches(&error->registry_failure, &error->registry));
    CHECK(mc_registry_classes_initialize_blocks(defining, &cached) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    published = (mc_registry_block_view){0}; /* End local borrowed view, no Source store. */
    CHECK(mc_registry_classes_free(defining, NULL) == MC_REGISTRY_OK);
    defining = NULL;
    CHECK(mc_world_registry_failure_matches(&saved, &saved_cause) &&
          strcmp(saved.exception_class, "java/lang/NoClassDefFoundError") == 0);

    /* The reached public array getter supplies a real Source Exception, not
     * a failed initializer or a forged mc_registry_error status. */
    mc_registry_block_o_view opaque;
    bool element = true;
    CHECK(mc_registry_block_o_get(f->registry[0], &opaque, &first) == MC_REGISTRY_OK &&
          mc_registry_block_o_at(&opaque, -1, &element, &first) == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          element &&
          !mc_world_mutation_registry_fail_in(f->worlds[0], f->registry[0], error, &first,
                                              first.message) &&
          error->registry_failure.kind == MC_WORLD_REGISTRY_FAILURE_SOURCE_EXCEPTION &&
          strcmp(error->registry_failure.exception_class,
                 "java/lang/ArrayIndexOutOfBoundsException") == 0 &&
          mc_world_registry_failure_matches(&error->registry_failure, &error->registry));
    /* Aliasing the paired cause is an admitted diagnostic copy operation. */
    CHECK(!mc_world_mutation_registry_fail(f->worlds[0], error, &error->registry_failure.cause,
                                           first.message) &&
          error->registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          error->registry_failure.kind == MC_WORLD_REGISTRY_FAILURE_UNCLASSIFIED);

    /* A genuinely reached native bootstrap frontier is positively native,
     * not unclassified and not promoted to a Source Error by another owner. */
    mc_registry_options native_options = options;
    native_options.resources = NULL;
    CHECK(mc_registry_classes_allocate(&native_options, &native_owner, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_blocks(native_owner, &first) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          !mc_world_mutation_registry_fail_in(f->worlds[0], native_owner, error, &first,
                                              first.message) &&
          error->registry_failure.kind == MC_WORLD_REGISTRY_FAILURE_NATIVE &&
          error->registry_failure.exception_class == NULL &&
          mc_world_registry_failure_matches(&error->registry_failure, &error->registry));
    ok = true;
cleanup:
    free(error);
    if (mc_registry_classes_free(defining, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_free(native_owner, NULL) != MC_REGISTRY_OK)
        return false;
    return ok;
}

static bool evaluated_world_namespace(fixture *f) {
    bool ok = false;
    mc_world_mutation_error *error = malloc(sizeof(*error));
    CHECK(error != NULL && f->registry[0]->blocks_phase == MC_REGISTRY_COLD);
    boxes_air(f, 2);
    CHECK(boxes_cell(f, 2, 8, 64, 8, 1, 0));
    for (unsigned int attempt = 0; attempt < 2; ++attempt) {
        mc_world_mutation_error_init(error);
        CHECK(mc_world_runtime_metadata_set(f->worlds[2], 8, 64, 8, 3, error) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              error->wrapper_bytecode == 21 &&
              error->registry.status ==
                  (attempt == 0 ? MC_REGISTRY_NULL_REFERENCE : MC_REGISTRY_FAILED_INITIALIZATION) &&
              error->registry_failure.kind == MC_WORLD_REGISTRY_FAILURE_UNCLASSIFIED);
        mc_item_render_throwable kind = MC_ITEM_RENDER_NO_THROWABLE;
        const char *name = NULL;
        mc_item_render_registry_classification(mc_world_runtime_registry(f->worlds[2]),
                                               &error->registry, &kind, &name);
        CHECK(kind == MC_ITEM_RENDER_SOURCE_ERROR && name != NULL &&
              strcmp(name, attempt == 0 ? "java/lang/ExceptionInInitializerError"
                                        : "java/lang/NoClassDefFoundError") == 0);
    }
    uint8_t metadata = 0;
    CHECK(mc_chunk_nibbles_get(f->states[2]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8,
                               &metadata) &&
          metadata == 3 && f->worlds[2]->active_calls == 0 && !f->sources[0].bad &&
          f->sources[0].opens == 1);
    ok = true;
cleanup:
    free(error);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *healthy = NULL, *cold = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "world-dispenser-registry-failure") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    healthy = calloc(1, sizeof(*healthy));
    cold = calloc(1, sizeof(*cold));
    CHECK(healthy != NULL && cold != NULL && registry_world(healthy, root, 0, false) &&
          registry_world(cold, root, 2, true) && defining_namespace(healthy) &&
          evaluated_world_namespace(cold));
    result = 0;
cleanup:
    if (!cleanup_fixture(healthy) || !cleanup_fixture(cold)) {
        fputs("Dispenser registry failure cleanup refused\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("Dispenser registry failure CPU controls passed: %u\n", checks);
    return result;
}
