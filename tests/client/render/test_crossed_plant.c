/* Reuse genuine Registry/Material/NW construction helpers. Its main and broad
 * face controls are not executed by this focused plant test. */
int mc_crossed_faces_fixture_main(void);
#define main mc_crossed_faces_fixture_main
#include "test_render_blocks_faces.c"
#undef main

typedef struct plant_profile {
    int32_t id, metadata, texture;
} plant_profile;

static const plant_profile plants[] = {
    {6, 0, 15},  {6, 1, 63},  {6, 2, 79},  {6, 3, 15},  {31, 0, 55}, {31, 1, 39}, {31, 2, 56},
    {32, 0, 55}, {37, 0, 13}, {38, 0, 12}, {39, 0, 29}, {40, 0, 28}, {83, 0, 73}};

/* Authored binary32 atlas expectations. They preserve the float15.99 addition
 * before divide256; no production geometry or arithmetic helper builds them. */
static const struct atlas {
    int32_t texture;
    uint32_t uv[4];
} atlas[] = {{15, {0x3f700000, 0x3f7ffd71, 0x00000000, 0x3d7fd70a}},
             {63, {0x3f700000, 0x3f7ffd71, 0x3e400000, 0x3e7ff5c2}},
             {79, {0x3f700000, 0x3f7ffd71, 0x3e800000, 0x3e9ffae1}},
             {55, {0x3ee00000, 0x3efffae1, 0x3e400000, 0x3e7ff5c2}},
             {39, {0x3ee00000, 0x3efffae1, 0x3e000000, 0x3e3ff5c2}},
             {56, {0x3f000000, 0x3f0ffd71, 0x3e400000, 0x3e7ff5c2}},
             {13, {0x3f500000, 0x3f5ffd71, 0x00000000, 0x3d7fd70a}},
             {12, {0x3f400000, 0x3f4ffd71, 0x00000000, 0x3d7fd70a}},
             {29, {0x3f500000, 0x3f5ffd71, 0x3d800000, 0x3dffeb85}},
             {28, {0x3f400000, 0x3f4ffd71, 0x3d800000, 0x3dffeb85}},
             {73, {0x3f100000, 0x3f1ffd71, 0x3e800000, 0x3e9ffae1}},
             {0, {0x00000000, 0x3d7fd70a, 0x00000000, 0x3d7fd70a}},
             {302, {0x3f600000, 0x3f6ffd71, 0x3e000000, 0x3e3ff5c2}}};

static const double positions[3][3] = {{0, 0, 0}, {-3, 50, 39}, {-16777215.75, 65, 256.75}};
/* x-low/high, y-top/bottom, z-low/high after the existing NW translation.
 * The last row distinguishes double-space addition before float narrowing. */
static const uint32_t coordinates[3][6] = {
    {0x41806666, 0x4187999a, 0xc23c0000, 0xc2400000, 0xc1ff999a, 0xc1f86666},
    {0x4150cccd, 0x415f3333, 0x40400000, 0x40000000, 0x40e1999a, 0x40fe6666},
    {0xcb7ffff0, 0xcb7fffef, 0x41900000, 0x41880000, 0x4360cccd, 0x4361b333}};

static unsigned int plant_index;

static void plant_sentinel(fixture *f) {
    const int32_t extent = f->nw->fields.B < 192 ? f->nw->fields.B : 192;
    for (int32_t i = 0; f->nw->fields.g != NULL && i < extent; ++i)
        ((int32_t *)(void *)f->nw->fields.g)[i] = INT32_C(0x5a6b7c8d);
}

