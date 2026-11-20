# Sign placement and editor

The client can place a Sign Item, open its four-line editor, process text and
navigation keys, draw the preview and Done button, then return to the World.
The Windows gameplay control follows that path twice, saves through the menu,
and reselects the World with both Signs and all eight lines intact.

The [Sign storage and renderer contract](../world/signs.md) covers loaded text,
NBT, support checks, removal, bounds and the board/pole model. This page describes
the placement and editor callers, their ownership, and the limits of the current
evidence.

## Placement order

The Sign Item's VC body rejects the bottom face and a clicked Material whose
solid query returns false. It offsets the target with Java integer wrapping,
then calls the standing Sign Block's inherited placement predicate. An upward
face places a standing Sign with the yaw-derived metadata masked to 0 through
15. Side faces place a wall Sign with the face as metadata.

The two bare World setters retain their original return handling. A normal
false setter result is discarded; the body still decrements the signed Stack
count, looks up the TileEntity, and, for a nonnull Sign, calls the Player hook.
An exception stops at its reached operation and preserves earlier effects.
The surrounding ItemStack/controller path supplies the used statistic and
clears a consumed normal one-item slot. The qualified Sign path adds no generic
placement sound.

Editor creation belongs to the client LocalPlayer's DC override. The original
base Player hook is empty; native callback admission covers the concrete client
receiver. Other Player dispatch remains an explicit reached limitation. DC
captures its App operand, constructs the real YC screen around the nullable
Sign, and enters the ordinary App display path. A later failure can therefore
follow a completed Block/count change. Construction and display are separate
retained calls.

## Four rows and UTF-16 input

The editor stores a selected row and a wrapping Java integer tick counter.
Character admission uses the captured FP String loaded from the local
`font.txt`. It appends one accepted UTF-16 code unit when the current line has
fewer than 15 units. This limit counts code units, including surrogate units;
it does not count rendered glyphs or user-perceived characters.

| Input | Effect before character admission |
| --- | --- |
| Up, key code 200 | Select the previous row, wrapping 0 to 3 |
| Down, key code 208 | Select the next row, wrapping 3 to 0 |
| Return, key code 28 | Select the next row |
| Backspace, key code 14 | Replace a nonempty line with its prefix minus one code unit |
| Numpad Enter, key code 156 | No special row change |
| Escape with no character | Leave the editor open |

Character admission follows those key branches. A direct call carrying both
Backspace and an allowed character can delete then append; Return plus a
character changes rows before appending. These finite component inputs do not
claim that ordinary SDL Backspace/Return events deliver a printable character.

The tested imported mapping accepts the selected ASCII and BMP characters and
rejects both units of the tested supplementary pair. A valid supplementary
UTF-8 text event reaches the SDK as two UTF-16 units, and each unit is tested
separately. Isolated high/low units have separate direct-component controls;
they are not presented as malformed UTF-8 events. Neither path normalizes text.

Backspace removes one unit and can split a pair already present in a line. A
loaded or explicitly supplied overlong line refuses further appends until its
length falls below 15. Backspace-to-empty uses the defining TileEntity
namespace's empty literal. Editing an initially shared empty value replaces
only the selected row. Key and tick methods do not dirty the TileEntity or
repair its read flag or public edit marker.

The native owner has four physical line slots and admits nullable individual
rows. A null row fails at the reached String access after any earlier row
selection. Replacing the whole Java array with a null or short array remains
outside the native representation.

## Drawing and renderer warmup

DA initialization captures the App's Font once. The title uses that captured
Font. The Sign preview uses LL's bound Font, while each ordinary Done-button
draw reads the current App Font. The direct fixture constructs two real Fonts,
captures Font1 during DA initialization, changes the declared public App Font
operand to Font2, and binds LL to Font1. This exercises the three reads without
substituting a Canvas text renderer.

YC draws the loaded-World gradient, centers the title, pushes its preview
matrix, reads the actual Sign Block and metadata, and applies the standing or
wall rotation. When `(tick / 6) % 2 == 0`, it stores the selected row in the
Sign's public edit marker. It then calls LL at `(-0.5, -0.75, -0.5)` with partial
time zero. The existing PO/RF renderer draws the model and four text rows.
On return, YC clears the marker to -1, pops its matrix, and draws the ordinary
buttons. During the other blink half, a pre-existing public marker remains in
effect until that normal clear.

