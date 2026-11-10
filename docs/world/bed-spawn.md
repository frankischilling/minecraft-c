# Saved-Bed spawn

The client implements the original `GS.a(World,BR)` saved-Bed check and
`VE.f(World,x,y,z,skip)` exit search on the actual client GAME World. The
respawn coordinator uses their result on its existing Load and World. A valid
Bed preserves the saved Bed coordinate in a newly constructed Player; a
missing Bed produces the ordinary translated HUD notice and uses World spawn.
The [death and respawn document](../client/death-respawn.md) covers the surrounding
Player replacement and screen lifetime.

The public component API is in
[bed_spawn.h](../../include/mc/world/runtime/bed_spawn.h), with the source body in
[bed_spawn.c](../../src/world/runtime/bed_spawn.c). It accepts the existing
`mc_player_coordinates` type and returns an owned nullable coordinate. A
nonnull result is a fresh `BR` identity, released with
`mc_world_bed_spawn_position_free`. Failure leaves the output slot unchanged.

## Saved-Bed validation order

`GS.a(World,BR)` captures `World.w()` once at bytecode 1. It invokes that same
provider's `c(II)` four times, in this order:

| Call bytecode | Wrapped Block coordinates before signed `>> 4` |
| --- | --- |
| 22 | `x-3, z-3` |
| 45 | `x+3, z-3` |
| 68 | `x-3, z+3` |
| 91 | `x+3, z+3` |

Each argument reads the coordinate fields again at its source position.
Callbacks can therefore change operands of later calls. Repeated Chunk
coordinates still produce four calls. The normal Map provider's `c` removes
the pending-unload key before its cache lookup, including a cache hit. Its
`b` getter can return a hit without that removal. The native adapter enters
the existing acquisition body with the captured provider, its actual cache,
pending set and owning World. It preserves that receiver if a callback changes
the World's current provider.

After all four calls return, GS reads the World's current Block at bytecode
110 using fresh coordinate fields. It then performs the late `UU.T` static
access at 113 and reads the Bed's current ID at 116. A different ID returns
null at 123. A match reloads the coordinate fields and invokes `VE.f` at 138
with skip zero, then returns its nullable result at 143. The helper does not
repeat this Bed-ID test.

The captured-provider adapter keeps the actual existing World acquisition
callbacks and diagnostic frame. Its generation and population children use
the captured provider receiver. It neither exports the cache nor substitutes
a second World. Native admission currently requires the normal client GAME
Map provider and its genuine bound acquisition context.

## Exit search

The private `bed_spawn_execution_internal.h` entry now runs the same VE exit
body for an explicit caller execution. It carries the independently supplied
Bed Registry and defining World code, with stable scalar/support frames in the
caller's heap. It does not use the saved-Bed GS validation wrapper as a search
substitute; that captured-provider wrapper remains separately unjoined.

Owned metadata and air calls use the [same World scalar bodies](blocks.md).
Held solid support still captures `Block.m` before its actual collision getter,
then indexes that captured array and reads Material/predicate only after both
channels complete. A native-only refusal stops before journal completion,
direction reads, another candidate or position publication. Actual lower raw
status is retained independently: a refused activation close produces its real
failed GET/status, while a separate completion refusal can retain actual GET OK
and raw OK. Neither case becomes an ordinary search result.

A nullable World token reaches the static Bed activation before its metadata
query throws at 4. Failed work keeps its reached ordinary World call and actual
lower diagnostics alive until explicit pending/close work drains under the
caller's exact scope. These helpers never guess a scope, replay a query or close
the caller's context. The native controls compare full quiet search journals
for skip 0/1/999, preserve the captured array through reentry, and exercise both
failure channels. Complete Player wake still needs notifying Bed occupancy and
its recursively reached mutation descendants.

The static VE invocation initializes the declaring Bed class before entering
its metadata body. At bytecode 4 it reads metadata and selects orientation
`metadata & 3`. The occupied and head bits do not change that orientation.
The actual Bed class owns the direction rows
`[(0,1),(-1,0),(0,-1),(1,0)]`.

