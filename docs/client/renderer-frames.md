# Source frame input and World drawing

The default client now executes focused mouse look and opens the source pause
screen. Its World draw enters the real `px.a(FJ)V` pipeline and stops at the
missing sky-pass fog-state setup at `px.a(FJ)V:306`. Sky/fog colors, camera
transforms and frustum extraction execute before that frontier. Terrain meshes,
HUD composition and a playable frame remain unfinished.

## Focused mouse input

The frame handles inactivity before its `px.b(F)V:38..194` input segment. A
false App.N reads no MouseHelper, Settings, CU filters or Player. A true N
invokes the same MouseHelper's `vy.c()V`, consuming SDK getDX and then getDY.
The source sensitivity formula is evaluated as separate binary32 operations.
Inversion follows the selected Settings flag. Smooth camera invokes the actual
PX n/o CU objects before inherited Player `sn.d(FF)V`; it does not turn App.i.

Each CU retains its accumulated input, accumulated output and smoothed delta.
Its comparisons preserve the original NaN branch behavior. The qualified
x86-64/JDK17 profile also preserves the observed right-operand NaN payload for
the three additions. Invalid native FP controls fail at the reached operation,
after any earlier motion consumption. A missing second filter preserves the
first filter's completed stores. Native retirement releases the call's PX hold
without undoing input, filter or angle changes.

The [CU fixtures](../../tests/reference/client/mouse_filter/README.md) contain
96 unchanged-original observations. The
[focused-frame fixture](../../tests/reference/client/renderer_focus/README.md)
contains 72 histories from the unchanged original PX method with genuine LWJGL
Display/Mouse setup. The native loaded-World profile supplies authored SDL
motion through the actual SDK and compares all 72 rows exactly. The original
observer's authored `w=true` makes that method return after mouse look; it does
not qualify World drawing or a complete gameplay frame.

## Pause screen

Both Escape and the inactive-window path invoke actual `Minecraft.i()V`.
An existing screen returns before allocation. Otherwise the shared transition
owner constructs OZ and publishes it through the existing display-screen body.
OZ initializes five real Buttons in source order: Back to game, Save and quit,
Options, Achievements and Statistics. Reinitialization resets the save-pass
counter and preserves its tick count. Back to game and inherited Escape close
through the genuine display/focus routes. Other destinations retain their
explicit missing descendants.

Focused screen publication releases Player keys before storing N=false and
calling MouseHelper ungrab. A null Player input fails before the N store.
The current default startup profile has a null source Canvas; focused ungrab
therefore retains the real `Component.getWidth` null failure after key release
and the N store. The inactive N=false path can initialize OZ without this call.
Native window dimensions do not replace the absent source Canvas.

The OZ draw entry uses the existing GL3.3 GUI kernels, executes background
drawing, advances the save-pass counter, and invokes actual incremental World
saving before its saving label/title/Buttons. The normal loaded-World frame
currently stops earlier in World drawing. Pause drawing and saving are not
qualified as part of that full frame. The
[pause fixture](../../tests/reference/client/pause_screen/README.md) compares
68 constructor/init/tick rows; live loaded-graph controls cover Escape, existing
screen admission, Back to game and key-release failure order.

## World frame

`px.a(FJ)V` enables the real Display's cull/depth state, publishes the source
view fallback if needed and invokes the existing picker. It captures the actual
camera, World, RenderGlobal and particle manager in source order, interpolates
camera coordinates, checks the actual provider class, and applies the first
anaglyph eye's mask and viewport.

Actual `px.h(F)` now reads [World sky, fog and cloud colors](../world/colors.md),
camera immersion, rain/thunder/lightning and PX brightness fields. It stores
canonical PX RGB, applies the original anaglyph operations and calls the real
GL clear-color bridge. Color/depth clear and culling precede actual camera at 282.

The canonical camera executes `px.a(FI)` and its FOV, hurt/death, bob and
orientation children. This includes portal rotation using actual Player.l,
E/F/G zoom, sleeping, debug and ordinary third person. The ordinary third-person
path makes all eight original World rays. The final RenderGlobal cloud query
uses its actual literal-false body and stores PX.D. Every child reloads its
source App fields at the reached operation. Native transforms update the same
Display projection/modelview ledger in source order.

At 285 the canonical W singleton reads those matrices into its three actual
Ge buffers, computes the combined matrix and normalizes six frustum planes.
The RenderModels namespace owns its static lifetime; teardown refuses active
calls or outstanding owners before retiring it. Matrix reads preserve the
original clear/flip/limit/get cursor effects. Box tests use the original plane
and corner order.

For render distance below2 the next missing child is `px.a(IF):306`, before
RenderGlobal sky drawing at 312. Other distances stop at the missing GL3 fog-enable
bridge at 318. Neither branch skips ahead to terrain. The performance deadline uses
the source 200/120/40 target and Java long wrapping. A retained frame owns its
native children and prevents another invocation until retirement. Failure
preserves completed colors, matrices, fields and source effects.

## Evidence and limits

The [full-fog fixture](../../tests/reference/client/world_frame/README.md)
contains 96 executions of unchanged original `px.h(F)` with real LWJGL clear-color
queries. Prepared PX/App/World/Player fields and authored block/metadata
transport bound this body comparison. Original World color and immersion
methods remain unchanged. Eight earlier JDK expression vectors separately
qualify the render-distance blend; they do not execute an original PX method.

The [camera fixture](../../tests/reference/client/renderer_camera/README.md)
contains 36 mode/partial profiles plus 36 scalar field rows. FOV, k/D and E/F/G
compare exactly. Projection/modelview coefficients use an explicit absolute
`2e-6` limit; the observed Windows maximum is `4.76837158203125e-7`.
Prepared original receivers and air/water transport do not qualify bypassed
constructors, actual Chunk transport or complete frames. Native collision-wall
and failure/ownership controls have separate scope.

The [frustum fixture](../../tests/reference/client/frustum/README.md) compares
four controlled original GL profiles, actual constructor/static identity,
matrix/plane bits, buffer cursors and box results. Its finite inputs do not
establish arbitrary cross-driver matrix generation. Native tests also cover
all 28 allocation boundaries, partial stores, FP refusal and namespace cleanup.

The genuine loaded Windows graph completes fog, camera and W before stopping
at 306. These observations establish neither a terrain frame nor original-pixel
equivalence. GL3 fog state, sky, Chunk culling/compilation, terrain, Entities,
particles, HUD and hand composition remain unfinished. The separate diagnostic
World renderer remains outside the default source path.
