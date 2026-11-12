/* Included in actual textures.c, where the real private device record layout
 * is defined. No descriptor/device layout is copied into sj or a side owner. */
struct mc_client_font_lease {
    mc_client_render_engine *engine;
    mc_client_render_engine_classes *classes;
    mc_client_display_texture_lease *generation;
    bool draw_live, active;
};

const mc_client_render_engine_classes *
mc_client_render_engine_class_identity(const mc_client_render_engine *engine) {
    return engine == NULL ? NULL : engine->classes;
}

static bool generation_lease_capture(mc_client_render_engine *engine, mc_client_display *display,
                                     mc_client_texture id, mc_client_graphics_site site,
                                     mc_client_font_lease **out, mc_client_graphics_error *e,
                                     bool image) {
    if (engine == NULL || out == NULL || engine->phase != MC_CLIENT_RENDER_ENGINE_READY ||
        engine->classes->resource_retirement_started || engine->classes->device == NULL ||
        engine->classes->device->display != display) {
        mc_client_graphics_fail(
            e, MC_CLIENT_GRAPHICS_INVALID_PHASE,
            (image ? "image actual resource namespace" : "font actual resource namespace"), site,
            true);
        return false;
    }
    mc_client_render_engine_classes *classes = engine->classes;
    if (classes->active_source_calls != 0 || classes->active_resource_calls != 0) {
        mc_client_graphics_fail(
            e, MC_CLIENT_GRAPHICS_BUSY,
            (image ? "image namespace capture active" : "font namespace capture active"), site,
            true);
        return false;
    }
    if (classes->font_lease_count == SIZE_MAX) {
        mc_client_graphics_fail(
            e, MC_CLIENT_GRAPHICS_LIMIT,
            (image ? "image namespace lease limit" : "font namespace lease limit"), site, true);
        return false;
    }
    mc_client_texture_resource *resource = NULL;
    for (size_t i = 0; i < classes->device->count; ++i)
        if (classes->device->records[i].resource != NULL &&
            classes->device->records[i].driver_generated &&
            classes->device->records[i].buffer_value == id) {
            resource = classes->device->records[i].resource;
            break;
        }
    if (resource == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT,
                                (image ? "image actual generated ID occurrence"
                                       : "font actual generated ID occurrence"),
                                site, true);
        return false;
    }
    if (mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_LIMIT,
                                (image ? "image Engine native hold" : "font Engine native hold"),
                                site, true);
        return false;
    }
    if (mc_client_render_engine_classes_retain(classes) != MC_CLIENT_RENDER_ENGINE_OK) {
        (void)mc_client_render_engine_release(engine);
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_LIMIT,
                                (image ? "image class native hold" : "font class native hold"),
                                site, true);
        return false;
    }
    ++classes->active_resource_calls;
    ++engine->active_resource_calls;
    mc_client_font_lease *lease = calloc(1, sizeof(*lease));
    bool ok = lease != NULL;
    if (!ok) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_ALLOCATION_FAILED,
                                (image ? "image generation owner" : "font generation owner"), site,
                                true);
    } else if (!mc_client_display_texture_lease_acquire(resource, &lease->generation,
                                                        &e->display)) {
        mc_client_graphics_fail(
            e,
            e->display.status == MC_CLIENT_DISPLAY_ALLOCATION_FAILED
                ? MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
                : MC_CLIENT_GRAPHICS_DISPLAY_FAILED,
            (image ? "image immutable Display generation" : "font immutable Display generation"),
            site, true);
        ok = false;
    }
    --engine->active_resource_calls;
    --classes->active_resource_calls;
    if (!ok) {
        free(lease);
        (void)mc_client_render_engine_release(engine);
        (void)mc_client_render_engine_classes_release(classes);
        return false;
    }
    lease->engine = engine;
    lease->classes = classes;
    lease->draw_live = true;
    ++classes->font_lease_count;
    *out = lease;
    return true;
}

bool mc_client_render_engine_font_lease(mc_client_render_engine *engine, mc_client_display *display,
                                        mc_client_texture id, mc_client_graphics_site site,
                                        mc_client_font_lease **out, mc_client_graphics_error *e) {
    return generation_lease_capture(engine, display, id, site, out, e, false);
}

bool mc_client_render_engine_image_lease(mc_client_render_engine *engine,
                                         mc_client_display *display, mc_client_texture id,
                                         mc_client_graphics_site site, mc_client_font_lease **out,
                                         mc_client_graphics_error *e) {
    return generation_lease_capture(engine, display, id, site, out, e, true);
}

bool mc_client_font_lease_begin(mc_client_font_lease *lease, mc_client_graphics_error *e) {
    if (lease == NULL || !lease->draw_live || lease->active ||
        lease->classes->resource_retirement_started) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_PHASE,
                                "font generation draw admission", (mc_client_graphics_site){0},
                                true);
        return false;
    }
    if (!mc_client_display_texture_lease_begin(lease->generation, &e->display)) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "font generation draw guard",
                                (mc_client_graphics_site){0}, true);
        return false;
    }
    lease->active = true;
    return true;
}

bool mc_client_font_lease_bind(mc_client_font_lease *lease, mc_client_graphics_error *e) {
    if (lease == NULL || !lease->draw_live || !lease->active) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_PHASE, "font generation source bind",
                                (mc_client_graphics_site){0}, true);
        return false;
    }
    if (!mc_client_display_texture_lease_bind(lease->generation, &e->display)) {
        mc_client_graphics_fail(e,
                                e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL
                                    ? MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY
                                    : MC_CLIENT_GRAPHICS_DISPLAY_FAILED,
                                "font generation source bind", (mc_client_graphics_site){0}, true);
        return false;
    }
    return true;
}

bool mc_client_font_lease_end(mc_client_font_lease *lease, mc_client_graphics_error *e) {
    if (lease == NULL || !lease->active) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_PHASE, "font generation draw end",
                                (mc_client_graphics_site){0}, true);
        return false;
    }
    if (!mc_client_display_texture_lease_end(lease->generation, &e->display)) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "font generation draw end",
                                (mc_client_graphics_site){0}, true);
        return false;
    }
    lease->active = false;
    return true;
}

bool mc_client_font_lease_release(mc_client_font_lease **slot, mc_client_graphics_error *e) {
    if (slot == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT, "font lease release slot",
                                (mc_client_graphics_site){0}, true);
        return false;
    }
    mc_client_font_lease *lease = *slot;
    if (lease == NULL)
        return true;
    if (lease->active || lease->classes->active_source_calls != 0 ||
        lease->classes->active_resource_calls != 0) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "font lease release during call",
                                (mc_client_graphics_site){0}, true);
        return false;
    }
    if (!mc_client_display_texture_lease_release(&lease->generation, &e->display)) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "font Display generation release",
                                (mc_client_graphics_site){0}, true);
        return false;
    }
    if (lease->draw_live) {
        --lease->classes->font_lease_count;
        lease->draw_live = false;
    }
    if (lease->engine != NULL) {
        const mc_client_render_engine_status status =
            mc_client_render_engine_release(lease->engine);
        if (status != MC_CLIENT_RENDER_ENGINE_OK) {
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "font Engine hold release pending",
                                    (mc_client_graphics_site){0}, true);
            return false;
        }
        lease->engine = NULL;
    }
    if (mc_client_render_engine_classes_release(lease->classes) != MC_CLIENT_RENDER_ENGINE_OK) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY,
                                "font final class hold pending native resource retirement",
                                (mc_client_graphics_site){0}, true);
        return false;
    }
    free(lease);
    *slot = NULL;
    return true;
}
