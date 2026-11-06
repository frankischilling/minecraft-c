/* Client xr placement and activation. The face predicate does not delegate to
 * the independently virtual base predicate, and neither checks replaceability. */
static bool lever_base_predicate(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                                 int32_t y, int32_t z, bool *out) {
    const int32_t xs[] = {mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x, x};
    const int32_t ys[] = {y, y, y, y, mc_java_i32_sub(y, 1)};
    const int32_t zs[] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1), z};
    const unsigned int calls[] = {7, 22, 37, 52, 67};
    const unsigned int returns[] = {14, 29, 44, 59, 74};
    bool value = false;
    unsigned int returned = 76;
    for (size_t i = 0; i < 5; ++i) {
        e->site.pc = e->base.pc = calls[i];
        if (!torch_solid(e, world, xs[i], ys[i], zs[i], &value))
            return false;
        if (value) {
            returned = returns[i];
            break;
        }
    }
    e->site.pc = e->base.pc = returned;
    e->base_returned = e->predicate_returned = true;
    e->base_value = *out = value;
    return true;
}

static bool lever_face_predicate(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                                 int32_t y, int32_t z, int32_t face, bool *out) {
    e->site = site("xr", "a", "(Lfd;IIII)Z", 106);
    e->predicate_entered = true;
    bool value = false;
    if (face >= 1 && face <= 5) {
        const int32_t xs[] = {x, x, x, mc_java_i32_add(x, 1), mc_java_i32_sub(x, 1)};
        const int32_t ys[] = {mc_java_i32_sub(y, 1), y, y, y, y};
        const int32_t zs[] = {z, mc_java_i32_add(z, 1), mc_java_i32_sub(z, 1), z, z};
        const size_t i = (size_t)(face - 1);
        e->site.pc = 13u + 21u * (unsigned int)i;
        if (!torch_solid(e, world, xs[i], ys[i], zs[i], &value))
            return false;
        if (value)
            e->site.pc = 20u + 21u * (unsigned int)i;
        else
            e->site.pc = 106;
    }
    e->predicate_returned = true;
    *out = value;
    return true;
}

typedef enum lever_child {
    LEVER_METADATA,
    LEVER_RANGE,
    LEVER_SOUND,
    LEVER_NEIGHBORS,
    LEVER_DROP,
    LEVER_REMOVE
} lever_child;

/* One successful child has no failed producer to retain. A failed child stays
 * intact, including all receipts, until the enclosing placement is retired. */
static bool lever_child_call(mc_world_placement_error *e, const mc_block *block,
                             mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                             int32_t argument, lever_child child) {
    if (!hold_world(e, world))
        return false;
    if (e->work->mutation == NULL) {
        e->work->mutation = malloc(sizeof(*e->work->mutation));
        if (e->work->mutation == NULL) {
            fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Lever retained mutation diagnostic", true);
            return false;
        }
        e->mutation = e->work->mutation;
    }
    mc_world_mutation_error *lower = e->work->mutation;
    mc_world_mutation_error_init(lower);
    mc_world_storage_error admission = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &admission);
    if (status != MC_WORLD_STORAGE_OK) {
        fail(e, status, admission.operation, true);
        e->cause = admission;
        return false;
    }
    bool completed = false, ignored;
    switch (child) {
        case LEVER_METADATA:
            completed = mc_world_runtime_metadata_set(world, x, y, z, argument, lower) ==
                        MC_WORLD_STORAGE_OK;
            break;
        case LEVER_RANGE:
            completed = mc_world_mutation_observe_range(world, x, y, z, x, y, z, lower);
            break;
        case LEVER_SOUND: {
            const mc_registry_string *name = NULL;
            e->site.pc = 81;
            if (mc_registry_runtime_literal_ascii(e->work->registry, "random.click", &name,
                                                  &e->registry) != MC_REGISTRY_OK) {
                registry_failed(e);
                break;
            }
            e->site.pc = 97;
            completed =
                mc_world_mutation_sound(world, (double)x + 0.5, (double)y + 0.5, (double)z + 0.5,
                                        name, 0.3f, argument > 0 ? 0.6f : 0.5f, lower);
            break;
        }
        case LEVER_NEIGHBORS:
            completed = mc_world_mutation_neighbors(world, x, y, z, block->fields.bn, lower);
            break;
        case LEVER_DROP:
            completed = mc_world_mutation_drop(world, block, x, y, z, argument, lower);
            break;
        case LEVER_REMOVE:
            completed = mc_world_runtime_block_set(world, x, y, z, 0, &ignored, lower) ==
                        MC_WORLD_STORAGE_OK;
            break;
    }
    mc_world_runtime_call_leave(world);
    if (!completed && e->status == MC_WORLD_STORAGE_OK) {
        fail(e, lower->status, lower->operation, true);
        e->cause = lower->cause;
        e->registry = lower->registry;
        if (e->registry.status != MC_REGISTRY_OK)
            registry_failed_in(e, mc_world_runtime_registry(world));
    }
    if (!capture(e)) {
        if (e->status == MC_WORLD_STORAGE_OK)
            fail(e, e->cleanup.status, e->cleanup.operation, true);
        return false;
    }
    if (!completed)
        classify_receipts(e);
    return completed;
}

