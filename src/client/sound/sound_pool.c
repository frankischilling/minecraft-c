#include "mc/client/sound/sound_pool.h"
#include "mc/client/sound/sound_file_registration.h"
#include <stddef.h>
#include <threads.h>

#include "pool_construction.h"

#include "mc/client/sound/sound_file.h"
#include "mc/core/java/integer_parse.h"

#include <stdlib.h>
#include <string.h>

typedef union pool_allocation pool_allocation;

union pool_allocation {
    struct {
        pool_allocation *next;
        size_t bytes;
    } header;

    void *pointer_alignment;
    uint64_t integer_alignment;
    long double floating_alignment;
};

typedef struct pool_group {
    struct pool_group *next;
    mc_sound_text key;
    const mc_sound_pool_entry **entries;
    size_t count, capacity;
} pool_group;

struct mc_sound_pool_entry {
    mc_sound_pool_entry *next_owned;
    mc_sound_pool_entry_view view;
};

struct mc_sound_pool {
    mc_java_random random;
    bool strip_digits;
    int32_t count;
    pool_group *groups, *last_group;
    size_t group_count;
    const mc_sound_pool_entry **entries;
    size_t entry_count, entry_capacity, owned_count;
    mc_sound_pool_entry *owned;
    pool_allocation *allocations;
    size_t allocation_bytes, allocation_limit;
    mc_sound_pool_registration *registration;
};

/* Fixed native monitor header is outside the unchanged source pool ledger.
 * No HTTP/XML/device ownership lives here. Final retirement is quiescent. */
typedef struct pool_native {
    mtx_t monitor;
    bool active;
    mc_sound_pool body;
} pool_native;

static pool_native *native_pool(const mc_sound_pool *pool) {
    return (pool_native *)((unsigned char *)(void *)pool - offsetof(pool_native, body));
}

struct mc_sound_pool_registration {
    mc_sound_pool *pool;
    pool_group *group;
    mc_sound_text logical_name;
    mc_sound_pool_report report;
    bool attempted;
};

static mc_sound_status fail(mc_sound_error *error, mc_sound_status status, const char *operation,
                            int32_t pc, mc_sound_source_exception exception) {
    if (error != NULL)
        *error = (mc_sound_error){
            .status = status, .operation = operation, .source_pc = pc, .exception = exception};
    return status;
}

static mc_sound_status success(mc_sound_error *error) {
    if (error != NULL)
        *error = (mc_sound_error){0};
    return MC_SOUND_OK;
}

static void report_empty(mc_sound_pool_report *report) {
    if (report != NULL)
        *report = (mc_sound_pool_report){.source_pc = -1};
}

static mc_sound_status registration_result(mc_sound_pool_registration *registration,
                                           mc_sound_pool_report *report, mc_sound_error *error,
                                           mc_sound_status status, const char *operation,
                                           int32_t pc, mc_sound_source_exception exception) {
    registration->report.source_pc = pc;
    registration->report.exception = exception;
    if (report != NULL)
        *report = registration->report;
    return status == MC_SOUND_OK ? success(error) : fail(error, status, operation, pc, exception);
}

static bool text_shape(mc_sound_text text) {
    return text.is_null ? text.units == NULL && text.length == 0
                        : text.length == 0 || text.units != NULL;
}

static mc_sound_status text_admit(mc_sound_text text, mc_sound_error *error) {
    if (!text_shape(text))
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool text view", -1,
                    MC_SOUND_EXCEPTION_NONE);
    if (text.length > MC_SOUND_POOL_MAX_TEXT_UNITS)
        return fail(error, MC_SOUND_LIMIT, "SoundPool text units", -1, MC_SOUND_EXCEPTION_NONE);
    return MC_SOUND_OK;
}

static void *allocate(mc_sound_pool *pool, size_t payload, mc_sound_status *status) {
    if (payload > SIZE_MAX - sizeof(pool_allocation) ||
        sizeof(pool_allocation) + payload > pool->allocation_limit - pool->allocation_bytes) {
        *status = MC_SOUND_LIMIT;
        return NULL;
    }
    const size_t bytes = sizeof(pool_allocation) + payload;
    pool_allocation *allocation = malloc(bytes);
    if (allocation == NULL) {
        *status = MC_SOUND_ALLOCATION_FAILED;
        return NULL;
    }
    allocation->header.next = pool->allocations;
    allocation->header.bytes = bytes;
    pool->allocations = allocation;
    pool->allocation_bytes += bytes;
    *status = MC_SOUND_OK;
    return allocation + 1;
}

static void release(mc_sound_pool *pool, void *payload) {
    if (payload == NULL)
        return;
    pool_allocation *allocation = (pool_allocation *)payload - 1;
    pool_allocation **link = &pool->allocations;
    while (*link != allocation)
        link = &(*link)->header.next;
    *link = allocation->header.next;
    pool->allocation_bytes -= allocation->header.bytes;
    free(allocation);
}

