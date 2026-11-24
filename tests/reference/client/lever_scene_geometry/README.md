# Lever circuit scene geometry

Two fresh ordinary VMs run 160 complete calls each against the verified,
unchanged Beta 1.7.3 client. The rows cover skylight subtraction 0 through 15,
ambient occlusion off/on, and the before/off/on circuit states. They render
Lever69 at (3,65,2), when present, and Dust55 at (4,65,2). The Tessellator's
actual translation is (0,-64,0), applied before final float narrowing.

The source `cv.b` dispatcher reaches ordinary visibility, the Lever `xr`
bound update, `cv.e` Lever or `cv.g` Dust body, and the selected flat/AO base.
The attached Stone hides one Lever base face. The complete Lever contribution
has 66 encoded triangle vertices: 30 for the base and 36 for the handle.
Dust contributes 12 vertices for its two overlays. Before placement, the
absent Lever changes Dust connectivity; that state has separate original rows.

The saved inputs are newly authored synthetic project data. `scene-input.bin`
contains Blocks, Data, SkyLight, BlockLight and HeightMap, in that order, for
Chunk(0,0). It has 82,176 bytes. `scene-input-origin.json` retains the independent
authoring audit and each plane's hash. The author is
`tests/tools/client/test_lever_loop.py:authored_regions`, using the unchanged
Torch 441-Chunk input builder and a fresh circuit edit. No personal World or
original game's generated terrain is contained in this binary.

Each original VM loads a distinct copy of the fresh 441-Chunk synthetic save.
Before the calls, the observation helper requires all five actual loaded
Chunk planes to equal the declared binary. Each profile changes only the
declared Lever cell and Lever/Dust metadata. It asserts that rendering leaves
all five planes intact. The original World, ChunkCache, Block, Chunk and
Tessellator are genuine owners. An explicitly raw application/settings prestate
provides the AO option; application construction is outside this comparison.
Network, native-library and process operations are denied and remain unreached.

The private capture is `.local/lever-scene-original-3`. Its two raw outputs,
compiled helpers and JDK input hashes are retained by `provenance.json`.
The earlier `.local/lever-scene-original-1` is a preliminary 128-call capture
with separately authored arrays. It does not supply these final 160 rows.
The old `lever_world` 32-row packet remains unchanged.

Capture 2 and its failed native comparison remain retained. Its helper called
`tess.b(0,-64,0)`: Java selected `nw.b(FFF)V`, the normal setter, instead of
the intended `nw.b(DDD)V` translation. Thus its Y coordinates remain near 65
and its inactive normal word is `00004000`. The raw packet is preserved
byte-for-byte under `history/capture2`; it is unqualified for translated
geometry. No renderer change or observed-word conversion resolves this error.
Capture 3 uses explicit double literals, asserts the actual translation fields
and unreached normal state, and checks the compiled helper's invocation
descriptor. Two fresh source VMs then supply the current literal rows.
Public controls reject the untranslated predecessor even if its row hash is
rebound to a new receipt.

To repeat the source calls, prepare a new circuit save with the author above:

```powershell
python -B tests/reference/client/lever_scene_geometry/capture.py --world <fresh-synthetic-world> --output .local/lever-scene-repeat
```

The output directory must not exist. The command requires the verified local
archive, pinned local Launcher libraries and matching Java/Javac tools. It
fails on missing prerequisites or altered inputs; absence is not a parity pass.

`test_lever_scene_geometry.c` compares all 160 native rows, including every
appended 32-byte record and final bounds, texture override, AO flag and pool
advance. It uses the actual native dispatcher and a genuine prepared owner
graph. Native fixture setup copies the declared synthetic planes before each
profile. Neither renderer nor SDK applies a coordinate correction afterward.

For a loaded SDK batch, `tests/tools/client/lever_geometry.py` requires one
unique contiguous Lever body and one unique contiguous Dust body. It compares
the exact position, UV and packed color arrays, 24 bytes per 32-byte vertex.
The final eight bytes are inactive NW backing cells when arrays7 is submitted.
The helper separately compares 150 Block, metadata, SkyLight and BlockLight
cells covering radius two around both cells. HeightMap and outside cells stay
retained but unqualified by that selected operand check.

These finite CPU calls qualify the declared scene operands. They do not prove
original loaded pixels, audio, arbitrary terrain/callbacks, application
construction, whole-World lifecycle, native focus reliability or FPS.
