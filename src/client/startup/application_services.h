#ifndef MC_CLIENT_STARTUP_APPLICATION_SERVICES_H
#define MC_CLIENT_STARTUP_APPLICATION_SERVICES_H

#include "application_owner.h"
#include "mc/core/java/print_stream.h"

/* Retain the SAME already-bootstrapped System-output service associated with
 * this actual MC runtime/class namespace. Native binding only: no bootstrap,
 * source System.out read, stream replacement, output or class active use.
 *
 * The class namespace remains live through this exclusive association call.
 * A successful output owns one counted System occurrence, independently of any
 * nullable App value. Release it with the canonical System-output API. Error is
 * mandatory. Failure preserves out. Caller slots/errors must be disjoint.
 * Later cz source calls read the service's current out at their actual sites.
 * The selected native mutex profile requires successful unlock by the owning
 * thread for this live valid class mutex. Violating that ownership/lifetime
 * invariant is outside the supported contract, not a recoverable call result. */
mc_java_print_status mc_client_application_classes_output_service(mc_client_application_classes *,
                                                                  mc_java_system_output **out,
                                                                  mc_java_print_error *error);

#endif
