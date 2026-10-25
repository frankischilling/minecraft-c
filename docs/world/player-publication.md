# World Player publication

`mc_world_runtime_attach_player` implements the reached client `fd.a(gs)` path
over the actual LocalPlayer and GAME World. It reads current Info.Player through
the concrete `ei.h()` getter, including the ordinary null tag of a new World.
It then checks the provider's actual constructor provenance and calls the real
Entity insertion operation. Reaching LocalPlayer READY does not attach it;
publication is an explicit later operation.

The current concrete client factory creates the `ok` map-cache family. That
class is unrelated to client `kx`; cache policy and retained grid storage do
not select a center operation. The native provider now retains immutable
concrete-kind metadata independently of its policy. No `kx` owner or constructor
is supplied by this increment. In the supplied client classfile, `kx` has no
constructor method. A future genuine `kx` implementation must preserve the
source `d2i -> i2f -> floor(float) -> >>4` argument order before `kx.d(II)`.

`mc_world_runtime_insert_entity` has the concrete Player-positive branch of
`fd.b(Entity)`. It calculates X/Z chunk coordinates first. LocalPlayer bypasses
the nonplayer presence test, appends the same Player to lowercase World `d`,
updates the sleeping flag, acquires the actual Chunk, appends to its actual
slice, appends to World `b`, and invokes the live WorldAccess list. These are
distinct source publications. No membership lookup or deduplication is added.
Uppercase `D` remains the separate deferred Entity list.

A later acquisition failure retains the `d` occurrence and sleeping update.
A Chunk failure retains that earlier prefix and any completed membership
stores. WorldAccess failure retains all three publications. Callback iteration
uses the existing held, live-index WorldAccess owner, so registration changes
and synchronous publication reentry remain observable. A missing or registered
null callback is an explicit reached failure, not a successful empty handler.
The ordinary boolean output is unchanged on failure. Player insertion does not
rewrite the Entity's construction World or perform a gameplay removal.

`mc_world_runtime_update_player_sleeping` implements `fd.y()`: assign
`J = !d.isEmpty()` before capturing the iterator, then invoke actual `gs.N()`
until the first awake Player sets `J=false` and stops. `mc_player_is_sleeping`
reads only the Player's stored `u`. It does not inspect health, bed state,
sleep timer or World flags. Neither operation enters a bed or implements a tick.
The direct sleeping error has the selected occurrence index and complete cause;
nested publication errors carry compact source sites and reuse their parent
Entity/cause fields to avoid multiplying large error frames through the mutation
context graph.

The new typed ledger captures the actual `d` descriptor at World constructor
store94. Each duplicate occurrence owns a separate real Entity hold, independent
of Chunk, `b`, uppercase `D`, nearby-query and caller holds. Unknown `void*`
values are never adopted as Entity ownership. The captured descriptor remains
the owner when a source projection is null or replaced. Terminal admission only
accepts that projection, null, or a shaped empty borrowed list; foreign storage
is never freed or cleared.

Native retirement validates every typed World ledger and bound Chunk before
releasing any occurrence. It marks source containers terminal, releases actual
Chunk/b/D/d and other occurrences, and leaves Entity.world and source membership
flags unchanged. A caller-held Player still keeps its construction World alive.
After ending source use, retire the typed containers, clear the native terminal
cache/pending descriptors, retire World contexts and Chunk headers, release
external Entities, then free the World and its store/class dependencies. This
is native cleanup, not source removal, saving or permission to resume gameplay.

Nonnull Info.Player enters the existing inherited Entity NBT reader. Malformed
tags can fail earlier at their real base getter/list sites. A well-formed base
tag retains its motion, position, previous fields, angles, scalar and bounds
stores before `sn.e311 -> dc.a2 -> gs.a2` reaches the explicit missing Living
read body. No complete Player/Living/inventory NBT load is claimed. The tag is
not cleared and publication does not continue after that frontier. The existing
READY-only native Info setter is reached only after a complete load; completing
the remaining NBT path must also qualify broader source setter prefixes.

The source attachment wrapper catches `java.lang.Exception` and prints its
stack at88. The native API returns its explicit reached error/frontier instead
of emulating Throwable or PrintStream, and never treats missing NBT as successful
clear/publication. Native policy failures keep their own meanings. Arguments,
output/error storage, owners and borrowed views must remain live, exclusive and
disjoint through each synchronous call.

The authored-resource test uses real LocalPlayer/World/Registry/Store/Chunk
owners. It covers duplicates, real null WorldAccess failure, callback reentry
and failure after publication, zero-provider-capacity failure after `d/J`,
native sleeping-cell controls, uncleared NBT failure prefixes and cycle teardown.
Those controls do not qualify bed-entry or original renderer callback bodies.
The local original comparison uses unchanged genuine `fd/wy/rh/gr/dc/ok/lm`
objects and a genuine registered null WorldAccess. It compares attachment,
duplicate and null-callback projections without substituting providers or
method bodies. See the reference README for the precise finite scope.

Player reset/controller actions, input attachment, World/Player ticks, sleeping
gameplay, full Player NBT, renderer callbacks, startup preparation and rendering
remain separate work. This increment adds no application Player or tick stub.
