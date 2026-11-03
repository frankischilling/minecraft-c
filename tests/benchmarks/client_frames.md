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
