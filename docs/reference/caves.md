# Normal cave evidence

These observations cover the normal
cave stage after surface replacement. The [C interface](../world/caves.md) uses
the [accepted lookup/floor helpers](math.md). Whole chunk finishing, lighting,
population, other dimensions and full game parity remain separate requirements.

## Original methods

The pinned original client/server JAR hashes are respectively:

```text
af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c
033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d
```

Paired `javap` bodies agree after explicit corresponding-symbol normalization,
with offsets, numeric constants and branch targets retained. Decompiled output
was used for navigation; the untouched binaries and controlled observations
supply the evidence.

| Body | Client | Server | Last offset / instructions |
| --- | --- | --- | --- |
| Cave-base constructor | `fv.<init>()V` | `dy.<init>()V` | 21 / 11 |
| Public neighbor scheduler | `fv.a(Lcl;Lfd;II[B)V` | `dy.a(Lbl;Ldj;II[B)V` | 138 / 74 |
| Base neighbor hook | `fv.a(Lfd;IIII[B)V` | `dy.a(Ldj;IIII[B)V` | 0 / 1 |
| Normal cave constructor | `sq.<init>()V` | `lt.<init>()V` | 4 / 3 |
| Room wrapper | `sq.a(II[BDDD)V` | `lt.a(II[BDDD)V` | 32 / 21 |
| Path geometry | `sq.a(II[BDDDFFFIID)V` | `lt.a(II[BDDDFFFIID)V` | 1213 / 653 |
| Normal neighbor hook | `sq.a(Lfd;IIII[B)V` | `lt.a(Ldj;IIII[B)V` | 250 / 132 |

The seven bodies total 895 instructions. The normal generators `yf/pk` construct
the actual `sq/lt` into `u` before their separate explicit-seed `j` and 82 noise
layers. The full normal entry runs base at 93, surface at 104, caves at 119 and
chunk finishing at 124. The measured public cave pass reads only the world's
stored seed and its supplied byte buffer; its generator argument is unused.

The public scheduler owns another Random, fixed range 8, and the two odd world
seed multipliers. Negative odd draws require signed truncating division;
`draw|1` gives a different result. The shared cave Random supplies one seed for
each local path Random, interleaved with room/count/angle/width draws. Child
paths execute depth first.

Water screening reads IDs 8/9 on the box shell, skipping the interior column
middle. Only stone 1, dirt 3 and grass 2 are ordinarily replaced. The addressed byte's
Y is one above geometric Y; geometric Y below 10 writes flowing lava 10. A
persistent per-column grass flag can repair dirt below the included geometry.
These differences are preserved in C.

## Original-only natural capture

The initial capture used native Temurin 17.0.17+10, untouched pinned JARs and
project-owned reflective source. It preceded numerical C fixtures. Each case
constructs a fresh genuine world, then a fresh measured normal generator.
Actual climate/base/surface prepare the input before invoking the actual `u`
public entry. Genuine server constructor setup can itself generate complete
chunks before the fresh measured generator; those setup effects are inventoried
separately.

Ten cases cover world seeds 0, 1, -1, signed-long endpoints and origin/moved/neighbor
chunks, including desert, cold and gravel contexts. Each performs first,
restored-input and reused-output calls on the same cave object. Every record
retains complete input/output 32771-byte arrays, four following cave longs,
generator Random continuation, all 82 layers, q425, all 8 scratch arrays, climate,
biome identities, materials and table controls.

| Case | Seed | Chunk | First/restored changed bytes |
| ---: | ---: | --- | ---: |
| 1 | 0 | 0,0 | 0 |
| 2 | 1 | 0,0 | 131 |
| 3 | -1 | 0,0 | 1290 |
| 4 | -9223372036854775808 | 0,0 | 0 |
| 5 | 9223372036854775807 | 0,0 | 1290 |
| 6 | 0 | -1099,279 | 665 |
| 7 | 0 | -8,1 | 1411 |
| 8 | 0 | 1,0 | 132 |
| 9 | 1 | -1,0 | 137 |
| 10 | 0 | -17,31 | 1060 |

