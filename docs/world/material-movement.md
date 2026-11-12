# Material immersion and acceleration

The GAME World now has the three reached base methods below. Entity eye
immersion uses the same real Entity, position, World, Block and Material owners.
All runtime additions are C17. They do not execute Living movement, Player tick,
fluid ticks, gravity, jump input or presentation.

| Native operation | Client | Paired server |
| --- | --- | --- |
| `mc_world_runtime_contains_material` | `fd.a(Leq;Lln;)Z` | `dj.a(Lcz;Lhj;)Z` |
| `mc_world_runtime_contains_liquid` | `fd.b(Leq;)Z` | `dj.c(Lcz;)Z` |
| `mc_world_runtime_material_accelerate` | `fd.a(Leq;Lln;Lsn;)Z` | `dj.a(Lcz;Lhj;Llq;)Z` |
| `mc_entity_inside_material` | `sn.a(Lln;)Z` | `lq.a(Lhj;)Z` |

Each World scan floors the actual minimum and maximum-plus-one coordinates,
then visits X/Y/Z with exclusive upper bounds. The liquid scan additionally
decrements each negative minimum. It calls the actual Material `d` predicate
of each nonnull Block. The material scan compares actual nullable Material
identities. It has no liquid-height test. These two scans use acquiring World
getters without a presence preflight. The source's asymmetric World borders,
Y bounds, actual Blank Chunk behavior and nested acquisition failures remain
those of the existing getters. Reversed and nonfinite input keeps the Java
narrowing, wrap and comparison rules.

Acceleration first performs the existing inclusive, nonloading chunk-presence
check, including its Y early return. Missing presence returns false before
Vec3 class initialization. A present region allocates the actual accumulator
from the loaded `bt/ba` pool. Matching Blocks use metadata and the float fluid
height formula in source order. Flowing and still fluid execute the real
`rp/kv` neighbor/material/decay/vector body. Falling flow checks the eight
source faces in order, excludes the actual ice identity, and conditionally
normalizes then adds `(0,-6,0)`. Stairs `ss/lu` invokes its actual wrapped Block;
the remaining registered concrete classes inherit the bytecode-proved empty
base acceleration body. The whole JAR audit finds exactly these three
declaring classes among all 73 Block subclasses on each side.

The final vector length and normalization narrow `sqrt` to float before
widening back to double. A positive accumulated length causes X, Y and Z
velocity stores, each adding its normalized component times the original
double `0.014`. A null Entity is reached only at those fields: a zero-vector
scan can complete with null. Matching material still returns true when the
resulting flow is zero or the actual Block acceleration body is empty.

The reached Vec3 factories, addition, normalization, length and pool lifecycle
live in the existing Entity classes owner. The vector pool has a separate lazy
class phase, cursor, List identity and native allocation/hold ledger. Source
reset changes only its cursor; later gets overwrite the same held objects.
Client clear detaches held vectors and retains backing capacity. Nothing
implicitly resets or clears the pool per World, Entity or query. Direct
construction canonicalizes signed zero; pooled set keeps raw coordinates.
Class free is busy while any vector native hold survives. This is the reached
Vec3 subset; ray, rotation and String methods remain outside the implementation.

Entity immersion dispatches the actual eye-height method for the existing
canonical native owners. Item, Falling Sand and Minecart inherit zero;
LocalPlayer inherits `gs.w()F = 0.12f`. The original Living `height*0.85f` body
is not substituted for Player. The method preserves the double eye sum,
floor-to-int, float conversion and second floor for Y, nonair check, live
Block Material identity, metadata formula, and strict eye-below-surface test.
Other Entity kinds fail explicitly at the missing concrete eye dispatch.

Native owners must remain live and exclusive through synchronous descendants.
World APIs admit bound GAME prefixes and reject retirement/composition;
the public Entity operation requires a genuinely constructed, held READY
owner. Native failures preserve outputs and completed class, pool, acquisition
and field prefixes. Error records retain World, fluid, child, Stairs and
Registry/vector frames. The optional default 1,048,576 work ceiling counts
visited cells and reached Stairs delegations. Exhaustion is a checked limit at
the pending source operation, never a truncated successful answer. Existing
provider, Entity and Registry budgets remain independent and unchanged.

The source-pins metadata records both complete JAR hashes, 31 class identities,
1,173 method entries and 1,166 Code hashes. Twelve paired method comparisons
check instruction/PC/branch/immediate structure independently of constant-pool
indexes. The source dependencies and numeric constants were also inspected in
the corresponding unchanged disassemblies. Direct original `in/fq` probes
confirm that signed quiet/signaling NaN floors return zero; the existing
canonical floor helper is reused without a duplicate or a core change.

The authored native test constructs genuine client/server GAME Worlds,
Registry/Material owners, loaded Store/Chunk records, Falling receivers and a
real LocalPlayer/Session/Inventory/Container closure. It declares prepared
voxel/metadata bytes as inputs on those actual owners. The original observer
uses an actual direct-metadata World, real provider/generation-acquired Chunk,
Falling constructor and `dc(null,world,new gr(null,null),0)` constructor. It
does not replace game classes, fabricate a World/Player, invoke Unsafe or run
the original graphical setup. The real Stairs wrapped-fluid diagnostic changes
and restores its actual wrapped field on each side; it is a controlled input,
separate from normal registered Stairs. Null projections, allocator faults,
recursive Stairs and native ownership guards are native invariant tests.

The unchanged client oracle compares 300 full records: flow velocities and
pool cursors, bounds scans, all 16 fluid metadata levels at eight eye positions
for Falling and Player, and Vec3 numeric/nonfinite cases. The shared native
server crosscheck has 172 corresponding records; there is no claim that an
original server process was executed. Strict original/native controllers use
bounded capture, deadlines, zero exits, empty stderr and positive final markers.
The original runner requires matching JDK17 runtime/compiler from one bin
directory and checks the supplied JAR/library pins. It retains the authored
helper class hashes and checks the supplied unchanged archives. No original classes,
assets or captured answers belong in the repository.

Capture success also requires an explicit empty `reader_errors` dictionary.
Open/read/write/close failures are recorded, terminate the child and cannot
be mistaken for a completed empty stderr stream. Short writes are failures;
capture records only the bytes actually written. Authored fault controls cover
a real stderr-log write failure, a partial write, a close failure and legacy
metadata lacking the reader-error field, in addition to the existing eight
subprocess controls.
