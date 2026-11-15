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

The existing observer accepts already-built executables and alternates their
order across rounds. A future comparison should retain every attempt, match
the authored scene and camera, require active World work, and distinguish
fixed-work CPU results from deadline-driven Source frames. Read physical
monitor topology and set `SDL_DISPLAY_USABLE_BOUNDS` before opening test windows.
