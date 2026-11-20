#include "mc/client/sound/sound_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

#define COUNT(values) (sizeof(values) / sizeof((values)[0]))
#define FULL_BUDGET MC_SOUND_POOL_MAX_ALLOCATION_BYTES

static const uint16_t url_units[] = {'/', 'a', 'u', 'd', 'i', 'o', '/', 'a', '%', '2', '0', 'b'};
static const mc_sound_file_identity file_identity = {
    .native_utf8_path = "/authored/audio/a b.ogg",
    .url_path = {.units = url_units, .length = COUNT(url_units)}};

static mc_sound_text ascii(const char *value, uint16_t *units) {
    const size_t length = strlen(value);
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)value[i];
    return (mc_sound_text){.units = units, .length = length};
}

static bool text_is(mc_sound_text text, const uint16_t *units, size_t length) {
    return !text.is_null && text.length == length &&
           (length == 0 || memcmp(text.units, units, length * sizeof(*units)) == 0);
}

static bool ascii_is(mc_sound_text text, const char *value) {
    if (text.is_null || text.length != strlen(value))
        return false;
    for (size_t i = 0; i < text.length; ++i)
        if (text.units[i] != (unsigned char)value[i])
            return false;
    return true;
}

static mc_sound_status add(mc_sound_pool *pool, mc_sound_text name,
                           const mc_sound_file_identity *identity, const mc_sound_pool_entry **out,
                           mc_sound_pool_report *report) {
    mc_sound_pool_registration *registration = NULL;
    mc_sound_status status = mc_sound_pool_begin(pool, name, &registration, report, NULL);
    if (status == MC_SOUND_OK)
        status = mc_sound_pool_commit(&registration, identity, out, report, NULL);
    mc_sound_pool_registration_free(registration);
    return status;
}

/* Independent literal Java LCG and signed31 rejection arithmetic. The only
 * producer input is an actual read-only constructor state. No expected state
 * or seed is installed into the production owner and no core draw is called. */
static size_t draw(uint64_t *state, uint32_t bound) {
    for (;;) {
        *state = (*state * UINT64_C(25214903917) + UINT64_C(11)) & UINT64_C(281474976710655);
        const uint32_t bits = (uint32_t)(*state / UINT64_C(131072));
        if ((bound & (bound - 1u)) == 0)
            return (size_t)(((uint64_t)bound * bits) / UINT64_C(2147483648));
        const uint32_t value = bits % bound;
        if ((uint64_t)bits - value + bound - 1u < UINT64_C(2147483648))
            return value;
    }
}

