# GuiIngame and the loaded frame

The loaded single-player client now composes terrain, Sky, clouds, the empty
hand and GuiIngame in one returning frame. The focused Windows profile shows
the actual imported hotbar, hearts and crosshair above the saved World. Its next
unpaused World tick reaches the concrete Mob construction dependency in
`cq.a:773`. This is a rendered frame, not a playable client.

## Construction, clearing and ticks

The [CPU owner](../../include/mc/client/gui/gui_ingame.h) retains actual inherited
`ub.k`, ten instance fields, a distinct List and ordinary Random, literal String
identities and a borrowed App. Class activation constructs its own static
ItemRenderer and publishes d only after that constructor returns. It remains
distinct from the achievement notice and RenderManager renderers. The model
namespace is retained; its borrowed Random profile outlives both owners.

Cold allocation performs no instance constructor work. Construction preserves
the superclass/field order and retains failed List/Random prefixes. Native
budget/profile failures differ from Java exceptions. The borrowed App remains
live through every exported occurrence. Strict retirement rejects extra holds
before revocation; failed child/namespace cleanup preserves retryable slots.

Normal startup constructs the instance at702 and publishes App.v at705 before
constructing the main menu. The display-screen caller clears messages at101.
Strict startup retirement preserves the App graph while an external UQ
occurrence exists. Clearing retains the actual List capacity.

The actual `uq.a()V` tick decrements positive j, increments h with Java wrapping,
then reads the current List. An initialized empty List returns70. A null List
fails34 after those stores; a null message fails56. Nonnull message age updates
still stop at the canonical SW owner/type dependency52. Neither ticking nor
drawing substitutes a copied chat-message model. The constructor/tick case is
included in the current Windows and Clang selections.

## Source drawing

The [retained draw API](../../include/mc/client/gui/gui_ingame_draw.h) runs
`uq.a(FZII)V`. Its first scaled resolution supplies integer GUI dimensions.
The actual PX overlay child constructs another resolution and uses its exact
binary64 extents for orthographic projection. At physical 641 by 481 with GUI
scale 2, geometry uses 321 by 241 while projection uses 320.5 by 240.5.
PX clears depth, selects and resets projection, applies the source ortho,
then resets modelview and translates it by -2000. It does not replace the World
frame with the separate inspection renderer.

The actual `Minecraft.u()` reads the canonical static App and its fancy setting.
Fancy drawing executes the real Entity brightness path and vignette. A present
area queries current World light; an absent area uses the Entity's brightness
floor. The original mixed float/double smoothing stores GuiIngame.c before the
vignette draw. Both bounds paths have unchanged-original observations.

Actual Engine lookups supply vignette, GUI and icon textures. Hotbar selection,
crosshair and health icons use the inherited UB textured rectangle method
through NW. Low health consumes ten real bounded Random calls for vertical
jitter. Hurt blinking uses actual Player counters and prior health; each draw
reseeds the same UQ Random with the wrapped `h * 312871` integer.

The default path then enables standard lighting, visits nine genuine empty
Inventory slots, disables lighting/rescale-normal and runs the remaining
sleep/debug/record/chat predicates. Empty chat returns at 2101. Nonempty item
icons, pumpkin/portal/water overlays, sleep fading, debug text, record messages
and nonempty chat keep their explicit missing descendants. The armor arithmetic
is implemented, but populated armor is outside the current reference selection.

Source effects survive failed draws. Pending UQ calls reject overlapping tick,
clear, construction and retirement work. Cleanup releases only owned lookups
and occurrences, preserves foreign lookup attempts and leaves reached GL state
intact. The parent retires a successful HUD immediately and keeps its scalar
receipt. It retires completed World drawing before the next frame's pool reset
and tick. The native cleanup does not replay source work.

## Parent frame

After World return1013, `px.b(F)V` reads performance Settings again. The limited
mode computes its remaining milliseconds from the original target and I field,
sleeping only for values strictly between 0 and 500. It then samples the native
System.nanoTime service and stores I at453. Fresh hide-GUI and screen reads
select the captured UQ receiver and its screen-presence argument. The source HUD
call is at506; the ordinary no-screen renderer returns709. These clock values
are real native samples, not cross-process clock-identity comparisons.

The focused loaded profile requires a complete frame return. A separate fresh
profile continues the ordinary loop until its next unpaused tick reaches the
spawn constructor, checking that no completed draw guard or stale HUD receipt
blocks it. That profile uses the normal terminal coordinator to end dispatcher
bindings and preserve retryable cleanup. The hidden-window profile follows
actual inactivity into OZ, draws the HUD and reaches the unfinished incremental
World save at `oz.a:22`. Save completion is not skipped.

## Evidence

The [original fixture](../../tests/reference/client/gui_ingame_draw/README.md)
contains 26 complete calls from unchanged classes: fast/fancy graphics, two
source dimensions, normal/low/blinking health, two selected slots and two
in-height brightness cases. It records source fields, Random state, matrices,
GL state and the final NW payload. Its authored pixel receipts are preserved
as reference observations; whole-frame and original HUD pixel equality are not
claimed.

The native test uses actual App startup, GAME World/Player and Controller owners,
with declared source operands and authored textures. It checks visible hotbar,
heart and crosshair pixels, pending calls, null receivers and foreign lookups.
No original assets are needed for that test. Loaded-frame testing uses locally
imported original assets and isolated copied saves. See [testing](../testing.md)
for final results and platform scope.

## Qualification

All 55 selected targets build on Windows MSVC Debug and Linux Clang18
ASan/UBSan. The selections pass 64 Windows tests in 84.40 seconds and 60 Linux
tests in 124.25 seconds, without failures, skips or sanitizer findings. All
2,349 C/helper/build inputs remain unchanged through builds and tests. Both
hosts match all 26 original state/Random and final NW rows exactly, with
bit-exact matrices. The successful in-height light receipts also match.

Native controls pass 1,742 checks on Windows and 1,743 on Linux, including
bounded teardown retries. The authored image has 3,521 green hotbar pixels,
270 red heart pixels and 60 white crosshair pixels on both hosts; their PNG
hashes are identical. Separate nonblack-background controls distinguish the
actual vignette blend `(0,769)` and crosshair blend `(775,769)`, returning exact
RGB `(38,51,38)` and `(204,153,102)`. Alpha is retained in raw readback but is
outside those RGB assertions. None of these native controls establishes
original HUD pixel equality.

With `MC_ENABLE_CLIENT_DISPLAY_TESTS` enabled, the authored test needs no
original assets:

```sh
cmake --build build --target mc_test_gui_ingame_draw
ctest --test-dir build -R '^client.gui_ingame_draw$' --output-on-failure
```

Use `--config Debug` and `-C Debug` for the Windows multi-configuration build.
Linux requires a working GL3.3 display; [build setup](../building.md) describes
the Xvfb/Mesa profile. The loaded-World test is a separate Windows selection
with imported resources, copied saves and a visible window for focused frames.