/* For BMP char inputs, the selected JDK17 isDigit profile is precisely the
 * 370 decimal units accepted by the existing radix10 parser. Calling the
 * parser on one unit reuses those independently checked character facts,
 * rather than importing a second Unicode table or JDK implementation. */
static bool decimal_unit(uint16_t unit) {
    int32_t value;
    const mc_java_integer_parse_text text = {.units = &unit, .length = 1};
    return mc_java_integer_parse(&text, &value) == MC_JAVA_INTEGER_PARSE_OK;
}

static bool key_equal(mc_sound_text key, mc_sound_text input, size_t length, bool replace_slash) {
    if (key.length != length)
        return false;
    for (size_t i = 0; i < length; ++i) {
        uint16_t unit = input.units[i];
        if (replace_slash && unit == '/')
            unit = '.';
        if (key.units[i] != unit)
            return false;
    }
    return true;
}

static pool_group *find_group(const mc_sound_pool *pool, mc_sound_text key, size_t length,
                              bool replace_slash) {
    if (key.is_null)
        return NULL;
    for (pool_group *group = pool->groups; group != NULL; group = group->next)
        if (key_equal(group->key, key, length, replace_slash))
            return group;
    return NULL;
}

static mc_sound_status grow(mc_sound_pool *pool, const mc_sound_pool_entry ***entries,
                            size_t *capacity, size_t count) {
    if (count < *capacity)
        return MC_SOUND_OK;
    const size_t next = *capacity == 0 ? 1 : *capacity * 2;
    if (next > MC_SOUND_POOL_MAX_ENTRIES)
        return MC_SOUND_LIMIT;
    mc_sound_status status;
    const mc_sound_pool_entry **replacement = allocate(pool, next * sizeof(*replacement), &status);
    if (replacement == NULL)
        return status;
    if (count != 0)
        memcpy(replacement, *entries, count * sizeof(*replacement));
    release(pool, (void *)*entries);
    *entries = replacement;
    *capacity = next;
    return MC_SOUND_OK;
}

/* Test builds may name a separately linked allocator around this same body.
 * Production always uses the original calloc, never a runtime callback. */
#if defined(MC_SOUND_POOL_CALLOC)
extern void *MC_SOUND_POOL_CALLOC(size_t, size_t);
#else
#define MC_SOUND_POOL_CALLOC calloc
#endif

mc_sound_status mc_sound_pool_allocate_raw(size_t max_allocation_bytes, mc_sound_pool **out,
                                           mc_sound_error *error) {
    if (out == NULL)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool constructor arguments", -1,
                    MC_SOUND_EXCEPTION_NONE);
    if (max_allocation_bytes < sizeof(mc_sound_pool) ||
        max_allocation_bytes > MC_SOUND_POOL_MAX_ALLOCATION_BYTES)
        return fail(error, MC_SOUND_LIMIT, "SoundPool allocation budget", -1,
                    MC_SOUND_EXCEPTION_NONE);
    pool_native *native = MC_SOUND_POOL_CALLOC(1, sizeof(*native));
    mc_sound_pool *pool = native == NULL ? NULL : &native->body;
    if (pool == NULL)
        return fail(error, MC_SOUND_ALLOCATION_FAILED, "SoundPool owner", -1,
                    MC_SOUND_EXCEPTION_NONE);
    if (mtx_init(&native->monitor, mtx_recursive) != thrd_success) {
        free(native);
        return fail(error, MC_SOUND_IO_FAILED, "SoundPool native monitor initialization", -1,
                    MC_SOUND_EXCEPTION_NONE);
    }
    /* Native charge is real even if the source constructor later fails. This
     * does not publish the embedded source Map/List or Random. */
    pool->allocation_bytes = sizeof(*pool);
    pool->allocation_limit = max_allocation_bytes;
    *out = pool;
    return success(error);
}

mc_sound_status mc_sound_pool_construct_raw(mc_sound_pool *pool, mc_java_random_profile *profile,
                                            mc_sound_pool_constructor_prefix *out,
                                            mc_sound_error *error) {
    mc_sound_pool_constructor_prefix prefix = {.last_store_pc = -1};
    if (pool == NULL || profile == NULL)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool constructor arguments", -1,
                    MC_SOUND_EXCEPTION_NONE);
    const mc_java_random_profile_status random =
        mc_java_random_construct_default(profile, &pool->random);
    if (random != MC_JAVA_RANDOM_PROFILE_OK) {
        fail(error, MC_SOUND_RANDOM_FAILED, "SoundPool Random constructor", 9,
             MC_SOUND_EXCEPTION_NONE);
        if (error != NULL)
            error->random_status = random;
        if (out != NULL)
            *out = prefix;
        return MC_SOUND_RANDOM_FAILED;
    }
    prefix.random_published = true;
    prefix.last_store_pc = 12;
    /* The actual native containers are embedded, not extra Java object
     * headers. Their empty representation becomes source-ready at23/34. */
    pool->groups = pool->last_group = NULL;
    pool->group_count = 0;
    prefix.map_published = true;
    prefix.last_store_pc = 23;
    pool->entries = NULL;
    pool->entry_count = pool->entry_capacity = 0;
    prefix.list_published = true;
    prefix.last_store_pc = 34;
    pool->count = 0;
    prefix.count_published = true;
    prefix.last_store_pc = 39;
    pool->strip_digits = true;
    prefix.strip_digits_published = true;
    prefix.last_store_pc = 44;
    if (out != NULL)
        *out = prefix;
    return success(error);
}