static bool empty_and_arguments(mc_java_random_profile *profile) {
    bool ok = false;
    mc_sound_pool *pool = NULL;
    mc_sound_pool_registration *registration = NULL;
    mc_sound_pool_report report;
    mc_sound_error error;
    mc_sound_pool_state before, after;
    const mc_sound_pool_entry *entry = NULL;
    uint16_t units[32];
    CHECK(mc_sound_pool_create(profile, FULL_BUDGET, &pool, &error) == MC_SOUND_OK);
    CHECK(error.status == MC_SOUND_OK);
    CHECK(mc_sound_pool_read_state(pool, &before, NULL) == MC_SOUND_OK);
    CHECK(before.strip_digits && before.count == 0 && before.group_count == 0 &&
          before.full_entry_count == 0 && before.owned_entry_count == 0 &&
          before.allocation_bytes > 0 && !before.registration_active);
    CHECK(!before.random.have_next_next_gaussian && before.random.next_next_gaussian == 0.0);
    CHECK(mc_sound_pool_select_any(pool, &entry, &report, NULL) == MC_SOUND_OK && entry == NULL &&
          report.source_pc == 13);
    CHECK(mc_sound_pool_select(pool, (mc_sound_text){.is_null = true}, &entry, &report, NULL) ==
          MC_SOUND_OK);
    CHECK(entry == NULL && report.source_pc == 19);
    CHECK(mc_sound_pool_select(pool, ascii("absent", units), &entry, NULL, NULL) == MC_SOUND_OK &&
          entry == NULL);
    CHECK(mc_sound_pool_read_state(pool, &after, NULL) == MC_SOUND_OK &&
          before.random.state == after.random.state &&
          before.allocation_bytes == after.allocation_bytes);
    CHECK(mc_sound_pool_begin(pool, (mc_sound_text){.is_null = true}, &registration, &report,
                              &error) == MC_SOUND_SOURCE_EXCEPTION);
    CHECK(registration == NULL && report.source_pc == 7 &&
          report.exception == MC_SOUND_EXCEPTION_NULL_NAME &&
          error.exception == MC_SOUND_EXCEPTION_NULL_NAME);
    CHECK(mc_sound_pool_begin(pool, (mc_sound_text){0}, &registration, &report, NULL) ==
          MC_SOUND_SOURCE_EXCEPTION);
    CHECK(report.source_pc == 10);
    CHECK(mc_sound_pool_begin(pool, ascii("without-dot", units), &registration, &report, NULL) ==
              MC_SOUND_SOURCE_EXCEPTION &&
          report.source_pc == 10);
    CHECK(mc_sound_pool_begin(pool, ascii("123.ogg", units), &registration, &report, NULL) ==
              MC_SOUND_SOURCE_EXCEPTION &&
          report.source_pc == 28);
    CHECK(mc_sound_pool_begin(pool, ascii(".ogg", units), &registration, &report, NULL) ==
              MC_SOUND_SOURCE_EXCEPTION &&
          report.source_pc == 28);
    const mc_sound_text malformed = {.units = NULL, .length = 1};
    CHECK(mc_sound_pool_begin(pool, malformed, &registration, &report, NULL) ==
          MC_SOUND_INVALID_ARGUMENT);
    const mc_sound_text over = {.units = units, .length = MC_SOUND_POOL_MAX_TEXT_UNITS + 1u};
    CHECK(mc_sound_pool_begin(pool, over, &registration, &report, NULL) == MC_SOUND_LIMIT);
    CHECK(mc_sound_pool_read_state(pool, &after, NULL) == MC_SOUND_OK && after.group_count == 0 &&
          after.allocation_bytes == before.allocation_bytes &&
          after.random.state == before.random.state);
    mc_sound_pool *preserved = pool;
    CHECK(mc_sound_pool_create(NULL, FULL_BUDGET, &pool, NULL) == MC_SOUND_INVALID_ARGUMENT &&
          pool == preserved);
    CHECK(mc_sound_pool_create(profile, 0, &pool, NULL) == MC_SOUND_LIMIT && pool == preserved);
    CHECK(mc_sound_pool_create(profile, (size_t)FULL_BUDGET + 1, &pool, NULL) == MC_SOUND_LIMIT &&
          pool == preserved);
    CHECK(mc_sound_pool_read_state(NULL, &after, NULL) == MC_SOUND_INVALID_ARGUMENT);
    CHECK(mc_sound_pool_entry_read(NULL, NULL, NULL) == MC_SOUND_INVALID_ARGUMENT);
    CHECK(mc_sound_pool_commit(NULL, NULL, &entry, NULL, NULL) == MC_SOUND_INVALID_ARGUMENT);
    ok = true;
cleanup:
    mc_sound_pool_registration_free(registration);
    if (mc_sound_pool_free(pool, NULL) != MC_SOUND_OK)
        ok = false;
    return ok;
}

