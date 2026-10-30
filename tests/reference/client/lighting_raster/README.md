# Lighting raster controls

These controls compare the canonical source GL3 renderer with authored legacy
OpenGL scenes on a real compatibility context. They use no Minecraft textures
or worlds. Most profiles call unchanged LWJGL wrappers directly. Separate
converted-geometry profiles call the original NW methods from the supplied
client archive and record their actual vertex words before drawing.

The frozen initial capture contains 38 profiles, 39 complete 64 by 64 RGBA8
frames and separate state rows. It covers signed-byte endpoints and zero,
current normals, four NORMALIZE/RESCALE combinations, varying normals/colors
with flat and smooth triangles/fans, light upload before or after matrix
changes, mirrored scale, material tracking, list replay, texture/alpha and fog.
Each captured pixel row contains all 16,384 bytes in top-down order. Replay
compares every byte and state field exactly; it applies no tolerance. A failure
records differing pixels/channels, maximum difference and bounded examples.

The initial names `converted_quad_flat` and `converted_quad_smooth` are retained
as historical input identities. Their Java `float[][]` arguments select NW's
`a(FFF)V` color overload. They produce no vertices and exercise genuine empty
NW draws. They provide no converted-geometry raster evidence. The separate
`ConvertedRasterObservation` uses explicit `double[][]` coordinates, requires
actual `h=6` and `o=48`, records all 48 source words, and checks the two fresh
duplicate normal words remain zero before the real draw. Its flat and smooth
frames are a distinct qualification.

The successor extended capture adds nonzero specular lighting with shininess
0/8/32, local viewer and positional lights, front/back materials and two-sided
lighting, back-only color tracking, separate specular color with texture/fog,
unlit varying-color flat/smooth triangles/fans, and lighting clamped before
interpolation. Each frame also records local-viewer, two-sided and color-control
state. These profiles remain separate from the frozen initial fixture.

Material tracking has its own 16-row control. It queries actual front/back
material components and shininess, current color and selectors before and after
tracked direct writes, face/mode switches, disabling/re-enabling tracking and
untracked writes. This state comparison does not depend on raster equality.

## Normal conversion and precision

The driver diagnostics distinguish three operations: immediate `glNormal3b`
queried with `GL_CURRENT_NORMAL`, actual `GL_BYTE` normal arrays captured as
`gl_Normal` through GLSL transform feedback, and explicit floating normals.
They separately record fixed-function lighting on the RGBA8 default framebuffer
and an actual RGBA32F attachment. The latter explicitly enables vertex-color
clamping and disables fragment/read clamping. These are authored driver probes;
they execute no original Minecraft or NW body.

The exhaustive successor records all 256 byte values and queries the normal
array type, stride and buffer binding. On the captured NVIDIA profile the
observed array conversion differs from immediate `glNormal3b`. Treat that as
measured driver behavior, not a replacement for the GL specification or proof
for other devices. The receipts preserve the exact component bits so a proposed
conversion can be checked against every observed value.

The separate compiled-list diagnostic captures all 256 signed-byte normals
through transform feedback after actual fixed-function list compilation. It
changes the backing storage and current normal-array state before two replays.
Six additional rows query the current normal, current color, tracked front/back
ambient/diffuse materials and selectors before/after compilation and two calls.
`list-state.json` projects each captured list's last vertex into the expected
native current normal and preserves the six directly queried state rows. The
256 projected values are feedback observations, not direct legacy queries of
post-list current state. No formula supplies their expected bits.

## Reproduce

Use JDK17, the supplied client archive and pinned local SDK/native archives.
Each driver requires a fresh output directory and retains before/after input
and helper hashes, bounded process receipts, actual driver/format receipts and
checked context cleanup. Its guard permits only the pinned `lwjgl64.dll`,
rejects writes/connections/listeners and requires zero network attempts.

```powershell
python -B tests/reference/client/lighting_raster/capture_reference.py `
  --client-jar b1.7.3.jar --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/lighting-raster-original
python -B tests/reference/client/lighting_raster/capture_extended.py `
  --client-jar b1.7.3.jar --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/lighting-raster-extended-original
python -B tests/reference/client/lighting_raster/capture_converted.py `
  --client-jar b1.7.3.jar --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/lighting-raster-converted-original
```

After reviewing successful complete captures, preserve their exact `finite.json`
bytes as `finite.json`, `extended-finite.json` and `converted-finite.json` in this
directory. `run_native.py`, `run_extended.py` and `run_converted.py` require those
complete inventories and checked native completion markers. A diagnostic
`--profiles` selection is explicitly marked incomplete and provides no credit
for omitted profiles. The material pair is `capture_material.py` and
`run_material.py`, using `material.json`. The separate precision diagnostics are
`capture_conversion.py` and `capture_conversion_receipts.py`; their outputs
remain distinct from pixel qualification.

`capture_list_conversion_receipts.py` records the separate compiled-list
diagnostic. `run_list_state.py --executable <native-fixture> --work-dir <receipts>`
uses `--list-state` on the canonical fixture and compares its 256 current-normal
rows and six state rows exactly. It checks native cleanup and input identities.
This control supplies no pixel or complete-frame evidence.

`run_array_normals.py` uses the separate native `--array-normals` control. After
each of 256 actual direct-array draws, it observes the bound GPU buffer and
attribute 3's float3 layout, then reads back all three submitted normal vectors
with `glGetBufferSubData`. Position and UV words must match the authored input,
and the canonical current normal must become unknown. `array-normals.json`
copies the actual array transform-feedback values from the complete public
`normal-array.json`; the runner verifies that file's hash and projection before
comparing every vector bit. It calculates no expected normal conversion.

These controls do not establish a complete original World frame, model/Entity
dispatch or playable client. Windows MSVC Debug replay matches all 58 profiles
and 59 complete frames exactly. The material, list-state and array-normal
comparisons also pass. These controls are part of a 15-test selection passing
in 46.78 seconds, with no failures or skips. Hosted qualification is pending;
other original legacy-driver profiles remain unqualified.