static bool plant_emit(fixture *f, const mc_block *block, const plant_profile *profile,
                       int32_t override, size_t point, int kind, const char *exception) {
    const mc_client_render_blocks *b = &f->fields;
    const mc_client_nw_fields *n = &f->nw->fields;
    CHECK(printf("PLANT %u %" PRId32 " %" PRId32 " %" PRId32 " %zu %d %d", plant_index++,
                 profile->id, profile->metadata, override, point, kind, b->m ? 1 : 0) >= 0);
    for (size_t i = 0; i < 27; ++i) {
        uint32_t bits;
        memcpy(&bits, &b->n_to_N[i], sizeof(bits));
        CHECK(printf(" %08" PRIx32, bits) >= 0);
    }
    unsigned int flags = 0;
    for (unsigned int i = 0; i < 12; ++i)
        if (b->ab_to_am[i])
            flags |= 1u << i;
    CHECK(printf(" %u", flags) >= 0);
    for (size_t i = 0; i < 12; ++i) {
        uint32_t bits;
        memcpy(&bits, &b->P_to_aa[i], sizeof(bits));
        CHECK(printf(" %08" PRIx32, bits) >= 0);
    }
    const mc_block_fields *block_fields = mc_block_fields_get(block);
    CHECK(block_fields != NULL);
    const double box[6] = {block_fields->bs, block_fields->bt, block_fields->bu,
                           block_fields->bv, block_fields->bw, block_fields->bx};
    for (size_t i = 0; i < 6; ++i)
        CHECK(printf(" %016" PRIx64, double_bits(box[i])) >= 0);
    CHECK(printf(" %" PRId32 " %" PRId32 " %" PRId32 " %08" PRIx32 " %016" PRIx64 " %016" PRIx64
                 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %d %d %d %d %d %" PRId32 " %" PRId32
                 " %s",
                 n->h, n->p, n->o, (uint32_t)n->k, double_bits(n->i), double_bits(n->j),
                 double_bits(n->s), double_bits(n->t), double_bits(n->u), n->l ? 1 : 0,
                 n->m ? 1 : 0, n->n ? 1 : 0, n->q ? 1 : 0, n->w ? 1 : 0, n->r, n->B,
                 exception) >= 0);
    const int32_t extent = n->g == NULL ? -1 : n->B < 192 ? n->B : 192;
    CHECK(printf(" %" PRId32, extent) >= 0);
    for (int32_t i = 0; i < extent; ++i)
        CHECK(printf(" %08" PRIx32, (uint32_t)n->g[i]) >= 0);
    CHECK(putchar('\n') != EOF);
    return true;
}

static bool plant_words(fixture *f, size_t position, int32_t texture) {
    const uint32_t *uv = NULL;
    for (size_t i = 0; i < sizeof(atlas) / sizeof(*atlas); ++i)
        if (atlas[i].texture == texture)
            uv = atlas[i].uv;
    CHECK(uv != NULL);
    /* Each row describes one plane's four source vertices in source order.
     * NW's actual quad conversion emits 0,1,2,0,2,3. */
    static const unsigned int plane[4][4][3] = {{{0, 2, 4}, {0, 3, 4}, {1, 3, 5}, {1, 2, 5}},
                                                {{1, 2, 5}, {1, 3, 5}, {0, 3, 4}, {0, 2, 4}},
                                                {{0, 2, 5}, {0, 3, 5}, {1, 3, 4}, {1, 2, 4}},
                                                {{1, 2, 4}, {1, 3, 4}, {0, 3, 5}, {0, 2, 5}}};
    static const unsigned int triangle[6] = {0, 1, 2, 0, 2, 3};
    static const unsigned int texture_corner[4][2] = {{0, 2}, {0, 3}, {1, 3}, {1, 2}};
    const mc_client_nw_fields *n = &f->nw->fields;
    CHECK(n->p == 16 && n->h == 24 && n->o == 192 && n->m && n->l && !n->n && n->w &&
          (uint32_t)n->k == UINT32_C(0xffe97f1f));
    for (size_t p = 0; p < 4; ++p)
        for (size_t t = 0; t < 6; ++t) {
            const size_t vertex = triangle[t], offset = (p * 6 + t) * 8;
            for (size_t axis = 0; axis < 3; ++axis)
                CHECK((uint32_t)n->g[offset + axis] ==
                      coordinates[position][plane[p][vertex][axis]]);
            CHECK((uint32_t)n->g[offset + 3] == uv[texture_corner[vertex][0]] &&
                  (uint32_t)n->g[offset + 4] == uv[texture_corner[vertex][1]] &&
                  (uint32_t)n->g[offset + 5] == UINT32_C(0xffe97f1f));
        }
    return true;
}

static bool plant_geometry(fixture *f) {
    static const int32_t overrides[] = {-1, 0, 302};
    for (size_t p = 0; p < sizeof(plants) / sizeof(*plants); ++p) {
        const mc_block *block = NULL;
        CHECK(mc_registry_block_get(f->registry, plants[p].id, &block, NULL) == MC_REGISTRY_OK &&
              block != NULL);
        for (size_t override = 0; override < 3; ++override)
            for (size_t position = 0; position < 3; ++position) {
                CHECK(prepare(f, 0, 0, 0, false, false, 0, -1));
                plant_sentinel(f);
                f->fields.d = overrides[override];
                const mc_client_render_blocks before = f->fields;
                CHECK(mc_cv_crossed_plant_geometry(f->context, block, plants[p].metadata,
                                                   positions[position][0], positions[position][1],
                                                   positions[position][2]));
                CHECK(!f->context->failed && f->context->depth == 0 &&
                      f->context->source_pc == 403 &&
                      memcmp(&before, &f->fields, sizeof(before)) == 0 && !f->nw->active &&
                      f->graphics->active_source_calls == 0);
                CHECK(plant_words(f, position,
                                  override == 0 ? plants[p].texture : overrides[override]));
                CHECK(plant_emit(f, block, &plants[p], overrides[override], position, -1, "NONE"));
            }
    }
    return true;
}

