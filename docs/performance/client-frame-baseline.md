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
