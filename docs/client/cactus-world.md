# Cactus World rendering

The World renderer now dispatches type13 to the original Cactus wrapper and
geometry bodies, `cv.m(Block,III)` and `cv.c(Block,III,FFF)`. Generated Cactus
previously stopped section compilation at the missing child. Item rendering
has its [separate inventory path](inventory-geometry.md).

The wrapper obtains the actual Block World color, applies the original RGB
and anaglyph arithmetic, then enters the geometry body. Cactus supplies white;
the captured anaglyph profile reaches that branch without qualifying arbitrary
nonwhite wrapper colors. The geometry body keeps the enclosing NW batch open.
It does not begin or draw each face, reset CV.m, change bounds or clear scratch
fields. A retained AO flag continues to select the existing face methods'
actual corner colors.

The six faces retain original order, side predicates, center and neighbor
brightness reads, bound/material substitutions, texture lookups and vertices.
Lateral faces add and reverse a 0.0625 float offset through the genuine NW
additive operation. All zero operands remain positive zero. A failing child
retains reached words and offsets; reversing an offset is normal source work,
not native cleanup. Fully culled output returns false separately from failure.

## Original evidence and native replay

The [Cactus packet](../../tests/reference/client/cactus_world) contains 25 calls
from two fresh ordinary VMs and a third fresh VM under read-only JDI. Complete
stdout agrees byte-for-byte. Genuine saved Worlds, four Chunks, EW caches, CV
constructors and the original NW singleton supply all owners. Its real
2,097,152-word backing remains intact. There is one begun batch and no draw,
GL context, raw owner allocation, fake World adapter or source replacement.

Profiles cover empty, occluded and mixed neighbors; render-all; partial and
outside bounds; each bound separately NaN; signed-zero lower bounds; retained
AO state; rotations, flips and overrides; live texture changes with signed
overflow; and nullable cache, Block and Material receivers. Nineteen calls
return true, one returns false and five throw. Appended geometry ends at
word5,232. Every CV primitive field and live Block bound retains its input.

The original JDI trace records 669 EW query entries with arguments/results,
148 additive calls and 444 ordered translation stores. Ten live method Code
fingerprints match the supplied archive. These are original observations;
matching native outer rows alone does not establish a native per-query trace.

| Reached null receiver | Original failure | Earlier geometry |
| --- | --- | --- |
| Cache, direct or wrapper | `UU.d:13`, from `CV.c:120` | None |
| Block, direct body | `CV.c:120` | None |
| Material, partial top | `CV.c:268` | 48 bottom-face words |
| Block, wrapper | `CV.m:9` | None |

Unit top bounds bypass Material.d, even with a null Material. NaN top bounds
reach that dependency; lateral NaN comparisons do not substitute center light.
The native fixture preserves these failure frames and prefixes. Its exact
comparison includes all fifty input/observation rows, complete NW primitive
state and reported backing words. It separately audits every native backing
word outside appended ranges against its own prior bytes and checks FP admission
and owner cleanup. Those native policies are separate from source exceptions.

## Native section and loaded-frame controls

The DK compiler control uses actual Sand below Cactus in a two-height section.
Six Sand faces and five Cactus faces publish 66 packed vertices in pass0.
The test checks Cactus coordinates and UVs, 24 lateral inset vertices, the
empty second pass, immutable definition reuse and a real dirty/recompile cycle.
The last scanned cell is Air; the last returned CV is the Cactus dispatcher
at PC65. The original Stone control and reached Torch dependency remain checked.

The Windows `client.world_selection_cactus` scene authors saved Sand/Cactus
and a saved Player pose before bootstrap. The ordinary source NBT reader places
the Player at x3.5/z0.5 with feet at y64, zero rotation/motion and the original
float eye offset. Independent decoding checks those input bits. Actual loaded
Player and camera fields, Cactus picking at (3,65,3), and the returned selection
outline are checked; native code does not teleport the Player after loading.

The visible section publishes 60 vertices: five Sand faces over the grass floor
and five Cactus faces. Returned World/HUD frames retain the same World, Registry,
section and list ID. The fixture validates each returned frame while waiting
at most sixteen frames for reuse of the current immutable definition. Pending
startup work can replace an earlier definition. A real World saved-light setter
then notifies the registered WorldAccess, queues that section and produces a
new definition through the normal returned frame. Bounds and source-read pose
remain intact. The final capture visibly shows Cactus, Sand, selection and HUD.

The isolated scene passes 1,484 checks in 11.53 seconds, including checked native
terminal cleanup and unchanged protected save/resource files. A preceding run
records Remote Desktop holding foreground, lost SDL focus and the ordinary
Pause screen after successful World/terrain/HUD children. Its failure remains
separate; the unchanged isolated pass does not establish desktop stability.

Run the public CPU comparison and packet controls without Java or original assets:

```sh
ctest --test-dir build-windows --output-on-failure --no-tests=error -R '^(client.cactus_world|tools.cactus_world_packet)$'
```

With imported assets and `MC_ENABLE_CLIENT_DISPLAY_TESTS=ON`, the additional
Windows frame scene runs as `client.world_selection_cactus`. The hidden-display
section control runs as `client.chunk_compile`.

The public capture driver can repeat the original observations with the locally
supplied archive, pinned libraries and JDK17.0.17. Raw captures, saved Worlds,
compiled helpers and original assets remain private. Nineteen packet controls
check hashes and independent changes to query order/results, source exceptions,
geometry words, field retention, source identities and ordered translation stores.

The DK and loaded-frame controls are native integration evidence. They do not
qualify original enclosing DK execution, matched original raster pixels, audio,
unrestricted play, direct World receivers in place of EW, arbitrary malformed
backing or an FPS gain. Other specialized World render families and TileEntity
paths remain unfinished.
