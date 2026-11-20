# Selected LocalPlayer camera operations

This source-only harness executes unchanged original methods on real `kq`,
`fd`, `dc`, `ji`, `n`, `px`, `ra` and `iy` owners inside a real LWJGL Pbuffer.
It compares selected native first-person frames with the original driver's
projection/modelview results. It does not establish visual equality, a complete
native EntityRenderer, its constructor, render tick, fog or scene scheduler.

Run the integrated native target first with authored resources:

```
python -B tests/tools/client/test_first_person.py --executable <mc_test_first_person> --work-dir <private-directory>
python -B tests/tools/client/test_first_person_driver.py --executable <mc_test_first_person> --work-dir <another-private-directory>
```

The original context profile is Windows x64 with the exact supplied client,
four pinned LWJGL/JInput utility JARs, pinned `lwjgl64.dll`, a desktop OpenGL
compatibility context and explicit matching JDK17 executables:

```
python -B tests/reference/client/camera/compare.py --client-jar <b1.7.3.jar> --native <mc_test_first_person> --callbacks <mc_test_first_person_callbacks> --java <java.exe> --javac <javac.exe> --libraries <private-jar-directory> --native-dir <private-dll-directory> --work-dir <private-output-directory>
```

Every run uses a fresh helper directory containing only the two authored
observer classes. The original JAR/libraries remain unchanged; no substitute
game class, Unsafe allocation, constructor bypass, supplied entropy, injected
camera clock or replacement World is used. Three private language/statistics
resources are extracted from the pinned original JAR for the native comparison.
No assets, class files, decompiled listings or captured results are distributed
with this harness. Original console banners remain in their actual order.
Ambient JVM options are rejected and CLASSPATH is cleared. Process capture uses
the existing reviewed statistics helper, requires explicit empty reader errors,
bounded output/time, reaping, exit zero and empty stderr. Negative protocol
controls reject crashes and sanitizer diagnostics; they cannot count as a
successful native rejection. Python `-O` preserves all checks.

`source-pins.json` records whole archives, 31 whole game classes and each Code
body, plus the three private resource hashes. The observer independently reads
those actual runtime classes' bytes and verifies their JAR code-source origins.
Class-initialization logging remains private. `provenance.json` also binds the
authored helpers and selected complete native source/header cohort.

The native fixture constructs real Registry, Session, World and LocalPlayer
owners. Source field controls set finite positions, rotations, bob/hurt/death,
portal, sleep and settings cells. Later cases acquire a genuine generated Chunk
and edit its real voxel/nibble arrays for water and bed controls. This is an
authored field fixture, not bed entry, fluid placement or a gameplay tick. Its
native cleanup supplies an actual pending collection entry, then runs the real
maintenance/detach/save path and retires the stored Chunk. The base provider's
dedicated-only mark-drop API is not substituted.

The 29 cases cover ordinary/bob/hurt, anaglyph passes, eye offset, death,
first-person debug, sleeping, bed metadata0/3, masked Java shifts, partial ticks
0/1/1.5, GLU zero-width early return, infinite aspect, NaN portal branch order,
source null settings/view entity/Entity World/application World, and native FP admission.
The actual nullable application World is supplied separately from Player.world;
sleeping null-App.f stops at g159 after its translation. Neither the CPU API
nor the finite view infers that field from its retained geometry owner. Positive
portal stops at the first actual `px.l` read `a292`; third-person stops at the
first unrepresented `px.u` read `g292`. Original executions of those branches
are retained separately, and are not called matching native completions.

Completed cases compare exact original FOV bits and a separately labelled
authored scalar projection of actual origin fields. Actual GL matrices are
compared with a declared absolute coefficient tolerance of `2e-6`; exact bits
and maximum observed difference are reported. This is an explicit native float
matrix policy, not a bit-exact driver claim. The source arithmetic, ordered GL
operations and underlying source instructions remain separately inspectable.
Geometry uses raw interpolated Entity position; the `bf-1.62f` translation stays
in modelview. The fog-eye sample subtracts that offset separately. Combining
both into one eye position would apply the height twice and alter narrowing.

Relevant selected bytecode is `px.a(FI)V` Code339, `d(F)F` Code76, `e(F)V`
Code113, `f(F)V` Code170 and `g(F)V` Code872. Full class/Code SHA256 values are
machine recorded. The zero constructor profile cells E1/F0/G0/z0/A0/B0/C0
have no other source value writers besides their previous-value copies. No
normal nonzero portal clock is inferred from an application frame count.

The optional native GPU target uses the genuine Player's frame on the real GL3
renderer and authored geometry: yaw changes move the wall away from the center
pixel; incomplete, changed-dimension and nonfinite frames are rejected. It
retains the existing inspection renderer test. This is a native rendering
contract, not an original screenshot comparison. The finite World view remains
fixed geometry and does not implement live mesh updates or chunk streaming.

## Remaining constructor/resource closure

