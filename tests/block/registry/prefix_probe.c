#include "mc/block/registry/registry.h"

#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#define REFERENCE_CAPACITY 1024u
#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

_Static_assert(sizeof(float) == sizeof(uint32_t), "protocol requires binary32 storage");
_Static_assert(sizeof(double) == sizeof(uint64_t), "protocol requires binary64 storage");
_Static_assert(MC_MATERIAL_COUNT == 28, "protocol requires 28 material roots");

/* Declaring-class field names in original declaration order. These select
 * public owner storage; they contain no expected Block values or identities. */
static const char *const block_names[] = {
    "u",  "v",  "w",  "x",  "y",  "z",  "A",  "B",  "C",  "D",  "E",  "F",  "G",  "H",  "I",  "J",
    "K",  "L",  "M",  "N",  "O",  "P",  "Q",  "R",  "S",  "T",  "U",  "V",  "W",  "X",  "Y",  "Z",
    "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj", "ak", "al", "am", "an", "ao", "ap",
    "aq", "ar", "as", "at", "au", "av", "aw", "ax", "ay", "az", "aA", "aB", "aC", "aD", "aE", "aF",
    "aG", "aH", "aI", "aJ", "aK", "aL", "aM", "aN", "aO", "aP", "aQ", "aR", "aS", "aT", "aU", "aV",
    "aW", "aX", "aY", "aZ", "ba", "bb", "bc", "bd", "be", "bf", "bg", "bh", "bi", "bj", "bk", "bl"};
_Static_assert(COUNT(block_names) == 96, "protocol requires 96 named fields");

typedef struct probe {
    const void *references[REFERENCE_CAPACITY];
    const mc_registry_string *strings[REFERENCE_CAPACITY];
    size_t reference_count, string_count, rows;
    bool failed;
} probe;

static bool fail(probe *p, const char *message)
{
    if (!p->failed)
        fprintf(stderr, "registry prefix probe: %s\n", message);
    p->failed = true;
    return false;
}

static bool fragment(probe *p, const char *format, ...)
{
    if (p->failed)
        return false;
    va_list arguments;
    va_start(arguments, format);
    int written = vprintf(format, arguments);
    va_end(arguments);
    if (written < 0)
        return fail(p, "stdout write failed");
    return true;
}

static bool row(probe *p, const char *format, ...)
{
    if (p->failed)
        return false;
    va_list arguments;
    va_start(arguments, format);
    int written = vprintf(format, arguments);
    va_end(arguments);
    if (written < 0)
        return fail(p, "stdout write failed");
    ++p->rows;
    return true;
}

static bool reference(probe *p, const void *value, const mc_registry_string *string, char token[32])
{
    if (value == NULL) {
        if (string != NULL)
            return fail(p, "invalid String reference enrollment");
        memcpy(token, "NULL", sizeof("NULL"));
        return true;
    }
    size_t index = 0;
    while (index < p->reference_count && p->references[index] != value)
        ++index;
    if (index == p->reference_count) {
        if (p->reference_count == REFERENCE_CAPACITY)
            return fail(p, "reference capacity exceeded");
        p->references[p->reference_count++] = value;
    }
    if (string != NULL && p->strings[index] == NULL) {
        p->strings[index] = string;
        ++p->string_count;
    }
    int length = snprintf(token, 32, "R:%zu", index + 1);
    if (length < 0 || length >= 32)
        return fail(p, "reference token formatting failed");
    return true;
}

static bool reference_row(probe *p, const char *category, const char *root, const char *field,
                          const void *value, const mc_registry_string *string)
{
    char token[32];
    if (!reference(p, value, string, token))
        return false;
    if (field == NULL)
        return row(p, "%s\t%s\t%s\n", category, root, token);
    return row(p, "%s\t%s\t%s\t%s\n", category, root, field, token);
}

static bool int_row(probe *p, const char *category, const char *root, const char *field,
                    int32_t value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return row(p, "%s\t%s\t%s\tI:%08" PRIx32 "\n", category, root, field, bits);
}

static bool float_row(probe *p, const char *category, const char *root, const char *field,
                      float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return row(p, "%s\t%s\t%s\tF:%08" PRIx32 "\n", category, root, field, bits);
}

static bool double_row(probe *p, const char *root, const char *field, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return row(p, "BLOCK\t%s\t%s\tD:%016" PRIx64 "\n", root, field, bits);
}

static bool bool_row(probe *p, const char *category, const char *root, const char *field,
                     bool value)
{
    return row(p, "%s\t%s\t%s\tZ:%u\n", category, root, field, value ? 1u : 0u);
}