static bool plant_failures(fixture *f) {
    static const unsigned int vertex_pc[] = {145, 145, 162, 179, 196, 213};
    for (int kind = 0; kind < 8; ++kind) {
        CHECK(prepare(f, 0, 0, 0, false, false, 0, kind));
        plant_sentinel(f);
        const mc_client_render_blocks before = f->fields;
        CHECK(!mc_cv_crossed_plant_geometry(f->context, kind == 0 ? NULL : f->stone, 0, 0, 0, 0));
        CHECK(f->context->failed && f->context->depth == 1 &&
              strcmp(f->context->failure.source_class, "cv") == 0 &&
              strcmp(f->context->failure.source_method, "a") == 0 &&
              strcmp(f->context->failure.source_descriptor, "(Luu;IDDD)V") == 0 &&
              memcmp(&before, &f->fields, sizeof(before)) == 0 && !f->nw->active &&
              f->graphics->active_source_calls == 0);
        CHECK(f->context->source_pc == (kind == 0 ? 8u : kind == 1 ? 145u : vertex_pc[kind - 2]));
        CHECK(!f->context->failure.native_policy);
        const plant_profile failed = {1, 0, 1};
        CHECK(plant_emit(f, f->stone, &failed, -1, 0, kind, kind < 2 ? "NPE" : "AIOOBE"));
        f->nw->fields.g = f->original_words;
        f->nw->fields.B = f->original_capacity;
    }
    CHECK(prepare(f, 0, 0, 0, false, false, 0, -1));
    const unsigned int controls = _mm_getcsr();
    _mm_setcsr(controls | 0x8040u);
    const bool bad_fp = mc_cv_crossed_plant_geometry(f->context, f->stone, 0, 0, 0, 0);
    _mm_setcsr(controls);
    CHECK(!bad_fp && f->context->failed && f->context->source_pc == 48 &&
          f->context->failure.status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP &&
          f->context->failure.native_policy && f->nw->fields.p == 0 && f->nw->fields.o == 0);
    CHECK(prepare(f, 0, 0, 0, false, false, 0, -1) && mc_cv_push(f->context, "i", "(Luu;III)Z"));
    bool rendered = false;
    CHECK(!mc_cv_crossed_plant(f->context, NULL, 0, 0, 0, &rendered) && !rendered &&
          f->context->failed && f->context->source_pc == 14 && f->context->depth == 1 &&
          f->context->failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          !f->context->failure.native_policy && f->nw->fields.p == 0);
    return true;
}

static bool run_plants(void) {
    fixture f = {0};
    f.context = calloc(1, sizeof(*f.context));
    CHECK(f.context != NULL && mc_materials_create(&f.materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f.profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f.profile, NULL, &f.entities) == MC_ENTITY_OK);
    static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                      "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
    resources r = {.streams = {{language, sizeof(language) - 1, 0, false}, {0}, {0}}};
    const mc_registry_resources api = {&r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f.profile, f.materials, 0, &api};
    CHECK(mc_registry_classes_allocate(&options, &f.registry, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_bind_entities(f.registry, f.entities, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_blocks(f.registry, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f.registry, 1, &f.stone, NULL) == MC_REGISTRY_OK &&
          f.stone != NULL);
    mc_client_model_error model;
    mc_client_graphics_error graphics;
    CHECK(mc_client_render_classes_create(f.entities, f.profile, &f.models) == MC_CLIENT_MODEL_OK &&
          mc_client_render_blocks_active_use(f.models, &model) &&
          mc_client_render_engine_classes_create(NULL, &f.graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(
              f.graphics, (mc_client_graphics_site){"native-test", "crossed-plant", "()V", 0},
              &f.nw, &graphics));
    f.original_words = f.nw->fields.g;
    f.original_capacity = f.nw->fields.B;
    CHECK(plant_geometry(&f) && plant_failures(&f) && plant_index == 125 && !r.invalid &&
          r.opens == 3 && r.closes == 1);
    CHECK(mc_client_render_engine_classes_release(f.graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_classes_free(f.models) == MC_CLIENT_MODEL_OK &&
          mc_registry_classes_free(f.registry, NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_free(f.entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f.profile);
    mc_materials_destroy(f.materials);
    free(f.context);
    return true;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u || !run_plants())
        return 1;
    printf("Crossed plants: %u checks; 117 exact NW atlas/position rows and 10 failure prefixes\n",
           checks);
    return 0;
}
