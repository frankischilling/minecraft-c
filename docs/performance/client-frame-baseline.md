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
