#include "internal.h"

#include "../../../entity/entity_internal.h"
#include "../../../world/runtime/construction_internal.h"
#include "../../../world/runtime/runtime_internal.h"
#include "../../gui/bitmap_font.h"
#include "../interpolation_internal.h"
#include "mc/client/settings/construction.h"
#include "mc/entity/living/sleeping.h"
#include "mc/world/runtime/blocks.h"

#include <stdlib.h>
#include <string.h>

static const char frame_descriptor[] = "(Lfd;Lji;Lsj;Lls;Lkv;F)V";

/* Large child errors never become automatic frame/retirement objects. This
 * one quota-owned record is reused and borrows only the current held World. */
struct manager_frame_diagnostics {
    mc_world_runtime_acquisition_error acquisition;
    mc_client_bitmap_font_error font;
    mc_client_bitmap_font_state font_state;
};

static bool frame_fail(mc_client_manager_error *e, mc_client_manager_status status,
                       const char *operation, int32_t pc, bool native) {
    manager_fail(e, status, operation, "th", "a", frame_descriptor, pc, native);
    e->th_pc = e->frame_pc = pc;
    return false;
}

static void *frame_allocate(mc_client_render_manager *m, size_t bytes, int32_t pc,
                            mc_client_manager_error *e) {
    void *p = manager_allocate_native(m->classes, bytes, "th", pc, e);
    if (p == NULL) {
        e->source_method = "a";
        e->source_descriptor = frame_descriptor;
        e->th_pc = e->frame_pc = pc;
    }
    return p;
}

static bool diagnostics(mc_client_render_manager *m, int32_t pc, mc_client_manager_error *e) {
    if (m->frame_diagnostics == NULL)
        m->frame_diagnostics = frame_allocate(m, sizeof(*m->frame_diagnostics), pc, e);
    return m->frame_diagnostics != NULL;
}

void manager_frame_invalidate(mc_client_render_manager *m) {
    if (m->frame_diagnostics != NULL)
        memset(m->frame_diagnostics, 0, sizeof(*m->frame_diagnostics));
}

void manager_frame_destroy(mc_client_render_manager *m) {
    if (m->frame_diagnostics != NULL) {
        m->classes->bytes -= sizeof(*m->frame_diagnostics);
        free(m->frame_diagnostics);
        m->frame_diagnostics = NULL;
    }
}

static bool ledger_failure(mc_client_manager_error *e) {
    manager_fail(e, MC_CLIENT_MANAGER_BUSY, "TH frame native occurrence invariant", NULL, NULL,
                 NULL, -1, true);
    return false;
}

static bool frame_ledger_admit(const mc_client_render_manager *m, mc_client_manager_error *e) {
    size_t engines = 0, fonts = 0, settings = 0;
    bool engine_found = m->fields.e == NULL, font_found = m->fields.p == NULL;
    bool settings_found = m->fields.k == NULL;
    for (const manager_engine_occurrence *p = m->engines; p != NULL; p = p->next) {
        if (engines == m->retained_engines || p->value == NULL)
            return ledger_failure(e);
        ++engines;
        if (p->value == m->fields.e)
            engine_found = true;
    }
    for (const manager_font_occurrence *p = m->fonts; p != NULL; p = p->next) {
        if (fonts == m->retained_fonts || p->value == NULL)
            return ledger_failure(e);
        ++fonts;
        if (p->value == m->fields.p)
            font_found = true;
    }
    for (const manager_settings_occurrence *p = m->settings; p != NULL; p = p->next) {
        if (settings == m->retained_settings || p->value == NULL)
            return ledger_failure(e);
        ++settings;
        if (p->value == m->fields.k)
            settings_found = true;
    }
    if (engines != m->retained_engines || fonts != m->retained_fonts ||
        settings != m->retained_settings || !engine_found || !font_found || !settings_found ||
        m->fields.h != m->living_occurrence ||
        (m->living_occurrence != NULL && m->living_occurrence->holds == 0) ||
        (m->fonts != NULL && m->frame_diagnostics == NULL))
        return ledger_failure(e);
    return manager_world_ledger_admit(m, e);
}