static bool lever_placed_face(mc_world_placement_error *e, const mc_block *block,
                              mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                              int32_t face) {
    e->site = site("xr", "e", "(Lfd;IIII)V", 5);
    int32_t metadata;
    if (!value_get(e, world, 0, x, y, z, true, &metadata))
        return false;
    const int32_t powered = (int32_t)((uint32_t)metadata & 8u);
    int32_t orientation = -1;
    if (face >= 1 && face <= 5) {
        const int32_t xs[] = {x, x, x, mc_java_i32_add(x, 1), mc_java_i32_sub(x, 1)};
        const int32_t ys[] = {mc_java_i32_sub(y, 1), y, y, y, y};
        const int32_t zs[] = {z, mc_java_i32_add(z, 1), mc_java_i32_sub(z, 1), z, z};
        const unsigned int pcs[] = {40, 71, 93, 115, 137};
        const size_t i = (size_t)(face - 1);
        e->site.pc = pcs[i];
        bool supported;
        if (!torch_solid(e, world, xs[i], ys[i], zs[i], &supported))
            return false;
        if (supported) {
            if (face == 1) {
                e->site.pc = 52;
                if (world->fields.r == NULL) {
                    null_reference(e, "Lever actual World Random nextInt null receiver");
                    return false;
                }
                int32_t choice;
                (void)mc_java_random_next_bounded(world->fields.r, 2, &choice);
                orientation = 5 + choice;
            } else {
                orientation = 6 - face;
            }
        }
    }
    if (orientation == -1) {
        e->site.pc = 163;
        if (!value_get(e, world, 1, x, y, z, true, &metadata))
            return false;
        e->site.pc = 166;
        if (!lever_child_call(e, block, world, x, y, z, metadata, LEVER_DROP))
            return false;
        e->site.pc = 175;
        if (!lever_child_call(e, block, world, x, y, z, 0, LEVER_REMOVE))
            return false;
        e->site.pc = 179;
        return true;
    }
    e->site.pc = 190;
    if (!lever_child_call(e, block, world, x, y, z, orientation + powered, LEVER_METADATA))
        return false;
    e->site.pc = 193;
    return true;
}

static bool lever_activate(mc_world_placement_error *e, const mc_block *block,
                           mc_world_runtime *world, int32_t x, int32_t y, int32_t z) {
    e->site = site("xr", "a", "(Lfd;IIILgs;)Z", 1);
    if (!hold_world(e, world))
        return false;
    e->remote_read = true;
    e->remote = world->fields.B;
    if (e->remote) {
        e->site.pc = 8;
        return true;
    }
    e->site.pc = 14;
    int32_t metadata;
    if (!value_get(e, world, 0, x, y, z, true, &metadata))
        return false;
    const int32_t orientation = (int32_t)((uint32_t)metadata & 7u);
    const int32_t powered = 8 - (int32_t)((uint32_t)metadata & 8u);
    e->site.pc = 46;
    if (!lever_child_call(e, block, world, x, y, z, orientation + powered, LEVER_METADATA))
        return false;
    e->site.pc = 58;
    if (!lever_child_call(e, block, world, x, y, z, 0, LEVER_RANGE))
        return false;
    if (!lever_child_call(e, block, world, x, y, z, powered, LEVER_SOUND))
        return false;
    e->site.pc = 109;
    if (!lever_child_call(e, block, world, x, y, z, 0, LEVER_NEIGHBORS))
        return false;
    const int32_t xs[] = {mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x};
    const int32_t zs[] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1)};
    const unsigned int pcs[] = {129, 152, 175, 198};
    e->site.pc = 215;
    const bool wall = orientation >= 1 && orientation <= 4;
    if (wall)
        e->site.pc = pcs[(size_t)(orientation - 1)];
    if (!lever_child_call(e, block, world, wall ? xs[(size_t)(orientation - 1)] : x,
                          wall ? y : mc_java_i32_sub(y, 1),
                          wall ? zs[(size_t)(orientation - 1)] : z, 0, LEVER_NEIGHBORS))
        return false;
    e->site.pc = 219;
    return true;
}
