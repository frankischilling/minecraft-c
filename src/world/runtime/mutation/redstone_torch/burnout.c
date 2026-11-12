#include "redstone_torch_internal.h"

static bool append(const mc_redstone_torch_frame *frame, int32_t x, int32_t y, int32_t z) {
    /* List receiver is evaluated before new XU and World.t. Keep that exact
     * reference, including NULL, until the eventual invokeinterface23. */
    mc_registry_redstone_torch_list_ref list;
    if (!mc_redstone_torch_list(frame, 5, &list))
        return false;
    mc_registry_redstone_torch_record *record = NULL;
    mc_registry_redstone_torch_error collection;
    mc_redstone_torch_at(frame, 8);
    (void)mc_registry_redstone_torch_record_new(frame->block->classes, &record, &collection);
    if (!mc_redstone_torch_collection(frame, 8, &collection, "Redstone Torch new XU storage"))
        return false;
    int64_t time;
    bool complete = mc_redstone_torch_time(frame, 17, &time);
    if (complete) {
        mc_redstone_torch_at(frame, 20);
        (void)mc_registry_redstone_torch_record_construct(record, x, y, z, time, &collection);
        complete = mc_redstone_torch_collection(frame, 20, &collection,
                                                "Redstone Torch actual XU constructor stores");
    }
    if (complete) {
        mc_redstone_torch_at(frame, 23);
        (void)mc_registry_redstone_torch_list_add(&list, record, &collection);
        complete = mc_redstone_torch_collection(frame, 23, &collection,
                                                "Redstone Torch actual captured List add");
    }
    mc_registry_redstone_torch_record_release(record);
    return complete;
}

bool mc_world_mutation_redstone_torch_burnout(mc_world_runtime *world, const mc_block *block,
                                              int32_t x, int32_t y, int32_t z, bool record,
                                              bool *out, mc_world_mutation_error *error) {
    const mc_redstone_torch_frame frame = {world, block, error, MC_REDSTONE_TORCH_BURNOUT};
    if (record && !append(&frame, x, y, z))
        return false;
    int32_t count = 0;
    for (int32_t index = 0;; index = mc_java_i32_add(index, 1)) {
        mc_registry_redstone_torch_list_ref list;
        mc_registry_redstone_torch_error collection;
        if (!mc_redstone_torch_list(&frame, 37, &list))
            return false;
        int32_t size;
        mc_redstone_torch_at(&frame, 40);
        (void)mc_registry_redstone_torch_list_size(&list, &size, &collection);
        if (!mc_redstone_torch_collection(&frame, 40, &collection,
                                          "Redstone Torch burnout actual List size"))
            return false;
        if (index >= size)
            break;
        if (!mc_redstone_torch_list(&frame, 48, &list))
            return false;
        mc_registry_redstone_torch_record *entry;
        mc_redstone_torch_at(&frame, 53);
        (void)mc_registry_redstone_torch_list_get(&list, index, &entry, &collection);
        if (!mc_redstone_torch_collection(&frame, 53, &collection,
                                          "Redstone Torch burnout actual List get"))
            return false;
        mc_redstone_torch_at(&frame, 65);
        if (entry == NULL)
            return mc_redstone_torch_null(&frame, 65,
                                          "Redstone Torch actual null XU coordinate read");
        mc_registry_redstone_torch_record_view view;
        const mc_registry_status status = mc_registry_redstone_torch_record_inspect(entry, &view);
        if (status != MC_REGISTRY_OK)
            return mc_redstone_torch_fail(&frame, 65, mc_world_runtime_registry_status(status),
                                          "Redstone Torch reached XU coordinate storage");
        if (view.x != x)
            continue;
        mc_redstone_torch_at(&frame, 74);
        if (view.y != y)
            continue;
        mc_redstone_torch_at(&frame, 83);
        if (view.z != z)
            continue;
        count = mc_java_i32_add(count, 1);
        if (count >= 8) {
            *out = true;
            return mc_redstone_torch_done(&frame);
        }
    }
    *out = false;
    return mc_redstone_torch_done(&frame);
}
