# Cloud drawing

The default World frame now invokes both cloud modes through the actual
RenderGlobal captured earlier in the frame. After clouds return, it disables
fog, performs another fog setup and clears depth before the first-person hand
call at `px.a(FJ)V:997`. The [empty hand and dry overlays](first-person-hand.md)
now return, allowing World drawing to return1013. Its parent then composes
[GuiIngame](gui-ingame.md) and returns the focused loaded frame. The next
unpaused tick reaches Mob construction; interactive gameplay remains unfinished.

## Source methods

The [cloud API](../../include/mc/client/world/render_global_clouds.h) and
[implementation](../../src/client/world/render_global_clouds.c) execute
`n.b(F)V`, including its `n.c(F)V` fancy child. Authority is the verified client
archive in the [binary audit](../reference/binaries.md). The
[method identities](../../tests/reference/client/clouds/source-pins.json) and
[operation map](../../tests/reference/client/clouds/operation-map.json) pin the
669-byte flat entry and 2,085-byte fancy body.

The entry reads the current App World and its Dimension's Nether flag before
Settings or graphics. Nether returns at 16. Otherwise the current fancy-graphics
setting selects the drawing body. Native state reports the reached flat or
fancy source position, including child failures.

Both modes interpolate camera Y from `bm`, but interpolate X/Z from `aJ/aL`.
They add the original tick-dependent X drift, wrap texture coordinates using
Math floors and retain float/double operation boundaries. Fresh World reads
supply actual cloud color and Dimension height. The inherited `xa.d()F` returns
108, while Sky's `ay.d()F` returns 8. The existing World color method supplies a
real pooled Vec3. Anaglyph conversion reads the current Settings at its reached
source position.

Flat clouds draw a 16-by-16 grid of 32-block quads in one NW batch. Fancy clouds
scale the current matrix by `(12,1,12)` and execute two passes over a six-by-six
tile grid. The first pass masks color writes. The second selects its color mask
from fresh Settings and the actual PX eye field. Each tile emits the reached
top/bottom faces and eight-strip sides with the original offsets, normals,
shading and texture coordinates. Actual NW methods perform conversion and
submission through the source Display.

Both drawing paths finish with white current color, blending disabled and
culling enabled. Fancy drawing leaves its scale and final color mask applied.
The source has no matrix push/pop or restoration at that boundary. Later frame
operations receive those effects.

## Native lifetime

A retained cloud call holds the actual N and reached World, Dimension, camera,
Math, Engine and color-vector occurrences. The pending call blocks other N
draw/mutation entries and terminal retirement. Source App and the complete graph
remain live and exclusive through checked cleanup. Reads remain fresh where the
source reloads fields; retained references do not replace later source reads.

Failures are terminal and preserve completed source effects. Retirement releases
native rights without drawing, replaying the source, restoring matrices, rolling
back fields or resetting NW. It retires only a texture attempt created by that
call. A foreign pending lookup survives. Refused cleanup preserves remaining
rights for another cleanup attempt. Borrowed diagnostic views end with their
actual owners.

## Evidence

The [reference fixture](../../tests/reference/client/clouds/README.md) runs 72
unchanged-original cloud calls. It crosses Overworld, Nether and Sky, flat/fancy
settings, three camera heights, anaglyph eyes and wrapped negative coordinates.
Two fresh original captures produce identical post-call state and final NW
batch hashes on the recorded NVIDIA compatibility context.

Native Windows and Linux controls compare all 72 post-call rows and 72 final-batch
hashes exactly. The observer performs the original current-color query before
matrix/state inspection. This matters on Mesa, where the query commits staged
attributes; passive ledger inspection is a different operation. The
[material commit contract](lighting-raster.md#material-commit-frontiers)
describes that distinction.

The native fixture uses actual App/startup/N owners with prepared World and
camera operands, authored white textures and the real OpenAL Soft null playback
backend. Its exact expected no-capture warning is retained in the report; other
diagnostics still fail the fixture. It makes no audio qualification claim.
Additional controls cover visible cloud pixels, foreign texture ownership,
pending-call guards, null App and checked cleanup.

The original geometry hashes cover the final NW batch, not every fancy tile.
These finite results do not establish complete-frame or original pixel equality,
unprepared World/camera construction, texture cache misses or gameplay.
The loaded-World test separately reaches the hand invocation at 997 using its
actual saved-World graph. Full Entity rendering, hands, HUD and repeated
interactive frames remain unfinished.

## Qualification

All 48 selected production/test targets build on Windows MSVC Debug and Linux
Clang18 ASan/UBSan. The selection passes 57 Windows tests in 75.89 seconds and
53 Linux tests in 96.70 seconds, without failures, skips or sanitizer findings.
All 2,327 C/helper/build inputs stay unchanged through builds and tests.
Formatting checks 1,638 files. The cloud control passes 2,518 native checks on
Windows and 2,529 on Linux, with all 72 state and 72 final-batch comparisons exact.
The Windows selection suite preserves saves/resources and 72 focus histories
while reaching PX997 on the actual loaded graph.
