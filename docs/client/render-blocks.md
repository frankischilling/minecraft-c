# RenderBlocks source geometry

The actual ChunkRenderer compiler now calls RenderBlocks through
`dk.a()V:464`. Standard blocks execute the `cv.b -> cv.l` path, sample the
same ChunkCache, and append geometry to the real NW Tessellator. A native
one-stone section completes its display list with 36 expanded vertices, then
the N caller clears that DK's dirty flag. The loaded authored saved World now
reaches `px.a(FJ)V:486`, the terrain-submission call.

Terrain submission and the rest of the frame remain unfinished. These changes
do not establish a complete rendered frame or matched original pixels.

## Dispatcher and standard blocks

`cv.b(Luu;III)Z` reads the Block's virtual render type before updating that
same Block's mutable bounds. Type 0 enters `cv.l(Luu;III)Z`. The other 17
recognized render families retain an explicit missing-child failure at their
source call; they do not return fabricated geometry. Unknown render types
follow the original ordinary false return.

The standard path obtains the Block color, converts its RGB channels with
binary32 arithmetic, reads the real PX anaglyph static, and calls the actual
Minecraft AO query. Its source order is:

| Method | Source PC | Reached operation |
| --- | ---: | --- |
| `dk.a()V` | 464 | Call the actual CV dispatcher |
| `cv.b(Luu;III)Z` | 1, 15, 29 | Read render type, update shared bounds, enter standard rendering |
| `cv.l(Luu;III)Z` | 9 | Invoke the Block color virtual |
| `cv.l(Luu;III)Z` | 56 | Read the defining PX class's anaglyph field |
| `cv.l(Luu;III)Z` | 128 | Call `Minecraft.v()Z` |
| `cv.l(Luu;III)Z` | 146 / 162 | Enter AO / flat shading |
| Six face methods | 0, 39 | Capture the NW singleton, then read the first Block bound |

`Minecraft.v()Z` reads the static App in its actual defining namespace. An
absent App returns false; a published App with null Settings throws at the
source dereference. It does not reuse an AO value captured by a renderer or
an earlier App. Class initialization, publication and retirement use the same
application namespace as the loaded client.

The color virtuals include the ordinary white results, redstone intensity,
grass, foliage and tall grass. Climate colors use EW's actual manager and the
original cached sampler. Each source manager read remains separate. IA/JH
colorizer initialization and their live arrays retain their own effects and
failures, including null or empty arrays after earlier successful sampling.
Metadata-selected foliage constants still perform the reached class active
use even when they bypass climate sampling.

## AO, flat shading and faces

The AO body preserves its 27 light scratch fields, 12 neighbor flags and 12
corner RGB fields on the same CV object. It follows the original side tests,
neighbor queries, averaging and grass overlays. It sets `m=true` on entry and
clears it on normal return. A failed query retains the reached scratch stores
and `m=true`; it does not perform the normal exit store.

The flat body sets `m=false`, computes the source directional colors, and
processes bottom, top, north, south, west and east in order. It uses the
current Block bounds and Material for partial-face brightness decisions.
Grass overlays use the real shared CV class flag and the reached texture
override tests. A fully culled call returns false without appending vertices;
a source failure is a separate result.

All six `cv.a` through `cv.f` face methods with descriptor `(Luu;DDDI)V` are
implemented. They read live Block bounds, each face's UV rotation and the
texture override in source order. The four horizontal faces apply the source
flip flag. UV arithmetic preserves the
original binary64 operations and the binary32 fallback for bounds outside
the unit cube. Each visible face sends four real UV vertices to the same NW
singleton. NW's source quad expansion stores six vertices, or 48 integer
words, per face.

AO faces interleave four corner-color calls with those vertices. The shared
`nw.a(FFF)V` leaf performs three binary32 multiplications by 255 and Java
float-to-int conversions before entering the integer color body's clamp and
color-lock logic. Color packing, partial array writes and quad copies remain
source effects in NW's actual backing storage.

## Ownership and failed prefixes

The entered CV context borrows the compiler's actual CV object, leased EW
cache, Block registry, application namespace and graphics/model class owners.
It neither starts a second Tessellator nor builds a detached inspection mesh.
EW queries use the captured Chunk identities and their Store leases. Bounds,
scratch colors and shared class fields are the canonical mutable source
storage.

A failure retains its CV/Block/NW source frames under the DK compile at 464
and the N call at 442 or 519. Earlier vertices, List mutations, open list and
matrix prefixes remain visible. Native retirement releases admitted owners
without replaying source work or inventing successful end-list operations.
The N caller clears `DK.u` only after that same compiler returns normally.

TileEntity renderer eligibility at `dk.a()V:399` and the chest texture
adjacency helper remain missing children. Together with the 17 specialized
geometry families, these limits prevent a claim that all Block rendering or
all nonempty sections are complete.

## Focused evidence

The finite references pin the original client JAR and selected method Code.
Their native runners compare the declared fields and vertex words exactly.

| Reference | Rows | Scope |
| --- | ---: | --- |
| [Faces and float color](../../tests/reference/client/render_blocks_faces/README.md) | 1,680 | Six faces, UV states, bounds, AO colors, IEEE color inputs and short-array failure prefixes |
| [Flat shading](../../tests/reference/client/render_blocks_flat/README.md) | 25 | Whole flat calls, visibility, brightness, overlays and retained failures |
| [Block colors and AO query](../../tests/reference/client/render_block_colors/README.md) | 25 + 7 | Color virtuals and actual static Minecraft query |
| [Ambient occlusion](../../tests/reference/client/render_blocks_ao/README.md) | 48 | Whole AO calls and retained fields, including four complete dispatcher/standard entries with AO and anaglyph profiles |
| [Chunk compilation](../../tests/reference/client/chunk_compile/README.md) | 1 original row | Empty whole DK call; separate native controls cover real nonempty list completion and a remaining-family failure |

All six focused MSVC CTests pass, including all the finite rows above and
the native DK controls. The loaded UI passes 702 checks, preserves resources and
saves, and retains all 72 focus histories. The new code has not yet completed
a full ALL build or hosted CI qualification. Historical suite totals qualify their
earlier checkpoints. See [ChunkRenderer compilation](chunk-compilation.md)
for the selector and cache ownership that this geometry path uses.
