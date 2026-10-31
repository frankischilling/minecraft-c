# Original cloud drawing

`CloudObservation.java` invokes unchanged `n.b(F)V` from the verified Beta 1.7.3
client archive. The fancy setting reaches unchanged `n.c(F)V`; the actual Nether
flag returns before either drawing path. Real NW methods and LWJGL GL operations
execute on the original NVIDIA compatibility context.

The fixture constructs N and its original star/sky display lists, WorldInfo and
the Overworld, Nether and Sky Dimension owners with their real biome managers
and noise. App, World, Settings, camera and Engine fields are explicit prepared
operands. Engine's genuine cached `ji.b` method finds a prepared actual Map entry
containing an actual authored white 32x32 device texture. No game method or
returned source color, height, vertex, texture ID or GL result is replaced.

Before each call, the actual NW `g` array is cleared and the actual `e.position`
is set to zero. These are declared fixture inputs, not source method effects.
This supplies known initial bytes for the original quad expansion's untouched
normal words. The current color is (.125,.25,.5,.75), culling and blending are
disabled, texture and alpha testing are enabled, fog and lighting are disabled,
depth writes are enabled, the color mask is all true, and projection/modelview
matrices are identity. No initial GL error is supplied.

## Profiles

There are 72 calls. The first 54 cross Dimension 0/-1/1, fancy false/true,
camera height 64.5/108.5/120.5 and anaglyph/eye tuples (0,0),(1,0),(1,1).
The last 18 cross Dimension, fancy and those same tuples at height 108.5 with
negative/wrapped texture coordinates.

All calls use partial tick .375, N tick 17, World time 6000, weather i/j/k/l zero
and World cloud color H=0xffffff. Ordinary current X/Z are 8.5, previous aJ=7.5
and aL=6.5. Negative-coordinate current X=-2049.25 and Z=4097.75 use aJ=X-1 and
aL=Z-2. All profiles use bm=Y-1, bl=X-1 and bn=Z-2. Cloud X/Z interpolation
uses aJ/aL; cloud Y interpolation uses bm. The Vec3 pool cursor resets before
each observed source call.

The exact class and selected Code identities are in `source-pins.json`.
`operation-map.json` records reached field and invocation PCs from unchanged
original bytecode. N.b has 669 Code bytes and N.c has 2,085. Selected child height
methods are genuine `xa.d()F` and Sky's `ay.d()F`.

## Recorded values

`draw.json` contains 72 `state_rows` and 72 `geometry_rows`.

Each `CLOUD_STATE` row contains, in order:

1. Index, Dimension index (0/-1/1 encoded 0/1/2), fancy, height index, anaglyph,
   eye and coordinate sample index.
2. Four current-color binary32 hex values.
3. Cull, blend, texture, alpha, fog and depth-write flags, then shade model.
4. Four color-mask flags and the actual Vec3 pool cursor.
5. All 16 modelview binary32 hex values and actual NW `e.position` in int words.

Each `CLOUD_GEOMETRY` row contains index, selected words per vertex, final batch
vertex count and SHA-256. The hash consumes actual retained `g` words 0..5 for
each final uploaded vertex, with word 6 additionally selected in fancy mode.
Every int is encoded big endian. Reserved word 7 is unqualified. Flat normals
are unused and omitted. The original NW quad conversion yields 1,536 flat
vertices; fancy rows contain the final tile's actual 102 or 108 vertices. Nether
returns have zero vertices and the empty digest. Raw selected words remain in
the private capture stdout for diagnosis.

Fancy's modelview scale (12,1,12) remains after return. Its final color mask follows
the anaglyph eye; the flat path leaves the initial mask untouched. Both drawing
paths publish white current color, disable blending and enable culling. The
original source does not restore these effects during native fixture cleanup.

## Capture and limits

Two fresh Windows JDK17/LWJGL2 captures produced identical state and geometry
rows. The observed profile is NVIDIA RTX 3060, OpenGL 4.6, driver 581.42. Capture
verifies pinned JAR, SDK/native archive, class and method hashes; it records
tool, helper and input hashes before and after execution. It denies network,
listeners, file writes and every native library except the extracted pinned
LWJGL DLL. No JDK AWT native library is loaded by this profile.

```sh
python -B tests/reference/client/clouds/capture_reference.py \
  --client-jar /path/to/b1.7.3.jar \
  --library-root /path/to/.minecraft/libraries \
  --output .local/clouds-original-new
```

The output directory must be new. Java and original archives are needed only
for this optional observer. They are not native runtime dependencies.

These observations qualify finite post-call source/GL state and the final NW
batch payload. They do not capture every fancy batch, compare original pixels,
establish complete camera/frame equality or qualify prepared game constructors,
Engine cache misses, texture uploads or gameplay. No original asset, class or
decompiled source is distributed.
