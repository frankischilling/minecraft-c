# Source frame input and World drawing

The default client now executes focused mouse look and opens the source pause
screen. Its World-frame source calls real
[terrain submission](terrain-submission.md) at `px.a(FJ)V:486` after colors,
fog, Sky, camera, culling and standard Block list compilation. A normal N return
continues through FLAT493, [standard lighting](standard-lighting.md)496 and the
captured camera's eye position504 to the actual captured RenderGlobal at510.
Its initial [Entity pass](render-global-entities.md) consumes the constructor's
I2 delay and returns17 with I1. PX then calls the actual EffectRenderer at518
and535, performs two further fog calls and submits translucent terrain. The
loaded profile returns from [precipitation drawing](precipitation.md), executes
the next fog setup and draws [clouds](clouds.md). The following fog setup and
depth clear enter the [first-person hand](first-person-hand.md) at997. Its
empty-arm and dry-overlay bodies return, allowing World draw1013 before the
parent's real timing and [GuiIngame draw](gui-ingame.md). The focused loaded
profile now returns from the complete frame. Its next unpaused tick reaches
the [Mob spawn prefix](../entity/mob-construction.md); hidden inactivity still reaches the pause save child. See the
[particle continuation](particle-rendering.md) for ordering and retained views.
Later N calls use actual LL/TH frame bindings, Entity Lists and
visibility predicates; reached Entity/TileEntity draws remain unfinished.
The specified Windows MSVC Debug state profiles and both hosted Linux sanitizer
replays pass. The [vertex-lighting backend](lighting-raster.md) now handles
source normals, materials and flat/smooth lighting. Specialized Block shapes,
nonempty HUD items, full Entity drawing and playable gameplay remain unfinished.

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

For render distance below 2 the frame invokes actual fog setup 306 and
RenderGlobal sky 312. It then enables fog 318, performs terrain fog setup 324,
reads ambient occlusion and applies smooth shading343 when selected. The real
SR constructor350 calls W again; recenter 363 stores the interpolated camera
position before the fresh App.g culling call378. Dirty-chunk compilation396
now completes for standard Block sections. The actual deadline controls further
update calls. Draw fog449, terrain texture470/473 and standard lighting476 then
run before real [N terrain submission](terrain-submission.md)486. The source
connects its ordinary and occlusion paths with recenter/JO sorting and TG replay.
After a normal N return it sets FLAT493, enables standard lighting496 and reads
the actual camera eye504 before N510. An initial delayed Entity return allows
the particle and translucent-terrain prefix to continue to weather932. Reached
Entity/TileEntity draws, other particle classes, HUD and hand work remain
unfinished.

The performance deadline uses the source 200/120/40 target and Java long
wrapping. Retained frames preserve completed fields, matrices, GL state and
complete child diagnostics. A successful Sky child retires before the next N
method; its scalar receipt remains while pool/array/error views expire. Cleanup
refusal refreshes every surviving child view. The existing successful World
attachment may remain retained during drawing; active or failed attachments
still block overlapping native operations.

## Fog state and source shader

Actual `px.a(IF)` clears the same PX.f FloatBuffer, writes g/h/i/1 and flips to
position 0/limit 4/capacity 16 before setting fog color. Current normal becomes
(0,-1,0) and current color becomes white. Cloud and water select EXP/.1; lava
selects EXP/2. Air selects LINEAR with start k*.25 and end k; a negative pass
changes those to 0 and k*.8. Actual context capability controls the radial
distance selector, and a fresh World Dimension read can set the start to 0.
Every branch enables FRONT/AMBIENT ColorMaterial. The partial argument is never
loaded. Anaglyph's unused local arithmetic still executes in exponential paths,
retaining its Settings/null/FP order and earlier state effects.

The existing GL3 source program now consumes this same fog ledger. It supports
LINEAR/EXP/EXP2, absolute/signed eye-plane and radial distance, RGB fog mixing
and source triangle fans. It retains inherited framebuffer, viewport, depth,
blend, texture and matrix state. It does not enter the diagnostic scene renderer.
GL_FLAT is admitted for the existing constant-color primitive profile; varying
flat colors retain an explicit dependency.

The original NVIDIA driver starts with absolute eye-plane distance 34140 and
selects radial 34139 on the first air call. Controlled pixels distinguish
vertex-distance interpolation from fragment Euclidean distance on that profile.
The source shader follows the observed vertex-distance behavior. This does not
claim that every OpenGL driver uses the same allowed approximation. The
[fog fixture](../../tests/reference/client/renderer_fog_state/README.md)
records normative specification links, driver identity and capability limits.

## Sky and ChunkRenderer culling

Actual `n.a(F)` uses its constructor-compiled upper, star and lower source lists.
It reads the genuine World sky vector and draws the fogged upper list before
disabling fog/alpha, enabling blend and disabling standard lighting. The
provider's actual sunrise array supplies a smooth triangle fan; its rim retains
the raw RGB even when anaglyph changes the center. Real Engine texture lookups
draw sun and moon, followed by stars, horizon tint and the lower list. Depth
writes and the source texture/alpha/blend/fog states follow the original order.
The initial Nether flag returns before any GL, Math or Tessellator operation.
[World sky leaves](../world/sky.md) provide the real scalar/array children.

The source SR wrapper owns the actual W result and live b/c/d camera origin.
Box tests subtract that origin before calling DR. N culling rereads its live
ChunkRenderer array, honors real DK empty-pass flags and the `(index+j)&15`
visibility throttle, tests actual DK bounds, then stores visibility. Its wrapping
j counter increments after the whole loop. No terrain mesh or always-visible
substitute supplies these results.

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

New source fixtures separately cover 366 World/provider sky rows, 48 fog-state
rows, 30 authored GPU pixels, eight SR/N/DK culling rows, four early N sky outcomes
and 48 whole-sky post-call states. The Windows NV-present fog context matches all
48 state rows exactly. NV-absent Linux contexts compare common original fields
separately from the bytecode-derived skipped-selector branch; no original
false-capability observation is claimed. All 30 authored GPU samples match exactly
on Windows NVIDIA and Linux Mesa. These are controlled shader probes rather
than complete sky/frame pixel comparisons.

The whole-sky native device fixture uses genuine App/startup and N list
construction with explicitly prepared World/camera operands. Its actual lower
list GPU vertex/color record is checked independently. Original Engine cache-hit
operands and authored native sun/moon textures bound that comparison. See the
[sky fixture](../../tests/reference/client/render_global_sky/README.md) and
[culling fixture](../../tests/reference/client/frustum_culling/README.md).

At the previous qualified standard Block checkpoint, the loaded Windows graph
entered [ChunkRenderer compilation](chunk-compilation.md) at396 after Sky, fog
and culling, compiled standard Block lists and reached PX486. Its inspected
854 by 480 back buffer showed the reached Sky passes without terrain or HUD.
The current loaded-World Windows profile passes 704 checks in 12.69 seconds,
retains all 72 original focus histories/resources/saves and reaches U.b496.
Its inspected `load.png.world-prefix.png` shows real authored grass terrain and
Sky without HUD. Eight focused submission CTests pass in 4.51 seconds, zero
failures/skips; hosted qualification is pending. Neither these native captures
nor finite body/state comparisons establish a matched original gameplay frame.
The seven-profile original N cohort excludes issued-query N execution, including
a 17-entry case. Specialized geometry, Entities, particles, HUD, hand composition and extended play
remain unfinished or unqualified. The separate diagnostic World renderer
remains outside the default source path.