All ten complete case records per side agree, covering 30 calls and their
continuations. Restoring the input reproduces both the complete output and the
post-call cave continuation. The public entry reseeds its Random from the
unchanged world seed. Reused outputs changed no further bytes in this corpus;
general idempotence is unverified. Constructor snapshots leave the unobserved
initial cave continuation null. Raw stdout is 15177192 bytes per side, SHA-256
`8b88de30bcdcb73ff76d7eb057b4eb55247121b2b883fb366ea230c7b8f5c29a`.
The initial full manifest SHA-256 is
`bab5f6aad4890ba7bbdcc74384f9274a60855bcbd47230199e2b6188e79d14f8`.
An independent reader checked all 66 attributed files, complete raw/saved
records, 30 ordered state controls per side, all before/after inputs and five
Java 11 helper classes. Client inventory has no files; server inventory has
ten 8-byte session locks. No existing worlds were opened or reused.

Lossless natural fixtures retain 40 complete array references and 120 continuation
longs. Their 15 unique arrays contain 25776 runs. Independent decoding of the C
initializers matched every byte against both originals, separately from the
extractor. The committed-source local acceptance below uses these independently
captured expectations.

## Original-only protected capture

The separate v2 capture invokes the actual protected room/path overloads on fresh
`sq/lt` objects with explicitly seeded actual cave Randoms. It constructs no
worlds. Native Temurin 17.0.17+10 produced 44 equal complete records per side:
28 rooms and 16 paths. Both retained working-directory inventories are empty.

Each record preserves the raw request, complete 32771-byte input and output,
four following shared Random longs before and after the call, changed-byte
count, original math-table digest, retained Random identity, fixed range 8 and
the untouched suffix `8a8b8c`. The observer checks the actual registered block
identities/IDs and immutable table before and after each call. It contains no
carving algorithm, copied Random implementation or expected carve mask.

Targets are `(0,0)`. Finite centers have X/Z in `0..15` and Y in `0..127`.
Paths accept binary32 width `0..3`, yaw `0..2*pi` using the original float upper
bound `0x40c90fdb`, pitch `-0.125..0.125`, end `85..112`, and
`0 <= step < end`; their vertical ratio is 1. The actual room wrapper supplies
its own width, `-1/-1` step/end signals and ratio 0.5. Raw binary32/binary64
echoes retain signed zero. Owned patterns use `(x*16+z)*128+y` order.

| Controls in the 44 requests | Observed result |
| --- | --- |
| Stone, dirt and grass fills | The seed-0 room at `(8,32,8)` changes 604 eligible bytes in each fill. |
| Solid IDs `0,7,12,13,24,79,128,255` | Each complete prefix remains unchanged. |
| One water 8/9 byte at `(9,33,8)` on stone | The water remains and 603 other bytes change. |
| Water planes on stone | ID 8 at Y 36 gives 0 changes, Y 37 gives 188 with a shifted mask, and Y 38 gives 604; ID 9 at Y 37 also gives 188. |
| Dirt below grass planes at Y 35/34, air above | 68/120 dirt bytes become grass; every repair is below air and outside the stone-baseline carve mask. |
| Room centers at Y `0,6,9,10,11,127` | Exercise lower/upper clipping and flowing-lava/air outcomes; center Y 6 changes 604 bytes to lava, while Y 127 changes none. |
| Path seeds, widths, step/end and angles | Include seeds 0/1/-1, widths 0/1/3, ends 85/96/112, final step 95, four yaw values, pitch endpoints and signed-zero width/yaw/pitch. Room seeds include both signed-long endpoints. |

The `shared_random_consumption` field reports a continuation-based inference.
After the actual call, its four following longs must match exactly one fresh
JDK Random candidate using the bytecode-permitted primitive draw counts. Each
room matches one `nextFloat` and one `nextLong`. Twelve paths match three
`nextLong` calls; four match one. Three path longs infer the root plus two child
seeds. This is an inference from shared state, not an instrumented branch trace
or a conclusion drawn from the carved shape. Changed-byte counts remain
observations and do not supply an oracle.

