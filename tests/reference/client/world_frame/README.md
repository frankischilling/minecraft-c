# Source World-frame prefix

The native test invokes the genuine `px.a(FJ)` composition on a real hidden
OpenGL3.3 Display. It reuses constructed GAME Worlds, Chunks and Players from
the picking fixture. PX source fields are prepared for this finite body test;
its constructor is not qualified by the test.

The ordered prefix enables source culling/depth, invokes the actual picker,
captures and interpolates the camera, reads the real provider provenance,
updates the first anaglyph eye/mask and sets the source viewport. Actual
`h(F)` uses the canonical World's sky/fog/cloud methods, weather and camera
immersion, stores canonical PX RGB channels, interpolates L/M brightness,
applies anaglyph conversion and sets the real GL clear color. The frame then
clears color/depth at269, enables culling275 and invokes actual camera282.
The canonical `w` frustum singleton then extracts those same camera matrices
at285. Retained diagnostics stop at sky-pass fog setup306 for distance<2 or
the GL3 fog-enable bridge318 otherwise. Later sky/terrain/Entity/particle/HUD
work is not skipped. Native controls also preserve the earlier null-N camera
fault865 and use a genuine RAW N for its literal false cloud query before
qualifying the frustum and the two following fog gates.
No inspection renderer, replacement texture or scene is called.

[FogObservation.java](FogObservation.java) executes the unchanged original
private `px.h(F)` on a real LWJGL2.9.0 Display. Its 96 rows in
[fog-vectors.json](fog-vectors.json) cover three source times, four supported
render distances, both anaglyph settings and air/water/lava/cloud branches.
The source PX/App/Settings/World/LocalPlayer fields are prepared. Real
WorldInfo, Overworld Dimension and manager/noise constructors run; original
World sky/fog/cloud/weather, camera immersion and biome HSB methods remain
unchanged. The helper World subclass supplies only authored block and
metadata transport (air, water9, lava11, metadata0). It does not override a
color method. Both original and native tests query actual GL clear-color
state and require bitwise agreement with their source RGB fields. A native
late clear-color admission failure also checks retained stores and cleanup.
No complete World or PX constructor, Chunk transport or visual parity is
claimed by these rows.

Recapture the full original fog rows with pinned JDK17/SDK inputs:

```text
python -B tests/reference/client/world_frame/capture_fog.py \
  --client-jar /private/b1.7.3.jar --library-root /private/launcher/libraries \
  --output /private/new-full-fog-capture
```

[Fog source pins](fog-source-pins.json) identify h and its World/camera
descendants. The Windows observer admits the authenticated lwjgl64.dll and
JDK AWT link needed by original biome HSB conversion; it denies network and
file writes. Capture receipts hash tools, source/SDK JARs, native archive,
extracted DLL, JDK AWT DLL, helpers and method pins before and after execution.

[Source pins](source-pins.json) record the unchanged original `px.a(FJ)V` and
`px.h(F)V` bytecode identities in the pinned client. Eight JDK17 vectors in
[math-vectors.json](math-vectors.json) qualify the bytecode-derived Java float,
`Math.pow` and narrowing expression for render distance. They are authored
expression observations, not execution of either original PX method. Native
libm must match the finite 0..3 vectors exactly. Division-by-zero gives negative
infinity at distance4; wrapped negative denominators retain NaN classification.
General NaN payload/sign parity and other render-distance values are unqualified.

Recapture the math vectors and verify the original method pins:

```text
python -B tests/reference/client/world_frame/capture_math.py \
  --client-jar /private/b1.7.3.jar --output /private/new-fog-blend-capture
```

The native controls also check source interpolation, deadline preservation,
occupied-output and pending-call admission, native occurrence counts and
checked cleanup. Successful GL error inspection follows the test; production
composition adds no GL error query, swap, event pump or source rollback.
These checks establish neither complete World rendering nor visual equivalence
with the original client.
