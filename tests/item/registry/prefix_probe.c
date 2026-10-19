#include "mc/item/registry/registry.h"

#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#define REFERENCE_CAPACITY 512u
#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

static const char *const names[] = {
    "d",  "e",  "f",  "g",  "h",  "i",  "j",  "k",  "l",  "m",  "n",  "o",  "p",  "q",  "r",  "s",
    "t",  "u",  "v",  "w",  "x",  "y",  "z",  "A",  "B",  "C",  "D",  "E",  "F",  "G",  "H",  "I",
    "J",  "K",  "L",  "M",  "N",  "O",  "P",  "Q",  "R",  "S",  "T",  "U",  "V",  "W",  "X",  "Y",
    "Z",  "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj", "ak", "al", "am", "an", "ao",
    "ap", "aq", "ar", "as", "at", "au", "av", "aw", "ax", "ay", "az", "aA", "aB", "aC", "aD", "aE",
    "aF", "aG", "aH", "aI", "aJ", "aK", "aL", "aM", "aN", "aO", "aP", "aQ", "aR", "aS", "aT", "aU",
    "aV", "aW", "aX", "aY", "aZ", "ba", "bb", "bc", "bd", "be"};
_Static_assert(COUNT(names) == 106, "all actual named Item fields required");

typedef struct probe {
    const void *references[REFERENCE_CAPACITY];
    const mc_registry_string *strings[REFERENCE_CAPACITY];
    size_t reference_count, rows;
    bool failed;
} probe;

static bool fail(probe *p, const char *message)
{
    if (!p->failed)
        fprintf(stderr, "Item prefix probe: %s\n", message);
    p->failed = true;
    return false;
}

static bool write_text(probe *p, bool complete_row, const char *format, ...)
{
    if (p->failed)
        return false;
    va_list arguments;
    va_start(arguments, format);
    int written = vprintf(format, arguments);
    va_end(arguments);
    if (written < 0)
        return fail(p, "stdout write failed");
    if (complete_row)
        ++p->rows;
    return true;
}

static size_t find(const probe *p, const void *pointer)
{
    for (size_t i = 0; i < p->reference_count; ++i)
        if (p->references[i] == pointer)
            return i + 1;
    return 0;
}

static size_t enroll(probe *p, const void *pointer)
{
    size_t known = find(p, pointer);
    if (known != 0)
        return known;
    if (pointer == NULL || p->reference_count == REFERENCE_CAPACITY) {
        (void)fail(p, "nonnull bounded reference enrollment required");
        return 0;
    }
    p->references[p->reference_count++] = pointer;
    return p->reference_count;
}

static bool item_token(probe *p, const mc_item *item, char token[32])
{
    if (item == NULL) {
        memcpy(token, "NULL", sizeof("NULL"));
        return true;
    }
    size_t rank = find(p, item);
    if (rank == 0 || rank > 106)
        return fail(p, "Item reference outside actual named owners");
    int length = snprintf(token, 32, "R:%zu", rank);
    return length > 0 && length < 32 ? true : fail(p, "reference formatting failed");
}

static bool integer(probe *p, size_t rank, const char *field, int32_t value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return write_text(p, true, "FIELD\t%zu\t%s\tI:%08" PRIx32 "\n", rank, field, bits);
}

static bool fields(probe *p, size_t rank, const mc_item *item)
{
    const mc_item_fields *f = mc_item_fields_get(item);
    if (f == NULL)
        return fail(p, "actual Item fields missing");
    if (!integer(p, rank, "bf", f->bf) || !integer(p, rank, "bg", f->bg) ||
        !integer(p, rank, "a", f->a) || !integer(p, rank, "bh", f->bh) ||
        !write_text(p, true, "FIELD\t%zu\tbi\tZ:%u\n", rank, f->bi ? 1u : 0u) ||
        !write_text(p, true, "FIELD\t%zu\tbj\tZ:%u\n", rank, f->bj ? 1u : 0u))
        return false;
    char token[32];
    if (!item_token(p, f->bk, token) || !write_text(p, true, "FIELD\t%zu\tbk\t%s\n", rank, token))
        return false;
    if (f->bl == NULL)
        return write_text(p, true, "FIELD\t%zu\tbl\tNULL\n", rank);
    size_t string_rank = enroll(p, f->bl);
    if (string_rank == 0)
        return false;
    if (string_rank <= 107)
        return fail(p, "String aliases an Item or its array");
    p->strings[string_rank - 1] = f->bl;
    return write_text(p, true, "FIELD\t%zu\tbl\tR:%zu\n", rank, string_rank);
}

