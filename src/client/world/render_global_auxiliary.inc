/* Actual n.a(Player,IIIII)V selected effect arms. The World caller owns event
 * order; this receiver does not defer sounds or particles until after mutation. */
static bool auxiliary_registry(access_call *call, mc_registry_classes *registry,
                               mc_registry_status status, unsigned int pc) {
    if (status == MC_REGISTRY_OK)
        return true;
    mc_particle_throwable throwable;
    const char *exception = NULL;
    mc_particle_registry_classification(registry, &call->error.registry, &throwable, &exception);
    call->source_exception_class = throwable == MC_PARTICLE_SOURCE_EXCEPTION ? exception : NULL;
    fail(&call->error,
         status == MC_REGISTRY_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
         : status == MC_REGISTRY_ALLOCATION_FAILED
             ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
             : MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
         call->error.registry.message, "a", "(Lgs;IIIII)V", pc,
         throwable == MC_PARTICLE_NO_THROWABLE);
    return false;
}

static bool auxiliary_break_sound(mc_render_global_access *access, access_call *call,
                                  mc_registry_classes *registry, int32_t id, int32_t x, int32_t y,
                                  int32_t z) {
    const char *descriptor = "(Lgs;IIIII)V";
    const mc_block *block = NULL;
    if (!auxiliary_registry(call, registry,
                            mc_registry_block_get(registry, id, &block, &call->error.registry),
                            411))
        return false;
    client_application *app = access->owner->fields.t;
    if (app == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N auxiliary actual App.B receiver", "a", descriptor, 418, false);
        return false;
    }
    mc_sound_manager *manager = app->B; /* Captured before the StepSound arguments. */
    const mc_block_fields *fields = mc_block_fields_get(block);
    if (fields == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N auxiliary actual Block.by receiver", "a", descriptor, 423, false);
        return false;
    }
    if (!auxiliary_registry(call, registry,
                            mc_registry_step_sound_name(registry, fields->by, true,
                                                        &call->effect_name, &call->error.registry),
                            426) ||
        !fp(call, descriptor, 431))
        return false;
    volatile float sx = (float)x + 0.5f;
    volatile float sy = (float)y + 0.5f;
    volatile float sz = (float)z + 0.5f;
    /* Fresh by fields at448 and460 occur after the allocating name method. */
    fields = mc_block_fields_get(block);
    float volume;
    if (!auxiliary_registry(
            call, registry,
            mc_registry_step_sound_volume(fields->by, &volume, &call->error.registry), 451))
        return false;
    volatile float raised_volume = volume + 1.0f;
    volatile float scaled_volume = raised_volume / 2.0f;
    fields = mc_block_fields_get(block);
    float pitch;
    if (!auxiliary_registry(call, registry,
                            mc_registry_step_sound_pitch(fields->by, &pitch, &call->error.registry),
                            463))
        return false;
    volatile float scaled_pitch = pitch * 0.8f;
    if (manager == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N auxiliary captured SoundManager receiver", "a", descriptor, 469, false);
        return false;
    }
    mc_registry_string_view name;
    if (!mc_registry_string_read(call->effect_name.string, &name)) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
             "N auxiliary returned StepSound String metadata", "a", descriptor, 469, true);
        return false;
    }
    mc_sound_manager_report report = {0};
    const mc_sound_status status =
        mc_sound_manager_effect(manager, (mc_sound_text){name.units, name.length, false}, sx, sy,
                                sz, scaled_volume, scaled_pitch, &report, &call->error.sound);
    if (status != MC_SOUND_OK) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
             call->error.sound.operation, "a", descriptor, 469,
             status != MC_SOUND_SOURCE_EXCEPTION);
        call->error.child =
            (mc_client_graphics_site){"yo", "b", "(Ljava/lang/String;FFFFF)V", report.source_pc};
        return false;
    }
    call->auxiliary.sound_returned = true;
    auxiliary_count(&access->auxiliary, &access->auxiliary.break_sound_returns, 1);
    return true;
}