static bool sound_rows(probe *p, const char *root, const mc_block_sound *sound)
{
    const mc_block_sound_fields *f = mc_block_sound_fields_get(sound);
    if (f == NULL)
        return fail(p, "missing sound fields");
    return reference_row(p, "SOUND", root, "a", f->a, f->a) &&
           float_row(p, "SOUND", root, "b", f->b) && float_row(p, "SOUND", root, "c", f->c);
}

static bool block_rows(probe *p, const char *root, const mc_block *block)
{
    const mc_block_fields *f = mc_block_fields_get(block);
    if (f == NULL)
        return fail(p, "missing Block fields");
    return int_row(p, "BLOCK", root, "bm", f->bm) && int_row(p, "BLOCK", root, "bn", f->bn) &&
           float_row(p, "BLOCK", root, "bo", f->bo) && float_row(p, "BLOCK", root, "bp", f->bp) &&
           bool_row(p, "BLOCK", root, "bq", f->bq) && bool_row(p, "BLOCK", root, "br", f->br) &&
           double_row(p, root, "bs", f->bs) && double_row(p, root, "bt", f->bt) &&
           double_row(p, root, "bu", f->bu) && double_row(p, root, "bv", f->bv) &&
           double_row(p, root, "bw", f->bw) && double_row(p, root, "bx", f->bx) &&
           reference_row(p, "BLOCK", root, "by", f->by, NULL) &&
           float_row(p, "BLOCK", root, "bz", f->bz) &&
           reference_row(p, "BLOCK", root, "bA", f->bA, NULL) &&
           float_row(p, "BLOCK", root, "bB", f->bB) &&
           reference_row(p, "BLOCK", root, "a", f->a, f->a);
}

static bool element_rows(probe *p, const mc_registry_block_view *view)
{
    for (size_t array = 0; array < 8; ++array) {
        char root[2] = {(char)('m' + (int)array), '\0'};
        for (size_t i = 0; i < 256; ++i) {
            char index[16];
            int length = snprintf(index, sizeof(index), "%zu", i);
            if (length < 0 || (size_t)length >= sizeof(index))
                return fail(p, "element index formatting failed");
            bool ok = false;
            switch (array) {
                case 0:
                    ok = reference_row(p, "ELEMENT", root, index, view->m[i], NULL);
                    break;
                case 1:
                    ok = bool_row(p, "ELEMENT", root, index, view->n[i]);
                    break;
                case 2:
                    ok = bool_row(p, "ELEMENT", root, index, view->o[i]);
                    break;
                case 3:
                    ok = bool_row(p, "ELEMENT", root, index, view->p[i]);
                    break;
                case 4:
                    ok = int_row(p, "ELEMENT", root, index, view->q[i]);
                    break;
                case 5:
                    ok = bool_row(p, "ELEMENT", root, index, view->r[i]);
                    break;
                case 6:
                    ok = int_row(p, "ELEMENT", root, index, view->s[i]);
                    break;
                case 7:
                    ok = bool_row(p, "ELEMENT", root, index, view->t[i]);
                    break;
                default:
                    break;
            }
            if (!ok)
                return false;
        }
    }
    return true;
}

static bool string_rows(probe *p)
{
    for (size_t i = 0; i < p->reference_count; ++i) {
        if (p->strings[i] == NULL)
            continue;
        mc_registry_string_view view;
        if (!mc_registry_string_read(p->strings[i], &view) ||
            (view.length != 0 && view.units == NULL))
            return fail(p, "invalid String view");
        if (!fragment(p, "STRING\t%zu\t%zu\t", i + 1, view.length))
            return false;
        if (view.length == 0) {
            if (!fragment(p, "-"))
                return false;
        } else {
            for (size_t unit = 0; unit < view.length; ++unit)
                if (!fragment(p, "%04x", (unsigned int)view.units[unit]))
                    return false;
        }
        if (!fragment(p, "\n"))
            return false;
        ++p->rows;
    }
    return true;
}

int main(int argc, char **argv)
{
    int result = 1;
    probe p = {0};
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_registry_classes *owner = NULL;
    mc_registry_error error;
    mc_registry_side side;
    if (argc != 2 || (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0)) {
        fprintf(stderr, "usage: %s client|server\n", argv[0]);
        goto cleanup;
    }
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        (void)fail(&p, "stdout binary mode failed");
        goto cleanup;
    }
#endif
#if defined(SIGPIPE)
    /* Convert a closed consumer into a checked stdio failure, so owned state
     * can be retired by the ordinary cleanup path. */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        (void)fail(&p, "stdout signal setup failed");
        goto cleanup;
    }
