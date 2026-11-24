# Original solid-support and presentation observations

The unchanged [client and server archives](binaries.md) each completed 56 direct
subject calls through `SolidSupportObservation.java`. Each experiment used a fresh
JVM and an absent owned World directory. All subject calls returned normally.
Five controller children, including compilation and version commands, exited
zero naturally and were reaped. Nineteen compiled helper classfiles and all
helper, archive and selected JDK inputs matched before and after execution.

These are finite original API observations. They have no native replay,
helper-free control equivalence, dedicated route, complete fluid/population or
save-compatibility claim.

## Genuine receiver and input preparation

The new observer calls the existing project-owned Mutation observer's actual
Model constructor/setup and Context readers through reflection. It does not call
that observer's measurement or main routine. Original game classes load from the
untouched selected archive in a fresh classloader. Actual base World, storage,
supplied Dimension, provider and reflected method/field descriptors are recorded;
their concrete types are not replaced by an inferred Overworld label.

One real acquiring World call returned a normal Chunk `(0,0)` on each side. The
helper verified its owner, complete block-array extent, actual provider presence
and cache identity. Nine prepared cases each changed one existing block-array
byte, called the actual getter and World support predicate, then restored only
that helper-controlled byte. All eighteen getter results across both experiments
matched their prepared input. All byte restorations completed. Genuine World,
cache, Random and queue effects were retained.

The inputs were air, stone, double and single slabs, glass, leaves, flowing water,
flowing lava and ID255. Separate direct Block support/opacity and Material c/h
calls used seven actual named Block instances. Those direct predicates have
narrower scope than the World calls. Five border queries and four sound/particle
calls with named and NULL Strings completed on each genuine empty WorldAccess list.
No predicate result was calculated from the rewritten implementation.

## Retained state and limits

Each side retains 132 raw encoded Context descriptors, plus actual Info/time,
flags, E/F identities/order, subject fields and outcomes. The sealed Context
reader clones complete arrays internally for its before/after comparisons, while
the exported descriptors contain array lengths and hashes. They do not contain
replayable array payloads. Context and helper inspection calls are additional to
the 56 counted subject calls; these experiments do not prove those reads inert.

The controller checked exact descriptor byte lengths/hashes, aggregate counts,
complete ASCII JSON framing, source-call ordering and matching immediate stderr
outcomes. Sixteen framing mutations were rejected across the two sides. Output,
heap, context and wall-time limits were finite; terminal exit and reaping were
required independently of the helper's completion flag. Failed or unavailable
setup would remain a separate outcome rather than a successful World sample.

The new [helper source](../../tests/reference/world/SolidSupportObservation.java)
depends on the unchanged [setup/reader](../../tests/reference/world/MutationObservation.java).
Compile both with the pinned Java17 profile, then run each archive separately:

```text
javac -encoding UTF-8 --release 17 -d <new-classes> MutationObservation.java SolidSupportObservation.java
java -Xint -Xmx1g --add-opens java.base/java.util=ALL-UNNAMED -cp <new-classes> SolidSupportObservation <original.jar> client|server <absent-owned-world-root>
```

The application runtime remains C and does not execute these helpers. The
[native fluid contracts](../world/fluids.md) are separate evidence and retain
their own coverage limits.
