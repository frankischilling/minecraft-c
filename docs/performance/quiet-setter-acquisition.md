# Quiet-setter acquisition diagnostics

The ordinary World setter acquisition helper now avoids initializing a successful acquisition diagnostic that its caller never reads. In the retained native Windows cohort, all eight warmed AIR no-op scenes had lower candidate scene-median elapsed time. The result covers complete public setter calls and their loop work. It establishes no changed-cell, natural-population or gameplay FPS improvement.

The [finite record](../../tests/benchmarks/quiet_setter_acquisition_windows.json) contains both separate smoke observations, every ordinary context/sample/invariant/cleanup record, all 448 cohort timings, logical artifact/hash bindings and the retained qualification history. Private absolute paths are replaced by logical roles. No executable, archive, World image, original class/JAR, asset payload or raw address is published in that record.

## One ordinary acquisition call

The change is one callee in [the World mutation helper](../../src/world/runtime/mutation/world.c):

```diff
-    const mc_chunk_acquire_status acquired = mc_world_runtime_get_detailed(
+    const mc_chunk_acquire_status acquired = mc_world_runtime_get_failure(
         world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), out, &detail);
```

Both internal getters call the same acquisition coordinator with the same World, coordinates, operation and output pointer. Admission, actual provider dispatch, live-cache lookup, pending state, cold load/generation, callbacks, activation/population, output publication and call balancing remain shared. The retained execution branch and the earlier NULL-provider guard are unchanged. There is no public header or ABI change, callback substitution, cached World state or rollback.

The difference is the successful diagnostic-output branch. `get_detailed` initializes the caller's acquisition diagnostic on success; `get_failure` leaves it untouched on success and supplies the same initialized failure detail on non-OK status. This caller returns immediately when acquisition succeeds and reads/copies `detail` only after failure. Its local object's address does not escape into the World. The successful initializer performs diagnostic assignments and nested initializers; it does not invoke Source work or own a resource. The acquisition diagnostic is 4,648 bytes in the measured ABI. That is its representation size, not a claim that a particular 4,648-byte machine instruction was removed.

The captured baseline is commit `986ca4f2719a597f7010c9e769656f0814890449`, with mutation source SHA-256 `88d718ef112b7f091b9c6cf23aff0504d9fed795ee4b2ea3923a5109820fa0ac`. The one-callee candidate is `b3e401beb3f63b5690a8ac3c661a8a444c8ae193b10c9b6489fa0e0ce301eaaf`. The candidate source was promoted after the measurements and functional controls completed. Historical before/after inventories retain their original baseline hashes; they do not assert that the subsequently promoted live source still equals that baseline.

## Fixed work and build ownership

The cohort contains eight scenes: 8 or 256 sparse resident Chunks, manually opened provider mutation depth 0 or 1, and ID-only or ID-plus-metadata quiet setters. Each selected target is AIR with existing zero metadata, so the actual setter must return false. Every 32nd call adds 524,288 block units to X, exercising the existing packed-key Chunk alias. No complete population-neighbor square is prepared.

Each fresh process warms both public setter bodies at every coordinate, then records seven samples of 8,192 calls. Every timed call resets `changed=true` before calling the actual setter. The QPC interval includes coordinate/alias selection, that sentinel store, status/changed accumulation and checksum folding. It excludes fixture construction, warm-up, diagnostic comparison, state snapshots and cleanup. It is elapsed time, not API-only cost or process CPU time.

One compiled V3 workload object is shared by both products: SHA-256 `aed495f17372938d94743b3218f62040b734766a4df3d64c650166cda8c550a0`. The separately compiled genuine World mutation objects are `7d2c1adcf64760ea33e01216e03bbe15bffdf8dbc9949d849f3d0fa22b5751eb` and `400988e9ace019fe966ecd4df77c96f21b182d7a1f7bb6c1460571d2049f5bbc`. Both links use the same ordered thirty-archive tail and system-library roles. No archive member is rewritten. The 776-byte V3 clock-start/loop source span is `b3034b29935d2e89a77968c7884b9e70f99c7c5e1f3d6779037823182f12156e`.

COFF external-definition and actual link-map ownership checks identify 37 definitions in each explicit mutation object. All 28 retained mutation exports come from the intended explicit object, with nine definitions dead-stripped and zero retained mutation exports supplied by the archive. Other retained `world.c.obj` symbols belong to the separate power module. This is function ownership evidence, not an inference that every archive member named `world.c.obj` was physically excluded.

The host reports an Intel Core i9-10900F, ten cores/twenty logical processors, 51,403,378,688 physical-memory bytes and Windows build 19045. The build uses MSVC toolset 14.51.36231 x64, `/O2 /Ob2 /W4 /WX /fp:strict`, C17 and the recorded runtime/atomics flags. Tool and library bytes are bound in the record. This does not establish fixed CPU frequency or absence of host scheduling effects.

## Retained elapsed observations

Two new V3 baseline/candidate smoke processes passed before the separate cohort. Their fourteen timings are retained separately and are not included in the cohort's 448 samples.

