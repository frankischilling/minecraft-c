#include "redstone_torch_internal.h"

#include "../../../../block/registry/runtime_internal.h"
#include "mc/block/registry/runtime.h"

#include <stdlib.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Redstone Torch floating operations require x86-64/SSE2"
#endif
#include <emmintrin.h>

static bool prune(const mc_redstone_torch_frame *frame) {
    for (;;) {
        mc_registry_redstone_torch_list_ref list;
        mc_registry_redstone_torch_error collection;
        if (!mc_redstone_torch_list(frame, 11, &list))
            return false;
        int32_t size;
        mc_redstone_torch_at(frame, 14);
        (void)mc_registry_redstone_torch_list_size(&list, &size, &collection);
        if (!mc_redstone_torch_collection(frame, 14, &collection,
                                          "Redstone Torch prune actual List size"))
            return false;
        if (size <= 0)
            return true;
        int64_t time;
        if (!mc_redstone_torch_time(frame, 23, &time))
            return false;
        if (!mc_redstone_torch_list(frame, 26, &list))
            return false;
        mc_registry_redstone_torch_record *entry;
        mc_redstone_torch_at(frame, 30);
        (void)mc_registry_redstone_torch_list_get(&list, 0, &entry, &collection);
        if (!mc_redstone_torch_collection(frame, 30, &collection,
                                          "Redstone Torch prune actual List get"))
            return false;
        mc_redstone_torch_at(frame, 38);
        if (entry == NULL)
            return mc_redstone_torch_null(frame, 38, "Redstone Torch actual null XU time read");
        mc_registry_redstone_torch_record_view view;
        const mc_registry_status status = mc_registry_redstone_torch_record_inspect(entry, &view);
        if (status != MC_REGISTRY_OK)
            return mc_redstone_torch_fail(frame, 38, mc_world_runtime_registry_status(status),
                                          "Redstone Torch reached XU time storage");
        if (mc_java_i64_sub(time, view.time) <= INT64_C(100))
            return true;
        if (!mc_redstone_torch_list(frame, 49, &list))
            return false;
        entry = NULL;
        mc_redstone_torch_at(frame, 53);
        (void)mc_registry_redstone_torch_list_remove_first(&list, &entry, &collection);
        if (!mc_redstone_torch_collection(frame, 53, &collection,
                                          "Redstone Torch prune actual List remove"))
            return false;
        mc_registry_redstone_torch_record_release(entry); /* Source ignored result pop58. */
    }
}

static bool burnout(const mc_redstone_torch_frame *frame, unsigned int pc, int32_t x, int32_t y,
                    int32_t z, bool record, bool *out) {
    mc_redstone_torch_at(frame, pc);
    mc_world_mutation_error *lower = mc_redstone_torch_workspace(frame, pc);
    if (lower == NULL)
        return false;
    return mc_redstone_torch_child(frame, pc, lower,
                                   mc_world_mutation_redstone_torch_burnout(
                                       frame->world, frame->block, x, y, z, record, out, lower));
}

static bool replace(const mc_redstone_torch_frame *frame, bool lit, int32_t x, int32_t y,
                    int32_t z) {
    const unsigned int capture_pc = lit ? 79u : 269u, field_pc = lit ? 82u : 272u;
    const unsigned int metadata_pc = lit ? 90u : 280u, setter_pc = lit ? 93u : 283u;
    mc_redstone_torch_at(frame, capture_pc);
    mc_registry_error registry = mc_world_mutation_empty_registry();
    const mc_block *target;
    const mc_registry_status status =
        mc_registry_block_static_get(frame->block->classes, lit ? "aQ" : "aR", &target, &registry);
    if (status != MC_REGISTRY_OK)
        return mc_redstone_torch_registry(frame, capture_pc, status, &registry,
                                          "Redstone Torch live replacement Block field");
    mc_redstone_torch_at(frame, field_pc);
    if (target == NULL)
        return mc_redstone_torch_null(frame, field_pc,
                                      "Redstone Torch actual null replacement Block");
    const int32_t id = target->fields.bn;
    int32_t metadata;
    if (!mc_redstone_torch_metadata(frame, metadata_pc, x, y, z, &metadata))
        return false;
    mc_redstone_torch_at(frame, setter_pc);
    mc_world_mutation_error *lower = mc_redstone_torch_workspace(frame, setter_pc);
    if (lower == NULL)
        return false;
    bool ignored;
    const bool complete =
        mc_world_runtime_block_metadata_set(frame->world, x, y, z, id, metadata, &ignored, lower) ==
        MC_WORLD_STORAGE_OK;
    return mc_redstone_torch_child(frame, setter_pc, lower, complete);
}

static bool literal(const mc_redstone_torch_frame *frame, unsigned int pc, const char *text,
                    const mc_registry_string **out) {
    mc_redstone_torch_at(frame, pc);
    mc_registry_error registry = mc_world_mutation_empty_registry();
    const mc_registry_status status =
        mc_registry_runtime_literal_ascii(frame->block->classes, text, out, &registry);
    return status == MC_REGISTRY_OK ||
           mc_redstone_torch_registry(frame, pc, status, &registry,
                                      "Redstone Torch actual String literal");
}

