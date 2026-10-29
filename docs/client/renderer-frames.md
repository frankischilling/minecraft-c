# Source frame input and World drawing

The default client now executes focused mouse look and opens the source pause
screen. Its World draw enters the real `px.a(FJ)V` pipeline and stops at the
missing sky-color query in `px.h(F)V:56`. Terrain meshes, fog/camera completion,
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
anaglyph eye's mask and viewport. Its fog entry computes the render-distance
blend before reaching the missing `fd.a(Entity,F)` sky-color query.

The performance deadline uses the source 200/120/40 target and Java long
wrapping. No timing, HUD or later camera/frustum/terrain operation is reported
complete after the reached failure. A retained call owns its native child
occurrences and prevents another World-frame invocation until retirement.

The [World-frame controls](../../tests/reference/client/world_frame/README.md)
use real GL and the same loaded GAME World/picker fixture. Eight finite math
vectors come from an actual JDK17 helper and pinned original method bodies;
they are not runtime observations of the complete original PX World renderer.
The separate diagnostic World renderer remains outside the default source path.