static bool capture_engine(mc_client_render_manager *m, mc_client_render_engine *value,
                           mc_client_manager_error *e) {
    if (value == NULL)
        return true;
    for (manager_engine_occurrence *p = m->engines; p != NULL; p = p->next)
        if (p->value == value)
            return true;
    if (m->retained_engines == SIZE_MAX)
        return frame_fail(e, MC_CLIENT_MANAGER_LIMIT, "TH native Engine occurrence bound", 7, true);
    manager_engine_occurrence *p = frame_allocate(m, sizeof(*p), 7, e);
    if (p == NULL)
        return false;
    e->engine_status = mc_client_render_engine_retain(value);
    if (e->engine_status != MC_CLIENT_RENDER_ENGINE_OK) {
        m->classes->bytes -= sizeof(*p);
        free(p);
        return frame_fail(e, MC_CLIENT_MANAGER_ENGINE_FAILED, "TH actual Engine native occurrence",
                          7, true);
    }
    p->value = value;
    p->next = m->engines;
    m->engines = p;
    ++m->retained_engines;
    return true;
}

static bool capture_settings(mc_client_render_manager *m, mc_game_settings *value,
                             mc_client_manager_error *e) {
    if (value == NULL)
        return true;
    for (manager_settings_occurrence *p = m->settings; p != NULL; p = p->next)
        if (p->value == value)
            return true;
    if (m->retained_settings == SIZE_MAX)
        return frame_fail(e, MC_CLIENT_MANAGER_LIMIT, "TH native Settings occurrence bound", 13,
                          true);
    manager_settings_occurrence *p = frame_allocate(m, sizeof(*p), 13, e);
    if (p == NULL)
        return false;
    if (mc_game_settings_borrow(value, &e->settings) != MC_GAME_SETTINGS_OK) {
        m->classes->bytes -= sizeof(*p);
        free(p);
        return frame_fail(e, MC_CLIENT_MANAGER_SETTINGS_FAILED,
                          "TH actual Settings native occurrence", 13, true);
    }
    p->value = value;
    p->next = m->settings;
    m->settings = p;
    ++m->retained_settings;
    return true;
}

static bool bind_living(mc_client_render_manager *m, mc_entity *value, mc_client_manager_error *e) {
    if (value != NULL) {
        mc_entity_class_literal_view view;
        const mc_entity_class_literal *living = NULL;
        bool matches = false;
        e->entity_status = mc_entity_class_literal_inspect(value->class_literal, &view);
        if (e->entity_status == MC_ENTITY_OK)
            e->entity_status = mc_entity_class_literal_get(
                m->classes->models->entities, MC_ENTITY_CLASS_KIND_LIVING_ENTITY, &living);
        if (e->entity_status == MC_ENTITY_OK)
            e->entity_status =
                mc_entity_class_literal_is_assignable_from(living, value->class_literal, &matches);
        if (e->entity_status != MC_ENTITY_OK)
            return frame_fail(e, MC_CLIENT_MANAGER_ENTITY_FAILED,
                              "TH canonical Living Class admission", 19, true);
        if (!matches || view.owner != m->classes->models->entities ||
            view.source != MC_ENTITY_CLIENT) {
            e->entity_status = MC_ENTITY_INVALID_ARGUMENT;
            return frame_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                              "TH Living from the same actual Entity namespace", 19, true);
        }
        e->entity_status = mc_entity_retain(value);
        if (e->entity_status != MC_ENTITY_OK)
            return frame_fail(e, MC_CLIENT_MANAGER_ENTITY_FAILED,
                              "TH actual Living native occurrence", 19, true);
    }
    mc_entity *previous = m->living_occurrence;
    m->fields.h = value;
    m->living_occurrence = value;
    m->last_frame_pc = 19;
    mc_entity_release(previous);
    return true;
}

