# Automatic Workbench close

The Workbench validity check belongs to the Player tick. It runs while the
screen remains open because Workbench does not pause the World. The client
must close that screen synchronously when its Table disappears or the Player
moves beyond the original distance limit.

## Original call order

The unchanged client archive identifies this sequence:

1. `GS.w_` completes the inherited LivingEntity tick at111, then reads the
   current World's remote flag at118. Remote Worlds skip the Container check.
   A null current Container also skips it.
2. The current Container receives `DW.b(Player)` at136. `IT` first reads the
   Block at its own coordinates and compares it with the current registered
   Crafting Table ID. A mismatch returns false before the distance child.
   A matching Table permits squared distance at most64 from its cell center.
3. A false result invokes virtual `Player.r` at143. For the LocalPlayer this
   is `DC.r`, which invokes `GS.r` at1. That superclass freshly reads default
   Container `d` at2 and stores current Container `e` at5.
4. `DC.r` freshly captures its Minecraft field at5, then calls
   `Minecraft.a(Screen)` with null at9. A null receiver fails at9 after the
   superclass store. Display replacement closes the old screen before storing
   the new screen at106 and entering its ordinary focus path at165.
5. `OO.h` first calls inherited `ID.h`. That child closes the Player's current
   Container, which is already the default Container. `OO` then independently
   captures and closes its own `IT`. The two calls preserve separate Player
   captures and diagnostics. Cursor and crafting-grid drops remain real World
   publications; the original Workbench grid aliases are not explicitly cleared.
6. After the entire `DC.r` child returns, `GS.w_` freshly reads `d` at148 and
   stores `e` at151. Smoothing starts at154 and the play-time statistic follows
   at387. An interrupted close suppresses that store and suffix.

The final default-Container read cannot reuse the earlier `GS.r` value.
Synchronous close and WorldAccess callbacks may change the live fields between
those reads. Display owns focus, mouse capture and its clock queries; automatic
close adds no separate Escape event or focus operation.

## Native connection and diagnostics

The [Player close host](../../include/mc/entity/player/close.h) describes one
synchronous, complete `DC.r` child using client-independent types. The optional
host travels with the existing Player action context. The
[Player tick](../../src/entity/player/player_tick.c) invokes it at the actual
virtual-call site before any close prefix. Without a host, the component path
retains its explicit unfinished display dependency.

The [client World tick](../../src/client/runtime/world_tick.c) supplies the
adapter from its genuine App and retained transition graph. It calls the
existing full Player-close entry, which executes `GS.r` once, reads the actual
LocalPlayer's Minecraft receiver and performs synchronous display replacement.
The simulation library does not depend on GUI code.

Neutral Player diagnostics contain only values and static text. The client
record keeps the typed screen, initialization, Container, drop, publication
and statistic failures from the reached invocation. Successful native cleanup
ends producer borrows; refused cleanup preserves still-live views. Cleanup
does not rerun the close or alter source fields.

On x64 MSVC, the World-tick record grows from 499,736 to 522,168 bytes.
The added 22,432-byte diagnostic region is reset on each actual World tick.
The Player-tick error grows by 96 bytes; the enclosing World Entity-tick error
keeps its existing 336,152-byte size. This adds no allocation. Existing Player
scratch allocations include the extra 96 bytes. Valid Workbench ticks do not
invoke close or copy failed children. These are storage measurements, not
frame-time or FPS results; no hotspot or speedup is established.

## Qualification

The final runtime passes all 631 Windows public tests in 361.32 seconds and
all 626 Linux Clang 18 Debug ASan/UBSan public tests in 208.75 seconds, with
leak detection, halt-on-error and Xvfb/software GL. Windows keeps 4,298 authored
files, 514 products and 823 assets unchanged; all 4,298 transferred files remain
unchanged on Linux and all 1,426 runtime files match between hosts. Formatting
passes for 1,982 C/header/fragments. Ten changed translation units also pass
GCC syntax/shadow checks using their actual target arguments; that check does
not execute GCC-built gameplay. Later documentation/binding updates receive
separate checks and do not alter runtime source.

An earlier Windows full-suite cohort passed 630 of 631 cases. Remote Desktop
took the foreground during the remaining Squid graphics case; its retained
native focus snapshots identify that process and an isolated recheck passed.
The final full cohort above passes all cases. Neither cohort establishes
global desktop focus stability or a performance comparison.

The [original packet](../../tests/reference/inventory/workbench_auto_close/README.md)
retains seven calls in each ordinary and read-only debugger process. Four
enter the actual Player tick: Table removal, excessive distance, removal with
shared Stack aliases and a valid Table that stays open. Two separate predicate
calls distinguish squared distance64 from the next declared value outside it.
A separately labeled direct `DC.r` call does not earn Player-tick coverage.

