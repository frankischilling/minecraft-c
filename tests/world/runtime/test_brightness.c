#include "mc/world/runtime/brightness.h"

#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/resources/translation/translation.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Brightness check%u at%d: %s\n", checks, __LINE__, #condition);        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const uint32_t VALUES[] = {UINT32_C(0),          UINT32_C(0x80000000), UINT32_C(0x3f800001),
                                  UINT32_C(0xbf800001), UINT32_C(0x00000001), UINT32_C(0x7f800000),
                                  UINT32_C(0xff800000), UINT32_C(0x7fc12345), UINT32_C(0xffc54321),
                                  UINT32_C(0x7fa12345)};

static uint32_t bits(const float *value) {
    uint32_t out;
    memcpy(&out, value, sizeof(out));
    return out;
}

static void install(float *value, uint32_t raw) {
    memcpy(value, &raw, sizeof(raw));
}

typedef struct text_stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} text_stream;

typedef struct fixture {
    mc_math_trig trig;
    mc_java_random_profile *profile;
    mc_materials *materials;
    mc_registry_classes *registry;
    mc_world_runtime_classes *classes, *server_classes;
    mc_world_runtime *world, *server;
    mc_world_provider *current, *replacement, *raw;
    text_stream streams[3];
    mc_registry_resources resources;
    bool replace, replaced, bad, original_caller_owned;
    size_t opens, guarded;
} fixture;

