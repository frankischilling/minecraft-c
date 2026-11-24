#ifndef MC_ENTITY_ACTION_ATAN_INTERNAL_H
#define MC_ENTITY_ACTION_ATAN_INTERNAL_H
/* Stage-private actual-JDK17 atan kernel, independently ported from the
 * permissive Netlib origin identified in licenses/fdlibm-action-atan.txt.
 * Require binary64 round-to-nearest, gradual underflow and strict operations;
 * no errno/fenv-flag parity. All IEEE input bits are accepted. */
double mc_action_atan(double value);
#endif