The direct control covers ticks 0, 5, 6, 11 and 12, standing metadata 8 and wall
metadata 2 through 5, with actual tick and Down calls. Each draw starts from a
declared public marker value of 3. Widths and row centers use Java integer
division, and selected rows keep both spaces in `> ` and ` <`.

Failures preserve the reached prefix. A null captured title Font fails at UB's
measure invocation at PC5 after the background returns. A null LL Font reaches
the preview's model/text prefix; the marker and outstanding matrix/depth state
remain. An already-open NW batch fails at its original begin guard after the
gradient's preceding GL changes. Native call retirement releases owners without
adding the missing Source pops, depth restores or batch draw.

RenderGlobal warmup has a separate ordering requirement. The early `I=1` to
`I=0` pass still returns at N17 and leaves LL unbound. The gameplay fixture waits
for a full returned Entity traversal that has entered LL44 and completed at
N569 before opening the editor. It changes no production delay or renderer
field. The original method's static Code proves that early-return order; an
actual original cold-input null failure has not been observed.

## Done, close and native lifetime

Done calls the actual detailed TileEntity dirty body before reading the current
App operand and requesting display of a null screen. The old screen's close
method disables keyboard repeat first, then reads App and World. Its local
World branch returns normally. The remote network branch remains a reached
limitation.

Dirty completion and persistence are separate. The gameplay check reads YC's
dirty-call result; placement has already dirtied the Chunk. It makes no claim
that Done caused a clean Chunk to become dirty. Ordinary saving later serializes
the live text through the World save path.

An input drain holds its old Screen receiver for the complete event sequence.
Two already-queued valid presses can invoke Done twice even after the first
changes the App's current screen. Each invocation owns its actual display child.
The event ledger transfers those calls to the router without replaying Source
work. Cleanup follows their dependency order, with later children ending before
the Source parent they still borrow. A live App screen or external hold can
return BUSY and leave the same owned slot available for retry.

Each Done invocation also retains the actual WorldAccess failure receipts
reached by its dirty call. Aliases within that invocation share one capture;
earlier invocations stay retained through later Done calls. A self-removing
producer cannot retire its receipt while YC still exposes that failure. Native
terminal cleanup expires producer views, releases captures, then ends the
TileEntity/World occurrence. Source flags, sites and scalar causes survive;
cleanup failures have a separate diagnostic. Diagnostic copies expire their
borrowed child views when the actual child is consumed.

The defining YC namespace keeps the FP owner live while NEW admission is open.
Ending that admission releases the FP hold and preserves the captured String
for existing screens. Instance terminal preparation separately requires the
screen's active calls and external holds to have ended. Native retirement does
not act as an editor close or reset a cached class failure.

## Save and reload evidence

The Windows scene starts with two authored ordinary count-one Sign slots. Real
SDK taps place them, real text/key events fill the rows, and real Done presses
return to focused play. The selected saved Chunk contains standing Signs at
`(0,64,2)` and `(1,64,2)`, both metadata 8 over the unchanged Dirt supports.

| Sign | Text1 | Text2 | Text3 | Text4 |
| --- | --- | --- | --- | --- |
| `(0,64,2)` | `ABCDEFGHIJKLMNZ` | `é B` | `row2` | `last` |
| `(1,64,2)` | `two` | `2` | `done` | Empty String |

Independent Region/NBT decoding checks all eight String tags, Int coordinates,
the initial Byte Slot/Count and Short id/Damage fields, and the final empty
Inventory. Save-list reselection constructs a distinct World and Player and
draws both loaded Signs with the same UTF-16 text. The retained saved/final
copies agree except for the actual eight-byte SessionLock; unrelated worlds
and preservation bytes stay unchanged.

The authored area begins with 289 Chunks. This run saves 382 after genuine
generation adds 93; a generated dungeon elsewhere contains a MobSpawner and a
Chest. The two Signs belong to selected Chunk `(0,0)`. At the captured save
entry, metadata already records time 32 and consumed slots while Region data
still has no Signs. The later saved Region contains the edited Signs. Its
metadata SizeOnDisk is the pre-Chunk-save 3,145,728 bytes; the live accounting
after Chunk saving is 3,543,040. Those observations preserve the save order.

