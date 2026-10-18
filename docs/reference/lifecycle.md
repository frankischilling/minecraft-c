# Original chunk lifecycle observations

The C lifecycle boundary implements cache saving, detachment, pending unload
selection and explicit owner retirement. The native runtime composes these
operations with owned storage, generation retention and a stable world clock.
Its [exact-source qualification](#exact-source-lifecycle-checkpoint) passes for
the finite scenarios and ownership domains described here.
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
| `cohort` | client | 160 | 6021 | 25 eligible identities, incremental/forced and client/world routes, empty maintenance, fatal cache-session conflict |
| `cohort` | base server | 134 | 4982 | Base policy, metadata-first saves and fatal cache-session conflict |
| `cohort` | dedicated owner | 137 | 5198 | Default dedicated policy, real far-drop producer, unload/reload and maintenance |
| `session-world` | each context | 6 | 59 each | Actual competing handler constructor, then outer world-save session failure |
| `unload101` | dedicated owner | 7 | 11298 | 101 acquisitions/drops, first 100 unloads, final one and fresh reloads |
| `detach-owned` | client/base | 3 each | 75 each | Actual Boat/Sign load/activation and direct chunk detachment |
| `detach-owned` | dedicated owner | 4 | 230 | Actual far-drop, detach/save/removal and remaining external owners |

These are finite schedules. They establish neither arbitrary runtime histories
nor entity simulation, terrain features, world ticking or crash recovery.
Dedicated saving-off has constructed C coverage; the genuine cohort retains
the default saving policy.

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
Boat; the generic coordinator must preserve the actual `q` state at callback return.
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
`Java_JAVAC_EXECUTABLE` to supplied archives and a pinned JDK17 or JDK21 installation.
Set `MC_REFERENCE_TESTED_COMMIT` to the full tested commit; source qualification
must separately verify this caller label against the actual tree.
Build the test targets, then run `ctest --test-dir <build> -C Debug -R
'^reference.lifecycle_' --output-on-failure`. Ten opt-in profiles run serially;
missing prerequisites report unavailable with exit77. A skipped profile does
not qualify as a passing comparison. The Java-free public
`world.lifecycle_callbacks_replay` uses the pinned fixture.

The producer measures selected JDK executables/runtime inputs, helper sources
and classes, project drivers, loaded Python module files/caches/extensions and
the interpreter executable/library before and after. Shared-library attribution
uses the actual loaded Windows DLL or Linux mapping; embedded Linux Python
records no interpreter library. This is a declared provenance boundary, not a
transitive operating-system dependency closure. Raw project support code is
compiled from its measured source rather than accepted from a timestamp cache.

Session-failure traces retain the original finally catch and the catch of its
rethrow. The pinned JDK17 observation reaches the observation helper; the pinned
JDK21 observation reaches `jdk.internal.reflect.DirectMethodHandleAccessor.invoke`
at PC82. Validation binds that bridge to the measured `java.base.jmod` class
version, method bytes and exception table, alongside the original fatal object,
message and handler identity.

Owned process groups/jobs include the JDI child. Successful captures require
clean exit/reaping without emergency termination; final stream hashes follow
whole-tree cleanup. Files, streams, heap, record sizes and operation counts have
explicit bounds. Supplied archives and personal worlds remain read-only.

A supplied tested-commit string is only a caller label. Final acceptance must
independently bind recorded sources, build inputs and native executables to the
actual Git tree and inspect complete test results. Working-tree passes, private
diagnostic captures and static mappings do not supply that qualification.

## Exact-source lifecycle checkpoint

Source `46546d8322de52beb6c3a51f30e827c0260d2b30`, tree
`0094fd8557889ec27491835054f93616ed082c65`, passes finite lifecycle
qualification on native Windows/MSVC and WSL GCC/Clang. Each complete selected
suite passed 92 tests without failures or skips: 68 public, 22 original
comparisons and two JDK tests. Independent source/provenance and lifecycle
semantic audits, unchanged storage rechecks and complete capture preservation
also pass. This does not complete world simulation or gameplay persistence.

| Platform | Sole qualified evidence root | Native build |
| --- | --- | --- |
| Windows/MSVC v10 | `.local/lifecycle-acceptance/46546d8-v10/windows` | `minecraft-c-lifecycle-46546d8-windows-v10` |
| WSL GCC v9 | `.local/lifecycle-acceptance/46546d8-v9/gcc` | `minecraft-c-lifecycle-46546d8-gcc-v9` |
| WSL Clang v9 | `.local/lifecycle-acceptance/46546d8-v9/clang` | `minecraft-c-lifecycle-46546d8-clang-v9` |

Each platform supplies its own complete rows, captures, controls, build and
audits at the same source/tree. Windows v10 freshly configured, built and ran
all 92 tests, including all ten lifecycle profiles. The whole Windows v9 attempt
is excluded; none of its successful rows or captures supplies this result.

| Native profile | Passed | Failed | Skipped | CTest seconds | Compiler | JDK | CMake | Python |
| --- | ---: | ---: | ---: | ---: | --- | --- | --- | --- |
| Windows/MSVC v10 | 92 | 0 | 0 | 1940.77 | MSVC 19.51.36260.0 | Eclipse Adoptium 17.0.17+10 | 4.2.3 | 3.14.3 |
| WSL GCC v9 | 92 | 0 | 0 | 2969.23 | GNU 13.3.0 | Ubuntu 21.0.12.1+1-1-24.04.4-Ubuntu | 3.28.3 | 3.12.3 |
| WSL Clang v9 | 92 | 0 | 0 | 2961.75 | Clang 18.1.3 | Ubuntu 21.0.12.1+1-1-24.04.4-Ubuntu | 3.28.3 | 3.12.3 |

These are actual complete-suite CTest times. All builds used Debug. Independent
checks bind the recorded Linux ASan/UBSan configuration and effective build
inputs, native executables, tools and all 322 tracked source files to the exact
tree. The source, JAR, tool, build-metadata and executable maps remain unchanged.

| Native profile | Producer `runner.json` SHA-256 | `ctest.xml` SHA-256 |
| --- | --- | --- |
| Windows v10 | `8821e709c38d5a7ad48554dae9c463fa15c51c70f3932c82f79933439782cd1b` | `eebea7380839ff6e74d3598a275cc78dbc2e76a94bc913e4a462630f1f69af1c` |
| GCC v9 | `98aa786f228b5385bdc72797e306c65d832b5539251a578d79f00aaf5f3b9c3a` | `9dc59598569fe9c6c3b7472dc8a1114348952fba3cfdc1af42954e7271265e32` |
| Clang v9 | `6080339f789e9e6405624d6b044141cc89e7bdf3519e2064dd4bb0b21ec8053c` | `ca542374ea095aedeeaf47864b73e28d52d972ef6ef18544a336d694a1e0bc53` |

Audit directory paths in the next table are relative to
`.local/lifecycle-acceptance/`. The v3 driver
reports bind the lifecycle and platform/older auditors to the matching complete
producer, measured source and before/after inputs.

| Platform | Audit directory | Driver SHA-256 | Lifecycle report SHA-256 | Platform/older report SHA-256 |
| --- | --- | --- | --- | --- |
| Windows v10 | `46546d8-v10/audit-windows-v3` | `885aa0a475ddb5645c2d06e8302f0f14817d09201a192b2220cd1a494a6a9227` | `4c5fe60eaaaa96d5670531633f749773f66dd2ca90a038bf18f1d1ff4203211c` | `868d9ac8b58a7ca9b5d964149dfe3d662322a5faaf336cfd31d25da0e24a53f7` |
| GCC v9 | `46546d8-v9/audit-gcc-v3` | `fdd4856e2bc0ced287790174737f4ad7d8e012e43482bbb00d649117e0c580de` | `b7fbaacd4165ed4886eb3c39618955ca9978c289f52ae8f63897d7cb94a17f77` | `df83b44fd17a6380b9882b82372182ae62a23cfa0f36ce1baa8f91b552084a72` |
| Clang v9 | `46546d8-v9/audit-clang-v3` | `be7bed3228a876413509f5f891577c9638c91c1c0017804bb05f81f9e2cf9743` | `dde51b77dcd3f692c56a5ec7b41badb5410755e6be46a27726a9c99b40e537c8` | `18e391dccaf61e275cffdb93380c22179ddb71efbf66f3b8d188afd73989d24c` |

The lifecycle audits independently check all ten profiles and 466 original
operations per platform, including 103925 complete original/native arrays,
357 actual timestamp inputs and 202 batch returns. Their callback domain retains
61 actual callback pairs, 141 boundary states and 690 complete arrays, including
explicit imported setup states. Class-file, clock, ownership and sparse-graph
proof requirements remain intact. The older-profile auditor checks eight other
original comparisons; the four storage profiles have separate rechecks.

| Platform | Storage recheck `runner.json` SHA-256 | `capture-preservation.json` SHA-256 | Retained roots / files |
| --- | --- | --- | --- |
| Windows v10 | `e95a9ce9451246052f2a7a7b2989969787e1f2d9f4dd0415b08a311f8a1240a7` | `927e0ffc767148a5e470375fbf3d550f85c4377294a5f9ea3bc7a9a994e4291b` | 22 / 11828 |
| GCC v9 | `f4f743bcb82b92d12e8ed450b60804a76a16de4ac7a01a0c2971499bd08a67c3` | `fa84229c1516baca32b183398f2631328ef6f9db28a4478b8f25970daf3b384d` | 22 / 11828 |
| Clang v9 | `65eeb80a0fd140ca27ced452013d73a304b3d6c7ab9e2873b9ada6c0fd2a68e2` | `d8695fb24018b29ed3ef6a86acc5768c7b85bbbd374606d1472b5658f7a33d0b` | 22 / 11828 |

Storage reports are under `<cohort>/<platform>/storage-recheck/runner.json`;
capture reports are under `<cohort>/<platform>/capture-preservation.json`.
Each capture report verifies all files and directories, including empty ones,
across its 22 original-profile roots. The complete per-profile manifest index is
`.local/lifecycle-review/qualified-capture-index-46546d8.json`, SHA-256
`5935f1e15710e4f650fb257329f66d8550b956c6fb25449629c49fa056340044`. Its 22 entries per platform retain exact manifest paths,
byte counts and hashes from the final independent audit records. Original assets
and raw captures remain private.

The joined qualification receipt is
`.local/lifecycle-acceptance/46546d8-final-qualification.json`, SHA-256
`519600d8e2d4509076a296ec759c830e3d418953094bf608399558c7acba14bc`.
It binds the exact three producer/audit/storage/capture sets and the separate
hosted result. The uninstrumented controls, native nonempty-activation refusals
and their file-preservation checks are part of this finite qualification.

| Evidence | Result |
| --- | --- |
| Hosted checks for the tested source | [Run 39156057705](https://github.com/frankischilling/minecraft-c/actions/runs/39156057705) at `46546d8322de52beb6c3a51f30e827c0260d2b30`: all four jobs passed. Each compiler passed 70 unique CTest cases with zero failures/skips; separate JDI and interval regressions passed on MSVC/GCC/Clang, with 556 positive/2011 negative interval checks per compiler. Formatting passed for 125 files. CTest times: MSVC 248.22s, GCC 171.45s, Clang 175.82s. |

### Audit attribution history

Earlier private audit attempts remain retained and excluded. The first platform
auditor expected 319 tracked files; the tested source has 322. The next Linux
attempt compared device numbers from different kernel mount sessions. The final
auditor instead binds the original boot journal and runner interval to the same
persistent ext4 filesystem, then requires the current UUID block device,
mountpoint and exact parent/build inodes to agree. Ownership markers, paths,
source/tool/build bytes and all other gates remain unchanged. The driver measures
the fixed origin proof before and after. This attributes retained snapshots; it
does not assert continuous immutability.

The corrected auditors ran fresh complete audits on all three platforms. These
audit-only corrections changed no producer or original capture. Failed audit-v1
and audit-v2 outputs remain immutable; neither supplies the accepted audit result.
The earlier whole Windows v9, v8 and v7 attempts remain excluded as recorded below.

### Excluded Windows v9 attempt

At the same source `46546d8322de52beb6c3a51f30e827c0260d2b30`, the Windows
v9 suite passed 91 tests, skipped 1 and failed none. `reference.chunk_data`
reported original setup unavailable before measurement: the actual server-world
queue contained 42964 entries against the complete-observation bound of 16384.
The helper returned 77 after `SETUP_BEGIN`, without reaching setup completion or
measurement. The original unavailable manifest, stderr and whole attempt remain
retained and unqualified.

This evidence establishes a premeasurement budget refusal; it does not identify
the internal producer of those queue entries. The Windows v10 retry uses the
unchanged source, observation bounds and full test catalogue. Its complete suite
and audits must independently pass; earlier successful Windows v9 rows supply no
part of that qualification.

### Excluded v8 attempt

Source `96bd35b4a0138383478a936465a9c69be99a2547`, tree
`d379e532a09dcccea13900dcd40c3f1aa32ac446`, completed its v8 matrix with these
terminal CTest results. They do not qualify the current source.

| Native profile | Passed | Failed | Skipped | CTest seconds |
| --- | ---: | ---: | ---: | ---: |
| Windows/MSVC | 92 | 0 | 0 | 1995.04 |
| WSL GCC with ASan/UBSan | 91 | 1 | 0 | 2802.89 |
| WSL Clang with ASan/UBSan | 91 | 1 | 0 | 2806.21 |

Both Linux dedicated `unload101` Java observations exited cleanly and were
reaped, then trace validation rejected the helper epoch interval before C
comparison. The actual `unload100` intervals were 281278ms on GCC and 280077ms
on Clang; both exceeded the validator's 180000ms cap while remaining within the
observed profile's 600-second controller budget. The two corresponding
operation-interval checks used the same shorter cap. Recorded source, input and
class maps remained stable; the failed cohort remains excluded.

Retained diagnostic replays scoped those two interval caps to the observed
dedicated101 profile. The Linux replays required the exact retained epoch error
in place of the empty-error prerequisite; they preserved the failed manifests.
All eight semantic sections passed for Windows, GCC and Clang, without findings
or proof gaps. Separate Linux native
`--development` comparisons passed all seven operations, 6090 full-array
comparisons, 486 file comparisons and 202 batch boundaries per compiler. These
diagnostics leave the original failed manifests and cohort result unchanged.
They are not exact-source qualification or fresh original observations.

[Hosted run 39151714689](https://github.com/frankischilling/minecraft-c/actions/runs/39151714689)
at the v8 source passed all four jobs. Each compiler passed 70 unique CTest
cases without skips, and the separate JDI regression passed on MSVC/GCC/Clang.
Formatting passed for 125 files. CTest times were 242.69s, 114.84s and 176.36s
respectively. These results remain attached to that source.

### Excluded v7 attempt

The earlier source `e5dc551a901a84fcec1568fac9e0d62fc5a6ee14` completed its
v7 matrix with these terminal results. They do not qualify the current selection.

| Native profile | Passed | Failed | Skipped |
| --- | ---: | ---: | ---: |
| Windows/MSVC | 92 | 0 | 0 |
| WSL GCC with ASan/UBSan | 91 | 1 | 0 |
| WSL Clang with ASan/UBSan | 90 | 1 | 1 |

Both Linux dedicated `unload101` observations completed Java execution but failed
trace validation of stable callback array/wrapper identities before C comparison.
Clang also skipped `reference.chunk_data` during unseeded original-constructor
setup. These failed/unavailable observations remain retained and excluded; the
Windows pass is a historical result for its own source.

### Debugger mirror retention

At `96bd35b4a0138383478a936465a9c69be99a2547`, the controller retains target
object mirrors from their first serialized IDs through the observation session,
including objects read before helper identity labels arrive. The table permits
32768 distinct mirrors, accepts known IDs at capacity and rejects a new ID before
insertion. Duplicate helper labels and reused object labels remain rejected.

[JDWP permits disposal of an objectID](https://docs.oracle.com/en/java/javase/21/docs/specs/jdwp/jdwp-protocol.html#JDWP_VirtualMachine_DisposeObjects)
while the target object remains live. Recording its numeric
[JDI uniqueID](https://docs.oracle.com/en/java/javase/21/docs/api/jdk.jdi/com/sun/jdi/ObjectReference.html#uniqueID())
does not retain the JDI mirror.

The [standalone regression](../testing.md) uses the actual controller methods and
synthetic static-rooted arrays/lists. On Linux JDK21, the old controller's marker
case proved disposal and changed IDs for all 128 live anchors. Its delayed-label
case also failed a duplicate-label guard; it is not a completed identity comparison.
The corrected controller passed both cases on native Windows JDK17 and Linux
JDK21: all 128 separate control mirrors cleared, completed disposal and changed
IDs, while all 128 recorded anchor IDs stayed stable. Capacity and duplicate-label
controls passed. Standalone regression capture IDs are `125982aab9064ddfb422c909e633c818`
(Windows) and `e5f0083c8d824a55a2b031b65e5c4cb6` (Linux). This standalone check
runs separately from the 92-test suites. The qualified Windows v10/Linux v9
original/control evidence and independent audits are recorded above.

Earlier JDK21 dedicated `unload101` observations reached the 300-second
controller deadline, including an isolated retry. They remained incomplete and
never entered native comparison. Retain these failed attempts; they do not
supply evidence for the accepted result.

Only dedicated `unload101` now permits 600 seconds in the Java controller and
660 seconds in its exporter-owned process. Other observed profiles retain
300/360 seconds; control and preparation processes retain their existing
180-second exporter limit. The validator selects the same profile cap and
requires an integer budget. These allowances change no event, graph, data,
thread, heap, operation-count, stream, file or comparison requirement.

### Profile-scoped helper intervals

At `46546d8322de52beb6c3a51f30e827c0260d2b30`, both helper-trace and
operation-state interval checks permit 600000ms only for the observed dedicated
`unload101` profile. JDI stops count toward the helper's wall-clock interval;
the cap matches that profile's controller budget. Other observed profiles,
control operations, preparation and constructor intervals retain 180000ms.
Every event must still lie inside its own sampled helper interval. Integer
types, ordering, timestamp correspondence and all data/graph checks remain
required. The 660-second exporter allowance is not the helper interval cap.

The asset-independent [interval regression](../testing.md) compiles the actual
validator checks and verifies their operation, role and context/profile
bindings. Before the change, both call sites rejected the retained 281278ms and
280077ms operands with their exact epoch errors. The corrected checks passed
556 positive and 2011 negative cases on native Windows and Linux, including
scope, type, limit-plus-one, constructor/preparation and event-bracketing
controls. This call-site regression is separate from complete capture
validation and the qualified Windows v10/Linux v9 evidence above.

This boundary covers finite seed23 constructor/API schedules, native empty
runtime records and separately labeled external Boat/Sign callback replay.
It does not establish Java6 collection history, Java GC lifetime, entity/player
simulation, terrain-feature bodies, light propagation, general deferred cleanup,
world ticking, gameplay dimension transitions, running dedicated-server
persistence or crash recovery. Native fatal storage outcomes remain a documented
departure from swallowed original I/O failures.
