#ifndef MC_CLIENT_LIVING_DRAW_BODY_INTERNAL_H
#define MC_CLIENT_LIVING_DRAW_BODY_INTERNAL_H
#include "../living_draw_internal.h"
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render_engine/nw.h"
#include "entity/entity_internal.h"
#include "entity/living/fields_internal.h"
#include "entity/mob/internal.h"
#include "entity/player/player_internal.h"
#include "entity/player/tick_math_internal.h"
#include "world/runtime/runtime_internal.h"
#include <string.h>

typedef struct ld_frame {
    mc_client_draw_scope *scope;
    mc_client_managed_renderer *renderer;
    mc_entity *actor;
    mc_client_living_draw_error *error;
} ld_frame;

mc_client_graphics_site ld_site(const char *, const char *, const char *, int32_t);
void ld_at(ld_frame *, const char *, const char *, const char *, int32_t);
bool ld_fail(ld_frame *, mc_client_living_draw_status, const char *);
bool ld_null(ld_frame *, const char *);
bool ld_fp(ld_frame *);
bool ld_work(ld_frame *);
bool ld_display(ld_frame *, bool);
bool ld_graphics(ld_frame *, bool);
bool ld_entity(ld_frame *, bool);
bool ld_model(ld_frame *, bool);
bool ld_capture(ld_frame *, bool);
mc_client_render_manager *ld_manager(ld_frame *, int32_t dereference_pc);
bool ld_texture(ld_frame *, const mc_player_string *, int32_t caller_pc);
bool ld_texture_engine(ld_frame *, mc_client_render_engine *, const mc_player_string *,
                       mc_client_graphics_site caller, mc_client_graphics_site lookup_site,
                       mc_client_graphics_site bind_site);
bool ld_skin(ld_frame *);
bool ld_brightness(ld_frame *, float, float *);
bool ld_sheep_pass(ld_frame *, int32_t, float, bool *);
bool ld_pig_pass(ld_frame *, int32_t, float, bool *);
bool ld_age(ld_frame *, float, float *);
bool ld_pass(ld_frame *, int32_t, float, bool *);
bool ld_shadow(ld_frame *, double, double, double, float, float);
void ld_registry_classify(ld_frame *, mc_registry_classes *, const mc_registry_error *);
void ld_access_classify(ld_frame *, const mc_world_access_failure *);
const mc_player_string *ld_fur_literal(void);
const mc_player_string *ld_shadow_literal(void);
#endif
