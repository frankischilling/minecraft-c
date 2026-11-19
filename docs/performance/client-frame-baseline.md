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
