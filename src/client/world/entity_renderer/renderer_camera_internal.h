#ifndef MC_CLIENT_RENDERER_CAMERA_INTERNAL_H
#define MC_CLIENT_RENDERER_CAMERA_INTERNAL_H
#include "mc/client/world/renderer_camera.h"
/* Actual px.a(FI), called synchronously with its real parent PX guard entered.
 * The call borrows that parent occurrence through retirement, owns reached
 * Entity/Math/Vec3/ray holds, and publishes each GL matrix effect immediately.
 * It uses canonical PX/App fields; the older prepared first-person component
 * is a separate entry. Out begins NULL. Failure is terminal, never replayed. */
mc_client_renderer_camera_status entity_renderer_camera_entered(mc_client_source_px *,
                                                                mc_math_classes *, float, int32_t,
                                                                mc_client_renderer_camera_call **,
                                                                mc_client_renderer_camera_error *);
/* Actual reusable px.e(F) hurt / px.f(F) bob children. These run the same
 * bodies used by camera setup and keep their captured Entities until retired. */
mc_client_renderer_camera_status
entity_renderer_camera_effect_entered(mc_client_source_px *, mc_math_classes *, float, bool bob,
                                      mc_client_renderer_camera_call **,
                                      mc_client_renderer_camera_error *);
#endif