static bool strings(probe *p)
{
    for (size_t i = 107; i < p->reference_count; ++i) {
        const mc_registry_string *string = p->strings[i];
        mc_registry_string_view view;
        if (string == NULL || !mc_registry_string_read(string, &view) ||
            (view.length != 0 && view.units == NULL) || view.length > 65536)
            return fail(p, "invalid actual String view");
        if (!write_text(p, false, "STRING\t%zu\t%zu\t", i + 1, view.length))
            return false;
        if (view.length == 0) {
            if (!write_text(p, false, "-"))
                return false;
        } else {
            for (size_t unit = 0; unit < view.length; ++unit)
                if (!write_text(p, false, "%04x", (unsigned int)view.units[unit]))
                    return false;
        }
        if (!write_text(p, true, "\n"))
            return false;
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
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        (void)fail(&p, "stdout signal setup failed");
        goto cleanup;
    }
#endif
    bool client = strcmp(argv[1], "client") == 0;
    if (mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK ||
        !mc_materials_create(&materials)) {
        (void)fail(&p, "actual profile/material creation failed");
        goto cleanup;
    }
    mc_registry_options options = {client ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER, profile,
                                   materials, 0, NULL};
    if (mc_registry_classes_allocate(&options, &owner, &error) != MC_REGISTRY_OK ||
        mc_registry_classes_initialize_blocks(owner, &error) !=
            MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY ||
        error.status != MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY || error.initializer_pc != 3398 ||
        error.active_initializer_pc != 4 || error.active_initializer_class == NULL ||
        strcmp(error.active_initializer_class, client ? "nh" : "ii") != 0 ||
        error.constructor_pc != 23 || error.constructor_class == NULL ||
        strcmp(error.constructor_class, client ? "nh" : "ii") != 0) {
        (void)fail(&p, "required nested native failure edge was not reached");
        goto cleanup;
    }
    mc_registry_item_view view;
    mc_registry_block_view block_view;
    if (mc_registry_classes_inspect_items(owner, &view, &error) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_blocks(owner, &block_view, &error) != MC_REGISTRY_OK ||
        view.phase != MC_REGISTRY_FAILED || view.initializer_pc != 2664 ||
        view.named_items != 106 || mc_registry_item_array_length(view.c) != 32000 ||
        block_view.phase != MC_REGISTRY_FAILED || block_view.initializer_pc != 3398 ||
        block_view.named_blocks != 96) {
        (void)fail(&p, "required retained Item/Block views missing");
        goto cleanup;
    }
    const mc_item *items[COUNT(names)];
    for (size_t i = 0; i < COUNT(names); ++i) {
        if (mc_registry_classes_inspect_item_static(owner, names[i], &items[i], &error) !=
                MC_REGISTRY_OK ||
            items[i] == NULL || enroll(&p, items[i]) != i + 1) {
            (void)fail(&p, "106 distinct actual named Items required");
            goto cleanup;
        }
    }
    if (enroll(&p, view.c) != 107) {
        (void)fail(&p, "actual Item array must have separate identity");
        goto cleanup;
    }
    if (!write_text(&p, true, "ITEM_BASE_PREFIX_V1\t%s\n", argv[1]) ||
        !write_text(&p, true, "STATIC\tc\tR:107\n"))
        goto cleanup;
    for (size_t i = 0; i < COUNT(names); ++i)
        if (!write_text(&p, true, "STATIC\t%s\tR:%zu\n", names[i], i + 1))
            goto cleanup;
    for (size_t i = 0; i < COUNT(names); ++i)
        if (!fields(&p, i + 1, items[i]))
            goto cleanup;
    if (!write_text(&p, true, "ARRAY\t107\t32000\n"))
        goto cleanup;
    for (size_t index = 0; index < 32000; ++index) {
        const mc_item *item = NULL;
        char token[32];
        if (mc_registry_item_array_get(view.c, index, &item, &error) != MC_REGISTRY_OK ||
            !item_token(&p, item, token) ||
            !write_text(&p, true, "ELEMENT\t%zu\t%s\n", index, token)) {
            (void)fail(&p, "complete actual Item array read failed");
            goto cleanup;
        }
    }
    if (!strings(&p))
        goto cleanup;
    if (mc_registry_classes_free(owner, &error) != MC_REGISTRY_OK) {
        (void)fail(&p, "class retirement failed");
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
    if (!write_text(&p, true, "END\t%zu\n", prior_rows) || fflush(stdout) != 0 || ferror(stdout)) {
        (void)fail(&p, "stdout terminal write failed");
        goto cleanup;
    }
    result = 0;
cleanup:
    if (owner != NULL && mc_registry_classes_free(owner, &error) != MC_REGISTRY_OK) {
        (void)fail(&p, "class cleanup refused; dependencies retained");
        return 1;
    }
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
