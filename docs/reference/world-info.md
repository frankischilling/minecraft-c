# WorldInfo fields and observations

The complete client `ei` and server `ct` classes in the
[verified binaries](binaries.md) establish the same fifteen instance fields.
Neither class has its own static initializer. The
[native owner](../world/info.md) implements fresh, NBT and copy construction,
common scalar setters, name replacement, client Player replacement, and the
no-list serializer. World startup and the live-player-list serializer remain
unfinished.

| Selector | Type | Meaning |
| --- | --- | --- |
| a | long | seed |
| b, c, d | int | spawn X, Y, Z |
| e | long | time |
| f | long | loaded LastPlayed |
| g | long | SizeOnDisk |
| h | compound reference | nullable Player |
| i | int | separately cached Player dimension |
| j | String reference | nullable name |
| k | int | version |
| l, m | boolean, int | raining, rainTime |
| n, o | boolean, int | thundering, thunderTime |

## Source order

Fresh `(long,String)` stores a6 and j11; every other field stays raw. The NBT
constructor reads the inner Data compound, with stores a11,b21,c31,d41,e51,f61,
g71,j81,k91,m101,l111,o121,n131. If Player exists, it stores that same compound
at h150 before reading Dimension and storing i163. Missing fields use the
original typed defaults; a missing name is a nonnull empty literal. A present
wrong type fails before that field's store. Native ownership conversion admits
codec-valid trees, not every malformed programmatically created Java object.

The copy destination stores a9,b17,c25,d33,e41,f49,g57,h65,i73,j81,k89,m97,l105,
o113,n121. Scalars are independent; h/j alias the original referents. Name
replacement accepts NULL. Client `a(compound)` replaces h only, without
recomputing i; the server has no corresponding method. Native snapshots expose
all fields on both sides, rather than inventing paired original getters.

No-list `a()` allocates its output compound, captures h11, then invokes the
private writer. Put calls occur at seed7, spawn17/27/37, Time47, SizeOnDisk57,
clock63/LastPlayed66, LevelName76, version86, rainTime96, raining106,
thunderTime116, thundering126 and optional Player137. It never reads or changes
f/i. The String tag constructor rejects NULL, so that failure follows the clock
and precedes Player insertion; nonnull empty strings work despite the exception
message. Player insertion invokes the base-tag name setter before map insertion,
renaming the same shared compound to Player. No Dimension validation occurs.

The list-taking overload is a distinct route: it examines only the first list
element, optionally invokes that live player's writer, then serializes that new
compound or NULL. It does not fall back to stored h or assign h. Those player
writer descendants have not been implemented by this owner.

## Actual original capture

One fresh Windows Temurin 17.0.17+10 interpreted run per original side executes
the unmodified classes through an isolated platform-parent class loader.
The project-owned `WorldInfoObservation.java` binds exact classes, descriptors,
all fifteen fields, real getters/setters and original compound/tag methods.
The JARs are checked before and after; no original class is rewritten,
recompiled, instrumented or allocated through Unsafe.

Client execution records nineteen complete snapshots, five successful no-list
serializations and one actual null-name refusal: 432 rows. Server records
fifteen snapshots, three successful serializations and one refusal: 326 rows.
Snapshots cover fresh named/NULL states, full/default NBT, copies, independent
scalar edits, equal-text distinct names, nullable rename, shared Player name
mutation and client replacement/clear with stale cached dimension. Every
successful serializer retains the same optional Player and leaves all fifteen
Info fields unchanged. The refusal records the actual IllegalArgumentException
and unchanged Info/Player-name identities, with no returned Data.

Actual output LastPlayed samples are retained in five/three separate
CLOCK_OUTPUT rows. The exported fixtures remove ONLY those complete rows,
leaving every other byte, ID, name, scalar and refusal unchanged: 427 client
plus 323 server rows. No expected timestamp, C output or inferred scalar supplies
these fixtures. The failed serializer's internal timestamp does not return and
is not observed or replaced by a helper sample.

