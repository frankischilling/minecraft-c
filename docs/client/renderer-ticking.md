# Renderer and World ticking

The source runtime composes the renderer tick, every-30-tick Player refresh,
World entity tick and the frame's existing sound/lighting services. The ordered
[World scheduler](../world/ticking.md) now runs weather and mob-spawn candidate
selection before its next missing descendant. Terrain-frame drawing remains
unfinished. These paths operate on
the constructed App's canonical World, Player, Inventory, renderer and Settings.

## Renderer tick

`px.a()V` is separate from picking `px.a(F)V`. The tick copies six interpolation
fields without arithmetic, publishes Player as the view only when App.i is null,
queries brightness at the view's floored coordinates, blends brightness using
the current render distance, and increments the renderer counter with Java
integer wrapping. It then invokes the real held-item and rain ticks.

World brightness is the actual `fd.c(III)F`: it captures the current Dimension's
light table before invoking the block-light query. A Registry/chunk callback
can replace World.t; the result still indexes the captured table. Native holds
keep that table alive across the query. Null Dimension, null table and nested
query failures preserve their distinct source order.

The held-item tick executes `ra.a()V` on the existing RA. It reads the selected
Stack through actual `ix.b()`, compares slot and Stack identities, replaces an
equivalent Stack by ID and metadata, and updates equip progress with binary32
arithmetic and the original clamps. RA.b has its own native Stack occurrence,
so replacing an Inventory slot cannot leave a dangling rendered item.

Rain updates use the renderer's actual Random. Zero rain returns after the
original Settings read. Nonzero rain retains the seed, draws, terrain/biome
queries and reservoir prefix in source order. Missing smoke `xn` and splash
`xd` construction stops at the reached allocations, respectively 280 and 387.
Dry, zero-iteration and skipped-height/biome branches can return normally.
No successful rain-particle substitute is supplied.

Each retained tick call owns its diagnostics and child calls. Completed source
stores survive failure and cleanup. Native cleanup releases children before
their parent occurrence; released child views end even when later cleanup
refuses. Copied value errors remain available. Source App.i stores share one
App-owned native occurrence with World loading, independent of App.h and World
membership. A foreign replacement refuses cleanup instead of dropping another
owner's reference.

## World and frame callers

`Minecraft.k` increments App.ah even when paused. At exactly 30 it resets the
counter and invokes `fd.g(sn)`: acquire 25 Chunks in the radius-two square in
X-major/Z-minor order, then append the Entity to World.b only if its source
ID-based equality is absent. This is neither insertion nor Chunk membership.
Difficulty, lightning countdown and spawn flags use their canonical World cells.

An unpaused segment executes renderer tick, RenderGlobal's wrapping counter
and the existing World entity tick, then invokes the retained World scheduler
at `k1260`. The loaded graph reaches the selected Mob constructor at `cq.a:773`. The entity tick receives the same Registry and MathHelper
table and can invoke genuine Player input/action/travel descendants. Paused
single-player segments skip those calls and return through 1328; the outer tick
then performs the actual wall-clock App.P store.

The frame supplies actual Player fields to the existing SoundManager listener,
drains World lighting, checks Player suffocation and clears third-person view
when required. Controller `os.a(F)V` stores GuiIngame.b before RenderGlobal.i,
including the literal-zero branch and partial effects of later failure. Paused
Timer update preserves/restores source partial c while allowing the tick
remainder and clock state to advance.

The native loaded-World profile now opens the source pause screen when inactive
for more than 500 ms. Focused mouse look is connected. Both paths enter the
[World frame](renderer-frames.md), completing colors, camera, fog state, sky
and ChunkRenderer culling before missing compilation at `px.a:396`. Direct segment checks on the same graph
separately exercise the paused refresh and unpaused renderer/entity/weather/
spawn-selection sequence through the Mob constructor frontier. None of
these component controls establish a rendered gameplay frame.

## Evidence

Pinned-original numeric fixtures cover [brightness](../../tests/reference/world/brightness/README.md),
[held items](../../tests/reference/client/held_item_tick/README.md),
[rain](../../tests/reference/client/renderer_rain/README.md),
[renderer ticking](../../tests/reference/client/renderer_tick/README.md) and
[controller interpolation](../../tests/reference/client/controller_interpolation/README.md).
They contain 12, 120, 20, 43 and 72 rows respectively. Prepared receivers qualify
only their recorded bodies and prefixes; their constructors and a complete
original frame are outside those observation scopes. Native loaded-World checks
use the genuine startup/loading graph and preserve save/resource files through
cleanup. Full scheduling, precipitation particles, terrain presentation,
extended play, multiplayer and the server remain unfinished or unqualified.
