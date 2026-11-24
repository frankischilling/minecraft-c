#ifndef MC_CORE_JAVA_OBJECT_IDENTITY_INTERNAL_H
#define MC_CORE_JAVA_OBJECT_IDENTITY_INTERNAL_H

#include <stdint.h>

/* Private selected HotSpot17 hashCode=5 transformation. These words belong to
 * the current logical Thread, above Worlds and collections. Authored unit
 * states exercise the conditional transformation independently of startup. */
typedef struct mc_java_identity_thread {
    uint32_t x, y, z, w;
} mc_java_identity_thread;

/* A distinct VM-identity child of the application profile, shared above
 * registries/Worlds. Native startup uses its own Park-Miller stream beginning
 * at1234567 and constructs one admitted main logical Thread with the selected
 * Thread constructor constants. It consumes neither Java Random nor Math and
 * does not reproduce the selected JVM's bootstrap object/hash history.
 * Additional logical Threads need explicit construction from this same
 * startup stream; creating a fresh profile per World or Set is invalid. */
typedef struct mc_java_identity_profile {
    uint32_t startup_random;
    mc_java_identity_thread main_thread;
} mc_java_identity_profile;

/* Fresh native owner only, exactly once before publication. No reset/reseed,
 * clock, I/O, allocation or captured input is accepted. */
void mc_java_identity_profile_initialize(mc_java_identity_profile *profile);

/* One cell on the actual object: zero means unassigned, otherwise a positive
 * 31-bit cached identity hash. Initialize new objects to zero without drawing.
 * Do not copy this cell on object construction, reload or serialization.
 * Moving an existing native owner preserves the cell. */
typedef struct mc_java_identity_cell {
    uint32_t hash;
} mc_java_identity_cell;

/* NULL cell denotes the actual null reference: return zero without accessing
 * Thread. A cached cell also leaves Thread untouched. An unassigned cell needs
 * a valid admitted current Thread, draws once, retains the full unmasked W and
 * installs the masked nonzero hash (0xBAD if the masked draw is zero).
 *
 * Cells/Threads require exclusive access, valid initialized state and disjoint
 * storage. This models neither JVM monitors/GC headers nor competing hash
 * installation. There is no allocation, I/O, clock or application Random use.
 * The caller must preserve completed hash effects across later failures. */
uint32_t mc_java_object_identity_hash(mc_java_identity_thread *thread, mc_java_identity_cell *cell);

#endif
