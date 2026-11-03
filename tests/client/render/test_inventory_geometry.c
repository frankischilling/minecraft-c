/* Genuine constructors and complete inventory calls. The included fixture's
 * resource adapters are reused; its prepared face/batch helpers never run. */
#define _CRT_SECURE_NO_WARNINGS
int mc_inventory_geometry_unused_faces_fixture_main(void);
#define main mc_inventory_geometry_unused_faces_fixture_main
#include "test_render_blocks_faces.c"
#undef main
#undef CHECK

#include "block/registry/registry_internal.h"
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/source_surface.h"

#include <errno.h>
#include <limits.h>

#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Inventory geometry check%u at%d: %s\n", checks, __LINE__, #value);    \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct inventory_fixture {
    fixture owners;
    resources input;
    mc_registry_resources resource_api;
    mc_client_display *display;
    mc_client_display_list_range *lists;
    int32_t list;
    bool compile, profile_open;
    unsigned int profiles, returned, failed, batches, words, inactive, faces;
    int32_t profile, id, metadata;
    float brightness;
    mc_block *block;
    uint32_t prior[48];
} inventory_fixture;

static uint32_t inventory_float_bits(float value) {
    uint32_t raw;
    memcpy(&raw, &value, sizeof(raw));
    return raw;
}

static bool inventory_integer(const char *text, int32_t *out) {
    errno = 0;
    char *end = NULL;
    const intmax_t value = strtoimax(text, &end, 10);
    CHECK(errno == 0 && end != text && *end == '\0' && value >= INT32_MIN && value <= INT32_MAX);
    *out = (int32_t)value;
    return true;
}

