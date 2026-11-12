/* Genuine NW singleton, source setters/batches and additive-translation calls. */
#include "client/render_engine/internal.h"
#include "client/render_engine/nw_internal.h"
#include "client/render_engine/textures.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "NW translation check%u at%d: %s\n", checks, __LINE__, #value);        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static uint64_t double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t hex(const char *text, size_t digits) {
    CHECK(strlen(text) == digits);
    for (size_t i = 0; i < digits; ++i)
        CHECK((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'));
    char *end;
    errno = 0;
    const unsigned long long value = strtoull(text, &end, 16);
    CHECK(errno == 0 && *end == '\0');
    return (uint64_t)value;
}

static double double_from_hex(const char *text) {
    const uint64_t bits = hex(text, 16);
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float float_from_hex(const char *text) {
    const uint32_t bits = (uint32_t)hex(text, 8);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void triple(const mc_client_nw_fields *fields) {
    printf("%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64, double_bits(fields->s),
           double_bits(fields->t), double_bits(fields->u));
}

static int32_t array_hash(const mc_client_nw_fields *fields) {
    uint32_t hash = 1;
    CHECK(fields->g != NULL && fields->B >= 0);
    for (int32_t i = 0; i < fields->B; ++i)
        hash = hash * 31u + (uint32_t)fields->g[i];
    int32_t signed_hash;
    memcpy(&signed_hash, &hash, sizeof(signed_hash));
    return signed_hash;
}

static const char *boolean(bool value) {
    return value ? "true" : "false";
}

static void other_state(const mc_client_nw_fields *f) {
    CHECK(f->d != NULL && f->e != NULL && f->f != NULL && f->y == NULL);
    printf("A=%" PRId32 ",B=%" PRId32 ",d=%zu/%zu/%zu,e=%zu/%zu/%zu,f=%zu/%zu/%zu,"
           "g=%" PRId32 "/%" PRId32 ",h=%" PRId32 ",i=%016" PRIx64 ",j=%016" PRIx64 ",k=%" PRId32
           ",l=%s,m=%s,n=%s,o=%" PRId32 ",p=%" PRId32 ",q=%s,r=%" PRId32 ",v=%" PRId32
           ",w=%s,x=%s,y=null,z=%" PRId32,
           f->A, f->B, f->d->position, f->d->limit, f->d->capacity, f->e->position, f->e->limit,
           f->e->capacity, f->f->position, f->f->limit, f->f->capacity, f->B, array_hash(f), f->h,
           double_bits(f->i), double_bits(f->j), f->k, boolean(f->l), boolean(f->m), boolean(f->n),
           f->o, f->p, boolean(f->q), f->r, f->v, boolean(f->w), boolean(f->x), f->z);
}

static void idle(mc_client_render_engine_classes *classes, mc_client_tessellator *owner,
                 size_t holds) {
    CHECK(classes->active_source_calls == 0 && !owner->active &&
          atomic_load(&classes->holds) == holds && classes->context_lease == NULL &&
          classes->bound_display == NULL);
}

static void native_controls(mc_client_render_engine_classes *classes,
                            mc_client_tessellator *owner) {
    mc_client_graphics_error error;
    CHECK(!mc_client_nw_translation_add(NULL, .0625f, 0, 0, &error) &&
          error.status == MC_CLIENT_GRAPHICS_NULL_REFERENCE && error.native_policy);
    CHECK(mc_client_nw_translation(owner, 16, -48, -32, &error));
    const mc_client_nw_fields before = owner->fields;
    const int32_t pc = owner->last_store_pc;
    const unsigned int saved = _mm_getcsr();
    static const unsigned int masks[] = {0x8000u, 0x0040u, 0x2000u};
    for (size_t i = 0; i < sizeof(masks) / sizeof(*masks); ++i) {
        const unsigned int controls = (saved & ~0x6000u) | masks[i];
        _mm_setcsr(controls);
        const bool result = mc_client_nw_translation_add(owner, .0625f, -.0625f, .0625f, &error);
        const unsigned int retained = _mm_getcsr();
        _mm_setcsr(saved);
        CHECK(!result && error.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP && error.native_policy &&
              !error.cleanup_failed && error.site.pc == 6 &&
              strcmp(error.site.class_name, "nw") == 0 && strcmp(error.site.method, "c") == 0 &&
              strcmp(error.site.descriptor, "(FFF)V") == 0 && retained == controls &&
              owner->last_store_pc == pc && memcmp(&before, &owner->fields, sizeof(before)) == 0);
        idle(classes, owner, 2);
    }
    _mm_setcsr(saved | 0x3fu);
    const bool returned = mc_client_nw_translation_add(owner, .0625f, -.0625f, .0625f, &error);
    const unsigned int retained = _mm_getcsr();
    _mm_setcsr(saved);
    CHECK(returned && error.status == MC_CLIENT_GRAPHICS_OK && !error.cleanup_failed &&
          owner->last_store_pc == 30 && (retained & 0xffc0u) == (saved & 0xffc0u));
    idle(classes, owner, 2);
    mc_client_render_texture_error retirement;
    CHECK(mc_client_render_engine_retire_resources(classes, &retirement) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    const mc_client_nw_fields stopped = owner->fields;
    CHECK(!mc_client_nw_translation_add(owner, 1, 2, 3, &error) &&
          error.status == MC_CLIENT_GRAPHICS_BUSY && error.native_policy && !error.cleanup_failed &&
          owner->last_store_pc == 30 && memcmp(&stopped, &owner->fields, sizeof(stopped)) == 0);
    idle(classes, owner, 2);
}

int main(int argc, char **argv) {
    CHECK(argc == 2 && (_mm_getcsr() & 0xffc0u) == 0x1f80u);
    FILE *input = fopen(argv[1], "rb");
    CHECK(input != NULL);
    mc_client_render_engine_classes *classes = NULL;
    mc_client_tessellator *owner = NULL;
    mc_client_graphics_error error;
    const mc_client_graphics_site caller = {"fixture", "nw_translation", "()V", -1};
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(classes, caller, &owner, &error) &&
          mc_client_render_engine_classes_retain(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(owner == classes->nw.a && classes->nw.b && !classes->nw.c && !owner->fields.x &&
          owner->fields.B == 2097152 && !owner->fields.w);
    size_t profiles = 0, rows = 0;
    char line[1024];
    while (fgets(line, sizeof(line), input) != NULL) {
        const size_t length = strlen(line);
        CHECK(length > 0 && line[length - 1] == '\n');
        line[length - 1] = '\0';
        char *columns[7] = {line};
        for (size_t i = 1; i < 7; ++i) {
            char *separator = strchr(columns[i - 1], '\t');
            CHECK(separator != NULL);
            *separator = '\0';
            columns[i] = separator + 1;
        }
        CHECK(strchr(columns[6], '\t') == NULL && strtoul(columns[0], NULL, 10) == profiles &&
              (strcmp(columns[2], "0") == 0 || strcmp(columns[2], "1") == 0));
        const bool building = strcmp(columns[2], "1") == 0;
        CHECK(mc_client_nw_translation(owner, double_from_hex(columns[3]),
                                       double_from_hex(columns[4]), double_from_hex(columns[5]),
                                       &error));
        if (building)
            CHECK(mc_client_nw_begin_quads(owner, &error));
        CHECK(owner->fields.w == building);
        char *delta = columns[6];
        size_t step = 0;
        while (delta != NULL) {
            char *next = strchr(delta, ';');
            if (next != NULL)
                *next++ = '\0';
            CHECK(strlen(delta) == 26 && delta[8] == ',' && delta[17] == ',');
            char payload[27];
            memcpy(payload, delta, sizeof(payload));
            delta[8] = delta[17] = '\0';
            const float x = float_from_hex(delta), y = float_from_hex(delta + 9),
                        z = float_from_hex(delta + 18);
            const mc_client_nw_fields before = owner->fields;
            const int32_t backing_hash = array_hash(&before);
            CHECK(mc_client_nw_translation_add(owner, x, y, z, &error) &&
                  error.status == MC_CLIENT_GRAPHICS_OK && !error.cleanup_failed &&
                  owner->last_store_pc == 30);
            mc_client_nw_fields after = owner->fields;
            after.s = before.s;
            after.t = before.t;
            after.u = before.u;
            CHECK(memcmp(&after, &before, sizeof(before)) == 0 &&
                  array_hash(&owner->fields) == backing_hash);
            idle(classes, owner, 2);
            printf("NW_OFFSET\t%zu\t%zu\t%s\t%s\t%zu\t", rows++, profiles, columns[1], columns[2],
                   step++);
            triple(&before);
            printf("\t%s\t", payload);
            triple(&owner->fields);
            putchar('\t');
            other_state(&owner->fields);
            putchar('\n');
            delta = next;
        }
        if (building)
            CHECK(mc_client_nw_draw(owner, NULL, &error));
        CHECK(!owner->fields.w);
        ++profiles;
    }
    CHECK(!ferror(input) && fclose(input) == 0 && profiles == 36 && rows == 106);
    native_controls(classes, owner);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK &&
          atomic_load(&classes->holds) == 1);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    printf("PASS: %zu original NW translation rows, %zu profiles, %u native checks\n", rows,
           profiles, checks);
    return 0;
}
