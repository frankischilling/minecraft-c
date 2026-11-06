#include "../../../src/block/registry/managed_internal.h"
#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/item/registry/stacks_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;

typedef union payload_alignment {
    long double floating;
    long long integer;
    void *pointer;
} payload_alignment;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool create(mc_registry_side side, mc_java_random_profile *profile, mc_materials *materials,
                   size_t spare, mc_registry_classes **out) {
    const mc_registry_options options = {side, profile, materials,
                                         sizeof(mc_registry_classes) + spare, NULL};
    return mc_registry_classes_allocate(&options, out, NULL) == MC_REGISTRY_OK;
}

static bool bytes_are(const void *storage, size_t count, unsigned char value) {
    const unsigned char *bytes = storage;
    for (size_t i = 0; i < count; ++i)
        if (bytes[i] != value)
            return false;
    return true;
}

static bool accounting(const mc_registry_classes *owner, size_t permanent, size_t managed,
                       size_t peak) {
    mc_registry_block_view view;
    return mc_registry_classes_inspect_blocks(owner, &view, NULL) == MC_REGISTRY_OK &&
           view.phase == MC_REGISTRY_COLD && view.initializer_pc == -1 && view.named_blocks == 0 &&
           view.m == NULL && view.allocation_bytes == permanent + managed &&
           owner->allocation_bytes == permanent && owner->managed_current_bytes == managed &&
           owner->total_peak_bytes == peak;
}

static bool stack_is(const mc_item_stack *stack, int32_t id, int32_t count, int32_t damage) {
    mc_item_stack_fields fields;
    mc_item_stack_phase phase;
    return mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
           mc_item_stack_phase_get(stack, &phase) == MC_ITEM_STACK_OK &&
           phase == MC_ITEM_STACK_READY && fields.id == id && fields.count == count &&
           fields.damage == damage && fields.animation == 0;
}

