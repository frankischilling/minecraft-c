#ifndef MC_CLIENT_TEXTURE_EFFECTS_LAVA_CONSTRUCTION_H
#define MC_CLIENT_TEXTURE_EFFECTS_LAVA_CONSTRUCTION_H
#include "mc/client/texture_effects/lava.h"

/* Private actual cg constructor allocation on the SAME owner/body. No trig,
 * lookup/Math activation, FP guard or animation is required by construction.
 * Source fields/defaults and Registry/profile holds are the canonical owner.
 * Constructor-ready does not make an unbound native animation context ready. */
mc_lava_texture_status mc_client_lava_texture_allocate_constructor(mc_registry_classes *,
                                                                   mc_java_random_profile *,
                                                                   const mc_lava_texture_options *,
                                                                   mc_client_lava_texture_effect **,
                                                                   mc_lava_texture_error *);
/* Native prerequisite only. Borrow a real initialized immutable lookup table
 * through effect retirement; no initialization/lookup/source store occurs here.
 * Bind once after actual construction; same identity is idempotent. Existing
 * public allocation-with-trig and animation behavior stay unchanged. */
mc_lava_texture_status mc_client_lava_texture_bind_trig(mc_client_lava_texture_effect *,
                                                        const mc_math_trig *,
                                                        mc_lava_texture_error *);
#endif
