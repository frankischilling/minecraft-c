# Source lighting and normals

The canonical Display renderer now computes legacy lighting once per vertex
in its OpenGL 3.3 program. It consumes the same modelview, lights and materials
that [standard lighting](standard-lighting.md) updates. The World frame still
stops at Entity/TileEntity rendering510; this backend work does not complete a
gameplay frame.

## Materials and shading

Front and back retain separate ambient, diffuse, specular, emission and
shininess values. ColorMaterial updates the selected properties through the
measured driver's commit order. NVIDIA updates eagerly; Mesa stages Color,
Normal and Material values until the query, state or draw boundaries described
below. Direct Material writes leave tracked properties alone. A direct
color-array draw makes the corresponding current color and tracked properties
indeterminate; a later draw must supply actual array values or restore those
properties.
On the measured NVIDIA profile, compiled array commands retain their last
converted normal and color after replay, including tracked material properties.
Mesa retains the preceding current attributes and explicit list Color instead.
Recorded commands do not execute during GL_COMPILE. NewList and EndList retain
their own commit boundaries.

The vertex stage transforms normals with the inverse transpose of modelview.
RESCALE_NORMAL and NORMALIZE remain separate controls, applied in that order.
Lighting includes the two supported lights, global ambient, material emission,
directional and positional positions, local viewer, two-sided materials and
single or separate specular color. Position uploads retain their upload-time
eye-space coordinates. Unsupported light indices, spotlight/attenuation setters
and projective or singular normal transforms still fail explicitly.
Used nonfinite inputs and conservative binary32 overflow bounds fail before
GPU construction or submission. These admissions change no source attributes.
Near-cancellation and driver arithmetic precision retain their finite device
qualification limits.

Both flat and smooth colors are computed before rasterization. Flat triangles
and fans use the actual last provoking vertex. Texture modulation, separate
specular addition, alpha testing and fog follow in the fragment stage.
Unlit varying-color geometry uses the same flat/smooth selection.

## Tessellator normal method

The complete `nw.b(FFF)V` normal setter preserves the original conversions.
It multiplies X by 128 and Y/Z by 127, applies Java float-to-int and byte
narrowing, then combines sign-extended bytes in source order. X's scale and
the sign extension are intentional. The existing quad conversion leaves the
duplicate vertices' normal words untouched; their prior array contents survive.

Outside an open batch, the method reads the actual associated System.out at7
and calls its captured PrintStream at12 before storing `n=true` at17. A missing
native System association remains a dependency, and an actual null out field
fails at12. The normal-started flag remains set when later FP admission fails.
Native bootstrap borrows the same System namespace already owned by Minecraft,
holds it independently, and removes the borrowed binding before releasing it.

## Captured reference profiles

