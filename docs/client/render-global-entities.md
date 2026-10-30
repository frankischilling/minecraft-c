# RenderGlobal Entity traversal

The native entry follows the complete 570-byte client
`n.a(Lbt;Lyn;F)V` control flow. It implements the delay, dispatcher frame
bindings, counter and origin stores, and the reached Entity predicates. An
admitted path can return at PC17 or PC569. Reached Entity and TileEntity draw
children still have explicit dependency boundaries; this is not a playable
World renderer or a claim of composed original/native equality.

The source authority is the client archive pinned in the
[binary audit](../reference/binaries.md). All PCs below are instruction offsets
in this method, unless another method is named. The 570-byte Code SHA-256 is
`4d5c587654aa1648eeecdc452396db0ebf401d836c91d955e2ceb2842025631b`.
The implementation is split
between [the entry and owner](../../src/client/world/render_global_entities.c)
and [the traversal body](../../src/client/world/render_global_entities/body.inc).

## Entry and dispatcher bindings

The first source read is `N.I` at PC1. If it is positive, the method rereads it
at9, subtracts one at13, stores at14 and returns at17. It does not read BT, YN,
partial, App, World, services, LL or TH, and does not sample floating controls.
The real N constructor stores `I=2` at its PC43; a completed World rebuild
stores `I=2` at its PC696. Each actual invocation consumes one delay. A failed
constructor is not an admitted constructed N merely because it reached its
earlier I store.

For `I<=0`, the source order is:

| PC | Operation |
| --- | --- |
| 18 | Active use/read of the actual `ll.a` singleton. |
| 22,26,33,40 | Read N World, N Engine, App Font, then a fresh App LivingEntity. |
| 44 | Execute the complete LL frame binding. |
| 47 | Active use/read of the actual `th.a` singleton, after LL returns. |
| 51,55,62,69,76 | Independently read N World, N Engine, App Font, App LivingEntity and App GameSettings. |
| 80 | Execute the complete TH frame binding. |
| 85,90,95 | Store zero to N counters J, K and L, in order. |

LL and TH use their existing actual constructors, World-change bodies and
frame methods. The two argument tuples are separate source reads. Their
earlier stores, constructor effects and typed failures survive a later
failure. Native namespace services are independently supplied ownership
information; they do not replace N/App operands or cause source reads before
their reached PCs.

## Origins and traversal

App LivingEntity is captured at102. Each coordinate evaluates the original
binary64 `previous + (current - previous) * (double)partial` operations afresh,
in subtract, widen, multiply and add order. TH b/c/d publish at127/150/173;
LL b/c/d then publish at196/219/242. There is no endpoint-partial shortcut.
The shared strict interpolation implementation preserves its measured
x86-64 arithmetic profile and refuses unsupported controls or its unqualified
two-NaN signaling-operand case at the reached operation. Completed component
stores remain published after a later refusal.

The remaining source sequence is:

| PCs | Operation and retained effects |
| --- | --- |
| 249,257,262 | Invoke `fd.o()`, capture that actual ordinary Entity List, read its size and store J. A null N World fails at249 after all six origin stores. |
| 274â€“335 | Reread the current World weather List and its size each iteration. Get the actual row at294; increment K at311 before its render-distance predicate317. A true predicate reaches TH draw329. |
| 345â€“522 | Recheck the captured ordinary List's live size, get its row357 and evaluate render distance370. Unless the actual Entity bM flag bypasses it, invoke YN on that Entity's bounds at390. |
| 404â€“436 | For the current camera Entity, GameSettings third-person mode or actual `Living.N()` sleeping dispatch determines whether it remains visible. An awake first-person camera is skipped. |
| 444â€“491 | For a remaining visible Entity, floor Y and clamp it to0..127; capture the current N World at470, then floor X/Z and invoke actual `fd.i(III)Z` presence491. |
| 507,516 | Increment K only after those ordinary-Entity predicates pass, then reach TH draw516. |
| 534â€“560 | Iterate the actual N rendered-TileEntity List, obtaining LL at542 before the reached row access551 and LL draw560. |
| 569 | Return after every reached loop completes. |

J is the captured ordinary List's initial size. K includes every visited
weather row even when its distance predicate is false, plus ordinary rows that
pass the full predicate chain. This source body clears L but never increments
it. Loop sizes and source fields remain live reads; they are not copied into a
synthetic empty list or an immutable contents snapshot.

## Supported dependencies and explicit stops

[World render reads](../../include/mc/world/runtime/render_entities.h) expose
the real client GAME World's constructor-owned b/e Lists and verify their
native backing, extents and reached-row provenance. RAW and failed GAME
prefixes can be read when their actual containers remain valid. Retired,
COMPOSITION or unknown replacement-container provenance is unsupported.
Null Lists, null rows and reached index failures retain their source frontier.
Presence491 calls the actual current ChunkProvider presence operation; it
does not load a Chunk or invent a loaded result.