static int test_release(mc_registry_side side, mc_java_random_profile *profile,
                        mc_materials *materials) {
    int result = 1;
    mc_registry_classes *owner = NULL, *other = NULL;
    void *payloads[4] = {0};
    const size_t sizes[4] = {17, 34, 51, 68};
    size_t charges[4] = {0};
    CHECK(create(side, profile, materials, 4096, &owner));
    CHECK(create(side, profile, materials, 4096, &other));
    unsigned char *permanent = mc_registry_allocate(owner, 32);
    unsigned char *isolated = mc_registry_allocate(other, 16);
    CHECK(permanent != NULL && isolated != NULL);
    memset(permanent, 0x3c, 32);
    memset(isolated, 0xa7, 16);
    const size_t base = owner->allocation_bytes, other_base = other->allocation_bytes;
    CHECK(accounting(owner, base, 0, base));
    CHECK(accounting(other, other_base, 0, other_base));
    CHECK(mc_registry_fail(owner, MC_REGISTRY_LIMIT, "retained diagnostic", NULL) ==
          MC_REGISTRY_LIMIT);
    mc_registry_error saved;
    memcpy(&saved, &owner->last_error, sizeof(saved));
    size_t current = 0, overhead = 0;
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_registry_managed_allocate(owner, sizes[i], &payloads[i]) == MC_REGISTRY_OK);
        CHECK((uintptr_t)payloads[i] % _Alignof(payload_alignment) == 0);
        CHECK(bytes_are(payloads[i], sizes[i], 0));
        charges[i] = owner->managed_current_bytes - current;
        if (i == 0) {
            CHECK(charges[i] > sizes[i]);
            overhead = charges[i] - sizes[i];
        }
        CHECK(charges[i] == overhead + sizes[i]);
        current += charges[i];
        CHECK(accounting(owner, base, current, base + current));
        memset(payloads[i], (int)(0x40 + i), sizes[i]);
    }
    const size_t peak = base + current;
    void *sentinel = permanent;
    CHECK(mc_registry_managed_allocate(owner, 0, &sentinel) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(sentinel == permanent);
    CHECK(mc_registry_managed_allocate(NULL, 1, &sentinel) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(sentinel == permanent);
    CHECK(mc_registry_managed_allocate(owner, 1, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_managed_allocate(owner, SIZE_MAX, &sentinel) == MC_REGISTRY_LIMIT);
    CHECK(sentinel == permanent);
    CHECK(!mc_registry_managed_fits(NULL, 0) && mc_registry_managed_remaining(NULL) == 0);
    const size_t remaining = mc_registry_managed_remaining(owner);
    CHECK(mc_registry_managed_fits(owner, remaining));
    CHECK(!mc_registry_managed_fits(owner, remaining + 1));
    CHECK(!mc_registry_managed_fits(owner, SIZE_MAX));
    CHECK(accounting(owner, base, current, peak));
    CHECK(memcmp(&saved, &owner->last_error, sizeof(saved)) == 0);

    /* Newest-first ledger: remove a middle entry, tail, head with a successor,
     * then the final entry. Surviving payloads and a separate owner stay live. */
    const size_t order[4] = {2, 0, 3, 1};
    for (size_t step = 0; step < 4; ++step) {
        const size_t index = order[step];
        mc_registry_managed_release(owner, payloads[index]);
        payloads[index] = NULL;
        current -= charges[index];
        CHECK(accounting(owner, base, current, peak));
        for (size_t i = 0; i < 4; ++i)
            if (payloads[i] != NULL)
                CHECK(bytes_are(payloads[i], sizes[i], (unsigned char)(0x40 + i)));
        CHECK(bytes_are(permanent, 32, 0x3c) && bytes_are(isolated, 16, 0xa7));
        CHECK(accounting(other, other_base, 0, other_base));
    }
    CHECK(mc_registry_managed_allocate(owner, 34, &sentinel) == MC_REGISTRY_OK);
    CHECK(bytes_are(sentinel, 34, 0));
    CHECK(accounting(owner, base, charges[1], peak));
    mc_registry_managed_retire(owner);
    mc_registry_managed_retire(owner);
    mc_registry_managed_retire(NULL);
    mc_registry_managed_release(owner, NULL);
    CHECK(accounting(owner, base, 0, peak));
    CHECK(bytes_are(permanent, 32, 0x3c) && bytes_are(isolated, 16, 0xa7));
    CHECK(memcmp(&saved, &owner->last_error, sizeof(saved)) == 0);
    result = 0;
cleanup:
    if (other != NULL)
        (void)mc_registry_classes_free(other, NULL);
    if (owner != NULL)
        (void)mc_registry_classes_free(owner, NULL);
    return result;
}

static int test_joint_budget(mc_registry_side side, mc_java_random_profile *profile,
                             mc_materials *materials) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    void *held = NULL, *filler = NULL;
    CHECK(create(side, profile, materials, 2048, &owner));
    const size_t initial = owner->allocation_bytes;
    unsigned char *permanent = mc_registry_allocate(owner, 32);
    CHECK(permanent != NULL);
    memset(permanent, 0x5a, 32);
    const size_t base = owner->allocation_bytes;
    CHECK(base > initial + 32);
    const size_t permanent_overhead = base - initial - 32;
    CHECK(mc_registry_managed_allocate(owner, 31, &held) == MC_REGISTRY_OK);
    const size_t charge = owner->managed_current_bytes;
    CHECK(charge > 31);
    const size_t overhead = charge - 31;
    memset(held, 0x64, 31);
    size_t remaining = mc_registry_managed_remaining(owner);
    CHECK(remaining > overhead);
    CHECK(mc_registry_managed_allocate(owner, remaining - overhead, &filler) == MC_REGISTRY_OK);
    const size_t maximum = owner->options.max_allocation_bytes;
    CHECK(accounting(owner, base, maximum - base, maximum));
    CHECK(mc_registry_managed_remaining(owner) == 0);
    CHECK(mc_registry_allocate(owner, 1) == NULL);
    CHECK(owner->last_error.status == MC_REGISTRY_LIMIT);
    CHECK(bytes_are(permanent, 32, 0x5a) && bytes_are(held, 31, 0x64));
    mc_registry_managed_release(owner, filler);
    filler = NULL;
    CHECK(accounting(owner, base, charge, maximum));

    /* Freed managed budget admits permanent storage. Its retained charge then
     * blocks managed storage in the opposite direction, without harming data. */
    remaining = mc_registry_managed_remaining(owner);
    CHECK(remaining > permanent_overhead);
    const size_t fill_bytes = remaining - permanent_overhead;
    unsigned char *retained = mc_registry_allocate(owner, fill_bytes);
    CHECK(retained != NULL && bytes_are(retained, fill_bytes, 0));
    memset(retained, 0x89, fill_bytes);
    CHECK(accounting(owner, maximum - charge, charge, maximum));
    void *sentinel = permanent;
    mc_registry_error saved;
    memcpy(&saved, &owner->last_error, sizeof(saved));
    CHECK(mc_registry_managed_allocate(owner, 1, &sentinel) == MC_REGISTRY_LIMIT);
    CHECK(sentinel == permanent && memcmp(&saved, &owner->last_error, sizeof(saved)) == 0);
    CHECK(accounting(owner, maximum - charge, charge, maximum));
    mc_registry_managed_release(owner, held);
    held = NULL;
    CHECK(accounting(owner, maximum - charge, 0, maximum));
    CHECK(mc_registry_managed_allocate(owner, 31, &held) == MC_REGISTRY_OK);
    CHECK(bytes_are(held, 31, 0));
    CHECK(accounting(owner, maximum - charge, charge, maximum));
    CHECK(bytes_are(permanent, 32, 0x5a) && bytes_are(retained, fill_bytes, 0x89));
    CHECK(memcmp(&saved, &owner->last_error, sizeof(saved)) == 0);
    result = 0;
cleanup:
    if (owner != NULL)
        (void)mc_registry_classes_free(owner, NULL);
    return result;
}

static int test_stack_budget(mc_registry_side side, mc_java_random_profile *profile,
                             mc_materials *materials) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    mc_item_stack *first = NULL, *next = NULL;
    void *held = NULL;
    CHECK(create(side, profile, materials, 2048, &owner));
    owner->entered = 1; /* The real private Stack allocator requires admitted access. */
    const size_t initial = owner->allocation_bytes;
    CHECK(mc_registry_stack_allocate(owner, &first) == MC_REGISTRY_OK);
    CHECK(mc_registry_stack_construct_numeric(owner, first, 264, 7, 3) == MC_REGISTRY_OK);
    const size_t base = owner->allocation_bytes, stack_charge = base - initial;
    CHECK(stack_charge > mc_item_stack_object_bytes());
    const size_t ledger_charge = stack_charge - mc_item_stack_object_bytes();
    CHECK(mc_registry_managed_allocate(owner, 1, &held) == MC_REGISTRY_OK);
    CHECK(owner->managed_current_bytes > 1);
    const size_t overhead = owner->managed_current_bytes - 1;
    mc_registry_managed_release(owner, held);
    held = NULL;
    const size_t remaining = mc_registry_managed_remaining(owner);
    CHECK(remaining > ledger_charge + overhead);
    const size_t payload_bytes = remaining - ledger_charge - overhead;
    CHECK(mc_registry_managed_allocate(owner, payload_bytes, &held) == MC_REGISTRY_OK);
    const size_t managed = owner->managed_current_bytes;
    memset(held, 0xb6, payload_bytes);
    CHECK(mc_registry_managed_remaining(owner) == ledger_charge);

    /* The retained ledger fits, but the actual external ItemStack does not.
     * A failed prefix remains charged; neither output nor prior Stack changes. */
    next = first;
    CHECK(mc_registry_stack_allocate(owner, &next) == MC_REGISTRY_LIMIT);
    CHECK(next == first && mc_registry_stack_count(owner) == 1);
    CHECK(owner->last_error.status == MC_REGISTRY_LIMIT);
    CHECK(strcmp(owner->last_error.message, "Stack object allocation bound") == 0);
    const size_t maximum = owner->options.max_allocation_bytes;
    CHECK(accounting(owner, base + ledger_charge, managed, maximum));
    CHECK(mc_registry_managed_remaining(owner) == 0);
    CHECK(stack_is(first, 264, 7, 3) && bytes_are(held, payload_bytes, 0xb6));
    mc_registry_error saved;
    memcpy(&saved, &owner->last_error, sizeof(saved));
    mc_registry_managed_release(owner, held);
    held = NULL;
    CHECK(accounting(owner, base + ledger_charge, 0, maximum));
    CHECK(memcmp(&saved, &owner->last_error, sizeof(saved)) == 0);
    CHECK(mc_registry_stack_allocate(owner, &next) == MC_REGISTRY_OK);
    CHECK(next != first && mc_registry_stack_count(owner) == 2);
    CHECK(mc_registry_stack_construct_numeric(owner, next, 280, 13, 5) == MC_REGISTRY_OK);
    CHECK(accounting(owner, base + ledger_charge + stack_charge, 0, maximum));
    CHECK(stack_is(first, 264, 7, 3) && stack_is(next, 280, 13, 5));
    result = 0;
cleanup:
    if (owner != NULL) {
        owner->entered = 0;
        (void)mc_registry_classes_free(owner, NULL);
    }
    return result;
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(profile);
    const mc_registry_side sides[2] = {MC_REGISTRY_CLIENT, MC_REGISTRY_SERVER};
    for (size_t i = 0; i < 2; ++i) {
        CHECK(test_release(sides[i], profile, materials) == 0);
        CHECK(test_joint_budget(sides[i], profile, materials) == 0);
        CHECK(test_stack_budget(sides[i], profile, materials) == 0);
    }
    CHECK(mc_java_random_profile_allocation_bytes(profile) == profile_bytes);
    printf("PASS: %u managed Registry allocation checks\n", checks);
    result = 0;
cleanup:
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
