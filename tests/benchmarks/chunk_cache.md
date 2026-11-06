The optional EW benchmark measures actual Block reads over nine saved Chunks
loaded through the ordinary source World and ChunkCache constructors. It opens
no display and uses no original assets or JVM.

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DMC_BUILD_BENCHMARKS=ON
cmake --build build-release --target mc_benchmark_chunk_cache_public mc_benchmark_chunk_cache_failure
python -B tests/tools/world/benchmark_chunk_cache.py --executable public=PUBLIC_EXE --executable failure=FAILURE_EXE --rounds 3 --artifacts NEW_PRIVATE_DIRECTORY
```

Both executables share one compiled workload object. Small adapters select the
public or internal failure-only accessor through the same function pointer
call. The public accessor supplies its full success diagnostic; the internal
accessor leaves the caller's error sentinel untouched on success. Both preserve
the original read, admission checks, values, failure output and failure details.

The runner authors deterministic Block, metadata, sky, block-light and height
planes, then independently decodes the saved inputs. Each process checks 1,024
entered coordinates, eight failure cases and five successful guard branches
before measurement. Seven samples each perform 65,536 real reads. Construction,
hashes, assertions, output, failure injection and retirement stay outside timing.
The runner alternates process order and retains every sample and failed attempt.
Use `--mode controls` to run without a timed interval.

Checks cover every returned value, all Chunk planes, complete World/EW owners,
captured occurrence identities, Random state, allocation/hold state and zero
World getter reacquisition. Failure comparisons inspect 754 declared diagnostic
leaves plus their fixed arrays, including strings and nested fields, without
padding. NBT payload unions remain inactive with OK statuses. Every control
also checks the EW owner before and after each call, including active/retiring
refusals. Disposable save files must remain unchanged except for the ordinary
eight-byte session lock.

The [Windows record](chunk_cache_windows.json) keeps the initial 42 samples and
a separate 84-sample confirmation. Confirmation median time falls from 8.39885
to 0.74005 ms per 65,536 reads (91.19%); p95 falls from 9.5065 to 0.9652 ms.
The 7,352-byte public error record requires one full initialization and one full
copy per call with a nonnull error output. Their nominal source-level volume is
14,704 bytes per call. That count does not measure memory bandwidth.

The measured pair changes one cache object in a frozen simulation archive;
the other 238 members and dependency libraries stay identical. Executable,
object, source and authored-input hashes bind the retained samples. The public
Block accessor and shared read body remain unchanged. Published formatting,
include names and additional untimed owner assertions have separate source
bindings; the timed C interval retains the captured tokens. Those additional
assertions pass a separate untimed qualification over the same runtime pair.
The published harness also passes Windows MSVC Release and Linux Clang 18
Debug ASan/UBSan builds and controls without collecting timing samples.

A first public-only attempt stopped at an aggregate continuity guard that used
padding-sensitive structure comparisons. Its binaries, sources and failure
remain private. The qualified harness compares all provider observation fields
and explicitly copies complete owner bytes. It retains every semantic guard
and uses none of that failed attempt's samples.

These results describe finite CPU API work. They do not establish scene FPS,
GPU performance, loading speed, unrestricted play or original-game parity.
