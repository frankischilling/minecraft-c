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

Replay selects the captured corpus through `reference-profiles.json`, using the
native context's queried vendor, renderer and complete version string. Legacy
and native context versions are recorded separately. The framebuffer format
must also match. Every corpus has a fixed hash and complete inventory; unknown
drivers or formats fail explicitly. Selection does not change any expected
pixel, source-state or vertex word.

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
Together with the two actual converted-quad profiles, each NVIDIA and Mesa
corpus contains 58 profiles, 59 frames and 138 pixel/state/auxiliary rows.

Material tracking has its own 16-row control. It queries actual front/back
material components and shininess, current color and selectors before and after
tracked direct writes, face/mode switches, disabling/re-enabling tracking and
untracked writes. This state comparison does not depend on raster equality.
The separate material-sequence control uses nine fresh contexts for prefixes
0 through 8, with one full material query at each endpoint. A tenth context
executes unchanged `RasterObservation.scene(32)`, including its draw and pixel
capture, before querying materials. These 12 rows preserve the query order that
exposes Mesa's staged updates. A query inserted between setters can change the
result. The native fixture uses source getters in the same order as the frozen
`MaterialObservation.state()` and normal-list `state()` helpers.

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
NVIDIA direct and compiled normal words differ for all 256 values. Mesa direct
and compiled words differ for 254 values; -128 and +127 agree. Comparing the
drivers' direct-array words gives 255 differences, with byte zero equal. Their
compiled normal words agree for all 256 values.

The separate compiled-list diagnostic captures all 256 signed-byte normals
through transform feedback after actual fixed-function list compilation. It
changes the backing storage and current normal-array state before two replays.
Six additional rows query the current normal, current color, tracked front/back
ambient/diffuse materials and selectors before/after compilation and two calls.
`list-state.json` projects each captured list's last vertex into the expected
submitted GPU normal and preserves the six directly queried state rows. The
256 projected values are feedback observations, not direct legacy queries of
post-list current state. NVIDIA adopts the last list-array normal/color;
Mesa retains the preceding current attributes and explicit list Color.
No formula supplies the expected feedback words.

The color diagnostic captures all 256 unsigned-byte values through direct
arrays and compiled lists, then repeats each list after a second storage/state
change. NVIDIA divides direct colors by 255 and uses reciprocal multiplication
for list colors; Mesa reverses those paths. Exactly 126 byte values distinguish
the rounding. Native `--color-inputs` reads the actual GPU buffer and compares
512 direct/list rows, or 6,144 RGBA components across three vertices. Repetition
must preserve the submitted words. Current color after a direct array draw is
observed separately and is not used as its attribute oracle.

`ControlledPrecisionObservation` is a separate authored diagnostic with 6,486
draws and 19,530 samples. Clear, uniform-fragment, fixed-function and GLSL120/330
controls separate byte/float attributes, flat/smooth varyings, clamping and
normalized output conversion. Actual RGBA8, RGBA32F and simultaneous mixed
attachments have separate float and byte reads. Its
[selected observations](driver-precision-observations.json) retain exact words
and hashes of the two complete captures. NVIDIA and Mesa preserve the same
floating words for half-color and the two transformed normalized controls,
while their RGBA8 bytes differ. Small float differences also occur in some
byte-color gradients. These observations justify keeping separate measured
driver corpora; they supply no predicted pixel values or conversion formula.

## Reproduce

Use JDK17, the supplied client archive and pinned local SDK/native archives.
Each driver requires a fresh output directory and retains before/after input
and helper hashes, bounded process receipts, actual driver/format receipts and
checked context cleanup. Windows guards permit the pinned `lwjgl64.dll`.
Linux guards permit the pinned `liblwjgl64.so` and the five explicitly named,
hashed JDK libraries. Both reject writes/connections/listeners and require zero
network attempts.

The Linux successor `capture_mesa.py` retains every frozen scene and input. It
generates local copies by replacing only each authored helper's native-link
guard. Reversing that one replacement must recover the full original source
bytes before compilation. The guard admits the pinned `liblwjgl64.so` and five
named local JDK libraries, all hashed before and after capture. The original
game archive and selected method bodies keep the same pins and authority
checks. All 58 profiles, 59 frames and 138 pixel/state/auxiliary rows are
mandatory; the Windows corpus remains unchanged.

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

On Linux, supply the separately pinned official LWJGL2.9.0 Linux native archive:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 -B \
  tests/reference/client/lighting_raster/capture_mesa.py \
  --client-jar b1.7.3.jar --library-root /path/to/minecraft/libraries \
  --native-archive /path/to/lwjgl-platform-2.9.0-natives-linux.jar \
  --output .local/lighting-raster-mesa-original
