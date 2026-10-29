# Selected World loading

The default client's World-selection route now calls the real retained
`Minecraft.a(String,String,long)` operation after publishing its singleplayer
controller. It uses the selected folder and display-name objects, the same
SaveFormat namespace, and the initialized App's Registry, entity classes,
MathHelper and Random profile. It does not borrow the diagnostic World viewer's
Store or construct a replacement App.

## Source order

The initial `a(NULL World)` call completes its reached statistics save,
loading-screen calls and null streaming-source branch. It clears the actual
World/Player/view fields and reaches the final advisory GC and `P=0` stores.
The outer operation then makes its own GC request before testing conversion.

The actual conversion predicate is `version == 0`. A matching save executes the
conversion title and message prefix, then stops at the unimplemented converter
call at bytecode 52. No successful conversion result or recursive seed-zero
load is supplied. Saves with other versions continue through the real
SaveHandler and `fd` constructor. A missing WorldInfo takes the new-World branch
and retains that branch's explicit unfinished spawn-construction dependency.

An existing WorldInfo can reach `CLIENT_LOAD_OR_CREATE` and `READY`. The
constructor uses the canonical [SaveHandler](../world/save-handler.md),+[SavedData](../world/saved-data.md), source region loader and existing World
construction graph. Metadata fallback, idcounts loading, session-token effects,
dimension choice and loaded-name assignment occur at their actual source
positions. Corrupt metadata is not replaced by a generated World.

After the source statistics increments and World publication, the World-change
method invokes `d(String)`. This body captures its actual provider and constructs
the canonical spawn coordinate. It visits all 289 samples in x-major order,
from offsets `(-128,-128)` through `(128,128)` in steps of 16. Each progress call
precedes its block lookup. Each sample drains the current World's light work
until the actual return is false. The final message and provider maintenance
follow; the dead local containing 2000 does not run simulation ticks.

On normal preparation return, the existing controller creates the real
LocalPlayer. The same App publishes `h`, performs spawn/yaw preparation and
attaches its canonical movement input. The next dependency is
`RenderGlobal.a(World)` at World-change bytecode 220. At that frontier `i` is still
null; the later effects, finish-loading call, World Player-data call and final
GC/`P` stores have not run. Rendering attachment and complete World entry remain
unfinished, so this path is not a playable session.

## Native lifetime and errors

The caller retains the complete source prefix: selected String arguments,
SaveHandler, constructed World, preparation samples, Player, input and typed
child failures. Inspectors borrow those exact objects. A lower loader error
keeps its File, region stream and source location through the enclosing World
and lighting error; it is not reduced to a generic block-get failure.

Checked native retirement drains only calls owned by this invocation. A
pre-existing retained Loading or SaveFormat failure belongs to its original
caller, even when the new load acquires a native borrow and returns BUSY.
Cleanup preserves completed source effects and releases actual containers,
cache registrations and Store-owned Chunks before retiring their World and
SaveHandler. External occurrences can make retirement return BUSY for a later
retry. Retirement never replays the load or restores prior source fields.

World-list drawing leaves its dedicated flat-shading state active. Loading-bar
batches can contain differently colored triangles when each complete triangle
has one uniform RGBA value. A gradient within a triangle remains unsupported.
This does not admit the separate public legacy `GL_FLAT` call.

## Verification boundaries

Focused Windows and Linux tests exercise source SaveHandler, World construction,
SavedData and region ownership, including malformed input and checked cleanup.
The native existing-save route and its full preparation/Player continuation
have a separate integration profile in the [testing record](../testing.md).
Original game pixels, extended play, dynamic World rendering, native Canvas
integration, movement, multiplayer and the standalone server remain outside
this qualification.
