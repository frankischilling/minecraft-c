# Source RenderGlobal sky draw

The production entry implements actual client `n.a(F)V`, separately from the
constructor's star/upper/lower display-list compilation. It consumes those
same compiled source programs and canonical World, Dimension, Settings,
TextureEngine and Tessellator owners. Retained calls preserve source/GL
prefixes and complete typed errors. Cleanup never issues source PopMatrix,
Draw, GL-state restoration or a replay of failed work.

The CPU fixture checks four early original outcomes: null App, null App World,
null Dimension and the true Nether flag returning before any GL or NW work.
Its native controls additionally verify cold Math/NW classes, unsupported-FP
admission on the arithmetic-free return, retained-call exclusion, strict owner
retirement and balanced native holds. Source PCs4/7/10/16 are static bytecode
controls; the original numeric rows record only actual return/throw outcomes.
The original receiver slots are explicitly prepared, so these rows do not
qualify App, World, N or biome construction.

The device observer executes the genuine full N constructor and verifies that
its actual star, upper and lower lists exist. It then invokes the unchanged
whole sky draw for 48 cases: Overworld/Nether/Sky, four day times, anaglyph on/off
and rain0/1. WorldInfo and Dimensions/managers/noise are actually constructed;
App, Settings, World, camera and Engine are prepared real classes. The Engine's
actual Map contains actual GL texture names and its real PackList slot is
present, so both sky texture calls execute the genuine cached `ji.b` path.
No method, texture return, list geometry, sky color, weather, MathHelper or
sunrise array is replaced. Engine/App/World constructors and cache-miss upload
are outside this original fixture's scope.

`draw.json` contains actual post-call current color, fog/alpha/blend/texture/
depth-write/shade state and Vec3 pool cursor. Two fresh Windows LWJGL2 runs
produced identical rows. The profile is NVIDIA RTX3060, OpenGL4.6, driver581.42.
The shader bridge compares these state/value observations exactly; this is
not a screenshot or cross-driver pixel equivalence claim.

The native device fixture uses the genuine whole-App/startup graph and its
constructor-compiled N programs. Its World/Info/Dimension/camera slots are
explicit direct operands, without claiming World load or N World attachment.
It primes actual texture lookups before observed cache-hit calls, uses passive
canonical draw/fog state plus real GL blend/depth queries, and independently
reads the actual last lower-list GPU vertex/color record. The lower record
must have y=-16 and the same uploaded RGBA as the source color state. The
normal method must complete every source list/texture/geometry stage and
restore depth writes; Nether must return before drawing.

Original capture requires locally supplied pinned archives and matching JDK17:

```text
python -B tests/reference/client/render_global_sky/capture_prefix.py \
  --client-jar /private/b1.7.3.jar --library-root /private/libraries \
  --output /private/new-sky-prefix
python -B tests/reference/client/render_global_sky/capture_draw.py \
  --client-jar /private/b1.7.3.jar --library-root /private/libraries \
  --output /private/new-sky-draw
```

The source pins contain selected Code/class hashes; runners verify the
original JAR, JDK, SDK and helper inputs before/after bounded subprocesses.
The CPU observer denies writes, network and native links. The GL observer
allows only the pinned LWJGL DLL and explicit JDK AWT links, with writes and
network denied. No original class, asset or decompiled listing is distributed.

Native CPU replay accepts `--executable` and `--work-dir`. Device replay also
requires the existing private `--resources` and `--assets` fixture inputs. It
copies assets into a fresh work tree, adds authored white1x1 sun/moon PNGs,
isolates APPDATA, requires normal completion/empty stderr and exact original
rows, and fingerprints all read-only inputs after execution.
