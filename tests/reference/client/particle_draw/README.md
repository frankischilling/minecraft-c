# Original particle draw prefixes

This optional observer calls unchanged original DN methods on genuine objects.
It requires the supplied Beta client JAR, matching JDK17 tools and four pinned
LWJGL/JInput/JUtils SDK archives. It creates no Display, native graphics context,
World, asset substitute or network connection. No native comparison is provided.

Three audited fresh JVM profiles supply the source evidence. Their raw output,
constructor entropy, private fields and local paths stay outside this package.
The Java helper is byte-identical to the reviewed observer. `source-pins.json`
contains original game class/method metadata and authored input hashes, not
compiled helper metadata. `run.py` records freshly compiled helper hashes and
adapts the reviewed controller to this location. Each new run must verify its
current manifest, helpers and actual inputs.

| Profile | Genuine setup and observed draw scope |
| --- | --- |
| empty | `new dn(null,null)`. Ten `b(null,F)` calls return16; ten `a(null,F)` calls fail at camera read1 despite empty Lists. Source Entity, Math helper, Math Random holder, NW and Ge remain uninitialized. |
| nulltarget | Real `ju(null)` positioned by `SN.e(DDD)`, real `em(null,picked,null,.375f)`, actual DN add. Ten `b(null,F)` calls reach EM's null-target read53 through DN69. |
| nullworld | A second real positioned `ju(null)` is the EM target. Ten `b(null,F)` calls reach EM's null-World brightness invocation220 through DN69. |

Every profile uses the exact partial bits `00000000`, `80000000`, `3e800000`,
`3f000000`, `3f800000`, `bf800000`, `7f800000`, `ff800000`, `7fc12345`,
`7f812345`. Integers are converted directly with `Float.intBitsToFloat`.
There is no Unsafe, constructor substitution, original field/phase/count/static
write, seed input or successful callback supplying a result. Reflection only
reads actual returned state. The source position setter receives declared
primitive operands and updates the same real Entity/bounds.

Nonempty DN.b reaches the actual default NW singleton. Original NW sets its VBO
control false before its private CPU constructor, so the reached constructor
allocates its real arrays/buffers and skips the GLContext/ARB calls. The helper
checks the actual CPU flags, complete word-array fold and buffer cursors. It
does not install NW, change a flag or supply a graphics-context substitute.
Cleanup calls actual DN.a((fd)null), observes all four real List.clear calls,
then verifies an actual empty DN.b return. NW statics live to JVM exit; no
source class destructor, renderer geometry or GPU cleanup is claimed.

`finite.json` is an authored projection of the audited captures. It contains
40 selected draw outcomes, source-PC mappings, passivity labels and capture
hashes. The raw evidence has 124 logical rows and 30 complete exception stacks;
those are not 124 native comparison rows. Java stack lines are preserved only
in newly generated private captures. PCs come separately from pinned complete
original Code; they are not BCIs measured by a Java stack trace.

The potential comparison ledger is narrow:

| Selected source projection | Outcomes |
| --- | ---: |
| Empty DN.b normal return16 |10 |
| Empty/null-camera DN.a null reference1 |10 |
| DN.b69 to EM null-target53 |10 |
| DN.b69 to EM null-World220 |10 |

Each process also proves its own DN/List/Random passivity. Populated profiles
prove their own JU/EM state remains unchanged across each failed draw. Future
native tests must use genuine native constructors and source APIs and derive
their own passivity flags. Absolute Java/C constructor entropy, actor fields,
private backing words, capacities, Java ArrayList.modCount, method locals,
pixels, normal successful rendering and full constructor-peer equality have
no comparison credit here. The three source World-clear operations and three
post-clear DN.b controls are cleanup evidence, not extra outcomes in the 40.
Current native comparison credit is zero.

To reproduce selected original profiles from the repository root, use actual
local JDK/library locations and a new output directory:

```powershell
$manifest = (Get-FileHash tests/reference/client/particle_draw/source-pins.json -Algorithm SHA256).Hash.ToLowerInvariant()
python -B tests/reference/client/particle_draw/run.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --jdk "$env:JAVA_HOME" `
  --profiles empty nulltarget nullworld `
  --manifest-sha256 $manifest `
  --output ORIGINAL_CAPTURE_OUTPUT
```

The explicit manifest hash binds the reviewed checkout's helper/controller and
README bytes. The controller verifies complete original classes and all method
Code attributes, SDK archives, matching JDK17 tools, full JDK backing files and
generated helper classes before/after. Its JMOD class/Code metadata is labelled
as archive metadata; runtime lib/modules is separately hashed. It denies all
native links, networking, filesystem writes/deletes and subprocesses after the
guard is installed. Only four declared JDK guard warnings are accepted in
complete stderr. Each fresh child has a 60-second timeout, bounded complete
process receipts and an unrotated initialization log. The controller authorizes
the VM's fixed class-init log output at launch, before the Java guard is
installed; those VM writes are separate from application write checks.
`checkPermission` permits arbitrary reads and other permissions outside the
explicit overridden checks. This guard is not an OS sandbox or a read allowlist.
Failure artifacts remain
available; an incomplete run cannot produce a passing original observation.

The original archive authority is SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The public replay needs that private original input only when this optional
observer is explicitly run; it adds no CMake test or runtime JAR dependency.