```

Windows raster fixtures stay in this directory; the separately captured Mesa
fixtures stay in `mesa/`. Their complete `finite.json`, `extended-finite.json`
and `converted-finite.json` bytes are bound by `reference-profiles.json`, with
the original capture hashes. `run_native.py`, `run_extended.py` and
`run_converted.py` require those
complete inventories and checked native completion markers. A diagnostic
`--profiles` selection is explicitly marked incomplete and provides no credit
for omitted profiles. The material pair is `capture_material.py` and
`run_material.py`, using `material.json`. The separate precision diagnostics are
`capture_conversion.py` and `capture_conversion_receipts.py`; their outputs
remain distinct from pixel qualification.

`capture_list_conversion_receipts.py` records the separate compiled-list
diagnostic. `run_list_state.py --executable <native-fixture> --work-dir <receipts>`
uses `--list-state` on the canonical fixture and compares its 256 submitted-normal
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

For Linux normal/list/material observations, `capture_mesa_attributes.py` uses
the same reversible guard replacement as the raster capture. Its frozen helper
bodies and original archive/type dependencies remain unchanged:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 -B \
  tests/reference/client/lighting_raster/capture_mesa_attributes.py \
  --client-jar b1.7.3.jar --library-root /path/to/minecraft/libraries \
  --native-archive /path/to/lwjgl-platform-2.9.0-natives-linux.jar \
  --output .local/lighting-mesa-attributes
```

`capture_color_conversion.py` supports both native profiles. Use
`--native-profile windows` on Windows with the same library root, or capture
Linux separately as follows. It needs no game archive:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 -B \
  tests/reference/client/lighting_raster/capture_color_conversion.py \
  --native-profile linux --library-root /path/to/minecraft/libraries \
  --native-archive /path/to/lwjgl-platform-2.9.0-natives-linux.jar \
  --output .local/lighting-mesa-colors
```

`capture_material_sequence.py` compiles the frozen Raster and Material helpers
unchanged and calls their scene/query methods from its own native guard. The
game archive is a pinned type-verification dependency. `--prefixes` with no
numbers captures all nine prefixes in separate JVMs; omitting it captures the
complete scene32 draw before its material query. Use different fresh output
directories for the two captures:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 -B \
  tests/reference/client/lighting_raster/capture_material_sequence.py \
  --client-jar b1.7.3.jar --library-root /path/to/minecraft/libraries \
  --native-profile linux \
  --native-archive /path/to/lwjgl-platform-2.9.0-natives-linux.jar \
  --prefixes --output .local/lighting-mesa-material-prefixes
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 -B \
  tests/reference/client/lighting_raster/capture_material_sequence.py \
  --client-jar b1.7.3.jar --library-root /path/to/minecraft/libraries \
  --native-profile linux \
  --native-archive /path/to/lwjgl-platform-2.9.0-natives-linux.jar \
  --output .local/lighting-mesa-material-sequence
```

On Windows, use Python directly with `--native-profile windows`; the script
finds the pinned Windows native archive under the supplied library root.
The combined `material-sequences.json` fixture contains the nine endpoint rows
followed by all three complete draw/state/material rows. Both capture hashes
are recorded. `run_material_sequences.py` requires all 12 rows from ten fresh
native processes. The material getters preserve ambient/diffuse/specular/
emission order for each face, then front/back shininess and current color.
The normal-list queries retain their separate normal/color/material order.

`run_color_inputs.py` compares the 512 submitted-color rows with
`color-inputs.json` and verifies their projection from `color-conversion.json`.
The direct-normal and list runners likewise validate their complete observed
inputs. All replay runners select the exact driver and framebuffer profile;
unknown identities or formats fail explicitly. The measured native identities
are NVIDIA RTX 3060, driver 581.42, core version `3.3.0 NVIDIA 581.42`, and Mesa
llvmpipe LLVM 20.1.2, core version
`4.5 (Core Profile) Mesa 25.2.8-0ubuntu0.24.04.4`. These identities do not grant
support to other versions or devices. The runtime also refuses unmeasured
driver-dependent array/material operations. Captured JSON is used by tests.
Display requests at least OpenGL 3.3; the measured Mesa 4.5 path checks availability
before using indexed core-state queries beyond that minimum.

The [renderer contract](../../../../docs/client/lighting-raster.md) records the
remaining Mesa PopMatrix guard. With pending STORED attributes and a matrix
changed since Push but numerically equal to its saved value, Pop refuses before
mutation because the full inverse/type metadata is not retained. Unchanged
matrices skip the flush and different matrices flush. The guard can reject real
no-ops; it does not classify every refusal as an original mismatch.

All 59 frames and separate attribute/material controls pass on the measured
Windows NVIDIA and Linux Mesa profiles. The broader selections pass 19 Windows
MSVC Debug CTests in 47.10 seconds and 18 Linux Clang18 ASan/UBSan CTests in
34.23 seconds, with no CTest failures or skips. Hosted results belong to each
exact revision. These finite controls do not establish a complete original
World frame, model/Entity dispatch or playable client.
