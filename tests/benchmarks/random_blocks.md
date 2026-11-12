# Grass and Leaves CPU benchmark

The optional benchmark compares two existing getter contracts inside the actual
Grass and Leaves tick bodies. `rb_get` uses the production Block or metadata
failure helper; the detailed baseline selects the corresponding public detailed
getter. Both execute the same source query and retain complete reached failures.
Public detailed success diagnostics remain intact.

Build both Release executables with optional benchmarks enabled. On Windows,
use a matching MSVC developer shell:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DMC_BUILD_BENCHMARKS=ON
cmake --build build --target mc_benchmark_random_blocks_detailed mc_benchmark_random_blocks_failure
python -B tests/tools/world/benchmark_random_blocks.py --executable detailed=build/mc_benchmark_random_blocks_detailed.exe --executable failure=build/mc_benchmark_random_blocks_failure.exe --mode controls --rounds 1 --artifacts .local/random-block-controls
python -B tests/tools/world/benchmark_random_blocks.py --executable detailed=build/mc_benchmark_random_blocks_detailed.exe --executable failure=build/mc_benchmark_random_blocks_failure.exe --mode measure --rounds 4 --artifacts .local/random-block-cpu
```

On other platforms, omit `.exe`. These commands describe reproduction; the
retained timing qualification is Windows only. Use a new artifact directory for
each attempt. The driver authors the saved Chunk inputs, captures raw output
and state files, checks every process and retains failures.

## Workloads

Each sample genuinely constructs a GAME World and loads its one normal Chunk
through the actual Store and provider. The same Registry and class owners live
across the sample Worlds. Initial World fields and the distinct supplied Random
follow the declared direct-leaf fixture: World.r uses seed 89, the supplied
Random seed 0, remote is false and weather interpolation is zero. These are
pre-entry inputs. Random and source effects continue throughout each sample.

| Mode | Calls per sample | Actual work |
| --- | ---: | --- |
| `grass_bright` | 32768 | Valid Grass at (8,64,8), west Dirt target, bright saved SkyLight above both. Every call consumes the supplied bounded draws in 3/5/3 order. The first spread changes the west target; later calls retain that result. |
| `leaves_flag0` | 32768 | Valid Leaves with metadata flag 8 clear. Each call reads actual metadata and returns without allocating scratch. |
| `leaves_connected` | 512 | Distinct flagged Leaves near a real Log plane, with radius-five presence inside the loaded Chunk. Each call performs the full 729-cell scan and source propagation, then clears its own flag. |

The connected input fills x1..14, z1..14 and y1..26 with Leaves except for a Log
plane at x8. Targets follow y5..22, z5..10 and x=[5,6,7,9,10], taking the first
512 distinct cells. No flag is rearmed. Leaves scratch is allocated by the first
reached source call and retains its actual identity and contents across later
Worlds. The flag-clear mode runs before that allocation.

Controls mode runs one complete unmeasured sample per mode. Measure mode runs
two complete warmups and seven measured samples per mode. Four rounds alternate
the process order and retain eight fresh processes, giving 168 measured samples
and 28 per variant and mode. The existing eighteen original-comparable Block
profiles remain separate correctness checks, including Grass mutation and
callback failure, Leaves allocation and stale scratch, and Ice/Snow effects.
They do not become original observations of the repeated CPU workload.

## Timing and receipts

One compiled [workload object](random_blocks.c) serves both typed adapters.
The timed loop includes real World call-enter, mutation-error initialization,
the selected genuine leaf and call-leave. Input construction, loading, expected
Random replay, checks, snapshots, output and retirement occur outside timing.
The callbacks record events in a prefaulted 4,096-entry memory buffer during the
call and serialize them after the stop clock. Overflow rejects the attempt. The benchmark performs no
stdout or file operation in that timed wrapper.

For every sample, the driver retains full before/after plane and scratch bytes,
ordered light regions, complete scheduled records with identity joins, ordered
callback events and native scalar state. Guards check real owner identities,
all World fields, holds, leases, dirty flags, provider observations and Random
trajectories. The actual positive-SkyLight class flag remains live and is never
restored between source calls. Comparison uses exact bytes and independent
format/shape checks, rather than hashes alone.

Each raw scalar receipt also retains the generator and cave Random triples.
The cave owner is genuinely constructed with default entropy and is not reached
by this cached workload. Both full generation triples must remain unchanged
within each sample. Only the independent cave 48-bit word is normalized across
processes; its Gaussian flag and positive-zero cache, the seeded generator and
every other scalar remain exact. No RNG or clock input is changed to make the
comparison pass. The initial control failure caused by the cave word remains
in the evidence alongside later qualified controls.

The [Windows receipt](random_blocks_windows.json) keeps the final formatted
cohort's 168 measured samples and a separate earlier passing cohort of 168.
They are not pooled or reduced to a passing subset. The build binding records
172 selected product and dependency hashes, the shared workload object, actual
compiler/dependency records and all six private baseline aliases: four leaf
symbol names and the two getter callees. Selected object symbols and final
executables establish which body each typed adapter calls.

The final medians are 3706.097/3366.246 ns per bright Grass call,
1995.621/1841.682 ns per flag-clear Leaves call and
367278.711/229325.781 ns per connected Leaves call, in detailed/failure order.
The reductions from unrounded medians are 9.170%, 7.714% and 37.561% respectively.
The [performance note](../../docs/performance/random-block-ticks.md) explains
the runtime change and scope. Full Windows and Linux builds pass, along with 16
focused tests on each host. Linux uses Clang Debug with ASan/UBSan and leak
checks. The separate six-process frame cohort remains unqualified because two
baseline processes lose foreground validity; every attempt remains retained.
These finite CPU results make no FPS improvement claim or hosted-CI claim.
