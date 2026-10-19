#include "mc/block/registry/registry.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"

#include <stdio.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        bool check_passed = (condition);                                                           \
        if (!check_passed) {                                                                       \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

typedef struct block_case {
    const char *field, *name;
    int32_t texture;
    char material, sound;
    float hardness, resistance;
} block_case;

/* Independent literals from paired uu/na.<clinit> through putstatic 3395,
 * resolved through the actual constructor/material chains. This is expected
 * test data, never an initialized production registry or a C-generated dump.
 * The full Item/bootstrap tail has not executed at this checkpoint. */
static const block_case blocks[] = {
    {"u", "stone", 1, 'e', 'h', 1.5f, 30.0f},
    {"v", "grass", 3, 'b', 'g', 0.6f, 3.0f},
    {"w", "dirt", 2, 'c', 'f', 0.5f, 2.5f},
    {"x", "stonebrick", 16, 'e', 'h', 2.0f, 30.0f},
    {"y", "wood", 4, 'd', 'e', 2.0f, 15.0f},
    {"z", "sapling", 15, 'j', 'g', 0.0f, 0.0f},
    {"A", "bedrock", 17, 'e', 'h', -1.0f, 18000000.0f},
    {"B", "water", 205, 'g', 'd', 100.0f, 500.0f},
    {"C", "water", 205, 'g', 'd', 100.0f, 500.0f},
    {"D", "lava", 237, 'h', 'd', 0.0f, 0.0f},
    {"E", "lava", 237, 'h', 'd', 100.0f, 500.0f},
    {"F", "sand", 18, 'n', 'l', 0.5f, 2.5f},
    {"G", "gravel", 19, 'n', 'f', 0.6f, 3.0f},
    {"H", "oreGold", 32, 'e', 'h', 3.0f, 15.0f},
    {"I", "oreIron", 33, 'e', 'h', 3.0f, 15.0f},
    {"J", "oreCoal", 34, 'e', 'h', 3.0f, 15.0f},
    {"K", "log", 20, 'd', 'e', 2.0f, 10.0f},
    {"L", "leaves", 52, 'i', 'g', 0.2f, 1.0f},
    {"M", "sponge", 48, 'k', 'g', 0.6f, 3.0f},
    {"N", "glass", 49, 'p', 'j', 0.3f, 1.5f},
    {"O", "oreLapis", 160, 'e', 'h', 3.0f, 15.0f},
    {"P", "blockLapis", 144, 'e', 'h', 3.0f, 15.0f},
    {"Q", "dispenser", 45, 'e', 'h', 3.5f, 17.5f},
    {"R", "sandStone", 192, 'e', 'h', 0.8f, 4.0f},
    {"S", "musicBlock", 74, 'd', 'd', 0.8f, 4.0f},
    {"T", "bed", 134, 'l', 'd', 0.2f, 1.0f},
    {"U", "goldenRail", 179, 'o', 'i', 0.7f, 3.5f},
    {"V", "detectorRail", 195, 'o', 'i', 0.7f, 3.5f},
    {"W", "pistonStickyBase", 106, 'B', 'h', 0.5f, 2.5f},
    {"X", "web", 11, 'A', 'd', 4.0f, 20.0f},
    {"Y", "tallgrass", 39, 'j', 'g', 0.0f, 0.0f},
    {"Z", "deadbush", 55, 'j', 'g', 0.0f, 0.0f},
    {"aa", "pistonBase", 107, 'B', 'h', 0.5f, 2.5f},
    {"ab", NULL, 107, 'B', 'h', 0.5f, 2.5f},
    {"ac", "cloth", 64, 'l', 'k', 0.8f, 4.0f},
    {"ad", NULL, 0, 'B', 'd', -1.0f, 0.0f},
    {"ae", "flower", 13, 'j', 'g', 0.0f, 0.0f},
    {"af", "rose", 12, 'j', 'g', 0.0f, 0.0f},
    {"ag", "mushroom", 29, 'j', 'g', 0.0f, 0.0f},
    {"ah", "mushroom", 28, 'j', 'g', 0.0f, 0.0f},
    {"ai", "blockGold", 23, 'f', 'i', 3.0f, 30.0f},
    {"aj", "blockIron", 22, 'f', 'i', 5.0f, 30.0f},
    {"ak", "stoneSlab", 6, 'e', 'h', 2.0f, 30.0f},
    {"al", "stoneSlab", 6, 'e', 'h', 2.0f, 30.0f},
    {"am", "brick", 7, 'e', 'h', 2.0f, 30.0f},
    {"an", "tnt", 8, 'q', 'g', 0.0f, 0.0f},
    {"ao", "bookshelf", 35, 'd', 'e', 1.5f, 7.5f},
    {"ap", "stoneMoss", 36, 'e', 'h', 2.0f, 30.0f},
    {"aq", "obsidian", 37, 'e', 'h', 10.0f, 6000.0f},
    {"ar", "torch", 80, 'o', 'e', 0.0f, 0.0f},
    {"as", "fire", 31, 'm', 'e', 0.0f, 0.0f},
    {"at", "mobSpawner", 65, 'e', 'i', 5.0f, 25.0f},
    {"au", "stairsWood", 4, 'd', 'e', 2.0f, 15.0f},
    {"av", "chest", 26, 'd', 'e', 2.5f, 12.5f},
    {"aw", "redstoneDust", 164, 'o', 'd', 0.0f, 0.0f},
    {"ax", "oreDiamond", 50, 'e', 'h', 3.0f, 15.0f},
    {"ay", "blockDiamond", 24, 'f', 'i', 5.0f, 30.0f},
    {"az", "workbench", 59, 'd', 'e', 2.5f, 12.5f},
    {"aA", "crops", 88, 'j', 'g', 0.0f, 0.0f},
    {"aB", "farmland", 87, 'c', 'f', 0.6f, 3.0f},
    {"aC", "furnace", 45, 'e', 'h', 3.5f, 17.5f},
    {"aD", "furnace", 45, 'e', 'h', 3.5f, 17.5f},
    {"aE", "sign", 4, 'd', 'e', 1.0f, 5.0f},
    {"aF", "doorWood", 97, 'd', 'e', 3.0f, 15.0f},
    {"aG", "ladder", 83, 'o', 'e', 0.4f, 2.0f},
    {"aH", "rail", 128, 'o', 'i', 0.7f, 3.5f},
    {"aI", "stairsStone", 16, 'e', 'h', 2.0f, 30.0f},
    {"aJ", "sign", 4, 'd', 'e', 1.0f, 5.0f},
    {"aK", "lever", 96, 'o', 'e', 0.5f, 2.5f},
    {"aL", "pressurePlate", 1, 'e', 'h', 0.5f, 2.5f},
    {"aM", "doorIron", 98, 'f', 'i', 5.0f, 25.0f},
    {"aN", "pressurePlate", 4, 'd', 'e', 0.5f, 2.5f},
    {"aO", "oreRedstone", 51, 'e', 'h', 3.0f, 15.0f},
    {"aP", "oreRedstone", 51, 'e', 'h', 3.0f, 15.0f},
    {"aQ", "notGate", 115, 'o', 'e', 0.0f, 0.0f},
    {"aR", "notGate", 99, 'o', 'e', 0.0f, 0.0f},
    {"aS", "button", 1, 'o', 'h', 0.5f, 2.5f},
    {"aT", "snow", 66, 't', 'k', 0.1f, 0.5f},
    {"aU", "ice", 67, 's', 'j', 0.5f, 2.5f},
    {"aV", "snow", 66, 'u', 'k', 0.2f, 1.0f},
    {"aW", "cactus", 70, 'v', 'k', 0.4f, 2.0f},
    {"aX", "clay", 72, 'w', 'f', 0.6f, 3.0f},
    {"aY", "reeds", 73, 'j', 'g', 0.0f, 0.0f},
    {"aZ", "jukebox", 74, 'd', 'h', 2.0f, 30.0f},
    {"ba", "fence", 4, 'd', 'e', 2.0f, 15.0f},
    {"bb", "pumpkin", 102, 'x', 'e', 1.0f, 5.0f},
    {"bc", "hellrock", 103, 'e', 'h', 0.4f, 2.0f},
    {"bd", "hellsand", 104, 'n', 'l', 0.5f, 2.5f},
    {"be", "lightgem", 105, 'e', 'j', 0.3f, 1.5f},
    {"bf", "portal", 14, 'y', 'j', -1.0f, 0.0f},
    {"bg", "litpumpkin", 102, 'x', 'e', 1.0f, 5.0f},
    {"bh", "cake", 121, 'z', 'k', 0.5f, 2.5f},
    {"bi", "diode", 6, 'o', 'e', 0.0f, 0.0f},
    {"bj", "diode", 6, 'o', 'e', 0.0f, 0.0f},
    {"bk", "lockedchest", 26, 'd', 'e', 0.0f, 0.0f},
    {"bl", "trapdoor", 84, 'd', 'e', 3.0f, 15.0f},
};

/* Captured base-constructor predicates and subsequent literal setters at
 * PC3395, rather than recomputation from a completed object's material. */
static const uint8_t nonopaque[] = {6,  8,  9,  10, 11, 20, 26, 27, 28, 29, 30, 31, 32,
                                    33, 34, 36, 37, 38, 39, 40, 43, 44, 50, 51, 52, 53,
                                    55, 59, 60, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72,
                                    75, 76, 77, 78, 79, 81, 83, 85, 90, 92, 93, 94, 96};
static const uint8_t ticking[] = {2,  6,  8,  10, 11, 18, 28, 31, 32, 37, 38, 39, 40, 50, 51, 59,
                                  60, 70, 72, 74, 75, 76, 77, 78, 79, 80, 81, 83, 86, 91, 92, 95};
static const uint8_t tiles[] = {23, 25, 36, 52, 54, 61, 62, 63, 68, 84};
static const uint8_t metadata[] = {5,  6,  8,  9,  10, 11, 17, 18, 23, 25, 26, 27, 28,
                                   29, 33, 34, 35, 50, 51, 53, 54, 55, 59, 61, 62, 63,
                                   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
                                   77, 84, 85, 86, 91, 92, 93, 94, 95, 96};
static const uint8_t nonsupporting_material[] = {6,  27, 28, 31, 32, 37, 38, 39, 40, 50, 51, 55,
                                                 59, 65, 66, 69, 75, 76, 77, 78, 83, 90, 93, 94};
static const uint8_t no_stats[] = {7,  8,  9,  10, 11, 18, 26, 51, 52, 55,
                                   59, 63, 64, 68, 71, 83, 92, 93, 94, 96};
static const uint8_t random_ids[] = {23, 54, 61, 62};
_Static_assert(COUNT(blocks) == 96, "all original named Block fields required");

static bool contains(const uint8_t *values, size_t count, size_t id)
{
    for (size_t i = 0; i < count; ++i)
        if ((size_t)values[i] == id)
            return true;
    return false;
}
#define HAS(array, id) contains((array), COUNT(array), (id))

static bool text_is(const mc_registry_string *value, const char *prefix, const char *suffix)
{
    mc_registry_string_view view;
    if (!mc_registry_string_read(value, &view))
        return false;
    size_t first = strlen(prefix), second = strlen(suffix);
    if (view.length != first + second || view.units == NULL)
        return false;
    for (size_t i = 0; i < first; ++i)
        if (view.units[i] != (uint16_t)(unsigned char)prefix[i])
            return false;
    for (size_t i = 0; i < second; ++i)
        if (view.units[first + i] != (uint16_t)(unsigned char)suffix[i])
            return false;
    return true;
}

static mc_material_id material_id(char letter)
{
    if (letter == 'A')
        return MC_MATERIAL_A;
    if (letter == 'B')
        return MC_MATERIAL_B;
    return (mc_material_id)((int)MC_MATERIAL_a + (int)letter - (int)'a');
}

static const mc_block_sound *sound_at(const mc_registry_block_view *view, char field)
{
    switch (field) {
        case 'd':
            return view->d;
        case 'e':
            return view->e;
        case 'f':
            return view->f;
        case 'g':
            return view->g;
        case 'h':
            return view->h;
        case 'i':
            return view->i;
        case 'j':
            return view->j;
        case 'k':
            return view->k;
        case 'l':
            return view->l;
        default:
            return NULL;
    }
}

static mc_java_random *random_at(const mc_registry_block_view *view, size_t id)
{
    if (view->m == NULL || view->m[id] == NULL)
        return NULL;
    if (id == 23)
        return view->m[id]->subtype.xq.a;
    if (id == 54)
        return view->m[id]->subtype.e.a;
    return view->m[id]->subtype.tc.a;
}

static uint64_t uniquifier(const mc_java_random_profile *profile)
{
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static uint64_t advanced(uint64_t value, size_t count)
{
    while (count-- != 0)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static bool same_random(const mc_java_random *a, const mc_java_random *b)
{
    uint64_t left, right;
    memcpy(&left, &a->next_next_gaussian, sizeof(left));
    memcpy(&right, &b->next_next_gaussian, sizeof(right));
    return a->state == b->state && left == right &&
           a->have_next_next_gaussian == b->have_next_next_gaussian;
}

static int check_profile(const mc_java_random_profile *profile, uint64_t expected)
{
    int result = 1;
    CHECK(uniquifier(profile) == expected);
    CHECK(profile->math == NULL);
    CHECK(atomic_load_explicit(&profile->holder_phase, memory_order_relaxed) ==
          MC_JAVA_RANDOM_HOLDER_COLD);
    result = 0;
cleanup:
    return result;
}

static int check_bounds(const mc_block_fields *f)
{
    int result = 1;
    /* All six fields are observed. Exact nondefault constructors below retain
     * the original float arithmetic before widening to these double fields. */
    CHECK(0.0 <= f->bs && f->bs <= f->bv && f->bv <= 1.0);
    CHECK(0.0 <= f->bt && f->bt <= f->bw && f->bw <= 1.0);
    CHECK(0.0 <= f->bu && f->bu <= f->bx && f->bx <= 1.0);
    float radius = 0.0f, height = 1.0f;
    switch (f->bn) {
        case 6:
        case 31:
        case 32:
            radius = 0.4f;
            height = 0.8f;
            break;
        case 37:
        case 38:
            radius = 0.2f;
            height = 0.2f * 3.0f;
            break;
        case 39:
        case 40:
            radius = 0.2f;
            height = 0.4f;
            break;
        case 26:
            radius = 0.5f;
            height = 0.5625f;
            break;
        case 27:
        case 28:
        case 66:
        case 78:
        case 93:
        case 94:
            radius = 0.5f;
            height = 0.125f;
            break;
        case 44:
            radius = 0.5f;
            height = 0.5f;
            break;
        case 55:
            radius = 0.5f;
            height = 0.0625f;
            break;
        case 59:
            radius = 0.5f;
            height = 0.25f;
            break;
        case 60:
            radius = 0.5f;
            height = 0.9375f;
            break;
        case 70:
        case 72:
            radius = 0.4375f;
            height = 0.03125f;
            break;
        case 63:
        case 68:
            radius = 0.25f;
            height = 1.0f;
            break;
        case 83:
            radius = 0.375f;
            height = 1.0f;
            break;
        default:
            break;
    }
    if (radius != 0.0f) {
        CHECK(f->bs == (double)(0.5f - radius));
        CHECK(f->bu == (double)(0.5f - radius));
        CHECK(f->bv == (double)(0.5f + radius));
        CHECK(f->bx == (double)(0.5f + radius));
        CHECK(f->bt == 0.0 && f->bw == (double)height);
    }
    if (f->bn == 1 || f->bn == 43) {
        CHECK(f->bs == 0.0 && f->bt == 0.0 && f->bu == 0.0);
        CHECK(f->bv == 1.0 && f->bw == 1.0 && f->bx == 1.0);
    }
    result = 0;
cleanup:
    return result;
}

static int check_complete(mc_registry_classes *owner, mc_materials *materials,
                          mc_registry_block_view *view)
{
    int result = 1;
    mc_registry_error error;
    CHECK(mc_registry_classes_inspect_blocks(owner, view, &error) == MC_REGISTRY_OK);
    CHECK(view->phase == MC_REGISTRY_FAILED && view->initializer_pc == 3398);
    CHECK(view->named_blocks == 96);
    const void *arrays[] = {view->m, view->n, view->o, view->p, view->q, view->r, view->s, view->t};
    for (size_t i = 0; i < COUNT(arrays); ++i) {
        CHECK(arrays[i] != NULL);
        for (size_t j = 0; j < i; ++j)
            CHECK(arrays[i] != arrays[j]);
    }
    static const char *const sound_names[] = {"stone", "wood",  "gravel", "grass", "stone",
                                              "stone", "stone", "cloth",  "sand"};
    for (size_t i = 0; i < COUNT(sound_names); ++i) {
        const mc_block_sound *sound = sound_at(view, (char)('d' + (int)i));
        const mc_block_sound_fields *fields = mc_block_sound_fields_get(sound);
        CHECK(fields != NULL);
        CHECK(text_is(fields->a, "", sound_names[i]));
        CHECK(fields->b == 1.0f && fields->c == (i == 5 ? 1.5f : 1.0f));
        CHECK(sound->kind == (i == 6 ? 1u : i == 8 ? 2u : 0u));
        for (size_t j = 0; j < i; ++j)
            CHECK(sound != sound_at(view, (char)('d' + (int)j)));
    }
    CHECK(view->d->fields.a == view->h->fields.a);
    CHECK(view->d->fields.a == view->i->fields.a);
    CHECK(view->d->fields.a == view->j->fields.a);
    CHECK(!mc_registry_string_read(view->d->fields.a, NULL));
    for (size_t id = 1; id <= COUNT(blocks); ++id) {
        const block_case *expected = &blocks[id - 1];
        const mc_block *named = NULL;
        CHECK(mc_registry_classes_inspect_block_static(owner, expected->field, &named, &error) ==
              MC_REGISTRY_OK);
        CHECK(named != NULL && named == view->m[id]);
        CHECK(named->classes == owner && named->phase == MC_BLOCK_CONSTRUCTED);
        const mc_block_fields *f = mc_block_fields_get(named);
        CHECK(f != NULL && f->bn == (int32_t)id && f->bm == expected->texture);
        CHECK(f->bo == expected->hardness && f->bp == expected->resistance);
        CHECK(f->bq && f->br == !HAS(no_stats, id));
        CHECK(f->by == sound_at(view, expected->sound));
        CHECK(f->bz == 1.0f && f->bB == (id == 79 ? 0.98f : 0.6f));
        CHECK(f->bA == mc_materials_get(materials, material_id(expected->material)));
        CHECK(expected->name == NULL ? f->a == NULL : text_is(f->a, "tile.", expected->name));
        CHECK(check_bounds(f) == 0);
        CHECK(view->n[id] == HAS(ticking, id));
        CHECK(view->o[id] == !HAS(nonopaque, id));
        CHECK(view->p[id] == HAS(tiles, id));
        CHECK(view->r[id] == HAS(nonsupporting_material, id));
        CHECK(view->t[id] == HAS(metadata, id));
        int32_t opacity = HAS(nonopaque, id) ? 0 : 255;
        if (id == 8 || id == 9 || id == 79)
            opacity = 3;
        if (id == 18 || id == 30)
            opacity = 1;
        if (id == 10 || id == 11 || id == 43 || id == 44 || id == 53 || id == 60 || id == 67)
            opacity = 255;
        CHECK(view->q[id] == opacity);
        int32_t emission = 0;
        switch (id) {
            case 10:
            case 11:
            case 51:
            case 89:
            case 91:
            case 95:
                emission = 15;
                break;
            case 39:
                emission = 1;
                break;
            case 50:
                emission = 14;
                break;
            case 62:
                emission = 13;
                break;
            case 74:
            case 94:
                emission = 9;
                break;
            case 76:
                emission = 7;
                break;
            case 90:
                emission = 11;
                break;
            default:
                break;
        }
        CHECK(view->s[id] == emission);
        for (size_t j = 1; j < id; ++j) {
            CHECK(view->m[j] != named);
            if (f->a != NULL && view->m[j]->fields.a != NULL)
                CHECK(f->a != view->m[j]->fields.a);
        }
    }
    for (size_t id = 0; id < 256; ++id) {
        if (id > 0 && id <= COUNT(blocks))
            continue;
        CHECK(view->m[id] == NULL);
        CHECK(!view->n[id] && !view->o[id] && !view->p[id] && !view->r[id] && !view->t[id]);
        CHECK(view->q[id] == 0 && view->s[id] == 0);
    }
    /* Opacity was called before subtype fields: the leaf's inherited b is
     * false, while double slab b became true only after o was captured. */
    CHECK(!view->m[18]->nr_b && view->m[18]->subtype.bk.c == 52);
    CHECK(view->m[18]->subtype.bk.a == NULL && view->o[18]);
    CHECK(view->m[43]->subtype.ys.b && !view->m[44]->subtype.ys.b);
    CHECK(!view->o[43] && !view->o[44]);
    CHECK(view->m[53]->subtype.ss.a == view->m[5]);
    CHECK(view->m[67]->subtype.ss.a == view->m[4]);
    CHECK(view->m[29]->subtype.jq.a && !view->m[29]->subtype.jq.b);
    CHECK(!view->m[33]->subtype.jq.a && !view->m[33]->subtype.jq.b);
    CHECK(view->m[34]->subtype.h.a == -1);
    CHECK(!view->m[61]->subtype.tc.b && view->m[62]->subtype.tc.b);
    CHECK(view->m[63]->subtype.uj.b && !view->m[68]->subtype.uj.b);
    CHECK(view->m[63]->subtype.uj.a == view->m[68]->subtype.uj.a);
    CHECK(view->m[63]->subtype.uj.a == &owner->sign_class);
    CHECK(strcmp(owner->sign_class.client_name, "yk") == 0);
    CHECK(strcmp(owner->sign_class.server_name, "pq") == 0);
    CHECK(!owner->gk_a && !owner->tc_c);
    CHECK(owner->nr_state.phase == MC_REGISTRY_READY && owner->jp_state.phase == MC_REGISTRY_READY);
    CHECK(owner->rp_state.phase == MC_REGISTRY_READY && owner->rw_state.phase == MC_REGISTRY_READY);
    CHECK(owner->block_states[MC_BLOCK_UU_NA].phase == MC_REGISTRY_FAILED);
    for (size_t i = 1; i < MC_BLOCK_KIND_COUNT; ++i)
        CHECK(owner->block_states[i].phase == MC_REGISTRY_READY);
    for (size_t id = 8; id <= 10; id += 2) {
        const mc_block *block = view->m[id];
        CHECK(block->subtype.om.a == 0 && block->subtype.om.b != NULL &&
              block->subtype.om.c != NULL);
        CHECK((const void *)block->subtype.om.b != (const void *)block->subtype.om.c);
        for (size_t i = 0; i < 4; ++i)
            CHECK(!block->subtype.om.b[i] && block->subtype.om.c[i] == 0);
    }
    CHECK(view->m[8]->subtype.om.b != view->m[10]->subtype.om.b);
    CHECK(view->m[8]->subtype.om.c != view->m[10]->subtype.om.c);
    CHECK(view->m[51]->subtype.yq.a != NULL && view->m[51]->subtype.yq.b != NULL);
    CHECK(view->m[51]->subtype.yq.a != view->m[51]->subtype.yq.b);
    for (size_t i = 0; i < 256; ++i)
        CHECK(view->m[51]->subtype.yq.a[i] == 0 && view->m[51]->subtype.yq.b[i] == 0);
    CHECK(owner->ve_a != NULL && owner->ys_a != NULL && owner->db_b != NULL);
    static const int32_t directions[4][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}};
    static const char *const slab_names[] = {"stone", "sand", "wood", "cobble"};
    for (size_t i = 0; i < 4; ++i) {
        CHECK(owner->ve_a[i] != NULL);
        CHECK(owner->ve_a[i][0] == directions[i][0] && owner->ve_a[i][1] == directions[i][1]);
        CHECK(text_is(owner->ys_a[i], "", slab_names[i]));
        for (size_t j = 0; j < i; ++j)
            CHECK(owner->ve_a[i] != owner->ve_a[j]);
    }
    CHECK(owner->ys_a[0] == view->d->fields.a);
    CHECK(owner->ys_a[1] == view->l->fields.a && owner->ys_a[2] == view->e->fields.a);
    CHECK(owner->db_b->elements == NULL && owner->db_b->count == 0 && owner->db_b->capacity == 0);
    CHECK(owner->wo_a != NULL && owner->wo_b != NULL);
    CHECK((const void *)owner->wo_a != (const void *)owner->wo_b);
    static const double offsets[] = {-0.0625, 0.0625, 0.1875, 0.3125};
    for (size_t i = 0; i < 4; ++i)
        CHECK(owner->wo_a[i] == offsets[i] && owner->wo_b[i] == (int32_t)i + 1);
    CHECK(owner->pressure_state.phase == MC_REGISTRY_READY && owner->pressure_values != NULL);
    const mc_pressure_category *categories[] = {owner->pressure_a, owner->pressure_b,
                                                owner->pressure_c};
    static const char *const category_names[] = {"everything", "mobs", "players"};
    for (size_t i = 0; i < 3; ++i) {
        CHECK(categories[i] != NULL && owner->pressure_values[i] == categories[i]);
        CHECK(categories[i]->ordinal == (int32_t)i);
        CHECK(text_is(categories[i]->name, "", category_names[i]));
        for (size_t j = 0; j < i; ++j)
            CHECK(categories[i] != categories[j]);
    }
    CHECK(view->m[70]->subtype.bv.a == owner->pressure_b);
    CHECK(view->m[72]->subtype.bv.a == owner->pressure_a);
    const mc_registry_wire_set *wire = view->m[55]->subtype.sm.b;
    CHECK(view->m[55]->subtype.sm.a && wire != NULL && wire->map != NULL);
    CHECK(wire->map->table == NULL && wire->map->size == 0 && wire->map->threshold == 0);
    CHECK(wire->map->load_factor == 0.75f && wire->map->mod_count == 0);
    for (size_t i = 0; i < COUNT(random_ids); ++i) {
        const mc_java_random *random = random_at(view, random_ids[i]);
        uint64_t cache;
        CHECK(random != NULL && random->state <= UINT64_C(0xffffffffffff));
        memcpy(&cache, &random->next_next_gaussian, sizeof(cache));
        CHECK(cache == 0 && !random->have_next_next_gaussian);
        for (size_t j = 0; j < i; ++j)
            CHECK(random != random_at(view, random_ids[j]));
    }
    result = 0;
cleanup:
    return result;
}

static int check_retry(mc_registry_classes *owner, mc_java_random_profile *profile)
{
    int result = 1;
    mc_registry_error error;
    mc_registry_block_view before, after;
    unsigned char saved[sizeof(*owner)];
    memcpy(saved, owner, sizeof(saved));
    mc_block *slots[256];
    bool n[256], o[256], p[256], r[256], t[256];
    int32_t q[256], s[256];
    unsigned char objects[96][sizeof(mc_block)];
    mc_java_random randoms[4];
    const mc_java_random *identities[4];
    const mc_registry_error failure = owner->block_failure;
    CHECK(mc_registry_classes_inspect_blocks(owner, &before, &error) == MC_REGISTRY_OK);
    CHECK(before.m != NULL && before.n != NULL && before.o != NULL && before.p != NULL);
    CHECK(before.q != NULL && before.r != NULL && before.s != NULL && before.t != NULL);
    memcpy(slots, before.m, sizeof(slots));
    memcpy(n, before.n, sizeof(n));
    memcpy(o, before.o, sizeof(o));
    memcpy(p, before.p, sizeof(p));
    memcpy(q, before.q, sizeof(q));
    memcpy(r, before.r, sizeof(r));
    memcpy(s, before.s, sizeof(s));
    memcpy(t, before.t, sizeof(t));
    for (size_t i = 0; i < COUNT(blocks); ++i)
        if (slots[i + 1] != NULL)
            memcpy(objects[i], slots[i + 1], sizeof(objects[i]));
    for (size_t i = 0; i < COUNT(random_ids); ++i) {
        identities[i] = random_at(&before, random_ids[i]);
        if (identities[i] != NULL)
            randoms[i] = *identities[i];
    }
    uint64_t state = uniquifier(profile);
    CHECK(mc_registry_classes_initialize_blocks(owner, &error) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(error.status == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(error.initializer_pc == before.initializer_pc);
    CHECK(error.active_initializer_class == failure.active_initializer_class &&
          error.active_initializer_pc == failure.active_initializer_pc);
    CHECK(error.constructor_class == failure.constructor_class &&
          error.constructor_pc == failure.constructor_pc &&
          error.profile_status == failure.profile_status);
    /* Byte snapshots are only observations of this same retained owner. */
    CHECK(memcmp(owner, saved, sizeof(saved)) == 0);
    CHECK(mc_registry_classes_inspect_blocks(owner, &after, &error) == MC_REGISTRY_OK);
    CHECK(after.phase == before.phase && after.initializer_pc == before.initializer_pc);
    CHECK(after.allocation_bytes == before.allocation_bytes &&
          after.named_blocks == before.named_blocks);
    CHECK(after.m == before.m && after.n == before.n && after.o == before.o && after.p == before.p);
    CHECK(after.q == before.q && after.r == before.r && after.s == before.s && after.t == before.t);
    CHECK(memcmp(after.m, slots, sizeof(slots)) == 0);
    CHECK(memcmp(after.n, n, sizeof(n)) == 0 && memcmp(after.o, o, sizeof(o)) == 0);
    CHECK(memcmp(after.p, p, sizeof(p)) == 0 && memcmp(after.q, q, sizeof(q)) == 0);
    CHECK(memcmp(after.r, r, sizeof(r)) == 0 && memcmp(after.s, s, sizeof(s)) == 0);
    CHECK(memcmp(after.t, t, sizeof(t)) == 0);
    for (size_t i = 0; i < COUNT(blocks); ++i)
        if (slots[i + 1] != NULL)
            CHECK(memcmp(slots[i + 1], objects[i], sizeof(objects[i])) == 0);
    for (size_t i = 0; i < COUNT(random_ids); ++i) {
        CHECK(random_at(&after, random_ids[i]) == identities[i]);
        if (identities[i] != NULL)
            CHECK(same_random(identities[i], &randoms[i]));
    }
    CHECK(check_profile(profile, state) == 0);
    result = 0;
cleanup:
    return result;
}

static int check_statistics_prefix(const mc_registry_classes *owner, mc_registry_side side,
                                   const mc_registry_error *failure)
{
    int result = 1;
    mc_registry_statistic_base_view base;
    mc_registry_statistic_view statistics;
    mc_registry_translation_view translation;
    mc_registry_statistic_key_view keys;
    const char *singleton_class = side == MC_REGISTRY_CLIENT ? "nh" : "ii";
    CHECK(owner->options.resources == NULL);
    CHECK(failure->status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(failure->active_initializer_class != NULL &&
          strcmp(failure->active_initializer_class, singleton_class) == 0 &&
          failure->active_initializer_pc == 4);
    CHECK(failure->constructor_class != NULL &&
          strcmp(failure->constructor_class, singleton_class) == 0 &&
          failure->constructor_pc == 23 && failure->profile_status == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_registry_classes_inspect_statistic_base(owner, &base, NULL) == MC_REGISTRY_OK);
    CHECK(base.phase == MC_REGISTRY_READY && base.initializer_pc == 51);
    CHECK(base.b != NULL && base.c != NULL && base.b != base.c);
    CHECK(base.i != NULL && base.j != NULL && base.k != NULL && base.i != base.j &&
          base.i != base.k && base.j != base.k);
    CHECK(mc_registry_classes_inspect_statistics(owner, &statistics, NULL) == MC_REGISTRY_OK);
    CHECK(statistics.phase == MC_REGISTRY_FAILED && statistics.initializer_pc == 59);
    CHECK(owner->statistics.a != NULL && statistics.map_size == 0);
    const mc_statistic_list *lists[] = {statistics.b, statistics.c, statistics.d, statistics.e};
    for (size_t i = 0; i < COUNT(lists); ++i) {
        CHECK(lists[i] != NULL && mc_statistic_list_size(lists[i]) == 0);
        for (size_t j = 0; j < i; ++j)
            CHECK(lists[i] != lists[j]);
    }
    CHECK(statistics.named != NULL);
    for (size_t i = 0; i < 23; ++i)
        CHECK(statistics.named[i] == NULL);
    CHECK(statistics.C == NULL && statistics.D == NULL && statistics.E == NULL &&
          statistics.F == NULL);
    CHECK(!statistics.G && !statistics.H && statistics.duplicate_message == NULL);
    CHECK(mc_registry_classes_inspect_translation(owner, &translation, NULL) == MC_REGISTRY_OK);
    CHECK(translation.singleton_phase == MC_REGISTRY_FAILED &&
          translation.singleton_initializer_pc == 4);
    CHECK(translation.wrapper_phase == MC_REGISTRY_FAILED &&
          translation.wrapper_initializer_pc == 0);
    CHECK(translation.singleton_a == NULL && translation.wrapper_a == NULL);
    CHECK(owner->translations.pending != NULL && owner->translations.pending->constructor_pc == 23);
    CHECK(mc_registry_classes_inspect_statistic_keys(owner, &keys, NULL) == MC_REGISTRY_OK);
    CHECK(keys.phase == MC_REGISTRY_COLD && keys.initializer_pc == -1);
    CHECK(keys.a == NULL && keys.construction == NULL && !keys.map_published);
    CHECK(keys.constructor_pc == -1 && keys.caught_status == MC_REGISTRY_OK &&
          keys.caught_pc == -1);
    result = 0;
cleanup:
    return result;
}

static int test_cold(mc_registry_side side, mc_java_random_profile *profile,
                     mc_materials *materials)
{
    int result = 1;
    mc_registry_classes *owner = NULL, *sentinel = NULL;
    mc_registry_error error;
    mc_registry_options options = {side, profile, materials, 0, NULL};
    mc_registry_block_view view = {.initializer_pc = 12345, .named_blocks = 77};
    uint64_t state = uniquifier(profile);
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    CHECK(owner != NULL);
    sentinel = owner;
    CHECK(mc_registry_classes_allocate(NULL, &sentinel, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(sentinel == owner);
    CHECK(mc_registry_classes_allocate(&options, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    options.max_allocation_bytes = 1;
    CHECK(mc_registry_classes_allocate(&options, &sentinel, &error) == MC_REGISTRY_LIMIT);
    CHECK(sentinel == owner);
    options.max_allocation_bytes = 0;
    options.random_profile = NULL;
    CHECK(mc_registry_classes_allocate(&options, &sentinel, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(sentinel == owner);
    options.random_profile = profile;
    options.materials = NULL;
    CHECK(mc_registry_classes_allocate(&options, &sentinel, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(sentinel == owner);
    options.materials = materials;
    options.side = (mc_registry_side)99;
    CHECK(mc_registry_classes_allocate(&options, &sentinel, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(sentinel == owner);
    CHECK(mc_registry_classes_inspect_blocks(NULL, &view, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(view.initializer_pc == 12345 && view.named_blocks == 77);
    CHECK(mc_registry_classes_inspect_blocks(owner, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_inspect_blocks(owner, &view, &error) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_COLD && view.initializer_pc == -1 && view.named_blocks == 0);
    CHECK(view.allocation_bytes != 0 && owner->allocations == NULL && owner->literals == NULL);
    CHECK(view.d == NULL && view.e == NULL && view.f == NULL && view.g == NULL && view.h == NULL);
    CHECK(view.i == NULL && view.j == NULL && view.k == NULL && view.l == NULL);
    CHECK(view.m == NULL && view.n == NULL && view.o == NULL && view.p == NULL);
    CHECK(view.q == NULL && view.r == NULL && view.s == NULL && view.t == NULL);
    for (size_t i = 0; i < COUNT(blocks); ++i) {
        const mc_block *block = NULL;
        CHECK(mc_registry_classes_inspect_block_static(owner, blocks[i].field, &block, &error) ==
              MC_REGISTRY_OK);
        CHECK(block == NULL);
    }
    const mc_block *out = (const mc_block *)owner;
    CHECK(mc_registry_classes_inspect_block_static(owner, "m", &out, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(out == (const mc_block *)owner);
    CHECK(mc_registry_classes_inspect_block_static(owner, "BL", &out, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(out == (const mc_block *)owner);
    CHECK(mc_registry_classes_inspect_block_static(owner, NULL, &out, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(out == (const mc_block *)owner);
    CHECK(mc_registry_classes_inspect_block_static(NULL, "u", &out, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(out == (const mc_block *)owner);
    CHECK(mc_registry_classes_inspect_block_static(owner, "u", NULL, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_block_fields_get(NULL) == NULL && mc_block_sound_fields_get(NULL) == NULL);
    uint16_t marker = 7;
    mc_registry_string_view text = {.units = &marker, .length = 12345};
    CHECK(!mc_registry_string_read(NULL, &text));
    CHECK(text.units == &marker && text.length == 12345);
    CHECK(mc_registry_classes_initialize_blocks(NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(check_profile(profile, state) == 0);
    CHECK(mc_registry_classes_free(owner, &error) == MC_REGISTRY_OK);
    owner = NULL;
    CHECK(check_profile(profile, state) == 0);
    CHECK(mc_registry_classes_free(NULL, &error) == MC_REGISTRY_OK);
    result = 0;
cleanup:
    if (owner != NULL)
        (void)mc_registry_classes_free(owner, NULL);
    return result;
}

static int test_owners(mc_registry_side side, mc_java_random_profile *profile,
                       mc_materials *materials, size_t *complete_bytes)
{
    int result = 1;
    mc_registry_classes *first = NULL, *second = NULL;
    mc_registry_options options = {side, profile, materials, 0, NULL};
    mc_registry_error error;
    mc_registry_block_view a, b;
    uint64_t state = uniquifier(profile);
    CHECK(mc_registry_classes_allocate(&options, &first, &error) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_allocate(&options, &second, &error) == MC_REGISTRY_OK);
    CHECK(first != second);
    CHECK(check_profile(profile, state) == 0);
    CHECK(mc_registry_classes_initialize_blocks(first, &error) ==
          MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(error.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && error.initializer_pc == 3398);
    CHECK(check_statistics_prefix(first, side, &error) == 0);
    CHECK(check_profile(profile, advanced(state, 5)) == 0);
    CHECK(check_complete(first, materials, &a) == 0);
    CHECK(check_retry(first, profile) == 0);
    CHECK(check_complete(first, materials, &a) == 0);
    CHECK(mc_registry_classes_initialize_blocks(second, &error) ==
          MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(error.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && error.initializer_pc == 3398);
    CHECK(check_statistics_prefix(second, side, &error) == 0);
    CHECK(check_profile(profile, advanced(state, 10)) == 0);
    CHECK(check_complete(second, materials, &b) == 0);
    CHECK(a.m != b.m && a.n != b.n && a.o != b.o && a.p != b.p);
    CHECK(a.q != b.q && a.r != b.r && a.s != b.s && a.t != b.t);
    CHECK(a.allocation_bytes == b.allocation_bytes);
    for (size_t id = 1; id <= COUNT(blocks); ++id) {
        CHECK(a.m[id] != b.m[id]);
        CHECK(a.m[id]->fields.bA == b.m[id]->fields.bA);
        CHECK(a.m[id]->fields.by != b.m[id]->fields.by);
    }
    for (size_t i = 0; i < COUNT(random_ids); ++i)
        CHECK(random_at(&a, random_ids[i]) != random_at(&b, random_ids[i]));
    CHECK(mc_registry_classes_free(first, &error) == MC_REGISTRY_OK);
    first = NULL;
    CHECK(check_profile(profile, advanced(state, 10)) == 0);
    CHECK(check_complete(second, materials, &b) == 0);
    CHECK(check_retry(second, profile) == 0);
    *complete_bytes = b.allocation_bytes;
    result = 0;
cleanup:
    if (second != NULL)
        (void)mc_registry_classes_free(second, NULL);
    if (first != NULL)
        (void)mc_registry_classes_free(first, NULL);
    return result;
}

/* Each attempt consumes its real reached default constructors. Count them
 * against the SAME shared full-width profile; never reset it during search. */
static int limited_owner(mc_registry_side side, mc_java_random_profile *profile,
                         mc_materials *materials, size_t limit, mc_registry_classes **out)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    mc_registry_options options = {side, profile, materials, limit, NULL};
    mc_registry_error error;
    uint64_t state = uniquifier(profile);
    mc_registry_status status = mc_registry_classes_allocate(&options, &owner, &error);
    CHECK(status == MC_REGISTRY_OK || status == MC_REGISTRY_LIMIT);
    size_t calls = 0;
    if (status == MC_REGISTRY_OK) {
        status = mc_registry_classes_initialize_blocks(owner, &error);
        CHECK(status == MC_REGISTRY_LIMIT || status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
        if (status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY) {
            CHECK(error.initializer_pc == 3398);
            CHECK(check_statistics_prefix(owner, side, &error) == 0);
        } else {
            CHECK(error.status == MC_REGISTRY_LIMIT);
        }
        mc_registry_block_view view;
        CHECK(mc_registry_classes_inspect_blocks(owner, &view, &error) == MC_REGISTRY_OK);
        CHECK(view.phase == MC_REGISTRY_FAILED && view.allocation_bytes <= limit);
        for (size_t i = 0; i < COUNT(random_ids); ++i)
            if (random_at(&view, random_ids[i]) != NULL)
                ++calls;
        if (owner->items.b != NULL)
            ++calls;
    }
    CHECK(check_profile(profile, advanced(state, calls)) == 0);
    *out = owner;
    owner = NULL;
    result = 0;
cleanup:
    if (owner != NULL)
        (void)mc_registry_classes_free(owner, NULL);
    return result;
}

static int test_random_prefixes(mc_registry_side side, mc_java_random_profile *profile,
                                mc_materials *materials, size_t upper)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    mc_registry_error error;
    for (size_t target = 0; target < COUNT(random_ids); ++target) {
        size_t id = random_ids[target], low = 1, high = upper;
        while (low < high) {
            size_t middle = low + (high - low) / 2;
            CHECK(limited_owner(side, profile, materials, middle, &owner) == 0);
            bool published =
                owner != NULL && owner->blocks.m != NULL && owner->blocks.m[id] != NULL;
            CHECK(mc_registry_classes_free(owner, &error) == MC_REGISTRY_OK);
            owner = NULL;
            if (published)
                high = middle;
            else
                low = middle + 1;
        }
        CHECK(low > 1 && low < upper);
        CHECK(limited_owner(side, profile, materials, low - 1, &owner) == 0);
        CHECK(owner == NULL || owner->blocks.m == NULL || owner->blocks.m[id] == NULL);
        CHECK(mc_registry_classes_free(owner, &error) == MC_REGISTRY_OK);
        owner = NULL;
        uint64_t before = uniquifier(profile);
        CHECK(limited_owner(side, profile, materials, low, &owner) == 0);
        CHECK(owner != NULL);
        mc_registry_block_view view;
        CHECK(mc_registry_classes_inspect_blocks(owner, &view, &error) == MC_REGISTRY_OK);
        CHECK(view.phase == MC_REGISTRY_FAILED && view.allocation_bytes == low);
        CHECK(owner->last_error.status == MC_REGISTRY_LIMIT);
        CHECK(view.named_blocks == id - 1 && view.m[id] != NULL && view.p[id]);
        CHECK(view.m[id + 1] == NULL && random_at(&view, id) == NULL);
        CHECK(view.m[id]->phase == MC_BLOCK_CONSTRUCTION_FAILED);
        CHECK(view.m[id]->constructor_pc == 9);
        CHECK(owner->last_error.constructor_pc == 9 && owner->last_error.block_id == (int32_t)id);
        const char *expected_class = target == 0   ? (side == MC_REGISTRY_CLIENT ? "xq" : "pa")
                                     : target == 1 ? "e"
                                                   : (side == MC_REGISTRY_CLIENT ? "tc" : "lz");
        CHECK(strcmp(view.m[id]->constructor_class, expected_class) == 0);
        const mc_block *named = view.m[id];
        CHECK(mc_registry_classes_inspect_block_static(owner, blocks[id - 1].field, &named,
                                                       &error) == MC_REGISTRY_OK);
        CHECK(named == NULL);
        const mc_block_fields *f = mc_block_fields_get(view.m[id]);
        CHECK(f->bn == (int32_t)id && f->bm == 0 && f->bo == 0.0f && f->bp == 0.0f);
        CHECK(f->bq && f->br && f->by == view.d && f->bz == 1.0f && f->bB == 0.6f);
        CHECK(f->bs == 0.0 && f->bt == 0.0 && f->bu == 0.0);
        CHECK(f->bv == 1.0 && f->bw == 1.0 && f->bx == 1.0 && f->a == NULL);
        CHECK(f->bA == mc_materials_get(materials, target == 1 ? MC_MATERIAL_d : MC_MATERIAL_e));
        /* All four inherit the base opacity body, including chest e/e. */
        CHECK(view.o[id] && view.q[id] == 255);
        CHECK(!view.n[id] && !view.r[id] && !view.t[id] && view.s[id] == 0);
        for (size_t previous = 1; previous < id; ++previous) {
            CHECK(mc_registry_classes_inspect_block_static(owner, blocks[previous - 1].field,
                                                           &named, &error) == MC_REGISTRY_OK);
            CHECK(named == view.m[previous] && named != NULL &&
                  named->phase == MC_BLOCK_CONSTRUCTED);
        }
        for (size_t i = 0; i < COUNT(random_ids); ++i)
            CHECK((random_at(&view, random_ids[i]) != NULL) == (i < target));
        CHECK(check_profile(profile, advanced(before, target)) == 0);
        CHECK(check_retry(owner, profile) == 0);
        uint64_t retained = uniquifier(profile);
        CHECK(mc_registry_classes_free(owner, &error) == MC_REGISTRY_OK);
        owner = NULL;
        CHECK(check_profile(profile, retained) == 0);
    }
    result = 0;
cleanup:
    if (owner != NULL)
        (void)mc_registry_classes_free(owner, NULL);
    return result;
}

static int test_cloth_prefix(mc_registry_side side, mc_java_random_profile *profile,
                             mc_materials *materials, size_t upper)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    mc_registry_error error;
    size_t low = 1, high = upper;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        CHECK(limited_owner(side, profile, materials, middle, &owner) == 0);
        bool published = owner != NULL && owner->blocks.m != NULL && owner->blocks.m[35] != NULL;
        CHECK(mc_registry_classes_free(owner, &error) == MC_REGISTRY_OK);
        owner = NULL;
        if (published)
            high = middle;
        else
            low = middle + 1;
    }
    CHECK(low > 1 && low < upper);
    CHECK(limited_owner(side, profile, materials, low - 1, &owner) == 0);
    CHECK(owner == NULL || owner->blocks.m == NULL || owner->blocks.m[35] == NULL);
    CHECK(mc_registry_classes_free(owner, &error) == MC_REGISTRY_OK);
    owner = NULL;
    uint64_t state = uniquifier(profile);
    CHECK(limited_owner(side, profile, materials, low, &owner) == 0);
    CHECK(owner != NULL);
    mc_registry_block_view view;
    CHECK(mc_registry_classes_inspect_blocks(owner, &view, &error) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_FAILED && view.initializer_pc == 1313);
    CHECK(view.allocation_bytes == low && view.named_blocks == 34);
    CHECK(owner->last_error.status == MC_REGISTRY_LIMIT && owner->last_error.block_id == 35);
    CHECK(view.m[35] != NULL && view.m[36] == NULL);
    CHECK(view.m[35]->phase == MC_BLOCK_CONSTRUCTED);
    const char *declaring = side == MC_REGISTRY_CLIENT ? "ee" : "cq";
    CHECK(strcmp(owner->last_error.constructor_class, declaring) == 0);
    const mc_block_fields *f = mc_block_fields_get(view.m[35]);
    CHECK(f->bn == 35 && f->bm == 64 && f->bA == mc_materials_get(materials, MC_MATERIAL_l));
    CHECK(f->bo == 0.8f && f->bp == 4.0f && f->by == view.k && f->a == NULL);
    CHECK(!view.t[35] && !view.p[35]);
    const mc_block *named = view.m[35];
    CHECK(mc_registry_classes_inspect_block_static(owner, "ac", &named, &error) == MC_REGISTRY_OK);
    CHECK(named == NULL);
    CHECK(random_at(&view, 23) != NULL && random_at(&view, 54) == NULL);
    CHECK(check_profile(profile, advanced(state, 1)) == 0);
    CHECK(check_retry(owner, profile) == 0);
    result = 0;
cleanup:
    if (owner != NULL)
        (void)mc_registry_classes_free(owner, NULL);
    return result;
}

static int test_direction_prefix(mc_registry_side side, mc_java_random_profile *profile,
                                 mc_materials *materials, size_t upper)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    size_t low = 1, high = upper;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        CHECK(limited_owner(side, profile, materials, middle, &owner) == 0);
        bool published = owner != NULL && owner->ve_a != NULL;
        CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK);
        owner = NULL;
        if (published)
            high = middle;
        else
            low = middle + 1;
    }
    CHECK(low > 1);
    CHECK(limited_owner(side, profile, materials, low - 1, &owner) == 0);
    CHECK(owner != NULL && owner->ve_a == NULL);
    const mc_registry_class_state *state = &owner->block_states[MC_BLOCK_VE_NJ];
    CHECK(state->phase == MC_REGISTRY_FAILED && state->pc == 49);
    CHECK(state->failure.status == MC_REGISTRY_LIMIT);
    CHECK(state->failure.active_initializer_pc == 49);
    CHECK(state->failure.active_initializer_class != NULL);
    CHECK(strcmp(state->failure.active_initializer_class,
                 side == MC_REGISTRY_CLIENT ? "ve" : "nj") == 0);
    CHECK(check_retry(owner, profile) == 0);
    mc_registry_error failure = owner->block_failure, error;
    uint64_t u = uniquifier(profile);
    /* The remaining bytes cannot allocate Item's first Random. Its distinct
     * failure must not replace the saved nested Block failure on later use. */
    CHECK(mc_registry_classes_initialize_items(owner, &error) == MC_REGISTRY_LIMIT);
    CHECK(error.active_initializer_pc == 0 && owner->items.b == NULL);
    CHECK(strcmp(error.active_initializer_class, side == MC_REGISTRY_CLIENT ? "gm" : "ej") == 0);
    CHECK(mc_registry_classes_initialize_blocks(owner, &error) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(error.initializer_pc == failure.initializer_pc);
    CHECK(error.active_initializer_class == failure.active_initializer_class);
    CHECK(error.active_initializer_pc == failure.active_initializer_pc);
    CHECK(state->failure.status == MC_REGISTRY_LIMIT && state->failure.active_initializer_pc == 49);
    CHECK(check_profile(profile, u) == 0);
    result = 0;
cleanup:
    if (owner != NULL)
        (void)mc_registry_classes_free(owner, NULL);
    return result;
}

static int test_side(mc_registry_side side, mc_java_random_profile *profile,
                     mc_materials *materials)
{
    size_t upper = 0;
    if (test_cold(side, profile, materials) != 0 ||
        test_owners(side, profile, materials, &upper) != 0 ||
        test_random_prefixes(side, profile, materials, upper) != 0 ||
        test_cloth_prefix(side, profile, materials, upper) != 0 ||
        test_direction_prefix(side, profile, materials, upper) != 0)
        return 1;
    return 0;
}

int main(int argc, char **argv)
{
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    bool client = argc == 1 || (argc == 2 && strcmp(argv[1], "client") == 0);
    bool server = argc == 1 || (argc == 2 && strcmp(argv[1], "server") == 0);
    if (!client && !server) {
        fprintf(stderr, "usage: %s [client|server]\n", argv[0]);
        goto cleanup;
    }
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    size_t profile_bytes = mc_java_random_profile_allocation_bytes(profile);
    if (client)
        CHECK(test_side(MC_REGISTRY_CLIENT, profile, materials) == 0);
    if (server)
        CHECK(test_side(MC_REGISTRY_SERVER, profile, materials) == 0);
    CHECK(mc_java_random_profile_allocation_bytes(profile) == profile_bytes);
    printf("PASS: %u registry construction-prefix checks\n", checks);
    result = 0;
cleanup:
    /* Every subordinate helper retires its class owners before returning. */
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
