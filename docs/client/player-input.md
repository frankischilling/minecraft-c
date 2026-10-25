# LocalPlayer input attachment and forwarding

The runtime uses the existing actual `uo`/`lr` MovementInput owner. Its
constructors, ten key cells, live GameSettings binding reads, reset and update
algorithms remain the same. Fresh LocalPlayer allocation starts `dc.a` null;
the constructor contains no input store. A controlled native RAW assignment is
therefore retained when the constructor reaches a later failure.

`mc_local_player_set_movement_input` performs the actual input field assignment
corresponding to Minecraft's `putfield dc.a` at205. It takes one native hold on
the exact input, stores it, then releases the previous field's hold. Repeated
assignment, replacement, sharing between Players and null assignment preserve
source identity. A native hold-limit failure leaves the old field intact.
Final Entity disposal releases its input occurrence without calling reset.

Each input constructor publishes one native hold. `mc_movement_input_retain`
acquires another; `mc_movement_input_retire` now releases the caller's one hold
and clears its pointer. It frees the key array and owner only after the final
hold. With a sole owner this has the previous retirement behavior. Hold counts
are native lifetime policy, not a Java field, allocation timing or GC claim.
The passive input state reports that count. A snapshot input pointer is borrowed;
retain it before using or retiring it independently of the Player field.

The options input still borrows its exact GameSettings identity, including null.
It does not retain, copy or retire settings. Keep settings live until **all**
input holds have ended, including attached Players. A caller clearing its own
input pointer does not end the settings borrow while a Player still owns that
input. Shared input/settings calls remain exclusive and synchronous; no stale
pointer detection, automatic settings-retirement guard or thread safety is added.

`mc_local_player_input_key` implements actual `dc.a(IZ)V`: load input at1 and
dynamically invoke the existing base/options method at6. Base truly does
nothing. Options reads the current six named settings references and key codes
in source order; the last match selects one cell. A null input fails at6 before
the child enters. Null settings/binding retains the existing child `lr` error
and no earlier matching key is committed before the final store.

`mc_local_player_input_reset` implements `dc.o_()V`: load input at1 and invoke
the existing reset at4. The base body is empty. The options body clears its ten
cells; source public axis/jump/sneak fields remain unchanged until an actual
input update. A null input fails at4.

`mc_local_player_is_sneaking` implements `dc.t()Z`: read input at1, read its
stored `e` at4, and only if true read sleeping `u` at11. It returns stored input
sneaking and not sleeping. It does not inspect the held key cell, watcher bit1,
health, World, bed, timer or settings. A null input fails at4 before a sleeping
read; a null Player fails at1. Native argument/hold failures remain distinct,
and failures preserve the ordinary boolean output.

These methods support the actual allocated LocalPlayer in RAW, failed and READY
phases because their source bodies have no construction-phase guard. Native
calls temporarily retain the Entity. Their separate compact error carries the
outer source site and the existing input child error; it does not enlarge shared
Entity/World error frames. Caller owners, borrowed settings/views and output/error
storage must remain live, exclusive and disjoint through each operation.

The authored test uses real Player/World/Registry/Session owners, shared/replaced
inputs, mutable settings, exact forwarding failures, source sleeping-cell
controls and terminal holds. Native RAW/failed-phase and counter-bound controls
are separately labelled. The finite unchanged-original comparison constructs
genuine completed Players, assigns their source public input fields, invokes
the actual forwarding methods and compares 21 input projections. It preserves
actual recipe/achievement banners and requires exit0 with empty stderr.

This field assignment is one concrete operation from Minecraft attachment. It
does not construct a Minecraft/controller/GUI owner or execute reset, World
publication, input events, focus policy, Player/Living/World ticks, collision,
rendering or movement. A direct call to the already existing input update is
still an input operation; it does not copy values to Living motion or tick a
Player. Application and simulation integration remain separate dependencies.

CTest runs the authored `entity.player_input` contracts without original files.
Enable `MC_ENABLE_PLAYER_INPUT_REFERENCE_TESTS=ON` with the supplied client JAR
and explicit JDK17 tools for `reference.player_input`. The existing
`client.movement_input` and optional `reference.movement_input` remain separate
regressions for the underlying owner and its original 857-record corpus.
