# Original held Item attack observations

This packet observes unchanged Beta1.7.3 controller attacks through genuine
App, World, Chunk, LocalPlayer, target and ItemStack constructors. It contains
422 exact selected integer/Boolean rows and a separate three-row lethal Cow
scope. It does not change the five unarmed rows in `../player_attack/`.

`cases.json` declares every input before invocation. The first32 rows reproduce
the retained Dirt/Wool research exactly. The expanded cases cover all22 direct
GM Item IDs, all38 actual runtime Item classes,20 tools/swords across durability
boundaries, signed damage overflow, zero/negative Stack counts, hurt rejection,
fatal Sheep and16 Saddle/Pig/non-Pig/already-saddled combinations.

Expanded nonfatal durable cases declare Cow health20. The separate
`cow_cases` declare health10: a Diamond Sword really kills that Cow with damage10
(11 while falling), including a simultaneous sword break. Both groups execute
the original bodies; health is never repaired during an invocation.

`compare.py` parses `HELD_ITEM ` JSON using exact typed schemas and compares all
41 selected fields. `audit.py` independently checks material damage, Java32-bit
overflow, strict damage>max break behavior, statistics, Stack clearing, Saddle
effects and target results. The 422 rows match in two independent ordinary VMs.

`cow.py` parses `HELD_ITEM_COW ` JSON. Each process's actual Cow-private48-bit
before-state independently determines its two sound Float draws, pitch,
`nextInt(3)` rejection loop, Leather quantity and ordered publication receipts.
The actual raw drops remain recorded: the two original ordinary cases produced
2 and0 Items. Only the explicitly selected40 fixed fields are compared across
independent entropy after both raw observations pass their own audits. Player
and World private Random states are checked unchanged. Item constructor private
states are retained and their shapes checked; their complete entropy/kinematics
are not reconstructed here. No Random is seeded/rebound by these probes.

`item-inventory.json` records all202 actual Item IDs, immediate superclasses,
combat/render method owners, max damage and original icon/color at metadata0.
It also records26 tool/sword/hoe/rod metadata15 calls and actual full3D/rotate
booleans. Complete original method inventories and Code hashes are in
`source-pins.json`. UY metadata15 color dependencies are not forced.

The prepared CPU scenes warm Item/statistic initialization. This is not a cold
initializer, graphical, audio output, complete World graph, full entropy or
whole-session parity claim. The source callbacks only read actual owners.
Network and native-library attempts are denied; original statistic-writer IO
is allowed inside the disposable prepared game directory. No substitute game
classes, uninitialized game allocations or invocation-state repairs are used.

Capture with the supplied verified JAR, libraries and JDK17:

```powershell
python -B tests/reference/entity/held_item_attack/capture_reference.py --jar b1.7.3.jar --libraries "$env:APPDATA/.minecraft/libraries" --output .local/held-item-capture
python -B tests/reference/entity/held_item_attack/test_packet.py
```

The optional `--research` argument independently audits the retained32-row
research capture. Raw local classes, original outputs and tools remain ignored;
the packet publishes authored helpers, selected observations and hash receipts.
