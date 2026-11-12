/* Candidate-only native operation-site controls; no original trace claim. */
#include "block/registry/rendering_internal.h"
#include "texture_fixture.inc"

int main(void) {
    int result = 1;
    fixture f = {0};
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_registry_error registry_error = poison_error();
    const mc_block *captured = NULL;
    mc_item_render_error error;
    mc_block *torch = NULL, *dust = NULL;
    int32_t output = INT32_C(-123456789);
    bool returning = false, direct = false, null_call = false, bypass = false, invalid = false;
#ifdef _WIN32
    CHECK(_setmode(_fileno(stdout), _O_BINARY) != -1);
#endif
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_materials_create(&materials));
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = profile,
                                         .materials = materials,
                                         .max_allocation_bytes = SIZE_MAX};
    CHECK(allocate_fixture(&f, options) == MC_REGISTRY_OK && fflush(stdout) == 0 &&
          mc_registry_classes_initialize_blocks(f.owner, &registry_error) == MC_REGISTRY_OK);
    CHECK(!f.bad && f.opens == 3 && f.closes == 1 && f.busy_checks == 9);
    CHECK(mc_registry_block_get(f.owner, 75, &captured, &registry_error) == MC_REGISTRY_OK &&
          captured != NULL);
    torch = (mc_block *)(void *)captured;
    CHECK(mc_registry_block_static_get(f.owner, "aw", &captured, &registry_error) ==
              MC_REGISTRY_OK &&
          captured != NULL);
    dust = (mc_block *)(void *)captured;
    CHECK(torch->kind == MC_BLOCK_REDSTONE_TORCH && dust->kind == MC_BLOCK_REDSTONE);
    CHECK(mc_block_render_texture_metadata_detail(torch, 1, INT32_MIN, &output, &error) ==
          MC_REGISTRY_OK);
    returning = output == dust->fields.bm && error.returned && error.status == MC_REGISTRY_OK &&
                error.site.class_name != NULL && strcmp(error.site.class_name, "db") == 0 &&
                error.site.pc == 13 && error.ancestor_count == 1 &&
                strcmp(error.ancestors[0].class_name, "db") == 0 && error.ancestors[0].pc == 10 &&
                error.child.class_name != NULL && strcmp(error.child.class_name, "sm") == 0 &&
                error.child.pc == 4;
    CHECK(returning);
    CHECK(mc_block_render_texture_metadata_detail(dust, INT32_MIN, INT32_MAX, &output, &error) ==
          MC_REGISTRY_OK);
    direct = output == dust->fields.bm && error.returned && error.site.class_name != NULL &&
             strcmp(error.site.class_name, "sm") == 0 && error.site.pc == 4 &&
             error.ancestor_count == 0 && error.child.class_name == NULL;
    CHECK(direct);
    /* Declared native named-cell input, not a JVM final-field replacement.
     * Restore it before checking either outcome, including a failed call. */
    mc_block *const original_named = f.owner->blocks.named[54];
    f.owner->blocks.named[54] = NULL;
    output = INT32_C(-123456789);
    const mc_registry_status null_status =
        mc_block_render_texture_metadata_detail(torch, 1, 7, &output, &error);
    null_call = null_status == MC_REGISTRY_NULL_REFERENCE && output == INT32_C(-123456789) &&
                !error.returned && error.source_exception && !error.native_policy &&
                error.throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION &&
                error.exception_class != NULL &&
                strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
                error.site.class_name != NULL && strcmp(error.site.class_name, "db") == 0 &&
                error.site.pc == 10 && fresh(error.registry, MC_REGISTRY_OK) &&
                error.ancestor_count == 0 && error.child.class_name == NULL;
    const mc_registry_status bypass_status =
        mc_block_render_texture_metadata_detail(torch, INT32_MIN, 7, &output, &error);
    bypass = bypass_status == MC_REGISTRY_OK && output == torch->fields.bm && error.returned &&
             error.site.class_name != NULL && strcmp(error.site.class_name, "db") == 0 &&
             error.site.pc == 20 && error.child.class_name != NULL &&
             strcmp(error.child.class_name, "uu") == 0 && error.child.pc == 2 &&
             f.owner->blocks.named[54] == NULL;
    f.owner->blocks.named[54] = original_named;
    CHECK(null_call && bypass && original_named == dust);
    output = INT32_C(-123456789);
    const mc_registry_status invalid_status =
        mc_block_render_texture_metadata_detail(NULL, 1, 7, &output, &error);
    invalid = invalid_status == MC_REGISTRY_INVALID_ARGUMENT && output == INT32_C(-123456789) &&
              !error.returned && !error.source_exception && error.native_policy &&
              error.throwable == MC_ITEM_RENDER_NO_THROWABLE && error.site.class_name == NULL;
    CHECK(invalid && !f.bad && f.owner->entered == 0 && f.owner->initializer == NULL);
    CHECK(retire(&f));
    mc_materials_destroy(materials);
    materials = NULL;
    mc_java_random_profile_free(profile);
    profile = NULL;
    CHECK(printf("DB_DETAIL_FINISH\t{\"checks\":%u,\"returning\":true,\"direct_sm\":true,"
                 "\"null_aw\":true,\"bypass\":true,\"invalid\":true,\"retired\":true}\n",
                 checks) > 0);
    CHECK(fflush(stdout) == 0 && !ferror(stdout));
    result = 0;
cleanup:
    if (!retirement_failed && f.owner != NULL && !retire(&f))
        result = 1;
    if (!retirement_failed) {
        mc_materials_destroy(materials);
        mc_java_random_profile_free(profile);
    }
    return result;
}
