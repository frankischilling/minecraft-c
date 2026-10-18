# Original chunk lifecycle observations

This boundary is under development. It implements cache saving, detachment,
pending unload selection and explicit C owner retirement. Selected working-tree
comparisons pass; exact-source acceptance and hosted qualification remain open.
The [persistence checkpoint](storage.md#exact-source-storage-checkpoint) remains
the accepted storage baseline. See the [API contract](../world/lifecycle.md).

## Original contexts and schedules

The archives are the verified client and server binaries in
[binary verification](binaries.md). Client observations use the persistent `fd`
world and `ok` cache. Server base observations use `dj`/`jb`; dedicated-owner
observations use the actual `dp`/`he` construction route. None starts the server
tick loop. Every profile uses seed 23, fresh owned preparation and separate
observed/control copies. Original APIs create the expected values; C never does.

| Profile | Context | Operations | Watched events | Purpose |
| --- | --- | ---: | ---: | --- |
| `cohort` | client | 160 | 6021 | 25 eligible identities, incremental/forced and client/world routes, unload/reload, fatal cache-session conflict |
| `cohort` | base server | 134 | 4982 | Base policy, metadata-first saves and fatal cache-session conflict |
| `cohort` | dedicated owner | 137 | 5198 | Dedicated gates, real far-drop producer and maintenance |
| `session-world` | each context | 6 | 59 each | Actual competing handler constructor, then outer world-save session failure |
| `unload101` | dedicated owner | 7 | 11298 | 101 acquisitions/drops, first 100 unloads, final one and fresh reloads |
| `detach-owned` | client/base | 3 each | 75 each | Actual Boat/Sign load/activation and direct chunk detachment |
| `detach-owned` | dedicated owner | 4 | 230 | Actual far-drop, detach/save/removal and remaining external owners |

These are finite schedules. They establish neither arbitrary runtime histories
nor entity simulation, terrain features, world ticking or crash recovery.

## Boundaries and controls

`ChunkLifecycleObservation.java` calls untouched original constructors and APIs.
`ChunkLifecycleController.java` observes the copied world through loopback JDI
in interpreted mode. It retains actual method descriptors, bytecode positions,
entry/return nesting, exception identities, class origins, full arrays and
complete bounded inventories. The independent uninstrumented invocation follows
the same API schedule. The pure `lifecycle_controls.py` validator reads the
original class files and captured records without starting Java or C.

Relevant entries include client `ok.a(ZLyb;)Z`, base `jb.a(ZLpj;)Z`, dedicated
`he.a(ZLpj;)Z`, and their `a()Z` maintenance methods. Normal detach is client
`lm.f()V` or server `hi.e()V`: active becomes false at bytecode offset 5, before
tile invalidation and all eight deferred entity slices. Dedicated maintenance
observes pending/map/list removal at offsets 86/97/109. Method fingerprints and
the complete watched catalog live in the validator beside the helpers.

The 101 profile explicitly records its reduced graph policy. Every outer API
state has complete retained payloads. Individual acquire/drop boundaries retain
all headers, affected-target arrays, membership and live metadata. Omitted
repeated event graphs require class-file-bound proofs of the exact pure/no-op
domain; missing proof fails default validation. Smaller profiles retain full
graphs at every watched event.

Session, LastPlayed and region timestamps remain actual recorded clock values.
Validation ties each permitted difference to its original operation, bytecode
and clock epoch. Unused per-VM Random seeds remain raw: serialization is bound
to actual JDI seed/Gaussian state and unchanged same-VM state. The narrow
cross-control exclusions do not replace RNG state with a fixed seed.

## Nonempty external owners

Preparation clones an actual origin schema through the original NBT APIs and
stores one genuine Boat and Sign at chunk `(16,0)`. The target is API-constructed
storage, not terrain generated at that coordinate. Only entity slice 4 contains
the Boat. The raw loader return has inactive state and last-save time zero;
the acquisition coordinator then stamps 6000 and publishes/activates it.

Detach invalidates the Sign and defers the same eight slice owners in order.
World entity/tile lists retain the Boat and invalid Sign; the deferred list gains
the same Boat. Dedicated cache removal also leaves the entity-ID registry
retaining that Boat. Unpublish therefore cannot imply immediate C retirement.
No tick or general deferred-cleanup routine runs in this profile.

The generic callback probe imports the raw decoder result and computes only
coordinator transitions. Boat/Sign decoding, registration, invalidation and
serialization bodies are external. In the dedicated save, the original
serializer temporarily clears entity eligibility and sets it again for the
Boat; the generic coordinator must preserve the actual callback return value.
The empty native runtime separately refuses this activation before publication.
Its copied files must remain byte-exact after failed loading and EOF.

The public original-only callback fixture is pinned by SHA-256
`763305cbd70129632ec079ae336a8d29dc5b721321e634898627d9bf088e9cc8`.
It retains four shared array blobs and 61 callback pairs across the three
contexts. Native replay checks 141 C states, including explicit import/setup
states, and 690 full normal-array comparisons. These counts include checking
the imported raw payload before the original loader-return boundary; they do
not represent additional original observations. Thirty-nine response corruptions
and seven actual malformed-input invocations must reject.

## Reproduction and provenance

`export_lifecycle.py` captures one context/profile without executing C.
`compare_lifecycle.py` replays native owner operations and complete copied files;
`compare_lifecycle_callbacks.py` handles the external nonempty callback domain.
`run_lifecycle.py` composes a fresh original/control capture and strict native
comparison. Run reports preserve failed attempts and measured input bytes.

Enable `MC_ENABLE_LIFECYCLE_REFERENCE_TESTS` and set
`MC_REFERENCE_CLIENT_JAR`, `MC_REFERENCE_SERVER_JAR`, `Java_JAVA_EXECUTABLE` and
`Java_JAVAC_EXECUTABLE` to supplied archives and one JDK17-or-newer installation.
Build the test targets, then run `ctest --test-dir <build> -C Debug -R
'^reference.lifecycle_' --output-on-failure`. Ten opt-in profiles run serially;
missing prerequisites report unavailable. The Java-free public
`world.lifecycle_callbacks_replay` uses the pinned fixture.

The producer measures selected JDK executables/runtime inputs, helper sources
and classes, project drivers, loaded Python module files/caches/extensions and
the interpreter executable/library before and after. Shared-library attribution
uses the actual loaded Windows DLL or Linux mapping; embedded Linux Python
records no interpreter library. This is a declared provenance boundary, not a
transitive operating-system dependency closure. Raw project support code is
compiled from its measured source rather than accepted from a timestamp cache.

Owned process groups/jobs include the JDI child. Successful captures require
clean exit/reaping without emergency termination; final stream hashes follow
whole-tree cleanup. Files, streams, heap, record sizes and operation counts have
explicit bounds. Supplied archives and personal worlds remain read-only.

A supplied tested-commit string is only a caller label. Final acceptance must
independently bind recorded sources, build inputs and native executables to the
actual Git tree and inspect complete test results. Working-tree passes, private
diagnostic captures and static mappings do not supply that qualification.
