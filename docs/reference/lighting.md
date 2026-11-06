# Light regions and ordered queue evidence

The supplied [client and server fingerprints](binaries.md) pin these observations.
Client classes `st`, `fd`, `eb` correspond to server `lv`, `dj`, `co` for the
selected methods. Complete class equivalence is not claimed.

| Behavior | Client | Server | Bytecode |
| --- | --- | --- | --- |
| Region construction | `st.<init>(Leb;IIIIII)V` | `lv.<init>(Lco;IIIIII)V` | Retains light kind and six bounds |
| Region merge | `st.a(IIIIII)Z` | `lv.a(IIIIII)Z` | Offsets 0 through 325 |
| Default queue request | `fd.a(Leb;IIIIII)V` | `dj.a(Lco;IIIIII)V` | Offsets 0 through 16; delegates with coalescing enabled |
| World admission and queue storage | `fd.a(Leb;IIIIIIZ)V` | `dj.a(Lco;IIIIIIZ)V` | Offsets 0 through 325; ordered search begins at 105, append at 215, overflow check at 243 |

Both selected merge bodies and queue overloads match after explicit class
correspondence. The C [queue contract](../world/lighting.md) covers bounds merging
and storage after world admission. It does not implement the admission checks.

## Concrete World source

`mc/world/runtime/lighting.h` supplies [actual GAME World operations](../world/runtime-lighting.md)
alongside the earlier caller-owned projections. Fresh paired bytecode reads
establish the following order; these are source observations, without a new
executed-original World comparison.

| Operation | Client | Server | Reached order |
| --- | --- | --- | --- |
| Height | `fd.d(II)I` | `dj.d(II)I` | Asymmetric border, nonloading presence33, separate getter48, unsigned height61 |
| Enqueue | `fd.a(Leb;IIIIIIZ)V` | `dj.a(Lco;IIIIIIZ)V` | Current Dimension.e4; Sky11 only on no-sky branch; shared A increment23; equality50; midpoint presence67; getter87; blank90 |
| Actual C storage | Same enqueue | Same enqueue | Initial size109; current size153/get163/type175; same-reference merge194; new region219/constructor234/add237; size251; diagnostic289 before clear296 |
| Guard cleanup | Same enqueue | Same enqueue | Decrement current A on ordinary306 and exceptional319 exits, with earlier explicit decrements on guarded returns |
| Light initialization | `eb.<clinit>()V` | `co.<clinit>()V` | Sky c=15/static12, Block c=0/static26; finite native handles omit generic Enum methods |
| Region construction | `st.<init>(Leb;IIIIII)V` | `lv.<init>(Lco;IIIIII)V` | Actual nullable type and six signed fields; no bounds normalization |

Native tests use real client World construction and store-owned restored chunks.
The scalar depth controls explicitly set actual shared A after construction;
they do not claim naturally reached recursion. Native allocation and output
failures retain their documented resource policy. Queue propagation, live
Chunk finishing and complete generation remain unfinished.

## Observations

`LightObservation.java` invokes actual untouched region constructors and merge
methods. Its 48 cases contain 72 sequential requests, including containment,
padding rejection, volume growth of two versus three, degenerate and reversed
bounds, negative coordinates, endpoint wrapping and wrapped volume products.
Every result includes the boolean and all six retained bounds. No translated
merge algorithm supplies expected results.

The world experiment reuses the genuine fresh-world construction in
`ChunkObservation.java`. If the actual cache is empty, it invokes the original
cache load/generate/insert method for `(0,0)`. Two genuine requests establish a
nonempty baseline while retaining any setup queue entries. The measured corpus
contains 43 requests, including both kinds, enabled/disabled coalescing, newest
matching entries, the fifth/sixth search positions, foreign-kind blockers,
contained/adjacent regions and missing midpoint chunks.

Before each request, actual original admission queries record whether the
request reaches queue storage. Complete world snapshots before and after those
queries must agree. The observer invokes the actual world queue overload and
records every queued kind and bound in order. Before/after snapshots retain
cache membership, chunks, array fingerprints, object identities and recursion
counters. No fake world/provider, field replacement, patched class or manual
queue drain is used.

Server spawn setup can vary even with a fixed world seed. C replay uses each
side's actual baseline and admission results independently. Client/server merge
results compare directly. Queue results compare directly only when the starting
queues and complete requests agree; otherwise distinct contexts remain explicit.

