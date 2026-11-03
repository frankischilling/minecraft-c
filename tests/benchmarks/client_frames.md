The optional Windows frame observer runs a fresh client in the saved sky-view
scene: seed 123, Tiny distance, 854x480 and Rotation 180,-30. Natural simulation,
sound work, loading, lighting, rendering, swaps, pacing and autosaves remain
enabled. It warms up for two seconds, then observes at least ten seconds of
ordinary frames. It requires imported resources and an idle, focused desktop.

Build and run from an existing Release Ninja/MSVC SDK configuration:

```powershell
python -B scripts/benchmarks/client_frames.py --build build-windows --out .local/frame-builds
python -B scripts/benchmarks/client_frames.py --executable current=PATH_TO_CLIENT_FRAMES_EXE --assets .local/imported-icons --artifacts .local/frame-runs --rounds 3
```

The driver retains every process, including focus and unsupported-behavior
failures. Only complete intervals with matching settings, assets, saved state,
camera and graphics context support comparisons. No FPS threshold determines
success. [The earlier frame comparison](client_frames_windows.json) retains its
separate qualifications and failed repeat cohort.

Observation version 2 adds the existing pre-HUD source clock and its reached and
returned flags to each raw row. It does not query another source clock. The
parser still accepts version 1. It applies Java signed-long wrapping when
subtracting source clocks, and rejects reversed, overlapping or out-of-range
host intervals. Public parser checks need no display or assets:

```powershell
python -B tests/tools/benchmarks/test_client_frames_run.py
```

## Host phase attribution

[The phase receipt](client_frame_phases_windows.json) records one instrumented
interval from runtime `56aca7961f1fa5aa50641d01852e84841925541a`. The observer
uses the same saved scene. It adds host QPC brackets around five actual native
calls, with fixed per-frame storage and no hook allocation or output. Source
clocks, Timer input, deadlines, work limits, gameplay state and GL calls remain
unchanged. Natural clocks keep advancing, so instrumentation overhead can affect
deadline-driven work. This interval establishes attribution, not an FPS change.

All 194 returned frames retain source focus, the authored camera and the
ordinary owned cache with zero raw exports. Cleanup, input and asset guards
pass. Each phase has one valid reached call per frame, zero observation flags,
and summed disjoint durations within its host frame.

| Actual native scope | Mean ms | Median ms | p95 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: |
| World-frame child | 22.432 | 5.398 | 56.761 | 104.088 |
| World light drain | 6.487 | 4.116 | 18.339 | 25.615 |
| HUD draw and checked retirement | 0.518 | 0.494 | 0.671 | 0.860 |
| SDK swap and message update | 0.428 | 0.367 | 0.890 | 2.422 |
| Previous-frame native retirement | 0.028 | 0.028 | 0.038 | 0.058 |

The whole instrumented host frame averages 51.635 ms. Separately, the existing
source tick-batch clock envelope averages 21.638 ms, with p95 112.312 ms and a
281.013 ms maximum. Tick work falls outside the five named native spans. World
rendering, tick work and lighting warrant narrower profiles in this scene.
Swap and HUD durations do not explain most observed time.

The receipt includes every scalar timing row, exact instrumentation patches,
the hook header and implementation, compile settings, source/object/executable
hashes and the baseline library binding audit. The private build reuses four
frozen SDK objects and 92 frozen archives. Its two changed object roles are the
observer and menu loop; all public menu-loop functions resolve to the private
override. It guards 7,785 build inputs before and after compilation. Two
pre-capture guard failures are retained separately: Windows path spelling and
assert-string matches in the symbol map. Neither attempt launched a client.

These CPU host spans include driver blocking. They do not measure GPU duration,
original-game pixels or unrestricted play. They do not establish a speedup.

## Nested World and tick attribution

[The nested receipt](client_frame_nested_phases_windows.json) records a separate
200-frame interval against the same frozen runtime. It adds host brackets around
actual child calls inside World rendering and tick execution. Every nested
duration remains within its parent. Parent and child times overlap and must not
be added together. Per-frame rows retain zero durations when no tick occurs.

| Actual native scope, summed per host frame | Calls | Mean ms | Median ms | p95 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| World-frame parent | 200 | 21.568 | 5.515 | 56.825 | 107.844 |
| ChunkRenderer update child | 305 | 17.220 | 1.143 | 52.354 | 103.413 |
| Opaque and translucent submit children | 400 | 0.915 | 0.822 | 1.426 | 2.479 |
| Complete tick parent | 193 | 21.612 | 13.534 | 88.102 | 214.911 |
| World tick child | 193 | 21.295 | 13.252 | 87.056 | 214.655 |
| Texture tick child | 193 | 0.230 | 0.195 | 0.638 | 2.819 |