static mc_java_properties_status read_text(void *context, uint8_t *out, size_t capacity,
                                           size_t *count) {
    text_stream *s = context;
    if (s == NULL || out == NULL || count == NULL || capacity == 0 || s->closed)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_text(void *context, const char *name,
                                    mc_java_properties_input *out) {
    fixture *f = context;
    static const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang",
                                        "/achievement/map.txt"};
    if (name == NULL || out == NULL || f->opens >= 3 || strcmp(name, names[f->opens]) != 0) {
        f->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    const size_t index = f->opens++;
    if (f->replace) {
        /* The actual outer World brightness capture and nested fd.n query
         * have independent native call guards. Drop the original source
         * Dimension field's sole preceding hold during Registry reentry.
         * Its float array must survive through the outer faload14. */
        if (f->world->active_calls != 2 ||
            mc_world_runtime_retire_world(f->world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_world_runtime_free(f->world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_world_provider_hold_count(f->current) != (f->replaced ? 1u : 2u)) {
            f->bad = true;
            return MC_REGISTRY_INVALID_ARGUMENT;
        }
        if (!f->replaced) {
            if (mc_world_provider_retain(f->replacement) != MC_WORLD_PROVIDER_OK) {
                f->bad = true;
                return MC_REGISTRY_INVALID_ARGUMENT;
            }
            f->world->fields.t = f->replacement;
            mc_world_provider_release(f->current);
            f->replaced = true;
        }
        ++f->guarded;
    }
    *out = (mc_java_properties_input){&f->streams[index], read_text};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_text(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    if (input == NULL || input->context != &f->streams[2] || input->read != read_text) {
        f->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    f->streams[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool ready_text(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    if (input == NULL || input->read != read_text)
        f->bad = true;
    return false;
}

static bool setup(fixture *f) {
    bool okay = false;
    static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                      "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
    CHECK(mc_math_trig_init(&f->trig));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&f->materials));
    for (size_t index = 0; index < 3; ++index)
        f->streams[index] =
            (text_stream){.bytes = language, .length = index == 0 ? sizeof(language) - 1u : 0u};
    f->resources = (mc_registry_resources){f, open_text, close_text, ready_text};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         &f->resources};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &f->trig, &f->classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes, f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_SERVER, f->profile, &f->trig,
                                          &f->server_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes, NULL,
                                          &f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE, f->server_classes,
                                          NULL, &f->server, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->current, NULL) == MC_WORLD_PROVIDER_OK);
    f->original_caller_owned = true;
    CHECK(mc_world_provider_create(0, NULL, &f->replacement, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_allocate(MC_WORLD_PROVIDER_OVERWORLD, NULL, &f->raw, NULL) ==
          MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_retain(f->current) == MC_WORLD_PROVIDER_OK);
    f->world->fields.t = f->current; /* Authored actual public field on genuine RAW World. */
    okay = true;
cleanup:
    return okay;
}

static bool cases(fixture *f) {
    bool okay = false;
    const uint32_t marker = UINT32_C(0x3f123456);
    float value;
    mc_world_brightness_error error;
    mc_world_provider_fields dimension;
    CHECK(mc_world_provider_fields_get(f->current, &dimension) == MC_WORLD_PROVIDER_OK &&
          dimension.f != NULL);
    float *table = (float *)dimension.f; /* Genuine Java public float array field control. */
    for (size_t index = 0; index < sizeof(VALUES) / sizeof(VALUES[0]); ++index) {
        install(&table[15], VALUES[index]);
        install(&value, marker);
        const size_t holds = mc_world_provider_hold_count(f->current);
        CHECK(mc_world_runtime_brightness(f->world, 32000000, INT32_MIN, 0, &value, &error) ==
                  MC_WORLD_STORAGE_OK &&
              bits(&value) == VALUES[index] && error.bytecode == 15 && error.query_reached &&
              error.light == 15 && error.source_exception == MC_WORLD_BRIGHTNESS_SOURCE_NONE &&
              !error.native_policy && error.operation == NULL &&
              mc_world_provider_hold_count(f->current) == holds && f->world->active_calls == 0);
        printf("BRIGHT %zu %08" PRIx32 "\n", index, bits(&value));
    }
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr((csr & ~0x6000u) | 0xe040u);
    const mc_world_storage_status unusual =
        mc_world_runtime_brightness(f->world, INT32_MAX, 64, 0, &value, &error);
    const unsigned int after_csr = _mm_getcsr();
    _mm_setcsr(csr);
    CHECK(unusual == MC_WORLD_STORAGE_OK && bits(&value) == VALUES[9] &&
          (after_csr & 0xffc0u) == (((csr & ~0x6000u) | 0xe040u) & 0xffc0u));
    install(&value, marker);
    CHECK(mc_world_runtime_brightness(NULL, 32000000, 64, 0, &value, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          bits(&value) == marker && error.native_policy && !error.query_reached);
    CHECK(mc_world_runtime_brightness(f->world, 32000000, 64, 0, NULL, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          error.native_policy && !error.query_reached);
    CHECK(mc_world_runtime_brightness(f->server, 32000000, 64, 0, &value, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          bits(&value) == marker && error.native_policy && !error.query_reached);
    f->world->fields.t = NULL;
    const mc_world_storage_status null_dimension =
        mc_world_runtime_brightness(f->world, 32000000, INT32_MIN, 0, &value, &error);
    f->world->fields.t = f->current;
    CHECK(null_dimension == MC_WORLD_STORAGE_UNSUPPORTED_STATE && bits(&value) == marker &&
          error.bytecode == 4 && !error.query_reached && !error.native_policy &&
          error.source_exception == MC_WORLD_BRIGHTNESS_SOURCE_NULL_DIMENSION);
    printf("BRIGHT_NULL dimension java.lang.NullPointerException\n");
    f->world->fields.t = f->raw;
    const mc_world_storage_status null_table =
        mc_world_runtime_brightness(f->world, 32000000, INT32_MIN, 0, &value, &error);
    f->world->fields.t = f->current;
    CHECK(null_table == MC_WORLD_STORAGE_UNSUPPORTED_STATE && bits(&value) == marker &&
          error.bytecode == 14 && error.query_reached && error.light == 15 &&
          error.source_exception == MC_WORLD_BRIGHTNESS_SOURCE_NULL_TABLE &&
          mc_world_provider_hold_count(f->raw) == 1 && f->world->active_calls == 0);
    printf("BRIGHT_NULL table java.lang.NullPointerException\n");
    /* A missing child dependency wins over a captured NULL array. The parent
     * has not reached faload14. Both genuine World call guards must retire. */
    f->world->fields.t = f->raw;
    const mc_world_storage_status query_failed =
        mc_world_runtime_brightness(f->world, 0, 64, 0, &value, &error);
    f->world->fields.t = f->current;
    CHECK(query_failed == MC_WORLD_STORAGE_UNSUPPORTED_STATE && bits(&value) == marker &&
          error.bytecode == 11 && error.query_reached && error.query.bytecode == 36 &&
          error.source_exception == MC_WORLD_BRIGHTNESS_SOURCE_NONE &&
          mc_world_provider_hold_count(f->raw) == 1 && f->world->active_calls == 0);
    /* Negative Y executes the source neighbor/static prefix then returns zero.
     * Registry initialization synchronously replaces t and releases its old
     * field occurrence. The captured original array must still supply index0. */
    install(&table[0], UINT32_C(0x3f123456));
    mc_world_provider_fields replacement;
    CHECK(mc_world_provider_fields_get(f->replacement, &replacement) == MC_WORLD_PROVIDER_OK);
    install(&((float *)replacement.f)[0], UINT32_C(0x3f654321));
    mc_world_provider_release(f->current); /* Old source field is its only ordinary hold. */
    f->original_caller_owned = false;
    f->replace = true;
    const mc_world_storage_status captured =
        mc_world_runtime_brightness(f->world, 0, INT32_MIN, 0, &value, &error);
    if (f->replaced)
        f->current = NULL; /* Its final brightness occurrence has ended. */
    CHECK(captured == MC_WORLD_STORAGE_OK && bits(&value) == UINT32_C(0x3f123456) &&
          error.bytecode == 15 && error.light == 0 && error.query_reached && !f->bad &&
          f->opens == 3 && f->guarded == 3 && f->replaced && f->world->fields.t == f->replacement &&
          mc_world_provider_hold_count(f->replacement) == 2 && f->world->active_calls == 0);
    okay = true;
cleanup:
    return okay;
}

static bool close_fixture(fixture *f) {
    if (f == NULL)
        return true;
    if (f->world != NULL && (mc_world_runtime_retire_world(f->world, NULL) != MC_WORLD_STORAGE_OK ||
                             mc_world_runtime_free(f->world, NULL) != MC_WORLD_STORAGE_OK))
        return false;
    if (f->server != NULL &&
        (mc_world_runtime_retire_world(f->server, NULL) != MC_WORLD_STORAGE_OK ||
         mc_world_runtime_free(f->server, NULL) != MC_WORLD_STORAGE_OK))
        return false;
    if (f->original_caller_owned)
        mc_world_provider_release(f->current);
    mc_world_provider_release(f->replacement);
    mc_world_provider_release(f->raw);
    if (mc_world_runtime_classes_free(f->classes, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_classes_free(f->server_classes, NULL) != MC_WORLD_STORAGE_OK ||
        mc_registry_classes_free(f->registry, NULL) != MC_REGISTRY_OK)
        return false;
    mc_materials_destroy(f->materials);
    mc_java_random_profile_free(f->profile);
    free(f);
    return true;
}

int main(void) {
    fixture *f = calloc(1, sizeof(*f));
    const bool okay = f != NULL && setup(f) && cases(f);
    if (!close_fixture(f)) {
        fputs("Brightness cleanup retained live owners\n", stderr);
        return 1;
    }
    if (!okay)
        return 1;
    printf("%u brightness checks passed\n", checks);
    return 0;
}
