# Native frame baseline

The [Windows record](../../tests/benchmarks/client_frame_baseline_windows.json)
contains one fresh native process in the authored Tiny sky-view scene. It uses
the existing [frame observer](../../scripts/benchmarks/client_frames.py), strict
MSVC Release libraries, 854x480, a saved rotation of 180/-30 and ordinary active
simulation. Two seconds of natural warmup precede a 10.0085-second interval.
Physical DISPLAY2 supplies the startup usable-bounds hint; this observer does
not emit a separate post-creation window-geometry receipt.

| Measurement | Observed value |
| --- | ---: |
| Complete frames | 374 |
| Measured FPS | 37.37 |
| Frame p50 | 20.83 ms |
| Frame p95 | 63.34 ms |
| Frame p99 | 134.60 ms |
| Longest frame | 228.71 ms |
| Process CPU time | 10.625 s |

All 374 frames return with retained camera pose, valid foreground and complete
World drawing. The World advances 194 ticks, completing 5,602,720 random block
samples and 194,000 random-display samples. Source and imported assets remain
unchanged during the process. Source saves and normal runtime work remain
enabled; no per-frame screenshots, disk hashing or test assertions run inside
the interval. Reference JVM launches are held while this process runs.

The inclusive tick-batch span averages 8.55 ms per frame, with a 216.23 ms
maximum. The render/update prefix averages 18.16 ms, including 17.66 ms before
the HUD boundary. These host-clock spans include their normal light, Chunk,
render and pacing work. They are not GPU timers or isolated function costs.
No clock overhead is subtracted.

This is a diagnostic baseline, with captured source and library hashes for the
uncommitted runtime. It compares neither implementations nor original-game
frames, and it does not establish an FPS improvement. One process does not
characterize unrestricted play, Far distance, other hardware or frame-time
stability. Earlier rejected frame cohorts retain their own limits in the
[Chunk-rendering record](chunk-rendering.md).

## Complete frames with authored assets

The observer can generate its own textures, fonts and statistics inputs
for the saved World fixture. In an x64 MSVC developer shell with Ninja,
build the SDK target and run one observation on physical DISPLAY2.

```powershell
cmake -S . -B frame-sdk -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DMC_BUILD_CLIENT=ON -DMC_BUILD_CLIENT_AUDIO=ON
cmake --build frame-sdk --target mc_test_workbench_open_shutdown
python scripts/benchmarks/client_frames.py --build frame-sdk --out frame-builds --authored-assets --artifacts frame-runs --rounds 1
```

Use the same observer source and build configuration for both
executables in a comparison.

```powershell
python scripts/benchmarks/client_frames.py --executable before=PATH --executable after=PATH --authored-assets --artifacts frame-runs --rounds 4
```

The report's `frame_interval` measures time from one returned frame to the
next. The first interval starts at the measurement boundary. These intervals
cover the full elapsed time used to compute FPS. Their percentiles and slow
frame counts include delays between callbacks, which `between_frames` reports
separately. `host_frame` measures the time spent inside each frame callback.
Both span host time, including scheduling delays.

Two groups of four alternating Windows Release pairs compared the light
and collision changes. The baseline relinked `queries.c`, `store.c`,
`boxes.c` and `entities.c` from `c34a7c23`. Both builds shared the
observer and the other locally built dependencies. All 16 runs matched
the fingerprints of 28 asset files and 11 save files, the camera pose,
the GPU context and Tiny distance at 854×480.

Every process completed focused, screen-free World frames and advanced
the simulation by 200 or 201 ticks. The measurements follow ordinary
real-time simulation from the same starting scene. The table reports
the medians of each process's frame statistics.

| Windows group | FPS before / after | Frame p95 before / after | Frame p99 before / after |
| --- | ---: | ---: | ---: |
| First four pairs | 108.7 / 123.0 | 17.37 / 15.96 ms | 32.92 / 31.21 ms |
| Four repeats through `--authored-assets` | 121.8 / 129.0 | 16.88 / 16.21 ms | 30.53 / 30.71 ms |
| All eight pairs | 114.8 / 129.0 | 17.16 / 15.96 ms | 32.60 / 31.21 ms |