mc_sound_status mc_sound_pool_create(mc_java_random_profile *profile, size_t max_allocation_bytes,
                                     mc_sound_pool **out, mc_sound_error *error) {
    if (profile == NULL || out == NULL)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool constructor arguments", -1,
                    MC_SOUND_EXCEPTION_NONE);
    mc_sound_pool *pool = NULL;
    mc_sound_status status = mc_sound_pool_allocate_raw(max_allocation_bytes, &pool, error);
    if (status != MC_SOUND_OK)
        return status;
    status = mc_sound_pool_construct_raw(pool, profile, NULL, error);
    if (status != MC_SOUND_OK) {
        (void)mc_sound_pool_free(pool, NULL);
        return status;
    }
    *out = pool;
    return success(error);
}

static mc_sound_status mc_sound_pool_free_body(mc_sound_pool *pool, mc_sound_error *error) {
    if (pool != NULL) {
        if (pool->registration != NULL)
            return fail(error, MC_SOUND_BUSY, "SoundPool active registration retirement", -1,
                        MC_SOUND_EXCEPTION_NONE);
        pool_allocation *allocation = pool->allocations;
        while (allocation != NULL) {
            pool_allocation *next = allocation->header.next;
            free(allocation);
            allocation = next;
        }
        free(native_pool(pool));
    }
    return success(error);
}

static mc_sound_status mc_sound_pool_set_strip_digits_body(mc_sound_pool *pool, bool strip_digits,
                                                           mc_sound_error *error) {
    if (pool == NULL)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool digit flag owner", -1,
                    MC_SOUND_EXCEPTION_NONE);
    if (pool->registration != NULL)
        return fail(error, MC_SOUND_BUSY, "SoundPool active registration flag", -1,
                    MC_SOUND_EXCEPTION_NONE);
    pool->strip_digits = strip_digits;
    return success(error);
}

mc_sound_status mc_sound_pool_begin(mc_sound_pool *pool, mc_sound_text logical_name,
                                    mc_sound_pool_registration **out, mc_sound_pool_report *report,
                                    mc_sound_error *error) {
    report_empty(report);
    if (pool == NULL || out == NULL)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool registration arguments", -1,
                    MC_SOUND_EXCEPTION_NONE);
    if (pool->registration != NULL)
        return fail(error, MC_SOUND_BUSY, "SoundPool active registration", -1,
                    MC_SOUND_EXCEPTION_NONE);
    mc_sound_status status = text_admit(logical_name, error);
    if (status != MC_SOUND_OK)
        return status;
    int32_t pc = -1;
    mc_sound_source_exception exception = MC_SOUND_EXCEPTION_NONE;
    size_t key_length = 0;
    if (logical_name.is_null) {
        pc = 7;
        exception = MC_SOUND_EXCEPTION_NULL_NAME;
    } else {
        while (key_length < logical_name.length && logical_name.units[key_length] != '.')
            ++key_length;
        if (key_length == logical_name.length) {
            pc = 10;
            exception = MC_SOUND_EXCEPTION_STRING_INDEX;
        } else if (pool->strip_digits) {
            while (key_length != 0 && decimal_unit(logical_name.units[key_length - 1]))
                --key_length;
            if (key_length == 0) {
                pc = 28;
                exception = MC_SOUND_EXCEPTION_STRING_INDEX;
            }
        }
    }
    if (exception != MC_SOUND_EXCEPTION_NONE) {
        if (report != NULL)
            *report = (mc_sound_pool_report){.source_pc = pc, .exception = exception};
        return fail(error, MC_SOUND_SOURCE_EXCEPTION, "SoundPool logical name", pc, exception);
    }
    const size_t name_bytes = logical_name.length * sizeof(uint16_t);
    mc_sound_pool_registration *registration =
        allocate(pool, sizeof(*registration) + name_bytes, &status);
    if (registration == NULL)
        return fail(error, status, "SoundPool registration storage", -1, MC_SOUND_EXCEPTION_NONE);
    *registration = (mc_sound_pool_registration){.pool = pool, .report = {.source_pc = 91}};
    uint16_t *name_copy = (uint16_t *)(registration + 1);
    if (name_bytes != 0)
        memcpy(name_copy, logical_name.units, name_bytes);
    registration->logical_name = (mc_sound_text){.units = name_copy, .length = logical_name.length};
    pool_group *group = find_group(pool, logical_name, key_length, true);
    if (group == NULL) {
        if (pool->group_count == MC_SOUND_POOL_MAX_GROUPS) {
            release(pool, registration);
            if (report != NULL)
                report->source_pc = 86;
            return fail(error, MC_SOUND_LIMIT, "SoundPool groups", 86, MC_SOUND_EXCEPTION_NONE);
        }
        group = allocate(pool, sizeof(*group) + key_length * sizeof(uint16_t), &status);
        if (group == NULL) {
            release(pool, registration);
            if (report != NULL)
                report->source_pc = 86;
            return fail(error, status, "SoundPool group storage", 86, MC_SOUND_EXCEPTION_NONE);
        }
        *group = (pool_group){0};
        uint16_t *units = (uint16_t *)(group + 1);
        for (size_t i = 0; i < key_length; ++i)
            units[i] = logical_name.units[i] == '/' ? '.' : logical_name.units[i];
        group->key = (mc_sound_text){.units = units, .length = key_length};
        if (pool->last_group != NULL)
            pool->last_group->next = group;
        else
            pool->groups = group;
        pool->last_group = group;
        ++pool->group_count;
        registration->report.group_created = true;
    }
    registration->group = group;
    pool->registration = registration;
    *out = registration;
    if (report != NULL)
        *report = registration->report;
    return success(error);
}

