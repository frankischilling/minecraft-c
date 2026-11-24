# Source EntityRenderer camera

This fixture executes unchanged `px.a(FI)V`, `d(F)F`, `e(F)V`, `f(F)V` and
`g(F)V` in a real LWJGL2 compatibility Display. It records 36 ordered camera
rows and 36 separate field/transport rows. Two fresh original runs produced
bit-identical field rows and queried matrices under the profile recorded in
[profile.json](profile.json). No game class, asset or decompiled source is
distributed. [Source pins](source-pins.json) bind the original method bodies.

PX, App, Settings, LocalPlayer, RenderGlobal and the World transport subclass
are genuine classes allocated without their constructors, with explicit fields
prepared by the observer. WorldInfo and the Overworld Dimension/manager/noise
are genuinely constructed. The only authored game descendants are World block
and metadata transport: air or water9, metadata0. Camera, immersion, Vec3,
raytrace, MathHelper and RenderGlobal's literal-false cloud predicate remain
unchanged original methods. This qualifies the reached camera bodies with
prepared operands, rather than those bypassed constructors, actual Chunk
storage, a complete World frame or screenshot equality.

Each of 12 modes uses partial ticks0/.5/1, with the same current/previous
positions, rotations, eye offset, PX interpolation fields and dimensions as
the native fixture. Modes cover ordinary, water, death, hurt, bob, both
anaglyph eyes, E/F/G zoom, positive portal, third-person debug, ordinary
third-person and sleeping over air. Ordinary third-person returns through the
original eight-iteration ray loop and original World ray methods; separate
field rows record the observed block/metadata transport counts. Transport
counts are observations of this authored transport profile, not a replacement
ray implementation or native cache-access count comparison.

`CAMERA` rows contain actual PX.k, D and the queried GL projection/modelview
float bits. `CAMERA_FIELDS` records original FOV plus actual E/F/G binary64
bits and transport counters. The native replay requires exact k/D/FOV/E/F/G
bits and all36 ordered rows. Its extra collision-wall and null-RenderGlobal
controls must also complete normally before the runner accepts a result.

Matrix comparison uses an explicit absolute coefficient limit of `2e-6`, the
existing native float camera matrix policy. It does not claim bit-exactness
between a GL3.3 CPU matrix bridge and the original compatibility driver. Every
replay receipt retains all native/original coefficient bits, exact-match
counts, per-case worst differences and the overall maximum. All coefficients
in this corpus must be finite; NaN/Infinity or an excessive difference fails.
The original profile is NVIDIA RTX3060, OpenGL4.6, driver581.42. A repeatable
driver capture alone does not prove a different matrix backend's precision.

Capture with the locally supplied pinned client, SDK metadata/native archive
and matching JDK17 tools:

```text
python -B tests/reference/client/renderer_camera/capture_reference.py \
  --client-jar /private/b1.7.3.jar \
  --library-root /private/launcher/libraries \
  --output /private/new-renderer-camera-capture
```

The original runner creates a fresh bounded helper directory, verifies the
JAR, SDK, selected native DLL and JDK tool hashes before/after, and requires
guarded completion. The observer denies writes, networking and listeners;
only the pinned LWJGL DLL and explicitly allowed JDK AWT links are permitted.
No helper overrides a camera, ray or cloud method. Only helper classes enter
the fresh classpath ahead of unchanged original archives.

The native runner accepts `--executable` and `--work-dir`, creates disposable
authored resources/region data through the existing picking fixture, requires
exit0 and empty stderr, and verifies input/resource hashes after execution.
Both source operands and backend precision remain separately reviewable.