The reviewed 44-request stream is 3644 bytes, SHA-256
`be792fc398e8a1e22702b08f7ff6bf96b007bb950e687e61e5550713e60c88b2`.
The full capture's raw stdout is 5800704 bytes per side, SHA-256
`765a965c77b8ec444408b282b1a252b45f3268804868c185a57b559257bb5a88`.
Its manifest SHA-256 is
`4ea867551cfc5902e0da6475a7adb4d3153ab82920d3c0d57d055b74a3a091bd`;
the preceding v2 smoke manifest is
`f03525e6378ae64ea919650a770ee607d0584a538039c21892658e7cf77743b0`.
An independent saved-artifact audit verified 139 unique file hashes across
smoke/full evidence, paired complete records, ordered controls, three helper
classes, preserved inputs and empty working-directory inventories. Its report
SHA-256 is
`17d2e92753ba982bbf0e4e801abf976c6f85a06fe0f7c158423828444d87ba0f`.
These remain original-only observations. The integrated local comparisons below
use fresh original executions; this earlier capture remains an independent
baseline.

Lossless protected fixtures retain 88 full array references, 47 unique arrays,
11548 runs and 352 continuation longs. Independent decoding of the C
initializers matched all 2883848 logical bytes per original side, raw request
bits, parameters and continuation metadata. The concatenated array SHA-256 is
`86f11f40793dd5e1b1d2ff0285f755926b0d4664d36a7da0a4abb0af87505551`.
Initial isolated MSVC AddressSanitizer and GCC/Clang AddressSanitizer plus
UndefinedBehaviorSanitizer builds each pass 2917405 protected-unit checks.
These runs precede the final committed-source whole-project matrix.

## Comparison transport

The optional natural helper is
[`compare_caves.py`](../../scripts/reference/caves/compare_caves.py), with
[`CavesObservation.java`](../../tests/reference/caves/CavesObservation.java) and
[`caves_probe.c`](../../tests/world/terrain/caves_probe.c). The separate protected
helper is [`compare_geometry.py`](../../scripts/reference/caves/compare_geometry.py),
with [`CaveGeometryObservation.java`](../../tests/reference/caves/CaveGeometryObservation.java)
and [`caves_geometry_probe.c`](../../tests/world/terrain/caves_geometry_probe.c).
Their CMake tests, `reference.caves` and `reference.caves_geometry`, are enabled
by `MC_ENABLE_CAVES_REFERENCE_TESTS=ON` and require the private pinned JARs and
a JDK of version 11 or newer. Requests are restricted to the respective reviewed
ten-case and 44-case whitelists.

Each C probe first emits all 65536 actual table floats as big-endian raw bits.
Python hashes those measured bytes and checks the accepted table digest
`87a67a39fff59636acb04edcda4ef622831037291f11fb61586c57596771dac2`.
Only a strict `table_unchanged:true` control maps to that measured digest field.
Every other response field compares unchanged in independent C/client, C/server
and client/server comparisons, with no tolerance or numerical normalization.
The protected helper loads the shared natural utilities and fixture dependency
by compiling their attributed current source bytes; cached bytecode is excluded.

| Limit | Natural | Protected |
| --- | ---: | ---: |
| Child time | 60 seconds | 30 seconds |
| Java heap | 512 MiB | 512 MiB |
| Live stdout / stderr | 64 / 4 MiB | 32 / 4 MiB |
| Single response record | 4 MiB | 512 KiB |
| Genuine world setup | Fresh owned roots | None |

Both helpers retain fresh report/class directories, live output limits,
input/class/executable/artifact hashes and completed-case failure precedence.
Natural setup additionally retains fresh owned world roots and bounded
inventories. Compilation uses Java release 11, strict warnings,
disabled processors/implicit compilation and fresh empty class/source search
paths. Every generated helper class is hashed before and after execution.
The executing helper must resolve to its attributed canonical source path
before report creation; a renamed copy is rejected.

