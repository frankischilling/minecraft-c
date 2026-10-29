# World selection

Singleplayer now opens the genuine `rq` screen from the initialized GuiMainMenu.
It retains that exact parent, publishes the new screen through the existing
display-screen method and initializes its actual save List, scrolling owner and
buttons. The default native loop draws and services this screen on the same App,
Display, Font and imported-resource graph as the main menu.

## Saves and drawing

The screen obtains its List from the App's real SaveFormat manager. Metadata
uses the existing File/GZIP/NBT/WorldInfo path, including `level.dat_old`
fallback. The SaveFormat namespace now borrows the same shared MathHelper class
owner for the reached String helper; it does not bypass class initialization
when a name is empty. The returned List is published before its stable sort and
the selection index resets to `-1` afterward. Rows use the actual save metadata,
conversion flag and translated labels.

The `mg`/`lg` owner performs selection synchronously during drawing. Its source
visibility test and later dirt overlays determine row clipping. The native
canvas supplies the existing texture, quad and glyph kernels. It adds no wheel
polling, deferred selection or scissor behavior. Text uses the screen's captured
canonical Font, with resource identity and width checks.

The real no-argument DateFormat constructor captures the default locale and
timezone even when the save List is empty. The supported profile is English or
`en_US`, UTC or US Eastern, for timestamps in UTC years 1967 through 2099.
Other locales, zones and dates fail explicitly. The observed SHORT/SHORT pattern
is `M/d/yy, h:mm a`. The owner uses native services; the client needs no JVM.
The [DateFormat observations](../../tests/reference/platform/date_format/README.md)
record the 2,926 JDK17 vectors, transition coverage and host provenance.

## Transitions and lifetime

Cancel displays the exact retained parent and reinitializes that same screen.
A screen change is synchronous. The source input call keeps its original
receiver and List alive until its remaining mouse and keyboard input has
drained; it does not switch the unread tail to the newly published screen.

Selecting a save performs the display-screen call, sets the loading guard and
constructs and publishes the real singleplayer controller `os`. It then stops
at the World-load method's `a(NULL World)` call at offset 2. No World or Player
is fabricated, and no playable session follows from this route. Create and
Rename stop at their destination allocations, `NEW162` and `NEW189`. Delete
stops at preparation offset 21; its earlier preparation is not skipped to reach
a confirmation screen.

Native transition ownership retains earlier screens and failed constructor or
display prefixes. Holding an old RQ occurrence makes teardown return BUSY while
preserving the App and its dependencies. Releasing it permits cleanup to
continue. Retirement does not replay source calls or restore old field values.
The complete startup graph also refuses retirement while the runtime holds its
lease. Failed native monitor reacquisition retains a recovery occurrence;
cleanup retries that monitor without replaying input or drawing.

## Verification scope

The genuine Windows native loop passes isolated empty, populated, sixteen-save
scrolling and double-click profiles. They check sorted real metadata, held-button entry, selection during
drawing, same-parent Cancel, original-receiver trailing input, the actual
controller and World-load frontier, retained-old-screen cleanup and unchanged
save/resource bytes. The scrollbar reaches its lower clamp and returns to zero.
A real double-click reaches the loader failure before the later list index and
clock stores. Empty, populated and bottom-scroll 854 by 480 captures were inspected.
A prepared native monitor-failure receipt checks recovery, active-call refusal
and idempotent cleanup; it does not force a host mutex failure. The DateFormat
owner matches all 2,926 JDK17 vectors on Windows and Linux for its admitted
profiles; this does not qualify original game-menu pixels or all timezone data.

All 468 selected Windows CTests pass without failures or skips, alongside the
client and headless builds and formatting for 1,356 files. The
[testing record](../testing.md#world-selection-in-the-default-runtime) separates
these focused results from earlier complete local and hosted checkpoints.
World construction/loading, Player publication, creation, rename, deletion and
gameplay remain unfinished.