Each process publishes nine actual Items. The observations preserve the
default-Container cursor drop before one Workbench grid loop, same-Stack
aliases, focus recursion, Random progress and the returned Player-tick suffix.
The debugger retains 625 events, 283 matched watched entries and returns,
45 selected Player phases and 31 entered live Code pins. Watched descendants
include JDK and LWJGL methods. The 112 captured inputs and five compiled helpers
remain unchanged. All 55 Java-free publication tests pass.

Two earlier cohorts retain observer failures: an overly narrow focus-recursion
assertion and a dotted/archive class-name lookup mismatch. Their source calls
returned; neither is classified as an original invocation failure.

Eleven authored native Player-tick controls cover present/removed Tables,
exact/just-outside distance, remote Worlds, null current Containers, whole-child
entry before the first restoration, failures and a fresh default-Container
read. These controls use a declared child adapter and do not credit the real
GUI connection.

The [native receipt](../../tests/reference/client/workbench_auto_close/native-validation.json)
binds all 1,426 runtime source files to five fresh Windows MSVC Release
processes. Four CTest cases pass in 102.10 seconds. Removal passes 747 checks
in 19.38 seconds; distance passes 725 in 21.71 seconds. Genuine SDK input opens
the authored Table and moves seven Planks to the cursor and two Sticks to its
grid. The trigger then uses the real World Block setter or Entity position
setter as an explicitly authored integration input. Neither is claimed as
SDK mining or keyboard movement.

Exactly one client World tick closes each screen, restores the default
Container and publishes both captured Stacks in order. Each actual structural
row matches its selected original primary row. The source grid alias remains.
Three later World/HUD/equipped-Pickaxe frames return, followed by ordinary Save
and Quit. Independent decoding checks all 40 Inventory cells, every live Item,
exact material totals, removed/retained Table terrain and unrelated region
bytes. Removal saves eight Items across 392 Chunks; distance saves two across
394 Chunks. These include unrelated natural Items and differ from the original
GS-only observation scope.

The two additional processes exercise a native WorldAccess callback during
the second grid-Item publication. It changes the default Container to a
separately owned actual AA after the inherited close has returned. Failure
then refuses that native callback; success returns normally. The resulting
505/500 checks distinguish suppressed GS151, smoothing and play-time work from
the successful fresh default read. Typed failure values survive repeated
cleanup and transition/bootstrap teardown. These are authored callback controls
with no ordinary-save claim. Source fields are restored only after every
observation for independent fixture teardown.

The first fresh-default control moved ten blocks and incorrectly assumed that
the old smoothing position would always stay unchanged. A retained diagnostic
shows the original greater-than-ten clamp executing because the earlier
smoothed coordinate was slightly short of its limit. The qualified input moves
to x=9.0 and checks every smoothing delta stays within ten. Runtime smoothing
is unchanged. Actual Source focus may return early for an inactive Display;
later SDK acquisition occurs after the complete close observations.

The crafting regression also passes, with 1,817 checks in 27.14 seconds,
ordinary Save and Quit, exact Inventory/all-Item NBT and cleanup. Its five
saved Items and 409 Chunks retain their own live scene scope. All selected
source/harness files and 413 imported assets remain unchanged during each run.

## Live pickup particles during native cleanup

The native terminal audit previously rejected complete EM pickup particles,
even though ordinary insertion and ticking already supported them. EM owns
the picked Entity and nullable target independently. A picked Item may have
left the World Entity list while that particle still holds it.

The [EffectRenderer audit](../../src/client/particles/effect_renderer.c)
now admits canonical layer-3 EM with its actual completed constructor and
qualified actors. The [World audit](../../src/world/runtime/terminal_inventory.c)
visits each distinct EM once, counts both actual owning field slots, and
discovers their actors and World backedges. Duplicate DN occurrences and
aliased actors retain their independent hold counts. Extra external holds,
malformed constructors and active calls still refuse cleanup without mutation.
No particle is aged, erased or marked dead to obtain admission.

Seven selected Windows tests pass, including real DN and GAME World/cache
regressions for duplicate/shared/nullable actors, exact occurrence counts,
extra-hold refusal and unchanged graph, Random and source fields. A prior SDK
run completed Source Save and Quit but refused effect-manager cleanup. A
diagnostic rerun passed without exposing that deeper refusal, so its particular
cause remains unconfirmed. The independently reproduced live-EM rejection
justifies this extension; it does not establish Java GC timing equivalence.

Existing keyboard-close crafting and save results remain documented in
[Workbench crafting](workbench.md). Those results alone do not verify this
Player-tick caller.

[Native shutdown with an open Workbench](workbench-open-shutdown.md) now has its
own ownership transfer and four Windows profiles. This connection does not
qualify unrestricted Containers, original pixels, Linux SDK gameplay, extended
play or FPS.