The cohort has four rounds and 64 fresh processes: four processes per variant per scene, seven samples per process, and 3,670,016 timed calls. Scene order is fixed lexicographically. For round `r` and scene ordinal `s`, the pair is baseline/candidate when `(r+s)` is even and candidate/baseline otherwise. Each scene therefore has two pairs of each order. There are no retries or omitted samples.

A process value below is the median of its seven raw elapsed samples. A scene/variant value is the median of its four process values. The ratio is baseline divided by candidate.

| Chunks | Depth | Setter | Baseline ns/call | Candidate ns/call | Baseline/candidate |
| ---: | ---: | --- | ---: | ---: | ---: |
| 8 | 0 | ID | 3823.694 | 3633.765 | 1.052268 |
| 8 | 0 | ID + metadata | 3921.857 | 3684.241 | 1.064495 |
| 8 | 1 | ID | 3877.960 | 3690.875 | 1.050689 |
| 8 | 1 | ID + metadata | 3892.657 | 3665.002 | 1.062116 |
| 256 | 0 | ID | 3912.164 | 3549.792 | 1.102083 |
| 256 | 0 | ID + metadata | 4113.324 | 3698.444 | 1.112177 |
| 256 | 1 | ID | 3646.960 | 3477.930 | 1.048601 |
| 256 | 1 | ID + metadata | 4061.792 | 3696.234 | 1.098900 |

These scene values represent 4.635%–10.086% lower candidate elapsed time for this fixed work. The candidate is faster in 29 of 32 adjacent same-scene/same-round process-median pairs. Three paired regressions remain included:

| Round | Chunks / depth / setter | Process indices | Candidate slower |
| ---: | --- | --- | ---: |
| 0 | 8 / 1 / metadata | 6, 7 | 0.213% |
| 2 | 256 / 0 / ID | 40, 41 | 3.836% |
| 3 | 8 / 0 / ID | 48, 49 | 0.863% |

Both pair-order strata favor the candidate overall, with different observed magnitudes. Baseline-first pairs have median ratio 1.090431 and geometric mean 1.080790; candidate-first pairs have median 1.047353 and geometric mean 1.051739. Fifteen of sixteen baseline-first pairs and fourteen of sixteen candidate-first pairs favor the candidate. The balanced design does not prove that order or other host effects are absent.

All raw timing extremes remain in the record. The fastest sample is 3335.742 ns/call; the slowest is 4607.190 ns/call. The slowest candidate sample is 4319.897 ns/call. No post-hoc outlier threshold or favorable subset was used.

## Diagnostic representation and failed preparations

V1 failed workload compilation with C2084 because a nested helper removed the attempted `main` macro rename. It produced no native benchmark timing. V2 used the corrected helper and linked both products, but its first baseline smoke failed the initial raw two-poison diagnostic comparison before any setter or timed loop. Its candidate was not executed. Both failures remain retained.

A separate pure initializer unit then made four calls to the unchanged initializer, without constructing a Source World or measuring time. Its ordinary two-poison pair reported 55 differing positions; a separate stack-tainted pair reported 212. All observed differences fall in proven padding. There were zero named-member and zero unknown-region differences. This separate unit cannot recover the earlier smoke's unrecorded differing bytes or retrospectively prove their exact classification.

The reviewed partial layout covers a 24,296-byte mutation diagnostic: 6,298 named bytes, 1,650 proven-padding bytes and 16,348 unknown bytes. V3 excludes only the proven padding and compares/hashes every other 22,646 bytes, including every unknown byte. It checks the exact size/counts and full mask fingerprint `8267465906294600077` before work begins. No result bytes are normalized. A different layout or differing unknown region fails this finite guard; a complete portable semantic serializer is not claimed.

Both poisoned warm setters per coordinate and every sample's final diagnostic must match the canonical initializer under that rule. The workload also checks complete live Chunk planes and captured state, World/Math/Random state, light-queue descriptors/backing/regions, cache/pending storage and ownership/scope cleanup. The emitted output fold is `15416205869579715459`; the masked canonical diagnostic digest is `14270828727553165931`. The exported receipt does not expose every diagnostic or live object byte, so the Python review independently recomputes the output fold and verifies the diagnostic digest/provenance rather than independently reconstructing the unexported object graph.

All 64 processes pass their before/after Source/product guards, authored-file continuity and cleanup. The 6,708 input roles include overlapping old 433-file, initializer 181-file, new 358-file and published-source 6,232-file inventories. Actual World files are unchanged except for the permitted eight-byte session token. Before-process host receipts satisfy the declared 512 MiB available-physical and 1 GiB available-commit floors. These continuity and headroom checks do not prove paging absence or whole-program equivalence.

## Functional qualification and the path confound

The first existing mutation/query/ring qualification remains **FAIL**. Its baseline mutation fixture passed, while the candidate failed save/drain assertions. The retained baseline temporary path was 259 UTF-16 code units excluding NUL; the candidate path was 260 because its variant directory name was one character longer. The getter comparison therefore also changed a filesystem input.

A declared four-case discriminator reused both unchanged mutation executables, the same test object and the unchanged public driver, changing only fresh work roots. All 6,341 input pins remained unchanged. The results were:

