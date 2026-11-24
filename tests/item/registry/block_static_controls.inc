#include "../../block/registry/static_index_cases.h"

#include <inttypes.h>

/* mc_registry_fail builds native_error, then copies the retained Block
 * driver PC. Unknown runtime names therefore differ from diagnostic-view
 * argument errors, whose independent result() record has initializer_pc=-1.
 * Check every canonical field; only this real owner PC supplies that field. */
static bool block_static_unknown_error(const mc_registry_classes *owner, mc_registry_error actual) {
    const mc_registry_error expected = {.status = MC_REGISTRY_INVALID_ARGUMENT,
                                        .message = "unknown Block static operand",
                                        .initializer_pc = owner->block_initializer_pc,
                                        .block_id = -1,
                                        .constructor_class = NULL,
                                        .constructor_pc = -1,
                                        .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                                        .active_initializer_class = NULL,
                                        .active_initializer_pc = -1,
                                        .item_argument = -1,
                                        .format_error = {.status = MC_JAVA_FORMAT_OK,
                                                         .offset = 0,
                                                         .length = 0,
                                                         .value = 0,
                                                         .flags = 0,
                                                         .conversion = 0,
                                                         .date_time = false}};
    const bool matched = same_error(actual, expected);
    if (!matched) {
        /* Failure-only ordered field diagnostics: no Source call or replay. */
        fprintf(stderr,
                "BLOCK_STATIC_UNKNOWN_ERROR status=%d/%d message=%s/%s "
                "initializer-pc=%d/%d block-id=%d/%d constructor=%s/%s "
                "constructor-pc=%d/%d profile=%d/%d active=%s/%s "
                "active-pc=%d/%d item-argument=%d/%d "
                "format-status=%d/%d offset=%zu/%zu length=%zu/%zu "
                "value=%" PRId32 "/%" PRId32 " flags=%u/%u conversion=%u/%u date=%d/%d\n",
                (int)actual.status, (int)expected.status,
                actual.message == NULL ? "NULL" : actual.message, expected.message,
                actual.initializer_pc, expected.initializer_pc, actual.block_id, expected.block_id,
                actual.constructor_class == NULL ? "NULL" : actual.constructor_class, "NULL",
                actual.constructor_pc, expected.constructor_pc, (int)actual.profile_status,
                (int)expected.profile_status,
                actual.active_initializer_class == NULL ? "NULL" : actual.active_initializer_class,
                "NULL", actual.active_initializer_pc, expected.active_initializer_pc,
                actual.item_argument, expected.item_argument, (int)actual.format_error.status,
                (int)expected.format_error.status, actual.format_error.offset,
                expected.format_error.offset, actual.format_error.length,
                expected.format_error.length, actual.format_error.value,
                expected.format_error.value, (unsigned int)actual.format_error.flags,
                (unsigned int)expected.format_error.flags,
                (unsigned int)actual.format_error.conversion,
                (unsigned int)expected.format_error.conversion, (int)actual.format_error.date_time,
                (int)expected.format_error.date_time);
    }
    return matched;
}

/* Reuse the genuine fixture, streams, diagnostic comparison and retirement
 * from test_runtime.c. No READY/entered/profile/owner counter is assigned. */
