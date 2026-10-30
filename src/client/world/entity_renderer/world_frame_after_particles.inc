/* Private continuation of the same retained px.a(FJ)V invocation. */
static void particle_views_expired(mc_effect_draw_state *state) {
    state->owner = NULL;
    state->camera = NULL;
    state->current = NULL;
    state->nw = NULL;
    state->failure = state->cleanup = NULL;
}

static bool particle_result(mc_client_world_frame_call *call, mc_effect_draw_status status,
                            const mc_effect_draw_error *error, unsigned int pc) {
    if (status == MC_EFFECT_DRAW_OK)
        return true;
    call->failure.particles = error;
    call->failure.child = error->child;
    call->failure.site = error->site;
    call->failure.status =
        status == MC_EFFECT_DRAW_SOURCE_NULL      ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
        : status == MC_EFFECT_DRAW_UNSUPPORTED_FP ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
        : status == MC_EFFECT_DRAW_DEPENDENCY     ? MC_CLIENT_WORLD_FRAME_DEPENDENCY
                                                  : MC_CLIENT_WORLD_FRAME_CHILD_FAILED;
    call->failure.operation = error->operation;
    call->failure.native_policy = error->native_policy;
    call->state.source_pc = pc;
    return false;
}

static void entities_views_expired(mc_client_world_frame_call *call) {
    call->entities_state.owner = NULL;
    call->entities_state.eye = NULL;
    call->entities_state.frustum = NULL;
    call->entities_state.ll = NULL;
    call->entities_state.th = NULL;
    call->entities_state.entities_list = NULL;
    call->entities_state.current = NULL;
    call->entities_state.failure = call->entities_state.cleanup = NULL;
}

static void render_error_views_expired(mc_client_render_global_error *error) {
    error->image_download = NULL;
    error->image_buffer = NULL;
    error->tile_renderer = NULL;
    error->render_distance = NULL;
    error->entity_culling = NULL;
    error->manager.font = NULL;
    error->manager.acquisition = NULL;
}

static bool end_completed_entities(mc_client_world_frame_call *call, unsigned int pc) {
    if (mc_client_render_global_entities_retire(&call->entities, &call->entities_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        if (call->entities != NULL)
            (void)mc_client_render_global_entities_inspect(call->entities, &call->entities_state);
        call->entities_retirement_error = call->entities_cleanup;
        call->failure.entities = &call->entities_retirement_error;
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_BUSY,
                          "completed N Entity rendering native retirement", pc, true);
    }
    entities_views_expired(call);
    render_error_views_expired(&call->entities_retirement_error);
    return true;
}

static bool source_player(mc_client_world_frame_call *call, bool *out, unsigned int pc) {
    *out = false; /* Java instanceof accepts null. */
    if (call->state.camera == NULL)
        return true;
    const mc_entity_class_literal *player = NULL;
    mc_entity_status status = mc_entity_class_literal_get(call->state.camera->classes,
                                                          MC_ENTITY_CLASS_KIND_PLAYER, &player);
    if (status == MC_ENTITY_OK)
        status = mc_entity_class_literal_is_assignable_from(player,
                                                            call->state.camera->class_literal, out);
    if (status == MC_ENTITY_OK)
        return true;
    call->failure.entity = status;
    return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED, "actual camera Player instanceof",
                      pc, true);
}

static bool source_water(mc_client_world_frame_call *call, bool *out, unsigned int material_pc,
                         unsigned int invoke_pc) {
    mc_material *water = NULL;
    if (mc_registry_material_get(call->state.app->registry, MC_MATERIAL_g, &water,
                                 &call->failure.registry) != MC_REGISTRY_OK)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                          "World-frame actual water Material active use", material_pc, true);
    if (call->state.camera == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual captured camera immersion receiver", invoke_pc, false);
    const mc_entity_status status =
        mc_entity_inside_material(call->state.camera, water, out, &call->failure.material);
    if (status == MC_ENTITY_OK)
        return true;
    const bool source_null =
        status == MC_ENTITY_WORLD_FAILED &&
        call->failure.material.world.status == MC_WORLD_STORAGE_INVALID_ARGUMENT;
    call->failure.child =
        (mc_client_graphics_site){"sn", "a", "(Lln;)Z", (int32_t)call->failure.material.bytecode};
    return outer_fail(
        call, source_null ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL : MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
        call->failure.material.operation, invoke_pc, !source_null);
}

