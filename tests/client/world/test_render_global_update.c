/* Prepared actual N/DK/RAW LocalPlayer fields; no source constructor claim. */
#include "client/application_state.h"
#include "client/render_engine/chunk_statistics.h"
#include "client/render_engine/internal.h"
#include "client/world/source_chunk_compile_internal.h"
#include "entity/entity_internal.h"
#include "mc/client/world/chunk_cache.h"
#include "mc/client/world/render_global_update.h"
#include "mc/core/math/classes.h"
#include "mc/entity/player/player.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/runtime_internal.h"

#include <emmintrin.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_ACTORS = 128, MAX_QUEUE = 128 };

typedef struct actor_input {
    int32_t q, r, s, w;
    bool visible, dirty;
} actor_input;

typedef struct update_input {
    const char *name;
    bool visible_only, null_camera, null_list;
    double camera[3];
    size_t actor_count, queue_count;
    actor_input actors[MAX_ACTORS];
    unsigned int queue[MAX_QUEUE];
} update_input;

/* Exact update inputs from render_global_update/inputs.json, SHA-256
 * ab9deaf1222ac07e190f0258633b9373c74fbc70f2e7717c0948c71751cb356a. */
static const update_input
    inputs[] =
        {
            {.name = "empty",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 0,
             .queue_count = 0,
             .actors =
                 {
                     {0},
                 },
             .queue = {0}},
            {.name = "empty_null_camera",
             .visible_only = false,
             .null_camera = true,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 0,
             .queue_count = 0,
             .actors =
                 {
                     {0},
                 },
             .queue = {0}},
            {.name = "empty_visible_null_camera",
             .visible_only = true,
             .null_camera = true,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 0,
             .queue_count = 0,
             .actors =
                 {
                     {0},
                 },
             .queue = {0}},
            {.name = "null_list",
             .visible_only = false,
             .null_camera = false,
             .null_list = true,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 0,
             .queue_count = 0,
             .actors =
                 {
                     {0},
                 },
             .queue = {0}},
            {.name = "near_invisible",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 1,
             .actors =
                 {
                     {0, 0, 0, 0, false, false},
                 },
             .queue = {1}},
            {.name = "distance_boundary",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 1,
             .actors =
                 {
                     {16, 0, 0, 0, false, false},
                 },
             .queue = {1}},
            {.name = "float_narrow_boundary",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {-1e-08, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 1,
             .actors =
                 {
                     {16, 0, 0, 0, false, false},
                 },
             .queue = {1}},
            {.name = "double_subtract_before_narrow",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {16777216.5, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 1,
             .actors =
                 {
                     {16777200, 0, 0, 0, false, false},
                 },
             .queue = {1}},
            {.name = "single_far",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 1,
             .actors =
                 {
                     {17, 0, 0, 0, false, false},
                 },
             .queue = {1}},
            {.name = "three_far",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 3,
             .queue_count = 3,
             .actors =
                 {
                     {100, 0, 0, 3, false, false},
                     {20, 0, 0, 2, false, false},
                     {50, 0, 0, 1, false, false},
                 },
             .queue = {1, 2, 3}},
            {.name = "far_visibility",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 3,
             .queue_count = 3,
             .actors =
                 {
                     {20, 0, 0, 1, false, false},
                     {100, 0, 0, 2, true, false},
                     {40, 0, 0, 3, true, false},
                 },
             .queue = {1, 2, 3}},
            {.name = "far_visible_beats_nearer",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 2,
             .queue_count = 2,
             .actors =
                 {
                     {100, 0, 0, 1, true, false},
                     {20, 0, 0, 2, false, false},
                 },
             .queue = {1, 2}},
            {.name = "far_equal_distinct_ordinals",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 3,
             .queue_count = 3,
             .actors =
                 {
                     {20, 0, 0, 3, false, false},
                     {20, 0, 0, 1, false, false},
                     {20, 0, 0, 2, false, false},
                 },
             .queue = {1, 2, 3}},
            {.name = "far_equal_same_ordinal",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 3,
             .queue_count = 3,
             .actors =
                 {
                     {20, 0, 0, 7, false, false},
                     {20, 0, 0, 7, false, false},
                     {20, 0, 0, 7, false, false},
                 },
             .queue = {1, 2, 3}},
            {.name = "duplicate_far",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 2,
             .actors =
                 {
                     {20, 0, 0, 0, false, false},
                 },
             .queue = {1, 1}},
            {.name = "duplicate_near",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 2,
             .actors =
                 {
                     {0, 0, 0, 0, false, false},
                 },
             .queue = {1, 1}},
            {.name = "mixed_near_far",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 4,
             .queue_count = 4,
             .actors =
                 {
                     {0, 0, 0, 1, false, false},
                     {20, 0, 0, 2, false, false},
                     {4, 0, 0, 3, true, false},
                     {100, 0, 0, 4, true, false},
                 },
             .queue = {1, 2, 3, 4}},
            {.name = "visible_only_mixed",
             .visible_only = true,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 4,
             .queue_count = 4,
             .actors =
                 {
                     {0, 0, 0, 1, false, false},
                     {100, 0, 0, 2, true, false},
                     {1, 0, 0, 3, true, false},
                     {1000, 0, 0, 4, false, false},
                 },
             .queue = {1, 2, 3, 4}},
            {.name = "visible_only_none_null_camera",
             .visible_only = true,
             .null_camera = true,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 2,
             .queue_count = 2,
             .actors =
                 {
                     {0, 0, 0, 0, false, false},
                     {100, 0, 0, 0, false, false},
                 },
             .queue = {1, 2}},
            {.name = "visible_only_one_null_camera",
             .visible_only = true,
             .null_camera = true,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 1,
             .actors =
                 {
                     {100, 0, 0, 0, true, false},
                 },
             .queue = {1}},
            {.name = "visible_only_two_null_camera",
             .visible_only = true,
             .null_camera = true,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 2,
             .queue_count = 2,
             .actors =
                 {
                     {100, 0, 0, 0, true, false},
                     {0, 0, 0, 0, true, false},
                 },
             .queue = {1, 2}},
            {.name = "normal_null_camera",
             .visible_only = false,
             .null_camera = true,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 1,
             .actors =
                 {
                     {0, 0, 0, 0, false, false},
                 },
             .queue = {1}},
            {.name = "null_first",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 2,
             .actors =
                 {
                     {0, 0, 0, 0, false, false},
                 },
             .queue = {0, 1}},
            {.name = "null_after_near",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 2,
             .queue_count = 3,
             .actors =
                 {
                     {0, 0, 0, 0, false, false},
                     {20, 0, 0, 0, false, false},
                 },
             .queue = {1, 0, 2}},
            {.name = "visible_null_entry",
             .visible_only = true,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 2,
             .actors =
                 {
                     {0, 0, 0, 0, true, false},
                 },
             .queue = {1, 0}},
            {.name = "dirty_null_world_near",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 1,
             .actors =
                 {
                     {0, 0, 0, 0, false, true},
                 },
             .queue = {1}},
            {.name = "dirty_null_world_far",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 1,
             .queue_count = 1,
             .actors =
                 {
                     {20, 0, 0, 0, false, true},
                 },
             .queue = {1}},
            {.name = "clean_before_dirty",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 2,
             .queue_count = 2,
             .actors =
                 {
                     {0, 0, 0, 1, false, false},
                     {4, 0, 0, 2, false, true},
                 },
             .queue = {1, 2}},
            {.name = "equal_ordinal_2",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 2,
             .queue_count = 2,
             .actors =
                 {
                     {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                 },
             .queue = {1, 2}},
            {.name = "equal_ordinal_3",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 3,
             .queue_count = 3,
             .actors =
                 {
                     {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                 },
             .queue = {1, 2, 3}},
            {.name = "equal_ordinal_8",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 8,
             .queue_count = 8,
             .actors =
                 {
                     {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                 },
             .queue = {1, 2, 3, 4, 5, 6, 7, 8}},
            {.name = "equal_ordinal_31",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 31,
             .queue_count = 31,
             .actors =
                 {
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                 },
             .queue = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                       17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31}},
            {.name = "equal_ordinal_32",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 32,
             .queue_count = 32,
             .actors =
                 {
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                 },
             .queue = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                       17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32}},
            {.name = "equal_ordinal_33",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 33,
             .queue_count = 33,
             .actors =
                 {
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false},
                 },
             .queue = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
                       18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33}},
            {.name = "equal_ordinal_64",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 64,
             .queue_count = 64,
             .actors =
                 {
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                     {0, 0, 0, 7, true, false}, {0, 0, 0, 7, true, false},
                 },
             .queue = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                       17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
                       33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
                       49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64}},
            {.name = "mixed_runs_31",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 31,
             .queue_count = 31,
             .actors =
                 {
                     {-3, 0, 0, 0, false, false}, {3, 0, 0, 2, true, false},
                     {2, 0, 0, 4, true, false},   {1, 0, 0, 1, false, false},
                     {0, 0, 0, 3, true, false},   {-1, 0, 0, 0, true, false},
                     {-2, 0, 0, 2, false, false}, {-3, 0, 0, 4, true, false},
                     {3, 0, 0, 1, true, false},   {2, 0, 0, 3, false, false},
                     {1, 0, 0, 0, true, false},   {0, 0, 0, 2, true, false},
                     {-1, 0, 0, 4, false, false}, {-2, 0, 0, 1, true, false},
                     {-3, 0, 0, 3, true, false},  {3, 0, 0, 0, false, false},
                     {2, 0, 0, 2, true, false},   {1, 0, 0, 4, true, false},
                     {0, 0, 0, 1, false, false},  {-1, 0, 0, 3, true, false},
                     {-2, 0, 0, 0, true, false},  {-3, 0, 0, 2, false, false},
                     {3, 0, 0, 4, true, false},   {2, 0, 0, 1, true, false},
                     {1, 0, 0, 3, false, false},  {0, 0, 0, 0, true, false},
                     {-1, 0, 0, 2, true, false},  {-2, 0, 0, 4, false, false},
                     {-3, 0, 0, 1, true, false},  {3, 0, 0, 3, true, false},
                     {2, 0, 0, 0, false, false},
                 },
             .queue = {1,  27, 18, 26, 31, 24, 10, 12, 22, 17, 20, 9,  21, 8,  2, 15,
                       29, 25, 23, 6,  30, 4,  11, 16, 14, 5,  7,  19, 3,  28, 13}},
            {.name = "mixed_runs_32",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 32,
             .queue_count = 32,
             .actors =
                 {
                     {-3, 0, 0, 0, false, false}, {3, 0, 0, 2, true, false},
                     {2, 0, 0, 4, true, false},   {1, 0, 0, 1, false, false},
                     {0, 0, 0, 3, true, false},   {-1, 0, 0, 0, true, false},
                     {-2, 0, 0, 2, false, false}, {-3, 0, 0, 4, true, false},
                     {3, 0, 0, 1, true, false},   {2, 0, 0, 3, false, false},
                     {1, 0, 0, 0, true, false},   {0, 0, 0, 2, true, false},
                     {-1, 0, 0, 4, false, false}, {-2, 0, 0, 1, true, false},
                     {-3, 0, 0, 3, true, false},  {3, 0, 0, 0, false, false},
                     {2, 0, 0, 2, true, false},   {1, 0, 0, 4, true, false},
                     {0, 0, 0, 1, false, false},  {-1, 0, 0, 3, true, false},
                     {-2, 0, 0, 0, true, false},  {-3, 0, 0, 2, false, false},
                     {3, 0, 0, 4, true, false},   {2, 0, 0, 1, true, false},
                     {1, 0, 0, 3, false, false},  {0, 0, 0, 0, true, false},
                     {-1, 0, 0, 2, true, false},  {-2, 0, 0, 4, false, false},
                     {-3, 0, 0, 1, true, false},  {3, 0, 0, 3, true, false},
                     {2, 0, 0, 0, false, false},  {1, 0, 0, 2, true, false},
                 },
             .queue = {21, 19, 13, 3, 10, 25, 14, 2, 27, 26, 22, 32, 9, 1,  12, 11,
                       20, 5,  30, 7, 29, 6,  17, 8, 23, 15, 31, 18, 4, 24, 16, 28}},
            {.name = "mixed_runs_33",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 33,
             .queue_count = 33,
             .actors =
                 {
                     {-3, 0, 0, 0, false, false}, {3, 0, 0, 2, true, false},
                     {2, 0, 0, 4, true, false},   {1, 0, 0, 1, false, false},
                     {0, 0, 0, 3, true, false},   {-1, 0, 0, 0, true, false},
                     {-2, 0, 0, 2, false, false}, {-3, 0, 0, 4, true, false},
                     {3, 0, 0, 1, true, false},   {2, 0, 0, 3, false, false},
                     {1, 0, 0, 0, true, false},   {0, 0, 0, 2, true, false},
                     {-1, 0, 0, 4, false, false}, {-2, 0, 0, 1, true, false},
                     {-3, 0, 0, 3, true, false},  {3, 0, 0, 0, false, false},
                     {2, 0, 0, 2, true, false},   {1, 0, 0, 4, true, false},
                     {0, 0, 0, 1, false, false},  {-1, 0, 0, 3, true, false},
                     {-2, 0, 0, 0, true, false},  {-3, 0, 0, 2, false, false},
                     {3, 0, 0, 4, true, false},   {2, 0, 0, 1, true, false},
                     {1, 0, 0, 3, false, false},  {0, 0, 0, 0, true, false},
                     {-1, 0, 0, 2, true, false},  {-2, 0, 0, 4, false, false},
                     {-3, 0, 0, 1, true, false},  {3, 0, 0, 3, true, false},
                     {2, 0, 0, 0, false, false},  {1, 0, 0, 2, true, false},
                     {0, 0, 0, 4, true, false},
                 },
             .queue = {4, 2,  22, 3,  31, 30, 33, 13, 16, 15, 12, 14, 18, 23, 19, 25, 20,
                       7, 21, 28, 17, 9,  11, 32, 26, 29, 5,  27, 8,  24, 10, 6,  1}},
            {.name = "mixed_runs_64",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 64,
             .queue_count = 64,
             .actors =
                 {
                     {-3, 0, 0, 0, false, false}, {3, 0, 0, 2, true, false},
                     {2, 0, 0, 4, true, false},   {1, 0, 0, 1, false, false},
                     {0, 0, 0, 3, true, false},   {-1, 0, 0, 0, true, false},
                     {-2, 0, 0, 2, false, false}, {-3, 0, 0, 4, true, false},
                     {3, 0, 0, 1, true, false},   {2, 0, 0, 3, false, false},
                     {1, 0, 0, 0, true, false},   {0, 0, 0, 2, true, false},
                     {-1, 0, 0, 4, false, false}, {-2, 0, 0, 1, true, false},
                     {-3, 0, 0, 3, true, false},  {3, 0, 0, 0, false, false},
                     {2, 0, 0, 2, true, false},   {1, 0, 0, 4, true, false},
                     {0, 0, 0, 1, false, false},  {-1, 0, 0, 3, true, false},
                     {-2, 0, 0, 0, true, false},  {-3, 0, 0, 2, false, false},
                     {3, 0, 0, 4, true, false},   {2, 0, 0, 1, true, false},
                     {1, 0, 0, 3, false, false},  {0, 0, 0, 0, true, false},
                     {-1, 0, 0, 2, true, false},  {-2, 0, 0, 4, false, false},
                     {-3, 0, 0, 1, true, false},  {3, 0, 0, 3, true, false},
                     {2, 0, 0, 0, false, false},  {1, 0, 0, 2, true, false},
                     {0, 0, 0, 4, true, false},   {-1, 0, 0, 1, false, false},
                     {-2, 0, 0, 3, true, false},  {-3, 0, 0, 0, true, false},
                     {3, 0, 0, 2, false, false},  {2, 0, 0, 4, true, false},
                     {1, 0, 0, 1, true, false},   {0, 0, 0, 3, false, false},
                     {-1, 0, 0, 0, true, false},  {-2, 0, 0, 2, true, false},
                     {-3, 0, 0, 4, false, false}, {3, 0, 0, 1, true, false},
                     {2, 0, 0, 3, true, false},   {1, 0, 0, 0, false, false},
                     {0, 0, 0, 2, true, false},   {-1, 0, 0, 4, true, false},
                     {-2, 0, 0, 1, false, false}, {-3, 0, 0, 3, true, false},
                     {3, 0, 0, 0, true, false},   {2, 0, 0, 2, false, false},
                     {1, 0, 0, 4, true, false},   {0, 0, 0, 1, true, false},
                     {-1, 0, 0, 3, false, false}, {-2, 0, 0, 0, true, false},
                     {-3, 0, 0, 2, true, false},  {3, 0, 0, 4, false, false},
                     {2, 0, 0, 1, true, false},   {1, 0, 0, 3, true, false},
                     {0, 0, 0, 0, false, false},  {-1, 0, 0, 2, true, false},
                     {-2, 0, 0, 4, true, false},  {-3, 0, 0, 1, false, false},
                 },
             .queue = {19, 22, 34, 50, 38, 61, 63, 30, 3,  21, 7,  62, 2,  10, 37, 40,
                       13, 18, 55, 25, 23, 20, 29, 56, 15, 14, 36, 59, 44, 9,  5,  26,
                       53, 48, 57, 60, 41, 54, 8,  17, 32, 52, 24, 49, 6,  39, 31, 1,
                       46, 35, 28, 43, 42, 47, 33, 12, 45, 4,  27, 51, 16, 11, 58, 64}},
            {.name = "mixed_runs_65",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 65,
             .queue_count = 65,
             .actors =
                 {
                     {-3, 0, 0, 0, false, false}, {3, 0, 0, 2, true, false},
                     {2, 0, 0, 4, true, false},   {1, 0, 0, 1, false, false},
                     {0, 0, 0, 3, true, false},   {-1, 0, 0, 0, true, false},
                     {-2, 0, 0, 2, false, false}, {-3, 0, 0, 4, true, false},
                     {3, 0, 0, 1, true, false},   {2, 0, 0, 3, false, false},
                     {1, 0, 0, 0, true, false},   {0, 0, 0, 2, true, false},
                     {-1, 0, 0, 4, false, false}, {-2, 0, 0, 1, true, false},
                     {-3, 0, 0, 3, true, false},  {3, 0, 0, 0, false, false},
                     {2, 0, 0, 2, true, false},   {1, 0, 0, 4, true, false},
                     {0, 0, 0, 1, false, false},  {-1, 0, 0, 3, true, false},
                     {-2, 0, 0, 0, true, false},  {-3, 0, 0, 2, false, false},
                     {3, 0, 0, 4, true, false},   {2, 0, 0, 1, true, false},
                     {1, 0, 0, 3, false, false},  {0, 0, 0, 0, true, false},
                     {-1, 0, 0, 2, true, false},  {-2, 0, 0, 4, false, false},
                     {-3, 0, 0, 1, true, false},  {3, 0, 0, 3, true, false},
                     {2, 0, 0, 0, false, false},  {1, 0, 0, 2, true, false},
                     {0, 0, 0, 4, true, false},   {-1, 0, 0, 1, false, false},
                     {-2, 0, 0, 3, true, false},  {-3, 0, 0, 0, true, false},
                     {3, 0, 0, 2, false, false},  {2, 0, 0, 4, true, false},
                     {1, 0, 0, 1, true, false},   {0, 0, 0, 3, false, false},
                     {-1, 0, 0, 0, true, false},  {-2, 0, 0, 2, true, false},
                     {-3, 0, 0, 4, false, false}, {3, 0, 0, 1, true, false},
                     {2, 0, 0, 3, true, false},   {1, 0, 0, 0, false, false},
                     {0, 0, 0, 2, true, false},   {-1, 0, 0, 4, true, false},
                     {-2, 0, 0, 1, false, false}, {-3, 0, 0, 3, true, false},
                     {3, 0, 0, 0, true, false},   {2, 0, 0, 2, false, false},
                     {1, 0, 0, 4, true, false},   {0, 0, 0, 1, true, false},
                     {-1, 0, 0, 3, false, false}, {-2, 0, 0, 0, true, false},
                     {-3, 0, 0, 2, true, false},  {3, 0, 0, 4, false, false},
                     {2, 0, 0, 1, true, false},   {1, 0, 0, 3, true, false},
                     {0, 0, 0, 0, false, false},  {-1, 0, 0, 2, true, false},
                     {-2, 0, 0, 4, true, false},  {-3, 0, 0, 1, false, false},
                     {3, 0, 0, 3, true, false},
                 },
             .queue = {15, 33, 25, 40, 43, 46, 26, 24, 39, 60, 54, 65, 32, 14, 30, 13, 9,
                       53, 29, 31, 4,  64, 57, 49, 23, 34, 61, 63, 22, 18, 21, 62, 1,  50,
                       38, 7,  42, 16, 58, 17, 45, 44, 2,  27, 19, 20, 55, 59, 3,  51, 37,
                       36, 35, 8,  11, 56, 52, 10, 6,  5,  41, 47, 28, 12, 48}},
            {.name = "mixed_runs_128",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 128,
             .queue_count = 128,
             .actors =
                 {
                     {-3, 0, 0, 0, false, false}, {3, 0, 0, 2, true, false},
                     {2, 0, 0, 4, true, false},   {1, 0, 0, 1, false, false},
                     {0, 0, 0, 3, true, false},   {-1, 0, 0, 0, true, false},
                     {-2, 0, 0, 2, false, false}, {-3, 0, 0, 4, true, false},
                     {3, 0, 0, 1, true, false},   {2, 0, 0, 3, false, false},
                     {1, 0, 0, 0, true, false},   {0, 0, 0, 2, true, false},
                     {-1, 0, 0, 4, false, false}, {-2, 0, 0, 1, true, false},
                     {-3, 0, 0, 3, true, false},  {3, 0, 0, 0, false, false},
                     {2, 0, 0, 2, true, false},   {1, 0, 0, 4, true, false},
                     {0, 0, 0, 1, false, false},  {-1, 0, 0, 3, true, false},
                     {-2, 0, 0, 0, true, false},  {-3, 0, 0, 2, false, false},
                     {3, 0, 0, 4, true, false},   {2, 0, 0, 1, true, false},
                     {1, 0, 0, 3, false, false},  {0, 0, 0, 0, true, false},
                     {-1, 0, 0, 2, true, false},  {-2, 0, 0, 4, false, false},
                     {-3, 0, 0, 1, true, false},  {3, 0, 0, 3, true, false},
                     {2, 0, 0, 0, false, false},  {1, 0, 0, 2, true, false},
                     {0, 0, 0, 4, true, false},   {-1, 0, 0, 1, false, false},
                     {-2, 0, 0, 3, true, false},  {-3, 0, 0, 0, true, false},
                     {3, 0, 0, 2, false, false},  {2, 0, 0, 4, true, false},
                     {1, 0, 0, 1, true, false},   {0, 0, 0, 3, false, false},
                     {-1, 0, 0, 0, true, false},  {-2, 0, 0, 2, true, false},
                     {-3, 0, 0, 4, false, false}, {3, 0, 0, 1, true, false},
                     {2, 0, 0, 3, true, false},   {1, 0, 0, 0, false, false},
                     {0, 0, 0, 2, true, false},   {-1, 0, 0, 4, true, false},
                     {-2, 0, 0, 1, false, false}, {-3, 0, 0, 3, true, false},
                     {3, 0, 0, 0, true, false},   {2, 0, 0, 2, false, false},
                     {1, 0, 0, 4, true, false},   {0, 0, 0, 1, true, false},
                     {-1, 0, 0, 3, false, false}, {-2, 0, 0, 0, true, false},
                     {-3, 0, 0, 2, true, false},  {3, 0, 0, 4, false, false},
                     {2, 0, 0, 1, true, false},   {1, 0, 0, 3, true, false},
                     {0, 0, 0, 0, false, false},  {-1, 0, 0, 2, true, false},
                     {-2, 0, 0, 4, true, false},  {-3, 0, 0, 1, false, false},
                     {3, 0, 0, 3, true, false},   {2, 0, 0, 0, true, false},
                     {1, 0, 0, 2, false, false},  {0, 0, 0, 4, true, false},
                     {-1, 0, 0, 1, true, false},  {-2, 0, 0, 3, false, false},
                     {-3, 0, 0, 0, true, false},  {3, 0, 0, 2, true, false},
                     {2, 0, 0, 4, false, false},  {1, 0, 0, 1, true, false},
                     {0, 0, 0, 3, true, false},   {-1, 0, 0, 0, false, false},
                     {-2, 0, 0, 2, true, false},  {-3, 0, 0, 4, true, false},
                     {3, 0, 0, 1, false, false},  {2, 0, 0, 3, true, false},
                     {1, 0, 0, 0, true, false},   {0, 0, 0, 2, false, false},
                     {-1, 0, 0, 4, true, false},  {-2, 0, 0, 1, true, false},
                     {-3, 0, 0, 3, false, false}, {3, 0, 0, 0, true, false},
                     {2, 0, 0, 2, true, false},   {1, 0, 0, 4, false, false},
                     {0, 0, 0, 1, true, false},   {-1, 0, 0, 3, true, false},
                     {-2, 0, 0, 0, false, false}, {-3, 0, 0, 2, true, false},
                     {3, 0, 0, 4, true, false},   {2, 0, 0, 1, false, false},
                     {1, 0, 0, 3, true, false},   {0, 0, 0, 0, true, false},
                     {-1, 0, 0, 2, false, false}, {-2, 0, 0, 4, true, false},
                     {-3, 0, 0, 1, true, false},  {3, 0, 0, 3, false, false},
                     {2, 0, 0, 0, true, false},   {1, 0, 0, 2, true, false},
                     {0, 0, 0, 4, false, false},  {-1, 0, 0, 1, true, false},
                     {-2, 0, 0, 3, true, false},  {-3, 0, 0, 0, false, false},
                     {3, 0, 0, 2, true, false},   {2, 0, 0, 4, true, false},
                     {1, 0, 0, 1, false, false},  {0, 0, 0, 3, true, false},
                     {-1, 0, 0, 0, true, false},  {-2, 0, 0, 2, false, false},
                     {-3, 0, 0, 4, true, false},  {3, 0, 0, 1, true, false},
                     {2, 0, 0, 3, false, false},  {1, 0, 0, 0, true, false},
                     {0, 0, 0, 2, true, false},   {-1, 0, 0, 4, false, false},
                     {-2, 0, 0, 1, true, false},  {-3, 0, 0, 3, true, false},
                     {3, 0, 0, 0, false, false},  {2, 0, 0, 2, true, false},
                     {1, 0, 0, 4, true, false},   {0, 0, 0, 1, false, false},
                     {-1, 0, 0, 3, true, false},  {-2, 0, 0, 0, true, false},
                     {-3, 0, 0, 2, false, false}, {3, 0, 0, 4, true, false},
                 },
             .queue = {85,  53,  93,  107, 57,  24,  102, 111, 114, 76,  35, 9,   125, 36,  109,
                       112, 43,  95,  79,  120, 47,  48,  115, 121, 28,  62, 69,  11,  46,  124,
                       100, 63,  75,  55,  2,   51,  113, 19,  54,  108, 15, 39,  8,   7,   30,
                       98,  116, 87,  26,  52,  38,  83,  42,  60,  27,  34, 18,  66,  101, 20,
                       65,  6,   49,  56,  123, 68,  1,   14,  74,  88,  89, 126, 12,  50,  22,
                       45,  90,  104, 97,  94,  4,   78,  77,  73,  21,  44, 25,  99,  110, 10,
                       92,  3,   122, 118, 29,  61,  41,  72,  37,  23,  96, 119, 82,  5,   40,
                       106, 127, 91,  71,  58,  105, 13,  17,  67,  32,  16, 33,  80,  86,  31,
                       84,  59,  70,  64,  117, 128, 81,  103}},
            {.name = "contract_violation_85",
             .visible_only = false,
             .null_camera = false,
             .null_list = false,
             .camera = {0.0, 0.0, 0.0},
             .actor_count = 85,
             .queue_count = 85,
             .actors =
                 {
                     {2, 0, 0, 2, true, false},  {4, 0, 0, 2, true, false},
                     {1, 0, 0, 0, true, false},  {3, 0, 0, 0, false, false},
                     {0, 0, 0, 2, false, false}, {1, 0, 0, 1, false, false},
                     {0, 0, 0, 0, false, false}, {3, 0, 0, 1, true, false},
                     {2, 0, 0, 2, true, false},  {1, 0, 0, 0, true, false},
                     {3, 0, 0, 0, false, false}, {0, 0, 0, 3, false, false},
                     {2, 0, 0, 3, true, false},  {4, 0, 0, 3, false, false},
                     {3, 0, 0, 3, false, false}, {0, 0, 0, 3, false, false},
                     {0, 0, 0, 1, false, false}, {0, 0, 0, 0, false, false},
                     {1, 0, 0, 1, false, false}, {2, 0, 0, 1, true, false},
                     {2, 0, 0, 3, true, false},  {1, 0, 0, 3, true, false},
                     {0, 0, 0, 2, false, false}, {4, 0, 0, 3, true, false},
                     {4, 0, 0, 3, false, false}, {2, 0, 0, 2, false, false},
                     {2, 0, 0, 2, false, false}, {0, 0, 0, 2, true, false},
                     {4, 0, 0, 2, false, false}, {2, 0, 0, 2, true, false},
                     {1, 0, 0, 3, false, false}, {3, 0, 0, 1, true, false},
                     {3, 0, 0, 3, false, false}, {0, 0, 0, 0, false, false},
                     {1, 0, 0, 1, true, false},  {1, 0, 0, 3, false, false},
                     {0, 0, 0, 0, false, false}, {2, 0, 0, 2, false, false},
                     {0, 0, 0, 2, true, false},  {0, 0, 0, 0, true, false},
                     {0, 0, 0, 3, true, false},  {2, 0, 0, 1, false, false},
                     {0, 0, 0, 2, true, false},  {1, 0, 0, 3, false, false},
                     {4, 0, 0, 2, true, false},  {2, 0, 0, 0, false, false},
                     {1, 0, 0, 0, true, false},  {0, 0, 0, 0, true, false},
                     {4, 0, 0, 2, true, false},  {0, 0, 0, 3, false, false},
                     {2, 0, 0, 1, true, false},  {2, 0, 0, 3, true, false},
                     {2, 0, 0, 0, true, false},  {2, 0, 0, 0, true, false},
                     {3, 0, 0, 3, false, false}, {1, 0, 0, 1, false, false},
                     {3, 0, 0, 1, false, false}, {3, 0, 0, 2, true, false},
                     {3, 0, 0, 3, true, false},  {0, 0, 0, 0, false, false},
                     {2, 0, 0, 2, true, false},  {0, 0, 0, 1, true, false},
                     {0, 0, 0, 1, false, false}, {2, 0, 0, 2, false, false},
                     {1, 0, 0, 0, true, false},  {0, 0, 0, 2, false, false},
                     {3, 0, 0, 3, true, false},  {2, 0, 0, 0, true, false},
                     {2, 0, 0, 2, true, false},  {3, 0, 0, 2, false, false},
                     {1, 0, 0, 3, true, false},  {2, 0, 0, 0, false, false},
                     {3, 0, 0, 2, true, false},  {4, 0, 0, 1, false, false},
                     {3, 0, 0, 0, true, false},  {1, 0, 0, 3, false, false},
                     {0, 0, 0, 3, true, false},  {4, 0, 0, 1, true, false},
                     {3, 0, 0, 2, false, false}, {1, 0, 0, 2, true, false},
                     {2, 0, 0, 3, true, false},  {3, 0, 0, 1, false, false},
                     {0, 0, 0, 3, false, false}, {1, 0, 0, 3, true, false},
                     {3, 0, 0, 0, true, false},
                 },
             .queue = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
                       18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
                       35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
                       52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68,
                       69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85}},
};

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_math_classes *math;
    mc_world_runtime_classes *world_classes;
    mc_client_render_engine_classes *graphics;
    client_application *app;
    mc_client_render_global_error *error;
} fixture;

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "N update check%u at%d: %s\n", checks, __LINE__, #value);              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static size_t actor_id(mc_client_source_dk *const *actors, size_t count,
                       const mc_client_source_dk *dk) {
    if (dk == NULL)
        return 0;
    for (size_t i = 0; i < count; ++i)
        if (actors[i] == dk)
            return i + 1;
    return SIZE_MAX;
}