static bool inventory_hex(const char *text, size_t digits, uint64_t *out) {
    CHECK(strlen(text) == digits && (digits == 8 || digits == 16));
    for (size_t i = 0; i < digits; ++i)
        CHECK((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'));
    errno = 0;
    char *end = NULL;
    const uintmax_t value = strtoumax(text, &end, 16);
    CHECK(errno == 0 && end != text && *end == '\0');
    *out = (uint64_t)value;
    return true;
}

static bool inventory_float(const char *text, float *out) {
    uint64_t raw;
    CHECK(inventory_hex(text, 8, &raw) && raw <= UINT32_MAX);
    const uint32_t word = (uint32_t)raw;
    memcpy(out, &word, sizeof(word));
    return true;
}

static bool inventory_double(const char *text, double *out) {
    uint64_t raw;
    CHECK(inventory_hex(text, 16, &raw));
    memcpy(out, &raw, sizeof(raw));
    return true;
}

static void inventory_bool(bool value) {
    fputs(value ? "true" : "false", stdout);
}

static void inventory_words(const uint32_t words[48]) {
    putchar('[');
    for (size_t i = 0; i < 48; ++i)
        printf("%s\"%08" PRIx32 "\"", i == 0 ? "" : ",", words[i]);
    putchar(']');
}

static void inventory_cv(const mc_client_render_blocks *f) {
    static const char *const lower[] = {"n", "o", "p", "q", "r", "s", "t", "u", "v",
                                        "w", "x", "y", "z", "A", "B", "C", "D", "E",
                                        "F", "G", "H", "I", "J", "K", "L", "M", "N"};
    static const char *const corner_names[] = {"P", "Q", "R", "S", "T", "U",
                                               "V", "W", "X", "Y", "Z", "aa"};
    static const char *const flags[] = {"ab", "ac", "ad", "ae", "af", "ag",
                                        "ah", "ai", "aj", "ak", "al", "am"};
    printf("{\"d\":%" PRId32 ",\"g\":%" PRId32 ",\"h\":%" PRId32 ",\"i\":%" PRId32 ",\"j\":%" PRId32
           ",\"k\":%" PRId32 ",\"l\":%" PRId32 ",\"O\":%" PRId32,
           f->d, f->g, f->h, f->i, f->j, f->k, f->l, f->O);
    fputs(",\"b\":", stdout);
    inventory_bool(f->b);
    fputs(",\"e\":", stdout);
    inventory_bool(f->e);
    fputs(",\"f\":", stdout);
    inventory_bool(f->f);
    fputs(",\"m\":", stdout);
    inventory_bool(f->m);
    for (size_t i = 0; i < 27; ++i)
        printf(",\"%s\":\"%08" PRIx32 "\"", lower[i], inventory_float_bits(f->n_to_N[i]));
    for (size_t i = 0; i < 12; ++i)
        printf(",\"%s\":\"%08" PRIx32 "\"", corner_names[i], inventory_float_bits(f->P_to_aa[i]));
    for (size_t i = 0; i < 12; ++i) {
        printf(",\"%s\":", flags[i]);
        inventory_bool(f->ab_to_am[i]);
    }
    putchar('}');
}

static void inventory_bounds(const mc_block *block) {
    const mc_block_fields *b = &block->fields;
    printf("[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]",
           double_bits(b->bs), double_bits(b->bt), double_bits(b->bu), double_bits(b->bv),
           double_bits(b->bw), double_bits(b->bx));
}

static void inventory_source(const mc_block *block) {
    if (block->kind != MC_BLOCK_STAIRS) {
        fputs("null", stdout);
        return;
    }
    const mc_block *source = block->subtype.ss.a;
    if (source == NULL)
        fputs("{\"id\":null,\"class\":null,\"texture\":null}", stdout);
    else
        printf("{\"id\":%" PRId32 ",\"class\":\"%s\",\"texture\":%" PRId32 "}", source->fields.bn,
               source->constructor_class, source->fields.bm);
}

static void inventory_nw(const mc_client_nw_fields *n) {
    printf("{\"A\":%" PRId32 ",\"B\":%" PRId32 ",\"h\":%" PRId32 ",\"i\":\"%016" PRIx64
           "\",\"j\":\"%016" PRIx64 "\",\"k\":%" PRId32,
           n->A, n->B, n->h, double_bits(n->i), double_bits(n->j), n->k);
    fputs(",\"l\":", stdout);
    inventory_bool(n->l);
    fputs(",\"m\":", stdout);
    inventory_bool(n->m);
    fputs(",\"n\":", stdout);
    inventory_bool(n->n);
    printf(",\"o\":%" PRId32 ",\"p\":%" PRId32 ",\"q\":", n->o, n->p);
    inventory_bool(n->q);
    printf(",\"r\":%" PRId32 ",\"s\":\"%016" PRIx64 "\",\"t\":\"%016" PRIx64
           "\",\"u\":\"%016" PRIx64 "\",\"v\":%" PRId32 ",\"w\":",
           n->r, double_bits(n->s), double_bits(n->t), double_bits(n->u), n->v);
    inventory_bool(n->w);
    fputs(",\"x\":", stdout);
    inventory_bool(n->x);
    printf(",\"z\":%" PRId32 "}", n->z);
}

static bool inventory_gl_state(inventory_fixture *f) {
    float projection[16], modelview[16];
    mc_client_source_draw_state draw_state;
    CHECK(mc_client_display_camera_matrices_inspect(f->display, projection, modelview) &&
          mc_client_display_source_draw_inspect(f->display, &draw_state));
    fputs("{\"modelview\":[", stdout);
    for (size_t i = 0; i < 16; ++i)
        printf("%s\"%08" PRIx32 "\"", i == 0 ? "" : ",", inventory_float_bits(modelview[i]));
    fputs("],\"projection\":[", stdout);
    for (size_t i = 0; i < 16; ++i)
        printf("%s\"%08" PRIx32 "\"", i == 0 ? "" : ",", inventory_float_bits(projection[i]));
    fputs("],\"color\":[", stdout);
    for (size_t i = 0; i < 4; ++i)
        printf("%s\"%08" PRIx32 "\"", i == 0 ? "" : ",", inventory_float_bits(draw_state.color[i]));
    fputs("],\"color_known\":", stdout);
    inventory_bool(draw_state.color_known);
    putchar('}');
    return true;
}

static bool inventory_start(inventory_fixture *f) {
    fixture *o = &f->owners;
    o->context = calloc(1, sizeof(*o->context));
    CHECK(o->context != NULL && mc_materials_create(&o->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &o->profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, o->profile, NULL, &o->entities) ==
              MC_ENTITY_OK);
    static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                      "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
    f->input = (resources){.streams = {{language, sizeof(language) - 1, 0, false}, {0}, {0}}};
    f->resource_api =
        (mc_registry_resources){&f->input, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, o->profile, o->materials, 0,
                                         &f->resource_api};
    CHECK(mc_registry_classes_allocate(&options, &o->registry, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_bind_entities(o->registry, o->entities, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_blocks(o->registry, NULL) == MC_REGISTRY_OK &&
          mc_client_render_classes_create(o->entities, o->profile, &o->models) ==
              MC_CLIENT_MODEL_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(o->models, &model_error) &&
          mc_client_render_engine_classes_create(NULL, &o->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_graphics_error graphics_error;
    CHECK(mc_client_nw_active_use(o->graphics,
                                  (mc_client_graphics_site){"fixture", "inventory", "()V", 0},
                                  &o->nw, &graphics_error));
    CHECK(o->nw->fields.g != NULL && o->nw->fields.B >= 48 && !o->nw->fields.w);
    o->original_words = o->nw->fields.g;
    o->original_capacity = o->nw->fields.B;
    memcpy(f->prior, o->nw->fields.g, sizeof(f->prior));
    mc_client_display_error display_error;
    CHECK(
        mc_client_display_create("Inventory geometry", 32, 32, true, &f->display, &display_error));
    /* Asset-independent CPU geometry scope. No original atlas or pixel claim. */
    CHECK(mc_client_display_source_texture_enable(f->display, false, &display_error));
    if (f->compile)
        CHECK(mc_client_display_lists_generate(f->display, 1, 1024, &f->lists, &f->list,
                                               &display_error));
    return true;
}

static bool inventory_block(inventory_fixture *f, int32_t id, mc_block **out) {
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->owners.registry, id, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && block->phase == MC_BLOCK_CONSTRUCTED);
    *out = (mc_block *)(void *)block;
    return true;
}

static bool inventory_field(inventory_fixture *f, const char *name, const char *type,
                            const char *text) {
    mc_client_render_blocks *b = &f->owners.fields;
    if (strcmp(type, "b") == 0) {
        int32_t value;
        CHECK(inventory_integer(text, &value) && (value == 0 || value == 1));
        bool *slot = strcmp(name, "b") == 0   ? &b->b
                     : strcmp(name, "e") == 0 ? &b->e
                     : strcmp(name, "f") == 0 ? &b->f
                     : strcmp(name, "m") == 0 ? &b->m
                                              : NULL;
        CHECK(slot != NULL);
        *slot = value != 0;
    } else if (strcmp(type, "i") == 0) {
        int32_t *slot = strcmp(name, "d") == 0   ? &b->d
                        : strcmp(name, "g") == 0 ? &b->g
                        : strcmp(name, "h") == 0 ? &b->h
                        : strcmp(name, "i") == 0 ? &b->i
                        : strcmp(name, "j") == 0 ? &b->j
                        : strcmp(name, "k") == 0 ? &b->k
                        : strcmp(name, "l") == 0 ? &b->l
                                                 : NULL;
        CHECK(slot != NULL && inventory_integer(text, slot));
    } else {
        CHECK(strcmp(type, "f") == 0);
        const size_t corner = strcmp(name, "aa") == 0 ? 11u
                              : strlen(name) == 1 && name[0] >= 'P' && name[0] <= 'Z'
                                  ? (size_t)(name[0] - 'P')
                                  : 12u;
        CHECK(corner < 12 && inventory_float(text, &b->P_to_aa[corner]));
    }
    return true;
}

static bool inventory_batches(inventory_fixture *f) {
    mc_client_source_list_state state;
    CHECK(mc_client_display_list_inspect(f->display, f->list, &state) && state.defined &&
          !state.building);
    for (size_t batch = 0; batch < state.draws; ++batch) {
        mc_client_source_batch_view view;
        uint32_t words[48];
        CHECK(mc_client_display_list_batch(f->display, f->list, batch, &view) &&
              view.byte_count == sizeof(words));
        memcpy(words, view.bytes, sizeof(words));
        const bool colored = (view.arrays & (1u << MC_CLIENT_SOURCE_COLOR_ARRAY)) != 0;
        for (size_t i = 0; i < 48; ++i) {
            const bool active =
                i % 8 < 5 || (i % 8 == 6 && (i / 8 < 3 || i / 8 == 5)) || (i % 8 == 5 && colored);
            if (!active) {
                CHECK(words[i] == f->prior[i]);
                ++f->inactive;
            }
        }
        memcpy(f->prior, words, sizeof(words));
        printf("INVENTORY_ROW {\"profile\":%" PRId32 ",\"id\":%" PRId32 ",\"batch\":%zu,\"words\":",
               f->profile, f->id, batch);
        inventory_words(words);
        printf(",\"vertices\":%" PRId32 ",\"mode\":%" PRId32 ",\"arrays\":%u}\n", view.vertices,
               view.mode, view.arrays);
        ++f->batches;
        f->words += 48;
    }
    CHECK(memcmp(f->prior, f->owners.nw->fields.g, sizeof(f->prior)) == 0);
    return true;
}

static bool inventory_call(inventory_fixture *f) {
    fixture *o = &f->owners;
    uint32_t before[48], after[48];
    memcpy(before, o->nw->fields.g, sizeof(before));
    /* The original input observer queries GL_CURRENT_COLOR after the authored
     * Color call. Reach that actual query frontier before passive inspection;
     * Mesa may still have the Color operands staged. This is not a state fixup. */
    float queried_color[4];
    mc_client_display_error display_error;
    CHECK(mc_client_display_source_current_attribute(f->display, 2816, queried_color, 4,
                                                     &display_error));
    printf("INVENTORY_INPUT {\"profile\":%" PRId32 ",\"id\":%" PRId32 ",\"metadata\":%" PRId32
           ",\"brightness\":\"%08" PRIx32 "\",\"cv\":",
           f->profile, f->id, f->metadata, inventory_float_bits(f->brightness));
    inventory_cv(&o->fields);
    fputs(",\"bounds\":", stdout);
    inventory_bounds(f->block);
    printf(",\"texture\":%" PRId32 ",\"source_block\":", f->block->fields.bm);
    inventory_source(f->block);
    fputs(",\"nw_before\":", stdout);
    inventory_nw(&o->nw->fields);
    fputs(",\"prior_words\":", stdout);
    inventory_words(before);
    fputs(",\"gl_before\":", stdout);
    CHECK(inventory_gl_state(f));
    puts("}");
    if (f->compile)
        CHECK(mc_client_display_lists_begin(f->display, f->list, &display_error));
    *o->context = (mc_cv_context){.fields = &o->fields,
                                  .registry = o->registry,
                                  .models = o->models,
                                  .graphics = o->graphics,
                                  .display = f->display};
    const bool returned = mc_cv_inventory(o->context, f->block, f->metadata, f->brightness);
    CHECK(!o->nw->active && o->graphics->active_source_calls == 0 &&
          o->graphics->active_resource_calls == 0 && o->nw->fields.g == o->original_words &&
          o->nw->fields.B == o->original_capacity);
    if (f->compile) {
        CHECK(mc_client_display_lists_end(f->display, &display_error));
        CHECK(inventory_batches(f));
    }
    if (returned) {
        CHECK(o->context->returned && !o->context->failed && !o->nw->fields.w);
        ++f->returned;
    } else {
        const mc_cv_context *c = o->context;
        CHECK(c->failed && !c->returned && !c->failure.native_policy &&
              c->failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE && c->source_pc == 5 &&
              c->depth == 2 && c->frames[0].pc == 836 && c->frames[1].pc == 5 &&
              strcmp(c->frames[1].class_name, "ss") == 0 && c->inventory_faces == 0 &&
              f->block->kind == MC_BLOCK_STAIRS && f->block->subtype.ss.a == NULL &&
              o->nw->fields.w && o->nw->fields.n && o->nw->fields.h == 0 && o->nw->fields.o == 0 &&
              o->nw->fields.p == 0);
        CHECK(memcmp(before, o->nw->fields.g, sizeof(before)) == 0);
        ++f->failed;
    }
    CHECK(o->context->inventory_faces <= UINT_MAX - f->faces);
    f->faces += (unsigned int)o->context->inventory_faces;
    memcpy(after, o->nw->fields.g, sizeof(after));
    uint32_t gl_error;
    CHECK(mc_client_display_source_get_error(f->display, &gl_error, &display_error));
    printf("INVENTORY_RETURN {\"profile\":%" PRId32 ",\"returned\":", f->profile);
    inventory_bool(returned);
    fputs(",\"cv\":", stdout);
    inventory_cv(&o->fields);
    fputs(",\"bounds\":", stdout);
    inventory_bounds(f->block);
    fputs(",\"source_block\":", stdout);
    inventory_source(f->block);
    fputs(",\"nw_after\":", stdout);
    inventory_nw(&o->nw->fields);
    fputs(",\"last_words\":", stdout);
    inventory_words(after);
    fputs(",\"gl_after\":", stdout);
    CHECK(inventory_gl_state(f));
    printf(",\"gl_error\":%" PRIu32 ",\"failure\":", gl_error);
    if (returned)
        fputs("null", stdout);
    else
        printf("{\"status\":%d,\"source_pc\":%u,\"site\":{\"class\":\"%s\",\"method\":\"%s\","
               "\"descriptor\":\"%s\",\"pc\":%u},\"caller_pc\":%u,\"native_policy\":false}",
               (int)o->context->failure.status, o->context->source_pc,
               o->context->failure.source_class, o->context->failure.source_method,
               o->context->failure.source_descriptor, o->context->failure.source_pc,
               o->context->frames[0].pc);
    puts("}");
    CHECK(gl_error == 0 && fflush(stdout) == 0);
    ++f->profiles;
    f->profile_open = false;
    return true;
}

static bool inventory_commands(inventory_fixture *f, const char *path) {
    FILE *input = fopen(path, "rb");
    CHECK(input != NULL);
    char line[512];
    while (fgets(line, sizeof(line), input) != NULL) {
        CHECK(strchr(line, '\n') != NULL);
        char *tokens[16];
        size_t count = 0;
        for (char *token = strtok(line, " \t\r\n"); token != NULL;
             token = strtok(NULL, " \t\r\n")) {
            CHECK(count < sizeof(tokens) / sizeof(*tokens));
            tokens[count++] = token;
        }
        CHECK(count != 0);
        if (strcmp(tokens[0], "PROFILE") == 0) {
            CHECK(count == 5 && !f->profile_open && f->failed == 0 &&
                  inventory_integer(tokens[1], &f->profile) && f->profile >= 0 &&
                  (unsigned int)f->profile == f->profiles && inventory_integer(tokens[2], &f->id) &&
                  inventory_integer(tokens[3], &f->metadata) &&
                  inventory_float(tokens[4], &f->brightness) &&
                  inventory_block(f, f->id, &f->block));
            /* JVM allocation supplies fresh zero-initialized CV storage before
             * this profile's actual constructor stores. */
            f->owners.fields = (mc_client_render_blocks){0};
            mc_client_render_blocks_construct_fields(&f->owners.fields, NULL, false);
            f->profile_open = true;
        } else {
            CHECK(f->profile_open);
            if (strcmp(tokens[0], "FIELD") == 0) {
                CHECK(count == 4 && inventory_field(f, tokens[1], tokens[2], tokens[3]));
            } else if (strcmp(tokens[0], "BOUNDS") == 0) {
                float b[6];
                CHECK(count == 7);
                for (size_t i = 0; i < 6; ++i)
                    CHECK(inventory_float(tokens[i + 1], &b[i]));
                mc_registry_block_bounds(f->block, b[0], b[1], b[2], b[3], b[4], b[5]);
            } else if (strcmp(tokens[0], "TEXTURE") == 0 || strcmp(tokens[0], "STAIR") == 0) {
                int32_t id, value;
                mc_block *block;
                CHECK(count == 3 && inventory_integer(tokens[1], &id) &&
                      inventory_integer(tokens[2], &value) && inventory_block(f, id, &block));
                if (strcmp(tokens[0], "TEXTURE") == 0)
                    block->fields.bm = value;
                else {
                    CHECK(block->kind == MC_BLOCK_STAIRS);
                    mc_block *source = NULL;
                    if (value != -1)
                        CHECK(inventory_block(f, value, &source));
                    block->subtype.ss.a = source;
                }
            } else if (strcmp(tokens[0], "TRANSLATION") == 0) {
                double p[3];
                CHECK(count == 4);
                for (size_t i = 0; i < 3; ++i)
                    CHECK(inventory_double(tokens[i + 1], &p[i]));
                mc_client_graphics_error error;
                CHECK(mc_client_nw_translation(f->owners.nw, p[0], p[1], p[2], &error));
            } else if (strcmp(tokens[0], "GL") == 0) {
                float values[7];
                CHECK(count == 8);
                for (size_t i = 0; i < 7; ++i)
                    CHECK(inventory_float(tokens[i + 1], &values[i]));
                mc_client_display_error error;
                CHECK(mc_client_display_source_matrix_mode(f->display, 5889, &error) &&
                      mc_client_display_source_load_identity(f->display, &error) &&
                      mc_client_display_source_matrix_mode(f->display, 5888, &error) &&
                      mc_client_display_source_load_identity(f->display, &error) &&
                      mc_client_display_source_translate(f->display, values[0], values[1],
                                                         values[2], &error) &&
                      mc_client_display_source_color(f->display, values[3], values[4], values[5],
                                                     values[6], &error));
            } else {
                CHECK(count == 1 && strcmp(tokens[0], "CALL") == 0 && inventory_call(f));
            }
        }
    }
    CHECK(!ferror(input) && fclose(input) == 0 && !f->profile_open && f->profiles == 17 &&
          f->returned == 16 && f->failed == 1 && f->faces == 216);
    CHECK(f->compile ? f->batches == 216 && f->words == 10368 && f->inactive == 2772
                     : f->batches == 0 && f->words == 0 && f->inactive == 0);
    return true;
}

static bool inventory_release(inventory_fixture *f) {
    fixture *o = &f->owners;
    if (f->lists != NULL) {
        mc_client_display_error error;
        CHECK(mc_client_display_lists_retire(&f->lists, &error));
    }
    CHECK(!f->input.invalid && f->input.opens == 3 && f->input.closes == 1);
    free(o->context);
    o->context = NULL;
    CHECK(mc_client_render_engine_classes_release(o->graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_classes_free(o->models) == MC_CLIENT_MODEL_OK &&
          mc_registry_classes_free(o->registry, NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_free(o->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(o->profile);
    mc_materials_destroy(o->materials);
    mc_client_display_error error;
    CHECK(mc_client_display_destroy(f->display, &error));
    return true;
}

static bool inventory_stop(inventory_fixture *f) {
    fixture *o = &f->owners;
    CHECK(o->nw->fields.w && o->nw->fields.n && o->nw->fields.h == 0 && o->nw->fields.o == 0 &&
          o->nw->fields.p == 0 && f->block->subtype.ss.a == NULL);
    return inventory_release(f);
}

static bool inventory_admission_call(inventory_fixture *f, const char *kind,
                                     mc_client_render_global_status expected_status,
                                     size_t expected_depth) {
    fixture *o = &f->owners;
    mc_client_graphics_error graphics_error;
    CHECK(!o->nw->fields.w && mc_client_nw_translation(o->nw, 0, 0, 0, &graphics_error));
    o->fields = (mc_client_render_blocks){0};
    mc_client_render_blocks_construct_fields(&o->fields, NULL, false);
    o->fields.b = false;
    *o->context = (mc_cv_context){.fields = &o->fields,
                                  .registry = o->registry,
                                  .models = o->models,
                                  .graphics = o->graphics,
                                  .display = f->display};
    float before_projection[16], before_modelview[16], after_projection[16], after_modelview[16];
    CHECK(
        mc_client_display_camera_matrices_inspect(f->display, before_projection, before_modelview));
    uint32_t prior_words[48];
    memcpy(prior_words, o->nw->fields.g, sizeof(prior_words));
    const bool returned = mc_cv_inventory(o->context, f->block, 0, 1);
    CHECK(
        !o->nw->active && o->graphics->active_source_calls == 0 &&
        o->graphics->active_resource_calls == 0 && o->nw->fields.g == o->original_words &&
        o->nw->fields.B == o->original_capacity &&
        mc_client_display_camera_matrices_inspect(f->display, after_projection, after_modelview) &&
        memcmp(before_projection, after_projection, sizeof(before_projection)) == 0);
    const mc_cv_context *c = o->context;
    CHECK(c->failure.status == expected_status && c->depth == expected_depth);
    if (expected_status == MC_CLIENT_RENDER_GLOBAL_OK) {
        CHECK(returned && c->returned && !c->failed && c->inventory_faces == 12 &&
              !o->nw->fields.w && o->nw->fields.n && o->nw->fields.h == 0 && o->nw->fields.o == 0 &&
              o->nw->fields.p == 0 &&
              memcmp(before_modelview, after_modelview, sizeof(before_modelview)) == 0 &&
              f->block->fields.bs == 0 && f->block->fields.bt == 0 && f->block->fields.bu == .5 &&
              f->block->fields.bv == 1 && f->block->fields.bw == .5 && f->block->fields.bx == 1);
    } else {
        CHECK(!returned && !c->returned && c->failed && c->source_pc == 5 &&
              c->frames[0].pc == 836 && c->inventory_faces == 0 && o->nw->fields.w &&
              o->nw->fields.n && o->nw->fields.v == -32512 && o->nw->fields.h == 0 &&
              o->nw->fields.o == 0 && o->nw->fields.p == 0 && f->block->fields.bs == 0 &&
              f->block->fields.bt == 0 && f->block->fields.bu == 0 && f->block->fields.bv == 1 &&
              f->block->fields.bw == 1 && f->block->fields.bx == .5 &&
              memcmp(prior_words, o->nw->fields.g, sizeof(prior_words)) == 0 &&
              c->failure.native_policy == (expected_status == MC_CLIENT_RENDER_GLOBAL_LIMIT));
        for (size_t i = 1; i < c->depth; ++i)
            CHECK(strcmp(c->frames[i].class_name, "ss") == 0 &&
                  strcmp(c->frames[i].method, "a") == 0 &&
                  strcmp(c->frames[i].descriptor, "(I)I") == 0 && c->frames[i].pc == 5);
        for (size_t i = 0; i < 16; ++i) {
            const float expected =
                i >= 12 && i <= 14 ? before_modelview[i] - .5f : before_modelview[i];
            CHECK(inventory_float_bits(after_modelview[i]) == inventory_float_bits(expected));
        }
    }
    printf("INVENTORY_ADMISSION_STEP {\"kind\":\"%s\",\"returned\":", kind);
    inventory_bool(returned);
    printf(",\"status\":%d,\"depth\":%zu,\"faces\":%zu,\"source_pc\":%u,\"native_policy\":",
           (int)c->failure.status, c->depth, c->inventory_faces, c->source_pc);
    inventory_bool(c->failure.native_policy);
    fputs(",\"batch_open\":", stdout);
    inventory_bool(o->nw->fields.w);
    fputs(",\"bounds\":", stdout);
    inventory_bounds(f->block);
    fputs(",\"frames\":[", stdout);
    for (size_t i = 0; i < c->depth; ++i)
        printf("%s{\"class\":\"%s\",\"method\":\"%s\",\"descriptor\":\"%s\",\"pc\":%u}",
               i == 0 ? "" : ",", c->frames[i].class_name, c->frames[i].method,
               c->frames[i].descriptor, c->frames[i].pc);
    puts("]}");
    if (!returned) {
        /* Separate declared source teardown after all retained-prefix checks.
         * No native flag or buffer replacement repairs the open batch. */
        CHECK(mc_client_nw_draw(o->nw, f->display, &graphics_error) && !o->nw->fields.w &&
              o->nw->fields.n && o->nw->fields.h == 0 && o->nw->fields.o == 0 &&
              o->nw->fields.p == 0 &&
              memcmp(prior_words, o->nw->fields.g, sizeof(prior_words)) == 0 &&
              f->block->fields.bu == 0 && f->block->fields.bx == .5);
        float projection[16], modelview[16];
        CHECK(mc_client_display_camera_matrices_inspect(f->display, projection, modelview) &&
              memcmp(after_projection, projection, sizeof(projection)) == 0 &&
              memcmp(after_modelview, modelview, sizeof(modelview)) == 0);
    }
    return true;
}

static bool inventory_admission_program(void) {
    inventory_fixture f = {0};
    CHECK(inventory_start(&f));
    mc_block *other, *wood;
    CHECK(inventory_block(&f, 53, &f.block) && inventory_block(&f, 67, &other) &&
          inventory_block(&f, 5, &wood) && f.block->kind == MC_BLOCK_STAIRS &&
          other->kind == MC_BLOCK_STAIRS);
    mc_block *prior_source = f.block->subtype.ss.a, *prior_other = other->subtype.ss.a;
    mc_client_display_error error;
    CHECK(mc_client_display_source_matrix_mode(f.display, 5889, &error) &&
          mc_client_display_source_load_identity(f.display, &error) &&
          mc_client_display_source_matrix_mode(f.display, 5888, &error) &&
          mc_client_display_source_load_identity(f.display, &error) &&
          mc_client_display_source_translate(f.display, .25f, -.125f, .5f, &error) &&
          mc_client_display_source_color(f.display, .125f, .25f, .5f, .75f, &error));
    f.block->subtype.ss.a = other;
    other->subtype.ss.a = wood;
    CHECK(inventory_admission_call(&f, "nested_success", MC_CLIENT_RENDER_GLOBAL_OK, 0) &&
          f.block->subtype.ss.a == other && other->subtype.ss.a == wood);
    other->subtype.ss.a = NULL;
    CHECK(inventory_admission_call(&f, "nested_null", MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, 3) &&
          f.block->subtype.ss.a == other && other->subtype.ss.a == NULL);
    f.block->subtype.ss.a = f.block;
    CHECK(inventory_admission_call(&f, "cycle_native_limit", MC_CLIENT_RENDER_GLOBAL_LIMIT, 16) &&
          f.block->subtype.ss.a == f.block);
    f.block->subtype.ss.a = prior_source;
    other->subtype.ss.a = prior_other;
    CHECK(!f.owners.nw->fields.w && f.block->fields.bu == 0 && f.block->fields.bx == .5 &&
          inventory_release(&f));
    printf("INVENTORY_ADMISSION {\"checks\":%u,\"nested_success\":1,\"nested_null\":1,"
           "\"cycle_native_limit\":1,\"max_frames\":16,\"cleanup\":true}\n",
           checks);
    return fflush(stdout) == 0;
}

static bool inventory_program(const char *mode, const char *commands) {
    CHECK(strcmp(mode, "compile") == 0 || strcmp(mode, "immediate") == 0);
    inventory_fixture f = {.compile = strcmp(mode, "compile") == 0};
    CHECK((_mm_getcsr() & 0xffc0u) == 0x1f80u && inventory_start(&f) &&
          inventory_commands(&f, commands) && inventory_stop(&f));
    printf("INVENTORY_FINISH {\"mode\":\"%s\",\"checks\":%u,\"profiles\":%u,\"returned\":%u,"
           "\"failed\":%u,\"faces\":%u,\"batches\":%u,\"words\":%u,\"inactive_words\":%u,"
           "\"cleanup\":true}\n",
           mode, checks, f.profiles, f.returned, f.failed, f.faces, f.batches, f.words, f.inactive);
    return fflush(stdout) == 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "admission") == 0)
        return inventory_admission_program() ? 0 : 1;
    return argc == 3 && inventory_program(argv[1], argv[2]) ? 0 : 1;
}
