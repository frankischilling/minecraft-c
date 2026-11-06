#include "mc/block/registry/registry.h"
#include "mc/core/java/random_profile.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

#include "../../../../src/core/java/random_profile_internal.h"

#include <stdio.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static unsigned int checks;
static bool retirement_failed;
static mc_math_trig trig;

/* Untouched ow/jh.<clinit> Class/String pairs and the referenced class headers,
 * not instance-kind ordinal conversions. These controls execute no original
 * game code and claim no original/native runtime comparison. */
static const struct {
    mc_tile_entity_class_kind kind;
    const char *client, *server;
} expected[] = {{MC_TILE_ENTITY_CLASS_KIND_TILE_ENTITY, "ow", "jh"},
                {MC_TILE_ENTITY_CLASS_KIND_FURNACE, "sk", "ln"},
                {MC_TILE_ENTITY_CLASS_KIND_CHEST, "js", "gk"},
                {MC_TILE_ENTITY_CLASS_KIND_JUKEBOX, "eg", "cr"},
                {MC_TILE_ENTITY_CLASS_KIND_DISPENSER, "az", "ak"},
                {MC_TILE_ENTITY_CLASS_KIND_SIGN, "yk", "pq"},
                {MC_TILE_ENTITY_CLASS_KIND_SPAWNER, "cy", "bx"},
                {MC_TILE_ENTITY_CLASS_KIND_NOTE_BLOCK, "tn", "mf"},
                {MC_TILE_ENTITY_CLASS_KIND_PISTON, "uk", "mu"}};

_Static_assert(COUNT(expected) == MC_TILE_ENTITY_CLASS_KIND_COUNT,
               "base and eight actual TileEntity Class cells");

static bool cold(const mc_tile_entity_piston_class_snapshot *view, size_t borrows) {
    return view->base_phase == MC_TILE_ENTITY_CLASS_COLD &&
           view->piston_phase == MC_TILE_ENTITY_CLASS_COLD &&
           view->base_initializer_bytecode == 0 && view->piston_initializer_bytecode == 0 &&
           view->base_failure == MC_TILE_ENTITY_OK && view->piston_failure == MC_TILE_ENTITY_OK &&
           view->base_name_count == 0 && view->base_class_count == 0 &&
           view->list_identity == NULL && view->list_count == 0 && view->list_capacity == 0 &&
           view->allocation_bytes == 0 && view->live_tile_entities == 0 &&
           view->list_mod_count_bits == 0 && view->class_literal_borrows == borrows;
}

