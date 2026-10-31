# Entity movement

The [movement interface](../../include/mc/entity/movement/movement.h) implements
the base movement body for actual READY Item and FallingSand instances. It uses
their existing coordinates, watcher, Random, counted World binding and permanent
bounds. The [source map](../reference/entity-movement.md) distinguishes complete
source bodies, original observations and native verification.

## Movement order

No-clip translates the same permanent bounds and recomputes position, returning
before any World access. A genuine null World is compatible with this branch;
native READY, canonical-kind, owner and strict-FP admission still apply.

Ordinary movement first decays smoothing and consumes an existing web flag,
including its velocity stores. It then obtains an actual pooled copy. Grounded
sneaking trims X and Z separately through the real watcher and collision query.
The main collision sweep clips Y, then X, then Z, translating the same permanent
box after each axis. The source cancellation flag can zero all remaining local
deltas without undoing an earlier translation.

A step trial copies the flat result, restores the initial box and performs a
second real query. Its downward clip uses the current float step height. A flat
horizontal distance greater than or equal to the trial restores the flat result,
including ties. Kept-step smoothing uses Java integer truncation of minY rather
than mathematical floor.

Final coordinates and collision flags precede the actual fall callback; changed
axis velocities reset only after that callback returns. Both current concrete
kinds return false from their real walking predicate, so walking effects are
skipped through actual virtual dispatch. Loaded-area and Block contact traversal
then precede wet, burning, damage, fire-counter and extinguishing behavior.

Extinguishing resolves the actual code literal before two Entity Random draws.
The Entity-based World sound call precedes the final fire-counter reset. A late
failure preserves the draws and every earlier field effect. The World receiver
is reloaded at each source call rather than captured once for the whole move.

## Identity and ownership

Movement retains the actual Entity, pooled initial/flat boxes and returned
[collision List views](../world/collision/boxes.md) through their final uses.
Each clipping loop reloads the current List size and selected element. A second
query can clear and refill the same K identity while an earlier view remains
held. Explicit pool reset can overwrite a held pooled box. Holds protect
lifetime; they do not freeze values or collection contents.

All private holds and views release on success and failure. Release does not
reset the pool, clear K, restore source fields, publish/remove an Entity or save
a Chunk. The actual permanent bounds remain the Entity's existing object.

Errors preserve the outer movement PC and independent bounds, collision,
Block-read, World, contact, effect and Registry children. Constructor diagnostics
remain untouched. Successful movement reports its actual return PC88 or PC2028;
effects and World queries have their own documented success frames.

## Effects and World dependencies

The [effects interface](../../include/mc/entity/movement/effects.h) reads the
actual typed watcher Map/Byte, signed flag shifts and sneak bit. Missing records
and unsupported native typed projections fail at reached operations. These
native controls do not establish general Java Object or wrong-cast equivalence.

Fall-state preserves accumulation and callback/reset order. The current base
float callback returns for a null passenger; an unowned nonnull passenger stops
explicitly at its missing recursive virtual. Item damage sets attacked,
subtracts health with Java wrapping and sets dead when reached. Falling's base
damage sets attacked without Item health changes. Item's fire override bypasses
the base immunity test; Falling inherits it. These effects do not remove World
or Chunk occurrences or create drops.

The [World dependency interface](../../include/mc/world/runtime/movement.h)
uses actual nonloading provider presence, interpolated rain strength, acquiring
sky/top queries, current WorldProvider point-Biome ownership and current Fire/Lava
statics. Even an all-air burning scan reaches its ordered class/static reads.
The distinct Entity-sound overload returns from a real empty WorldAccess List
before reading the Entity or doing its argument arithmetic. Typed nonempty
WorldAccess dispatch is implemented; concrete audio receivers remain unfinished.
Acquiring shared queries reject an
unsupported current provider binding instead of running an older context.

[Block contact dispatch](../../include/mc/world/runtime/contact.h) follows actual
canonical inheritance. The 63 native kinds inheriting the base body genuinely
return without work. Portal relationship guards, soul-sand X/Z products, cactus
damage and web's next-move flag use their actual bodies. Pressure and detector
contacts execute their real remote/metadata guards. The
[wooden pressure helper](../world/pressure-plates.md) queries actual published
entities and performs ordered activation, notifications and scheduling. Stone
and Player Class queries and the detector's positive helper remain explicit
reached dependencies. No empty Class query or substitute Entity supplies them.

## Native limits

`mc_entity_move` accepts an optional `mc_entity_move_limits`. A null limits
pointer selects 1,048,576 work units. Units count ledge collision attempts,
indexed clipping iterations and contact cells. Exhaustion returns LIMIT at the
pending source operation with its completed prefix; it never reports truncated
work as successful movement. This is a native bounded-execution policy rather
than original Java behavior. Allocation and descendant query limits remain
separate; this is not a total simulation-work or elapsed-time bound.

The implementation requires strict x86-64 binary32/binary64 evaluation. Inputs
are not clamped to finite values, and source association, narrowing, signed zero
and wrapped integer behavior remain significant. Numerical source review and
original observations do not themselves qualify every compiled composition.

## Verification scope

Focused MSVC, GCC and Clang receipts pass five cases: `entity.movement` with 544
checks, `world.movement_dependencies` with 202, `world.nearby_entities` with 606,
`world.collision_boxes` with 732 and `world.queries` with 6,114. Linux enables
ASan/UBSan/leak checks. These finite
constructed contracts cover actual GAME owners, collision order, step/tie
behavior, web delay, effects, late failures, native limits, mutable aliases and
retirement. All 476 C files and headers pass the requested clang-format18 style.
The final World test also passes a fresh GCC recheck after explicit test-output
initialization; production bytes are unchanged. Full local suites were not run
because retained builds exceeded available disk space. The configured native
inventory is 167 MSVC and 169 per Linux compiler; hosted CI runs that complete
inventory separately.

The [native replay](../../tests/tools/entity/movement/compare_movement.py) passes
all 59,654 qualified client rows and 24,109 intact server rows on MSVC, GCC and
Clang. Each server process executes the complete 35,748-input prefix, including
11,639 state-only rows, to preserve real pool history. All snapshot bits, pool
tails, aliases and intermediate sequence snapshots compare exactly. The only
row-field differences are independently guarded native-failure spelling and
the disclosed native field-preparation provenance. Constructor Math entropy
remains independently retained.

These probes relink byte-identical qualified core archives with unchanged
production/fixture inputs. A later Python-only bootstrap fix has a separate
input binding to the unchanged C objects and binaries; it is not a new C build.
This replay does not add a full current local-suite result. The original server
family remains incomplete, with its disk/receipt/provenance limits retained.
The source map records [reproduction and exact scope](../reference/entity-movement.md#retained-original-native-replay).
Completed original normal movement and nonnull-World traversal remain unqualified.

Other Entity kinds, walking-positive descendants, active Item/Falling ticks,
landing/drop behavior, detector helpers, concrete audio WorldAccess receivers,
unowned passenger recursion and whole-game acceptance remain
unfinished. Implementing movement does not complete those gameplay routes.
