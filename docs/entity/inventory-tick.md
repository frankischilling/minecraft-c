# Player inventory updates

This client-first runtime implements `ix.e()V`, `iz.a(Lfd;Lsn;IZ)V`, the actual
concrete Item update dispatch, and `wr.a(Liz;Lfd;)Liu;` Map lookup/create on
the existing InventoryPlayer, Stack, Registry, Entity, World and SavedData
owners. It lives in `mc_simulation`; the lower inventory/item libraries gain
no World dependency. It does not execute `gs.o`, a complete Player tick or
unfinished Map tracking/terrain behavior.

The actual client Stack method is named **a**, not b. InventoryPlayer.e has
Code 61: initialize index 0, read current main-array length 7, read cell 16, reread
cell 25, read Player/world 30, reread Player 34, read current selection 39, and
invoke Stack.a at 51. Only the actual main array is visited. Armor, cursor,
selection and changed flag are not updated. The native implementation rereads
the real owner fields/cells, retains actual source identities across calls and
uses no copied array or generic callback. Every original JAR constant-pool
reference to ix.a/b was audited: seven classes reference them, and all four
putfield writes are ix construction and NBT read, allocating 36/4 cells.
Current native owners retain those real fixed-extent arrays. Arbitrary foreign
raw-array adoption is not supported; no source gameplay array extent is changed.

Stack.a has Code 35. It tests animation at1 and, only if positive, rereads and
stores Java b-1 at14. This precedes Item.c active use17, current Stack ID read at 21,
aaload on that captured array at 24 and actual virtual invocation at 31. Negative and
zero animation remain unchanged. Null c precedes signed bounds; a selected null
item reaches invocation at 31. The two new private Stack kernels provide the exact
animation field store and positive decrement without a phase/provenance change.
Existing count/id/damage, NBT and public APIs remain unchanged.

The supplied JAR has 40 Item descendants. Exhaustive superclass/method resolution
proves 39 use the inherited `gm.a(Liz;Lfd;Lsn;IZ)V`, Code 1 return0; only wr
overrides it, through cp. The native dispatch table explicitly enumerates all
37 represented base-body kinds. A future unreviewed kind fails explicitly.
No empty Map override or generic successful callback substitutes for a body.
Base Item update genuinely ignores nullable Stack/World/Entity arguments.

Map update first reads actual World.B at 1; true returns 7. Otherwise it executes
actual Map lookup at 11, then source Player instanceof at 17 using the canonical
Class assignability relation. A LocalPlayer is a Player; exact-kind equality
would be wrong. Actual Player input reaches `iu.a(Player,Stack)` at 34. That
tracker/decoration body is still required and reports an explicit dependency
before entry. Non-Player selected input similarly reaches the missing
`wr.a(World,Entity,MapData)` terrain/pixel body at 47. Unselected non-Player input
and remote World complete their genuine source branches. No entire Player Map
update is reported complete.

Lookup/create constructs the two separate initial `map_` String identities,
rereading Stack.i at 13/39, then delegates World.a(Class,String) at 48 to that World's
real SavedData manager. It performs the actual iu checkcast at 51. A hit returns
the same owner. A miss obtains real World.b("map")65, stores Stack damage at 68,
builds the new map name from the updated damage, constructs actual MapData at 99,
rereads World.x/Info spawn X at 110 and Z at 122, stores scale 3, reads live provider.g at 140
and Java byte-narrows it, marks dirty at 149, then publishes the actual same name
and value through World.a(String,SavedData) at 156. The existing manager/cache,
next-ID persistence, caught diagnostics and failure policies are reused.
No fabricated Map object, empty named cache result or shadow counter is used.

Failed operations retain earlier animation, counter, damage, Info-field and
storage effects. A newly allocated but unpublished MapData is safely retired
after an escaping failure; error scalars preserve its observed prefix without
returning dangling pointers. A pending-map error pointer is borrowed only after
successful lookup, when that exact manager owns the value. Keep the supplying
manager/World live to inspect it. Successful direct lookup supplies an owned
value hold. Name and MapData construction each use an independently configurable
bound defaulting to the existing SavedData 4MiB policy; manager, NBT and referenced
owners keep their separate budgets. No existing budget is increased.

The code namespace is a borrowed actual Registry, independently of nullable
World. Entry retains it without active use. Empty inventory therefore requires
neither World nor Item initialization. Reached statics require that actual
Registry; Map Worlds and Entity Class tests require its matching namespace.
Native active/hold guards block retirement/reentry through synchronous calls,
protect captured Stacks across real resource-loading callbacks, and are not
source fields. Keep inputs exclusive/live and outputs/errors disjoint. Native
constructor diagnostics are untouched; successful errors are fresh and empty.

Map damage assignment uses the approved private phase-transparent source field
store. Live RAW and failed-construction prefixes remain usable without a fake
READY transition; phase and provenance stay unchanged. The existing public
damage setter retains its READY guard and delegates to that same primitive.
Native controls execute real Map miss/create and update on these actual native
construction prefixes; they make no original-Java RAW-object parity claim.

Tests construct actual GAME/Store/Registry/Player/Stack owners. They cover 201
non-Map instances, nullable arguments, aliases in two main slots plus armor,
animation boundaries, null array/index/item failures, remote Map, same-value
cache hits, distinct Map creation, real disk reload and an actual constructed
Falling receiver. Player inventory with a Map preserves earlier aliases and
stops before later slots at the real tracker boundary. Native resource callbacks
exercise mutable cells, retirement refusal and captured-Stack unwind. These are
existing class-resource transport callbacks; no gameplay callback is invented.

The optional original observer runs unchanged client classes with genuine
constructors and handlers, checks class origins and JAR/Code pins, and compares
exact completed records. It does not compare a completed Player Map update,
selected terrain body or native full-tick/exception parity. A separate retained
fresh-JVM probe confirms real cold Item active use throws initializer failure
after animation5→4 and retry fails after4→3; the native cached-failure controls
preserve that ordering. No fabricated READY owner, authored game subclass,
Unsafe, original class rewrite or runtime JVM is introduced. No server gameplay
claim is made.