Pass zero precedes pass one. For each pass, the source separately captures the
current Bed table at 26 and 43, loads its selected row at 31 and 48, and reads
the X/Z values at 33 and 50. It searches the inclusive square from
`(x-dx*pass-1,z-dz*pass-1)` through that point plus two on each axis. X is the
outer ascending loop; Z is the inner ascending loop. A callback in pass zero
can change the table values read for pass one. The registry accessors retain
the actual array identities, null checks and signed bounds checks, including
replaced rows backed by the Registry's existing allocation arena.

For each candidate, the source short-circuits these calls:

1. `World.h(cx,y-1,cz)` at 101 requires solid support. Its existing body
   captures `Block.m`, queries the Block, then checks the actual Material `h`
   and virtual Block `d` methods.
2. `World.d(cx,y,cz)` at 113 requires Block ID zero.
3. `World.d(cx,y+1,cz)` at 127 also requires Block ID zero.

A positive skip count decrements for each qualifying occurrence. Overlapping
cells in the two squares count again. Zero or negative skip selects the first
qualifying occurrence, allocates `BR` at 144 and returns it at 156. Exhaustion
returns null at 176. These methods perform no bed-pair validation, occupancy
mutation, liquid test, Entity-clearance query or Random draw.

Integer arithmetic and increments use Java wrapping. The default native work
ceiling is 1,048,576 entered loop steps. It counts both an outer X iteration
at 75 and a candidate at 93, so it also bounds wrapping loops with empty inner
ranges. An ordinary complete search visits at most 18 candidates and consumes
24 such steps. Exhausting the ceiling refuses before the next source step
with a native limit error; it never converts an unfinished search into null
success.

## Call and diagnostic ownership

The caller keeps the input coordinate live across all children. The Player's
[Q getter](../../include/mc/entity/player/coordinates.h) returns a scoped pin on
the actual nullable saved coordinate. That pin protects a node even if a
callback replaces the Player's field and the normal collector runs. The
coordinator owns this pin through its last source use. Merely retaining the
Player does not protect a replaced coordinate node from collection.

Once World admission is reached, a Bed call holds its source World. At the
first acquisition, the wrapper also holds the captured provider's owning
World. These are two occurrences even when both refer to the same World. A
failure returned through `mc_world_bed_spawn_error` transfers those native
holds to that error. Its
tagged acquisition, Registry or solid-support child retains the reached
diagnostic prefix and borrows the actual producer. The World holds prevent a
displaced provider or source loader from retiring under those views. Pending
load and activation work remains with its actual World or loader.

`mc_world_bed_spawn_error_retire` checks and consumes the retained World
occurrences. It performs no source retry, rollback, pending-load completion or
container cleanup. A failed owning error is noncopyable and must be retired
before reuse or graph destruction. The caller also keeps supplying Registry
dependencies live and excludes mutation of borrowed failure producers until
retirement. With a null error output, no diagnostic view escapes and temporary
World holds end before return.

The error includes a passive journal of reached provider, Block, metadata,
support and air invocations. Its 64 retained events record completed results
or a failed invocation's operand prefix. Journal overflow only limits retained
diagnostics; it cannot stop or complete gameplay work.

## Original and native component evidence