/* Native filenames have no embedded NUL by their C-string representation.
 * Reject ill-formed UTF-8 before copying; URL.path itself remains raw UTF-16. */
static mc_sound_status path_size(const char *path, size_t *bytes) {
    if (path == NULL)
        return MC_SOUND_INVALID_ARGUMENT;
    size_t size = 0;
    while (size <= MC_SOUND_POOL_MAX_PATH_BYTES && path[size] != '\0')
        ++size;
    if (size > MC_SOUND_POOL_MAX_PATH_BYTES)
        return MC_SOUND_LIMIT;
    if (size == 0)
        return MC_SOUND_INVALID_ARGUMENT;
    size_t offset = 0;
    while (offset < size) {
        const unsigned char first = (unsigned char)path[offset++];
        uint32_t value;
        size_t continuation;
        uint32_t minimum;
        if (first < 0x80)
            continue;
        if (first >= 0xc2 && first <= 0xdf) {
            value = first & 0x1fu;
            continuation = 1;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            value = first & 0x0fu;
            continuation = 2;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            value = first & 0x07u;
            continuation = 3;
            minimum = 0x10000;
        } else {
            return MC_SOUND_INVALID_ARGUMENT;
        }
        if (continuation > size - offset)
            return MC_SOUND_INVALID_ARGUMENT;
        for (size_t i = 0; i < continuation; ++i) {
            const unsigned char unit = (unsigned char)path[offset++];
            if ((unit & 0xc0u) != 0x80u)
                return MC_SOUND_INVALID_ARGUMENT;
            value = (value << 6) | (unit & 0x3fu);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return MC_SOUND_INVALID_ARGUMENT;
    }
    *bytes = size + 1;
    return MC_SOUND_OK;
}

mc_sound_status mc_sound_pool_commit(mc_sound_pool_registration **registration_address,
                                     const mc_sound_file_identity *identity,
                                     const mc_sound_pool_entry **out, mc_sound_pool_report *report,
                                     mc_sound_error *error) {
    report_empty(report);
    if (registration_address == NULL || *registration_address == NULL || out == NULL)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool commit arguments", -1,
                    MC_SOUND_EXCEPTION_NONE);
    mc_sound_pool_registration *registration = *registration_address;
    mc_sound_pool *pool = registration->pool;
    if (registration->attempted)
        return registration_result(registration, report, error, MC_SOUND_BUSY,
                                   "SoundPool terminal registration", -1, MC_SOUND_EXCEPTION_NONE);
    registration->attempted = true;
    if (identity == NULL)
        return registration_result(registration, report, error, MC_SOUND_SOURCE_EXCEPTION,
                                   "SoundPool null File", 98, MC_SOUND_EXCEPTION_NULL_FILE);
    size_t native_bytes = 0;
    mc_sound_status status = path_size(identity->native_utf8_path, &native_bytes);
    if (status != MC_SOUND_OK)
        return registration_result(registration, report, error, status,
                                   "SoundPool native file path", 98, MC_SOUND_EXCEPTION_NONE);
    status = text_admit(identity->url_path, error);
    if (status != MC_SOUND_OK || identity->url_path.is_null)
        return registration_result(registration, report, error,
                                   status != MC_SOUND_OK ? status : MC_SOUND_INVALID_ARGUMENT,
                                   "SoundPool resolved URL.path", 101, MC_SOUND_EXCEPTION_NONE);
    if (pool->owned_count == MC_SOUND_POOL_MAX_ENTRIES)
        return registration_result(registration, report, error, MC_SOUND_LIMIT,
                                   "SoundPool owned entries", 104, MC_SOUND_EXCEPTION_NONE);
    const size_t name_bytes = registration->logical_name.length * sizeof(uint16_t);
    const size_t url_bytes = identity->url_path.length * sizeof(uint16_t);
    mc_sound_pool_entry *entry =
        allocate(pool, sizeof(*entry) + name_bytes + url_bytes + native_bytes, &status);
    if (entry == NULL)
        return registration_result(registration, report, error, status, "SoundPool entry storage",
                                   104, MC_SOUND_EXCEPTION_NONE);
    *entry = (mc_sound_pool_entry){.next_owned = pool->owned};
    uint16_t *name = (uint16_t *)(entry + 1);
    uint16_t *url = name + registration->logical_name.length;
    char *native = (char *)(url + identity->url_path.length);
    if (name_bytes != 0)
        memcpy(name, registration->logical_name.units, name_bytes);
    if (url_bytes != 0)
        memcpy(url, identity->url_path.units, url_bytes);
    memcpy(native, identity->native_utf8_path, native_bytes);
    entry->view = (mc_sound_pool_entry_view){
        .logical_name = {.units = name, .length = registration->logical_name.length},
        .file = {.native_utf8_path = native,
                 .url_path = {.units = url, .length = identity->url_path.length}}};
    pool->owned = entry;
    ++pool->owned_count;
    registration->report.entry_created = true;
    pool_group *group = registration->group;
    status = grow(pool, &group->entries, &group->capacity, group->count);
    if (status != MC_SOUND_OK)
        return registration_result(registration, report, error, status, "SoundPool group append",
                                   124, MC_SOUND_EXCEPTION_NONE);
    group->entries[group->count++] = entry;
    registration->report.group_appended = true;
    status = grow(pool, &pool->entries, &pool->entry_capacity, pool->entry_count);
    if (status != MC_SOUND_OK)
        return registration_result(registration, report, error, status,
                                   "SoundPool full-list append", 136, MC_SOUND_EXCEPTION_NONE);
    pool->entries[pool->entry_count++] = entry;
    registration->report.full_list_appended = true;
    pool->count = mc_java_i32_add(pool->count, 1);
    registration->report.count_incremented = true;
    registration_result(registration, report, error, MC_SOUND_OK, NULL, 154,
                        MC_SOUND_EXCEPTION_NONE);
    *out = entry;
    mc_sound_pool_registration_free(registration);
    *registration_address = NULL;
    return MC_SOUND_OK;
}