static bool selection_body(mc_client_world_frame_call *call, bool underwater) {
    const unsigned int app_pc = underwater ? 585u : 881u;
    const unsigned int hit_pc = underwater ? 588u : 884u;
    const unsigned int inventory_pc = underwater ? 594u : 890u;
    const unsigned int current_pc = underwater ? 597u : 893u;
    const unsigned int invoke_pc = underwater ? 601u : 897u;
    mc_local_player_snapshot player;
    call->failure.entity = mc_local_player_get_snapshot(call->state.camera, &player);
    if (call->failure.entity != MC_ENTITY_OK)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                          "actual Player fields for reached selection draw",
                          underwater ? 569u : 865u, true);
    if (!display_result(call,
                        mc_client_display_source_alpha_test(call->state.owner->services.display,
                                                            false, &call->failure.display),
                        underwater ? 577u : 873u))
        return false;
    call->state.selection_alpha_disabled = true;
    client_application *app = app_at(call, app_pc, "a", "(FJ)V");
    if (app == NULL)
        return false;
    mc_entity_hit_result *hit = app->y;
    if (hit != NULL) {
        if (mc_entity_hit_result_retain(hit, &call->failure.hit) != MC_ENTITY_HIT_OK)
            return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                              "World-frame selection Hit occurrence", hit_pc, true);
        call->selection_hit = hit;
    }
    call->state.selection_hit = hit;
    /* The actual gs.c read occurs after the fresh Hit argument. No cached
     * Inventory/Stack from the earlier predicate supplies this call. */
    call->failure.entity = mc_local_player_get_snapshot(call->state.camera, &player);
    if (call->failure.entity != MC_ENTITY_OK)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                          "actual Player Inventory field", inventory_pc, true);
    if (player.player.c == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual selected Inventory receiver", current_pc, false);
    const mc_item_stack *stack = NULL;
    const mc_inventory_status current =
        mc_inventory_player_current(player.player.c, &stack, &call->failure.inventory);
    if (current != MC_INVENTORY_OK) {
        call->failure.child = (mc_client_graphics_site){
            call->failure.inventory.site.class_name, call->failure.inventory.site.method,
            call->failure.inventory.site.descriptor, (int32_t)call->failure.inventory.site.pc};
        return outer_fail(call,
                          call->failure.inventory.native_policy ? MC_CLIENT_WORLD_FRAME_CHILD_FAILED
                                                                : MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          call->failure.inventory.operation, current_pc,
                          call->failure.inventory.native_policy);
    }
    call->state.selection_current_returned = true;
    if (stack != NULL) {
        call->failure.stack = mc_item_stack_retain((mc_item_stack *)stack);
        if (call->failure.stack != MC_ITEM_STACK_OK)
            return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                              "World-frame selected Stack occurrence", current_pc, true);
        call->selection_stack = (mc_item_stack *)stack;
    }
    call->state.selection_stack = stack;
    /* The source evaluates every argument before the captured N receiver is
     * dereferenced. The actual damage/placement overlay precedes n.b outline. */
    if (!end_completed_entities(call, invoke_pc))
        return false;
    if (call->state.render_global == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual captured selection RenderGlobal receiver", invoke_pc, false);
    call->failure.child = (mc_client_graphics_site){"n", "a", "(Lgs;Lvf;ILiz;F)V", 0};
    return outer_fail(call, MC_CLIENT_WORLD_FRAME_DEPENDENCY,
                      "actual damage/placement overlay render child is not connected", invoke_pc,
                      true);
}