| Artifact | SHA-256 |
| --- | --- |
| Java observer | `69bea70d6d21914687ceee6fa9211b57e367884f22dd14e6d03610869c7afb4d` |
| Original recorder | `9ddd373e45c8ad968cf7ac0164563b29d44552b1d1036338f5793565883d9812` |
| Original capture receipt | `0d27a5410c4ba7e90fecae8467023e0fdd0377f24a57f04be56067aa6025d884` |
| Client stable fixture | `e192d46047daff38652634b29246a3cf46c388b3c1527fc8678745dc30931a36` |
| Server stable fixture | `6eff4edea0833b4e7a815cffeaea6477f4cf36638af52ed895957585e2df237d` |
| Sanitized provenance | `77f6162aab786245839806035da256967cd723b39bd1afbf157385e2454f1f69` |

The [provenance file](../../tests/reference/world/world_info_provenance.json)
records exact Java/compiler/modules/native/source-archive fingerprints, both
compiled helper classes, complete child-stream fingerprints, natural exit code 0
and reaped child outcomes. Compiler stdout and all stderr streams are empty.
All twelve measured inputs and both compiled classes remain unchanged. The generic bounded process
helper has SHA-256
`f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c`.
These finite executions do not qualify arbitrary JVMs, intrinsics, the installed
clock bridge or World bootstrap.

## Native checks

`world.info` passes 8,465 checks on MSVC 19.51, GCC 13.3 and Clang 18.1.3.
Linux uses Debug ASan/UBSan and leak checks; Windows uses strict Debug without
sanitizers. Both serializer clock brackets are ordered on each compiler.
Coverage includes every field, all fourteen wrong Data field types plus wrong
Dimension and their retained prefixes, exact copy/reference behavior, nullable
names, typed Data assignments, stale dimension, independent lifetime,
provenance raw bits, codec failure after rename, budgets and invalid phases.
Wrong-type prefixes are native/source checks, not new original exception traces.

`world.info_observation` separately matches all 750 stable original-derived
rows byte for byte on each compiler. The native probe calls public production
constructors/setters/serializer and reports actual held pointers/handles as local
IDs in observation order. No identity is recovered from equal text. The actual
record name handle and Player/child/root pointers establish the serialized
reference graph. Only actual returned clock rows are excluded from comparison.
The refusal maps native INVALID_FIELD/LevelName to the recorded Java failure
category; native C does not throw that exception.

Fourteen constructed verifier corruptions are rejected per compiler: copied
Player alias, cached dimension, loaded LastPlayed, shared rename, null-name
return, field order and dropped field, on each side. Two additional text controls
change only excluded clock-row values and preserve the stable comparison. These
are verifier tests derived from actual native output, not injected game clocks.

The first GCC unit build stopped on a nested zero-initializer warning in the
test. Replacing it with one aggregate `{0}` retained coverage; the final unit
was rebuilt and passed on all three compilers. The final guarded replay also
passes on each compiler. No production changes followed these checks.
clang-format 18 passes all 171 current C/header files. The public inventory is
now 88 cases; this selected two-case qualification does not claim a fresh full
88-case pass or hosted CI. The earlier complete 85-case Entity run remains its
own historical checkpoint.

## Reproduction and remaining work

Public replay needs Python and the native build, without Java or game files:

```sh
cmake --build build --target mc_test_world_info mc_world_info_observation_probe
ctest --test-dir build --output-on-failure -R '^world[.]info(_observation)?$'
```

For a fresh original capture, supply the pinned Windows JDK and verified JARs
to `scripts/reference/world/capture_world_info.py` using `--java`, `--javac`,
`--client-jar`, `--server-jar` and a fresh `--output-dir`. Its exporter accepts
`--capture-dir` and a fresh set of fixture destinations under `--output-dir`.
Original archives, raw captures and compiled classes remain local. The public
replay retains complete bounded native streams and comparison reports under
the build's `world-info-observation` directory.

World/runtime binding, typed store save/SizeOnDisk accounting, the live-player
list route, mutable Player children, full NBT virtual behavior, resource-failure
equivalence and whole population acceptance remain unfinished. Neither the
source map nor this finite corpus completes those requirements.