#endif
    side = strcmp(argv[1], "client") == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER;
    if (mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK) {
        (void)fail(&p, "profile creation failed");
        goto cleanup;
    }
    if (!mc_materials_create(&materials)) {
        (void)fail(&p, "material creation failed");
        goto cleanup;
    }
    mc_registry_options options = {side, profile, materials, 0, NULL};
    if (mc_registry_classes_allocate(&options, &owner, &error) != MC_REGISTRY_OK) {
        (void)fail(&p, "class owner allocation failed");
        goto cleanup;
    }
    if (mc_registry_classes_initialize_blocks(owner, &error) !=
            MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY ||
        error.status != MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY || error.initializer_pc != 3398) {
        (void)fail(&p, "required construction-prefix stop was not reached");
        goto cleanup;
    }
    mc_registry_block_view view;
    if (mc_registry_classes_inspect_blocks(owner, &view, &error) != MC_REGISTRY_OK ||
        view.phase != MC_REGISTRY_FAILED || view.initializer_pc != 3398 ||
        view.named_blocks != 96) {
        (void)fail(&p, "required failed-prefix view was not retained");
        goto cleanup;
    }
    const mc_block_sound *sounds[] = {view.d, view.e, view.f, view.g, view.h,
                                      view.i, view.j, view.k, view.l};
    const void *arrays[] = {view.m, view.n, view.o, view.p, view.q, view.r, view.s, view.t};
    const mc_block *blocks[COUNT(block_names)];
    for (size_t i = 0; i < COUNT(sounds); ++i) {
        if (sounds[i] == NULL) {
            (void)fail(&p, "required sound root is missing");
            goto cleanup;
        }
    }
    for (size_t i = 0; i < COUNT(arrays); ++i) {
        if (arrays[i] == NULL) {
            (void)fail(&p, "required registry array is missing");
            goto cleanup;
        }
    }
    for (size_t i = 0; i < COUNT(block_names); ++i) {
        if (mc_registry_classes_inspect_block_static(owner, block_names[i], &blocks[i], &error) !=
                MC_REGISTRY_OK ||
            blocks[i] == NULL) {
            (void)fail(&p, "required named Block root is missing");
            goto cleanup;
        }
    }
    if (!row(&p, "REGISTRY_BASE_PREFIX_V1\t%s\n", argv[1]))
        goto cleanup;
    for (size_t i = 0; i < COUNT(sounds); ++i) {
        char field[2] = {(char)('d' + (int)i), '\0'};
        if (!reference_row(&p, "STATIC", field, NULL, sounds[i], NULL))
            goto cleanup;
    }
    for (size_t i = 0; i < COUNT(arrays); ++i) {
        char field[2] = {(char)('m' + (int)i), '\0'};
        if (!reference_row(&p, "STATIC", field, NULL, arrays[i], NULL))
            goto cleanup;
    }
    for (size_t i = 0; i < COUNT(block_names); ++i)
        if (!reference_row(&p, "STATIC", block_names[i], NULL, blocks[i], NULL))
            goto cleanup;
    for (size_t i = 0; i < (size_t)MC_MATERIAL_COUNT; ++i) {
        char field[2] = {(char)(i < 26 ? 'a' + (int)i : 'A' + (int)i - 26), '\0'};
        if (!reference_row(&p, "MATERIAL", field, NULL,
                           mc_materials_get(materials, (mc_material_id)i), NULL))
            goto cleanup;
    }
    for (size_t i = 0; i < COUNT(sounds); ++i) {
        char field[2] = {(char)('d' + (int)i), '\0'};
        if (!sound_rows(&p, field, sounds[i]))
            goto cleanup;
    }
    for (size_t i = 0; i < COUNT(block_names); ++i)
        if (!block_rows(&p, block_names[i], blocks[i]))
            goto cleanup;
    if (!element_rows(&p, &view) || !string_rows(&p))
        goto cleanup;

    /* All borrowed data have been emitted. Retire owners before announcing a
     * complete protocol; no pointer table entry is dereferenced afterward. */
    if (mc_registry_classes_free(owner, &error) != MC_REGISTRY_OK) {
        (void)fail(&p, "class owner retirement failed");
        goto cleanup;
    }
    owner = NULL;
    mc_materials_destroy(materials);
    materials = NULL;
    mc_java_random_profile_free(profile);
    profile = NULL;
    if (fflush(stdout) != 0 || ferror(stdout)) {
        (void)fail(&p, "stdout flush failed");
        goto cleanup;
    }
    size_t prior_rows = p.rows;
    if (!row(&p, "END\t113\t96\t2048\t%zu\t%zu\t%zu\n", p.string_count, p.reference_count,
             prior_rows))
        goto cleanup;
    if (fflush(stdout) != 0 || ferror(stdout)) {
        (void)fail(&p, "stdout final flush failed");
        goto cleanup;
    }
    result = 0;
cleanup:
    if (owner != NULL && mc_registry_classes_free(owner, &error) != MC_REGISTRY_OK) {
        (void)fail(&p, "class owner cleanup failed");
        /* Preserve the borrowed dependencies if retirement was refused. */
        return 1;
    }
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