[Entity render distance](../../include/mc/entity/render_distance.h) dispatches
on the canonical client class. Implemented inherited cases and the actual
BY/VV/CF/LX range overrides use their own formulas. C's missing private-b body
and unknown concrete overrides remain explicit dependencies. The YN operand
is the actual source frustum, with its live recentering and bounds method.
Sleeping dispatch uses the actual LivingEntity kind. Math is activated only
when a reached floor operation needs its namespace; floating admission is
likewise deferred to reached arithmetic.

A reached `th.a(Lsn;F)V` at329 or516 returns an explicit unimplemented-child
failure after capturing the real singleton receiver. Nonempty rendered
TileEntity traversal stops at551 because ownership of the actual DK/JE row
has not been implemented; it does not cast an untyped row or substitute the
World TileEntity list. Malformed native extents and unsupported provenance
also refuse explicitly. None of these paths reports source return569.

## Public API and lifetime

[render_global_entities.h](../../include/mc/client/world/render_global_entities.h)
provides `mc_client_render_global_entities_begin`, `inspect` and checked
`retire`. Begin requires an empty output slot. Native admission checks the
constructed, unentered N, allocates one call record and retains N before the
first source I read. An admitted failed call is still returned for inspection
and checked retirement; it must not be replayed to recover a receipt.

The call retains each reached LL/TH receiver and each current nonnull Entity
occurrence. It also holds one local lease per distinct reached World, up to16
Worlds. That fixed bound is native policy: the seventeenth distinct reached
World fails with LIMIT before acquiring its lease. Leases prevent container
retirement, not source List mutation, and are released during checked call
retirement. They must end before World terminal admission.

App and the complete BT/YN/service graph remain live and exclusive on the
video thread through retirement. BT and YN passage alone does not retain or
validate them; their enclosing owner keeps their actual storage alive. State
and nested diagnostics are borrowed views and expire as cleanup progresses.
Output/error storage must be disjoint from all owners and from each other.
Cleanup preserves a failing owned slot for retry rather than publishing a
cached permission to finish later.

N owns distinct LL and TH consumer records across calls and ordinary World
changes. Call retirement releases the call's receiver occurrences, while
one-way N terminal detachment or final owner retirement ends only that N's
consumer records. Startup and foreign consumer records remain attached, and
ending a consumer does not overwrite TH/LL source World fields. Pending N
calls block competing entries and terminal/final-owner cleanup.

## App prerequisite and current World-frame boundary

A genuine `new n(null, null)` cannot provide a successful original N for
dispatcher tests: after its earlier constructor effects, it invokes
`Minecraft.n()` on the null App at190. A nonnull constructor-only App still has
null private S, so `Minecraft.n()` returns null and the following `cx.a()`
invocation fails at193. The getter only reads S; it does not construct CX.
Real original startup constructs CX at470/474 and publishes S477 before its
N construction/publication. That prerequisite includes the actual context,
statistics, timer worker, resource graph and checked cleanup. Reflective S
installation or a failed N reconstructed for later calls is not equivalent.

Native loaded controls use the actual completed bootstrap App and a second
SOURCE-constructed N. They exercise genuine I2 delay calls, the null-World249
prefix, a real rebuild, nonempty Player traversal, actual frustum predicates
and exact consumer retirement. These are native composition controls;
separate original LL/TH observations do not establish composed N parity.

The enclosing PX World frame invokes its previously captured N at510 with the
actual pooled eye BT and actual SR. On its initial I2 invocation, native N
returns17 with I1 after all preceding terrain, lighting and eye work. PX then
continues through both [particle methods](particle-rendering.md), two further
fog calls and translucent terrain submission. The loaded profile reaches the
weather child at PX932. N510 is checked-retired before later N entries; its
scalar receipt remains while its borrowed owner, List and dispatcher views end.
This remains a failed World-frame prefix. Complete frames, repeated gameplay
and composed original/native equality remain unqualified.

## Preceding native scope at 9fb0b27

All 42 selected production/test targets build on Windows MSVC Debug and Linux
Clang18 ASan/UBSan. The selection passes 51 Windows CTests in 70.85 seconds and 48
Linux tests in 105.25 seconds, without failures, skips or sanitizer findings.
All 2,300 C/helper/build inputs remain unchanged across both builds and tests.
Formatting checks 1,621 C/header/fragment files, including this traversal body.

The local Windows loaded-terminal profile passes 34 composed N checks. It
executes genuine constructor/rebuild delays, a nondefault-FTZ delay, both real
dispatcher binds, null World249 and a nonempty Player List returning569. In
this capture the actual culler admits the camera, so one frustum check and one
first-person camera skip occur. Both successful loaded modes retain all 72
original focus histories and preserve all pre-existing save/resource bytes.
The frame-prefix image still shows authored grass terrain and Sky without HUD.

The asset-independent `entity.render_distance` test passes 193 native checks
per host, including the strict distance boundary, unordered inputs and null/FP
precedence. `world.render_entities` passes 95 checks per host using actual
duplicate Entity/weather publication, a genuine null weather row, live captured
List identities, presence queries and balanced leases blocking retirement.
Range-override instances, nonempty weather traversal through N and reached N
draw children are not newly executed by these controls. The existing 432-row TH
and 254/251/96-row LL comparisons retain their separately stated credit limits.
There is no new original composed-N or whole-frame equality result.
