# HUD chat messages

The client displays the missing-Bed notification through the actual LocalPlayer,
translation and HUD message bodies. The loaded Windows scenario shows the
imported English notice after Respawn and observes its message age at 9. It
supplies fatal damage directly and a saved Bed coordinate through authored NBT;
it does not qualify ordinary sleeping or Bed interaction.

## Translation and message ownership

The missing-Bed branch calls `DC.b(String)` at `Minecraft79`. DC captures its
actual App at 1, reads that App's UQ at 4 and calls `UQ.c(String)` at 8. UQ
activates the actual NH translation singleton at 0, performs the property
lookup with the original key as fallback at 6, then calls its ordinary
add-message body at 12. The separate translation wrapper stays outside this
path. The notice comes from the configured resource owner; the runtime supplies
no English replacement.

Each SW message owns its actual immutable String reference and age field.
Translated values retain their Registry namespace; a missing property retains
the same supplied key String. A message may also contain the source null String.
The retained invocation keeps temporary Strings, Font occurrences and a pending
SW allocation live through failure. Earlier inserted fragments survive a later
failure. Checked retirement releases the invocation's workspace while the UQ
List keeps its inserted messages.

UQ measures text through the actual App Font. Widths above 320 split at UTF16
boundaries using the source prefix loop and recursive insertion order. The
remainder becomes the newest line. This body does not carry formatting codes
into a later fragment or treat a newline as a separate chat line. It inserts
each SW at List index 0 and removes the last entry while size exceeds 50.

The ordinary UQ tick decrements a positive record-notice counter, increments
the HUD counter and then increments every SW age in List order, using Java int
wrapping. Clear performs the actual ArrayList clear, including its modCount
increment for an empty List. Native cleanup collects unreachable canonical
SW/String occurrences after clear or completed call retirement. Failed cleanup
keeps the remaining owners for retry. Foreign native List entries are never
read as invented SW objects.

SW ownership, ticking and cleanup stay in the headless GUI library. Font
measurement, translation/add entry calls and drawing remain in the client
resources and draw targets. The separate headless coordinate/World-Bed tests
and GUI constructor test pass; their dependency inspection finds no SDL,
OpenGL or audio libraries.

## Closed-chat drawing

The admitted screen types take the original closed-chat branch. UQ considers
the first ten List entries and draws ages below 200. It computes the source
double fade from age, clamps and squares it, then converts the scaled result
to integer alpha. A zero result skips the line.

Each visible entry draws its real UB background rectangle with half the text
alpha, enables blending and invokes the captured bitmap Font's shadow draw.
The origin is `(0, scaledHeight - 48)`; text starts at x=2 and successive List
indices move upward by 9 pixels. These operations use the actual Display,
Tessellator and Font owners and retain reached effects on failure. The native
fixture checks ten considered entries, eight drawn backgrounds and texts, and
the selected age/fade values. It saves the returned frame for inspection.

## Verification and limits

The [original component packet](../../tests/reference/client/gui_ingame_chat/finite.json)
contains 81 exact rows from two unchanged-original JDK17 VMs. It covers null and
empty text, UTF16 content, formatting, width splitting, List growth and cap50,
translation identity and fallback, age overflow and clear. Its App is a
prepared original `kq`; its SJ Font has declared widths of 7 and space width 4.
FP and language resources are authored inputs. This evidence does not include
original drawing or complete App/Font construction.

The positive native fixture constructs the genuine App and Font through the
existing startup graph. Its component replay uses a separately constructed UQ
with the same App, Font and class namespace. The startup UQ remains unchanged
for the later HUD draw. This distinction matters: startup's genuine main-menu
display already clears its empty message List once. The first comparison
reused that UQ and failed every case header by modCount +1. A fixture-only
correction selected a fresh real receiver; the original packet and production
code did not change, and the failed records remain retained.

The corrected replay passes all 81 rows on Windows in 2.33 CTest seconds and on
Linux with ASan/UBSan in 16.82 seconds. The Windows fixture completes 791 native
checks, including the separate constructed-Font draw. The expanded CPU fixture
passes 158 checks for real null App/Font failures, NH initialization before a
null key, and retained-call mutation/retirement refusal. It also checks absent
language/statistics streams at the actual NH constructor loads 26/40, repeated
active use of each failed namespace, and a distinct missing-provider native
failure. Classification reads the immutable initialization cause. It initializes
no Video or GL.
Packet corruption controls pass in normal and optimized Python.

Run the registered cases `client.gui_ingame_chat_cpu`,
`client.gui_ingame_chat`, `tools.gui_ingame_chat_packet` and
`tools.gui_ingame_chat_packet_optimized`. The
[native driver](../../tests/tools/client/test_gui_ingame_chat.py) retains process
receipts and checks source, product, packet and authored-resource continuity.
Its 120-second native deadline leaves cleanup and receipt time beneath the
180-second CTest watchdog.
The corrected component cohort kept all 5,406 source inputs unchanged.

The [saved-Bed scenario](death-respawn.md#saved-bed-respawn) separately qualifies
the imported notice in a loaded native World. Typed chat input, an open chat
screen, scrolling, multiplayer delivery, original pixels and performance remain
outside this evidence.
