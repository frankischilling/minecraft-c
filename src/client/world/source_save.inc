/* Shared actual fd.a(yb) join for new-World save and ordinary leave. */
static mc_chunk_callback_result
source_save_progress(void *owner, const mc_world_save_context *context, mc_world_save_stage stage) {
    (void)context;
    source_save *save = owner;
    mc_client_world_load_call *call = save->call;
    const bool level = stage == MC_WORLD_SAVE_LEVEL;
    const int pc = level ? 20 : 36;
    mc_client_loading_error *error = level ? &save->level : &save->chunks;
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
                            "FD Saving chunks native literal owner", save->caller_pc);
                call->failure.child = (mc_client_world_load_site){"fd", "a", "(ZLyb;)V", 34};
                return MC_CHUNK_CALLBACK_FATAL;
            }
        }
        text = call->classes->saving_chunks;
    }
    /* Concrete mk.b ignores its String operand, sets e, then reloads its own c.
     * No surrogate progress result or eagerly substituted title is supplied. */
    if (!loading(call, save->progress, text, level ? 2 : 0, "a", CHANGE_DESC, save->caller_pc,
                 error)) {
        call->failure.child = (mc_client_world_load_site){"fd", "a", "(ZLyb;)V", pc};
        return MC_CHUNK_CALLBACK_FATAL;
    }
    if (level)
        save->level_returned = true;
    else
        save->chunks_returned = true;
    return MC_CHUNK_CALLBACK_OK;
}

static bool source_world_save(mc_client_world_load_call *call, mc_world_runtime *world,
                              source_save *save, int caller_pc) {
    save->call = call;
    save->caller_pc = caller_pc;
    call->state.last_change_pc = caller_pc;
    /* The caller reloads s at61 or336 and passes the SAME nullable yb.
     * FD.a(yb) invokes its full forced-save body with this actual argument. */
    mc_client_loading_screen *progress = call->classes->app->loading_screen;
    if (progress != NULL) {
        if (mc_client_loading_screen_retain(progress) != MC_CLIENT_LOADING_OK) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "captured FD progress argument",
                        caller_pc);
            return false;
        }
        save->progress = progress;
    }
    const mc_chunk_lifecycle_status saved = mc_world_runtime_save_with_progress_detailed(
        world, true, save, progress == NULL ? NULL : source_save_progress, &save->saving);
    if (saved != MC_CHUNK_LIFECYCLE_OK) {
        call->failure.saving = &save->saving;
        call->failure.world = &save->saving.cause;
        if (call->failure.status == MC_CLIENT_WORLD_LOAD_OK) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, save->saving.cause.operation,
                        caller_pc);
            const mc_world_source_error *source =
                save->saving.save.source_chunk.source.status != MC_WORLD_SOURCE_OK
                    ? &save->saving.save.source_chunk.source
                : save->saving.save.metadata.source_handler.status != MC_WORLD_SOURCE_OK
                    ? &save->saving.save.metadata.source_handler
                    : &save->saving.source_session;
            if (source->frame_count != 0)
                call->failure.child = (mc_client_world_load_site){
                    source->frames[0].class_name, source->frames[0].method,
                    source->frames[0].descriptor, source->frames[0].pc};
            else
                call->failure.child = (mc_client_world_load_site){"fd", "a", "(ZLyb;)V",
                                                                  save->chunks_returned ? 47 : 26};
            if (source->status != MC_WORLD_SOURCE_OK)
                call->failure.native_policy = source->native_policy;
        }
        return false;
    }
    if (save->progress != NULL) {
        if (mc_client_loading_screen_release(save->progress) != MC_CLIENT_LOADING_OK) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                        "completed FD progress argument native release", caller_pc);
            return false;
        }
        save->progress = NULL;
    }
    return true;
}
