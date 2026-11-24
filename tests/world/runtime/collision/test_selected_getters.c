/* Reuse genuine GAME Registry/World/Store/Chunk constructors and teardown.
 * This translation unit is compiled alone; neither imported main is run. */
#define MC_COLLISION_BOXES_MAIN mc_selected_getters_unused_boxes_main
#include "test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include <emmintrin.h>
#include <inttypes.h>

typedef struct selected_case {
    const char *name;
    int32_t id, metadata, x, y, z;
    bool world, collision, expect_success, bad_fp, output_guard, custom_prototype;
} selected_case;

/* Declared finite authored inputs, not observations or expected native results. */
#include "selected_getter_cases.inc"

static void selected_bits(const double *values) {
    putchar('[');
    for (size_t i = 0; i < 6; ++i) {
        uint64_t bits;
        memcpy(&bits, &values[i], sizeof(bits));
        printf("%s\"%016" PRIx64 "\"", i ? "," : "", bits);
    }
    putchar(']');
}

static void selected_fields(const mc_block *block, double out[6]) {
    if (block == NULL) {
        memset(out, 0, 6 * sizeof(*out));
        return;
    }
    const mc_block_fields *f = mc_block_fields_get(block);
    const double values[] = {f->bs, f->bt, f->bu, f->bv, f->bw, f->bx};
    memcpy(out, values, sizeof(values));
}