| Executable | Region / temporary path units | Native result | Region bytes |
| --- | --- | --- | ---: |
| Baseline, short root | 134 / 158 | All 218 checks pass | 12288 |
| Candidate, short root | 134 / 158 | All 218 checks pass | 12288 |
| Candidate, long root | 236 / 260 | Same save/drain failure | 8192 |
| Baseline, long root | 236 / 260 | Same save/drain failure | 8192 |

The two short native outputs and saved regions match exactly. The public Python driver outputs include their distinct retained report paths. Both long cases fail the same assertions at the frozen fixture's lines 684 and 818 and retain the same zero-header prefix as the original failed candidate. This supports the path factor for this fixture on this host; it does not identify an observed OS/CRT error code, prove the precise failing API call, fix long-path behavior or relabel the first failed qualification as passed. The new short case is not a new test at 259 units.

Separately, both query variants and four client/server ring native replays passed. Together with the matched-short mutation pair, these provide eight passing normal native control observations. Ring numerical replay is functional evidence, not population timing.

The final ten fresh setter controls also passed, with exact ordinary native bytes matching across variants for each of five scenarios:

| Scenario | Compared result/prefix in both variants |
| --- | --- |
| Cold quiet ID no-op | OK, `changed=false`, neutral carried children, cleanup |
| Cold quiet ID + metadata no-op | OK, `changed=false`, neutral carried children, cleanup |
| Cold notifying no-op | OK, `changed=false`, neutral carried children, cleanup |
| NULL provider | Existing failure, World PC48/provider PC6, caller Boolean remains true |
| Registered entity-added observer failure | One actual callback, activation PC42, World/publication PC34, observer PC29, batch index0, native code71, caller Boolean remains true |

The callback case follows the actual activation/publication failure prefix; it is not a fabricated Java exception or a rollback of published effects. The available public mutation diagnostic does not expose every private child field. All ten processes finish their declared cleanup, leave authored World files unchanged except session tokens, and pass their recorded before/after Source/product inventories.

After promotion, the focused Windows mutation, query and natural-ring checks passed 3/3 in 12.94 seconds. The complete strict MSVC Release build then passed 441 steps, and all 1,270 default CTests passed in 525.01 seconds with four jobs, no skips or duplicate/missing names, and all 6,238 frozen inputs unchanged. Three focused Clang18 Debug tests passed in 19.16 seconds with ASan, UBSan and leak detection; the changed World mutation unit also compiled under strict GCC13. The Linux source snapshot retained the same 6,238 input files. This is focused Linux qualification.

Both optional public benchmark targets compile and link under strict MSVC Release settings. Two fresh quiet-setter semantic smokes pass, covering both setter forms at 8/256 Chunks and depths 0/1. Fifteen synthetic reader and failure-retention methods pass normally and with Python optimization, including raw Windows CRLF hash continuity and simultaneous native/file failures. Formatting passes all 2,410 C files and fragments.

The public harness derives from the measured V3 workload through reviewed include/entry bindings, header/comment changes and reversible clang-format 18.1.3 formatting. Its complete raw body bytes differ; both bodies match under the same formatter, and the 776-byte timed span remains exact. Its separate current-product build/smoke provenance supplies no comparative timing credit and does not replace this historical two-variant cohort. Result annotations, documentation and current-source fingerprints were refreshed after the completed local checks; runtime and harness payloads remain unchanged.

The result is bounded to the actual ordinary setter call and these native controls. No complete original-program parity, natural-population speedup or gameplay FPS gain is claimed.

## Reproducing one current scene

Configure with `-DMC_BUILD_BENCHMARKS=ON -DBUILD_TESTING=ON`, then build the optional `mc_benchmark_quiet_setter_noop` target. With a configured Release build, one Windows invocation is:

```powershell
cmake --build build --target mc_benchmark_quiet_setter_noop
python -B scripts/benchmarks/quiet_setter_noop.py --executable build/mc_benchmark_quiet_setter_noop.exe --artifacts C:/bench/quiet-setter --chunks 256 --depth 1 --setter block
ctest --test-dir build -R "tools[.]quiet_setter_noop" --output-on-failure
```

Adjust the build/configuration and executable location for the configured generator. Choose a writable, short Windows artifact root; the retained retirement experiment shows why differing long work roots can confound a functional comparison on this host. The runner creates fresh authored inputs, retains complete raw captures and before/after file/input hashes, and never retries a failed process. Supported scene choices are chunks 8 or 256, depth 0 or 1, and setter `block` or `metadata`.

The normal and optimized `tools.quiet_setter_noop` CTests are Python reader controls. The optional native timing process is not added to default CTest runs. One runner invocation measures one current executable; it cannot establish a revision speedup or replace a separately balanced two-product cohort.

The benchmark retains the explicit measured ABI/layout guard: 24,296 mutation-diagnostic bytes, 6,298 named / 1,650 proven-padding / 16,348 unknown bytes, and mask `8267465906294600077`. All 22,646 non-padding bytes remain compared. Include relocation and formatting do not broaden this contract: a different layout or failed canonical comparison is rejected, and a different compiler/ABI is not qualified merely because it can compile the target.
