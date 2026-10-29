# Source World-frame prefix

The native test invokes the genuine `px.a(FJ)` composition on a real hidden
OpenGL3.3 Display. It reuses constructed GAME Worlds, Chunks and Players from
the picking fixture. PX source fields are prepared for this finite body test;
its constructor is not qualified by the test.

The ordered prefix enables source culling/depth, invokes the actual picker,
captures and interpolates the camera, reads the real provider provenance,
updates the first anaglyph eye/mask and sets the source viewport. It then
executes `h(F)`'s render-distance blend and retains the actual missing sky-color
query at `px.h(F):56`. It does not skip fog to reach later camera, frustum,
ChunkRenderer, Entity, particle, HUD or hand work. No inspection renderer,
replacement texture or scene is called.

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
composition adds no GL error query, clear, swap, event pump or source rollback.
These checks establish neither complete World rendering nor visual equivalence
with the original client.