static bool selected_one(fixture *f, const selected_case *input, mc_entity_bounds *marker) {
    bool ok = false;
    mc_entity_classes *const entities = f->entity_classes[0];
    mc_world_runtime_classes *const classes = f->classes[0];
    mc_block *const block = input->id == 0 ? NULL : f->registry[0]->blocks.m[input->id];
    mc_entity_bounds sentinel = {-9, -8, -7, -6, -5, -4};
    mc_entity_bounds *out = NULL;
    mc_world_selected_box_error selected = {0};
    mc_world_collision_error collision = mc_world_collision_empty_error();
    double before[6], after[6], original_prototype[6] = {0}, prototype_after[6] = {0};
    mc_block *prototype = NULL;
    const unsigned int original_fp = _mm_getcsr();
    bool prototype_changed = false;
    CHECK(block != NULL || input->id == 0);
    if (input->world && input->y >= 0 && input->y < 128) {
        /* Declared fixture cell input in an actual loaded Chunk. Metadata uses
         * the genuine quiet World setter; no getter callback/backend is used. */
        CHECK(input->x == 3 && input->z == 5);
        f->states[0]->data->blocks[boxes_index(3, input->y, 5)] = (uint8_t)input->id;
        bool changed = false;
        CHECK(mc_world_runtime_metadata_set_quiet(f->worlds[0], 3, input->y, 5, input->metadata,
                                                  &changed, NULL) == MC_WORLD_STORAGE_OK &&
              changed);
        int32_t observed = -1;
        CHECK(mc_world_runtime_metadata_get(f->worlds[0], 3, input->y, 5, &observed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              observed == input->metadata);
    }
    if (block != NULL && block->kind == MC_BLOCK_STAIRS) {
        prototype = block->subtype.ss.a; /* Passive actual constructor field. */
        CHECK(prototype != NULL && prototype->classes == f->registry[0]);
        selected_fields(prototype, original_prototype);
        if (input->custom_prototype) {
            /* Genuine source UU.a(FFFFFF), never a prototype-pointer write. */
            mc_registry_block_bounds(prototype, .125f, .25f, .5f, .75f, .875f, 1.25f);
            prototype_changed = true;
            mc_world_collision_error shape_error = mc_world_collision_empty_error();
            CHECK(mc_world_collision_block_state_shape_in_classes_held(
                      classes, NULL, block, input->x, input->y, input->z, &shape_error) ==
                  MC_WORLD_STORAGE_OK);
        }
    }
    selected_fields(block, before);
    CHECK(mc_entity_bounds_pool_reset(entities, NULL) == MC_ENTITY_OK);
    const int32_t cursor_before = entities->bounds_cursor;
    const size_t holds_before = entities->bounds_holds;
    if (input->output_guard)
        out = &sentinel;
    if (input->bad_fp)
        _mm_setcsr((original_fp & ~0x6000u) | 0x2000u); /* Native FP refusal control. */
    const mc_world_storage_status status =
        input->collision ? mc_world_collision_block_box_in_classes_held(
                               classes, input->world ? f->worlds[0] : NULL, block, input->x,
                               input->y, input->z, &out, &collision)
                         : mc_world_collision_block_selected_box_in_classes_held(
                               classes, input->world ? f->worlds[0] : NULL, block, input->x,
                               input->y, input->z, &out, &selected);
    _mm_setcsr(original_fp); /* Restore before output checks, branches or teardown. */
    selected_fields(block, after);
    selected_fields(prototype, prototype_after);
    if (prototype_changed) {
        mc_registry_block_bounds(prototype, (float)original_prototype[0],
                                 (float)original_prototype[1], (float)original_prototype[2],
                                 (float)original_prototype[3], (float)original_prototype[4],
                                 (float)original_prototype[5]);
        prototype_changed = false;
    }
    const int32_t cursor_after = entities->bounds_cursor;
    const size_t holds_after = entities->bounds_holds;
    const bool source_success = status == MC_WORLD_STORAGE_OK;
    const bool output_preserved = out == (input->output_guard ? &sentinel : NULL);
    const bool same_marker = out == marker;
    printf("SELECTED {\"name\":\"%s\",\"status\":%d,\"success\":%s,"
           "\"source_null\":%s,\"native_policy\":%s,\"returned\":%s,"
           "\"out_null\":%s,\"output_preserved\":%s,\"same_marker\":%s,"
           "\"cursor\":[%d,%d],\"holds\":[%zu,%zu],\"bounds\":",
           input->name, (int)status, source_success ? "true" : "false",
           selected.source_null ? "true" : "false", selected.native_policy ? "true" : "false",
           selected.returned ? "true" : "false", out == NULL ? "true" : "false",
           output_preserved ? "true" : "false", same_marker ? "true" : "false", cursor_before,
           cursor_after, holds_before, holds_after);
    if (source_success && out != NULL) {
        double coordinates[6];
        _Static_assert(sizeof(*out) == sizeof(coordinates), "six binary64 bounds coordinates");
        memcpy(coordinates, out, sizeof(coordinates));
        selected_bits(coordinates);
    } else
        fputs("null", stdout);
    fputs(",\"fields_before\":", stdout);
    selected_bits(before);
    fputs(",\"fields_after\":", stdout);
    selected_bits(after);
    printf(",\"prototype_id\":%d,\"prototype_same_as_registered\":%s,\"prototype_fields\":",
           prototype == NULL ? -1 : prototype->fields.bn,
           prototype != NULL && prototype->fields.bn >= 1 && prototype->fields.bn <= 96 &&
                   f->registry[0]->blocks.m[prototype->fields.bn] == prototype
               ? "true"
               : "false");
    selected_bits(prototype_after);
    printf(",\"collision_child\":[\"%s\",%u],\"frames\":[",
           collision.block_class == NULL ? "" : collision.block_class, collision.block_bytecode);
    for (size_t i = 0; i < selected.frame_count; ++i) {
        const mc_world_selected_box_frame *frame = &selected.frames[i];
        printf("%s{\"class\":\"%s\",\"pc\":%u,\"receiver_id\":%d,\"reference_owner\":\"%s\","
               "\"bounds_store\":%u,\"metadata_returned\":%s,\"metadata\":%d,"
               "\"state_shape_returned\":%s,\"returned\":%s}",
               i ? "," : "", frame->declaring_class, frame->bytecode, frame->receiver->fields.bn,
               frame->reference_owner == NULL ? "" : frame->reference_owner, frame->bounds_store_pc,
               frame->metadata_returned ? "true" : "false", frame->metadata,
               frame->state_shape_returned ? "true" : "false", frame->returned ? "true" : "false");
    }
    printf("],\"selected_child\":[\"%s\",%u]}\n",
           selected.child.block_class == NULL ? "" : selected.child.block_class,
           selected.child.block_bytecode);
    if (out != NULL && out != &sentinel) {
        CHECK(mc_entity_bounds_release(entities, out) == MC_ENTITY_OK);
        out = NULL;
    }
    CHECK(source_success == input->expect_success && entities->bounds_holds == holds_before);
    CHECK(holds_before == 1 && cursor_before == 0 && cursor_after == (source_success ? 1 : 0));
    CHECK(source_success ? same_marker && holds_after == holds_before + 1
                         : output_preserved && holds_after == holds_before);
    ok = true;
cleanup:
    _mm_setcsr(original_fp);
    if (prototype_changed)
        mc_registry_block_bounds(prototype, (float)original_prototype[0],
                                 (float)original_prototype[1], (float)original_prototype[2],
                                 (float)original_prototype[3], (float)original_prototype[4],
                                 (float)original_prototype[5]);
    if (out != NULL && out != &sentinel)
        (void)mc_entity_bounds_release(entities, out);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_entity_bounds *marker = NULL;
    char root[8192];
    const uint16_t title[] = {'S', 'e', 'l', 'e', 'c', 't', 'e', 'd'};
    if (argc != 3 || strcmp(argv[1], "selected-getters") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          r->opens == 3 && r->closes == 1 && !r->bad);
    CHECK(boxes_open_store(f, 0, root) && boxes_create_world(f, 0));
    CHECK(mc_entity_bounds_pool_get(f->entity_classes[0], -9, -8, -7, -6, -5, -4, &marker, NULL) ==
              MC_ENTITY_OK &&
          marker != NULL);
    for (size_t i = 0; i < COUNT(selected_cases); ++i)
        CHECK(selected_one(f, &selected_cases[i], marker));
    CHECK(f->entity_classes[0]->bounds_holds == 1 &&
          mc_entity_bounds_native_holds(f->entity_classes[0], marker) == 1 &&
          mc_entity_bounds_release(f->entity_classes[0], marker) == MC_ENTITY_OK);
    marker = NULL;
    CHECK(f->entity_classes[0]->bounds_holds == 0);
    result = 0;
cleanup:
    if (f != NULL && marker != NULL)
        (void)mc_entity_bounds_release(f->entity_classes[0], marker);
    if (!cleanup_fixture(f)) {
        fputs("selected getter fixture cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0) {
        printf("SELECTED_CLEANUP {\"marker_release_ok\":true,\"fixture_cleanup_ok\":true,"
               "\"cases\":%zu,\"checks\":%u}\n",
               COUNT(selected_cases), checks);
        if (printf("PASS: %u selected getter checks (selected-getters)\n", checks) < 0)
            return 1;
    }
    return result;
}