Separate original NVIDIA and Mesa captures each contain 58 profiles, 59 complete
RGBA8 frames and 138 pixel, state and auxiliary rows. Replay requires every
frame byte and state word to match the corpus for the actual native driver.
The native Windows selection passes 19 CTests in 47.10 seconds; Linux Clang18
with ASan/UBSan passes 18 in 34.23 seconds. Neither selection has CTest failures
or skips. The [hosted compiler matrix](https://github.com/frankischilling/minecraft-c/actions/workflows/ci.yml)
records results for each exact revision.

Direct-array GPU controls compare 2,304 normal components across 256 byte values
and three vertices. Compiled-list controls compare 256 submitted normal values
and six separate current-attribute/material rows. Color controls compare 512
direct/list rows containing 6,144 submitted RGBA components. Repeated list draws
also check that changes to the original array storage do not change the recorded
attributes. Expected words come from actual transform-feedback observations.

The [raster controls](../../tests/reference/client/lighting_raster/README.md)
compare full 64 by 64 RGBA8 frames and state separately. Material tracking has
16 queried rows per driver. Nine fresh prefix contexts and a separate complete
draw add 12 rows, including a material query after the draw. The
[NW controls](../../tests/reference/client/nw_normals/README.md)
compare 30 original source-state and vertex rows, including nonfinite Java
conversions and stale duplicate words. These finite observations use unchanged
original classes where stated; authored GL scenes have their own scope.

On the NVIDIA GeForce RTX 3060, driver 581.42, an exhaustive probe records all
256 signed-byte array conversions through `gl_Normal` transform feedback, with
actual GL_BYTE, stride 32 and client-memory
receipts. Every observed array component equals the float conversion of a
replicated byte interpreted as signed 16-bit, divided by 32767. Immediate glNormal3b
has a different conversion. The native array decoder follows this measured
reference profile; it does not claim that every legacy driver uses that mapping.

A separate exhaustive NVIDIA list probe records all 256 values under GL_COMPILE
and after two different storage/current-normal poisons. Its compiled normals equal
the immediate conversion, using a binary32 reciprocal multiplication. They
differ from NVIDIA direct-array conversion for all 256 byte values. The native
test observes the submitted list attributes in the GPU buffer, separately from
the driver's current attributes, while using the current modelview and lighting.
Six additional state receipts record the actual current normal, color and
tracked materials before compilation and after replay and repetition.
The direct-array native control queries the real bound VBO and attribute layout,
then reads back all three submitted normal vectors. Its expected bits come
from the complete original driver observations rather than a second decoder.

## Driver precision

The measured profiles are NVIDIA 581.42 on an RTX 3060 and Mesa 25.2.8 on
llvmpipe, LLVM 20.1.2. The manifest records the legacy compatibility context and
native core context identities separately: NVIDIA reports versions 4.6.0 and
3.3.0 respectively; Mesa reports 4.5 Compatibility and 4.5 Core. The native
renderer selects attribute behavior from the actual context's complete vendor,
renderer and version identity. Other contexts report unsupported driver-dependent
array or material operations until their behavior has been measured. The shader
uses GLSL 330 and Display requests at least OpenGL 3.3. The measured Mesa 4.5 path
also uses indexed core-state queries beyond GL 3.3, after checking that their
function addresses are available. C owns
the source state; captured JSON files are test inputs.

Direct normals on Mesa multiply the signed byte by a binary32 reciprocal of
127 and clamp the lower endpoint to -1. Its direct-array words differ from
NVIDIA's for 255 of 256 values; byte zero agrees. Both drivers' compiled normal
words agree for all 256 values. Mesa's direct and compiled words differ for
254 values; the -128 and +127 endpoints agree. Unsigned-byte
colors also have two paths: NVIDIA divides direct inputs by 255 and multiplies
compiled inputs by the binary32 reciprocal. Mesa reverses those choices. All
256 values were observed independently, and 126 distinguish the rounding.
Native tests read the actual submitted VBO: all three normal vectors and all
three RGBA vectors must match their captured transform-feedback words.

A separate authored diagnostic runs the same 6,486 draws and 19,530 samples on
the NVIDIA device and Mesa llvmpipe 25.2.8, LLVM 20.1.2. It uses no game classes
or assets. Both complete captures preserve their input and helper hashes. The
[selected observations](../../tests/reference/client/lighting_raster/driver-precision-observations.json)
retain exact input/output bits and the complete capture hashes.

The diagnostic separates framebuffer clears, uniform fragment colors, fixed
lighting, flat/smooth varyings and byte/float color arrays. It reads both bytes
and floats from actual RGBA8 and RGBA32F targets. A simultaneous mixed-target
draw writes the same shader value to both formats. For exact 0.5, NVIDIA stores
127 in RGBA8 while Mesa stores 128. Both floating targets retain `0x3f000000`.
For `0x3f131f43`, their RGBA8 values are 146 and 147 respectively, while both
floating targets preserve that word. Clear and uniform-fragment controls show
the same differences, so smooth interpolation and lighting are not required
to produce them. The observations locate this difference in normalized output
conversion; they do not identify the driver's internal export/storage stage.

The transformed byte-normal controls with NORMALIZE enabled also produce
identical floating RGB words, `0x3f131f43`, on both legacy devices. This closes
the normalization question for those two measured controls. Other byte-color
and smooth-gradient samples differ by up to two float ULPs across drivers,
despite matching sampled byte reads. These finite results do not establish a
universal conversion formula or prove that every frame difference has one
cause. Renderer arithmetic and pixel comparisons retain their exact rules.

## Material commit frontiers

Mesa stages float Color, Normal and Material attributes separately from their
committed values. A current-attribute query commits staged values while keeping
the staged slots. A material query first flushes stored attributes, then current
attributes. Selector changes and tracking enable/disable have their own ordered
frontiers. These differences are observable when several ColorMaterial changes
occur without intervening queries. NVIDIA uses the measured eager updates.

Nine independent prefix captures query only at the end of each sequence. A
separate capture executes the unchanged complete draw before querying its
materials. Querying after every setter would change the Mesa sequence being
measured. Native controls therefore call explicit source getters in the same
order as the original queries. Passive inspection remains a copy of the
committed ledger. Getter tests also preserve unused output-buffer words.

The transition rules were checked against the
[Mesa 25.2.8 source release](https://docs.mesa3d.org/relnotes/25.2.8.html) and
the [matching Ubuntu source descriptor](https://archive.ubuntu.com/ubuntu/pool/main/m/mesa/mesa_25.2.8-0ubuntu0.24.04.4.dsc),
then compared with 26 captured material snapshots per driver: 16 queried
controls, nine independent prefixes and one query after the complete draw. The
draw also retains its full pixel and source-state rows. Draw admission uses a
local predicted state; a rejected numerical draw leaves the live staged and
committed state intact. Accepted draws publish the attribute changes before
GPU submission. Single `glCallList` commits current attributes at entry, while
bulk `glCallLists` retains its separate source ordering.

Mesa PopMatrix still has a conservative metadata guard. With pending STORED
attributes, a matrix changed since its matching Push and numerically equal to
the saved matrix is refused before mutation: those elements alone do not
establish the original inverse/type metadata. A matrix unchanged since Push
skips the flush; different numerical matrices flush stored attributes. This
guard can reject real no-ops and does not establish a mismatch for every refused
call.

Source texture binds flush only when the bound object changes. Owned image
uploads retain Mesa's flush before native image validation; supported texture
parameters flush only for valid changes. With stored attributes pending,
subimage updates currently require a live owned level-zero RGBA image and a
proven in-bounds rectangle. Other image/update paths and changed optional
extension wrap modes fail explicitly. Backend texture binds, shader setup and
diagnostic pixel reads do not acquire extra source commit boundaries.

The initial two profiles named `converted_quad_*` called NW's float color
overload and emitted no vertices. Their original clear frames remain preserved.
Separate double-coordinate captures require six emitted vertices, record all 48
source words and exercise actual flat/smooth quad rendering. Neither group
establishes model dispatch, Entities, particles, HUD or complete World frames.