void mc_sound_pool_registration_free(mc_sound_pool_registration *registration) {
    if (registration != NULL) {
        mc_sound_pool *pool = registration->pool;
        pool->registration = NULL;
        release(pool, registration);
    }
}

mc_sound_status mc_sound_pool_add_file(mc_sound_pool *pool, mc_sound_text logical_name,
                                       const char *native_utf8_path,
                                       const mc_sound_pool_entry **out,
                                       mc_sound_pool_report *report, mc_sound_error *error) {
    if (out == NULL) {
        report_empty(report);
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool file output", -1,
                    MC_SOUND_EXCEPTION_NONE);
    }
    mc_sound_pool_registration *registration = NULL;
    mc_sound_status status = mc_sound_pool_begin(pool, logical_name, &registration, report, error);
    if (status != MC_SOUND_OK)
        return status;
    mc_sound_file *file = NULL;
    status = mc_sound_file_resolve(native_utf8_path, &file, error);
    if (status == MC_SOUND_OK) {
        mc_sound_file_identity identity;
        if (!mc_sound_file_read_identity(file, &identity)) {
            status =
                registration_result(registration, report, error, MC_SOUND_INVALID_ARGUMENT,
                                    "SoundPool resolved identity", 101, MC_SOUND_EXCEPTION_NONE);
        } else {
            status = mc_sound_pool_commit(&registration, &identity, out, report, error);
        }
    } else {
        registration->report.source_pc = 98;
        registration->report.exception = error != NULL ? error->exception : MC_SOUND_EXCEPTION_NONE;
        if (native_utf8_path == NULL)
            registration->report.exception = MC_SOUND_EXCEPTION_NULL_FILE;
        if (report != NULL)
            *report = registration->report;
        if (error != NULL)
            error->source_pc = 98;
    }
    mc_sound_file_free(file);
    mc_sound_pool_registration_free(registration);
    return status;
}

static mc_sound_status select_entries(mc_sound_pool *pool,
                                      const mc_sound_pool_entry *const *entries, size_t count,
                                      const mc_sound_pool_entry **out, mc_sound_pool_report *report,
                                      mc_sound_error *error) {
    if (count == 0) {
        if (report != NULL)
            *report = (mc_sound_pool_report){.source_pc = 31,
                                             .exception = MC_SOUND_EXCEPTION_RANDOM_BOUND};
        return fail(error, MC_SOUND_SOURCE_EXCEPTION, "SoundPool empty group nextInt", 31,
                    MC_SOUND_EXCEPTION_RANDOM_BOUND);
    }
    int32_t index;
    if (!mc_java_random_next_bounded(&pool->random, (int32_t)count, &index))
        return fail(error, MC_SOUND_RANDOM_FAILED, "SoundPool nextInt", 31,
                    MC_SOUND_EXCEPTION_NONE);
    *out = entries[(size_t)index];
    if (report != NULL)
        report->source_pc = 42;
    return success(error);
}

