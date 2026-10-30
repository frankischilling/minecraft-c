#include "mc/entity/particles/origin.h"

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/classes/classes_internal.h"
#include "mc/entity/entity.h"

#include <emmintrin.h>
#include <stdio.h>
#include <string.h>

/* Native ownership/source-storage controls. No original runtime comparison or
 * synthetic native class phase, counter, hold or Entity owner is supplied. */
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static double from_bits(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint64_t bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool cells(const mc_entity_classes *classes, uint64_t l, uint64_t m, uint64_t n) {
    mc_particle_origin origin;
    return mc_entity_particle_origin_read(classes, &origin, NULL) == MC_ENTITY_OK &&
           bits(origin.l) == l && bits(origin.m) == m && bits(origin.n) == n;
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *first = NULL, *second = NULL, *server = NULL;
    mc_entity *actor = NULL;
    const unsigned int initial_controls = _mm_getcsr();
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &first) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &second) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_SERVER, profile, NULL, &server) == MC_ENTITY_OK);
    mc_entity_classes first_before, second_before, server_before;
    memcpy(&first_before, first, sizeof(first_before));
    memcpy(&second_before, second, sizeof(second_before));
    memcpy(&server_before, server, sizeof(server_before));
    CHECK(cells(first, 0, 0, 0) && cells(second, 0, 0, 0));
    CHECK(first->entity_phase == MC_ENTITY_CLASS_COLD &&
          second->entity_phase == MC_ENTITY_CLASS_COLD);
    CHECK(memcmp(first, &first_before, sizeof(*first)) == 0 &&
          memcmp(second, &second_before, sizeof(*second)) == 0);
    const uint64_t unique_before = atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(profile);
    CHECK(profile->math == NULL);

    mc_entity_error error;
    memset(&error, 0xa5, sizeof(error));
    CHECK(mc_entity_particle_origin_store(first, 3, 4.0, &error) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(error.status == MC_ENTITY_INVALID_ARGUMENT && error.operation != NULL);
    CHECK(mc_entity_particle_origin_store(NULL, 0, 4.0, NULL) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_entity_particle_origin_store(server, 0, 4.0, NULL) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(memcmp(first, &first_before, sizeof(*first)) == 0 &&
          memcmp(server, &server_before, sizeof(*server)) == 0);
    mc_particle_origin out = {1.0, 2.0, 3.0}, saved = out;
    CHECK(mc_entity_particle_origin_read(NULL, &out, &error) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(memcmp(&out, &saved, sizeof(out)) == 0);
    CHECK(mc_entity_particle_origin_read(server, &out, NULL) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(memcmp(&out, &saved, sizeof(out)) == 0);
    CHECK(mc_entity_particle_origin_read(first, NULL, NULL) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(memcmp(first, &first_before, sizeof(*first)) == 0);

    /* Hardware control variation proves this storage/class-active-use leaf
     * adds no FP guard or arithmetic. Restore before checking any result. */
    const unsigned int changed_controls = initial_controls | 0x8040u;
    _mm_setcsr(changed_controls);
    const mc_entity_status stored =
        mc_entity_particle_origin_store(first, 2, from_bits(UINT64_C(0x7ff0000000001234)), &error);
    const unsigned int after_controls = _mm_getcsr();
    _mm_setcsr(initial_controls);
    CHECK(stored == MC_ENTITY_OK && after_controls == changed_controls);
    CHECK(error.status == MC_ENTITY_OK && error.operation == NULL && error.bytecode == 0);
    CHECK(first->entity_phase == MC_ENTITY_CLASS_READY && first->next_id == 0 &&
          first->live_entities == 0 && first->allocated_bytes == first_before.allocated_bytes);
    CHECK(cells(first, 0, 0, UINT64_C(0x7ff0000000001234)));
    CHECK(mc_entity_particle_origin_store(first, 0, from_bits(UINT64_C(0x8000000000000000)),
                                          NULL) == MC_ENTITY_OK);
    CHECK(cells(first, UINT64_C(0x8000000000000000), 0, UINT64_C(0x7ff0000000001234)));
    CHECK(mc_entity_particle_origin_store(first, 1, from_bits(UINT64_C(0x0000000000000001)),
                                          NULL) == MC_ENTITY_OK);
    CHECK(cells(first, UINT64_C(0x8000000000000000), UINT64_C(1), UINT64_C(0x7ff0000000001234)));
    CHECK(cells(second, 0, 0, 0) && memcmp(second, &second_before, sizeof(*second)) == 0);
    CHECK(mc_entity_particle_origin_store(second, 0, 7.0, NULL) == MC_ENTITY_OK);
    CHECK(cells(second, UINT64_C(0x401c000000000000), 0, 0));
    CHECK(cells(first, UINT64_C(0x8000000000000000), UINT64_C(1), UINT64_C(0x7ff0000000001234)));
    CHECK(profile->math == NULL &&
          mc_java_random_profile_allocation_bytes(profile) == profile_bytes &&
          atomic_load_explicit(&profile->uniquifier, memory_order_relaxed) == unique_before);

    /* A genuine constructor advances SN's counter. A later origin store must
     * use the already initialized class without resetting that source history. */
    CHECK(mc_falling_entity_allocate(first, &actor, NULL) == MC_ENTITY_OK);
    CHECK(mc_falling_entity_construct_world(actor, NULL, NULL) == MC_ENTITY_OK);
    CHECK(first->next_id == 1);
    mc_entity_snapshot actor_before, actor_after;
    CHECK(mc_entity_get_snapshot(actor, &actor_before) == MC_ENTITY_OK);
    memcpy(&first_before, first, sizeof(first_before));
    const uint64_t constructed_unique =
        atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
    CHECK(mc_entity_particle_origin_store(first, 1, from_bits(UINT64_C(0xfff8000000005678)),
                                          &error) == MC_ENTITY_OK);
    CHECK(first->next_id == 1 && first->live_entities == first_before.live_entities &&
          first->borrowers == first_before.borrowers &&
          first->allocated_bytes == first_before.allocated_bytes &&
          first->bounds_holds == first_before.bounds_holds);
    CHECK(cells(first, UINT64_C(0x8000000000000000), UINT64_C(0xfff8000000005678),
                UINT64_C(0x7ff0000000001234)));
    CHECK(mc_entity_get_snapshot(actor, &actor_after) == MC_ENTITY_OK);
    CHECK(actor_after.phase == actor_before.phase && actor_after.base.id == actor_before.base.id &&
          actor_after.random == actor_before.random &&
          actor_after.watcher == actor_before.watcher && actor_after.bounds == actor_before.bounds);
    CHECK(atomic_load_explicit(&profile->uniquifier, memory_order_relaxed) == constructed_unique);
    memcpy(&first_before, first, sizeof(first_before));
    CHECK(mc_entity_particle_origin_read(first, &out, &error) == MC_ENTITY_OK);
    CHECK(error.status == MC_ENTITY_OK && error.operation == NULL &&
          memcmp(first, &first_before, sizeof(*first)) == 0);
    result = 0;
cleanup:
    _mm_setcsr(initial_controls);
    mc_entity_release(actor);
    if (first != NULL && mc_entity_classes_free(first) != MC_ENTITY_OK)
        result = 1;
    if (second != NULL && mc_entity_classes_free(second) != MC_ENTITY_OK)
        result = 1;
    if (server != NULL && mc_entity_classes_free(server) != MC_ENTITY_OK)
        result = 1;
    mc_java_random_profile_free(profile);
    if (result == 0)
        printf("Particle camera-origin native controls: %u checks\n", checks);
    return result;
}