static bool capture_font(mc_client_render_manager *m, mc_client_bitmap_font *value,
                         mc_client_manager_error *e) {
    if (value == NULL)
        return true;
    for (manager_font_occurrence *p = m->fonts; p != NULL; p = p->next)
        if (p->value == value)
            return true;
    if (m->retained_fonts == SIZE_MAX)
        return frame_fail(e, MC_CLIENT_MANAGER_LIMIT, "TH native Font occurrence bound", 24, true);
    /* Retirement must be able to report the complete closer failure without
     * allocating after any held occurrence has been consumed. */
    if (!diagnostics(m, 24, e))
        return false;
    manager_font_occurrence *p = frame_allocate(m, sizeof(*p), 24, e);
    if (p == NULL)
        return false;
    const mc_client_bitmap_font_status status = mc_client_bitmap_font_retain(value);
    e->font_status = (int)status;
    if (status != MC_CLIENT_BITMAP_FONT_OK) {
        m->classes->bytes -= sizeof(*p);
        free(p);
        return frame_fail(e, MC_CLIENT_MANAGER_FONT_FAILED,
                          "TH actual BitmapFont native occurrence", 24, true);
    }
    p->value = value;
    p->next = m->fonts;
    m->fonts = p;
    ++m->retained_fonts;
    return true;
}

static bool sleeping_dispatch(mc_entity *living, bool *sleeping, mc_client_manager_error *e) {
    mc_entity_sleeping_error child;
    const mc_entity_sleeping_status status = mc_entity_living_is_sleeping(living, sleeping, &child);
    e->entity_status = child.entity;
    switch (status) {
        case MC_ENTITY_SLEEPING_OK:
            e->player = child.player;
            return true;
        case MC_ENTITY_SLEEPING_SOURCE_NULL:
            return frame_fail(e, MC_CLIENT_MANAGER_NULL_REFERENCE, "actual null Living.N receiver",
                              29, false);
        case MC_ENTITY_SLEEPING_ENTITY_FAILED:
            return frame_fail(e, MC_CLIENT_MANAGER_ENTITY_FAILED, "TH actual Living.N dispatch", 29,
                              true);
        case MC_ENTITY_SLEEPING_PLAYER_FAILED:
            e->player = child.player;
            return frame_fail(e, MC_CLIENT_MANAGER_PLAYER_FAILED, "actual GS.N sleeping field", 29,
                              true);
        case MC_ENTITY_SLEEPING_UNIMPLEMENTED_DEPENDENCY:
            return frame_fail(e, MC_CLIENT_MANAGER_UNIMPLEMENTED_DEPENDENCY,
                              "actual Living.N override requires its concrete owner", 29, true);
        case MC_ENTITY_SLEEPING_INVALID_ARGUMENT:
        default:
            return frame_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                              "TH actual Living.N dispatch output", 29, true);
    }
}

static bool floor_coordinate(mc_math_classes *math, double coordinate, int32_t pc, int32_t *out,
                             mc_client_manager_error *e) {
    if (math == NULL)
        return frame_fail(e, MC_CLIENT_MANAGER_UNIMPLEMENTED_DEPENDENCY,
                          "TH reached canonical Math namespace", pc, true);
    if (mc_math_classes_ensure(math, &e->math) != MC_MATH_CLASS_OK)
        return frame_fail(e, MC_CLIENT_MANAGER_MATH_FAILED, "actual in.b(D)I active use", pc, true);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return frame_fail(e, MC_CLIENT_MANAGER_UNSUPPORTED_FP,
                          "TH floor strict floating environment", pc, true);
    *out = mc_math_floor_double(coordinate);
    return true;
}

static bool interpolate_failure(mc_client_manager_error *e,
                                const mc_client_interpolation_error *child) {
    return frame_fail(e,
                      child->status == MC_CLIENT_INTERPOLATION_UNSUPPORTED_FP
                          ? MC_CLIENT_MANAGER_UNSUPPORTED_FP
                          : MC_CLIENT_MANAGER_UNSUPPORTED_NUMERIC,
                      child->status == MC_CLIENT_INTERPOLATION_UNSUPPORTED_FP
                          ? "TH frame strict floating environment"
                          : "TH frame unmeasured two-NaN signaling operand",
                      child->pc, true);
}

