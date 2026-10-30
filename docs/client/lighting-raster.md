# Source lighting and normals

The canonical Display renderer now computes legacy lighting once per vertex
in its OpenGL 3.3 program. It consumes the same modelview, lights and materials
that [standard lighting](standard-lighting.md) updates. The World frame still
stops at Entity/TileEntity rendering510; this backend work does not complete a
gameplay frame.

## Materials and shading

Front and back retain separate ambient, diffuse, specular, emission and
shininess values. ColorMaterial changes the selected properties continuously:
enabling tracking, changing its face or property, and setting Color all affect
the same material records. Disabling tracking freezes their current values.
Direct Material writes leave tracked properties alone. A color-array draw makes
the corresponding current color and tracked properties indeterminate; a later
draw must supply actual array values or restore those properties.
Compiled array commands instead retain their last converted normal and color
after replay, including the resulting tracked material properties. Compilation
alone changes none of those current attributes.

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

## Reference profile

Fifteen selected Windows MSVC Debug CTests pass in 46.78 seconds, with zero
failures or skips. Raster replay matches all 58 profiles and 59 full frames
exactly. Direct-array GPU readback matches all 2,304 normal components across
256 byte values; compiled-list replay matches 256 normal values and six
current-attribute/material rows. The source U comparison still matches all
86 rows, and strict terrain replay still requires and matches all 35 rows.
The same selection includes World-frame/list controls and the loaded-World
regression, which passes in 12.99 seconds with 72 original focus histories and
preserved save/resource bytes. Hosted qualification is pending.

The [raster controls](../../tests/reference/client/lighting_raster/README.md)
compare full 64 by 64 RGBA8 frames and state separately. Material tracking has
16 original rows. The [NW controls](../../tests/reference/client/nw_normals/README.md)
compare 30 original source-state and vertex rows, including nonfinite Java
conversions and stale duplicate words. These finite observations use unchanged
original classes where stated; authored GL scenes have their own scope.

The measured legacy device is an NVIDIA GeForce RTX 3060, driver 581.42.
An exhaustive driver probe records all 256 signed-byte array conversions through
`gl_Normal` transform feedback, with actual GL_BYTE, stride 32 and client-memory
receipts. Every observed array component equals the float conversion of a
replicated byte interpreted as signed 16-bit, divided by 32767. Immediate glNormal3b
has a different conversion. The native array decoder follows this measured
reference profile; it does not claim that every legacy driver uses that mapping.

A separate exhaustive list probe records all 256 values under GL_COMPILE and
after two different storage/current-normal poisons. Its compiled normals equal
the immediate conversion, using a binary32 reciprocal multiplication. They
differ from direct-array conversion for every byte value. Replay preserves
those captured attributes while consuming the current modelview and lighting.
Six additional state receipts record the actual current normal, color and
tracked materials before compilation and after replay and repetition.
The direct-array native control queries the real bound VBO and attribute layout,
then reads back all three submitted normal vectors. Its expected bits come
from the complete original driver observations rather than a second decoder.

The initial two profiles named `converted_quad_*` called NW's float color
overload and emitted no vertices. Their original clear frames remain preserved.
Separate double-coordinate captures require six emitted vertices, record all 48
source words and exercise actual flat/smooth quad rendering. Neither group
establishes model dispatch, Entities, particles, HUD or complete World frames.
