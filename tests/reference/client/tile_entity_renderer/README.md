# TileEntityRenderer (Beta) original reference

`finite.json` contains 254 observed rows from four fresh JDK17 processes using
the unchanged original client. The input, observer, generated-class and
initialization-log hashes are recorded in `provenance.json`. Native replay
matches all 254 rows on Windows MSVC Debug and Linux Clang18 ASan/UBSan. It
projects only the original null-Class Java stack line to its independently
pinned source PC26; the raw fixture and its hash remain unchanged.

The authority is the unchanged Beta client archive with SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
`source-pins.json` records complete class identities and selected original
method Code/Code-attribute hashes. No game class or constructor is copied,
replaced, transformed, instrumented, or fabricated.

Four mandatory fresh JVM profiles observe the complete `ll.a` singleton
constructor; all eight registered TileEntity subclass Class lookups plus OW
in forward and reverse registration order, with repeated queries; and the
null TileEntity and null Class paths. The Sign renderer's RF model has its
two real PS parts observed through raw state, position, UV and quad-alias
words. AG's actual Map and HY's initial field are read directly. JE
backpointers are compared to the same published LL instance.

Class operands are acquired with `Class.forName(name, false, loader)`.
Original `ow.<clinit>` bytecode supplies passive registration-literal
authority; the observer never reads OW or Piston statics or constructs a
TileEntity. A complete bounded JVM initialization log verifies that these
classes remain uninitialized. The null Class failure is observed as its
actual exception and top original frame; PC26 is a separate pinned bytecode
mapping, not a BCI read from a Java stack trace.

Logical Map entries are sorted by Class name for finite rows. Actual Java
Map iteration, table bucket, node hash and chain order are captured separately
and preserved without sorting in stdout and the capture report. These
physical orders depend on Class identity hashes and provide no deterministic
replay-order qualification. Geometry and failure rows are not normalized.

Required inputs are matching JDK17 Java/Javac, the pinned original JAR, and
the four pinned LWJGL2.9.0/JInput/JUtils SDK archives under a Minecraft library
root. SDK archives supply runtime type resolution only. No native archive,
native library, asset pack, display, or World is required. The Java guard
rejects networking, native links, filesystem writes/deletes and subprocesses.
The capture parent alone writes class files, bounded process receipts,
initialization logs and an observed `finite.json` into a new output directory.

Reproduce from the repository root:

```powershell
python tests/reference/client/tile_entity_renderer/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --java 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin/java.exe' `
  --javac 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin/javac.exe' `
  --output .local/tile-entity-renderer-original-20261030a
```

Each profile has a 60-second timeout and bounded stdout/stderr capture. All
profiles, before/after hashes, helper class inventory, initialization logs and
zero external-work completion markers must pass before a fixture is emitted.
No World binding, frame binding, renderer draw, asset lookup, GL operation,
instance eligibility or whole-frame completion is qualified by this probe.