static bool read_world(mc_client_render_manager *m, mc_world_runtime *world, int32_t x, int32_t y,
                       int32_t z, bool metadata, int32_t *out, mc_client_manager_error *e) {
    const int32_t pc = metadata ? 101 : 60;
    if (world == NULL)
        return frame_fail(e, MC_CLIENT_MANAGER_NULL_REFERENCE,
                          metadata ? "actual null fd.e receiver" : "actual null fd.a receiver", pc,
                          false);
    if (!diagnostics(m, pc, e))
        return false;
    e->acquisition = &m->frame_diagnostics->acquisition;
    const mc_world_storage_status status =
        metadata ? mc_world_runtime_metadata_get_detailed(world, x, y, z, out, &e->world,
                                                          &m->frame_diagnostics->acquisition)
                 : mc_world_runtime_block_get_detailed(world, x, y, z, out, &e->world,
                                                       &m->frame_diagnostics->acquisition);
    if (status != MC_WORLD_STORAGE_OK)
        return frame_fail(e, MC_CLIENT_MANAGER_WORLD_FAILED,
                          metadata ? "actual sleeping World metadata read"
                                   : "actual sleeping World block read",
                          pc, true);
    return true;
}

mc_client_manager_status mc_client_render_manager_bind_frame(
    mc_client_render_manager *m, mc_math_classes *math, mc_world_runtime *world,
    mc_client_render_engine *engine, mc_client_bitmap_font *font, mc_entity *living,
    mc_game_settings *settings, float partial, mc_client_manager_error *error) {
    mc_client_manager_error e;
    manager_error_init(&e);
    if (m == NULL)
        (void)frame_fail(&e, MC_CLIENT_MANAGER_NULL_REFERENCE, "actual TH frame receiver", 2,
                         false);
    else if (m->active || m->classes->active)
        manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "TH frame native operation admission", NULL, NULL,
                     NULL, -1, true);
    else if (m->classes->phase != MC_CLIENT_MANAGER_READY || m->classes->a != m)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_PHASE, "TH frame requires completed singleton",
                     NULL, NULL, NULL, -1, true);
    else if (frame_ledger_admit(m, &e)) {
        m->active = m->classes->active = true;
        m->last_frame_pc = -1;
        if (world != NULL &&
            (world->construction.mode != MC_WORLD_RUNTIME_GAME || world->classes == NULL ||
             world->classes->registry != m->classes->registry ||
             world->classes->entities != m->classes->models->entities ||
             world->classes->profile != m->classes->profile ||
             world->classes->side != MC_WORLD_INFO_CLIENT)) {
            (void)frame_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                             "TH actual GAME World from the same canonical namespace", 2, true);
            goto leave;
        }
        if (!manager_world_store_entered(m, world, &e)) {
            (void)frame_fail(&e, e.status, e.operation, 2, true);
            goto leave;
        }
        m->last_frame_pc = 2;
        if (!capture_engine(m, engine, &e))
            goto leave;
        m->fields.e = engine;
        m->last_frame_pc = 7;
        if (!capture_settings(m, settings, &e))
            goto leave;
        m->fields.k = settings;
        m->last_frame_pc = 13;
        if (!bind_living(m, living, &e) || !capture_font(m, font, &e))
            goto leave;
        m->fields.p = font;
        m->last_frame_pc = 24;
        bool sleeping;
        if (!sleeping_dispatch(living, &sleeping, &e))
            goto leave;
        m->last_frame_pc = 29;
        mc_entity_base_fields *f = mc_entity_runtime_fields(living);
        if (sleeping) {
            int32_t x, y, z, block, metadata;
            if (!floor_coordinate(math, f->x, 41, &x, &e) ||
                !floor_coordinate(math, f->y, 49, &y, &e) ||
                !floor_coordinate(math, f->z, 57, &z, &e) ||
                !read_world(m, world, x, y, z, false, &block, &e))
                goto leave;
            m->last_frame_pc = 60;
            const mc_block *bed = NULL;
            if (mc_registry_block_static_get(m->classes->registry, "T", &bed, &e.registry) !=
                MC_REGISTRY_OK) {
                (void)frame_fail(&e, MC_CLIENT_MANAGER_REGISTRY_FAILED,
                                 "actual UU.T sleeping Bed active use", 67, false);
                e.registry_delegated = true;
                goto leave;
            }
            if (bed == NULL) {
                (void)frame_fail(&e, MC_CLIENT_MANAGER_NULL_REFERENCE,
                                 "actual null UU.T.bn receiver", 70, false);
                goto leave;
            }
            m->last_frame_pc = 70;
            if (block == mc_block_fields_get(bed)->bn) {
                /* The World/provider child may have changed this SAME Living.
                 * Source loads all three coordinates again after the Bed test. */
                f = mc_entity_runtime_fields(living);
                if (!floor_coordinate(math, f->x, 82, &x, &e) ||
                    !floor_coordinate(math, f->y, 90, &y, &e) ||
                    !floor_coordinate(math, f->z, 98, &z, &e) ||
                    !read_world(m, world, x, y, z, true, &metadata, &e))
                    goto leave;
                m->last_frame_pc = 101;
                m->fields.i = (float)((metadata & 3) * 90 + 180);
                m->last_frame_pc = 123;
                m->fields.j = 0.0f;
                m->last_frame_pc = 128;
            }
        } else {
            float value;
            mc_client_interpolation_error child;
            if (!mc_client_interpolate_float(f->aU, f->yaw, partial, 150, 153, 154, &value,
                                             &child)) {
                (void)interpolate_failure(&e, &child);
                goto leave;
            }
            m->fields.i = value;
            m->last_frame_pc = 155;
            if (!mc_client_interpolate_float(f->aV, f->aT, partial, 174, 177, 178, &value,
                                             &child)) {
                (void)interpolate_failure(&e, &child);
                goto leave;
            }
            m->fields.j = value;
            m->last_frame_pc = 179;
        }
        /* Fresh actual fields after either World child, never an entry snapshot. */
        f = mc_entity_runtime_fields(living);
        double position;
        mc_client_interpolation_error child;
        if (!mc_client_interpolate_double(f->bl, f->x, partial, 198, 201, 202, 203, &position,
                                          &child)) {
            (void)interpolate_failure(&e, &child);
            goto leave;
        }
        m->fields.l = position;
        m->last_frame_pc = 204;
        if (!mc_client_interpolate_double(f->bm, f->y, partial, 223, 226, 227, 228, &position,
                                          &child)) {
            (void)interpolate_failure(&e, &child);
            goto leave;
        }
        m->fields.m = position;
        m->last_frame_pc = 229;
        if (!mc_client_interpolate_double(f->bn, f->z, partial, 248, 251, 252, 253, &position,
                                          &child)) {
            (void)interpolate_failure(&e, &child);
            goto leave;
        }
        m->fields.n = position;
        m->last_frame_pc = e.frame_pc = 257;
    leave:
        m->active = m->classes->active = false;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool manager_frame_terminal_admit(mc_client_render_manager *m, mc_client_manager_error *e) {
    if (!frame_ledger_admit(m, e))
        return false;
    for (manager_font_occurrence *p = m->fonts; p != NULL; p = p->next) {
        mc_client_bitmap_font_state *s = &m->frame_diagnostics->font_state;
        if (!mc_client_bitmap_font_inspect(p->value, s) || s->holds == 0)
            return ledger_failure(e);
        if (s->active || s->phase == MC_CLIENT_BITMAP_FONT_CONSTRUCTING) {
            e->font_status = (int)MC_CLIENT_BITMAP_FONT_BUSY;
            manager_fail(e, MC_CLIENT_MANAGER_BUSY, "TH retained Font operation blocks retirement",
                         NULL, NULL, NULL, -1, true);
            return false;
        }
    }
    for (manager_settings_occurrence *p = m->settings; p != NULL; p = p->next) {
        mc_game_settings_state s;
        if (!mc_game_settings_read_state(p->value, &s) || s.native_borrows == 0)
            return ledger_failure(e);
        mc_game_settings_source_state source;
        if (mc_game_settings_source_inspect(p->value, &source) &&
            (source.active || source.phase == MC_GAME_SETTINGS_SOURCE_CONSTRUCTING)) {
            e->settings.status = MC_GAME_SETTINGS_BUSY;
            manager_fail(e, MC_CLIENT_MANAGER_BUSY,
                         "TH retained Settings operation blocks retirement", NULL, NULL, NULL, -1,
                         true);
            return false;
        }
    }
    for (manager_engine_occurrence *p = m->engines; p != NULL; p = p->next) {
        mc_client_render_engine_state s;
        if (!mc_client_render_engine_inspect(p->value, &s) || s.native_holds == 0)
            return ledger_failure(e);
        if (s.phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING) {
            e->engine_status = MC_CLIENT_RENDER_ENGINE_BUSY;
            manager_fail(e, MC_CLIENT_MANAGER_BUSY,
                         "TH retained Engine constructor blocks retirement", NULL, NULL, NULL, -1,
                         true);
            return false;
        }
    }
    return true;
}

