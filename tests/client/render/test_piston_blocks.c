#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Authored native Source-body controls, not an original runtime parity cohort.
 * The saved GAME World, Registry, Chunk and application owners are genuine.
 * Only the helpers are reused: the included picking main is never executed. */
#define MC_PICKING_BODY_MAIN_NAME mc_piston_blocks_picking_fixture_main
int mc_piston_blocks_picking_fixture_main(int argc, char **argv);
#include "../world/test_picking.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/render/blocks/internal.h"
#include "client/render/model_graph_internal.h"
#include "client/render_engine/nw.h"
#include "client/startup/application_owner.h"
#include "mc/client/settings/game_settings.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"
#include "world/storage/objects_internal.h"

typedef struct piston_controls {
    picking_fixture *fixture;
    mc_client_chunk_cache *cache;
    mc_client_application_classes *application;
    mc_game_settings *settings;
    mc_client_tessellator *nw;
    mc_cv_context *call, *reference;
    int32_t prefix[48];
    size_t bindings, rows;
} piston_controls;

/* Source orientation edits, in field order g,h,i,j,k,l. Entries left at zero
 * below mean "retain the incoming field", not "store zero". */
static const int32_t piston_rotations[6][6] = {{3, 3, 3, 3, 0, 0}, {0, 0, 0, 0, 0, 0},
                                               {0, 0, 1, 2, 0, 0}, {0, 0, 2, 1, 3, 3},
                                               {1, 2, 0, 0, 2, 1}, {2, 1, 0, 0, 1, 2}};
static const float piston_base_boxes[6][6] = {{0, .25f, 0, 1, 1, 1}, {0, 0, 0, 1, .75f, 1},
                                              {0, 0, .25f, 1, 1, 1}, {0, 0, 0, 1, 1, .75f},
                                              {.25f, 0, 0, 1, 1, 1}, {0, 0, 0, .75f, 1, 1}};
static const float piston_head_boxes[6][6] = {{0, 0, 0, 1, .25f, 1}, {0, .75f, 0, 1, 1, 1},
                                              {0, 0, 0, 1, 1, .25f}, {0, 0, .75f, 1, 1, 1},
                                              {0, 0, 0, .25f, 1, 1}, {.75f, 0, 0, 1, 1, 1}};
static const float piston_seed_box[6] = {.125f, .25f, .375f, .875f, .75f, .625f};
static const float piston_unit_box[6] = {0, 0, 0, 1, 1, 1};