static bool fizz(const mc_redstone_torch_frame *frame, int32_t x, int32_t y, int32_t z,
                 mc_java_random *random) {
    /* First reached FP conversion is i2f112, after setter and List recording.
     * Admission observes the caller's environment without repairing it. */
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return mc_redstone_torch_fail(frame, 112, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Redstone Torch fizz FP admission");
    const float sound_x = (float)x + 0.5f, sound_y = (float)y + 0.5f, sound_z = (float)z + 0.5f;
    const mc_registry_string *sound;
    if (!literal(frame, 130, "random.fizz", &sound))
        return false;
    mc_java_random *world_random = frame->world->fields.r;
    mc_redstone_torch_at(frame, 140);
    if (world_random == NULL)
        return mc_redstone_torch_null(frame, 140,
                                      "Redstone Torch actual null World Random float call");
    const float first = mc_java_random_next_float(world_random);
    world_random = frame->world->fields.r;
    mc_redstone_torch_at(frame, 147);
    if (world_random == NULL)
        return mc_redstone_torch_null(frame, 147,
                                      "Redstone Torch actual null World Random float call");
    const float second = mc_java_random_next_float(world_random);
    const float difference = first - second;
    const float scaled = difference * 0.8f;
    const float pitch = 2.6f + scaled;
    mc_redstone_torch_at(frame, 155);
    mc_world_mutation_error *lower = mc_redstone_torch_workspace(frame, 155);
    if (lower == NULL)
        return false;
    if (!mc_redstone_torch_child(frame, 155, lower,
                                 mc_world_mutation_sound(frame->world, (double)sound_x,
                                                         (double)sound_y, (double)sound_z, sound,
                                                         0.5f, pitch, lower)))
        return false;
    for (int32_t i = 0; i < 5; ++i) {
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
            return mc_redstone_torch_fail(frame, 168, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Redstone Torch particle FP admission");
        mc_redstone_torch_at(frame, 171);
        if (random == NULL)
            return mc_redstone_torch_null(frame, 171,
                                          "Redstone Torch actual null supplied Random double call");
        const double draw_x = mc_java_random_next_double(random);
        const double scaled_x = draw_x * 0.6;
        const double offset_x = (double)x + scaled_x;
        const double particle_x = offset_x + 0.2;
        mc_redstone_torch_at(frame, 189);
        const double draw_y = mc_java_random_next_double(random);
        const double scaled_y = draw_y * 0.6;
        const double offset_y = (double)y + scaled_y;
        const double particle_y = offset_y + 0.2;
        mc_redstone_torch_at(frame, 208);
        const double draw_z = mc_java_random_next_double(random);
        const double scaled_z = draw_z * 0.6;
        const double offset_z = (double)z + scaled_z;
        const double particle_z = offset_z + 0.2;
        const mc_registry_string *particle;
        if (!literal(frame, 223, "smoke", &particle))
            return false;
        mc_redstone_torch_at(frame, 234);
        lower = mc_redstone_torch_workspace(frame, 234);
        if (lower == NULL)
            return false;
        if (!mc_redstone_torch_child(frame, 234, lower,
                                     mc_world_mutation_particle(frame->world, particle, particle_x,
                                                                particle_y, particle_z, 0.0, 0.0,
                                                                0.0, lower)))
            return false;
    }
    return true;
}

bool mc_world_mutation_redstone_torch_tick(mc_world_runtime *world, const mc_block *block,
                                           int32_t x, int32_t y, int32_t z, mc_java_random *random,
                                           mc_world_mutation_error *error) {
    const mc_redstone_torch_frame frame = {world, block, error, MC_REDSTONE_TORCH_TICK};
    mc_redstone_torch_at(&frame, 6);
    mc_world_mutation_error *lower = mc_redstone_torch_workspace(&frame, 6);
    if (lower == NULL)
        return false;
    bool powered;
    if (!mc_redstone_torch_child(
            &frame, 6, lower,
            mc_world_mutation_redstone_torch_powered(world, block, x, y, z, &powered, lower)))
        return false;
    if (!prune(&frame))
        return false;
    mc_redstone_torch_at(&frame, 63);
    if (block->subtype.db.a) {
        if (powered) {
            if (!replace(&frame, true, x, y, z))
                return false;
            bool burned;
            if (!burnout(&frame, 104, x, y, z, true, &burned))
                return false;
            if (burned && !fizz(&frame, x, y, z, random))
                return false;
        }
    } else if (!powered) {
        bool burned;
        if (!burnout(&frame, 258, x, y, z, false, &burned))
            return false;
        if (!burned && !replace(&frame, false, x, y, z))
            return false;
    }
    return mc_redstone_torch_done(&frame);
}
