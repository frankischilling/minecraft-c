/* Actual null-target MC change on the SAME retained selected Load. No
 * renderer/effects World store occurs on this original branch. */
static bool source_app_slots(const mc_client_world_load_call *call) {
    const client_application *app = call->classes->app;
    const mc_client_world_leave_state *left = &call->state.leave;
    if ((!left->started && (left->i_cleared || left->f_cleared || left->h_cleared)) ||
        (left->h_cleared && !left->f_cleared) || (left->f_cleared && !left->i_cleared))
        return false;
    mc_entity *view = left->i_cleared ? NULL : call->state.player;
    mc_entity *player = left->h_cleared ? NULL : call->state.player;
    mc_world_runtime *world = left->f_cleared ? NULL : call->state.world;
    return call->i_occurrence == view && app->i == view && app->view_occurrence == view &&
           call->h_occurrence == player && app->h == player &&
           call->world_slot_owned == !left->f_cleared && app->active_world == world;
}

static bool previous_world_save(mc_client_world_load_call *call) {
    source_leave *left = call->active_leave;
    call->state.last_change_pc = 64;
    if (left == NULL || call->classes->app->active_world != call->state.world) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_STATE, "actual previous World native provenance",
                    64);
        return false;
    }
    return source_world_save(call, call->classes->app->active_world, &left->save, 64);
}

static mc_client_world_load_status leave_admission(mc_client_world_load_error *out,
                                                   mc_client_world_load_status status,
                                                   const char *operation) {
    if (out != NULL)
        *out = (mc_client_world_load_error){.status = status,
                                            .operation = operation,
                                            .site = {MC_CLASS, "a", "(Lfd;)V", 0},
                                            .native_policy = true};
    return status;
}

mc_client_world_load_status mc_client_world_load_leave(mc_client_world_load_call *call,
                                                       mc_client_world_load_error *out) {
    if (call == NULL)
        return leave_admission(out, MC_CLIENT_WORLD_LOAD_ARGUMENT, "source leave native owner");
    if (call->state.active || call->classes->active || call->classes->retiring ||
        call->state.retiring)
        return leave_admission(out, MC_CLIENT_WORLD_LOAD_BUSY,
                               "source leave active/retiring owner");
    if (!call->state.source_returned || call->failure.status != MC_CLIENT_WORLD_LOAD_OK ||
        call->classes->latest != call || call->state.world == NULL || call->state.player == NULL ||
        call->state.input == NULL || call->state.handler == NULL ||
        !call->state.player_load_returned || !source_app_slots(call))
        return leave_admission(out, MC_CLIENT_WORLD_LOAD_STATE,
                               "source leave completed selected ownership scope");
    if (call->state.leave.failed)
        return leave_admission(out, MC_CLIENT_WORLD_LOAD_BUSY,
                               "source leave retained failed invocation");
    if (call->leave_count >= 32)
        return leave_admission(out, MC_CLIENT_WORLD_LOAD_LIMIT,
                               "source leave retained invocation bound");
    source_leave *left = calloc(1, sizeof(*left));
    if (left == NULL)
        return leave_admission(out, MC_CLIENT_WORLD_LOAD_ALLOCATION,
                               "source leave retained invocation");
    left->next = call->leaves;
    call->leaves = left;
    ++call->leave_count;
    call->state.leave.calls = call->leave_count;
    call->state.leave.started = true;
    call->state.leave.source_returned = false;
    call->state.active = call->classes->active = true;
    call->active_leave = left;
    const mc_client_world_load_error loaded = call->failure;
    call->failure = (mc_client_world_load_error){0};
    /* Actual one-/two-argument wrappers supply the canonical empty literal and
     * NULL Player, then invoke the three-argument body, not native retirement. */
    const bool returned =
        change_world_at(call, NULL, mc_client_loading_classes_empty(call->classes->gui), 2);
    left->pc = call->state.last_change_pc;
    left->failure = call->failure;
    call->failure = loaded; /* Original successful load remains independently inspectable. */
    call->active_leave = NULL;
    call->state.active = call->classes->active = false;
    call->state.leave.source_returned = returned;
    call->state.leave.failed = !returned;
    if (out != NULL)
        *out = left->failure;
    return left->failure.status;
}