static uint32_t piston_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float piston_float(int32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool piston_bounds(mc_block *block, const float box[6]) {
    CHECK(mc_block_render_bounds(block, box[0], box[1], box[2], box[3], box[4], box[5]) ==
          MC_REGISTRY_OK);
    return true;
}

static bool piston_bounds_are(const mc_block *block, const float box[6]) {
    const double values[6] = {block->fields.bs, block->fields.bt, block->fields.bu,
                              block->fields.bv, block->fields.bw, block->fields.bx};
    for (size_t i = 0; i < 6; ++i)
        CHECK(values[i] == (double)box[i]);
    return true;
}

static void piston_rotate(mc_client_render_blocks *fields, int32_t direction) {
    int32_t *values[6] = {&fields->g, &fields->h, &fields->i, &fields->j, &fields->k, &fields->l};
    for (size_t i = 0; i < 6; ++i)
        if (piston_rotations[direction][i] != 0)
            *values[i] = piston_rotations[direction][i];
}

static bool piston_rotations_are(const mc_client_render_blocks *fields, int32_t value) {
    CHECK(fields->g == value && fields->h == value && fields->i == value && fields->j == value &&
          fields->k == value && fields->l == value);
    return true;
}

static bool piston_prepare(piston_controls *p, int32_t id, int32_t metadata) {
    mc_chunk_data *data = p->fixture->chunks[0]->data;
    memset(data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(data->skylight, 0xff, MC_CHUNK_NIBBLE_BYTES);
    memset(data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    data->blocks[(8u << 11u) | (8u << 7u) | 64u] = (uint8_t)id;
    CHECK(mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, (uint8_t)metadata));
    return true;
}

static void piston_context(piston_controls *p, mc_cv_context *c, mc_client_render_blocks *fields,
                           const void *receiver) {
    memset(c, 0, sizeof(*c));
    mc_client_render_blocks_construct_fields(fields, receiver, true);
    fields->f = true;
    fields->m = true;
    fields->e = true;
    fields->g = fields->h = fields->i = fields->j = fields->k = fields->l = 2;
    c->fields = fields;
    c->cache = p->cache;
    c->world = p->fixture->base.world;
    c->registry = p->fixture->base.registry;
    c->models = p->fixture->models;
    c->graphics = p->fixture->graphics;
    c->application = p->fixture->app;
    c->code_classes = p->fixture->base.world_classes;
    c->tessellator = p->nw;
}

static bool piston_state(piston_controls *p, mc_client_nw_state *out) {
    CHECK(mc_client_nw_inspect(p->fixture->graphics, out) && out->a == p->nw && out->fields.w &&
          out->fields.r == 7 && out->fields.s == .125 && out->fields.t == -.5 &&
          out->fields.u == .25 && out->fields.g != NULL && out->fields.o >= 48 &&
          memcmp(out->fields.g, p->prefix, sizeof(p->prefix)) == 0 &&
          p->fixture->base.world->persistent_bindings == p->bindings + 1);
    return true;
}

/* Existing face/standard bodies already have an original vertex oracle. Here
 * they independently consume the authored bounds/rotation table, testing the
 * new Piston body glue down to actual position, UV and packed color words.
 * NW's unused normal/padding words are deliberately outside this comparison. */
static bool piston_same_vertices(const int32_t *words, int32_t first, int32_t second,
                                 int32_t vertices) {
    for (int32_t vertex = 0; vertex < vertices; ++vertex)
        CHECK(memcmp(words + first + vertex * 8, words + second + vertex * 8, 6 * sizeof(*words)) ==
              0);
    return true;
}

static bool piston_reference_plate(piston_controls *p, mc_block *block,
                                   const mc_client_render_blocks *entered, const float box[6],
                                   int32_t direction, int32_t actual_first) {
    mc_client_render_blocks fields = *entered;
    piston_context(p, p->reference, &fields, entered->c);
    fields = *entered;
    piston_rotate(&fields, direction);
    CHECK(piston_bounds(block, box) && mc_cv_push(p->reference, "l", "(Luu;III)Z"));
    mc_client_nw_state before, after;
    CHECK(piston_state(p, &before));
    bool rendered = false;
    CHECK(mc_cv_standard(p->reference, block, 8, 64, 8, &rendered) && rendered &&
          !p->reference->failed && piston_state(p, &after) &&
          after.fields.o == before.fields.o + 36 * 8 &&
          piston_same_vertices(after.fields.g, actual_first, before.fields.o, 36));
    return true;
}

static bool piston_rod(piston_controls *p, int32_t first, int32_t direction, bool full_rod,
                       int32_t override) {
    mc_client_nw_state state;
    CHECK(piston_state(p, &state));
    const int32_t axis = direction < 2 ? 1 : direction < 4 ? 2 : 0;
    const double translation[3] = {.125, -.5, .25};
    const float origin[3] = {8, 64, 8};
    const float length = full_rod ? 1.f : .5f;
    const float low = (direction & 1) ? .75f - length : .25f;
    const float high = (direction & 1) ? .75f : .25f + length;
    const int32_t texture = override >= 0 ? override : 108;
    const int32_t atlas_x = (texture & 15) * 16, atlas_y = texture & 240;
    const uint32_t u[2] = {
        piston_float_bits((float)atlas_x / 256.f),
        piston_float_bits((float)(((double)atlas_x + (full_rod ? 16. : 8.) - .01) / 256.))};
    const uint32_t v[2] = {
        piston_float_bits((float)atlas_y / 256.f),
        piston_float_bits((float)(((double)((float)atlas_y + 4.f) - .01) / 256.))};
    static const unsigned int uv[6][2] = {{1, 0}, {0, 0}, {0, 1}, {1, 0}, {0, 1}, {1, 1}};
    bool seen_low = false, seen_high = false;
    for (int32_t vertex = 0; vertex < 24; ++vertex) {
        const int32_t *words = state.fields.g + first + vertex * 8;
        for (int32_t coordinate = 0; coordinate < 3; ++coordinate) {
            const float a =
                (float)((double)(origin[coordinate] + (coordinate == axis ? low : .375f)) +
                        translation[coordinate]);
            const float b =
                (float)((double)(origin[coordinate] + (coordinate == axis ? high : .625f)) +
                        translation[coordinate]);
            const float actual = piston_float(words[coordinate]);
            CHECK(actual == a || actual == b);
            if (coordinate == axis) {
                seen_low |= actual == a;
                seen_high |= actual == b;
            }
        }
        CHECK((uint32_t)words[3] == u[uv[vertex % 6][0]] &&
              (uint32_t)words[4] == v[uv[vertex % 6][1]]);
    }
    CHECK(seen_low && seen_high);
    return true;
}

static bool piston_body(piston_controls *p, mc_block *block, int32_t direction, bool head,
                        bool argument, bool metadata_extended, int32_t override) {
    const bool extended = argument || metadata_extended;
    CHECK(piston_prepare(p, block->fields.bn, direction | (metadata_extended ? 8 : 0)) &&
          piston_bounds(block, piston_seed_box));
    mc_client_render_blocks fields;
    piston_context(p, p->call, &fields, p->fixture->base.world);
    fields.d = override;
    fields.f = (direction & 1) == 0;
    const mc_client_render_blocks entered = fields;
    mc_client_nw_state before, after;
    CHECK(piston_state(p, &before));
    bool rendered = false;
    CHECK((head ? mc_cv_piston_head(p->call, block, 8, 64, 8, argument, &rendered)
                : mc_cv_piston_base(p->call, block, 8, 64, 8, argument, &rendered)) &&
          rendered && !p->call->failed && p->call->depth == 0 &&
          p->call->source_pc == (head ? 1660u : 498u) && fields.f == entered.f && !fields.m &&
          piston_rotations_are(&fields, 0) && piston_state(p, &after) &&
          after.fields.o == before.fields.o + (head ? 60 : 36) * 8 &&
          after.fields.p == before.fields.p + (head ? 40 : 24));
    mc_client_render_blocks retained = entered;
    retained.m = false;
    retained.g = retained.h = retained.i = retained.j = retained.k = retained.l = 0;
    CHECK(memcmp(&fields, &retained, sizeof(fields)) == 0 &&
          piston_bounds_are(block, head || extended ? piston_unit_box : piston_seed_box));
    const float *box = head       ? piston_head_boxes[direction]
                       : extended ? piston_base_boxes[direction]
                                  : piston_seed_box;
    if (head)
        CHECK(piston_rod(p, before.fields.o + 36 * 8, direction, argument, override));
    CHECK(piston_reference_plate(p, block, &entered, box, direction, before.fields.o));
    ++p->rows;
    return true;
}

static bool piston_wrappers(piston_controls *p, mc_block *base, mc_block *head) {
    for (int kind = 0; kind < 4; ++kind) {
        mc_block *block = kind == 0 || kind == 3 ? base : head;
        const bool is_head = block == head, full_rod = kind == 2;
        CHECK(piston_prepare(p, block->fields.bn, 4) && piston_bounds(block, piston_unit_box));
        mc_client_render_blocks fields;
        piston_context(p, p->call, &fields, p->fixture->base.world);
        fields.f = true; /* Successful source wrappers overwrite even incoming true. */
        const mc_client_render_blocks entered = fields;
        mc_client_nw_state before, after;
        CHECK(piston_state(p, &before));
        const bool complete = kind == 0 ? mc_cv_piston_base_all(p->call, block, 8, 64, 8)
                              : kind == 3
                                  ? mc_cv_render_all(p->call, block, 8, 64, 8)
                                  : mc_cv_piston_head_all(p->call, block, 8, 64, 8, full_rod);
        CHECK(complete && !p->call->failed && p->call->returned && p->call->depth == 0 &&
              !fields.f && piston_rotations_are(&fields, 0) && piston_state(p, &after) &&
              after.fields.o == before.fields.o + (is_head ? 60 : 36) * 8 &&
              piston_bounds_are(block, piston_unit_box));
        const float *box = is_head     ? piston_head_boxes[4]
                           : kind == 0 ? piston_base_boxes[4]
                                       : piston_unit_box;
        CHECK(piston_reference_plate(p, block, &entered, box, 4, before.fields.o));
        if (is_head)
            CHECK(piston_rod(p, before.fields.o + 36 * 8, 4, full_rod, -1));
        ++p->rows;
    }
    /* Generic all-faces also reaches the actual type17 child with flagtrue. */
    CHECK(piston_prepare(p, head->fields.bn, 1) && piston_bounds(head, piston_unit_box));
    mc_client_render_blocks fields;
    piston_context(p, p->call, &fields, p->fixture->base.world);
    mc_client_nw_state before, after;
    CHECK(piston_state(p, &before) && mc_cv_render_all(p->call, head, 8, 64, 8) && !fields.f &&
          !p->call->failed && piston_state(p, &after) &&
          after.fields.o == before.fields.o + 60 * 8 &&
          piston_rod(p, before.fields.o + 36 * 8, 1, true, -1));
    ++p->rows;
    return true;
}

static bool piston_failures(piston_controls *p, mc_block *base, mc_block *head) {
    mc_world_chunk *chunk =
        mc_world_store_chunk_object(p->fixture->base.store, p->fixture->chunks[0]);
    CHECK(chunk != NULL && chunk->e != NULL);
    for (int kind = 0; kind < 6; ++kind) {
        const bool is_head = (kind & 1) != 0, metadata_failure = kind >= 2 && kind < 4;
        const bool wrapper = kind >= 4;
        mc_block *block = is_head ? head : base;
        CHECK(piston_prepare(p, block->fields.bn, 0) && piston_bounds(block, piston_seed_box));
        mc_client_render_blocks fields;
        piston_context(p, p->call, &fields,
                       metadata_failure ? (const void *)p->fixture->base.world : NULL);
        fields.f = false;
        const mc_client_render_blocks entered = fields;
        const mc_block_fields entered_block = block->fields;
        mc_world_chunk_nibble *saved_metadata = chunk->e;
        if (metadata_failure)
            chunk->e = NULL;
        mc_client_nw_state before, after;
        CHECK(piston_state(p, &before));
        int32_t *prior = malloc((size_t)before.fields.o * sizeof(*prior));
        CHECK(prior != NULL);
        memcpy(prior, before.fields.g, (size_t)before.fields.o * sizeof(*prior));
        bool output = true;
        const bool complete =
            wrapper ? (is_head ? mc_cv_piston_head_all(p->call, block, 8, 64, 8, true)
                               : mc_cv_piston_base_all(p->call, block, 8, 64, 8))
                    : (is_head ? mc_cv_piston_head(p->call, block, 8, 64, 8, true, &output)
                               : mc_cv_piston_base(p->call, block, 8, 64, 8, true, &output));
        chunk->e = saved_metadata;
        mc_client_render_blocks expected = entered;
        expected.f = wrapper;
        CHECK(!complete && output && p->call->failed && !p->call->returned &&
              p->call->source_pc == 8 && p->call->depth == (wrapper ? 2u : 1u) &&
              p->call->frames[p->call->depth - 1].pc == 8 &&
              strcmp(p->call->frames[p->call->depth - 1].descriptor, "(Luu;IIIZ)Z") == 0 &&
              p->call->last_store_pc == (wrapper ? 2u : 0u) &&
              memcmp(&fields, &expected, sizeof(fields)) == 0 &&
              memcmp(&block->fields, &entered_block, sizeof(entered_block)) == 0 &&
              piston_state(p, &after) && after.fields.o == before.fields.o &&
              memcmp(&after.fields, &before.fields, sizeof(before.fields)) == 0 &&
              memcmp(after.fields.g, prior, (size_t)before.fields.o * sizeof(*prior)) == 0);
        if (metadata_failure)
            CHECK(p->call->world_error.kind == MC_CV_WORLD_METADATA &&
                  p->call->world_error.failed &&
                  p->call->world_error.value.query.cause.status != MC_WORLD_STORAGE_OK);
        else
            CHECK(p->call->failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
                  !p->call->failure.native_policy);
        /* Retained failure graph: a new wrapper call cannot execute source
         * stores, replace the original cause, or append to the caller batch. */
        memcpy(p->reference, p->call, sizeof(*p->call));
        CHECK(!mc_cv_render_all(p->call, block, 8, 64, 8) &&
              memcmp(p->reference, p->call, sizeof(*p->call)) == 0 &&
              memcmp(&fields, &expected, sizeof(fields)) == 0 && piston_state(p, &after) &&
              after.fields.o == before.fields.o &&
              memcmp(after.fields.g, prior, (size_t)before.fields.o * sizeof(*prior)) == 0);
        free(prior);
        ++p->rows;
    }
    /* A reached standard-render child failure retains preceding orientation
     * and bound stores. No finally-style reset is credited on that path. */
    for (int kind = 0; kind < 2; ++kind) {
        const bool is_head = kind != 0;
        mc_block *block = is_head ? head : base;
        CHECK(piston_prepare(p, block->fields.bn, 0) && piston_bounds(block, piston_seed_box));
        mc_client_render_blocks fields;
        piston_context(p, p->call, &fields, p->fixture->base.world);
        const mc_client_render_blocks entered = fields;
        mc_client_nw_state before, after;
        CHECK(piston_state(p, &before));
        p->fixture->app->settings = NULL;
        bool output = true;
        const bool complete = is_head ? mc_cv_piston_head(p->call, block, 8, 64, 8, true, &output)
                                      : mc_cv_piston_base(p->call, block, 8, 64, 8, true, &output);
        p->fixture->app->settings = p->settings;
        mc_client_render_blocks expected = entered;
        piston_rotate(&expected, 0);
        CHECK(!complete && output && p->call->failed && p->call->depth == 2 &&
              p->call->frames[0].pc == (is_head ? 141u : 265u) && p->call->frames[1].pc == 128 &&
              p->call->failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
              memcmp(&fields, &expected, sizeof(fields)) == 0 &&
              piston_bounds_are(block, is_head ? piston_head_boxes[0] : piston_base_boxes[0]) &&
              piston_state(p, &after) && after.fields.o == before.fields.o);
        ++p->rows;
    }
    return true;
}

/* Authored READY UK fields are installed by the genuine semantic NBT read,
 * then the real World setter publishes that same identity. These are native
 * method controls; no original renderer observation is claimed here. */
static bool piston_moving_install(piston_controls *p, int32_t direction, bool extending,
                                  mc_tile_entity **out) {
    CHECK(piston_prepare(p, 36, direction));
    mc_tile_entity *tile = NULL;
    mc_tile_entity_error error;
    CHECK(mc_tile_entity_allocate(p->fixture->base.world, MC_TILE_ENTITY_PISTON, &tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_construct(tile, 34, 0, direction, extending, true, &error) ==
              MC_TILE_ENTITY_OK);
    static const char *const keys[5] = {"blockId", "blockData", "facing", "progress", "extending"};
    uint16_t names_utf16[5][10] = {{0}};
    mc_nbt_tag tags[5] = {{0}};
    const mc_nbt_tag *items[5];
    for (size_t i = 0; i < 5; ++i) {
        const size_t length = strlen(keys[i]);
        for (size_t j = 0; j < length; ++j)
            names_utf16[i][j] = (uint16_t)(unsigned char)keys[i][j];
        tags[i].name = (mc_nbt_string){names_utf16[i], length};
        tags[i].type = i == 3 ? MC_NBT_FLOAT : i == 4 ? MC_NBT_BYTE : MC_NBT_INT;
        items[i] = &tags[i];
    }
    tags[0].value.int_value = 34;
    tags[2].value.int_value = direction;
    tags[3].value.float_bits = UINT32_C(0x3e800000); /* Both real progress fields = .25. */
    tags[4].value.byte_value = extending ? 1 : 0;
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND, .value.compound = {items, 5}};
    mc_live_tile_entity_error live;
    CHECK(mc_tile_entity_read_nbt(tile, &root, &error) == MC_TILE_ENTITY_OK &&
          mc_live_tile_entities_set(p->fixture->base.world, 8, 64, 8, tile, &live) ==
              MC_WORLD_STORAGE_OK);
    *out = tile;
    return true;
}

static bool piston_bounds_controls(piston_controls *p, mc_block *base, mc_block *head) {
    const int32_t override_before = head->subtype.h.a;
    for (int32_t direction = 0; direction < 6; ++direction) {
        for (int32_t side = 0; side < 6; ++side) {
            int32_t texture = -999;
            CHECK(mc_block_render_piston_head_texture_reset(head) == MC_REGISTRY_OK &&
                  mc_block_render_texture_metadata(head, side, direction, &texture) ==
                      MC_REGISTRY_OK &&
                  texture == (side == direction         ? head->fields.bm
                              : side == (direction ^ 1) ? 107
                                                        : 108));
            CHECK(mc_block_render_texture_metadata(head, side, direction | 8, &texture) ==
                      MC_REGISTRY_OK &&
                  texture == (side == direction         ? head->fields.bm - 1
                              : side == (direction ^ 1) ? 107
                                                        : 108));
            CHECK(mc_block_render_piston_head_texture_set(head, 17) == MC_REGISTRY_OK &&
                  mc_block_render_texture_metadata(head, side, direction | 8, &texture) ==
                      MC_REGISTRY_OK &&
                  texture == (side == direction         ? 17
                              : side == (direction ^ 1) ? 107
                                                        : 108));
        }
    }
    CHECK(mc_block_render_piston_head_texture_reset(head) == MC_REGISTRY_OK);
    ++p->rows;
    /* Invalid direction reaches H's actual WJ.a element; it is not masked
     * into an ordinary facing or hidden behind the native texture wrapper. */
    for (int fault = 0; fault < 2; ++fault) {
        CHECK(piston_prepare(p, 34, fault ? 0 : 7));
        mc_registry_classes *registry = p->fixture->base.registry;
        int32_t *const opposite = registry->direction_a;
        if (fault)
            registry->direction_a = NULL;
        mc_client_render_blocks fields;
        piston_context(p, p->call, &fields, p->fixture->base.world);
        int32_t texture = 12345;
        CHECK(!mc_cv_texture(p->call, head, 8, 64, 8, fault ? 1 : 0, 12, &texture) &&
              texture == 12345 && p->call->texture_error.site.pc == 46 &&
              !p->call->failure.native_policy && p->call->exception_class != NULL &&
              strcmp(p->call->exception_class,
                     fault ? "java/lang/NullPointerException"
                           : "java/lang/ArrayIndexOutOfBoundsException") == 0 &&
              mc_cv_retire(p->call, NULL));
        registry->direction_a = opposite;
        ++p->rows;
    }
    CHECK(mc_block_render_piston_head_texture_set(head, override_before) == MC_REGISTRY_OK);
    for (int32_t direction = 0; direction < 8; ++direction) {
        CHECK(piston_prepare(p, 33, direction | 8));
        mc_client_render_blocks fields;
        piston_context(p, p->call, &fields, p->fixture->base.world);
        CHECK(piston_bounds(base, piston_seed_box) &&
              mc_cv_update_bounds(p->call, base, 8, 64, 8, 15) &&
              piston_bounds_are(base,
                                direction < 6 ? piston_base_boxes[direction] : piston_seed_box));
        CHECK(piston_bounds(head, piston_seed_box) &&
              mc_cv_update_bounds(p->call, head, 8, 64, 8, 15) &&
              piston_bounds_are(head,
                                direction < 6 ? piston_head_boxes[direction] : piston_seed_box) &&
              mc_cv_retire(p->call, NULL));
        ++p->rows;
    }
    const mc_block *moving_const = NULL;
    CHECK(mc_registry_block_get(p->fixture->base.registry, 36, &moving_const, NULL) ==
              MC_REGISTRY_OK &&
          moving_const != NULL && moving_const->kind == MC_BLOCK_MOVING_PISTON);
    mc_block *moving = (mc_block *)(void *)moving_const;
    const mc_block_fields saved = moving->fields;
    static const int32_t axes[6][3] = {{0, -1, 0}, {0, 1, 0},  {0, 0, -1},
                                       {0, 0, 1},  {-1, 0, 0}, {1, 0, 0}};
    for (int32_t direction = 0; direction < 6; ++direction) {
        for (int extend = 0; extend < 2; ++extend) {
            mc_tile_entity *tile = NULL;
            CHECK(piston_moving_install(p, direction, extend != 0, &tile));
            const size_t held = tile->holds;
            mc_client_render_blocks fields;
            piston_context(p, p->call, &fields,
                           extend ? (const void *)p->cache : (const void *)p->fixture->base.world);
            CHECK(piston_bounds(moving, piston_seed_box) &&
                  mc_cv_update_bounds(p->call, moving, 8, 64, 8, 15) &&
                  p->call->piston_tile_count == 0 && tile->holds == held &&
                  p->call->tile_classes != NULL);
            const double actual[6] = {moving->fields.bs, moving->fields.bt, moving->fields.bu,
                                      moving->fields.bv, moving->fields.bw, moving->fields.bx};
            for (size_t i = 0; i < 6; ++i)
                CHECK(actual[i] ==
                      (double)piston_head_boxes[direction][i] -
                          (double)((float)axes[direction][i % 3] * (extend ? .75f : .25f)));
            CHECK(mc_cv_retire(p->call, NULL) && p->call->tile_classes == NULL &&
                  mc_cv_retire(p->call, NULL));
            mc_live_tile_entity_error live;
            CHECK(!p->fixture->base.world->fields.L &&
                  mc_live_tile_entities_remove(p->fixture->base.world, 8, 64, 8, &live) ==
                      MC_WORLD_STORAGE_OK);
            mc_tile_entity_release(tile);
            ++p->rows;
        }
    }
    /* Real WJ.c = NULL fails only after the genuine first bs store. The
     * retained actual Tile and Class occurrences end through native retire,
     * which must not reset source Block bounds or the original error. */
    mc_tile_entity *tile = NULL;
    CHECK(piston_moving_install(p, 4, false, &tile) && piston_bounds(moving, piston_seed_box));
    const size_t held = tile->holds;
    mc_registry_classes *registry = p->fixture->base.registry;
    int32_t *const axis_y = registry->direction_c;
    registry->direction_c = NULL;
    mc_client_render_blocks fields;
    piston_context(p, p->call, &fields, p->fixture->base.world);
    CHECK(!mc_cv_update_bounds(p->call, moving, 8, 64, 8, 15) && p->call->failed &&
          p->call->source_pc == 110 && p->call->last_store_pc == 96 &&
          p->call->failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          !p->call->failure.native_policy && p->call->piston_tile_count == 1 &&
          tile->holds == held + 1 && moving->fields.bs == .25 && moving->fields.bt == .25 &&
          moving->fields.bu == .375 && moving->fields.bv == .875 && moving->fields.bw == .75 &&
          moving->fields.bx == .625);
    const mc_block_fields prefix = moving->fields;
    const mc_client_render_global_error failure = p->call->failure;
    CHECK(mc_cv_retire(p->call, NULL) && tile->holds == held && p->call->piston_tile_count == 0 &&
          p->call->tile_classes == NULL &&
          memcmp(&failure, &p->call->failure, sizeof(failure)) == 0 &&
          memcmp(&prefix, &moving->fields, sizeof(prefix)) == 0 && mc_cv_retire(p->call, NULL));
    registry->direction_c = axis_y; /* Correct only the authored native fault after retirement. */
    mc_live_tile_entity_error live;
    CHECK(mc_live_tile_entities_remove(p->fixture->base.world, 8, 64, 8, &live) ==
          MC_WORLD_STORAGE_OK);
    mc_tile_entity_release(tile);
    moving->fields = saved;
    ++p->rows;
    return true;
}

static bool piston_run(picking_fixture *f) {
    piston_controls p = {.fixture = f, .bindings = f->base.world->persistent_bindings};
    p.call = calloc(1, sizeof(*p.call));
    p.reference = calloc(1, sizeof(*p.reference));
    CHECK(p.call != NULL && p.reference != NULL);
    mc_client_chunk_cache_error cache_error;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &p.cache,
                                       &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    /* Same declared prepared App input used by the original-tested lever/AO
     * fixtures. No App constructor completion or startup behavior is claimed. */
    CHECK(mc_client_application_classes_create(NULL, &p.application, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK &&
          mc_client_application_classes_active_use(p.application, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK &&
          mc_client_application_lifetime_register(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK &&
          mc_client_application_namespace_attach(p.application, f->app, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK);
    f->app->constructor_phase = MC_CLIENT_APP_CONSTRUCTING;
    CHECK(mc_client_application_classes_publish(p.application, f->app, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK &&
          mc_game_settings_create_defaults(NULL, &p.settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(p.settings, &settings));
    settings.values.ambient_occlusion = false;
    CHECK(mc_game_settings_set_values(p.settings, &settings.values, NULL) == MC_GAME_SETTINGS_OK);
    f->app->settings = p.settings;
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(f->models, &model_error));
    const bool overlay = f->models->cv_a, anaglyph = f->models->px_a;
    f->models->cv_a = f->models->px_a = false;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f->graphics,
                                  (mc_client_graphics_site){"native-test", "piston", "()V", 0},
                                  &p.nw, &graphics) &&
          mc_client_nw_begin_quads(p.nw, &graphics) &&
          mc_client_nw_translation(p.nw, .125, -.5, .25, &graphics) &&
          mc_client_nw_color(p.nw, 31, 127, 233, 255, &graphics));
    for (int vertex = 0; vertex < 4; ++vertex)
        CHECK(mc_client_nw_vertex_uv(p.nw, vertex & 1, vertex >> 1, -3, .125, .75, &graphics));
    mc_client_nw_state initial;
    CHECK(mc_client_nw_inspect(f->graphics, &initial) && initial.fields.o == 48);
    memcpy(p.prefix, initial.fields.g, sizeof(p.prefix));
    f->base.world->fields.f = 3;
    const mc_block *blocks[3] = {NULL, NULL, NULL};
    CHECK(mc_registry_block_get(f->base.registry, 29, &blocks[0], NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->base.registry, 33, &blocks[1], NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->base.registry, 34, &blocks[2], NULL) == MC_REGISTRY_OK &&
          blocks[0] != NULL && blocks[1] != NULL && blocks[2] != NULL &&
          blocks[0]->kind == MC_BLOCK_PISTON_BASE && blocks[1]->kind == MC_BLOCK_PISTON_BASE &&
          blocks[2]->kind == MC_BLOCK_PISTON_HEAD);
    const mc_block_fields saved[3] = {blocks[0]->fields, blocks[1]->fields, blocks[2]->fields};
    for (size_t base = 0; base < 2; ++base)
        for (int32_t direction = 0; direction < 6; ++direction)
            for (int scene = 0; scene < 3; ++scene)
                CHECK(piston_body(&p, (mc_block *)(void *)blocks[base], direction, false,
                                  scene == 1, scene == 2, -1));
    for (int32_t direction = 0; direction < 6; ++direction)
        for (int full = 0; full < 2; ++full)
            for (int override = 0; override < 2; ++override)
                CHECK(piston_body(&p, (mc_block *)(void *)blocks[2], direction, true, full != 0,
                                  (direction & 1) != 0, override ? 17 : -1));
    CHECK(
        piston_wrappers(&p, (mc_block *)(void *)blocks[1], (mc_block *)(void *)blocks[2]) &&
        piston_failures(&p, (mc_block *)(void *)blocks[1], (mc_block *)(void *)blocks[2]) &&
        piston_bounds_controls(&p, (mc_block *)(void *)blocks[1], (mc_block *)(void *)blocks[2]) &&
        p.rows == 97);
    for (size_t i = 0; i < 3; ++i)
        ((mc_block *)(void *)blocks[i])->fields = saved[i];
    f->models->cv_a = overlay;
    f->models->px_a = anaglyph;
    CHECK(mc_client_nw_translation(p.nw, 0, 0, 0, &graphics));
    f->app->constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(mc_client_application_namespace_detach(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK &&
          mc_client_application_classes_release(&p.application, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK &&
          mc_client_application_lifetime_retire(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    f->app->settings = NULL;
    CHECK(mc_game_settings_retire(&p.settings, NULL) == MC_GAME_SETTINGS_OK &&
          mc_client_chunk_cache_retire(&p.cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK &&
          f->base.world->persistent_bindings == p.bindings);
    free(p.call);
    free(p.reference);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !piston_run(f) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf("PASS: %u Source Piston RenderBlocks checks\n", checks);
    return 0;
}