static mc_sound_status mc_sound_pool_select_body(mc_sound_pool *pool, mc_sound_text key,
                                                 const mc_sound_pool_entry **out,
                                                 mc_sound_pool_report *report,
                                                 mc_sound_error *error) {
    report_empty(report);
    if (pool == NULL || out == NULL)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool selection arguments", -1,
                    MC_SOUND_EXCEPTION_NONE);
    if (pool->registration != NULL)
        return fail(error, MC_SOUND_BUSY, "SoundPool active registration selection", -1,
                    MC_SOUND_EXCEPTION_NONE);
    mc_sound_status status = text_admit(key, error);
    if (status != MC_SOUND_OK)
        return status;
    pool_group *group = find_group(pool, key, key.length, false);
    if (group == NULL) {
        *out = NULL;
        if (report != NULL)
            report->source_pc = 19;
        return success(error);
    }
    return select_entries(pool, group->entries, group->count, out, report, error);
}

static mc_sound_status mc_sound_pool_select_any_body(mc_sound_pool *pool,
                                                     const mc_sound_pool_entry **out,
                                                     mc_sound_pool_report *report,
                                                     mc_sound_error *error) {
    report_empty(report);
    if (pool == NULL || out == NULL)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool any-selection arguments", -1,
                    MC_SOUND_EXCEPTION_NONE);
    if (pool->registration != NULL)
        return fail(error, MC_SOUND_BUSY, "SoundPool active registration any-selection", -1,
                    MC_SOUND_EXCEPTION_NONE);
    if (pool->entry_count == 0) {
        *out = NULL;
        if (report != NULL)
            report->source_pc = 13;
        return success(error);
    }
    return select_entries(pool, pool->entries, pool->entry_count, out, report, error);
}

static mc_sound_status mc_sound_pool_read_state_body(const mc_sound_pool *pool,
                                                     mc_sound_pool_state *out,
                                                     mc_sound_error *error) {
    if (pool == NULL || out == NULL)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool state arguments", -1,
                    MC_SOUND_EXCEPTION_NONE);
    *out = (mc_sound_pool_state){.strip_digits = pool->strip_digits,
                                 .registration_active = pool->registration != NULL,
                                 .count = pool->count,
                                 .group_count = pool->group_count,
                                 .full_entry_count = pool->entry_count,
                                 .owned_entry_count = pool->owned_count,
                                 .allocation_bytes = pool->allocation_bytes,
                                 .allocation_limit = pool->allocation_limit,
                                 .random = pool->random};
    return success(error);
}

static const pool_group *group_at(const mc_sound_pool *pool, size_t index) {
    const pool_group *group = pool->groups;
    while (group != NULL && index != 0) {
        group = group->next;
        --index;
    }
    return group;
}

static mc_sound_status mc_sound_pool_read_group_body(const mc_sound_pool *pool, size_t index,
                                                     mc_sound_pool_group_view *out,
                                                     mc_sound_error *error) {
    if (pool == NULL || out == NULL || index >= pool->group_count)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool group index", -1,
                    MC_SOUND_EXCEPTION_NONE);
    const pool_group *group = group_at(pool, index);
    *out = (mc_sound_pool_group_view){.key = group->key, .entry_count = group->count};
    return success(error);
}

static mc_sound_status mc_sound_pool_group_entry_body(const mc_sound_pool *pool, size_t group,
                                                      size_t index, const mc_sound_pool_entry **out,
                                                      mc_sound_error *error) {
    if (pool == NULL || out == NULL || group >= pool->group_count)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool group-entry index", -1,
                    MC_SOUND_EXCEPTION_NONE);
    const pool_group *selected = group_at(pool, group);
    if (index >= selected->count)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool group-entry index", -1,
                    MC_SOUND_EXCEPTION_NONE);
    *out = selected->entries[index];
    return success(error);
}

static mc_sound_status mc_sound_pool_entry_at_body(const mc_sound_pool *pool, size_t index,
                                                   const mc_sound_pool_entry **out,
                                                   mc_sound_error *error) {
    if (pool == NULL || out == NULL || index >= pool->entry_count)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool full-list index", -1,
                    MC_SOUND_EXCEPTION_NONE);
    *out = pool->entries[index];
    return success(error);
}

mc_sound_status mc_sound_pool_entry_read(const mc_sound_pool_entry *entry,
                                         mc_sound_pool_entry_view *out, mc_sound_error *error) {
    if (entry == NULL || out == NULL)
        return fail(error, MC_SOUND_INVALID_ARGUMENT, "SoundPool entry view", -1,
                    MC_SOUND_EXCEPTION_NONE);
    *out = entry->view;
    return success(error);
}