static bool ownership_and_selection(mc_java_random_profile *profile) {
    bool ok = false;
    mc_sound_pool *pool = NULL;
    const mc_sound_pool_entry *entries[4] = {0}, *selected = NULL;
    mc_sound_pool_entry_view view;
    mc_sound_pool_group_view group;
    mc_sound_pool_state state;
    mc_sound_pool_report report;
    uint16_t name[64], key[64], mutable_url[] = {'/', 0, 0xd800, 0xdc00, 'x'};
    char path[] = "/owned/\xc3\xa9.ogg";
    const mc_sound_file_identity mutable_file = {
        .native_utf8_path = path, .url_path = {.units = mutable_url, .length = COUNT(mutable_url)}};
    CHECK(mc_sound_pool_create(profile, FULL_BUDGET, &pool, NULL) == MC_SOUND_OK);
    CHECK(add(pool, ascii("random/click1.ogg", name), &mutable_file, &entries[0], &report) ==
          MC_SOUND_OK);
    CHECK(report.group_created && report.entry_created && report.group_appended &&
          report.full_list_appended && report.count_incremented && report.source_pc == 154);
    path[1] = 'X';
    mutable_url[0] = 'X';
    name[0] = 'X';
    CHECK(mc_sound_pool_entry_read(entries[0], &view, NULL) == MC_SOUND_OK);
    CHECK(ascii_is(view.logical_name, "random/click1.ogg"));
    CHECK(strcmp(view.file.native_utf8_path, "/owned/\xc3\xa9.ogg") == 0);
    const uint16_t retained_url[] = {'/', 0, 0xd800, 0xdc00, 'x'};
    CHECK(text_is(view.file.url_path, retained_url, COUNT(retained_url)));
    CHECK(add(pool, ascii("random/click2.ogg", name), &file_identity, &entries[1], &report) ==
              MC_SOUND_OK &&
          !report.group_created);
    CHECK(add(pool, ascii("random/click1.ogg", name), &file_identity, &entries[2], NULL) ==
              MC_SOUND_OK &&
          entries[2] != entries[0]);
    CHECK(add(pool, ascii("other.extra.suffix.ogg", name), &file_identity, &entries[3], NULL) ==
          MC_SOUND_OK);
    CHECK(mc_sound_pool_read_group(pool, 0, &group, NULL) == MC_SOUND_OK &&
          ascii_is(group.key, "random.click") && group.entry_count == 3);
    CHECK(mc_sound_pool_read_group(pool, 1, &group, NULL) == MC_SOUND_OK &&
          ascii_is(group.key, "other") && group.entry_count == 1);
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_sound_pool_entry_at(pool, i, &selected, NULL) == MC_SOUND_OK &&
              selected == entries[i]);
        if (i < 3)
            CHECK(mc_sound_pool_group_entry(pool, 0, i, &selected, NULL) == MC_SOUND_OK &&
                  selected == entries[i]);
    }
    CHECK(mc_sound_pool_read_state(pool, &state, NULL) == MC_SOUND_OK && state.count == 4 &&
          state.owned_entry_count == 4 && state.full_entry_count == 4 && state.group_count == 2);
    uint64_t expected = state.random.state;
    for (size_t i = 0; i < 96; ++i) {
        const size_t index = draw(&expected, 3);
        CHECK(mc_sound_pool_select(pool, ascii("random.click", key), &selected, &report, NULL) ==
                  MC_SOUND_OK &&
              selected == entries[index] && report.source_pc == 42);
        CHECK(mc_sound_pool_read_state(pool, &state, NULL) == MC_SOUND_OK &&
              state.random.state == expected && !state.random.have_next_next_gaussian);
    }
    const uint64_t before_one = expected;
    CHECK(draw(&expected, 1) == 0);
    CHECK(mc_sound_pool_select(pool, ascii("other", key), &selected, NULL, NULL) == MC_SOUND_OK &&
          selected == entries[3]);
    CHECK(mc_sound_pool_read_state(pool, &state, NULL) == MC_SOUND_OK &&
          state.random.state == expected && state.random.state != before_one);
    for (size_t i = 0; i < 64; ++i) {
        const size_t index = draw(&expected, 4);
        CHECK(mc_sound_pool_select_any(pool, &selected, NULL, NULL) == MC_SOUND_OK &&
              selected == entries[index]);
    }
    CHECK(mc_sound_pool_read_state(pool, &state, NULL) == MC_SOUND_OK &&
          state.random.state == expected);
    CHECK(mc_sound_pool_select(pool, ascii("random/click1.ogg", key), &selected, NULL, NULL) ==
              MC_SOUND_OK &&
          selected == NULL);
    selected = entries[0];
    CHECK(mc_sound_pool_entry_at(pool, 4, &selected, NULL) == MC_SOUND_INVALID_ARGUMENT &&
          selected == entries[0]);
    CHECK(mc_sound_pool_group_entry(pool, 0, 3, &selected, NULL) == MC_SOUND_INVALID_ARGUMENT &&
          selected == entries[0]);
    ok = true;
