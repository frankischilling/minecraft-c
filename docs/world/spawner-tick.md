# MobSpawner ticks

The client World Entity loop enters the actual MobSpawner `cy.n_()V` body.
It uses the TileEntity's defining Entity and Registry namespaces independently
of its mutable World field. The [rendered Pig preview](../client/spawner-rendering.md)
has a separate cache and never supplies a spawning candidate.

## Source order

The prefix stores the previous rotation, asks the real World for the nearest
Player within sixteen blocks, emits smoke and flame, advances rotation, then
tests the remote flag and delay. Positive delays decrement and return. An
expired delay enters up to four attempts:

1. The actual named factory constructs a fresh candidate. Unknown names return
   NULL and end the tick. A registered nonliving candidate fails the Living cast
   outside the factory's reflection catch.
2. The candidate's actual Class selects a fresh World Class/AABB query. Its unit
   box and expanded box use the real shared bounds pool. Six matching nearby
   Entities reset the delay and end the tick, after construction has already
   consumed its ID and entropy.
3. Fresh World Random reads select position and yaw. The inherited pose method
   updates the actual Entity and bounds. Pig eligibility uses the existing
   Grass, light, collision, liquid and path-weight methods.
4. An eligible candidate enters actual World insertion. The returned boolean
   is discarded. Normal false therefore still reaches effects and delay reset;
   a failed insertion preserves any Chunk and World publication prefix.
5. Twenty shared smoke/flame positions precede the separate Living `ls.V()V`
   method. That method emits twenty explode particles using the candidate's
   own Gaussian and float draws, including the three velocity-times-ten
   position offsets. Only then does the Spawner reset its delay.

Success continues to the next attempt. Each attempt performs a new population
query, so earlier publications can make a later attempt reach the cap. Rejected
candidates do not reset the delay. A complete four-attempt loop reaches the
inherited empty TileEntity tick.

## Native ownership and failures

Each query result owns its fresh List and Entity occurrences. The caller ends
those holds after the reached size read and balances both returned pooled box
holds without resetting the source pool cursor. A candidate's temporary hold
lasts through pose, eligibility, publication and effects. Ending that temporary
hold never removes the World or Chunk occurrences created by publication.

An expired branch requires an outer error output before entering the factory.
The tagged Spawner error retains a failed candidate, its evaluated World and
the exact reached WorldAccess receipt when one exists. Query and predicate
errors retain their full selected descendants. Receipt-capture refusal is
reported separately and cannot replace the primary source failure. Callers
keep the defining namespaces and reached graph live and exclusive until
`mc_world_entity_tick_error_retire` ends the diagnostic owner. Retirement does
not replay source work or restore Tile fields.

The outer World sets `L=true` before active TileEntity traversal. A failure
leaves that prefix intact. Only normal traversal completion clears it and
drains the deferred TileEntity List. Client terminal cleanup retires Spawner
and Furnace errors before ending their World graphs.

Copied counters record reached factory, query, predicate, insertion, particle
and delay calls. The client loop consumes each tick's observation once before
the next Timer tick can replace it. Saturation is visible in the accumulated
diagnostics and controls no gameplay or scheduling.

## Evidence boundaries

Native controls use authored saved NBT, genuine Player construction and
publication, real Chunks and predicates, and a declared initial Random seed.
They check IDs, both Random streams, pool holds, source failure prefixes,
actual publication membership and terminal ownership. Those authored seeds
are separate from original constructor entropy.

The [original packet](../../tests/reference/world/spawner_tick/README.md)
contains twenty direct expired Tile ticks across two ordinary JVM modes.
Native tests compare the four stable unknown-name, Arrow-cast, six-Pig cap and
Stone-rejection projections: ID delta, return, rotation and initial particle
count, plus the cast's exception class. Successful effect counts come from
all thirteen observed original publication groups. The original Grass cases'
different positions, delays and success counts remain separate from the
native seeded controls. This is not complete original/native field equality
or an original whole-World-loop trace.

The native matrix passes 518 Spawner checks across sixteen fresh Worlds and
277 Living-effect checks per host on MSVC Release, GCC 15.2 and Clang 21.1.8.
Both Linux builds use Debug `-g1`, ASan, UBSan and leak detection. They also
pass the World Entity-loop regression and all seven new registered tests.
The original packet rejects 69 corruptions in ordinary and optimized Python.

Four fresh serial Windows SDK processes on monitor2 qualify real countdown,
two independent spawn/save/reselection runs and failure cleanup. Their check
counts are 374, 942, 971 and 461. The two save runs insert one and two actual
Pigs, then match their immutable imported NBT after ordinary reselection with
no fresh Spawner insertions. Current runtime fields are sampled separately
because ordinary ticks can evolve them after loading.

The failure observer implements the full passive WorldAccess interface. It
removes its own registration and fails at the first post-publication smoke
callback. After the actual Frame close, App terminal preparation preserves the
sampled World time, Tile fields and published candidate while ending the
owned failed child. Only its one expected client diagnostic is admitted on
stderr. Earlier observer and reader failures remain separate attempts.

The [sanitized SDK record](../../tests/benchmarks/spawner_tick_windows.json)
binds the four processes to one executed product and 2,362 selected inputs,
plus images and private receipts. These are text-mode SDK stream records,
not raw OS pipes. All Pig-bearing Chunks in the qualifying positive captures
were loaded. The independent decoder classifies disk records by witnessed
nonloading Chunk presence; its absent-Chunk branch has synthetic controls,
without a new dynamic absent-Chunk qualification.

The complete MSVC Release build and all 1,352 configured Windows tests pass in
448.92 seconds. Every prior test remains with exactly seven additions; unique
names, IDs, completion ordinals, Start/completion joins and JUnit records agree.
There are no skipped or unrun tests. All 6,465 public worktree files stayed
unchanged throughout the sweep. GCC and Clang each pass the eight-test affected
selection in 9.11 and 9.07 seconds. Full local Linux suites are not claimed.
Validation text and current admission fingerprints were updated afterward;
runtime and native harness bytes remained unchanged. Hosted results retain
their own exact-commit scope.

The public Living effect entry also admits NaN positions and sizes through
the real Entity setters. Two native controls check the existing
[interpreter arithmetic profile](../reference/bounds.md), including the
quieted right operand for a two-NaN addition. They add no fresh original
Living-method or default-JIT NaN-payload qualification.

Normal false insertion is implemented from the source's discarded boolean.
It is not forced in the stable Pig fixture: the preceding Grass lookup has
already acquired the candidate Chunk. Unsupported named constructors remain
explicit dependencies. Static paired server bytecode does not qualify a
server runtime, original graphics, full-game parity or FPS.
