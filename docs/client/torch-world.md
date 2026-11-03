# Torch World rendering

World render type 2 enters the actual Torch wrapper `cv.c(Block,III)` and shared
primitive `cv.a(Block,DDDDD)`. Registered Torch 50 and RedstoneTorch 75/76 use
the same geometry. Metadata 1..4 selects the four wall tilts; the other twelve
nibble values use the standing origin. The wrapper reads metadata before NW,
queries Block brightness, then rereads the live emission array before setting
gray color. The primitive calls the real side-texture getter even when a
texture override is set.

Each successful call appends five quads: twenty source vertices expanded to
thirty packed triangle vertices, or 240 words. It keeps the enclosing NW batch
open and preserves translation, Block bounds and unrelated CV scratch. It
does not set a normal, begin or draw a batch. The native World wrapper and
the direct helper entry call this primitive. Native inventory type 2 rendering
does not currently call it.

## Original evidence and native replay

The [Torch packet](../../tests/reference/client/torch_world/README.md) records
66 calls using genuine saved Worlds, four loaded Chunks, EW/CV constructors
and the original NW singleton. The matrix includes metadata 0..15 for all three
registered Blocks, light/emission/live-texture and override controls, four
direct primitive calls, five reached null failures and a null-material bypass.
Every field input is declared. Nonfinite coordinates and malformed backing
remain outside the cohort.

Twelve complete target stdout streams agree across the retained private and
public cohorts. Two early JDI observers reached their unchanged 150-second
deadline after 31 and 41 complete cases. Their partial traces are exact prefixes
of the completed trace, and their ordinary target outputs remain intact.
Batched read-only field reads and an inline equivalent backing audit reduced
observer overhead without changing source methods, profiles or assertions.
The public helper bytes were frozen before a fresh two-ordinary/one-JDI capture.

The completed trace records 8,723 events, 286 EW queries, 235 static captures,
63 shared-primitive entries and 13 live Code fingerprints. There are 1,220 calls
each to the NW five-double wrapper, UV setter and packed writer. Fifty-seven
World calls return true, four direct calls return void and five throw before
appending. The batch ends at word 14,640. All raw active words, retained failure
words, CV/NW primitive fields and Block bounds remain in the selected data.
Each VM also checks 138,397,392 non-emitted words against its own prior memory.

| Reached null receiver | Original failure | Earlier effects |
| --- | --- | --- |
| Cache, including null Block and cache together | `CV.c:8` | No fresh NW capture or appended words |
| Block, World wrapper with real cache | `CV.c:29` | Metadata and NW capture completed |
| Block, shared primitive, including texture override | `CV.a:7` | NW capture completed; no appended words |

Native CPU replay passes all 132 input/observation rows and 14,640 appended words.
It separately checks its own backing, strict FP admission and owner cleanup.
Outer-row equality does not establish a native per-query trace comparison.
Native particle draw comparison also passes, with the separate scope below.

## Native section controls

The DK control compiles Sand/Torch sections at metadata 1..5. Each publishes
66 packed vertices, including 30 Torch vertices, and checks positions, UVs,
colors, pass flags, unchanged definition reuse and real dirty recompilation.
Fire type 3 remains an explicit failed compile with its earlier list and
unfinished definition preserved. These are native integrations, separate from
executed-original enclosing DK evidence.

## Loaded native scene

The Windows `client.world_selection_torch` scene authors Sand and an upright
Torch in saved Chunk data, with metadata 5 and a saved Player pose, before
bootstrap. The ordinary NBT loader supplies the actual Player and camera.
The returned section has 60 packed vertices, including 30 Torch vertices with
checked positions, UVs and colors. Five ordinary returned World/HUD frames
include two reuse frames and one recompile after a real WorldAccess light
notification. The same World, Registry and display list remain in use; source
bounds and protected save bytes stay intact.

A genuine Torch display callback constructs Smoke and Flame through the same
DN manager without changing the World Random stream. A separately labelled,
explicitly entered World draw then proves both recorded particle identities
are visited exactly once in the complete four-entry DN traversal. It uses the
actual Timer partial, current performance setting 1 and a deadline independently
derived from the live PX.I value. The World call returns and retires with
checked cleanup. Full Timer bytes, loop frame/tick counts and loop clocks stay
unchanged around this entered draw.

The caller then resumes ordinary source advancement and obtains the five
World/HUD frames above. The particle-identity proof belongs to the explicitly
entered World draw; it does not assert that both newly published identities
survive until a later full run-loop frame. Ordinary Timer catch-up can expire
short-lived Smoke before that frame.

The final capture shows the upright Torch, Flame, Smoke and Sand, plus selection
outline, crosshair, hearts, hotbar, hand and clouds. This bounded native scene
passes 1,624 checks in 13.87 seconds (13.91 seconds CTest total), including
`cleanup=1`. Earlier 13.15-second/1,454-check and 13.50-second scene passes remain
separate receipts. Stronger ordinary-frame identity failures retain their
different timing premise and are not classified as gameplay defects. The
missing-factory and deterministic fixture failures also remain retained.
Original pixels, unrestricted play and FPS remain unqualified.

## Smoke and Flame

Torch display effects now have concrete Smoke (`xn`) and Flame (`qu`) owners,
with constructor and tick comparisons passing. Smoke's scale 2.5 constructor
also has a separate native/original comparison. The large-Smoke N branch is
wired and reviewed for source sites/order, without a separate live callback
replay. Native draw matches 18 rows and 25,165,440 own-inactive words,
including the XN61/QU46 and SN118/QU32 failure prefixes. The loaded native scene
above also reaches genuine source-callback construction and the separately
entered World draw, followed by ordinary World/HUD frames.
[Particle documentation](particles.md#smoke-and-flame) separates the original
independent-entropy constructors, deterministic components and native controls.

The original CPU evidence uses no draw/GL context or source replacement. It
does not establish original DK/frame/pixel equivalence, automatic NW flush,
unrestricted gameplay, audio or an FPS gain. Other specialized World shapes,
TileEntity draws and particle families retain their own unfinished paths.

## Full native qualification

The completed public suites pass 660/660 Windows MSVC Release tests in 338.89
seconds and 653/653 Linux Clang 18 ASan/UBSan tests in 162.88 seconds. These are
CTest wall times. Both preserve4,548 source inputs and 532 products, and Windows
also preserves 823 assets. All 1,445 runtime files match across hosts. GCC 13.3
checks 27 changed translation units across 28 actual configured option sets;
formatting covers 2,021 files. These broad native checks do not expand the
original CPU, entered World draw or ordinary scene qualification scopes.

Run the asset-independent comparison and packet controls with:

```sh
ctest --test-dir build-windows --output-on-failure --no-tests=error -R '^(client.torch_world|tools.torch_world_packet)$'
```
