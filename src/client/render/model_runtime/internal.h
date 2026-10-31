#ifndef MC_CLIENT_RENDER_MODEL_RUNTIME_PRIVATE_INTERNAL_H
#define MC_CLIENT_RENDER_MODEL_RUNTIME_PRIVATE_INTERNAL_H

#include "../../../entity/player/tick_math_internal.h"
#include "../model_runtime_internal.h"

void model_runtime_at(mc_client_model_runtime_error *, const char *, const char *, const char *,
                      int32_t);
bool model_runtime_fail(mc_client_model_runtime_error *, mc_client_model_runtime_status,
                        const char *);
bool model_runtime_null(mc_client_model_runtime_error *, const char *);
bool model_runtime_admit(mc_client_managed_renderer *, manager_model *, mc_client_graphics_site,
                         mc_client_model_runtime_error *);
bool model_runtime_quadruped(const manager_model *);
bool model_runtime_fp(mc_client_model_runtime_error *);
bool model_runtime_math(const mc_client_model_runtime_context *, mc_client_model_runtime_error *);
bool model_runtime_call_push(mc_client_model_runtime_error *);
void model_runtime_call_pop(mc_client_model_runtime_error *);
bool model_runtime_pose_body(manager_model *, const float[6],
                             const mc_client_model_runtime_context *,
                             mc_client_model_runtime_error *);
bool model_runtime_draw_part(mc_client_managed_renderer *, mc_client_model_part *, float,
                             const mc_client_model_runtime_context *,
                             mc_client_model_runtime_error *);
bool model_runtime_nn_pose(manager_model *, const float[6], const mc_client_model_runtime_context *,
                           mc_client_model_runtime_error *);
bool model_runtime_nn_render(mc_client_managed_renderer *, manager_model *, const float[6],
                             const mc_client_model_runtime_context *,
                             mc_client_model_runtime_error *);
bool model_runtime_hh_pose(manager_model *, const float[6], const mc_client_model_runtime_context *,
                           mc_client_model_runtime_error *);
bool model_runtime_hh_render(mc_client_managed_renderer *, manager_model *, const float[6],
                             const mc_client_model_runtime_context *,
                             mc_client_model_runtime_error *);
bool model_runtime_td_pose(manager_model *, const float[6], const mc_client_model_runtime_context *,
                           mc_client_model_runtime_error *);
bool model_runtime_td_render(mc_client_managed_renderer *, manager_model *, const float[6],
                             const mc_client_model_runtime_context *,
                             mc_client_model_runtime_error *);

#endif
