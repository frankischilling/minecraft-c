# Item icons and foliage color

Sapling and Leaves Items now use their original icon overrides. Each reads the
live named Block static (`UU.z` or `UU.L`), then calls that receiver's texture
method with side 0 and the unchanged signed damage value. The Item's cached
icon and numeric Block-array slot do not replace these reads. A captured NULL
Block fails at the original invocation position.

Sapling selects icons 63 and 79 when `(damage & 3)` equals 1 and 2 respectively;
other values use the live inherited texture field. Leaves adds 80 to its live
texture field when `(damage & 3)` equals 1. That addition wraps as a Java signed
integer. A genuine pre-change Linux UBSan probe with texture `2147483647`
reported signed overflow at this addition; the current sanitized replay
checks the wrapping boundary against the executed original method.

Leaves Item color tests damage mask 1 first, then mask 2. It invokes the actual
Foliage Colorizer child for pine, birch or default color: 6396257, 8431445 or
4764952. Even these constant-return methods must first initialize their
declaring class, including its 65,536-element integer array allocation. The
GUI, billboard and held-Item callers supply their actual graphics owner and
reader. Cold initialization, allocation limits and cached failures retain
their typed child causes. The inherited white Item color ignores that context.

This uses the existing shared icon and color dispatch. Ready constant color
calls do not read the color array, climate, World or floating-point state.
Native controls check unchanged Registry allocations and RNG state; these
facts are not a measured rendering-speed improvement.

## Original and native comparison

The [original appearance packet](../../tests/reference/item/appearance/README.md)
comes from two fresh ordinary VMs running unchanged methods from the supplied
Beta 1.7.3 client. Both produce identical observations for all 202 registered
Items, 38 receiver classes and twenty signed damage operands. The packet
contains 202 class/declaring-owner rows, 4,040 direct/actual-ItemStack appearance
rows, 64 mutable-texture rows and a final completion row.

Windows MSVC Release and Linux Clang Debug ASan/UBSan match all 4,307 selected
lines literally. All icon, color, full-3D and rotation results are included.
The packet has 49 corruption controls; the native comparator has 35.

The first Windows comparison failed on transport: native bootstrap output
used CRLF while the probe's data rows used LF. All data values already matched.
The comparator now requires the exact platform bootstrap and strict LF data
segment. It neither normalizes values nor discards unknown rows; the failed
output remains retained.

The native Inventory Block fixture passes 19,999 checks per host. Added controls
cover RAW and registered receivers, actual Stack wrappers, named-static versus
array aliases, NULL and rebound Blocks, live texture mutation, signed edges,
all low metadata bits and additional signed operands. Color controls exercise
cold/ready/failed Colorizer states, missing context and allocation limits,
including the retained first cause and later cached failure.

## Native GUI drawing

A separate native GUI fixture bootstraps the actual application, SDK, Display,
RenderEngine and a distinct ItemRenderer. It makes sixteen genuine GUI calls:
Sapling and Leaves, damage 0 through 3, with the renderer tint flag false and
true. Each host passes 350 checks through returned and retired calls, exact
icon/color receipts, matrix restoration, ownership refusals, window removal
and native cleanup. Leaves submits six faces; Sapling submits four quad
vertices. The color getter runs even with tint disabled; the flag gates its
application to GL color.

Windows completes in 5.67 CTest seconds. Linux software GL completes the full
outer cohort in 12.64 seconds with ASan/UBSan, leak detection and halt-on-error.
Natural bootstrap cleanup takes four calls/three BUSY returns on Windows and
26 calls/25 BUSY returns on Linux. These are this fixture's joins, separate
from historical Workbench captures.

All sixteen RGBA captures per host are nonblank. Sapling's actual opaque atlas
samples agree with the declared native color operation within two RGB8 units;
Leaves checks exact source color and six submitted faces separately from its
nonblank framebuffer. These are imported native-texture checks, without an
original-pixel comparison. The captured cohorts preserve 1,457 selected inputs
on Windows, 1,456 on Linux, and 413 assets on each host. Linux uses a private
mount of the actual account game-directory path; the host directory's contents
and filesystem identity remain unchanged.

Six parent regressions pass on both hosts: first-person hand, Dye icons,
Item billboard, held extrusion, specialized Inventory geometry and Piston
Inventory texture. This component fixture creates no World and runs no ordinary
input or loaded-play loop. [Plant placement](../world/plant-placement.md) now
has separate ordinary Sapling/Flower input and save/reload evidence. Other
Block hooks, loaded family coverage, original pixels/audio and sustained FPS
retain their separate unfinished scopes.