static bool auxiliary_destroy_particles(mc_render_global_access *access, access_call *call,
                                        int32_t x, int32_t y, int32_t z, int32_t id,
                                        int32_t metadata) {
    const char *descriptor = "(Lgs;IIIII)V";
    client_application *app = access->owner->fields.t; /* Fresh N.t473. */
    if (app == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N auxiliary actual App.j receiver", "a", descriptor, 476, false);
        return false;
    }
    mc_client_effect_renderer *receiver = app->effect_renderer;
    if (!budget(access, sizeof(access_particle))) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_LIMIT,
             "N auxiliary DN capture native byte limit", "a", descriptor, 476, true);
        return false;
    }
    access_particle *p = calloc(1, sizeof(*p));
    if (p == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
             "N auxiliary DN capture allocation", "a", descriptor, 476, true);
        return false;
    }
    call->particle = p;
    access->bytes += sizeof(*p);
    if (mc_client_application_render_borrow(app, &p->application_error) !=
        MC_CLIENT_APPLICATION_OWNER_OK) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_BUSY, "N auxiliary actual App lifetime", "a",
             descriptor, 476, true);
        return false;
    }
    p->application = app;
    if (receiver == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N auxiliary captured DN receiver", "a", descriptor, 499, false);
        return false;
    }
    const mc_effect_renderer_status held = mc_client_effect_renderer_retain(receiver);
    if (held != MC_EFFECT_RENDERER_OK) {
        fail(&call->error, effect_status(held), "N auxiliary captured DN native occurrence", "a",
             descriptor, 499, true);
        return false;
    }
    p->receiver = receiver;
    call->auxiliary.destruction_called = true;
    call->auxiliary.sound_before_destruction = call->auxiliary.sound_returned;
    auxiliary_count(&access->auxiliary, &access->auxiliary.destruction_calls, 1);
    const mc_effect_renderer_status status = mc_client_effect_renderer_destroy_block(
        receiver, app->registry, app->source_math_classes, x, y, z, id, metadata, &p->effects);
    /* Admission failure can report another caller's pending DN operation.
     * Only a returned newly allocated receipt grants this capture cleanup. */
    p->block_work = p->effects.block != NULL;
    call->auxiliary.destruction_status = (int)status;
    if (p->effects.block != NULL) {
        const mc_effect_block_state *block = p->effects.block;
        call->auxiliary.constructed = block->constructed;
        call->auxiliary.appended = block->appended;
        call->auxiliary.destruction_pc = block->source_pc;
        call->auxiliary.destruction_returned = block->returned;
        auxiliary_count(&access->auxiliary, &access->auxiliary.destruction_constructed,
                        (uint64_t)block->constructed);
        auxiliary_count(&access->auxiliary, &access->auxiliary.destruction_appended,
                        (uint64_t)block->appended);
        if (block->returned)
            auxiliary_count(&access->auxiliary, &access->auxiliary.destruction_returns, 1);
    }
    call->error.effects = &p->effects;
    if (status != MC_EFFECT_RENDERER_OK) {
        call->source_exception_class =
            p->effects.source_exception ? p->effects.exception_class : NULL;
        fail(&call->error, effect_status(status), p->effects.operation, "a", descriptor, 499,
             p->effects.native_policy);
        call->error.child =
            (mc_client_graphics_site){"dn", p->effects.source_method, p->effects.source_descriptor,
                                      (int32_t)p->effects.source_pc};
        return false;
    }
    return true;
}

