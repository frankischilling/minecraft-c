# Loaded Signs

Saved standing and wall Signs construct real native TileEntity owners and reach
the ordinary World renderer. Four held UTF-16 lines, NBT, model and Font drawing,
state-dependent wall bounds, inherited empty ticking, and client support/removal
hooks are implemented. Placement and the editor remain open.

## Text and storage

The [original component packet](../../tests/reference/world/sign_nbt/README.md)
retains constructor, NBT read and NBT write observations from eight fresh VMs.
The main cohort has 22 cases and 101 selected calls per VM. A separate four-case
cohort supplies public null-line inputs to the actual original writer.

Construction gives four shared empty values and edit row -1. Reading sets the
private read flag false before reading coordinates, then assigns lines in order.
Each line longer than 15 UTF-16 units is truncated to that length. This can split
a surrogate pair. Earlier coordinate and text assignments survive a later wrong
tag. Writing preserves full line lengths. A null line fails in the original
String-tag constructor after earlier output fields have been written.

Native line getters return independent holds. Native text copies do not claim
the original Java String-reference aliases. The owner admits four lines and
nullable individual entries; arbitrary replacement or nulling of the entire
public Java array remains outside its API. Fresh source NBT and storage overlays
have separate unknown-field contracts. Native immutable builders do not claim
mutable original Compound failure prefixes or original binary tag order.

## Rendering and lifecycle

The Sign renderer uses the existing board and pole model parts, their lazy draw
cache, the real RenderEngine texture lookup and the BitmapFont. Standing Signs
use all 16 metadata rotations; wall Signs use the original facing angles and
fallback. Wall bounds read through the actual World or ChunkCache receiver.
Text centering uses Java integer division, including odd widths. Selected-row
markers preserve both spaces in `> ` and ` <`.

The renderer records held text arguments and successful row results. A Font
failure preserves the reached GL prefix; native retirement releases owners
without inventing the original method's missing matrix pops or depth restore.
The terminal null-Font comparison exercises that distinction.

Client support checks read the actual Material below a standing Sign or behind
a wall Sign with metadata 2 through 5. An unsupported Sign reads fresh metadata,
drops its Item, then requests notifying removal. Retained addition/removal uses
the normal factory and World/Chunk owners, including lazy Sign construction,
the removal body's two Chunk acquisitions, and nullable receiver failures.
Native controls cover these hooks and their failure prefixes; they do not
qualify dedicated-server behavior or arbitrary recursive mutation graphs.

## Formatted Unicode

BitmapFont, CPU layout and font commands share the
[qualified whole-String hex projection](../../tests/reference/core/string_lower_hex/README.md).
It admits formatted Unicode such as `§béΩ` when possible default-Locale lowercase
paths preserve every UTF-16 position and the selected color-code result. The
whole input is checked before projecting a format site's original index.

Potential insertion or removal remains unsupported: examples include dotted
capital I, the relevant Lithuanian I/J combining-mark contexts and precomposed
I accents, and Turkish/Azerbaijani I plus combining dot. The check conservatively
refuses a possible width change even after the selected format site. Unformatted
text and a trailing section sign do not reach this lowercase dependency. This
is a bounded hex projection, not general Unicode or Locale lowercase support.

Two fresh JDK17 VMs agree on observations for all 1,114,112 codepoint values,
including 2,048 isolated-surrogate values, plus 222 Locale/context rows each.
Complete special-case source review and combining-class observations support
the conservative domain. Windows core, layout and command controls pass. A
genuine SourceFont control on monitor 2 passes 680 checks. Its final unsafe
U+0130 input reaches the native refusal at SJ178 after queuing an earlier glyph.
The GL/NIO prefix survives both an independent alias release and final native
retirement without Source GL repair.

## Finite validation

The complete serial MSVC Release sweep passes all 1,380 tests in 1804.48
seconds. All 1,372 preceding names remain plus exactly eight additions, with
complete inventory/log/JUnit and Start/completion joins and no skips or unrun
tests. All 6,601 public files and 689 native products stay unchanged during
the sweep. The preceding local suite used four workers; these elapsed times
are not a performance comparison. Later validation annotations, admission
fingerprints and CRLF whitespace declarations preserve the tested native
code and products.

After the Unicode Font change and reviewed null-World removal fix, seven GCC
and eleven Clang tests pass in 35.11 and 40.38 seconds. Both enable ASan, UBSan
and leak detection and stop on undefined behavior. They include 471 Sign
lifecycle checks; Clang also covers CPU Font layout/commands, the shared Tile
draw controls and 637 Sign bounds/draw-gate checks. These are affected
selections, not full local Linux suites or performance measurements.

The [original draw packet](../../tests/reference/client/sign_draw/README.md)
retains the offscreen GL observations for 36 cases in interpreter
and default VMs: 35 normal returns and a terminal null-Font failure. Native
replay passes 4,760 selected checks against the original owner/model/Font/NW
state and represented GL core/facade fields. Cases cover all standing rotations,
wall facings and fallbacks, shared board/pole compilation history, formatted and
Unicode text, surrogate boundaries, selected-row markers and odd-width centering.

Both V2 debugger observations provide 140 actual Font measurements and 140 draws
per VM. Their exact raw UTF-16 arguments, including isolated surrogates, join to
native argument lengths/digests and exact widths and draw coordinates for the
35 completed cases. This corrects the earlier observer transport's inability to
retain an unpaired surrogate. The original active Font buffers remain separate
evidence: no native active GL-list payload equality is claimed.

Cold legacy matrix values, texture matrices/stacks and display-list mode/index
remain outside the represented native GL comparison. The offscreen results do
not establish visible-window, physical-monitor or pixel equality.

The final eight-process Windows SDK sweep uses physical monitor 2: standing metadata
0/4/8/12 and wall metadata 2/3/4/5. Each loads one prelaunch saved Sign, reaches
the ordinary Timer, World, HUD and Sign draw, saves through the menu, reselects
the world, and closes both native windows. Their checks are 1,009/1,000/967/963
and 1,002/1,012/976/966 respectively. The prelaunch text, actual Font arguments and
independently decoded saved text agree on 31 UTF-16 units. The runner checks
source/product continuity and imported-asset continuity. A separate read-only
audit checks all 48 Sign draws, 32 image files, 40 selected saved Chunk/metadata
copies and 2,398 shared source/helper/product pins across the eight processes.
Complete 413-file asset inventories agree before and after the sweep. These
passes include the shared Unicode Font change and null-World removal fix.

Current monitor-2 pushed-Stone, Furnace and dense-Chest regressions also pass
961, 9,710 and 971 checks through ordinary save/reselection and checked closure.

The first SDK process completed 967 native checks, but its result reader used a
reversed tick-baseline inequality and rejected the run. That failure remains
retained. The corrected reader has positive and negative baseline controls; all
eight later processes pass it.

These are bounded loaded-world and component scenarios. They establish no
sustained-play, gameplay FPS, item-placement, editor, networking or complete Sign
parity result.