static mc_sound_status pool_enter(mc_sound_pool *pool, mc_sound_error *e) {
    if (pool == NULL)
        return fail(e, MC_SOUND_INVALID_ARGUMENT, "SoundPool native receiver", -1,
                    MC_SOUND_EXCEPTION_NONE);
    pool_native *n = native_pool(pool);
    if (mtx_lock(&n->monitor) != thrd_success)
        return fail(e, MC_SOUND_IO_FAILED, "SoundPool native monitor lock", -1,
                    MC_SOUND_EXCEPTION_NONE);
    if (n->active) {
        mtx_unlock(&n->monitor);
        return fail(e, MC_SOUND_BUSY, "SoundPool same-thread native reentry", -1,
                    MC_SOUND_EXCEPTION_NONE);
    }
    n->active = true;
    return MC_SOUND_OK;
}

static mc_sound_status pool_leave(mc_sound_pool *pool, mc_sound_status status, mc_sound_error *e) {
    pool_native *n = native_pool(pool);
    n->active = false;
    if (mtx_unlock(&n->monitor) != thrd_success)
        return fail(e, MC_SOUND_IO_FAILED, "SoundPool native monitor unlock", -1,
                    MC_SOUND_EXCEPTION_NONE);
    return status;
}

mc_sound_status mc_sound_pool_free(mc_sound_pool *pool, mc_sound_error *e) {
    if (pool == NULL)
        return success(e);
    mc_sound_status s = pool_enter(pool, e);
    if (s != MC_SOUND_OK)
        return s;
    if (pool->registration != NULL)
        return pool_leave(pool,
                          fail(e, MC_SOUND_BUSY, "SoundPool active registration retirement", -1,
                               MC_SOUND_EXCEPTION_NONE),
                          e);
    /* Caller proves all worker/borrowed-entry users ended; no concurrent final
     * free is admitted. Unlock before destroying the sole native header. */
    s = pool_leave(pool, MC_SOUND_OK, e);
    if (s != MC_SOUND_OK)
        return s;
    mtx_destroy(&native_pool(pool)->monitor);
    return mc_sound_pool_free_body(pool, e);
}

mc_sound_status mc_sound_pool_set_strip_digits(mc_sound_pool *p, bool b, mc_sound_error *e) {
    if (p == NULL)
        return mc_sound_pool_set_strip_digits_body(p, b, e);
    mc_sound_status s = pool_enter(p, e);
    return s != MC_SOUND_OK ? s : pool_leave(p, mc_sound_pool_set_strip_digits_body(p, b, e), e);
}

mc_sound_status mc_sound_pool_select(mc_sound_pool *p, mc_sound_text key,
                                     const mc_sound_pool_entry **out, mc_sound_pool_report *r,
                                     mc_sound_error *e) {
    if (p == NULL || out == NULL)
        return mc_sound_pool_select_body(p, key, out, r, e);
    mc_sound_status s = pool_enter(p, e);
    return s != MC_SOUND_OK ? s : pool_leave(p, mc_sound_pool_select_body(p, key, out, r, e), e);
}

mc_sound_status mc_sound_pool_select_any(mc_sound_pool *p, const mc_sound_pool_entry **out,
                                         mc_sound_pool_report *r, mc_sound_error *e) {
    if (p == NULL || out == NULL)
        return mc_sound_pool_select_any_body(p, out, r, e);
    mc_sound_status s = pool_enter(p, e);
    return s != MC_SOUND_OK ? s : pool_leave(p, mc_sound_pool_select_any_body(p, out, r, e), e);
}

mc_sound_status mc_sound_pool_read_state(const mc_sound_pool *p, mc_sound_pool_state *out,
                                         mc_sound_error *e) {
    if (p == NULL || out == NULL)
        return mc_sound_pool_read_state_body(p, out, e);
    mc_sound_pool *m = (mc_sound_pool *)(void *)p;
    mc_sound_status s = pool_enter(m, e);
    return s != MC_SOUND_OK ? s : pool_leave(m, mc_sound_pool_read_state_body(p, out, e), e);
}

mc_sound_status mc_sound_pool_read_group(const mc_sound_pool *p, size_t i,
                                         mc_sound_pool_group_view *out, mc_sound_error *e) {
    if (p == NULL || out == NULL)
        return mc_sound_pool_read_group_body(p, i, out, e);
    mc_sound_pool *m = (mc_sound_pool *)(void *)p;
    mc_sound_status s = pool_enter(m, e);
    return s != MC_SOUND_OK ? s : pool_leave(m, mc_sound_pool_read_group_body(p, i, out, e), e);
}

mc_sound_status mc_sound_pool_group_entry(const mc_sound_pool *p, size_t g, size_t i,
                                          const mc_sound_pool_entry **out, mc_sound_error *e) {
    if (p == NULL || out == NULL)
        return mc_sound_pool_group_entry_body(p, g, i, out, e);
    mc_sound_pool *m = (mc_sound_pool *)(void *)p;
    mc_sound_status s = pool_enter(m, e);
    return s != MC_SOUND_OK ? s : pool_leave(m, mc_sound_pool_group_entry_body(p, g, i, out, e), e);
}