static bool auxiliary_fizz(mc_render_global_access *access, access_call *call,
                           mc_registry_classes *registry, mc_java_random *random, int32_t x,
                           int32_t y, int32_t z) {
    const char *descriptor = "(Lgs;IIIII)V";
    mc_world_runtime *receiver = access->owner->fields.k; /* Captured k614. */
    if (!fp(call, descriptor, 618))
        return false;
    volatile float sx = (float)x + 0.5f;
    volatile float sy = (float)y + 0.5f;
    volatile float sz = (float)z + 0.5f;
    const mc_registry_string *name = NULL;
    if (!auxiliary_registry(call, registry,
                            mc_registry_runtime_literal_ascii(registry, "random.fizz", &name,
                                                              &call->error.registry),
                            637))
        return false;
    if (random == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N auxiliary captured Random receiver", "a", descriptor, 645, false);
        return false;
    }
    const float first = mc_java_random_next_float(random);
    const float second = mc_java_random_next_float(random);
    volatile float difference = first - second;
    volatile float scaled = difference * 0.8f;
    volatile float pitch = 2.6f + scaled;
    if (receiver == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N auxiliary captured World sound receiver", "a", descriptor, 658, false);
        return false;
    }
    if (!budget(access, sizeof(mc_world_mutation_error))) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_LIMIT,
             "N auxiliary reached World sound diagnostic ceiling", "a", descriptor, 658, true);
        return false;
    }
    call->world_effect = calloc(1, sizeof(*call->world_effect));
    if (call->world_effect == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
             "N auxiliary reached World sound diagnostic allocation", "a", descriptor, 658, true);
        return false;
    }
    access->bytes += sizeof(*call->world_effect);
    call->error.auxiliary_world = call->world_effect;
    const mc_world_storage_status status = mc_world_runtime_sound(
        receiver, (double)sx, (double)sy, (double)sz, name, 0.5f, pitch, call->world_effect);
    if (status != MC_WORLD_STORAGE_OK) {
        const char *exception = NULL;
        const bool source = mc_world_access_failure_source_exception(
            &call->world_effect->world_access_failure, &exception);
        call->source_exception_class = source ? exception : NULL;
        call->error.world = call->world_effect->cause;
        fail(&call->error,
             status == MC_WORLD_STORAGE_BUSY    ? MC_CLIENT_RENDER_GLOBAL_BUSY
             : status == MC_WORLD_STORAGE_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
             : status == MC_WORLD_STORAGE_ALLOCATION_FAILED
                 ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                 : MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
             call->world_effect->operation, "a", descriptor, 658, !source);
        call->error.child =
            (mc_client_graphics_site){"fd", "a", "(DDDLjava/lang/String;FF)V",
                                      (int32_t)call->world_effect->world_access_bytecode};
        return false;
    }
    return true;
}

static mc_world_storage_status auxiliary(void *context, mc_world_runtime *world, mc_entity *player,
                                         int32_t effect, int32_t x, int32_t y, int32_t z,
                                         int32_t data, mc_world_storage_error *error,
                                         mc_world_access_failure *failure) {
    (void)world;
    (void)player; /* These two source arms do not read the nullable argument. */
    const char *descriptor = "(Lgs;IIIII)V";
    mc_render_global_access *access = context;
    access_call *call = call_begin(access, descriptor, error, failure);
    if (call == NULL)
        return error->status;
    if (call->error.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return call_end(access, call, error, failure);
    if (effect == 2001) {
        call->auxiliary2001 = true;
        auxiliary_count(&access->auxiliary, &access->auxiliary.event2001_entered, 1);
        call->auxiliary =
            (mc_render_global_auxiliary_event){.sequence = access->auxiliary.event2001_entered,
                                               .x = x,
                                               .y = y,
                                               .z = z,
                                               .data = data,
                                               .block_id = data & 255,
                                               .metadata = mc_java_i32_shr(data, 8) & 255};
    }
    mc_world_runtime *captured_world = access->owner->fields.k;
    if (captured_world == NULL) {
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
             "N auxiliary actual World.r receiver", "a", descriptor, 4, false);
        return call_end(access, call, error, failure);
    }
    mc_java_random *random = captured_world->fields.r; /* Captured even for2001/default. */
    mc_registry_classes *registry = captured_world->classes->registry;
    bool returned = false;
    if (effect == 2001) {
        const int32_t id = data & 255;
        returned =
            (id <= 0 || auxiliary_break_sound(access, call, registry, id, x, y, z)) &&
            auxiliary_destroy_particles(access, call, x, y, z, id, mc_java_i32_shr(data, 8) & 255);
    } else if (effect == 1004)
        returned = auxiliary_fizz(access, call, registry, random, x, y, z);
    else if (effect != 1000 && effect != 1001 && effect != 1002 && effect != 1003 &&
             effect != 1005 && effect != 2000)
        returned = true; /* Actual default switch arm goes directly to716. */
    else
        fail(&call->error, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
             "N reached auxiliary effect source arm", "a", descriptor,
             effect == 1000   ? 123u
             : effect == 1001 ? 101u
             : effect == 1002 ? 146u
             : effect == 1003 ? 505u
             : effect == 1005 ? 664u
                              : 224u,
             true);
    if (returned) {
        call->error.source_returned = true;
        if (call->auxiliary2001)
            auxiliary_count(&access->auxiliary, &access->auxiliary.event2001_returned, 1);
    }
    return call_end(access, call, error, failure);
}