static int block_static_semantics(mc_registry_options options, bool missing) {
    int result = 1;
    fixture f = {.missing = missing};
    mc_block anchor = {0};
    const mc_block *out = &anchor;
    mc_registry_error error = poison_error();
    const uint64_t before_random = uniquifier(options.random_profile);
    CHECK(allocate_fixture(&f, options) == MC_REGISTRY_OK);
    const size_t cold_bytes = f.owner->allocation_bytes;
    unsigned char cold_owner[sizeof(*f.owner)];
    memcpy(cold_owner, f.owner, sizeof(cold_owner));
    for (size_t i = 0; i < COUNT(block_static_names); ++i) {
        out = &anchor;
        error = poison_error();
        CHECK(mc_registry_classes_inspect_block_static(f.owner, block_static_names[i], &out,
                                                       &error) == MC_REGISTRY_OK &&
              out == NULL && fresh(error, MC_REGISTRY_OK));
    }
    CHECK(memcmp(cold_owner, f.owner, sizeof(cold_owner)) == 0 && f.opens == 0 &&
          uniquifier(options.random_profile) == before_random);
    out = &anchor;
    CHECK(mc_registry_block_static_get(f.owner, NULL, &out, &error) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          out == &anchor && fresh(error, MC_REGISTRY_INVALID_ARGUMENT) &&
          strcmp(error.message, "Block runtime static arguments") == 0);
    CHECK(mc_registry_block_static_get(f.owner, "u", NULL, &error) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          fresh(error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_block_static_get(NULL, "u", &out, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          out == &anchor && fresh(error, MC_REGISTRY_INVALID_ARGUMENT) &&
          strcmp(error.message, "registry owner") == 0);
    CHECK(f.owner->blocks_phase == MC_REGISTRY_COLD && f.owner->allocation_bytes == cold_bytes &&
          f.opens == 0 && uniquifier(options.random_profile) == before_random);

    /* Authored native RAW metadata owned by this Registry arena. It bypasses
     * Java new/class initialization only as an explicit diagnostic input. */
    mc_block *raw = mc_registry_allocate(f.owner, sizeof(*raw));
    CHECK(raw != NULL);
    raw->classes = f.owner;
    raw->kind = MC_BLOCK_BASE;
    raw->phase = MC_BLOCK_RAW;
    raw->requested_id = raw->constructor_pc = -1;
    mc_block *const cold_cell = f.owner->blocks.named[95];
    f.owner->blocks.named[95] = raw;
    out = &anchor;
    const mc_registry_status raw_status =
        mc_registry_classes_inspect_block_static(f.owner, "bl", &out, &error);
    f.owner->blocks.named[95] = cold_cell;
    CHECK(raw_status == MC_REGISTRY_OK && out == raw && raw->phase == MC_BLOCK_RAW &&
          fresh(error, MC_REGISTRY_OK) && f.owner->blocks_phase == MC_REGISTRY_COLD &&
          f.opens == 0 && uniquifier(options.random_profile) == before_random);

    /* Unknown selection must still run actual Block initialization FIRST.
     * The missing-resource arm must propagate its first failure, not return
     * an early name error. Both arms use the existing real byte streams. */
    out = &anchor;
    error = poison_error();
    const mc_registry_status first_status =
        mc_registry_block_static_get(f.owner, "missing", &out, &error);
    CHECK(out == &anchor && !f.bad && f.owner->entered == 0 && f.owner->initializer == NULL &&
          uniquifier(options.random_profile) == advanced(before_random, 5));
    if (missing) {
        CHECK(first_status == MC_REGISTRY_NULL_REFERENCE && f.opens == 1 && f.busy_checks == 3 &&
              f.owner->blocks_phase == MC_REGISTRY_FAILED &&
              same_error(error, f.owner->block_failure));
        const mc_registry_error first = error;
        mc_registry_error retry = first;
        retry.status = MC_REGISTRY_FAILED_INITIALIZATION;
        const size_t failed_bytes = f.owner->allocation_bytes;
        const uint64_t failed_random = uniquifier(options.random_profile);
        for (size_t i = 0; i < COUNT(block_static_names); ++i) {
            out = &anchor;
            error = poison_error();
            CHECK(mc_registry_block_static_get(f.owner, block_static_names[i], &out, &error) ==
                      MC_REGISTRY_FAILED_INITIALIZATION &&
                  out == &anchor && same_error(error, retry));
            const mc_registry_error owner_error = f.owner->last_error;
            CHECK(mc_registry_classes_inspect_block_static(f.owner, block_static_names[i], &out,
                                                           &error) == MC_REGISTRY_OK &&
                  out == f.owner->blocks.named[i] && out != NULL && fresh(error, MC_REGISTRY_OK) &&
                  same_error(owner_error, f.owner->last_error));
        }
        for (size_t i = 0; i < COUNT(block_static_invalid); ++i) {
            out = &anchor;
            error = poison_error();
            CHECK(mc_registry_block_static_get(f.owner, block_static_invalid[i], &out, &error) ==
                      MC_REGISTRY_FAILED_INITIALIZATION &&
                  out == &anchor && same_error(error, retry));
            const mc_registry_error owner_error = f.owner->last_error;
            CHECK(mc_registry_classes_inspect_block_static(f.owner, block_static_invalid[i], &out,
                                                           &error) ==
                      MC_REGISTRY_INVALID_ARGUMENT &&
                  out == &anchor && fresh(error, MC_REGISTRY_INVALID_ARGUMENT) &&
                  strcmp(error.message, "unknown Block static name") == 0 &&
                  same_error(owner_error, f.owner->last_error));
        }
        CHECK(f.owner->allocation_bytes == failed_bytes && f.opens == 1 &&
              uniquifier(options.random_profile) == failed_random &&
              same_error(f.owner->block_failure, first));
    } else {
        CHECK(first_status == MC_REGISTRY_INVALID_ARGUMENT &&
              block_static_unknown_error(f.owner, error) &&
              same_error(error, f.owner->last_error) &&
              f.owner->blocks_phase == MC_REGISTRY_READY && f.opens == 3 && f.closes == 1 &&
              f.busy_checks == 9);
        const size_t ready_bytes = f.owner->allocation_bytes;
        const uint64_t ready_random = uniquifier(options.random_profile);
        for (size_t i = 0; i < COUNT(block_static_names); ++i) {
            mc_block *const original = f.owner->blocks.named[i];
            mc_block *const other = f.owner->blocks.named[(i + 1) % COUNT(block_static_names)];
            out = &anchor;
            CHECK(mc_registry_block_static_get(f.owner, block_static_names[i], &out, &error) ==
                      MC_REGISTRY_OK &&
                  out == original && original != NULL && original->phase == MC_BLOCK_CONSTRUCTED &&
                  fresh(error, MC_REGISTRY_OK));
            /* Restore each authored live cell before assertions/cleanup. */
            f.owner->blocks.named[i] = other;
            const mc_registry_status alias_status =
                mc_registry_block_static_get(f.owner, block_static_names[i], &out, &error);
            const mc_block *alias = out;
            const mc_registry_error alias_error = error;
            const mc_registry_status alias_view_status = mc_registry_classes_inspect_block_static(
                f.owner, block_static_names[i], &out, &error);
            const mc_block *alias_view = out;
            f.owner->blocks.named[i] = NULL;
            const mc_registry_status null_status =
                mc_registry_block_static_get(f.owner, block_static_names[i], &out, &error);
            const mc_block *null_out = out;
            const mc_registry_error null_error = error;
            const mc_registry_status null_view_status = mc_registry_classes_inspect_block_static(
                f.owner, block_static_names[i], &out, &error);
            const mc_block *null_view = out;
            f.owner->blocks.named[i] = original;
            CHECK(alias_status == MC_REGISTRY_OK && alias == other &&
                  fresh(alias_error, MC_REGISTRY_OK) && alias_view_status == MC_REGISTRY_OK &&
                  alias_view == other && null_status == MC_REGISTRY_OK && null_out == NULL &&
                  fresh(null_error, MC_REGISTRY_OK) && null_view_status == MC_REGISTRY_OK &&
                  null_view == NULL && fresh(error, MC_REGISTRY_OK));
            CHECK(mc_registry_block_static_get(f.owner, block_static_names[i], &out, NULL) ==
                      MC_REGISTRY_OK &&
                  out == original);
        }
        for (size_t i = 0; i < COUNT(block_static_invalid); ++i) {
            out = &anchor;
            error = poison_error();
            CHECK(mc_registry_block_static_get(f.owner, block_static_invalid[i], &out, &error) ==
                      MC_REGISTRY_INVALID_ARGUMENT &&
                  out == &anchor && block_static_unknown_error(f.owner, error) &&
                  same_error(error, f.owner->last_error));
            const mc_registry_error owner_error = f.owner->last_error;
            CHECK(mc_registry_classes_inspect_block_static(f.owner, block_static_invalid[i], &out,
                                                           &error) ==
                      MC_REGISTRY_INVALID_ARGUMENT &&
                  out == &anchor && fresh(error, MC_REGISTRY_INVALID_ARGUMENT) &&
                  strcmp(error.message, "unknown Block static name") == 0 &&
                  same_error(owner_error, f.owner->last_error));
        }
        CHECK(f.owner->allocation_bytes == ready_bytes && f.opens == 3 && f.closes == 1 &&
              uniquifier(options.random_profile) == ready_random &&
              f.owner->blocks_phase == MC_REGISTRY_READY && f.owner->entered == 0 &&
              f.owner->initializer == NULL && !f.bad);
    }
    result = 0;
cleanup:
    if (!retire(&f))
        result = 1;
    return result;
}
