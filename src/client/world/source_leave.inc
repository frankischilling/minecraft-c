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

static mc_chunk_callback_result leave_progress(void *owner, const mc_world_save_context *context,
                                               mc_world_save_stage stage) {
    (void)context;
    mc_client_world_load_call *call = owner;
    source_leave *left = call->active_leave;
    const bool level = stage == MC_WORLD_SAVE_LEVEL;
    const int pc = level ? 20 : 36;
    mc_client_loading_error *error = level ? &left->level : &left->chunks;
    mc_player_string *text = NULL;
    if (!level) {
        if (call->classes->saving_chunks == NULL) {
            static const uint16_t units[] = {'S', 'a', 'v', 'i', 'n', 'g', ' ',
                                             'c', 'h', 'u', 'n', 'k', 's'};
            const mc_player_status created = mc_player_string_create(
                units, sizeof(units) / sizeof(*units), &call->classes->saving_chunks);
            if (created != MC_PLAYER_OK) {
                change_fail(call,
                            created == MC_PLAYER_LIMIT ? MC_CLIENT_WORLD_LOAD_LIMIT
                                                       : MC_CLIENT_WORLD_LOAD_ALLOCATION,
                            "FD Saving chunks native literal owner", 64);
                call->failure.child = (mc_client_world_load_site){"fd", "a", "(ZLyb;)V", 34};
                return MC_CHUNK_CALLBACK_FATAL;
            }
        }
        text = call->classes->saving_chunks;
    }
    /* Concrete mk.b ignores its String operand, sets e, then reloads its own c.
     * No surrogate progress result or eagerly substituted title is supplied. */
    if (!loading(call, left->progress, text, level ? 2 : 0, "a", CHANGE_DESC, 64, error)) {
        call->failure.child = (mc_client_world_load_site){"fd", "a", "(ZLyb;)V", pc};
        return MC_CHUNK_CALLBACK_FATAL;
    }
    if (level)
        left->level_returned = true;
    else
        left->chunks_returned = true;
    return MC_CHUNK_CALLBACK_OK;
}

static bool previous_world_save(mc_client_world_load_call *call) {
    client_application *app = call->classes->app;
    source_leave *left = call->active_leave;
    call->state.last_change_pc = 64;
    if (left == NULL || app->active_world != call->state.world) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_STATE, "actual previous World native provenance",
                    64);
        return false;
    }
    /* MC reloads s at61 and passes that SAME nullable yb for both FD stages. */
    mc_client_loading_screen *progress = app->loading_screen;
    if (progress != NULL) {
        if (mc_client_loading_screen_retain(progress) != MC_CLIENT_LOADING_OK) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "captured FD progress argument", 64);
            return false;
        }
        left->progress = progress;
    }
    const mc_chunk_lifecycle_status saved = mc_world_runtime_save_with_progress_detailed(
        app->active_world, true, call, progress == NULL ? NULL : leave_progress, &left->saving);
    if (saved != MC_CHUNK_LIFECYCLE_OK) {
        call->failure.saving = &left->saving;
        call->failure.world = &left->saving.cause;
        if (call->failure.status == MC_CLIENT_WORLD_LOAD_OK) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, left->saving.cause.operation, 64);
            const mc_world_source_error *source =
                left->saving.save.source_chunk.source.status != MC_WORLD_SOURCE_OK
                    ? &left->saving.save.source_chunk.source
                : left->saving.save.metadata.source_handler.status != MC_WORLD_SOURCE_OK
                    ? &left->saving.save.metadata.source_handler
                    : &left->saving.source_session;
            if (source->frame_count != 0)
                call->failure.child = (mc_client_world_load_site){
                    source->frames[0].class_name, source->frames[0].method,
                    source->frames[0].descriptor, source->frames[0].pc};
            else
                call->failure.child = (mc_client_world_load_site){"fd", "a", "(ZLyb;)V",
                                                                  left->chunks_returned ? 47 : 26};
            if (source->status != MC_WORLD_SOURCE_OK)
                call->failure.native_policy = source->native_policy;
        }
        return false;
    }
    left->prefix.previous_save_returned = true;
    if (left->progress != NULL) {
        if (mc_client_loading_screen_release(left->progress) != MC_CLIENT_LOADING_OK) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                        "completed FD progress argument native release", 64);
            return false;
        }
        left->progress = NULL;
    }
    return true;
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