static void submission_views_expired(mc_client_render_global_submit_state *state) {
    state->frames = NULL;
    state->frame_count = 0;
    state->rebuild = NULL;
    state->failure = state->cleanup = NULL;
    state->owner = NULL;
    state->camera = state->batch_camera = NULL;
    state->math = NULL;
}

static bool translucent_submit(mc_client_world_frame_call *call, mc_math_classes *math,
                               unsigned int pc) {
    if (!end_completed_entities(call, pc))
        return false;
    if (call->state.render_global == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual captured translucent RenderGlobal receiver", pc, false);
    const mc_client_render_global_status status = mc_client_render_global_submit_begin(
        call->state.render_global, call->state.camera, 1, (double)call->state.partial, math,
        &call->translucent_submission, &call->translucent_submission_error);
    if (call->translucent_submission != NULL) {
        (void)mc_client_render_global_submit_inspect(call->translucent_submission,
                                                     &call->translucent_submission_state);
        call->state.translucent_submission = &call->translucent_submission_state;
    }
    if (status != MC_CLIENT_RENDER_GLOBAL_OK) {
        const mc_client_render_global_error *error = &call->translucent_submission_error;
        call->failure.submission = call->translucent_submission == NULL
                                       ? error
                                       : call->translucent_submission_state.failure;
        call->failure.status = status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                   ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                               : status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
                                   ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                               : status == MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                                   ? MC_CLIENT_WORLD_FRAME_DEPENDENCY
                                   : MC_CLIENT_WORLD_FRAME_CHILD_FAILED;
        call->failure.operation = error->operation;
        call->failure.native_policy = error->native_policy;
        call->failure.site =
            (mc_client_graphics_site){error->source_class, error->source_method,
                                      error->source_descriptor, (int32_t)error->source_pc};
        call->failure.child = error->child;
        call->state.source_pc = pc;
        return false;
    }
    call->state.translucent_count = call->translucent_submission_state.result;
    if (mc_client_render_global_submit_retire(&call->translucent_submission,
                                              &call->translucent_submission_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        if (call->translucent_submission != NULL)
            (void)mc_client_render_global_submit_inspect(call->translucent_submission,
                                                         &call->translucent_submission_state);
        call->translucent_retirement_error = call->translucent_submission_cleanup;
        call->failure.submission = &call->translucent_retirement_error;
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_BUSY,
                          "completed translucent N submission native retirement", pc, true);
    }
    submission_views_expired(&call->translucent_submission_state);
    call->state.translucent_returned = true;
    return true;
}

static bool translucent_replay_body(mc_client_world_frame_call *call) {
    if (call->state.render_global == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual captured translucent replay receiver", 786, false);
    const mc_client_render_global_status status = mc_client_render_global_replay_begin(
        call->state.render_global, 1, (double)call->state.partial, &call->translucent_replay,
        &call->translucent_replay_error);
    if (call->translucent_replay != NULL) {
        (void)mc_client_render_global_submit_inspect(call->translucent_replay,
                                                     &call->translucent_replay_state);
        call->state.translucent_replay = &call->translucent_replay_state;
    }
    if (status != MC_CLIENT_RENDER_GLOBAL_OK) {
        const mc_client_render_global_error *error = &call->translucent_replay_error;
        call->failure.submission =
            call->translucent_replay == NULL ? error : call->translucent_replay_state.failure;
        call->failure.status = status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                   ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                               : status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
                                   ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                               : status == MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                                   ? MC_CLIENT_WORLD_FRAME_DEPENDENCY
                                   : MC_CLIENT_WORLD_FRAME_CHILD_FAILED;
        call->failure.operation = error->operation;
        call->failure.native_policy = error->native_policy;
        call->failure.site =
            (mc_client_graphics_site){error->source_class, error->source_method,
                                      error->source_descriptor, (int32_t)error->source_pc};
        call->failure.child = error->child;
        call->state.source_pc = 786;
        return false;
    }
    call->state.translucent_replay_returned = true;
    if (mc_client_render_global_submit_retire(&call->translucent_replay,
                                              &call->translucent_replay_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        if (call->translucent_replay != NULL)
            (void)mc_client_render_global_submit_inspect(call->translucent_replay,
                                                         &call->translucent_replay_state);
        call->translucent_retirement_error = call->translucent_replay_cleanup;
        call->failure.submission = &call->translucent_retirement_error;
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_BUSY,
                          "completed translucent replay native retirement", 786, true);
    }
    submission_views_expired(&call->translucent_replay_state);
    return true;
}

static bool settings_at(mc_client_world_frame_call *call, unsigned int app_pc,
                        unsigned int field_pc, mc_game_settings_state *out) {
    client_application *app = app_at(call, app_pc, "a", "(FJ)V");
    if (app == NULL)
        return false;
    return mc_game_settings_read_state(app->settings, out) ||
           outer_fail(call,
                      app->settings == NULL ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                                            : MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                      "actual later World-frame Settings receiver", field_pc,
                      app->settings != NULL);
}

static bool after_particles_body(mc_client_world_frame_call *call, mc_math_classes *math) {
    mc_client_source_px *owner = call->state.owner;
    mc_client_display *display = owner->services.display;
    client_application *app = app_at(call, 539, "a", "(FJ)V");
    if (app == NULL)
        return false;
    call->state.first_hit_read = true;
    call->state.first_hit_present = app->y != NULL;
    if (app->y != NULL) {
        if (!source_water(call, &call->state.first_water, 550, 553))
            return false;
        call->state.first_water_read = true;
        if (call->state.first_water) {
            if (!source_player(call, &call->state.first_player, 561))
                return false;
            call->state.first_player_read = true;
            if (call->state.first_player)
                return selection_body(call, true);
        }
    }
    if (!display_result(
            call,
            mc_client_display_source_blend_function(display, 770, 771, &call->failure.display),
            640) ||
        !fog_setup_body(call, 4, 0, 646) ||
        !display_result(
            call, mc_client_display_source_blend_enable(display, true, &call->failure.display),
            652) ||
        !display_result(
            call, mc_client_display_source_cull_test(display, false, &call->failure.display), 658))
        return false;
    app = app_at(call, 665, "a", "(FJ)V");
    if (app == NULL)
        return false;
    mc_client_render_engine *engine = app->render_engine;
    if (engine == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual translucent terrain Engine receiver", 673, false);
    if (mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_BUSY,
                          "translucent terrain Engine native occurrence", 673, true);
    call->translucent_engine = engine;
    static const uint16_t terrain[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '.', 'p', 'n', 'g'};
    const mc_texture_pack_text key = {terrain, sizeof terrain / sizeof(*terrain), false};
    mc_client_texture texture;
    const bool lookup_pending = mc_client_texture_lookup_pending(engine);
    const mc_client_texture_lookup_status texture_status = mc_client_render_engine_texture(
        engine, display, NULL, key, &texture, &call->translucent_texture_error);
    call->translucent_lookup_owned = !lookup_pending && mc_client_texture_lookup_pending(engine);
    if (texture_status != MC_CLIENT_LOOKUP_OK) {
        call->failure.texture = &call->translucent_texture_error;
        call->failure.child = call->translucent_texture_error.site;
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                          call->translucent_texture_error.operation, 673,
                          call->translucent_texture_error.native_policy);
    }
    if (!display_result(
            call, mc_client_display_source_bind_id(display, texture, &call->failure.display), 676))
        return false;
    call->state.translucent_texture_bound = true;
    mc_game_settings_state settings;
    if (!settings_at(call, 680, 686, &settings))
        return false;
    call->state.fancy_graphics_read = true;
    call->state.fancy_graphics = settings.values.fancy_graphics;
    if (settings.values.fancy_graphics) {
        if (!settings_at(call, 693, 699, &settings))
            return false;
        call->state.translucent_ambient_read = true;
        call->state.translucent_ambient = settings.values.ambient_occlusion;
        if (settings.values.ambient_occlusion &&
            !display_result(
                call, mc_client_display_source_shade_model(display, 7425, &call->failure.display),
                708))
            return false;
        if (!display_result(call,
                            mc_client_display_source_color_mask(display, false, false, false, false,
                                                                &call->failure.display),
                            715))
            return false;
        call->state.translucent_color_masked = true;
        if (!translucent_submit(call, math, 725) || !settings_at(call, 731, 737, &settings))
            return false;
        call->state.translucent_anaglyph_read = true;
        call->state.translucent_anaglyph = settings.values.anaglyph;
        const bool anaglyph = settings.values.anaglyph;
        const bool first_eye = !anaglyph || owner->services.models->px_b == 0;
        if (!display_result(call,
                            mc_client_display_source_color_mask(
                                display, !anaglyph || !first_eye, !anaglyph || first_eye,
                                !anaglyph || first_eye, true, &call->failure.display),
                            anaglyph ? (first_eye ? 753u : 763u) : 773u))
            return false;
        call->state.translucent_color_restored = true;
        if (call->state.translucent_count > 0 && !translucent_replay_body(call))
            return false;
        if (!display_result(
                call, mc_client_display_source_shade_model(display, 7424, &call->failure.display),
                792))
            return false;
    } else if (!translucent_submit(call, math, 805)) {
        return false;
    }
    if (!display_result(call,
                        mc_client_display_source_depth_mask(display, true, &call->failure.display),
                        810) ||
        !display_result(
            call, mc_client_display_source_cull_test(display, true, &call->failure.display), 816) ||
        !display_result(
            call, mc_client_display_source_blend_enable(display, false, &call->failure.display),
            822))
        return false;
    call->state.translucent_state_restored = true;
    if (!fp(call, "a", "(FJ)V", 830))
        return false;
    call->state.selection_zoom_read = true;
    if (owner->fields.E == 1.0) {
        if (!source_player(call, &call->state.selection_player, 836))
            return false;
        call->state.selection_player_read = true;
        if (call->state.selection_player) {
            app = app_at(call, 843, "a", "(FJ)V");
            if (app == NULL)
                return false;
            call->state.selection_hit_read = true;
            call->state.selection_hit_present = app->y != NULL;
            if (app->y != NULL) {
                if (!source_water(call, &call->state.selection_water, 854, 857))
                    return false;
                call->state.selection_water_read = true;
                if (!call->state.selection_water)
                    return selection_body(call, false);
            }
        }
    }
    const mc_client_precipitation_status weather = entity_renderer_precipitation_entered(
        owner, math, call->state.partial, &call->precipitation, &call->precipitation_error);
    if (call->precipitation != NULL) {
        (void)mc_client_entity_renderer_precipitation_inspect(call->precipitation,
                                                              &call->precipitation_state);
        call->state.precipitation = &call->precipitation_state;
    }
    if (weather != MC_CLIENT_PRECIPITATION_OK) {
        const mc_client_precipitation_error *error = call->precipitation == NULL
                                                         ? &call->precipitation_error
                                                         : call->precipitation_state.failure;
        call->failure.precipitation = error;
        call->failure.child =
            (mc_client_graphics_site){"px", "c", "(F)V", (int32_t)error->source_pc};
        return outer_fail(call,
                          weather == MC_CLIENT_PRECIPITATION_SOURCE_NULL
                              ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                          : weather == MC_CLIENT_PRECIPITATION_UNSUPPORTED_FP
                              ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                              : MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                          error->operation, 932, error->native_policy);
    }
    call->state.precipitation_returned = true;
    if (!display_result(
            call, mc_client_display_source_fog_enable(display, false, &call->failure.display), 938))
        return false;
    call->state.weather_fog_disabled = true;
    /* The m942 read has no child invocation or side effect on either branch. */
    if (!fog_setup_body(call, 5, 0, 951) ||
        !display_result(
            call, mc_client_display_source_fog_enable(display, true, &call->failure.display), 957))
        return false;
    call->state.cloud_fog_enabled = true;
    if (call->state.render_global == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual captured clouds RenderGlobal receiver", 963, false);
    call->failure.child = (mc_client_graphics_site){"n", "b", "(F)V", 0};
    return outer_fail(call, MC_CLIENT_WORLD_FRAME_DEPENDENCY,
                      "actual clouds draw child is not connected", 963, true);
}

static bool particles_body(mc_client_world_frame_call *call, mc_math_classes *math) {
    if (call->state.effects == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual captured EffectRenderer receiver", 518, false);
    client_application *app = call->state.app;
    const mc_effect_draw_services services = {
        .graphics = call->state.owner->services.graphics,
        .math = math,
        .display = call->state.owner->services.display,
        .manager = app->startup == NULL ? NULL : app->startup->manager_classes};
    mc_effect_draw_status status = mc_client_effect_renderer_lit_begin(
        call->state.effects, call->state.camera, call->state.partial, &services,
        &call->lit_particles, &call->lit_particles_error);
    if (call->lit_particles != NULL) {
        (void)mc_client_effect_renderer_draw_inspect(call->lit_particles,
                                                     &call->lit_particles_state);
        call->state.lit_particles = &call->lit_particles_state;
    }
    if (!particle_result(call, status,
                         call->lit_particles == NULL ? &call->lit_particles_error
                                                     : call->lit_particles_state.failure,
                         518))
        return false;
    call->state.lit_particles_returned = true;
    if (mc_client_effect_renderer_draw_retire(&call->lit_particles, &call->lit_particles_cleanup) !=
        MC_EFFECT_DRAW_OK) {
        if (call->lit_particles != NULL)
            (void)mc_client_effect_renderer_draw_inspect(call->lit_particles,
                                                         &call->lit_particles_state);
        call->particles_retirement_error = call->lit_particles_cleanup;
        call->failure.particles = &call->particles_retirement_error;
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_BUSY,
                          "completed lit particle native retirement", 518, true);
    }
    particle_views_expired(&call->lit_particles_state);
    if (!mc_client_lighting_disable(
            call->state.owner->services.graphics, call->state.owner->services.display,
            (mc_client_graphics_site){"px", "a", "(FJ)V", 521}, &call->particle_lighting_error)) {
        call->failure.lighting = &call->particle_lighting_error;
        call->failure.child = call->particle_lighting_error.site;
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                          call->particle_lighting_error.operation, 521,
                          call->particle_lighting_error.native_policy);
    }
    call->state.particle_lighting_disabled = true;
    if (!fog_setup_body(call, 3, 0, 527))
        return false;
    status = mc_client_effect_renderer_sprites_begin(
        call->state.effects, call->state.camera, call->state.partial, &services,
        &call->sprite_particles, &call->sprite_particles_error);
    if (call->sprite_particles != NULL) {
        (void)mc_client_effect_renderer_draw_inspect(call->sprite_particles,
                                                     &call->sprite_particles_state);
        call->state.sprite_particles = &call->sprite_particles_state;
    }
    if (!particle_result(call, status,
                         call->sprite_particles == NULL ? &call->sprite_particles_error
                                                        : call->sprite_particles_state.failure,
                         535))
        return false;
    call->state.sprite_particles_returned = true;
    if (mc_client_effect_renderer_draw_retire(
            &call->sprite_particles, &call->sprite_particles_cleanup) != MC_EFFECT_DRAW_OK) {
        if (call->sprite_particles != NULL)
            (void)mc_client_effect_renderer_draw_inspect(call->sprite_particles,
                                                         &call->sprite_particles_state);
        call->particles_retirement_error = call->sprite_particles_cleanup;
        call->failure.particles = &call->particles_retirement_error;
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_BUSY,
                          "completed sprite particle native retirement", 535, true);
    }
    particle_views_expired(&call->sprite_particles_state);
    return after_particles_body(call, math);
}
