# Plant placement and survival predicates

[`plants.h`](../../include/mc/world/runtime/plants.h) exposes the actual Block
receiver's can-place and survival methods for flowers, saplings, crops, tall
grass, dead bushes, mushrooms, reeds, cactus and pumpkins. Dispatch follows the concrete receiver
kind and proved inheritance. It does not select behavior from a numeric ID or
a detached material/property table. Other receiver kinds fail explicitly.

| Receiver | Can-place | Survival |
| --- | --- | --- |
| Flower, sapling and tall grass | Base target replaceability, then grass/dirt/farmland substrate | Plant light at least8 or visible sky, then the same substrate |
| Crops | Base target replaceability, then farmland substrate | Inherited flower light/sky test, then farmland substrate |
| Dead bush | Inherited flower placement with sand-only substrate | Inherited flower light/sky test, then sand-only substrate |
| Mushroom | Inherited flower placement with the live Block.o substrate | Y0..127, plant light below13, then live Block.o substrate |
| Reeds | Same receiver ID below, or grass/dirt beside actual shared water material | Calls its actual can-place method |
| Cactus | Base target replaceability, then its survival method | Test neighbor materials in -X,+X,-Z,+Z order; reject the first solid one, then require cactus or sand below |
| Pumpkin | Target replaceability, then actual solid support below | Proved inherited base literaltrue body |

Placement and survival are distinct methods. For example, mushroom placement
does not add survival's light bound. Reeds do not accept sand in this source
body. Flower survival does not invent a target-air or Y-bounds gate. Each source
static field is loaded only when its branch reaches it; Registry aliases and
nullable values keep their actual short-circuit behavior.

The implementation invokes the concrete [World queries](queries.md), material
getter and solid-support method. Material failures carry a fresh nested Registry
frame rather than copying an unrelated earlier World diagnostic. Mushroom
substrate captures the actual opacity-array reference before its separate
element read. This same captured-array operation is needed by tree leaf checks.

The World and its bound Registry remain live throughout each call. Ordinary
Boolean results and native failures are separate. Failures preserve the caller's
result and earlier source effects, including acquisition and the shared positive
sky-light flag. Errors identify the deepest predicate, its immediate predicate
caller, and fresh World-query, material, support or Registry causes.

The source mapping is [documented separately](../reference/plant-predicates.md).
The [six plant feature families](population/plants.md) invoke these predicates
and actual quiet World setters. Neighbor and random-tick callbacks, drops and
the complete normal population controller remain separate requirements. Native
trials cover all nine receiver kinds, distinct placement/survival branches,
each reed/cactus neighbor direction, late nullable predicates and prefix
behavior. These are prepared native inputs; original/native plant comparisons
remain pending.
