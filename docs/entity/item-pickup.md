# Item insertion and pickup

`mc_inventory_player_insert` executes actual client `ix.a(iz)Z` and its reached
`d(iz)I`, `e(iz)I`, `j()I` helpers on the existing InventoryPlayer's genuine
36-cell main array. It does not copy the array or route through crafting slots.
Source Stack fields and array cells are reread at each operation. The fixed
extent comes from the existing constructor-owned array; arbitrary foreign
array adoption and unfinished Inventory NBT replacement are not supplied here.

The actual Stack property closure is `iz.a/c/d/e/f/g/i`: real current `gm.c`
capture, later live ID read and array selection, max-stack `gm.d -> bg`, subtype
`gm.e -> bj`, and durability `gm.f -> a`. All 40 current original Item classes
inherit these three exact base property bodies, independently checked by the
reference audit. Damage metadata is not confused with the subtype flag, and
the source's repeated active uses and short circuits remain separate.

The insertion body preserves Java wrapping counts, matching metadata only
when the existing destination has subtypes, and both Item and Inventory limits.
It creates genuine Stack owners, sets destination animation 5 at its actual
store, and never invents a changed flag, clamps negative input or filters zero
before the source does. Damaged inputs follow the actual copy path, including
its original whole-count behavior even when the Item stack limit is 1. New
stacks have animation 0 until the explicit source animation store.

An important source quirk remains: filling part of the last available capacity
can change both input and destination counts yet return false. The final
no-progress iteration replaces the old-count local before the return test.
`hl.b(gs)` then genuinely skips achievement, sound and notification work. The
original/native observations check that completed count prefix exactly.

The private phase-transparent Stack clone factors the existing public clone's
body. It allocates at `iz.k` PC0 before reading ID5/count9/damage13 and numeric
construction16. Existing public clone retains its READY guard. Native raw and
failed-construction storage can use the source kernel without changing its
phase or provenance; a fresh copy owns independent READY storage and no copied
provenance. Those extra native phase/allocator controls are not presented as
original JVM failed-constructor object observations.

`mc_item_entity_pickup` executes actual `hl.b(gs)`: World.B first; original count
capture before delay; Inventory receiver/Stack argument evaluation; actual
insertion; then separately reloaded ID and live Block.K/Item.aD comparisons.
Achievement `ep.g`/`ep.t` uses the genuine canonical Player statistics bridge.
The sound World is captured before literal resolution. Both actual Entity
Random floats are drawn before pitch arithmetic/FP admission, and the actual
entity-coordinate World sound overload runs before Player notification.

The concrete notification now executes `dc.b(sn,int)`: it captures Minecraft.j,
allocates `em`, reloads Minecraft.f for construction, then invokes the captured
manager. A null App fails at dc PC4. A real null manager fails at PC25 after
particle construction and its ID/entropy effects. The application World is a
separate source read from the Player's World. See [particle ownership](../client/particles.md).

After notification returns, pickup reloads the current Stack and performs the
count<=0 check before inherited Item.K. A successful pickup can therefore enqueue
the effect and mark the Item dead. The effect keeps its own Item hold until
retirement. A failed notification preserves completed insertion, achievements,
sound and Random effects and leaves this later death check unreached. Particle
drawing, World scheduling and the playable client remain unfinished.

Action now delegates its real Item contact to this body using the existing
explicit code Registry context. It does not obtain the namespace from a
nullable or changed World. `mc_player_action_error` stores travel and pickup
as a tagged union: failed travel returns before pickup, while reaching pickup
requires completed travel with a fresh child. Independent Inventory, Registry,
bounds, nearby and caller-PC siblings remain intact. Read `.pickup` only when
`child_kind` is PICKUP; read `.travel` for NONE/TRAVEL. Fresh errors and successful
pickup restore the original fresh travel sentinels and NONE. All working
frames remain private to each invocation, with no caller-output scratch or
stack-limit change. A complete ABI-consistent rebuild is required.

The optional work ceiling counts pending insertion/scan iterations, failing at
the reached operation without truncated success. Inventory active state, input
and selected Stack local holds, and the Registry borrow protect real owners
across source operations. Class initialization is not performed at native
admission. The caller keeps code Registry, profile/resources, actual App/World
and diagnostics exclusive and live; no synthetic World or callback is used.

Authored tests construct the genuine App, Writer, Session, World, Player, Item
and Stack owners. They exercise connected insertion/pickup, real achievement
completion, exact sound arguments and FP failure after the source draws, native
allocator/phase boundaries, nullable early prefixes and actual Action contact.
The sound callback is a supported native observation-transport slot; it is not
an original WorldAccess implementation or a substituted game body.

The source-only original observer executes unchanged classes from the supplied
JAR on real constructed `dc`, World, Inventory and Items, with null Minecraft
at the genuinely nullable constructor input. It compares23 insertion rows,
seven pickup rows and seven resulting Inventory rows, including signed/wrapped
counts, metadata, animation and same-owner aliases. Two insertion source
failures, four completed pickup early returns, two achievement null-App prefixes
and one notification null-App prefix are separately identified. Existing Item
Random is explicitly reseeded after construction; its final raw state matches.
Constructor entropy and original nonnull-App particle/death correspondence are
outside this finite comparison. Separate native tests cover the real App,
completed pickup, effect queueing and Item lifetime through the third effect
tick. The particle CPU comparison has its own 71 original records and does not
extend this null-App pickup profile.

`source-pins.json` contains54 whole original classes with all method Code hashes.
`audit_dispatch.py` regenerates all120 current Item-property resolutions from
the pinned unchanged archive. The controller requires a matching same-bin JDK17
runtime/compiler, pinned archive/dependency/helper/native identities, bounded
captures, exit0, empty stderr and exact positive final markers. Normal CTest
uses only authored resources and native owners, without a JDK or game assets.