## Finite validation

| Evidence | Qualified scope |
| --- | --- |
| Original editor observations | Eighteen fresh VMs across healthy, captured-screen-Font failure and manager-Font failure cohorts, six per cohort. Finite fields, method PCs, public GL state and active Font-buffer observations are retained. |
| Windows native direct drawing | Twenty-five returned draws and four terminal Source failures, with actual Display/Input/Fonts/Worlds, exact row argument digests, widths/centers, marker/GL/NW prefixes and checked ownership retirement. |
| Native event ownership | Forty-seven component checks cover two queued Done calls, retained old receivers, display-call adoption and external-hold BUSY cleanup across fallback and later screens. |
| Editor and dirty components | Windows reports 663 CPU checks or 713 with real SDK input. Linux passes three core tests, 54 real-World dirty ownership checks and 711 checks in the SDK run. |
| Ordinary Windows gameplay | One monitor-2 run passes 1,833 checks across 118 frames, including 31 and 15 returned editor frames, two single-tap placements, both Done calls, ordinary save/reselection, five fresh PNGs and checked window closure. |

The original healthy caller has 20 draw inputs; the native direct fixture has
25 normal inputs. The complete caller-wide active buffers are not paired.
Original buffer observations and the earlier loaded-PO comparisons retain
their own scopes. Native command payload equality and original pixel equality
do not follow from the caller observations.

The original failure observations retain raw messages separately at the throw
site and at a later `getMessage` read. No message normalization is applied.
The class, site and completed prefix do not imply literal message equality
between those observation points.

Independent audits re-read the physical products, raw streams, source/helper
and imported-asset brackets, saved copies and images. Their parser-check totals
are not coverage or gameplay-case counts. The gameplay audit checks all 2,425
selected input pins and 413 asset files without running the target again.

The combined Windows build and all 1,387 configured tests pass in 654 seconds,
retaining all 1,380 preceding test names and adding exactly seven Sign tests.
No test is skipped or left unrun. The current GCC and Clang client builds and
seven selected tests also pass; those selections take 3.63 and 3.21 seconds.
These local configurations retain separate scopes from the hosted matrix.

Linux component results do not establish ordinary loaded editor gameplay. The
local Mesa 26 full-startup path currently stops at an explicit unmeasured
graphics-attribute requirement before the editor. Other driver profiles,
server/network behavior, arbitrary replacement arrays and recursive graphs,
sustained play and FPS remain outside this qualification.

## Running the controls

Build with the imported resources described in [build setup](../building.md).
The targets are `mc_test_source_sign_screen`,
`mc_test_source_sign_screen_dirty`, `mc_test_source_sign_drawing`,
`mc_test_source_sign_event_ownership` and `mc_test_sign_gameplay`.

```sh
ctest --test-dir build -R '^client\.sign_screen_(cpu|dirty)$' --output-on-failure
python -B tests/tools/client/test_source_sign_drawing.py --executable EXE --assets ASSETS --artifacts CAPTURES
python -B tests/tools/client/test_sign_gameplay.py --executable EXE --assets ASSETS --artifacts CAPTURES
```

Use the drawing target's binary for the first Python command and the gameplay
target's binary for the second. `ASSETS` is the locally imported resource root;
each runner retains its own capture directory. The drawing runner supports
Windows and Linux, verifies monitor 2 on Windows, and records monitor 0 with the
actual SDL Display identity on Linux. It creates no physical-monitor claim for
a headless display. The ordinary gameplay runner requires Windows monitor 2.

CTest registers `client.source_sign_drawing` and `client.sign_event_ownership`
when `MC_ENABLE_CLIENT_DISPLAY_TESTS` and
`MC_APPLICATION_STARTUP_IMPORTED_RESOURCES` are set. Add
`MC_ENABLE_CLIENT_SIGN_GAMEPLAY_TESTS` for `client.sign_gameplay` on Windows.
The optional `client.sign_screen_sdk` uses the authored icon tree selected by
`MC_SIGN_SCREEN_SDK_RESOURCES`. Native tests leave their imported assets
unchanged and retain failed attempts separately.
