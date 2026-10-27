# RenderEngine construction and generated image textures

The native RenderEngine owns the real `ji(ik,kv)` constructor state and selected
`ji.a(BufferedImage)` registration operation. It creates the actual collections,
direct buffers, ARGB missing-texture image and Graphics/default-font draw in
source order. Construction performs no GL call. Texture registration uses a
separate Display operation, generated-ID ledger and original-image Map.

The CPU API is in `mc/client/render_engine/render_engine.h` and
`mc/client/fonts/default_font.h`. Device operations are in
`src/client/render_engine/textures.h`. Keep one genuine ji/ge class namespace
for its actual engine objects. Allocate the RAW engine with retained class and
font owners, then construct it with genuine nullable texture-pack/Settings
identities. These identities are borrowed through final release, including a
stored failed prefix. A failed constructor is retained for inspection and
native retirement; it cannot be retried or published as a ready App owner.

The font provider discovers real system bytes through GDI or Fontconfig and
rasterizes with pinned FreeType. [Dependency setup](../dependencies.md#native-default-font-rasterizer)
records the archive, offline override and notices. The selected Windows Dialog/
Arial and Linux sans profiles do not implement every logical-font/script
fallback. No captured bitmap or bundled system font replaces the provider.

Generated driver resources and the source ge List are separate owners. The
native descriptor/context hold is reserved before GL generation. Later null
image, buffer, policy, map or driver failures retain completed source/device
effects and preserve the caller's result output. Nullable Settings skips
anaglyph at the actual source branch. The retained original image identity is
distinct from device texels; editing it does not automatically upload it.

Call `mc_client_render_engine_retire_resources` while its actual Display is
live, retain/retry BUSY or current-context failures, then release CPU owners
and their borrowed dependencies. Display destruction is BUSY while an external
generated resource remains. This explicit native cleanup is not source ge
cleanup. Context-loss cleanup leaves remaining driver state to actual context
destruction. These owners are exclusive-use; they are not a concurrent API.

## Build and tests

The three libraries are client-only: `mc_client_default_font`,
`mc_client_render_engine` and `mc_client_render_textures`. The constructor has
no SDL/OpenGL link dependency. The device target uses real Display. Simulation
and server targets acquire none of these dependencies. A fresh
`-DMC_BUILD_CLIENT=OFF` configuration omits them entirely.

With `BUILD_TESTING=ON`, `client.render_engine` runs the native CPU contract
without Java, original assets or a display. Both native test executables are
built so an ALL build covers their code. Enabling
`MC_ENABLE_CLIENT_DISPLAY_TESTS` also declares `client.render_textures`,
`tools.render_engine_normal` and `tools.render_engine_optimized`. These tests
require a real OpenGL3.3-core-capable context. The two Python contracts execute
both genuine native observers and protocol rejection controls, including
Python-O. A missing graphics context fails rather than skipping those tests.

`MC_ENABLE_RENDER_ENGINE_REFERENCE_TESTS=ON` adds `reference.render_engine` on
Windows/Linux. It requires explicit `MC_REFERENCE_CLIENT_JAR`,
`MC_REFERENCE_CLIENT_LIBRARY_DIR`, and matching JDK17 `Java_JAVA_EXECUTABLE` /
`Java_JAVAC_EXECUTABLE`. It freshly compares the full64x64 ARGB image and ten
glyph records with the supplied unchanged constructor/JDK font provider.

`MC_ENABLE_RENDER_TEXTURE_REFERENCE_TESTS=ON` adds `reference.render_textures`.
It additionally requires Windows, `MC_ENABLE_CLIENT_DISPLAY_TESTS=ON` and
`MC_REFERENCE_CLIENT_NATIVE_DIR`. This profile executes the actual original
compatibility Pbuffer and compares its selected generation/parameter/cache/
GPU/transfer rows with the native core-context operation. There is no Linux
original-GL profile. Original files and generated observations stay private.

The reference runner also supports direct execution:

```
python -B tests/reference/client/render_engine/compare.py --client-jar <jar> --constructor <native-constructor> --java <java> --javac <javac> --libraries <library-directory> --work-dir <private-output>
```

On qualified Windows, add `--textures <native-textures> --native-dir <natives>`
for the original GL comparison. Complete readers, exact successful exits,
positive terminal inventories, class/Code origins and before/after product
hashes are required. Captured helper classes, fonts and raster output are not
public source payloads.

The combined public matrix passes 389 Windows and 381 tests on each sanitized
Linux compiler. Every preceding 376/348/348 case remains. Each compiler passes
8,333 CPU, 223 device and 55 separate trace checks, plus nineteen controller
controls normally and under Python optimization. Fresh original CPU comparisons
match 4,096 ARGB pixels and ten glyph metrics on each host; ten original GL rows
match on Windows. Actual source, configured products and official FreeType
sources remain unchanged. Separate production/headless build-mode checks pass.

## Resource lookup observations

The selected-Pack String lookup and CPU color-array owners use the actual b/c
Maps and shared ImageIO input path. Their supported Default/ZIP/cache/missing
paths retain source order, array identity and failed prefixes. Native stop points
for strip conversion, diagnostic continuation, unsupported providers, GL_CLAMP
and mipmaps remain explicit; none becomes a completed lower call.

A retained producer cohort passes ALL, six selected CTests and eleven native-only
profiles on MSVC Debug and sanitized Clang 18/GCC 13. The later Python publication
policy has separate normal/-O parser controls. One retained Windows JDK17 CPU
default attempt has independently verified read-only admission and matches seven
original rows, including whole arrays and cache identity. Its original failed
reader report remains retained. Added exact JDK class/nest proof and the narrow
library-tag correction change admission only; they are not new producer code.
Original ZIP, private getRGB, cold-static and texture/GL scope remains separate.

`MC_ENABLE_RESOURCE_LOOKUP_OBSERVERS=ON` compiles the two probes and adds four pure
public Python tests. The CI proposal enables it on Windows and Linux without
turning on supplied-original options or adding private resources. This later
source/workflow/provenance proposal has no new native or hosted qualification.
The [observer README](../../tests/reference/client/resource_lookup_common/README.md)
records capture, actual-origin and producer/policy boundaries.

## Remaining source boundary

There is no App.p publication or normal bootstrap adoption in this component.
Startup adoption of the selected-pack/String/ImageIO and FontRenderer owners,
effects registration and update/subimage upload remain separate work. The mipmap path stops
at its first unimplemented body after level zero; source GL_CLAMP stops at its
first incompatible parameter instead of silently becoming CLAMP_TO_EDGE.
Dynamic textures, source ge cleanup and full scene rendering remain unfinished.
Fixed generated-world inspection and these finite owner tests do not establish
a playable client, normal world-entry rendering or complete visual equivalence.
