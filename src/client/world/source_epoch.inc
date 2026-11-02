/* Native retained invocation epochs around genuine subsequent source loads. */
static bool epoch_admit(const mc_client_world_load_call *previous) {
    return previous != NULL && previous->classes->latest == previous &&
           previous->classes->calls != SIZE_MAX && previous->successor == NULL &&
           !previous->state.active && !previous->state.retiring &&
           previous->state.source_returned && previous->failure.status == MC_CLIENT_WORLD_LOAD_OK &&
           previous->state.leave.source_returned && !previous->state.leave.failed &&
           previous->state.leave.i_cleared && previous->state.leave.f_cleared &&
           previous->state.leave.h_cleared && source_app_slots(previous);
}

static bool source_epoch_slots(const mc_client_world_load_call *call) {
    if (!call->state.superseded)
        return source_app_slots(call);
    const mc_client_world_load_call *next = call->successor;
    return next != NULL && next->previous == call && next->classes == call->classes &&
           next->state.source_returned && next->failure.status == MC_CLIENT_WORLD_LOAD_OK &&
           next->state.world != NULL && next->state.world != call->state.world &&
           call->state.leave.source_returned && !call->state.leave.failed &&
           call->state.leave.i_cleared && call->state.leave.f_cleared &&
           call->state.leave.h_cleared && call->h_occurrence == NULL &&
           call->i_occurrence == NULL && !call->world_slot_owned && !call->effects_world_owned &&
           !call->effects_held && call->effects_owner == NULL;
}

static bool prepare_previous(mc_client_world_load_call *previous,
                             mc_client_world_load_call *current) {
    /* Completed source-call locals can end before a later actual invocation.
     * This does not run N.a(NULL), detach N/DN, retire a World, alter source
     * renderer admission, or release any global dispatcher source field. */
    previous->state.active = true;
    previous->cleanup = (mc_client_world_load_error){0};
    if (cleanup_records(previous) != MC_CLIENT_WORLD_LOAD_OK) {
        previous->state.active = false;
        current->classes->active = true;
        fail(current, MC_CLIENT_WORLD_LOAD_BUSY, previous->cleanup.operation, "a", LOAD_DESC, -1);
        current->failure.render_global = previous->cleanup.render_global;
        current->failure.renderer = previous->cleanup.renderer;
        current->failure.loading = previous->cleanup.loading;
        current->failure.terrain = previous->cleanup.terrain;
        return false;
    }
    previous->state.active = false;
    if (previous->effects_held) {
        if (mc_client_effect_renderer_release(previous->effects_owner) != MC_EFFECT_RENDERER_OK) {
            fail(current, MC_CLIENT_WORLD_LOAD_BUSY, "completed prior DN receiver native release",
                 "a", LOAD_DESC, -1);
            return false;
        }
        previous->effects_held = false;
    }
    return true;
}

mc_client_world_load_status mc_client_world_load_begin_after(mc_client_world_load_call *previous,
                                                             mc_world_info_name *folder,
                                                             mc_world_info_name *display,
                                                             int64_t seed,
                                                             mc_client_world_load_call **out) {
    if (previous == NULL)
        return MC_CLIENT_WORLD_LOAD_ARGUMENT;
    return load_begin(previous->classes, previous, folder, display, seed, out);
}

mc_client_world_load_status mc_client_world_load_terminal_records(mc_client_world_load_call *call) {
    if (call == NULL)
        return MC_CLIENT_WORLD_LOAD_ARGUMENT;
    if (call->state.active || call->classes->active)
        return MC_CLIENT_WORLD_LOAD_BUSY;
    if (call->state.retiring && !call->state.terminal_prepare_selected)
        return MC_CLIENT_WORLD_LOAD_BUSY;
    const mc_client_world_load_status ended =
        mc_client_world_load_classes_terminal_end(call->classes);
    if (ended != MC_CLIENT_WORLD_LOAD_OK)
        return ended;
    call->state.retiring = call->state.terminal_prepare_selected = true;
    call->state.active = call->classes->active = true;
    call->cleanup = (mc_client_world_load_error){0};
    if (cleanup_records(call) != MC_CLIENT_WORLD_LOAD_OK)
        return call->cleanup.status;
    call->state.active = call->classes->active = false;
    return MC_CLIENT_WORLD_LOAD_OK;
}