Missing prerequisites and narrowly identified class/world setup failures return
77. The natural helper also recognizes a validated unfinished genuine-world
setup timeout. Numerical differences, method failures, measured-stage timeouts,
resource limits and incomplete controls fail 1. Complete earlier comparisons
are checked before a later unavailable result. Raw partial output and diagnostics
are retained. World inventory polling is not a hard filesystem quota.

The public suites have 18 asset-independent guards each. They cover schema and
table controls, exact source attribution, stale-report replacement, input/work
aliases and actual renamed-script rejection. Natural inventory guards include
the Windows `DirEntry.stat` regression: no-follow `os.stat` accepts an ordinary
file and rejects a verified actual hard link. Numerical acceptance uses the
separate original comparisons below.


## Integrated local acceptance

Exact source [`a507f59d756acf2d035ac7d4ea2da7171f60ad2f`](https://github.com/frankischilling/minecraft-c/commit/a507f59d756acf2d035ac7d4ea2da7171f60ad2f)
passed all 33 configured CTests on each toolchain without skips. All 30 original
comparison manifests, ten per toolchain including prior modules and both cave
comparisons, passed for that same source.

| Local toolchain | CTests | Original JVM | Natural cases | Protected cases |
| --- | ---: | --- | ---: | ---: |
| Native MSVC | 33/33 | Temurin 17 | 10/10 | 44/44 |
| WSL GCC, ASan/UBSan | 33/33 | OpenJDK 21 | 10/10 | 44/44 |
| WSL Clang, ASan/UBSan | 33/33 | OpenJDK 21 | 10/10 | 44/44 |

Every cave comparison passed all three independent pairings: C/client, C/server
and client/server. Fresh original runs retained complete records and controls;
all measured C tables matched the accepted raw digest. The manifests record
preserved source/tool inputs, generated requests, helper classes and artifacts.
The acceptance audit checked the source, probe executables, class files and
artifact identities against those records.

Natural and protected unit suites passed 2021631 and 2917405 checks respectively
on every toolchain. Both public helper suites passed their 18 asset-independent
guards without skips on all three platforms. Formatting passed for all 50 checked
C/header files. A native library build with `BUILD_TESTING=OFF` also passed.

The retained consolidated record is `.local/caves-acceptance.json`, SHA-256
`50c03126e6a55089cf9108ff1562745821bbc4661790915dfb6fd6e71cbe39b9`.
It identifies the exact accepted source and hashes all 30 reference manifests.
The original-only baseline hashes above remain unchanged and separate from this
later C acceptance.

These results cover the bounded normal cave stage, its finite protected adapters
and the configured regression suite. Complete chunk finishing, lighting,
population, other dimensions and save integration remain separate work in the
[broader parity inventory](../parity.md).

## Public integration

The accepted source is integrated at `49f87ef9c803909ca8d89c8efe67a32da648cca1`.
[Exact-head CI](https://github.com/frankischilling/minecraft-c/actions/runs/39043911065)
and [resulting-main CI](https://github.com/frankischilling/minecraft-c/actions/runs/39044349578)
each pass 23 tests on native Windows MSVC and Linux GCC/Clang, without skips,
plus formatting for all 50 C/header files. CI uses no supplied game files.

The independent six-report acceptance audit checks all raw pairings, committed
source objects, 634 current file hashes, process controls and owned inventories.
Its SHA-256 is
`5ef7423834b4b365bb28d2bdbb60bbe07565d1805c787e6b804f096494ef8f34`.
Windows CRLF and Linux LF account for raw stream-size differences; decoded
response content is exact, without numerical normalization.

The [wiki](https://github.com/frankischilling/minecraft-c/wiki) is published at
`be88d6e8231d171c224cdb470199c7af7b41f43a`. All five page blobs match the
versioned wiki sources at the runtime integration. Home, architecture,
build/tests and storage/reference each return HTTP200 with the current cave content.