static size_t actor_value(const mc_client_source_dk *dk, unsigned int field) {
    switch (field) {
        case 0:
            return dk->fields.u;
        case 1:
            return (dk->fields.p[0] ? 1u : 0u) | (dk->fields.p[1] ? 2u : 0u);
        case 2:
            return dk->fields.E;
        case 3:
            return dk->fields.A;
        default:
            return dk->fields.B->count;
    }
}

static bool observe(size_t index, const update_input *input, mc_client_render_global *n,
                    mc_client_source_dk *const *actors,
                    const mc_client_render_global_update_state *state, int32_t counter, bool light,
                    const char *exception) {
    for (size_t i = 0; n->fields.m != NULL && i < n->fields.m->count; ++i)
        CHECK(actor_id(actors, input->actor_count, n->fields.m->entries[i]) != SIZE_MAX);
    CHECK(state->completed_compiles <= state->compile_attempts);
    for (size_t i = 0; i < state->compile_attempts; ++i)
        CHECK(actor_id(actors, input->actor_count, state->compile_order[i]) != SIZE_MAX);
    printf("UPDATE %zu %s %d %s ", index, input->name,
           state->returned ? (state->result ? 1 : 0) : -1, exception);
    if (n->fields.m == NULL)
        printf("N");
    else if (n->fields.m->count == 0)
        printf("-");
    else
        for (size_t i = 0; i < n->fields.m->count; ++i)
            printf("%s%zu", i == 0 ? "" : ",",
                   actor_id(actors, input->actor_count, n->fields.m->entries[i]));
    for (unsigned int field = 0; field < 5; ++field) {
        printf(" ");
        if (input->actor_count == 0)
            printf("-");
        else
            for (size_t i = 0; i < input->actor_count; ++i)
                printf("%s%zu", i == 0 ? "" : ",", actor_value(actors[i], field));
    }
    printf(" %" PRId32 " %u %u ", counter, light ? 1u : 0u, state->source_pc);
    if (state->compile_attempts == 0)
        printf("-");
    else
        for (size_t i = 0; i < state->compile_attempts; ++i)
            printf("%s%zu@%u", i == 0 ? "" : ",",
                   actor_id(actors, input->actor_count, state->compile_order[i]),
                   i < state->near_length ? 442u : 519u);
    printf(" ");
    if (state->completed_compiles == 0)
        printf("-");
    else
        for (size_t i = 0; i < state->completed_compiles; ++i)
            printf("%s%zu", i == 0 ? "" : ",",
                   actor_id(actors, input->actor_count, state->compile_order[i]));
    printf("\n");
    return true;
}