mc_sound_status mc_sound_pool_entry_at(const mc_sound_pool *p, size_t i,
                                       const mc_sound_pool_entry **out, mc_sound_error *e) {
    if (p == NULL || out == NULL)
        return mc_sound_pool_entry_at_body(p, i, out, e);
    mc_sound_pool *m = (mc_sound_pool *)(void *)p;
    mc_sound_status s = pool_enter(m, e);
    return s != MC_SOUND_OK ? s : pool_leave(m, mc_sound_pool_entry_at_body(p, i, out, e), e);
}

struct mc_sound_pool_file_call {
    mc_java_uri *uri;
    mc_java_url *url;
    mc_sound_pool_file_error failure;
};

mc_sound_status mc_sound_pool_add_java_file(mc_sound_pool *pool, mc_sound_text name,
                                            mc_java_file *file, mc_sound_pool_file_call **out,
                                            const mc_sound_pool_entry **entry,
                                            mc_sound_pool_file_error *error) {
    mc_sound_pool_file_error e = {0};
    if (out == NULL || entry == NULL) {
        e.status = MC_SOUND_INVALID_ARGUMENT;
        if (error != NULL)
            *error = e;
        return e.status;
    }
    mc_sound_pool_file_call *call = calloc(1, sizeof(*call));
    if (call == NULL) {
        e.status = MC_SOUND_ALLOCATION_FAILED;
        if (error != NULL)
            *error = e;
        return e.status;
    }
    *out = call;
    e.status = pool_enter(pool, &e.pool);
    if (e.status != MC_SOUND_OK)
        goto done;
    mc_sound_pool_registration *registration = NULL;
    e.status = mc_sound_pool_begin(pool, name, &registration, &e.report, &e.pool);
    e.hr_pc = e.report.source_pc;
    if (e.status != MC_SOUND_OK)
        goto leave;
    e.hr_pc = 98;
    if (file == NULL) {
        e.status = MC_SOUND_SOURCE_EXCEPTION;
        e.pool = (mc_sound_error){.status = e.status,
                                  .operation = "actual hr null File receiver",
                                  .source_pc = 98,
                                  .exception = MC_SOUND_EXCEPTION_NULL_FILE};
        goto leave;
    }
    if (mc_java_file_to_uri(file, &call->uri, &e.url) != MC_JAVA_URL_OK) {
        e.status = e.url.status == MC_JAVA_URL_BUSY         ? MC_SOUND_BUSY
                   : e.url.status == MC_JAVA_URL_LIMIT      ? MC_SOUND_LIMIT
                   : e.url.status == MC_JAVA_URL_ALLOCATION ? MC_SOUND_ALLOCATION_FAILED
                                                            : MC_SOUND_UNSUPPORTED_PATH;
        goto leave;
    }
    e.hr_pc = 101;
    if (mc_java_uri_to_url(call->uri, &call->url, &e.url) != MC_JAVA_URL_OK) {
        e.status = MC_SOUND_UNSUPPORTED_PATH;
        e.print_stack_trace_pending = e.url.source_exception;
        goto leave;
    }
    mc_java_url_view view;
    if (!mc_java_url_inspect(call->url, &view) || !view.file) {
        e.status = MC_SOUND_INVALID_ARGUMENT;
        goto leave;
    }
    mc_sound_file_identity identity = {view.native_file,
                                       {view.file_url_path, view.file_url_length, false}};
    e.status = mc_sound_pool_commit(&registration, &identity, entry, &e.report, &e.pool);
    e.hr_pc = e.report.source_pc;
    if (e.status == MC_SOUND_OK) {
        e.hr_pc = 154;
        e.source_returned = true;
    }
leave:
    /* Native token retirement cannot remove any reached source pool effect.
     * No thread exits while owning this monitor or a split registration. */
    mc_sound_pool_registration_free(registration);
    e.status = pool_leave(pool, e.status, &e.pool);
done:
    call->failure = e;
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_sound_status mc_sound_pool_file_call_release(mc_sound_pool_file_call **slot,
                                                mc_sound_pool_file_error *error) {
    mc_sound_pool_file_error e = {0};
    if (slot == NULL) {
        e.status = MC_SOUND_INVALID_ARGUMENT;
        if (error != NULL)
            *error = e;
        return e.status;
    }
    mc_sound_pool_file_call *c = *slot;
    if (c != NULL) {
        if (mc_java_url_release(&c->url, &e.url) != MC_JAVA_URL_OK ||
            mc_java_uri_release(&c->uri, &e.url) != MC_JAVA_URL_OK) {
            e.status = e.url.status == MC_JAVA_URL_BUSY ? MC_SOUND_BUSY : MC_SOUND_IO_FAILED;
            if (error != NULL)
                *error = e;
            return e.status;
        }
        free(c);
        *slot = NULL;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
