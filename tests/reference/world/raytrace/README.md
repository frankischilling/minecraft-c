# Original World ray observations

`finite.json` contains 365 observations from the unchanged, pinned original
client JAR. The authored `RaytraceObservation.java` glue uses the existing
World/Chunk preparation helper, assigns controlled cells and singleton Block
bounds, and invokes the actual World ray body. It does not replace that body
with a recompiled method. The fixture records the original JAR hash, authored
observer hashes, JVM version and execution flags.

The cases cover IDs 1 through 96, metadata and optional movement-box gates,
fluid flags, all axis directions, tied crossings, negative boundaries, the
201-iteration limit, null receivers, NaN/infinity, state-dependent shapes and
pooled-input aliasing. Comparisons include nullable result fields, hit-vector
bits, mutated input coordinates, both pool cursors and stored singleton bounds.
These are controlled cached-World observations; unloaded-chunk transport,
generation, Player/PX construction and rendering have separate scopes.

Reproduce with a supplied JAR, built native test and new private directory:

```text
python -B tests/tools/world/observe_raytrace.py --jar /owned/b1.7.3.jar --native /build/mc_test_world_raytrace --work-dir /owned/ray-observation --java /jdk17/bin/java --javac /jdk17/bin/javac
```

The observer checks SHA-256 before use and keeps generated classes, World files
and captures in that work directory. No original assets, classfiles or Java
method bodies are distributed here.

Ordinary `world.raytrace` CTest runs use the frozen fixture and authored temporary
native GAME storage. They need neither Java nor the original JAR. The input
wire transfers explicit binary64 bits: Java textual NaN is encoded before the
native call, and explicitly supplied payloads pass through unchanged. Outputs
are compared without NaN normalization. This distinction avoids a measured
MSVC CRT input-parser difference without changing the runtime ray algorithm or
the expected observations.

Windows Debug, GCC13 and Clang18 sanitizer profiles each pass all 365 exact
rows. Finite cases do not establish general NaN-payload parity or complete
client targeting.