FPS rose in seven pairs. Frame p95 fell in all eight, and p99 fell in six.
The fixture starts with empty saved Entity lists. The
[crowded collision workload](mob-tick-diagnostics.md#nearby-list-reads-during-collision-queries)
and [fixed light drains](world-queries.md#saved-light-reads-after-acquisition)
exercise the two changes independently.

## Complete frames at Far distance

Add `--view-distance far` to run at Far distance. The runner writes the
setting before startup and checks the loaded setting against the request.
The authored save includes 1,369 populated Chunks from −18 through 18 on
each axis. Tiny keeps the existing 289-Chunk fixture. Each cohort uses
one distance, and qualification requires matching saves, camera and
graphics settings across its processes.

```powershell
python scripts/benchmarks/client_frames.py --executable current=PATH --authored-assets --view-distance far --artifacts far-runs --rounds 3
```

One Windows Release smoke run at 854×480 on DISPLAY2 completed 656 frames
and 200 World ticks, measuring 65.52 FPS. The live Provider held 413
Chunks at the end of warmup and 549 at the end of timing.

Three separate runs used host-clock measurements around the display
update, World drawing, Light drain and section compile calls. Each
run returned 200 or 201 World ticks and 2,125–2,147 section compile
calls during the measured frames.

| Time per frame | Range of medians across three profiles |
| --- | ---: |
| Complete frame | 14.84–14.97 ms |
| World drawing, including section compiles | 13.41–13.65 ms |
| Section compiles | 8.69–8.80 ms |
| SDK display update | 0.29 ms |
| Light drain | 0.001 ms |

Section compiles took 8.96–9.06 ms at the median on zero-tick frames and
4.45–4.51 ms on tick-bearing frames. This profile covers the stationary
save while its renderer builds sections over successive frames.

### Compile and dirty-queue timing during movement

Two Windows traces at `de712fda` measured ten seconds of forward movement at
854×480 with authored assets. Each returned 200 World ticks and completed the
normal pause-menu save. Nested timers measured section compilation separately
from the rest of the dirty-queue update. All 4,376 frame rows matched their
timing and work records.

| Warmup | Frames | Median frame | Median section compile | Median update outside compile | Section compile p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 2 s | 719 | 13.805 ms | 7.689 ms | 0.155 ms | 8.855 ms |
| 25 s | 3,657 | 2.003 ms | 0.000 ms | 0.003 ms | 8.331 ms |

The early run spent most of its median frame in section compilation. After
25 seconds the median frame compiled no sections, and occasional compilation
still appeared in the slowest frames. The uninstrumented early control
completed timing and movement, then missed the save confirmation. These
traces locate the cost inside the update and supply no speedup comparison.

### Block descriptions during section compilation

Pass selection now supplies its Block description to the renderer for the
same Block. The renderer still captures the type before updating the Block's
bounds. Independent render calls get their description through the existing
dispatch.

Eight alternating Windows Release pairs compiled authored 15×16×15
sections with flat lighting, at 21 measured compiles per fixture and
process. Timings cover the compile call. Pass counts, vertex counts and
hashes of the complete vertex batches matched.

| Authored section | Median before / after | p95 before / after |
| --- | ---: | ---: |
| 64 solid Blocks | 92.8 / 92.6 µs | 95.3 / 95.5 µs |
| Filled with solid Blocks | 2.424 / 2.425 ms | 2.636 / 2.516 ms |
| Alternating solid Blocks and Ice | 8.460 / 8.369 ms | 9.206 / 8.961 ms |

Two groups of four alternating Far-distance pairs used the same observer,
save and textures. The first tested the render-description change in a
prototype, and the repeat compiled the production source. All 16 runs
returned complete focused frames and advanced World time by 200 or 201
ticks. The table shows the median of the per-process statistics.

| Windows group | FPS before / after | Frame p95 before / after | Frame p99 before / after |
| --- | ---: | ---: | ---: |
| First four pairs | 66.68 / 68.44 | 16.54 / 16.07 ms | 17.75 / 17.14 ms |
| Four repeats | 68.20 / 69.21 | 16.16 / 16.00 ms | 17.38 / 17.10 ms |
| All eight pairs | 67.93 / 69.21 | 16.25 / 16.00 ms | 17.56 / 17.10 ms |

FPS rose in seven pairs, while p95 and p99 fell in six. The median of
each run's longest frame rose from 19.48 to 21.09 ms.

### Longest frames while sections compile

Four further pairs timed both renderer revisions around the World draw,
section update, section compile, Display update and retirement calls.
All eight intervals used the same authored Far save and advanced 200
World ticks. The median of each process's longest frame went from
18.26 to 19.07 ms in this instrumented cohort.

The longest earlier frame took 21.20 ms with no World tick, and two
section compiles took 14.71 ms of that frame. The longest later frame
took 21.79 ms. Its single tick batch took 5.30 ms, and the World draw
took 15.58 ms, including one 9.51 ms section compile.

Across 5,481 measured frames, previous-frame draw retirement took at
most 0.070 ms. Section-child retirement took at most 0.064 ms in a
frame, and Display list publication at most 0.020 ms. Each measurement
uses the host clock. The frame and World-draw spans include nested work.

Three profiles of the later revision also timed individual World tick
calls. The median tick batch took 5.21–5.43 ms on frames that ran a tick.
Random block work took 2.62–2.72 ms at the median, and spawning took
1.80–1.86 ms. The longest frame in those profiles took 24.67 ms,
including an 8.92 ms tick batch and a 14.40 ms World draw. Within that
tick, random block work took 4.14 ms and spawning took 3.17 ms.

### Renderer source-frame updates

The Block renderer now inlines its two small operations for recording
the current program counter and popping a completed call frame. The
same checks protect an empty or failed frame stack.

Eight alternating Windows Release pairs measured 168 compiles per
fixture and variant. The authored sparse section went from 92.6 to
91.7 µs at the median. The solid section went from 2.418 to 2.401 ms,
and the mixed solid-and-Ice section went from 8.471 to 8.184 ms.
Pass counts, vertex counts and hashes of the complete vertex batches
matched for all three fixtures.

Two groups of four alternating Far frame pairs used the same stationary
authored save and graphics settings. The second group built the
production source. All 16 runs finished and advanced 200 or 201 World
ticks.

| Windows group | Median FPS before / after | Frame p95 before / after | Frame p99 before / after |
| --- | ---: | ---: | ---: |
| First four pairs | 68.79 / 69.66 | 15.99 / 15.86 ms | 17.12 / 16.95 ms |
| Four repeats | 68.61 / 69.44 | 16.05 / 15.90 ms | 17.25 / 17.53 ms |
| All eight pairs | 68.67 / 69.44 | 16.05 / 15.89 ms | 17.25 / 17.17 ms |

FPS rose in seven pairs, p95 fell in six, and p99 fell in three.
Across all eight pairs, the median of each run's longest frame
fell from 20.11 to 19.50 ms.

### Spawn candidate indexing trial

The trial reused the random Chunk tick's call-local coordinate index during
spawning. It stored ledger positions, preserved Set traversal and version
checks, and scanned the ledger above 512 entries or when a probe ran out.
The trial replaced two objects from baseline `3a9f04bb` in an MSVC Release
client build with the standard frame observer.

Eight alternating pairs measured nine authored spawn cases per process. The
40 resident samples per build rejected spawn predicates. The 32 cold samples
per build included Chunk acquisition and publication. Candidate visits, Random
draws and final spawning Random state, constructor and rejection counts, and
publications matched across all pairs. Each process also checked its later
random-block work independently.

| Spawning workload | Median before / trial | p95 before / trial |
| --- | ---: | ---: |
| Resident Chunks, rejected predicates | 0.785 / 0.753 ms | 0.854 / 0.805 ms |
| Cold acquisition and publication | 59.27 / 59.54 ms | 62.19 / 63.29 ms |

Two groups of four alternating Far pairs used the same authored save, textures,
854×480 viewport and fixed camera on `DISPLAY2`. All 16 runs passed the observer
checks and advanced 200 World ticks. The table reports the median of each
per-process statistic.

| Windows group | FPS before / trial | Frame p95 before / trial | Frame p99 before / trial | Longest frame before / trial |
| --- | ---: | ---: | ---: | ---: |
| First four pairs | 68.23 / 67.94 | 16.14 / 16.30 ms | 17.06 / 17.31 ms | 17.94 / 21.11 ms |
| Four repeats | 68.11 / 67.94 | 16.20 / 16.27 ms | 17.36 / 17.48 ms | 19.62 / 20.81 ms |
| All eight pairs | 68.18 / 67.94 | 16.17 / 16.30 ms | 17.12 / 17.43 ms | 18.62 / 20.85 ms |

Four more pairs ran the same baseline binary under both labels. Median FPS
was 68.03 and 68.07. Individual runs ranged from 66.54 to 68.73 FPS, and
their longest frames ranged from 18.47 to 22.86 ms.

World tick batch p99 fell from 6.73 to 6.38 ms across the eight pairs. The
pre-HUD span p99 rose from 16.05 to 16.30 ms. Two trial runs peaked on a
frame with zero World ticks, and the pre-HUD span accounted for over 97% of
both frames. That span includes draw, display update and pacing. The cause of
the longer tails remains uncertain. The index remains a prototype.

### Far draw, display swap and pacing

Four alternating pairs on Windows measured `21682c1e` with the standard
observer and a diagnostic build. Both builds used the same MSVC Release
inputs, authored Far save, procedural textures and 854×480 fixed view on
`DISPLAY2`. All eight runs passed focus, camera and World tick checks. The
diagnostic build collected host-clock spans in memory and wrote them after
timing. Median FPS was 68.89 with the standard observer and 69.77 with the
diagnostic build. This comparison checks the profiling setup.

| Host time in 2,788 profiled frames | Median | p99 | Maximum |
| --- | ---: | ---: | ---: |
| World draw | 12.97 ms | 15.32 ms | 21.44 ms |
| Section updates, including compiles | 8.49 ms | 10.22 ms | 15.22 ms |
| Section compiles | 8.35 ms | 9.78 ms | 14.78 ms |
| SDK display update | 0.269 ms | 0.673 ms | 1.708 ms |
| `SDL_GL_SwapWindow` | 0.021 ms | 0.295 ms | 1.070 ms |
| Display messages and input processing | 0.009 ms | 0.013 ms | 0.086 ms |

The longest frame took 22.16 ms. It ran no World tick and spent 14.78 ms
in eight section compiles. The swap took 0.026 ms. In the longest frame
with a World tick, the tick took 7.73 ms and one section compile took
4.41 ms of the 18.91 ms frame. The spans are nested host measurements.
The swap presented the preceding frame before the current World draw.
Every profiled frame ran with performance mode 1, which sets the section
update deadline. The World-render and inactive sleep paths were never called.

Two 45-second profiles of the same scene each completed 900 World ticks.
The last section compile started 20.78 seconds into timing in both runs.
Between seconds 25 and 45 the renderer compiled no sections. In those
five-second windows, median frame time ranged from 2.11 to 2.14 ms and
p99 ranged from 7.75 to 8.13 ms. The default benchmark window samples the
initial section build in this scene. After settling, every frame at or above
p99 included a World tick. The two longest measured frames in this period
took 12.38 and 11.31 ms, including tick batches of 7.01 and 8.47 ms.

Use a longer warmup when measuring the settled view. A freshly built observer
accepts 2 to 90 seconds. The benchmark checks the warmup duration in its
receipt before comparing the ten-second timed intervals.

```powershell
python scripts/benchmarks/client_frames.py --executable current=PATH --authored-assets --view-distance far --warmup-seconds 25 --artifacts far-settled --rounds 3
```

Two runs with the 25-second warmup measured 405.65–409.20 FPS with p99
frame times of 8.17–8.19 ms. Both timed intervals ran 200 World ticks.

### Random block ticks after Far settles

The [random block sample changes](world-queries.md#random-block-sample-dispatch)
were compared in four Windows Release pairs with the same authored save,
textures, 854×480 view and 25-second warmup. The executables shared the
observer and replaced only the scheduler and Registry runtime objects.
Every measured interval completed 200 World ticks with the camera fixed.

| Median of per-run statistics | Before | After |
| --- | ---: | ---: |
| FPS | 407.88 | 415.59 |
| Frame p50 | 2.103 ms | 2.132 ms |
| Frame p95 | 4.141 ms | 3.518 ms |
| Frame p99 | 7.945 ms | 6.785 ms |
| Longest frame | 11.144 ms | 9.897 ms |

FPS, p95, p99 and the longest frame improved in all four pairs. Frame p50
rose in all four. This cohort measures the stationary Far scene after the
initial section queue has drained. Every reported FPS string counted zero
Chunk updates.

Three repeats used the compiled production objects. Median FPS rose from
378.87 to 380.21, and median p99 fell from 9.11 to 7.36 ms. Frame p50 rose
from 2.191 to 2.253 ms. Four runs reported Chunk updates in the first FPS
string and every later string reported zero. Across the last eight seconds
of each run, median p99 fell from 9.12 to 7.30 ms, with lower p99 in all
three pairs.

Three instrumented pairs timed 600 ticks per build in the same scene. Median
tick batch time fell from 5.226 to 4.165 ms. The random block phase fell from
2.645 to 1.583 ms, and its sample loops fell from 2.296 to 1.233 ms. Those spans
are nested. Spawning measured 1.820 and 1.834 ms. The longest instrumented
frame rose from 11.255 to 13.639 ms, with 4.389 ms in its tick batch.

Four additional pairs used the default two-second warmup and included the
initial section work. Median FPS rose from 68.55 to 69.98 and median frame
p99 fell from 17.56 to 16.78 ms. Both improved in all four pairs. The longest
frame improved in two pairs and rose in two.

### Mob spawning position reads

Three Windows profiles at `585897da` captured 601 World ticks after a
30-second Far warmup. The camera stayed fixed and the FPS strings all
reported zero Chunk updates. The timers bracket individual calls and
contribute to the absolute timings. Position checks are part of trials
and include solid-support and material reads.

| Work per tick | Median calls | Median host time |
| --- | ---: | ---: |
| Spawning | 1 | 2.158 ms |
| Biome lookup | 867 | 0.423 ms |
| Weighted spawn entry | 867 | 0.088 ms |
| Spawn position checks | 3,480 | 0.821 ms |
| Solid-support reads | 4,443 | 0.972 ms |
| Material reads | 486 | 0.053 ms |

The solid-support count includes the initial candidate filters as well as
the trial positions. These measurements overlap within the scheduler.

Eight Windows Release pairs compared a lighter solid-support read in the
authored scheduler cases. It uses the Block array captured before acquisition
and copies a diagnostic record to the spawning caller on failure. The measured
prefixes matched in candidate visits, Random draws and end state, constructors,
rejections and publications.

| Spawning workload | Samples per build | Median before / trial | p95 before / trial |
| --- | ---: | ---: | ---: |
| Resident Chunks, rejected predicates | 40 | 0.800 / 0.767 ms | 0.884 / 0.910 ms |
| Cold acquisition and publication | 32 | 60.37 / 59.57 ms | 63.77 / 61.67 ms |

Four Far pairs with separate builds reduced the median World tick batch from
4.230 to 3.996 ms, with a decrease in every pair. Median FPS went from 387.56
to 377.41 and frame p99 went from 7.62 to 7.69 ms. FPS and p99 improved in two
pairs each. The median frame with no World tick rose from 2.192 to 2.264 ms.

Three more pairs used a byte-identical executable that selected the
solid-support path only for spawning. Median tick batches fell from 4.198 to
3.955 ms, and frame p99 fell from 7.86 to 7.25 ms. Both fell in every pair.
Median FPS was 388.81 with the original path and 388.78 with the trial path.
All reported FPS strings in both cohorts counted zero Chunk updates.

Two repeats used normal CMake Release libraries. Median Far FPS rose from
385.87 to 391.73, frame p99 fell from 7.59 to 7.24 ms, and the median tick
batch fell from 4.214 to 3.914 ms. Both pairs improved on those three
metrics. All reported FPS strings counted zero Chunk updates. The native
controls checked failure records and the spawning caller's error buffer on
success. A callback test replaced the Block array during acquisition.

### Single-point Biome sampling

The [octave sampler](../world/simplex.md) handles a 1×1 fill without going
through the nested grid loops for each layer. Spawning still goes through the
World manager's ordinary chunk lookup and updates its climate arrays.

Three Windows MSVC Release processes per build ran the native World manager
fixture with a warmed registry and single-element climate buffers. Each
sample repeated 867 lookups 220 times and divided the host time by 220. The
coordinates covered three 17×17 chunk squares centered at `(0, 0)`,
`(29, -31)` and `(58, -62)`. Each process first hashed the three climate
values and the returned Biome kind at every coordinate. All hashes matched.

Three more processes per build first grew the climate buffers to 256 elements.
The same lookup hashes matched, and every reused climate buffer had a
positive-zero tail after each checked lookup.

| Time per 867 lookups, 27 samples per build | Median before / after | p95 before / after |
| --- | ---: | ---: |
| Single-element buffers | 0.379 / 0.339 ms | 0.415 / 0.366 ms |
| Reused 256-element buffers | 0.411 / 0.372 ms | 0.438 / 0.400 ms |

Four alternating Far pairs used the same Windows Release observer and
libraries with only the simplex object replaced. All eight runs used the
authored 854×480 scene and fixed camera on `DISPLAY2`. After the 30-second
warmup, each run advanced 200 World ticks and every FPS string reported
zero Chunk updates.

| Median of per-run statistics | Before | After |
| --- | ---: | ---: |
| FPS | 390.14 | 390.00 |
| Frame p50 | 2.183 ms | 2.208 ms |
| Frame p95 | 6.040 ms | 5.985 ms |
| Frame p99 | 7.647 ms | 7.241 ms |
| Longest frame | 11.755 ms | 10.206 ms |
| World tick batch p50 on tick-bearing frames | 3.972 ms | 3.862 ms |

Frame p99 and World tick batch p50 fell in three of the four pairs. Median
FPS was nearly level. Two repeats used normal CMake Release libraries, with
only `mc_world.lib` changing. Both repeats lowered frame p99 and tick batch
p50. Their median frame p99 went from 7.621 to 7.524 ms and tick batch p50
went from 4.004 to 3.890 ms. Median FPS went from 393.25 to 391.56, and
the median longest frame rose from 10.175 to 10.582 ms. The frame comparisons
cover one settled scene.

### Region header reads and writes

The live save handler reads each region's two 4 KiB header tables in blocks.
Creating a header writes two 4 KiB sectors. A new-region control counted
8,192 reads and 8,192 writes before the change and two of each after it.
Both builds transferred 8,192 bytes in each direction.

Sixteen alternating Windows Release pairs ran `mc_test_source_regions`
against fresh copies of the same fixture. Each process opened a stored region,
read records, observed an in-place payload update, flushed the cache, and
created an empty region. Elapsed time includes process startup. The median
fell from 63.06 to 5.96 ms, with a decrease in all sixteen pairs. Console
output and the resulting region files matched byte for byte.

Three alternating rounds on an i9-10900F and RTX 3060 compared the baseline,
the header and Chunk-cache scan fixes, and those fixes with a reusable vertex
conversion buffer. All nine runs matched the authored Far scene at 854×480
and returned 200 World ticks after the two-second warmup. The table shows
the median of each run's statistics.

| Build | FPS | Frame p95 | Frame p99 | Longest frame |
| --- | ---: | ---: | ---: | ---: |
| Baseline `9530fac5` | 72.33 | 15.59 ms | 16.55 ms | 17.19 ms |
| Header and Chunk-cache fixes | 72.24 | 15.32 ms | 16.42 ms | 17.93 ms |
| With conversion-buffer trial | 71.02 | 15.60 ms | 16.52 ms | 17.84 ms |

The conversion-buffer trial removed allocations on repeated draws while
producing identical upload bytes. It measured lower FPS in all three
rounds than the fixes alone and was dropped. One run in the earlier cohort
overlapped a source-input edit, so that cohort did not qualify. The header
timing targets region acquisition and does not establish a frame-rate gain.

## Fixed-input observer

Build both comparison executables with the same observer source. The runner
selects physical `DISPLAY2` before launching and checks the bounds of the
Frame and Canvas before world selection and after timing. It alternates the
executable order and retains every attempt.

The observer drops nonzero mouse deltas during setup, then filters gameplay
keyboard and mouse events during warmup and timing. Window focus and close
events still reach the client. Normal tick, draw and swap paths run, and each
timed frame checks the Player's position and rotation against the entry pose.
This measures steady frames for a stationary scene. Input latency and active
play require separate measurements.

Three alternating Windows MSVC Release pairs on the i9-10900F and RTX 3060
passed with matching poses, active World work and both windows on `DISPLAY2`.
The baseline rebuilt two objects from before the lookup optimization. Both
executables shared the observer, the other runtime objects and the resource
fixture. All six runs used Tiny distance at 854×480.

| Round | Before FPS | After FPS | Before p95 (ms) | After p95 (ms) |
| --- | ---: | ---: | ---: | ---: |
| 1 | 27.12 | 26.89 | 90.79 | 86.82 |
| 2 | 21.11 | 26.60 | 111.77 | 83.61 |
| 3 | 24.03 | 28.03 | 108.40 | 74.36 |
| Median | 24.03 | 26.89 | 108.40 | 83.61 |

The first round's FPS fell slightly, while p95 improved in all three rounds.
The sample covers one stationary scene on one machine.

A second cohort balanced executable order over four pairs. All eight runs
kept the timed camera pose, but one baseline run lost native foreground and
paused World simulation. That cohort has no aggregate FPS comparison.

## World-frame storage reuse trial

The trial kept one retired World-frame allocation per renderer. A later
call cleared and reused it. A call entered spare storage after child
retirement and release of its renderer hold. A call with the final renderer
hold freed both allocations at retirement.

The Windows MSVC Release call occupies 472,344 bytes. Twelve alternating
native samples at that size ran 5,000 operations each. The median fell from
11.50 µs for allocation and free to 8.76 µs for clearing preallocated storage.
Each operation checked and touched the first and last bytes. Clearing reused
storage was faster in every pair. This isolates the allocation and clear costs.

The trial passed six focused Windows Release tests, four Linux Clang
ASan/UBSan tests, and the ten-load Windows save and reload profile. The
World-frame control covered refused retirement, state clearing on reuse,
and release of the frame's final renderer hold.

Four alternating Windows Release pairs used the same authored Far scene,
camera, assets and observer. All eight runs advanced 200 World ticks after
a 30-second warmup. Each run qualified on `DISPLAY2` at 854×480.

| Median of per-run statistics | Before | Trial |
| --- | ---: | ---: |
| FPS | 491.43 | 488.12 |
| Frame interval p95 | 3.183 ms | 3.239 ms |
| Frame interval p99 | 6.275 ms | 6.738 ms |
| Longest frame interval | 8.828 ms | 9.917 ms |

Frame interval p99 rose in all four pairs. Every measured frame stayed below
16.67 ms. The trial was dropped because the full client had longer frame
times despite less allocation work in the fixed-size benchmark.
All 394 frames in the slowest 1% of the eight runs processed a tick. The
median tick batch on tick-bearing frames went from 3.661 to 3.769 ms across
the paired runs.

## Frame report parsing

The observer splits the captured output into lines once. Each timing
distribution sorts its samples once and uses that order for p50, p95 and
p99. Means still sum samples in capture order.

Fourteen alternating Windows Python 3.14 pairs replayed the eight Far
captures above, totaling 39,031 frame rows. Median parsing time fell from
390.95 to 304.15 ms. Each pair was faster, and both parsers reproduced every
retained summary exactly. This measures report processing after capture.