cleanup:
    if (mc_sound_pool_free(pool, NULL) != MC_SOUND_OK)
        ok = false;
    return ok;
}

static bool unicode_and_streaming(mc_java_random_profile *profile) {
    bool ok = false;
    mc_sound_pool *pool = NULL;
    mc_sound_pool_group_view group;
    mc_sound_pool_entry_view view;
    const mc_sound_pool_entry *entry = NULL;
    /* Arabic, extended Arabic, fullwidth and Devanagari decimal tails. */
    const uint16_t name[] = {'a', '/', 'b', 0x0662, 0x06f3, 0xff14, 0x096b, '.', 'o', 'g', 'g'};
    const uint16_t key[] = {'a', '.', 'b'};
    const uint16_t non_decimal[] = {'x', 0x00b2, '.', 'o', 'g', 'g'};
    const uint16_t sup_key[] = {'x', 0x00b2};
    const uint16_t raw[] = {'a', 0, 0xd800, '7', '.', 'x'};
    const uint16_t raw_key[] = {'a', 0, 0xd800};
    const uint16_t all_digits[] = {0x0661, 0xff12, '.', 'x'};
    uint16_t ascii_units[32];
    CHECK(mc_sound_pool_create(profile, FULL_BUDGET, &pool, NULL) == MC_SOUND_OK);
    CHECK(add(pool, (mc_sound_text){.units = name, .length = COUNT(name)}, &file_identity, &entry,
              NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_read_group(pool, 0, &group, NULL) == MC_SOUND_OK &&
          text_is(group.key, key, COUNT(key)));
    CHECK(add(pool, (mc_sound_text){.units = non_decimal, .length = COUNT(non_decimal)},
              &file_identity, &entry, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_read_group(pool, 1, &group, NULL) == MC_SOUND_OK &&
          text_is(group.key, sup_key, COUNT(sup_key)));
    CHECK(add(pool, (mc_sound_text){.units = raw, .length = COUNT(raw)}, &file_identity, &entry,
              NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_read_group(pool, 2, &group, NULL) == MC_SOUND_OK &&
          text_is(group.key, raw_key, COUNT(raw_key)));
    CHECK(mc_sound_pool_entry_read(entry, &view, NULL) == MC_SOUND_OK &&
          text_is(view.logical_name, raw, COUNT(raw)));
    CHECK(add(pool, (mc_sound_text){.units = all_digits, .length = COUNT(all_digits)},
              &file_identity, &entry, NULL) == MC_SOUND_SOURCE_EXCEPTION);
    CHECK(mc_sound_pool_set_strip_digits(pool, false, NULL) == MC_SOUND_OK);
    CHECK(add(pool, ascii("13.mus", ascii_units), &file_identity, &entry, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_read_group(pool, 3, &group, NULL) == MC_SOUND_OK &&
          ascii_is(group.key, "13"));
    CHECK(add(pool, ascii(".ogg", ascii_units), &file_identity, &entry, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_read_group(pool, 4, &group, NULL) == MC_SOUND_OK && !group.key.is_null &&
          group.key.length == 0);
    CHECK(mc_sound_pool_select(pool, (mc_sound_text){0}, &entry, NULL, NULL) == MC_SOUND_OK &&
          entry != NULL);
    CHECK(mc_sound_pool_select(pool, (mc_sound_text){.is_null = true}, &entry, NULL, NULL) ==
              MC_SOUND_OK &&
          entry == NULL);
    CHECK(mc_sound_pool_set_strip_digits(pool, true, NULL) == MC_SOUND_OK);
    CHECK(add(pool, ascii("13.mus", ascii_units), &file_identity, &entry, NULL) ==
          MC_SOUND_SOURCE_EXCEPTION);
    CHECK(mc_sound_pool_read_group(pool, 3, &group, NULL) == MC_SOUND_OK && group.entry_count == 1);
    ok = true;
cleanup:
    if (mc_sound_pool_free(pool, NULL) != MC_SOUND_OK)
        ok = false;
    return ok;
}

static bool registration_prefixes(mc_java_random_profile *profile) {
    bool ok = false;
    mc_sound_pool *pool = NULL;
    mc_sound_pool_registration *registration = NULL, *unchanged = NULL;
    const mc_sound_pool_entry *entry = NULL;
    mc_sound_pool_report report;
    mc_sound_pool_state before, after;
    mc_sound_pool_group_view group;
    mc_sound_error error;
    uint16_t units[32];
    CHECK(mc_sound_pool_create(profile, FULL_BUDGET, &pool, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_read_state(pool, &before, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_begin(pool, ascii("unresolved1.ogg", units), &registration, &report,
                              NULL) == MC_SOUND_OK &&
          report.group_created);
    CHECK(mc_sound_pool_read_state(pool, &after, NULL) == MC_SOUND_OK &&
          after.registration_active && after.group_count == 1);
    CHECK(mc_sound_pool_free(pool, NULL) == MC_SOUND_BUSY);
    CHECK(mc_sound_pool_select_any(pool, &entry, NULL, NULL) == MC_SOUND_BUSY);
    CHECK(mc_sound_pool_set_strip_digits(pool, false, NULL) == MC_SOUND_BUSY);
    CHECK(mc_sound_pool_begin(pool, ascii("second.ogg", units), &unchanged, NULL, NULL) ==
              MC_SOUND_BUSY &&
          unchanged == NULL);
    CHECK(mc_sound_pool_commit(&registration, NULL, &entry, &report, &error) ==
              MC_SOUND_SOURCE_EXCEPTION &&
          report.source_pc == 98 && report.exception == MC_SOUND_EXCEPTION_NULL_FILE &&
          report.group_created && !report.entry_created && entry == NULL);
    CHECK(mc_sound_pool_commit(&registration, &file_identity, &entry, NULL, NULL) == MC_SOUND_BUSY);
    mc_sound_pool_registration_free(registration);
    registration = NULL;
    CHECK(mc_sound_pool_read_group(pool, 0, &group, NULL) == MC_SOUND_OK &&
          ascii_is(group.key, "unresolved") && group.entry_count == 0);
    CHECK(mc_sound_pool_select(pool, ascii("unresolved", units), &entry, &report, NULL) ==
              MC_SOUND_SOURCE_EXCEPTION &&
          report.source_pc == 31 && report.exception == MC_SOUND_EXCEPTION_RANDOM_BOUND);
    CHECK(mc_sound_pool_select_any(pool, &entry, NULL, NULL) == MC_SOUND_OK && entry == NULL);
    CHECK(mc_sound_pool_read_state(pool, &after, NULL) == MC_SOUND_OK &&
          before.random.state == after.random.state && after.count == 0 &&
          after.full_entry_count == 0 && after.owned_entry_count == 0);
    CHECK(add(pool, ascii("unresolved2.ogg", units), &file_identity, &entry, &report) ==
              MC_SOUND_OK &&
          !report.group_created);
    CHECK(mc_sound_pool_read_group(pool, 0, &group, NULL) == MC_SOUND_OK && group.entry_count == 1);
    const mc_sound_pool_entry *preserved = entry;
    CHECK(mc_sound_pool_add_file(pool, ascii("nullfile.ogg", units), NULL, &entry, &report,
                                 &error) == MC_SOUND_SOURCE_EXCEPTION &&
          report.source_pc == 98 && report.exception == MC_SOUND_EXCEPTION_NULL_FILE &&
          entry == preserved);
    CHECK(mc_sound_pool_read_group(pool, 1, &group, NULL) == MC_SOUND_OK &&
          ascii_is(group.key, "nullfile") && group.entry_count == 0);
    CHECK(mc_sound_pool_add_file(pool, ascii("relative.ogg", units), "relative.ogg", &entry,
                                 &report, NULL) == MC_SOUND_UNSUPPORTED_PATH &&
          entry == preserved);
    CHECK(report.group_created && report.source_pc == 98);
    CHECK(mc_sound_pool_begin(pool, ascii("cancel.ogg", units), &registration, NULL, NULL) ==
          MC_SOUND_OK);
    mc_sound_pool_registration_free(registration);
    registration = NULL;
    CHECK(mc_sound_pool_read_group(pool, 3, &group, NULL) == MC_SOUND_OK &&
          ascii_is(group.key, "cancel") && group.entry_count == 0);
    const unsigned char invalid_utf8[] = {'/', 0xc0, 0xaf, 0};
    const mc_sound_file_identity bad = {
        .native_utf8_path = (const char *)invalid_utf8,
        .url_path = {.units = url_units, .length = COUNT(url_units)}};
    CHECK(add(pool, ascii("bad.ogg", units), &bad, &entry, &report) == MC_SOUND_INVALID_ARGUMENT &&
          report.group_created && !report.entry_created && entry == preserved);
    const mc_sound_file_identity null_url = {.native_utf8_path = "/a",
                                             .url_path = {.is_null = true}};
    CHECK(add(pool, ascii("url.ogg", units), &null_url, &entry, &report) ==
              MC_SOUND_INVALID_ARGUMENT &&
          report.source_pc == 101 && entry == preserved);
    ok = true;
cleanup:
    mc_sound_pool_registration_free(registration);
    if (mc_sound_pool_free(pool, NULL) != MC_SOUND_OK)
        ok = false;
    return ok;
}

static bool budget_prefixes(mc_java_random_profile *profile) {
    bool ok = false;
    mc_sound_pool *pool = NULL;
    mc_sound_pool_registration *registration = NULL;
    const mc_sound_pool_entry *entry = NULL, *group_entry = NULL;
    mc_sound_pool_state initial, state;
    mc_sound_pool_group_view group;
    mc_sound_pool_report report;
    uint16_t units[32];
    bool group_failure = false, entry_failure = false, group_append_failure = false;
    bool full_append_failure = false, completed = false;
    CHECK(mc_sound_pool_create(profile, FULL_BUDGET, &pool, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_read_state(pool, &initial, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_free(pool, NULL) == MC_SOUND_OK);
    pool = NULL;
    /* Finite budget search exercises actual allocation stages using only
     * public allocation accounting, without allocator/owner-layout injection. */
    for (size_t budget = initial.allocation_bytes; budget <= initial.allocation_bytes + 1024;
         ++budget) {
        CHECK(mc_sound_pool_create(profile, budget, &pool, NULL) == MC_SOUND_OK);
        mc_sound_status status =
            mc_sound_pool_begin(pool, ascii("a1.ogg", units), &registration, &report, NULL);
        if (status == MC_SOUND_LIMIT && report.source_pc == -1) {
            CHECK(registration == NULL);
        } else if (status == MC_SOUND_LIMIT) {
            /* Native group allocation has no published group. */
            CHECK(registration == NULL);
            group_failure = true;
        } else {
            CHECK(status == MC_SOUND_OK && registration != NULL);
            status = mc_sound_pool_commit(&registration, &file_identity, &entry, &report, NULL);
            CHECK(mc_sound_pool_read_state(pool, &state, NULL) == MC_SOUND_OK &&
                  state.allocation_bytes <= budget);
            CHECK(mc_sound_pool_read_group(pool, 0, &group, NULL) == MC_SOUND_OK);
            if (status == MC_SOUND_LIMIT && report.source_pc == 104) {
                entry_failure = true;
                CHECK(!report.entry_created && group.entry_count == 0 &&
                      state.owned_entry_count == 0);
            } else if (status == MC_SOUND_LIMIT && report.source_pc == 124) {
                group_append_failure = true;
                CHECK(report.entry_created && !report.group_appended && group.entry_count == 0 &&
                      state.owned_entry_count == 1);
            } else if (status == MC_SOUND_LIMIT && report.source_pc == 136) {
                full_append_failure = true;
                CHECK(report.entry_created && report.group_appended && !report.full_list_appended &&
                      !report.count_incremented && group.entry_count == 1 && state.count == 0 &&
                      state.full_entry_count == 0 && state.owned_entry_count == 1);
                CHECK(mc_sound_pool_group_entry(pool, 0, 0, &group_entry, NULL) == MC_SOUND_OK &&
                      group_entry != NULL);
            } else {
                CHECK(status == MC_SOUND_OK && report.count_incremented && state.count == 1);
                completed = true;
            }
            mc_sound_pool_registration_free(registration);
            registration = NULL;
            CHECK(mc_sound_pool_read_state(pool, &state, NULL) == MC_SOUND_OK &&
                  !state.registration_active && state.allocation_bytes <= budget);
            if (report.source_pc == 136) {
                CHECK(mc_sound_pool_select(pool, ascii("a", units), &entry, NULL, NULL) ==
                          MC_SOUND_OK &&
                      entry == group_entry);
                CHECK(mc_sound_pool_select_any(pool, &entry, NULL, NULL) == MC_SOUND_OK &&
                      entry == NULL);
            }
        }
        CHECK(mc_sound_pool_free(pool, NULL) == MC_SOUND_OK);
        pool = NULL;
        entry = NULL;
        group_entry = NULL;
        if (completed)
            break;
    }
    CHECK(group_failure && entry_failure && group_append_failure && full_append_failure &&
          completed);
    ok = true;
cleanup:
    mc_sound_pool_registration_free(registration);
    if (mc_sound_pool_free(pool, NULL) != MC_SOUND_OK)
        ok = false;
    return ok;
}

static bool bounded_full_list(mc_java_random_profile *profile) {
    bool ok = false;
    mc_sound_pool *pool = NULL;
    mc_sound_pool_state state;
    mc_sound_pool_report report;
    mc_sound_pool_group_view group;
    const mc_sound_pool_entry *entry = NULL, *first = NULL, *last = NULL;
    uint16_t units[32];
    CHECK(mc_sound_pool_create(profile, FULL_BUDGET, &pool, NULL) == MC_SOUND_OK);
    const mc_sound_text name = ascii("duplicate1.ogg", units);
    for (size_t i = 0; i < MC_SOUND_POOL_MAX_ENTRIES; ++i) {
        CHECK(add(pool, name, &file_identity, &entry, NULL) == MC_SOUND_OK);
        if (i == 0)
            first = entry;
        last = entry;
    }
    CHECK(first != last);
    CHECK(mc_sound_pool_read_state(pool, &state, NULL) == MC_SOUND_OK &&
          state.count == (int32_t)MC_SOUND_POOL_MAX_ENTRIES &&
          state.full_entry_count == MC_SOUND_POOL_MAX_ENTRIES &&
          state.owned_entry_count == MC_SOUND_POOL_MAX_ENTRIES && state.group_count == 1);
    CHECK(add(pool, name, &file_identity, &entry, &report) == MC_SOUND_LIMIT &&
          report.source_pc == 104 && !report.entry_created && entry == last);
    CHECK(mc_sound_pool_read_group(pool, 0, &group, NULL) == MC_SOUND_OK &&
          group.entry_count == MC_SOUND_POOL_MAX_ENTRIES);
    CHECK(mc_sound_pool_entry_at(pool, 0, &entry, NULL) == MC_SOUND_OK && entry == first);
    CHECK(mc_sound_pool_entry_at(pool, MC_SOUND_POOL_MAX_ENTRIES - 1u, &entry, NULL) ==
              MC_SOUND_OK &&
          entry == last);
    ok = true;
cleanup:
    if (mc_sound_pool_free(pool, NULL) != MC_SOUND_OK)
        ok = false;
    return ok;
}

static bool bounded_groups_and_independent_owners(mc_java_random_profile *profile) {
    bool ok = false;
    mc_sound_pool *pool = NULL, *other = NULL;
    mc_sound_pool_registration *registration = NULL;
    mc_sound_pool_state before, after, other_state;
    mc_sound_pool_group_view group;
    mc_sound_pool_report report;
    const mc_sound_pool_entry *entry = NULL;
    uint16_t units[64];
    char name[64];
    CHECK(mc_sound_pool_create(profile, FULL_BUDGET, &pool, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_read_state(pool, &before, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_create(profile, FULL_BUDGET, &other, NULL) == MC_SOUND_OK);
    CHECK(add(other, ascii("other.ogg", units), &file_identity, &entry, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_pool_select_any(other, &entry, NULL, NULL) == MC_SOUND_OK && entry != NULL);
    CHECK(mc_sound_pool_read_state(other, &other_state, NULL) == MC_SOUND_OK &&
          other_state.count == 1);
    CHECK(mc_sound_pool_read_state(pool, &after, NULL) == MC_SOUND_OK && after.count == 0 &&
          after.group_count == 0 && after.random.state == before.random.state);
    CHECK(mc_sound_pool_set_strip_digits(pool, false, NULL) == MC_SOUND_OK);
    for (size_t i = 0; i < MC_SOUND_POOL_MAX_GROUPS; ++i) {
        const int written = snprintf(name, sizeof(name), "group%zu.ogg", i);
        CHECK(written > 0 && (size_t)written < sizeof(name));
        CHECK(mc_sound_pool_begin(pool, ascii(name, units), &registration, &report, NULL) ==
                  MC_SOUND_OK &&
              report.group_created);
        mc_sound_pool_registration_free(registration);
        registration = NULL;
    }
    CHECK(mc_sound_pool_begin(pool, ascii("one-more.ogg", units), &registration, &report, NULL) ==
              MC_SOUND_LIMIT &&
          registration == NULL && report.source_pc == 86 && !report.group_created);
    CHECK(mc_sound_pool_read_state(pool, &after, NULL) == MC_SOUND_OK &&
          after.group_count == MC_SOUND_POOL_MAX_GROUPS && after.count == 0 &&
          after.full_entry_count == 0 && after.owned_entry_count == 0 &&
          after.random.state == before.random.state && !after.registration_active);
    CHECK(mc_sound_pool_read_group(pool, 0, &group, NULL) == MC_SOUND_OK &&
          ascii_is(group.key, "group0") && group.entry_count == 0);
    const int written = snprintf(name, sizeof(name), "group%u", MC_SOUND_POOL_MAX_GROUPS - 1u);
    CHECK(written > 0 && (size_t)written < sizeof(name));
    CHECK(mc_sound_pool_read_group(pool, MC_SOUND_POOL_MAX_GROUPS - 1u, &group, NULL) ==
              MC_SOUND_OK &&
          ascii_is(group.key, name) && group.entry_count == 0);
    CHECK(mc_sound_pool_read_state(other, &other_state, NULL) == MC_SOUND_OK &&
          other_state.count == 1 && other_state.group_count == 1);
    ok = true;
cleanup:
    mc_sound_pool_registration_free(registration);
    if (mc_sound_pool_free(pool, NULL) != MC_SOUND_OK)
        ok = false;
    if (mc_sound_pool_free(other, NULL) != MC_SOUND_OK)
        ok = false;
    return ok;
}

int main(void) {
    mc_java_random_profile *profile = NULL;
    if (mc_java_random_profile_create(1024u * 1024u, &profile) != MC_JAVA_RANDOM_PROFILE_OK) {
        fputs("SoundPool test Random profile failed\n", stderr);
        return 1;
    }
    const bool ok = empty_and_arguments(profile) && ownership_and_selection(profile) &&
                    unicode_and_streaming(profile) && registration_prefixes(profile) &&
                    budget_prefixes(profile) && bounded_full_list(profile) &&
                    bounded_groups_and_independent_owners(profile);
    mc_java_random_profile_free(profile);
    if (!ok)
        return 1;
    if (printf("PASS: %u SoundPool checks\n", checks) < 0 || fflush(stdout) != 0)
        return 1;
    return ferror(stdout) ? 1 : 0;
}