The [original packet](../../tests/reference/world/bed_spawn/README.md) contains
30 prepared helper/wrapper profiles from the unchanged supplied client archive,
SHA256 `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Two ordinary JDK17 interpreter VMs agree on the selected results, coordinate
identity relations, Block/metadata plane preservation and all three World
Random fields. A separate read-only JDI run agrees on those observations and
records 272 ordered caller-level World/provider queries. Seven reached method
Code bodies and twelve complete class origins/hashes are pinned.

Profiles cover all sixteen metadata values; first, last, second-pass-only and
absent exits; positive, zero and negative skips; overlap; Bed/non-Bed wrapper
results; positive and negative Chunk edges; and Y zero/127. The observers use
the genuine World, normal Map provider and sixteen normal Chunks. Constructor
state outside the selected prepared planes is outside the comparison.

`world.bed_spawn` passes all 2,014 declared scalar comparisons and 1,560 native
checks on Windows and Linux Clang with ASan/UBSan. The earlier 1,550-check
component also passes in the separate Linux client-off build. Its
[native driver](../../tests/world/runtime/test_bed_spawn.c) checks plane and
Random preservation, distinct result owners, null/short tables, exact failure
sites, loop limits, pending-key removal on repeated hits and checked error
retirement. Additional controls retain an actual acquisition-capacity refusal
as a limit and classify the actual null Block array in a support query as a
source exception. Authored native callbacks change a Bed row between passes, change
coordinate operands/current provider after acquisition, or refuse a real
loader call. Those ownership and refusal controls have native scope; the
successful original packet does not qualify their Java exception equivalence.
The packet readers reject 60 corruptions in each of normal and optimized
Python modes.

## Loaded respawn and save evidence

The Windows SDK scenarios create private saves with all 289 stored Chunks and
a genuine Player NBT saved spawn of `(3,64,4)`. The valid profile supplies Bed
foot `(3,64,3)` metadata zero and head `(3,64,4)` metadata eight, both at Y 64
above grass. The missing profile leaves both cells as air. An independent
decoder checks the Player input, both Bed cells, height/skylight consistency
and preservation of the other 288 compressed Chunk records before bootstrap.

Both scenarios enter the actual death screen after a declared direct
100-damage component input, then click its real SDK Respawn Button. The valid
profile completes two cycles, checks the newly copied saved coordinate and
near-Bed exit, and ends with 1,013 checks in 14.86 seconds. The missing profile
completes one cycle, checks null Q and the real UQ message, and ends with 977
checks in 11.00 seconds. Its UTF16 text matches the imported
`tile.bed.notValid` property and its live message age is nine, within the
visible range below 200. Both ran on monitor 2.

Every cycle checks the coordinator's saved-read/check/valid/notice/copy/position
flags, fresh Player/Inventory/Input owners, the same World and Entity ID, health
20, 289 terrain samples and returned World/HUD frames. The test's observing Q
pin ends before respawn; only the old Player is retained for identity checks.
Both scenarios call the actual direct World full-save API and independently
decode its Player output: health 20, empty Inventory, valid SpawnXYZ
`3/64/4`, or all three tags absent for the missing Bed. Both complete the
common checked Loop/App/bootstrap retirement. The 5,408 inventoried source
inputs and imported resource bytes remain unchanged through those runs.

The first two retained reports reached the native return and cleanup, then
failed the Python readback's assumed empty Inventory element type. The source
`SP.a` writer at 35..45 emits Byte type 1 for an empty list. The corrected
reader requires that exact type; the subsequent independent scenarios above
pass. Their earlier failed reports remain retained. A separate pair of
original VMs reads copies of the first scenes' C-written metadata through the
actual NBT, Entity, DC/GS/Living and Inventory readers and Q. Both recover the
same health, empty main/armor storage, nullable saved coordinate and pose bits,
while preserving the prepared Random state. That readback is a component
observation on copied files, without an original World restart or GUI session.

The loaded cases are `client.bed_respawn_valid` and
`client.bed_respawn_missing`, enabled by
`MC_ENABLE_CLIENT_BED_RESPAWN_TESTS=ON` with the existing native Windows
display/startup imported-resource configuration. Their
[reader](../../tests/tools/client/test_bed_respawn.py) retains before, death and
returned-frame PNGs and labels the save as a direct World full save. SDK save
menus, restart/reload, ordinary hazard-driven death, Bed placement/use/sleep,
other dimensions, original whole-frame/audio equality and sustained frame
times remain outside these observations.
