#ifndef MC_CLIENT_WORLD_FRUSTUM_INTERNAL_H
#define MC_CLIENT_WORLD_FRUSTUM_INTERNAL_H

#include "mc/client/world/frustum_culling.h"

/* Internal callers can request diagnostics only for a failed box test. */
mc_client_frustum_status mc_client_frustum_box_test(const mc_client_source_dr *, double min_x,
                                                    double min_y, double min_z, double max_x,
                                                    double max_y, double max_z, bool *out,
                                                    mc_client_frustum_error *, bool report_success);
mc_client_frustum_status mc_client_source_frustum_bounds_test(mc_client_source_frustum *,
                                                              const mc_entity_bounds *, bool *out,
                                                              mc_client_culling_error *,
                                                              bool report_success);

#endif