`px.<init>(Minecraft)V` Code217 constructs six `cu`, creates a genuine default
Random at169, allocates a FloatBuffer16 at193, stores the actual application201
and invokes `ra.<init>(Minecraft)V` at210. `ra` Code64 creates `cv` at24 and
invokes `iy.<init>(sj,kv,ji)V` at57. `iy` Code71 allocates int16384 at8,
stores actual settings/font identities, creates a transparent BufferedImage
128x128 at36 and calls the actual `ji.a(BufferedImage)I` at39, then writes its
int array's zero prefix. A nullable font is stored unread by this constructor.

The observer also verifies these genuine map owner/array/reference identities,
retained image dimensions/zero pixels, IntBuffer/ByteBuffer position-limit
prefix and actual device texture's65536 zero bytes. It reports the actual
fallback Graphics default font without substituting a bitmap font.

`ji.<init>(ik,kv)V` Code158 constructs four distinct maps, IntBuffer1,
ByteBuffer1048576, ArrayList, false j/k, BufferedImage64x64 ARGB, and nullable
pack/settings references. It obtains actual Graphics116, sets WHITE124,
fills64x64 at134, sets BLACK141, draws default-font `missingtex` at(1,10)150,
disposes154 and returns157. The existing native Minecraft bitmap font owner is
not this AWT default-font body. Its real image/font provider remains an explicit
constructor dependency; no fixed captured image or guessed glyph rasterizer is
used to declare `ji` complete.

`ji.a(BufferedImage)I` Code45 clears e4, calls actual `ge.a(IntBuffer)`12
(generates GL IDs and appends the actual global ge.b ledger), reads e[0]20,
uploads at27, then publishes the same original image in map d39. Upload Code705
binds the ID4 before the null image width read154, allocates int[w*h]168 and
byte[w*h*4]178, converts ARGB to RGBA with optional live settings anaglyph,
mutates the shared transfer buffer clear401/put411/position420/limit426,
then issues actual glTexImage2D451. The normal static a=false skips mipmaps.
Source GL calls do not perform native error checking; native failure handling
and any retained ID/image prefix must therefore be reported separately.

The current native Display upload API combines allocation/upload and deletes a
failed ID. Finishing this closure needs a narrow real Display resource seam:
allocate/adopt a texture ID into a retained native ledger first, upload into an
existing ID, and release that resource explicitly. A real `ji` owner must retain
its maps, buffers, original image identities and nullable pack/settings fields,
publish d only after upload returns, and keep failed allocated resources until
native retirement. No such unfinished operation is exposed as successful here.


## Live owner and callback revision

`mc_client_first_person_camera_for_application` and
`mc_client_world_view_draw_application` borrow the full genuine production
`client_application`. They reload its actual Settings at a8/35/176/208,
g122/282/728 and its nullable World at g135/179. Settings values are read at the
reached kv.e11/g38/g179/f211/F125/A285/F731 sites. Actual selected h/i LocalPlayer
identity is still an explicit caller argument: these entries do not construct a
native px or claim the complete Minecraft render entry. Geometry keeps its own
retained World. It never substitutes that owner for App.f.

The authored callback target is independently runnable with no JAR:

```
python -B tests/tools/client/test_first_person_callbacks.py --executable <mc_test_first_person_callbacks> --work-dir <private-directory>
```

Its sixteen cases load a serialized genuine Item through the actual region
loader, Entity factory and Chunk activation. The resulting actual WorldAccess
entity_added hook mutates the same Settings, replaces the actual App settings,
changes App.f between block159 and metadata203, sets late nullable settings or
Worlds, and controls the native FP environment. The App fixture uses the complete
production storage and actual counted World adoption helper. It never supplies a
World-getter or fake camera callback. Two cases also change the actual Entity
World field with balanced native holds, proving block49 keeps its original
receiver while metadata82 reloads aI73. This fixture-only assignment is not a new
public Entity transfer operation.

The unchanged original methods run seven paired callback cases on real kq/fd/dc
owners with genuine mx/ok region storage, saved hl/iz Item records and authored
pm observers. All seven outcomes and ten GL prefix matrices (including the two
source null failures) are checked. Java source controls do not change MXCSR.
Native FP rejection is separately labelled native policy: d35/62, g185/219/238,
and material85 preserve prior World/GL-operation prefixes and leave unsupported
arithmetic unexecuted. The lower helper itself checks after metadata82, before
rp.d85; a later camera check cannot undo lower arithmetic. Each actual World
receiver is retained through callbacks. Two native counter-limit controls check
that failed temporary holds preserve the sleeping g115 and projection a55 prefixes. Errors and out values retain their
previous source/native contract.

The original c51 camera body and old material body are retained privately as red
controls: the same-Settings callback must fail the old snapshot body's model196
assertion, and the lower output-preservation/FP85 case must fail the old material
body. The shipped tests require real completions, not successful rejections or
sanitizer crashes. Source-owned startup, fog/clear, real ji registration/upload,
full native px allocation and complete application h/i publication remain later
work. Production first-person draw still requires an explicit caller fog value.