After freezing the bounded queue snapshots, a separate experiment appends real
uncoalesced requests until the original list reaches exactly one million entries.
It checks every count increase, retained queue identity and restored depth. The
bounded starting prefix retains its objects and bounds. The next actual request
prints the original warning and clears the list to zero. This is the original
automatic abort, rather than an observer clear operation.

## Reproduction and limits

Configure `MC_ENABLE_LIGHT_REFERENCE_TESTS=ON` with the supplied JAR paths and
`MC_REFERENCE_TESTED_COMMIT`. CTest runs `reference.light_queue`. The direct
equivalent is:

```sh
python scripts/reference/lighting/compare_light.py --client b1.7.3.jar --server b1.7.3_server.jar --executable build/mc_light_queue_probe --work-dir .local/reference/light-queue --tested-commit FULL_COMMIT_ID
```

On Windows use the executable under the build's `Debug` folder. Optional
`--java`, `--javac` and `--javap` must identify one JDK11+ installation. Original
classes load through an isolated loader with the platform loader as parent.
Compilation uses copied helper sources, Java11 target classes, disabled annotation
processing and empty search paths. Java environment injection is rejected.

Each run owns a unique capture directory and fresh world roots. Direct child
processes have a 90-second bound, a 512 MiB Java heap and 32 MiB limits on each
output stream. The observer bounds ordinary queue snapshots to 32768 entries;
the separate million-entry experiment records count/prefix controls, rather than
serializing that large list. World inventories are observed during execution;
these limits are not an operating-system process-tree or filesystem quota.

Manifests retain actual helper/runtime sources, executables, JDK files, Python,
original JAR fingerprints, compiled helpers, commands, complete records and world
inventories. Inputs and helpers are checked again after execution. Missing
prerequisites return 77; original method failures and completed mismatches fail.
An unavailable later side cannot erase a completed C comparison failure.

Public fixtures come from the original-only capture at
`.local/reference/light-fixed-final/capture-d2d761ac34aa402887bf844df3bfceb6/manifest.json`.
The generated header records that manifest and both record hashes. The exporter
copies observations and records at most one changed queue entry per step; public
tests reconstruct and compare the complete live queue after every request.
They also exercise bounded storage, aliases, failure preservation and the exact
million-entry policy without Java or original files.

This evidence does not establish world admission in C, full chunk finishing,
dirty state, height-query tracing, cache lifecycle, propagation or gameplay.
Exact tested source and platform results belong in [status](../status.md).

## Recorded results

Source `900369d2bdea598324e5faf61e9a97d9a1aabd70` passes all 33 local tests
on native Windows/MSVC19.51 and WSL GCC13.3/Clang18.1, without skips. The Linux
builds use ASan/UBSan. Actual original observations use Temurin17 on Windows
and OpenJDK21 on WSL. Public queue tests pass 2001560 checks.

Each comparison replays 112 complete records per original side: 72 merge
results, one starting queue and 39 admitted enqueue results. Four requests with
missing midpoint chunks preserve the preceding complete queue. All three local
builds match both sides. These fresh runs also have identical starting queues
and requests, so complete client/server queue comparisons pass directly. The
frozen fixture capture retains additional actual server baseline entries.
Merge results agree directly between both originals.

All three reports identify that source, successful comparisons, no errors and
unchanged before/after input/helper fingerprints. Independent fixture review
reconstructed all 86 complete queue states from the generated baseline/delta
records and matched every kind and bound to the original-only capture.

Reports remain under each `build-light-*/reference-light-queue/capture-*/` folder:

| Build | Capture identifier | Manifest SHA-256 |
| --- | --- | --- |
| Windows/MSVC | `541321276f344f6991e9b9b4a4927f88` | `a119bf9a99bedf2aaa8b33ee24ce0432c1695c5382df7e3c3c23c791f8dee224` |
| WSL/GCC | `3e2748b30d66460abc3a9c7fc5829765` | `849d9223e3531958ef9825adce2dbc7653474827d1735d8247686091b66065c2` |
| WSL/Clang | `9e4a170a5c7a44658e94c90687b0e987` | `79ab9a822d6730f1209bc8bd697addf2c05e72666348a29af4bbb8c03bacf860` |

The runtime is integrated at `fe83e6fe67f287698312bd32136b83a7f0cd31f2`.
Native Windows/MSVC and Linux GCC/Clang each pass all 30 public/numerical tests,
plus formatting for 66 C/header files, on the exact branch source and resulting
main. The integration tree matches the tested branch tree; see
[branch checks](https://github.com/frankischilling/minecraft-c/actions/runs/39071978648)
and [main checks](https://github.com/frankischilling/minecraft-c/actions/runs/39072086470).