/* Called with RETIRING and both TH guards held. Consume each occurrence only
 * after checked release; failed Font closers or active Settings remain retryable.
 * Fonts and Settings end before any Engine occurrence. No source null stores. */
bool manager_frame_retire(mc_client_render_manager *m, mc_client_manager_error *e) {
    manager_frame_invalidate(m);
    while (m->fonts != NULL) {
        manager_font_occurrence *p = m->fonts;
        const bool current = m->fields.p == p->value;
        e->font = &m->frame_diagnostics->font;
        const mc_client_bitmap_font_status status =
            mc_client_bitmap_font_release(&p->value, &m->frame_diagnostics->font);
        e->font_status = (int)status;
        if (status != MC_CLIENT_BITMAP_FONT_OK) {
            manager_fail(e, MC_CLIENT_MANAGER_FONT_FAILED,
                         "TH retained BitmapFont occurrence retirement", NULL, NULL, NULL, -1,
                         true);
            return false;
        }
        e->font = NULL;
        if (current)
            m->fields.p = NULL;
        m->fonts = p->next;
        --m->retained_fonts;
        m->classes->bytes -= sizeof(*p);
        free(p);
    }
    while (m->settings != NULL) {
        manager_settings_occurrence *p = m->settings;
        const bool current = m->fields.k == p->value;
        if (mc_game_settings_release_borrow(&p->value, &e->settings) != MC_GAME_SETTINGS_OK) {
            manager_fail(e, MC_CLIENT_MANAGER_SETTINGS_FAILED,
                         "TH retained Settings occurrence retirement", NULL, NULL, NULL, -1, true);
            return false;
        }
        if (current)
            m->fields.k = NULL;
        m->settings = p->next;
        --m->retained_settings;
        m->classes->bytes -= sizeof(*p);
        free(p);
    }
    while (m->engines != NULL) {
        manager_engine_occurrence *p = m->engines;
        const bool current = m->fields.e == p->value;
        e->engine_status = mc_client_render_engine_release(p->value);
        if (e->engine_status != MC_CLIENT_RENDER_ENGINE_OK) {
            manager_fail(e, MC_CLIENT_MANAGER_ENGINE_FAILED,
                         "TH retained Engine occurrence retirement", NULL, NULL, NULL, -1, true);
            return false;
        }
        if (current)
            m->fields.e = NULL;
        m->engines = p->next;
        --m->retained_engines;
        m->classes->bytes -= sizeof(*p);
        free(p);
    }
    mc_entity *living = m->living_occurrence;
    m->living_occurrence = NULL;
    m->fields.h = NULL;
    mc_entity_release(living);
    return true;
}
