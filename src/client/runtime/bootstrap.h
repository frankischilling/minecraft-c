#ifndef MC_CLIENT_RUNTIME_BOOTSTRAP_H
#define MC_CLIENT_RUNTIME_BOOTSTRAP_H

#include "../application.h"
#include "../startup/application_owner.h"
#include "mc/core/math/classes.h"
#include "mc/core/math/math.h"
#include "mc/resources/files/files.h"

typedef struct mc_client_bootstrap mc_client_bootstrap;
typedef struct mc_client_render_engine_classes mc_client_render_engine_classes;

typedef enum mc_client_bootstrap_status {
    MC_CLIENT_BOOTSTRAP_OK,
    MC_CLIENT_BOOTSTRAP_BUSY,
    MC_CLIENT_BOOTSTRAP_INVALID_ARGUMENT,
    MC_CLIENT_BOOTSTRAP_UNSUPPORTED_OPTION,
    MC_CLIENT_BOOTSTRAP_ALLOCATION_FAILED,
    MC_CLIENT_BOOTSTRAP_OWNER_FAILED
} mc_client_bootstrap_status;

/* The native main/video thread owns one complete application and its service
 * namespaces. The actual constructor and default no-connection startup must
 * return through Minecraft.a749 before open succeeds. The run clock/frame loop
 * is the caller's responsibility. No READY state or successful child is supplied.
 *
 * *out must initially be NULL. Once allocation succeeds, even a failed open
 * returns its retained context in *out; close it before releasing the caller's
 * native platform. Diagnostics report the actual failed owner/site to stderr.
 * CLI game-directory, fixed-time, GUI-scale and World-inspection overrides use
 * separate diagnostic entry points until their source integration is available. */
mc_client_bootstrap_status mc_client_bootstrap_open(const mc_client_application_options *,
                                                    mc_client_bootstrap **out);

/* Retire caller-owned renderers, assets and other borrowed users first. Native
 * cancellation/join, manager/graphics terminal retirement and App callback-cycle
 * teardown retain their real dependencies on BUSY or failure. Retry while *slot
 * remains nonnull; success consumes/nulls it. No source J/o store is synthesized.
 * All borrowed getters expire at the first close attempt. A live transition
 * lease returns BUSY before startup/global fallback destruction; keep that
 * same owner graph for explicit terminal preparation and cleanup retry. */
mc_client_bootstrap_status mc_client_bootstrap_close(mc_client_bootstrap **slot);

/* Explicit native dispatcher retirement after the caller's actual menu/Load/N
 * terminal prefix. Keep the complete App/World/Player/services graph exclusive
 * and live. End only this startup's real consumer, passively discover existing
 * LL, then retire LL and TH (including RA) through their checked owners. BUSY
 * retains each remaining owner for retry; successful earlier consumption is
 * final and borrowed source views expire. No World container/cache/provider or
 * handler cleanup occurs. Success alone is not World/App destruction authority.
 * Normal close does not implicitly select this policy. A prior close attempt
 * does not prevent cleanup-only continuation through the SAME live App,
 * startup and inactive prepared terminal transition lease. Startup/App identity
 * must still match; legacy startup RETIRING cannot be upgraded without its
 * actual terminal consumer policy. Use the retained bootstrap association:
 * expired getters stay closed, retiring is never undone, and no source entry
 * or new borrow is authorized by this cleanup call. */
mc_client_bootstrap_status mc_client_bootstrap_terminal_dispatchers(mc_client_bootstrap *);

client_application *mc_client_bootstrap_app(mc_client_bootstrap *);
const mc_registry_resources *mc_client_bootstrap_resources(const mc_client_bootstrap *);
mc_resource_files *mc_client_bootstrap_class_files(const mc_client_bootstrap *);
const mc_math_trig *mc_client_bootstrap_trig(const mc_client_bootstrap *);
mc_math_classes *mc_client_bootstrap_math(const mc_client_bootstrap *);
mc_client_render_engine_classes *mc_client_bootstrap_graphics(const mc_client_bootstrap *);

#endif
