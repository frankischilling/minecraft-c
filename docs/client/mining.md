# Empty-hand mining and dropped blocks

The client connects empty-hand survival mining to the existing SDK input,
singleplayer controller, World mutation and Item owners. Dirt is the first
complete interaction scenario. Tools, combat, right-click placement and the
Inventory screen retain their own reached dependencies.

## Source order

The authority is the original client archive, SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The [static reference packet](../../tests/reference/client/mining/README.md)
pins 19 classes and 58 methods, including the complete Glass and Log method
inventories. Its bounded verifier reads the archive without loading classes
and passes on Windows and Linux. The implementation follows these bytecode entries:

| Entry | Connected behavior |
| --- | --- |
| `Minecraft.k`, fresh click462 and held click980 | Real SDK events, source focus and 200ms age checks, Player swing and the return-only `ag=V` store. |
| `Minecraft.a(I)V` | Left-button suppression, swing20, miss delay, and actual Block controller call184. Entity combat and other buttons retain their reached boundaries. |
| `os.a(IIII)V` | Face-neighbor fire query19, target read32, initial Block hook74 and strength95; source comparison controls immediate destruction109. |
| `os.c(IIII)V` | Cooldown, coordinate changes, repeated-coordinate strength/progress, step sound, destruction and return-only progress resets/cooldown. |
| `Minecraft.a(IZ)V` | A returned controller progress call102 precedes actual DN hit particles121. |
| `os.b(IIII)Z` and `ob.b(IIII)Z` | Capture ID/metadata, event2001 before notifying air mutation, removal hook, harvest permission and statistic/drop. |
| `dn.a(IIII)V`, `dn.a(IIIII)V` and `qm` | Actual hit placement and 4 by 4 by 4 destruction loop, own Random, real QM construction/tint, inherited XW tick and layer-one terrain-atlas drawing. |
| `bb.a(Lhl;DDDFF)V` and `cv.a(Luu;IF)V` | Dropped-block count, bob, rotation, seeded scatter and six inventory-Block faces. Collected Dirt uses the real GUI and held-block paths. |
| `em.a(NW,FFFFFF)V:286` | The pickup arc calls TH's coordinate overload with the already narrowed/widened coordinates, picked yaw and actual partial argument. A picked Item remains drawable after death and a zero Stack count. |

Negative hardness returns before Player work. NaN hardness reaches Player work,
as prescribed by `UU.fcmpg`; NaN progress does not trigger destruction through
`OS.fcmpl`. Water and airborne penalties divide speed by five in their original
order. Inventory reads use the actual selected Stack. Unsupported tools fail at
their reached invocation rather than borrowing empty-hand behavior.

The World auxiliary method reloads the live WorldAccess List as it advances.
For event2001, RenderGlobal returns from the break sound before constructing
and appending all 64 QM particles. These effects precede the notifying air write.
Face-neighbor Fire dispatches event1004 before its own air write; the concrete
unimplemented effect remains a reached failure when required.

## Owners and failures

Each controller call owns a retained receipt and controller occurrence. It
preserves completed stores, mutations, sounds, particles, statistics and Item
publication if a later child fails. Retirement releases the exact captured
children without replay or rollback. Cleanup refusal keeps diagnostics live
for retry.
HUD cleanup copies compact CV source/graphics diagnostics before releasing an
Item child, then detaches the freed workspace pointer. Terminal cleanup failures
use separate storage from retry results. A real entered Font call exercises
partial retirement, repeated refusal, stable diagnostics and successful retry.

Pickup drawing owns each reached TH call until checked retirement. Successful
calls retire before the next pickup; failed calls retain their actual child
diagnostics. Copied per-invocation receipts preserve scalar observations after
those views end. A returning two-pickup control also checks that rejected work
does not consume another caller's pending TH draw.

DN retains one exact block-effect operation until checked cleanup. A rejected
second caller cannot consume that work or its Random draws. Failed WorldAccess
receipts retain their own producers; deduplicated occurrence holds cover all
diagnostic routes. Particles and dropped Items remain genuine Entity owners in
the existing World and terminal-retirement graph.

BB uses its actual inherited BW manager and shadow fields. The following
no-normal shadow batch exposed a backend mistake: direct normal arrays had
discarded the prior current normal. Separate unchanged-original NW observations
establish retention on the exact NVIDIA 581.42 and Mesa 25.2.8 profiles. The
[finite observation manifest](../../tests/reference/client/lighting_raster/normal-transition.json)
records inputs, queried and no-query controls, consumed normal words and pixels.
Other drivers retain an explicit unqualified boundary. This evidence does not
establish a complete original BB frame or general OpenGL behavior.

## Verification

Public authored tests cover mining leaves and numeric branches, controller
coordinate/failure ownership, live auxiliary callbacks, inventory-Block
geometry, genuine QM construction/ticking/drawing and refused competing work.
They run without the original archive, assets or a JVM. The focused controls
pass on native Windows MSVC and Linux Clang with AddressSanitizer and
UndefinedBehaviorSanitizer. The following normal transition has separate native
lit/unlit and queried/no-query regressions on both measured graphics profiles.

`client.mining_loop` is a local Windows SDK integration test, enabled with
`MC_ENABLE_CLIENT_MINING_TESTS=ON`, display tests and imported resources. It
authors saved Player/terrain inputs before bootstrap, then uses ordinary frames,
SDK mouse/key events and menu save/reselection. It never assigns runtime pose,
Hit, Timer, progress, actors or focus flags. Its required sequence is a fresh
click, held Dirt break with visible cracks/QM, returned dropped-Item frames,
save/title/reselection with the live Item, keyboard approach and pickup,
nonempty HUD/hand frames, and a second save/title/reselection. An independent
Python decoder checks actual region and Player records at each save.

Three complete Windows MSVC Release sequences pass in 21.53, 22.39 and 19.46
seconds with checked window/owner cleanup; the final run completes 1,843 native
checks. Each observes a genuine fresh click, partial cracks, ticking/drawn QM,
three returned Dirt BB frames and three nonempty HUD/hand frames. Disk decoding
searches all allocated Chunk records and follows the actual dropped Item
location. Each run verifies its absence after pickup while
Inventory retains one Dirt. Incidental seed Items remain included in the full
live-entry/disk comparison. Source, executable and imported-resource hashes
remain unchanged through the run. The [validation record](../../tests/reference/client/mining/native-validation.json)
identifies its inputs and finite results.

A preceding Debug run under parallel build load exhausts its 12 real taps:
rendering takes 225–1,285 ms, so the original 200 ms rule discards every event.
That failure remains a configuration/performance limit; no clock, Timer,
progress, freshness threshold or click assertion is repaired to obtain a pass.
A separate Release run reaches a failed natural Mob tick during the second
pause transition, after pickup. Its selected descendant remains unresolved.
Failure-only diagnostics now expose that child for follow-up; three passing
runs do not qualify stable unrestricted play.

Linux SDK integration, original mining pixel/audio comparisons,
default-Far performance, extended play and unrestricted World reselection
remain unqualified. The client remains pre-playable.
