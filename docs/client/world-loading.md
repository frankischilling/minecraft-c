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
constructor uses the canonical [SaveHandler](../world/save-handler.md),
[SavedData](../world/saved-data.md), source region loader and existing World
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
attaches its canonical movement input. At World-change bytecode 220, the actual
[RenderGlobal](render-global.md) attaches that World and constructs its source
ChunkRenderer grid. Those constructors compile bounding boxes; they do not
compile terrain meshes or increment the Chunk-update counter. The particle
manager then receives the same World at 235, and the controller finishes loading
at 246.

The canonical `fd.a(gs)` call at 324 consumes optional WorldInfo Player NBT in
source order, then inserts the actual Player into World and Chunk containers.
Its WorldAccess callback invokes the Player's appearance override before
requesting skin and cloak images through the real
[image factory](image-downloads.md). Factory return and asynchronous worker
completion remain distinct. A callback failure retains the partial publication
and its exact invocation receipt; it does not acknowledge the source boolean
return that follows the callback.

For the existing-save profile, the final new-World save branch is false. The
caller publishes the same Player in `i347`, requests its third advisory GC,
stores `P=0` and returns at World-change 366 and outer load 130. The following
`displayScreen(NULL)` reads actual Player health. A healthy Player leaves `r`
null and runs the actual focus operation: Display activity and `N` guards,
`N=true`, MouseHelper grab, recursive null-screen call, then `W`/`ag` stores.
The recursive call returns through the now-true `N` guard. Null Player and
death-screen paths preserve their reached source boundaries.

The native selection profile next stops at gameplay input `Minecraft.k:337`
in the same tick. Later ticks also require the unfinished mouseover and World
tick paths; a frame requires sound, controller and terrain-render integration.
Loading a World is therefore still not a playable session.

## Native lifetime and errors

The caller retains the complete source prefix: selected String arguments,
SaveHandler, constructed World, preparation samples, Player, input and typed
child failures. Inspectors borrow those exact objects. A lower loader error
keeps its File, region stream and source location through the enclosing World
and lighting error; it is not reduced to a generic block-get failure.

Checked native retirement drains only calls owned by this invocation. A
pre-existing retained Loading or SaveFormat failure belongs to its original
caller, even when the new load acquires a native borrow and returns BUSY.
Cleanup releases captured callback receipts before renderer detachment. It
ends only owned WorldAccess registrations and the renderer/particle World
occurrences before retiring World containers. Actual containers, cache
registrations and Store-owned Chunks retire before their World and SaveHandler;
the input owner remains alive through Player membership cleanup. External
occurrences can make retirement return BUSY for a later retry. Retirement
preserves completed source effects, never replays the load and never restores
prior source fields.

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
