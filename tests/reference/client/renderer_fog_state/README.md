# Actual PX fog state and source shader probes

The observer invokes unchanged original private `px.a(IF)` on a genuine
LWJGL2.9.0 Display. It declares prepared PX/App/Settings/LocalPlayer/World
fields, a real ge.e(16) FloatBuffer and real Overworld/Nether Dimension binds.
Only World block and metadata transport are authored: air/water9/lava11,
metadata0. Original immersion, FloatBuffer and PX methods remain unchanged.
The PX constructor and source World construction are not qualified here.

The 48 scalar rows cover both Dimensions/anaglyph values, pass -1/0/1 and
cloud/water/lava/air. Partial tick includes negative infinity and NaN because
the pinned method never loads that argument. GL fog color/mode/density/range,
current normal/color, ColorMaterial selectors, actual NV distance selector and
buffer position/limit/capacity are observed. Initial NV advertisement/selector
is recorded before the first method. Cloud/water/lava precede the first air
call, showing that the exponential paths preserve the initial selector.

Native replay preserves all original and native values. An NV-present native
context must match the complete original rows. An NV-absent context compares
common fields to the original independently of the bytecode-derived skipped
selector behavior. Such a run does not qualify the original NV true branch or
claim an observed original false-capability profile.

Separate authored GPU probes use source projection scale(1/8,1/8,1/32),
modelview identity and one white triangle at (-6,-6,-2), (6,-6,-18), (0,6,-2).
They disable dither/texture/lighting/blend/alpha/depth/cull/scissor/multisample,
use black fog and sample five RGBA pixels for absolute and radial distances
under LINEAR range[0,24], EXP density.1 and EXP2 density.1. These are source
shader probes, not source sky/frame observations. Native geometry goes through
the canonical 32-byte source batch and context lease; no diagnostic scene entry
or mesh renderer is used. The current finite comparison requires exact RGBA
bytes and records every sample/delta before failing on any mismatch.

The original NVIDIA profile advertises NV distance, initially selects absolute
eye plane34140 and changes to radial34139 on the first air call. Its controlled
radial LINEAR pixels distinguish vertex-distance interpolation from per-fragment
Euclidean distance. This finite driver observation does not generalize to every
OpenGL implementation. The normative RGB fog mixing and available distance
profiles are described by the [OpenGL2.1 specification](https://registry.khronos.org/OpenGL/specs/gl/glspec21.pdf)
and [NV_fog_distance](https://registry.khronos.org/OpenGL/extensions/NV/NV_fog_distance.txt).

Recapture on Windows with pinned JDK17, SDK JARs and authenticated native DLL:

```text
python -B tests/reference/client/renderer_fog_state/capture_reference.py \
  --client-jar /private/b1.7.3.jar --library-root /private/launcher/libraries \
  --output /private/new-fog-state-capture
```

Receipts hash tools, source/SDK inputs, native archive/extracted DLL, helpers
and method pins before and after capture. The observer admits only the exact
SDK native link, denies network/file writes and records actual GL driver text.
