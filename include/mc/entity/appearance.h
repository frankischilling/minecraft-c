#ifndef MC_ENTITY_APPEARANCE_H
#define MC_ENTITY_APPEARANCE_H

#include "mc/entity/player/player.h"

/* Actual client sn.u_ virtual dispatch for the currently concrete native
 * owners: Item/FallingSand/Minecart/pickup particle and the eleven concrete
 * Spider/Zombie/Skeleton/Creeper/Slime/Pig/Sheep/Cow/Chicken/Wolf/Squid owners
 * inherit sn's empty return;
 * LocalPlayer inherits gs.u_. Dispatch uses the SAME canonical Class literal,
 * never a caller-selected kind or the presence of an invented Player record.
 * RAW and failed owners are admitted too: neither source body has a READY gate.
 * Unknown descendants and server owners remain explicit native rejections.
 *
 * gs appends its actual nullable l username as counted UTF16 (NULL -> "null"),
 * stores a fresh String to n28, then that SAME identity to Entity.bB36. Both
 * fields own independent native occurrences; a later native hold failure keeps
 * the already stored n and the old bB. Constructor bA is untouched. No Random,
 * World, Registry, network, authentication or source callback is reached.
 * Keep owners live/exclusive and error storage disjoint. The call takes a
 * temporary Entity hold. Native String limits use MAX_TEXT_UNITS and preserve
 * prior fields; no JVM allocation timing or StringBuilder object identity is
 * implied by the bounded immutable String implementation. */
mc_player_status mc_entity_update_cloak(mc_entity *entity, mc_player_error *error);

/* Passive reads of the actual Entity String fields, with no allocation,
 * activation or hold transfer. NULL entity returns NULL as a native query;
 * source callers perform their own receiver admission at the actual call site.
 * Borrow while Entity is held/exclusive. Cloak views expire on update or final
 * Entity release; retain the SAME String for independent use. */
const mc_player_string *mc_entity_skin_url(const mc_entity *entity);
const mc_player_string *mc_entity_cloak_url(const mc_entity *entity);

#endif