static bool one_case(fixture *f, size_t index) {
    const update_input *input = &inputs[index];
    mc_client_render_global *n = NULL;
    mc_client_render_global_update_call *call = NULL, *other = NULL;
    mc_client_source_dk *actors[MAX_ACTORS] = {0};
    mc_entity *camera = NULL;
    CHECK(mc_client_render_global_allocate(f->graphics, NULL, &n, f->error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    n->fields.t = f->app;
    const mc_client_graphics_site prepared = {"native-test", "prepare", "()V", -1};
    if (!input->null_list) {
        n->fields.m = mc_render_global_allocate(n, sizeof(*n->fields.m), prepared, f->error);
        CHECK(n->fields.m != NULL);
    }
    for (size_t i = 0; i < input->actor_count; ++i) {
        const actor_input *source = &input->actors[i];
        mc_client_source_dk *dk = actors[i] = mc_source_dk_allocate(n, f->error);
        CHECK(dk != NULL);
        dk->fields.q = source->q;
        dk->fields.r = source->r;
        dk->fields.s = source->s;
        dk->fields.w = source->w;
        dk->fields.o = source->visible;
        dk->fields.u = source->dirty;
        dk->fields.f = dk->fields.g = dk->fields.h = 16;
        dk->fields.p = mc_render_global_allocate(n, 2 * sizeof(bool), prepared, f->error);
        dk->fields.B = mc_render_global_allocate(n, sizeof(*dk->fields.B), prepared, f->error);
        CHECK(dk->fields.p != NULL && dk->fields.B != NULL);
        const unsigned int mask = (unsigned int)(i + 1) % 4u;
        dk->fields.p[0] = (mask & 1u) != 0;
        dk->fields.p[1] = (mask & 2u) != 0;
        dk->fields.E = (i + 1) % 2u != 0;
        dk->fields.A = true;
        CHECK(mc_render_global_pointer_append(n, dk->fields.B, NULL, prepared, f->error));
    }
    for (size_t i = 0; i < input->queue_count; ++i) {
        const unsigned int id = input->queue[i];
        CHECK(id <= input->actor_count && n->fields.m != NULL);
        CHECK(mc_render_global_pointer_append(n, n->fields.m, id == 0 ? NULL : actors[id - 1],
                                              prepared, f->error));
    }
    if (!input->null_camera) {
        CHECK(mc_local_player_allocate(f->entities, &camera, NULL) == MC_ENTITY_OK);
        camera->base.x = input->camera[0];
        camera->base.y = input->camera[1];
        camera->base.z = input->camera[2];
    }
    mc_client_graphics_error graphics;
    const mc_client_graphics_site statistic = {"dk", "a", "()V", 8};
    CHECK(mc_client_chunk_statistics_reset(f->graphics, statistic, &graphics));
    f->world_classes->chunk_a = true;
    mc_client_render_list *const queue = n->fields.m;
    const mc_client_render_global_status status =
        mc_client_render_global_update_begin(n, camera, input->visible_only, &call, f->error);
    mc_client_render_global_update_state state;
    CHECK(call != NULL && mc_client_render_global_update_inspect(call, &state) && !state.active &&
          !n->active && n->update_call == call && n->holds == 2 && n->fields.m == queue &&
          state.owner == n && state.camera == camera && state.visible_only == input->visible_only &&
          (camera == NULL || camera->holds == 2) && f->graphics->active_source_calls == 0 &&
          f->graphics->active_resource_calls == 0);
    const char *exception = "NONE";
    if (!state.returned) {
        CHECK(state.failure != NULL && state.failure->status == status);
        if (status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE) {
            CHECK(!state.failure->native_policy);
            exception = "NPE";
        } else {
            CHECK(status == MC_CLIENT_RENDER_GLOBAL_SOURCE_ILLEGAL_ARGUMENT &&
                  !state.failure->native_policy && state.source_pc == 411);
            exception = "IAE";
        }
    } else
        CHECK(status == MC_CLIENT_RENDER_GLOBAL_OK && state.source_pc == 697);
    int32_t counter;
    CHECK(mc_client_chunk_statistics_get(f->graphics, statistic, &counter, &graphics));
    CHECK(observe(index, input, n, actors, &state, counter, f->world_classes->chunk_a, exception));
    if (state.compilation != NULL && !state.compilation->returned) {
        CHECK(state.compilation->source_pc == 159 && state.compilation->cache != NULL &&
              state.compilation->cache_failure != NULL &&
              state.compilation->cache_failure->bytecode == 112 &&
              state.compilation->cache_failure->status == MC_CLIENT_CHUNK_CACHE_SOURCE_NULL &&
              !state.compilation->cache_failure->native_policy &&
              state.compilation->chunk->fields.u && counter == 1 && !f->world_classes->chunk_a);
    }
    CHECK(mc_client_render_global_update_begin(n, camera, false, &call, f->error) ==
              MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT &&
          n->update_call == call && n->holds == 2);
    CHECK(mc_client_render_global_update_begin(n, camera, false, &other, f->error) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          other == NULL && n->update_call == call && n->holds == 2);
    CHECK(mc_client_render_global_retire(&n, f->error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          n != NULL);
    size_t saved[MAX_ACTORS][5];
    for (size_t i = 0; i < input->actor_count; ++i)
        for (unsigned int field = 0; field < 5; ++field)
            saved[i][field] = actor_value(actors[i], field);
    void *saved_queue[MAX_QUEUE];
    const size_t saved_count = queue == NULL ? 0 : queue->count;
    CHECK(saved_count <= MAX_QUEUE);
    for (size_t i = 0; i < saved_count; ++i)
        saved_queue[i] = queue->entries[i];
    CHECK(mc_client_render_global_update_retire(&call, f->error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          call == NULL && n->holds == 1 && n->update_call == NULL && !n->active &&
          n->fields.m == queue && (camera == NULL || camera->holds == 1));
    CHECK(queue == NULL || queue->count == saved_count);
    for (size_t i = 0; i < saved_count; ++i)
        CHECK(queue->entries[i] == saved_queue[i]);
    for (size_t i = 0; i < input->actor_count; ++i)
        for (unsigned int field = 0; field < 5; ++field)
            CHECK(actor_value(actors[i], field) == saved[i][field]);
    int32_t after;
    CHECK(mc_client_chunk_statistics_get(f->graphics, statistic, &after, &graphics) &&
          after == counter);
    CHECK(mc_client_render_global_retire(&n, f->error) == MC_CLIENT_RENDER_GLOBAL_OK && n == NULL);
    mc_entity_release(camera);
    return true;
}

static bool run(void) {
    fixture f = {0};
    f.app = calloc(1, sizeof(*f.app));
    f.error = calloc(1, sizeof(*f.error));
    CHECK(f.app != NULL && f.error != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f.profile, NULL, &f.entities) == MC_ENTITY_OK);
    CHECK(mc_math_classes_create(SIZE_MAX, &f.math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(f.math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f.profile,
                                          mc_math_classes_table(f.math), &f.world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &f.graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    f.app->entities = f.entities;
    f.app->world_classes = f.world_classes;
    for (size_t i = 0; i < sizeof(inputs) / sizeof(*inputs); ++i)
        CHECK(one_case(&f, i));
    CHECK(mc_client_render_engine_classes_release(f.graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_entity_classes_free(f.entities) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_free(f.world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&f.math, NULL) == MC_MATH_CLASS_OK);
    mc_java_random_profile_free(f.profile);
    free(f.error);
    free(f.app);
    return true;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u || !run())
        return 1;
    printf("%u N update checks passed\n", checks);
    return 0;
}
