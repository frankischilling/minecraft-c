# Block use and Dirt placement

The client connects both original right-button callers to its actual controller,
Item, World, Inventory and held-item owners. Dirt uses the inherited BlockItem
placement body. Interactive Blocks and distinct Item overrides retain their
reached dependencies. Tools, combat and the Inventory screen remain separate
work.

## Source order

The authority is the original client archive, SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The [static reference packet](../../tests/reference/client/placement/README.md)
records complete declaring-method inventories and the original construction
classes behind the Block kinds. Static matching does not establish an original
executed placement or full visual/audio parity.

Fresh right-button input enters `Minecraft.a(1)` at `k:488` after the existing
focus and 200ms event-age checks. That arm has no click-timer store. Held right
input uses the existing quarter-rate caller1031, whose returned call permits
`ag=V`. Both use the actual SDK queues and Timer.

| Entry | Connected behavior |
| --- | --- |
| `Minecraft.a(I)V` | Captured Block coordinates, current Stack197, saved count209, controller239, successful-use swing251, count0 selected-slot clear289, changed nonzero count equip reset310, and fresh in-air fallback350/reset363. A missing Hit still reaches the right-button fallback. |
| `ob.a(Player,World,Stack,IIII)Z` | Clicked ID7 and actual Block activation31 precede nullable Stack44 and ItemStack use56. Dirt inherits UU's literal false activation. |
| `iz.a(Player,World,IIII)Z` | Actual Item use14 precedes the used-Item statistic `jl.E[stack.id]`34. This statistic is separate from the mined-Block statistic. |
| `ck.a(Stack,Player,World,IIII)Z` | Snow replacement, six independent face offsets with Java integer wrapping, count0 and solid ceiling127 checks, World placement admission127, notifying ID/metadata setter162, placed-face185 and placed-by204 hooks, World sound260, then Stack decrement270. Dirt metadata inherits GM's result0. |
| `fd.a(IIIIZI)Z` | Candidate collision-box getter32 and actual nearby-Entity collision53 precede occupancy. Ignoring Entities discards the box after its getter. Actual water/lava/Fire/Snow singleton replacement precedes the virtual Block placement check130 and its fresh target query. |
| `ob.a(Player,World,Stack)Z` | Original count, actual same-Stack/override use, returned identity/count comparison and fresh selected-slot stores. Dirt's inherited in-air use returns the same Stack without a count change. |
| `ra.b()V`, `ra.c()V` | Each stores only actual equip progress `c=+0`, at2, and returns5. |

A false notifying setter still makes CK return true274. It skips placed hooks,
sound and consumption, but the later ItemStack statistic and Minecraft swing
can still run. Separate receipts distinguish admission, the actual setter
Boolean, sound, consumption, statistic, swing and slot clearing.

Consuming count1 Dirt clears the selected slot and bypasses the immediate
`ra.b()` reset. Ordinary held-item ticks continue afterward. Placement never
clears RA's earlier Stack to manufacture that continuation.

## Owners and failure state

Each admitted controller and Item call owns a retained occurrence and its
reached child diagnostics until checked retirement. The Minecraft caller keeps
its captured local Stack alive across selected-slot removal and the fresh
fallback. A failed action preserves earlier mutations, hooks, sound, count and
statistics; cleanup neither replays the action nor rolls it back.

The placement check uses the actual pooled collision box and nearby-Entity
rule. Retirement releases that exact bounds hold. World sound goes through the
live WorldAccess List and its real render/audio callback. Failed acquisition,
mutation and callback producers remain inspectable; cleanup refusal preserves
their remaining ownership for retry. Successful cleanup detaches borrowed child
views while retaining scalar observations.

Failure classification uses each exact retained callback receipt and the
Registry's immutable initialization cause. Native refusals remain distinct from
source Exceptions, the first `ExceptionInInitializerError`, and subsequent
`NoClassDefFoundError` uses. The shared classifier also recognizes the actual
statistics and translation initialization states reached by the used-Item
statistic.

## Verification scope

Public authored component controls use genuine Registry, World, Player, Chunk
and ownership construction. They require no original archive, assets or JVM.
They cover source branches, unsupported overrides, original callback order and
retained failure state.
Graphics-enabled controls execute actual cold RenderGlobal marking and sound
failures. Authored nested producers retain those real child diagnostics and
their own distinct receipts; they do not manufacture exception names or a
completed renderer constructor. Separate cold Registry controls cover first
and cached failures. These component failures are distinct from the normal
SDK success scenario.

`client.placement_loop` is the local Windows SDK scenario, enabled with
`MC_ENABLE_CLIENT_PLACEMENT_TESTS=ON`, display tests and imported resources. It
extends the [mining and pickup scenario](mining.md): three ordinary World
epochs produce one collected Dirt; actual keyboard movement then backs away
until picking reaches the floor beside the mined cell. A real right-button tap
places the collected block. Returned terrain, empty hotbar and hand frames
precede a third ordinary save/title/reselection and fourth World epoch.

The scenario independently decodes the saved Block ID/metadata, Player and
every saved Item against owned semantic snapshots taken before the actual
leave frame. It does not assign runtime pose, Hit, Timer, progress, actors or
focus flags. Static, component and SDK results remain distinct. Extended play,
default-Far rendering, general Item/Block use and unrestricted reselection are
outside this finite scenario.

Six focused suites, including Skeleton and existing Zombie damage, pass on
Windows MSVC Debug in 1.43 seconds and Linux Clang ASan/UBSan in 16.35 seconds.
Three Windows Release placement runs of the corrected code pass in 25.11, 25.98 and
28.37 seconds. Each records one actual placement, sound, statistic, consumption,
slot clear and swing; count1 correctly produces no immediate equip reset.
Screenshots show the placed Dirt, empty hotbar and empty hand. These checks
observe completed compile calls and a clean target DK without attributing an
individual compilation to that DK.

Failed runs remain separate evidence. One earlier run reached the missing
Skeleton damage dispatch; its deterministic regression is now fixed. Another
exhausted the prior pickup loop's 180-frame bound. The fixture now records
input/focus, Player motion and live Dirt state only on that failure, but later
passes do not establish its cause. The older Mob failure without a nested
diagnostic also remains unclassified. No runtime inputs or acceptance bounds
were repaired to obtain these passes.
