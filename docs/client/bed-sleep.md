# Loaded nighttime Bed scene

The local Windows fixture loads an authored saved World through the ordinary
World selection screen. It places Item 355 with an SDK mouse tap, activates
the placed Bed with another tap, and observes the normal Player sleep timer.
It does not set the timer, advance the clock, repair focus, or resume a failed
source call. The completed route is default-hostile nightmare
rejection, dawn, the actual wake, the HUD fade, copied Bed spawn, full source
save, independent NBT readback and checked native shutdown.

The accepted `bed-sleep-ea30eu61` opportunity passes 2,086 native checks.
It observes 88 timer transitions, a highest timer of 99 and 163 returned HUD overlays
before the actual scheduler enters CQ once and receives false. Both source
wake calls and returns equal 1. The Player's timer subsequently reaches 0;
independent saved NBT records Time 24011, health 20, one remaining Bed Item,
sleeping false, cleared Bed occupancy and copied spawn (0,64,3). All 2,216 guarded
source/product inputs and 413 imported-resource files remain unchanged during
that opportunity. The [frozen scalar receipt](../../tests/reference/client/bed_sleep/receipt.json)
records its source/product hashes; saves, screenshots and raw reports stay local.

The input has 289 stored Chunks, initial Time 13000, difficulty 2, no Entities,
and a Glowstone floor at Y63. Its complete BlockLight planes follow the real
emission above that floor: 15 at Glowstone, 14 directly above, then 13 through 1
at Y77. Two opaque Grass cells support the Bed; their internal light is 0 and
the light above them is one level lower, supplied by adjacent lit air.
All inputs are written and independently
decoded before bootstrap. Glowstone sustains the light during ordinary
updates; reached hostile spawn predicates must reject the supported columns.
Hostile spawning remains enabled, so the scene still enters the real
`CQ.a(World,List)Z` body.

Completed source-call totals in the menu loop preserve the observations when
one native frame contains several simulation ticks. A later tick may replace
its borrowed child diagnostic. These totals control no source behavior,
scheduling or clock. The fixture checks elapsed Player ticks and observed
timer progression, rather than treating a prepared timer of 100 as natural sleep.

## Monitor and evidence

A fresh scene after the final eligibility/loader ownership and client cleanup
fixes also passes: `bed-sleep-65wbapzf`, 2,072 checks, 97 subsequent natural
ticks, 88 timer transitions and 162 returned overlays. Its
[separate frozen receipt](../../tests/reference/client/bed_sleep/receipt-current.json)
preserves the earlier pass and all four failures. Differences in frame and
observation counts follow the actual elapsed Timer; neither scene changes it.

Before opening a game window, the Python wrapper reads the actual Windows
monitor topology and requires `DISPLAY2`. It supplies that monitor's usable
bounds through SDL's process-local startup hint. The hidden source frame is
centered before its first show. The native fixture then checks both the frame
and child canvas against the requested rectangle and their actual physical
SDL display. This changes no desktop settings or production defaults.

Every opportunity gets a fresh directory with stdout, stderr, source/product
and imported-resource hashes, save snapshots and a report. Failed attempts
remain failed. Four preceding local attempts are retained as follows:

- `bed-sleep-88mmc0na` reaches successful SDK placement and then fails the
  sleep-entry test assertion. That combined assertion does not identify the
  failed term. A borrowed input view can be overwritten by another genuine
  tick in the same frame; the fixture now also uses the already completed
  input-call total. This failed attempt receives no sleep credit.
- The second opportunity loses foreground focus to `msrdc.exe` during
  placement preparation, before the second Bed tap.
- The third reaches actual sleeping state, timer 1, head coordinate (0,64,3),
  `World.J=true`, difficulty 2 and hostiles enabled, and captures a sleeping HUD
  frame. It then loses focus to Remote Desktop during natural progression.
- `bed-sleep-3exebbc3` fails the shared placement completion assertion while
  the native game still owns focus. The same transient child-view assumption
  also existed there. The corrected fixture preserves all source-effect,
  completed-call, placement, statistic, Stack and drained-input requirements.

Both source windows are observed at (-1387,280), 854x480 within monitor 2's
usable rectangle (-1920,0,1920,1040) in those opportunities. The later passing
opportunity qualifies its prepared loaded-night route. Unrestricted night,
uninterrupted focus across other processes, original pixels/audio, Linux loaded
play and sustained FPS remain unqualified.

## Sleep observation boundary

A private stress run lets two ordinary rendered frames finish before the dawn
watch begins, with real host delays between frames. Sleep has already reached
timer 23 at that boundary. Natural dawn correctly arrives 77 ticks later at
timer 100, with all wake, spawn and sky checks satisfied. Requiring 90 further
ticks after that late boundary rejects correct behavior.

The fixture now captures completed World ticks immediately before the SDK
sleep gestures and passes that local marker through the observation. The
receipt names the measurement `input-to-dawn-world-ticks`. The threshold 90,
observed timer progression, overlay, nightmare/wake counters, copied spawn,
sleep flags and sky assertions remain. No game call, scheduler, clock or
runtime field changes. The same delayed-frame stress passes with 101 completed
World ticks from the input boundary and all predicates intact. This explains
the demonstrated measurement flaw; the earlier uncaptured failure retains its
own scope.

## Reproduction

Build `mc_test_bed_sleep` with the Windows client. Run with locally imported
original resources and a connected second monitor:

```powershell
python -B tests/tools/client/test_bed_sleep.py --executable build-windows/mc_test_bed_sleep.exe --assets .local/assets --work-dir build-windows/bed-sleep-scenes
```

The optional CMake switch `MC_ENABLE_CLIENT_BED_SLEEP_TESTS` registers the
visible test only with the existing display/imported-resource prerequisites.
Both ordinary and optimized Python input controls run in the portable suite
without opening windows. This scene's result remains separate from the
[nightmare component](../world/nightmare.md), the
[source wake controls](../entity/player-sleep.md), and the
[broader test record](../testing.md).
