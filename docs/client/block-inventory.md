# Block Items in inventory rendering

The client resolves the original inventory color and shape methods for all
69 represented Block kinds. This lets ordinary cube Items such as Sand,
Gravel, ores, Glass, Snow, Clay and storage blocks continue through the common
renderer instead of stopping at a missing inherited method.

These two method families cover the represented kinds. The separate
[inventory geometry](inventory-geometry.md) body supports all five types in
the original 3D Item predicate, including Cactus, Stairs, Fence and Piston
base textures. Other World rendering paths retain their own limits.

## Original methods and native ownership

The verified client archive has 73 classes in the complete Block hierarchy,
including intermediate classes. Only Leaves overrides `UU.b(I)I`. Every
other represented receiver inherits the literal white result, independent of
metadata. Leaves keeps its real client JH namespace, active initialization,
subtype precedence and palette behavior; the shared Registry does not create
a graphics namespace for it.

`UU.g()V` returns without reading or changing any bounds. Five descendants
override it: Pressure plate, Piston base, Button, Trapdoor and Cake. Their
native bodies perform the actual six ordered bounds stores with the original
binary32 operands. Inherited receivers preserve their current bounds, including
previously authored non-cube bounds. A closed kind-to-method table records
these families; a new or invalid kind does not receive a guessed default.

The [original packet](../../tests/reference/client/block_inventory) records
all 96 actual registered Block singletons, their runtime classes and declaring
method owners. Two fresh guarded original VMs agree on 13,194 rows. The native
comparison matches 13,098 behavior rows: render type, 23 metadata colors,
constructor and authored bounds, and six face textures for all 57 type0
receivers. Late texture-field changes exercise live fields rather than an
expected texture table. The other 96 rows record original declaration facts.
Complete class inventories, superclass chains and Code hashes bind the absence
of additional overrides. The retained 17 original Leaf rows also match.

Native admission controls separately exercise all 69 genuine RAW owners,
invalid kinds and the real BASE allocation-flow restriction. They do not
substitute native unsupported results for an original returned value.

## Actual Sand scene

`client.sand_harvest` loads an authored supported Sand block and an empty
Player Inventory, then mines through real SDL/source input. The naturally
published Item appears once in the same World and Chunk, returns and retires
its actual BB draw, and is picked up through ordinary W movement and pickup
delay. Three returned held frames render its six faces and 24 vertices.
E then opens the real Player Inventory and closes it through the source route.
The scene checks the mined statistic and all 64 destruction particles.

The initial qualified Windows scene passes 771 checks in 10.75 seconds.
Independent typed NBT contains the exact Sand Item and the picked-up Sand
Stack. Its stopped 13-file save tree, totaling 1,225,714 bytes, remains
byte-identical through checked native retirement; both windows are absent.
Actual dropped, held and Inventory screenshots are inspected separately.
Later runtime changes require their own fresh selected-input qualification.

The current-source Sand recheck passes 853 checks in 9.76 seconds, preserving
1,939 selected inputs, 413 assets and its stopped 13-file, 1,225,714-byte save
tree. A mixed-cohort attempt loses SDL focus during pickup and fails its focus
gate; checked cleanup still closes both windows and preserves the stopped save.
That failure remains recorded, and its cause is unproven. The isolated pass
does not establish unrestricted desktop stability.

The Inventory has 45 Slots and makes 45 returned BB GUI calls, including
NULL Stack calls. In this original archive even armor Slot PV and result
Slot YV inherit `GP.e()I == -1`; an empty armor Slot does not take a different
background-icon path. That call count is not the number of nonempty Stacks.
The scene checks the actual payload independently through Inventory/NBT state.

Enable `MC_ENABLE_CLIENT_SAND_HARVEST_TESTS=ON` with the existing Windows display
and imported-resource options, then run:

```sh
ctest --test-dir build-windows --output-on-failure --no-tests=error -R '^client.sand_harvest$'
```

The CPU comparison runs as `client.inventory_block`, and packet corruption
controls run as `tools.block_inventory_packet`. The SDK scene qualifies Sand's
declared mining, drawing, pickup and Inventory path. It does not establish
every recipe or acquisition route, original pixels/audio, Linux SDK play,
unrestricted sessions, general FPS or dedicated-server behavior.