static bool run_side(mc_world_info_side side, mc_java_random_profile *profile,
                     mc_materials *materials) {
    bool ok = false;
    mc_world_runtime_classes *world_classes[3] = {0};
    mc_tile_entity_classes *classes[3] = {0}, *again = NULL;
    mc_registry_classes *registry[2] = {0};
    mc_tile_entity *instances[3] = {0}, *failed = NULL;
    const mc_tile_entity_class_literal *literals[MC_TILE_ENTITY_CLASS_KIND_COUNT] = {0};
    mc_tile_entity_piston_class_snapshot view;
    mc_world_runtime_class_state world_state;
    const uint64_t unique = atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
    const unsigned int holder = atomic_load_explicit(&profile->holder_phase, memory_order_relaxed);
    const bool admitted =
        atomic_load_explicit(&profile->constructor_admitted, memory_order_relaxed);
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(profile);

    for (size_t i = 0; i < 2; ++i)
        CHECK(mc_world_runtime_classes_create(side, profile, &trig, &world_classes[i], NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_tile_entity_classes_borrow(world_classes[i], &classes[i]) == MC_TILE_ENTITY_OK);
    CHECK(classes[0] != classes[1] &&
          mc_tile_entity_classes_borrow(world_classes[0], &again) == MC_TILE_ENTITY_OK &&
          again == classes[0]);
    CHECK(mc_tile_entity_piston_class_get_snapshot(world_classes[0], &view) == MC_TILE_ENTITY_OK &&
          cold(&view, 2));

    for (size_t i = 0; i < COUNT(expected); ++i) {
        const mc_tile_entity_class_literal *repeat = NULL, *foreign = NULL, *parent = NULL;
        mc_tile_entity_class_literal_view literal_view;
        CHECK(mc_tile_entity_class_literal_get(classes[0], expected[i].kind, &literals[i]) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_class_literal_get(again, expected[i].kind, &repeat) ==
                  MC_TILE_ENTITY_OK &&
              repeat == literals[i] &&
              mc_tile_entity_class_literal_get(classes[1], expected[i].kind, &foreign) ==
                  MC_TILE_ENTITY_OK &&
              foreign != literals[i]);
        CHECK(mc_tile_entity_class_literal_inspect(literals[i], &literal_view) ==
                  MC_TILE_ENTITY_OK &&
              literal_view.owner == classes[0] && literal_view.world_classes == world_classes[0] &&
              literal_view.side == side && literal_view.kind == expected[i].kind &&
              strcmp(literal_view.client_name, expected[i].client) == 0 &&
              strcmp(literal_view.server_name, expected[i].server) == 0 &&
              strcmp(literal_view.superclass_name,
                     i == 0 ? "java.lang.Object"
                            : (side == MC_WORLD_INFO_CLIENT ? expected[0].client
                                                            : expected[0].server)) == 0);
        for (size_t j = 0; j < i; ++j)
            CHECK(literals[i] != literals[j]);
        if (i != 0)
            CHECK(mc_tile_entity_class_literal_superclass(literals[i], &parent) ==
                      MC_TILE_ENTITY_OK &&
                  parent == literals[0]);
    }

    const mc_tile_entity_class_literal *out = literals[2];
    CHECK(mc_tile_entity_class_literal_superclass(literals[0], &out) ==
              MC_TILE_ENTITY_INVALID_KIND &&
          out == literals[2]); /* OW/JH really extends Object, never a fabricated NULL. */
    CHECK(mc_tile_entity_class_literal_get(classes[0], (mc_tile_entity_class_kind)-1, &out) ==
              MC_TILE_ENTITY_INVALID_KIND &&
          out == literals[2]);
    CHECK(mc_tile_entity_class_literal_get(classes[0], MC_TILE_ENTITY_CLASS_KIND_COUNT, &out) ==
              MC_TILE_ENTITY_INVALID_KIND &&
          out == literals[2]);
    CHECK(mc_tile_entity_class_literal_get(NULL, MC_TILE_ENTITY_CLASS_KIND_CHEST, &out) ==
              MC_TILE_ENTITY_INVALID_ARGUMENT &&
          out == literals[2] &&
          mc_tile_entity_class_literal_get(classes[0], MC_TILE_ENTITY_CLASS_KIND_CHEST, NULL) ==
              MC_TILE_ENTITY_INVALID_ARGUMENT &&
          mc_tile_entity_class_literal_superclass(NULL, &out) == MC_TILE_ENTITY_INVALID_ARGUMENT &&
          out == literals[2] &&
          mc_tile_entity_class_literal_superclass(literals[2], NULL) ==
              MC_TILE_ENTITY_INVALID_ARGUMENT);
    mc_tile_entity_class_literal_view literal_view, saved_view;
    CHECK(mc_tile_entity_class_literal_inspect(literals[2], &saved_view) == MC_TILE_ENTITY_OK);
    literal_view = saved_view;
    CHECK(mc_tile_entity_class_literal_inspect(NULL, &literal_view) ==
              MC_TILE_ENTITY_INVALID_ARGUMENT &&
          literal_view.owner == saved_view.owner &&
          literal_view.world_classes == saved_view.world_classes &&
          literal_view.side == saved_view.side && literal_view.kind == saved_view.kind &&
          literal_view.client_name == saved_view.client_name &&
          literal_view.server_name == saved_view.server_name &&
          literal_view.superclass_name == saved_view.superclass_name &&
          mc_tile_entity_class_literal_inspect(literals[2], NULL) ==
              MC_TILE_ENTITY_INVALID_ARGUMENT);
    mc_tile_entity_classes *preserved = classes[0];
    CHECK(mc_tile_entity_classes_borrow(NULL, &preserved) == MC_TILE_ENTITY_INVALID_ARGUMENT &&
          preserved == classes[0] &&
          mc_tile_entity_classes_borrow(world_classes[0], NULL) ==
              MC_TILE_ENTITY_INVALID_ARGUMENT &&
          mc_tile_entity_classes_release(NULL) == MC_TILE_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_tile_entity_piston_class_get_snapshot(world_classes[0], &view) == MC_TILE_ENTITY_OK &&
          cold(&view, 2) &&
          mc_tile_entity_piston_class_get_snapshot(world_classes[1], &view) == MC_TILE_ENTITY_OK &&
          cold(&view, 1) &&
          mc_world_runtime_classes_get(world_classes[0], &world_state) == MC_WORLD_STORAGE_OK &&
          !world_state.initialized && world_state.live_worlds == 0 &&
          atomic_load_explicit(&profile->uniquifier, memory_order_relaxed) == unique &&
          atomic_load_explicit(&profile->holder_phase, memory_order_relaxed) == holder &&
          atomic_load_explicit(&profile->constructor_admitted, memory_order_relaxed) == admitted &&
          mc_java_random_profile_allocation_bytes(profile) == profile_bytes);

    /* Both retirement entry points must refuse even before any active use or
     * World instance exists; a refused retirement must preserve every cell. */
    mc_world_storage_error error = {0};
    CHECK(mc_live_tile_entities_classes_retire(world_classes[0], &error) == MC_WORLD_STORAGE_BUSY &&
          error.status == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_classes_free(world_classes[0], &error) == MC_WORLD_STORAGE_BUSY &&
          error.status == MC_WORLD_STORAGE_BUSY &&
          mc_tile_entity_class_literal_get(classes[0], MC_TILE_ENTITY_CLASS_KIND_CHEST, &out) ==
              MC_TILE_ENTITY_OK &&
          out == literals[2]);
    CHECK(mc_tile_entity_classes_release(&again) == MC_TILE_ENTITY_OK && again == NULL &&
          mc_tile_entity_classes_release(&again) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_class_get_snapshot(world_classes[0], &view) == MC_TILE_ENTITY_OK &&
          cold(&view, 1) &&
          mc_world_runtime_classes_free(world_classes[0], NULL) == MC_WORLD_STORAGE_BUSY);

    /* Bind Registry AFTER the passive owner was created. First real OW/JH use
     * must use that same later application binding, without replacing cells. */
    mc_registry_options options = {.side = side == MC_WORLD_INFO_CLIENT ? MC_REGISTRY_CLIENT
                                                                        : MC_REGISTRY_SERVER,
                                   .random_profile = profile,
                                   .materials = materials};
    CHECK(mc_registry_classes_allocate(&options, &registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_registry(world_classes[0], registry[0], NULL) ==
              MC_REGISTRY_OK);
    static const mc_tile_entity_kind instance_kinds[] = {
        MC_TILE_ENTITY_CHEST, MC_TILE_ENTITY_SPAWNER, MC_TILE_ENTITY_PISTON};
    static const mc_tile_entity_class_kind instance_classes[] = {MC_TILE_ENTITY_CLASS_KIND_CHEST,
                                                                 MC_TILE_ENTITY_CLASS_KIND_SPAWNER,
                                                                 MC_TILE_ENTITY_CLASS_KIND_PISTON};
    for (size_t i = 0; i < COUNT(instances); ++i) {
        const mc_tile_entity_class_literal *expected_class = NULL;
        mc_tile_entity_snapshot instance_view;
        CHECK(mc_tile_entity_allocate_in_classes(world_classes[0], instance_kinds[i],
                                                 &instances[i]) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_get_snapshot(instances[i], &instance_view) == MC_TILE_ENTITY_OK &&
              instance_view.phase == MC_TILE_ENTITY_RAW &&
              mc_tile_entity_class_literal_get(classes[0], instance_classes[i], &expected_class) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_get_class(instances[i], &out) == MC_TILE_ENTITY_OK &&
              out == expected_class &&
              mc_tile_entity_get_class(instances[i], NULL) == MC_TILE_ENTITY_INVALID_ARGUMENT);
    }
    /* Capture the real Registry bytes after the eight base registration
     * literals, before a Spawner constructor adds Pig. This becomes an exact
     * native quota control for a separate genuine failed construction. */
    mc_registry_block_view registry_view;
    CHECK(mc_registry_classes_inspect_blocks(registry[0], &registry_view, NULL) == MC_REGISTRY_OK &&
          registry_view.phase == MC_REGISTRY_COLD);
    const size_t registration_bytes = registry_view.allocation_bytes;
    for (size_t i = 0; i < COUNT(instances); ++i) {
        const mc_tile_entity_class_literal *expected_class = NULL;
        mc_tile_entity_snapshot instance_view;
        CHECK(mc_tile_entity_construct(instances[i]) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_get_snapshot(instances[i], &instance_view) == MC_TILE_ENTITY_OK &&
              instance_view.phase == MC_TILE_ENTITY_READY &&
              mc_tile_entity_class_literal_get(classes[0], instance_classes[i], &expected_class) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_get_class(instances[i], &out) == MC_TILE_ENTITY_OK &&
              out == expected_class);
    }
    CHECK(mc_tile_entity_get_class(NULL, &out) == MC_TILE_ENTITY_INVALID_ARGUMENT &&
          mc_tile_entity_piston_class_get_snapshot(world_classes[0], &view) == MC_TILE_ENTITY_OK &&
          view.base_phase == MC_TILE_ENTITY_CLASS_READY && view.base_name_count == 8 &&
          view.base_class_count == 8 && view.piston_phase == MC_TILE_ENTITY_CLASS_READY &&
          view.live_tile_entities == COUNT(instances) && view.class_literal_borrows == 1);

    options.max_allocation_bytes = registration_bytes;
    CHECK(mc_registry_classes_allocate(&options, &registry[1], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(side, profile, &trig, &world_classes[2], NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(world_classes[2], registry[1], NULL) ==
              MC_REGISTRY_OK &&
          mc_tile_entity_classes_borrow(world_classes[2], &classes[2]) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_allocate_in_classes(world_classes[2], MC_TILE_ENTITY_SPAWNER, &failed) ==
              MC_TILE_ENTITY_OK);
    const mc_tile_entity_class_literal *failed_class = NULL;
    mc_tile_entity_error constructor_error;
    mc_tile_entity_snapshot failed_view;
    CHECK(mc_tile_entity_get_class(failed, &failed_class) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct_detailed(failed, &constructor_error) == MC_TILE_ENTITY_LIMIT &&
          constructor_error.registry.status == MC_REGISTRY_LIMIT &&
          constructor_error.constructor_bytecode == 15 &&
          mc_tile_entity_get_snapshot(failed, &failed_view) == MC_TILE_ENTITY_OK &&
          failed_view.phase == MC_TILE_ENTITY_FAILED_CONSTRUCTION &&
          mc_tile_entity_get_class(failed, &out) == MC_TILE_ENTITY_OK && out == failed_class &&
          mc_tile_entity_class_literal_inspect(out, &literal_view) == MC_TILE_ENTITY_OK &&
          literal_view.kind == MC_TILE_ENTITY_CLASS_KIND_SPAWNER &&
          literal_view.owner == classes[2] &&
          mc_tile_entity_construct(failed) == MC_TILE_ENTITY_INVALID_PHASE);
    for (size_t i = 0; i < COUNT(instances); ++i) {
        mc_tile_entity_release(instances[i]);
        instances[i] = NULL;
    }
    mc_tile_entity_release(failed);
    failed = NULL;
    CHECK(mc_world_runtime_classes_free(world_classes[0], NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_classes_free(world_classes[2], NULL) == MC_WORLD_STORAGE_BUSY);
    /* Preserve only a nonowning diagnostic alias to test unbalanced release
     * while its World class is still live; do not use literals after release. */
    preserved = classes[0];
    CHECK(mc_tile_entity_classes_release(&classes[0]) == MC_TILE_ENTITY_OK && classes[0] == NULL &&
          mc_tile_entity_classes_release(&preserved) == MC_TILE_ENTITY_INVALID_ARGUMENT &&
          preserved != NULL);
    preserved = NULL;
    CHECK(mc_world_runtime_classes_free(world_classes[0], NULL) == MC_WORLD_STORAGE_OK);
    world_classes[0] = NULL;
    ok = true;
cleanup:
    for (size_t i = 0; i < COUNT(instances); ++i)
        mc_tile_entity_release(instances[i]);
    mc_tile_entity_release(failed);
    if (mc_tile_entity_classes_release(&again) != MC_TILE_ENTITY_OK)
        retirement_failed = true;
    for (size_t i = 0; i < COUNT(classes); ++i)
        if (mc_tile_entity_classes_release(&classes[i]) != MC_TILE_ENTITY_OK)
            retirement_failed = true;
    for (size_t i = 0; i < COUNT(world_classes); ++i)
        if (mc_world_runtime_classes_free(world_classes[i], NULL) != MC_WORLD_STORAGE_OK)
            retirement_failed = true;
    if (retirement_failed) {
        fputs("TileEntity Class retirement refused; dependencies retained\n", stderr);
        return false;
    }
    for (size_t i = 0; i < COUNT(registry); ++i)
        if (mc_registry_classes_free(registry[i], NULL) != MC_REGISTRY_OK)
            retirement_failed = true;
    return ok && !retirement_failed;
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    CHECK(mc_math_trig_init(&trig) && mc_materials_create(&materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(run_side(MC_WORLD_INFO_CLIENT, profile, materials) &&
          run_side(MC_WORLD_INFO_SERVER, profile, materials));
    result = 0;
cleanup:
    if (retirement_failed)
        return 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    if (result == 0 && printf("PASS: %u passive TileEntity Class checks\n", checks) < 0)
        result = 1;
    return result;
}
