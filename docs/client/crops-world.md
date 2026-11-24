# Crops World rendering

World render type 6 now enters the original Crops geometry family. It draws four
vertical planes with both windings, using stage-dependent terrain UVs and the
Block's actual brightness. The geometry appends 32 source vertices to the
caller's existing NW batch; ordinary quad expansion produces 48 triangle
vertices. It does not begin or draw a separate batch.

The authority is the unchanged client JAR SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The [reference packet](../../tests/reference/client/crops_world/README.md)
records real World/EW/CV/NW executions and method fingerprints.

| Original client method | Reached behavior |
| --- | --- |
| `cv.b(Luu;III)Z` | Captures render type, calls that same Block's bounds update at 15, invokes type 6 `j` at 95 and returns at 98. |
| `cv.j(Luu;III)Z` | Captures NW at 0, reads brightness at 14, sets gray color at 27, reads metadata at 40, invokes the primitive at 56 and returns true at 60. |
| `cv.b(Luu;IDDD)V` | Captures NW at 0, calls metadata texture at 8 before applying override, computes float UVs widened to double, emits eight quads and returns at 721. |
| `ni.a(II)I` | Replaces negative metadata with 7, then adds live `bm` with JVM signed wrapping at 12. Positive metadata has no upper clamp; side is unused. |
| `uu.a(Lxp;III)V` | Empty inherited bounds update. NI and WB declare no override; all prior bounds remain. |

The wrapper converts integer Y to float, subtracts `0.0625f`, then widens the
result to double. It does not subtract in double precision. The packet includes
large positive and negative Y values that distinguish those operations.

The first pair of planes uses `(x+0.5)±0.25`, spanning `(z+0.5)±0.5`; the second
uses `(z+0.5)±0.25`, spanning `(x+0.5)±0.5`. Those double operations retain their
source order. Every plane has both original windings and UV orientation.
Neither Crops bounds nor unrelated CV rotation, color or lighting scratch
changes the plane positions.

Native geometry lives in [crops.c](../../src/client/render/blocks/crops.c).
The existing registry metadata-texture query supplies the actual Block result,
including the signed-add behavior. Brightness and metadata use the same live
World/EW interface as the neighboring block families. A valid texture override
does not bypass the virtual texture getter.

The packet covers 44 calls, including all 16 World metadata nibbles, direct
primitive metadata extremes, mutable texture indices, brightness/emission,
overrides, null prefixes, retained bounds/CV/NW fields and translated geometry.
Successful calls append 15,744 words; their own prior inactive backing checks
cover 92,258,944 words. Native replay compares the same 88 input/observation rows,
with four additional actual dispatch controls and two native floating-point
admission controls. The original EW/JDI trace remains reference evidence;
matching outer native rows does not claim a native per-query trace comparison.

The Windows MSVC Release replay passes 3,512 checks; its packet has 20 corruption
controls. Linux Clang 18 Debug ASan/UBSan also passes the native replay, in 1.31
seconds within the complete 661-test public cohort. Strict GCC 13 C17 compilation
passes for the new runtime and native fixture, with the configured warning,
floating-point and sanitizer options. Those compile checks retain their separate
scope from the Linux execution result.

The actual DK controls now cover 16 profiles: eight fluid cases, four Ladder
orientations and Crops stages 0, 7, 8 and 15. Each Crops case reaches the type 6
dispatcher return at 98, publishes 48 vertices, retains its completed definition
on an update with no compilation, then recompiles after a real World light
notification. Positions, UVs and colors match the original section row; unused
packed words retain their own prior backing values. See
[ChunkRenderer compilation](chunk-compilation.md).

## Loaded Farmland and Crops

The saved scene contains mature Crops (ID 59, metadata 7) at `(3,65,3)` above
Farmland (ID 60) at `(3,64,3)`. The saved Player stands on a Stone platform at
`(3,64,0)`, with feet at 65, yaw 0 and pitch 26. The wrapper independently decodes
the saved cells and pose before bootstrap. The ordinary source loop loads that
World, performs actual picking and draws its selection outline.

The latest Windows MSVC Release scene passes 2,624 checks in 9.39 seconds. Its
target section contains 108 vertices: 30 platform, 30 Farmland and 48 Crops
vertices. All 48 Crops vertices match the original position/UV words at the same
coordinates and NW translation; all 48 color words also match that selected
original row. The control checks four returned World/HUD frames, including two
reuse observations and one dirty recompile. The second reuse observation keeps
the same definition and bytes. A World light notification then dirties the
section and produces a new checked definition. Bounds and ownership cleanup
pass, pre-existing save bytes remain unchanged, and the only new save file is
the source-created session lock.

These results remain separate from earlier failures. The initial original
capture exceeded its JDI deadline and is retained as a partial attempt; the
published packet comes from the later complete three-VM cohort. An earlier
loaded Crops attempt failed during advancement and recorded `msrdc.exe` holding
foreground with the ordinary source Pause screen. The later passing scene does
not replace that failure or establish unrestricted focus stability.

Adjacent fresh scene runs separately pass Ladder in 8.72 seconds and Lava in
9.60 seconds. The adjacent Water run times out waiting for SDL keyboard focus
while its receipt records native Frame foreground and Canvas focus. That
divergence does not identify the preceding input origin. The
[testing status](../testing.md) keeps those results and limits separate.

Run the focused controls with:

```text
ctest --test-dir build --output-on-failure -R "^(client[.]crops_world|tools[.]crops_world_packet)$"
```

The component and loaded controls qualify the stated finite geometry,
compilation and ownership scenarios. Original pixels, full farming simulation,
extended play and performance remain unqualified.