The whole instrumented host frame averages 50.405 ms. Light drain averages
6.128 ms; HUD, SDK update and prior-frame retirement average 0.522, 0.445 and
0.028 ms. The World child includes a source-controlled update loop whose call
count varies with the existing deadline. Its 305 update calls remain intact.
All six tick scopes have exactly 193 calls, matching source tick returns. The
200-frame parent/child audits, scene checks and cleanup pass; 7,787 guarded build
inputs remain unchanged.

The remaining measured targets are ChunkRenderer update and World tick work.
An update-child duration includes its real compilation work; narrower allocation,
query and meshing measurements are needed before assigning its cost to one
operation. This capture does not compare runtime variants. Its three private
object overrides are the observer, menu loop and World-frame coordinator. The
receipt includes exact additional patches, hooks, raw rows and binding hashes.

## Current compilation and scheduler work

[The work receipt](client_frame_work_windows.json) records two separate intervals
against frozen runtime `ec6f2b38e0160e1e9bc6f54d579736bf305554ad`. The current SDK
target completed an actual no-op build check before its four support objects
and 92 archives were frozen. Every project header/helper used by the observer
comes from that frozen source cohort. Compact raw rows retain all phase durations,
call counts and passive work counts. Public parser tests recompute their
summaries, instrumentation hashes and parent bounds.

In the first interval, all 455 attempted compilations enter dirty and return.
DK compilation accounts for 18.071 of 18.132 ms per host frame in the complete
ChunkRenderer update span. Queue scanning, sorting and outer handling consume
the remaining 0.061 ms. The World scheduler accounts for 14.820 of 15.751 ms in
the World tick span. Entity ticking and random-display work average 0.518 and
0.392 ms. All 196 ticks retain 70,756 random Chunks, 5,660,480 random Block
samples and 196,000 random-display samples.

The separate scheduler interval preserves all 190 source ticks:

| Actual scheduler child, summed per host frame | Calls | Mean ms | p95 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: |
| Random Chunk tick | 190 | 14.321 | 63.460 | 333.469 |
| Spawning | 190 | 0.960 | 2.749 | 5.074 |
| Scheduled updates | 190 | 0.752 | 3.488 | 32.540 |
| Conditional autosave | 4 | 0.326 | 0.000 | 17.621 |
| Provider maintenance | 190 | 0.018 | 0.044 | 0.113 |

Random Chunk work accounts for 14.321 of 16.384 ms in its scheduler parent.
Every one of 68,590 completed random Chunks performs all eighty samples,
totalling 5,487,200. Spawning retains 82,654 candidate visits, 4,998 constructors
and spawn predicates, and 88 publications. Scheduled updates retain 10,271
iterations and 10,089 removals. No per-Block clock calls were added.

These intervals identify actual DK compilation and random Chunk ticking as the
next measured targets. Natural spawning, loading, generation, updates, saves
and source deadlines continue to run; interval work can differ. The intervals
do not form a speed comparison and must not be combined into one FPS result.

## Integrated helper frame attempts

[The helper frame receipt](client_frame_helper_windows.json) retains two complete
three-round alternating cohorts and a separate original/shared-observer control.
Both cohorts are rejected. They establish no aggregate FPS improvement.

The baseline uses runtime `ec6f2b38`; the candidate replaces only ChunkCache,
CV access and DK compilation objects. Both executables use one identical
uninstrumented observer object, four SDK support objects and the same 92-archive
tail. Added observer work consists of existing scalar counters read before and
after the interval, plus printing afterward. Public symbol binding, unchanged
structure layouts and frozen inputs pass their audits.

All six first-cohort attempts fail before the added work assertions. Native
debug/screenshot frontiers, focus changes and altered camera poses remain in
their records. The user confirmed keyboard and mouse use during recent runs;
the exact origin of every recorded F2/F3 event was not independently traced.

In the diagnostic control, the original observer returns normally but fails its
camera guard while passive host tracing records 53 last-input changes. The
shared-counter observer passes all original scene guards with no last-input
changes. Its authored camera stays exact. This control includes concurrent host
polling, so it is excluded from performance comparisons.

The second six-process cohort records passive host snapshots only outside each
native process. Five of six three-second pre-launch windows already show changed
last-input values while no native child is running. All six child ranges end
with changed last-input values. Native menu frontiers and focus/camera guards
again reject every interval before work assertions. These observations rule out
the child alone as the source of all pre-launch activity. They do not identify
which process or device supplied every input.

Every scheduled attempt remains retained; no input is filtered or cleared, no
camera is repaired, and no guard is relaxed. The public receipt omits private
paths, native addresses, window handles and original assets. Its foreground
projection preserves window presence and process-ownership equality for replay.
Parser tests re-evaluate the actual raw rejection records and input snapshots.
