/* Real generation leases retain Display identity without retaining the raw
 * resource descriptor which native retirement can free. Source deletion is
 * separate: its later body may revoke live/deletion_right despite a lease. */
static bool source_generation_leased(const mc_client_display *display, uint64_t generation) {
    for (const mc_client_display_texture_lease *lease = display->source.texture_leases;
         lease != NULL; lease = lease->next)
        if (lease->generation == generation) return true;
    return false;
}

static mc_client_texture_resource *source_live_texture(mc_client_display *display,
                                                       mc_client_texture id, uint64_t generation,
                                                       bool exact_generation) {
    for (size_t i = 0; i < display->texture_count; ++i) {
        mc_client_texture_resource *resource = display->textures[i].resource;
        if (resource != NULL && resource->id == id && resource->live && resource->driver_generated &&
            (!exact_generation || resource->generation == generation)) return resource;
    }
    return NULL;
}

bool mc_client_display_source_bind_id(mc_client_display *display, mc_client_texture id,
                                      mc_client_display_error *error) {
    if (!source_enter(display, "source numeric texture bind", error)) return false;
    mc_client_texture_resource *resource = source_live_texture(display, id, 0, false);
    bool ok;
    if (resource == NULL) {
        ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source numeric texture bind",
                  "deleted/unregistered/uncertain name path", 0);
    } else {
        display->source.internal_resource_bind = true;
        ok = mc_client_display_bind_texture(resource, error);
        display->source.internal_resource_bind = false;
        display->source.texture_binding_known = ok;
        if (ok) {
            display->source.bound_texture = resource->id;
            display->source.bound_texture_generation = resource->generation;
        }
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_texture_lease_acquire(mc_client_texture_resource *resource,
                                           mc_client_display_texture_lease **out,
                                           mc_client_display_error *error) {
    if (out == NULL || resource_slot(resource) == NULL || resource->id == 0 ||
        !resource->live || !resource->driver_generated)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "font generation lease",
                    "actual live generated resource required", 0);
    mc_client_display *display = resource->display;
    if (resource->active || display->texture_resource_calls != 0 || display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "font generation lease", "resource call active", 0);
    if (display->source.texture_lease_count == SIZE_MAX)
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, "font generation lease", "hold limit", 0);
    mc_client_display_texture_lease *lease = calloc(1, sizeof(*lease));
    if (lease == NULL)
        return fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "font generation lease", "allocation failed", 0);
    lease->display = display;
    lease->generation = resource->generation;
    lease->id = resource->id;
    lease->next = display->source.texture_leases;
    display->source.texture_leases = lease;
    ++display->source.texture_lease_count;
    *out = lease;
    return success(error);
}

bool mc_client_display_texture_lease_begin(mc_client_display_texture_lease *lease,
                                          mc_client_display_error *error) {
    if (lease == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "begin font generation draw", "null lease", 0);
    if (lease->active || lease->display->source.active_calls != 0 || lease->display->texture_resource_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "begin font generation draw", "draw/resource active", 0);
    lease->active = true;
    return success(error);
}

bool mc_client_display_texture_lease_end(mc_client_display_texture_lease *lease,
                                        mc_client_display_error *error) {
    if (lease == NULL || !lease->active)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "end font generation draw", "no draw active", 0);
    lease->active = false;
    return success(error);
}

bool mc_client_display_texture_lease_bind(mc_client_display_texture_lease *lease,
                                        mc_client_display_error *error) {
    if (lease == NULL || !lease->active)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "leased source texture bind", "no generation draw active", 0);
    mc_client_display *display = lease->display;
    if (!source_enter(display, "leased source texture bind", error)) return false;
    mc_client_texture_resource *resource = source_live_texture(display, lease->id, lease->generation, true);
    bool ok;
    if (resource == NULL) {
        ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "leased source texture bind",
                  "deleted/unregistered/reused generation path", 0);
    } else {
        display->source.internal_resource_bind = true;
        ok = mc_client_display_bind_texture(resource, error);
        display->source.internal_resource_bind = false;
        display->source.texture_binding_known = ok;
        if (ok) {
            display->source.bound_texture = resource->id;
            display->source.bound_texture_generation = resource->generation;
        }
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_texture_lease_release(mc_client_display_texture_lease **slot,
                                           mc_client_display_error *error) {
    if (slot == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "release font generation lease", "null slot", 0);
    mc_client_display_texture_lease *lease = *slot;
    if (lease == NULL) return success(error);
    mc_client_display *display = lease->display;
    if (lease->active || display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "release font generation lease", "draw active", 0);
    mc_client_display_texture_lease **link = &display->source.texture_leases;
    while (*link != lease) link = &(*link)->next;
    *link = lease->next;
    --display->source.texture_lease_count;
    free(lease);
    *slot = NULL;
    return success(error);
}
