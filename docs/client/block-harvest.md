# Leaves, Snow-layer and Ice harvesting

The single-player controller executes the three concrete harvest overrides
after its notifying Block removal and held-Item destruction. A tool may already
have broken and cleared the selected Inventory cell when harvesting begins.
Earlier effects remain if a later child fails.

The authority is the supplied client archive, SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The [original packet](../../tests/reference/block/harvest/README.md) records
unchanged methods in actual World, Chunk, Player, Stack and listener graphs.

| Entry | Behavior |
| --- | --- |
| Leaves `bk.a(Lfd;Lgs;IIII)V` | On the local path, read the current hand separately for the NULL check and Stack ID. Compare that numeric ID with the current Shears static. The special branch records the nullable mined statistic, constructs one Leaf Stack with metadata `& 3`, then calls the existing protected Stack spawn. Otherwise run base harvesting and its sapling drop path. |
| Snow layer `jr.a(Lfd;Lgs;IIII)V` | Read the current Snowball Item first. Consume three World Random floats, allocate the Entity before the Stack, construct the actual ItemEntity and set pickup delay 10. Publish it, perform the notifying air write, then record the mined statistic. This method has no remote-World guard. |
| Ice `nk.a(Lfd;Lgs;IIII)V` | Run base statistics/drop first. Query the below Material at wrapping `y - 1`; test its solid predicate, then its liquid predicate only if needed. On support, read the current flowing-Water static and perform the notifying write. The result is Water ID 8; random Ice melting uses the distinct still-Water path. |

Shears that break from count 1 leave an empty current hand before the Leaf
override runs. That takes the ordinary base/sapling route. A breaking Stack
whose count falls from 2 to 1 remains selected with reset damage, so it can
still take the special Leaf drop route. The harvest body does not borrow the
controller's earlier captured tool as its fresh hand.

Ordinary false Entity insertion and Block setter results are discarded at the
source sites. Native failures retain the reached child and earlier effects.
The Snow path shares lower Entity/Stack ownership services with other drops,
but preserves its direct allocation order and unconditional remote behavior.
The Leaf path reuses the existing protected spawn body, including that body's
own remote guard.

## CPU qualification

Two guarded original processes agree on 92 prepared calls: 63 complete
single-player controller removals and 29 direct concrete harvest calls. The
packet includes 51 Item records and 135 listener events, with exact position,
velocity, yaw, age, pickup delay, World/Math Random state and World/Chunk aliases.
Network, listening and native loading are denied; separate controls verify
those denials. No game source, assets or class files are distributed.

The Windows MSVC Release controller fixture matches all 208 selected rows from
the 63 controller scenarios. Four additional native controls check later Ice
Material/static failures and Snow publication/statistic failures. All 67 fresh
native processes complete 6,158 checks in the 1.39-second CTest run and retire
their actual owners. The 29 direct original calls are reference observations;
they are not additional native direct-entry coverage.

```sh
ctest --test-dir build --output-on-failure --no-tests=error \
  -R '^(client.block_harvest|tools.block_harvest_packet)$'
```

## Particle and dropped-Item continuation

The real SDK scene exposed two further required descendants. QM first refused
the Leaf World-color method before removal; after that method was connected,
the newly published Leaf Item reached an unsupported nonopaque scale branch.
Both failures remain recorded with their successful checked cleanup.

QM and the World renderer now share the actual BK World-color body. Metadata
is read first; bit 1 takes pine precedence over bit 2/birch. The other branch
samples climate and reloads the manager from the captured World for each channel.
The actual JH namespace performs class active use and palette reads. A real
synchronous Chunk-load callback changes the particle's World field, proving
that the invocation continues with its earlier captured World operand.

DN reuses one complete color diagnostic child across its 64 particles. Its
private metadata getter preserves full failure receipts while leaving success
diagnostics untouched; the public detailed getter keeps its success reset.
World queries, the existing 25-row color replay with new QM controls, and the
real DN Leaf hit/destruction/failure controls pass their focused Windows cases.
TallGrass retains its separate reached boundary.

The dropped-Item renderer executes BB's normal-render predicate before its
remaining scale prefix. Leaves and Glass inherit a true result, so they retain
scale .25 regardless of transparency and skip the later Stack/static reads.
On a false result, the numeric current Slab singleton match and render type 16
retain .25; the remaining branch uses .5. Source reads and reached NULL failures
remain ordered. Leaf inventory drawing calls the real BK metadata
color body and JH active use; the inherited shape method is a literal no-op.
The [inventory packet](../../tests/reference/client/leaf_inventory/README.md)
has two matching original captures and 17 exact color/shape rows. The native
inventory test passes 2,863 checks, including signed metadata, cold initialization,
NULL palettes, failed class initialization and floating-point prefixes.

## Windows SDK qualification

`client.leaf_harvest` passes in 7.64 seconds with 467 native checks. The authored
scene selects surviving Shears and a Leaf of subtype 2. Actual SDL/source input
removes the target, publishes all 64 destruction particles, wears the Shears by
one without used/broken statistics, and publishes one Leaf Item with metadata 2
in both World and Chunk. The same Item reaches a returned and retired BB draw.
Independent typed NBT checks the Item and the surviving selected Shears.

All 1,928 selected inputs and 413 resources remain unchanged. The stopped save
tree retains 12 files and 1,225,321 bytes through checked native closing, and
both native windows are absent afterwards. Native before/after screenshots
are inspected separately. An earlier native success with a mistaken validator
expectation remains recorded: Entity-body NBT omits the String ID supplied by
the separate Chunk-save wrapper. The corrected validator requires its absence.

Enable `MC_ENABLE_CLIENT_LEAF_HARVEST_TESTS=ON` with the existing Windows display
and imported-resource options, then run:

```sh
ctest --test-dir build-windows --output-on-failure --no-tests=error \
  -R '^client.leaf_harvest$'
```

This qualifies one surviving-Shears/subtype-2 Windows scene. Save/reload of that
scene, extended play, original pixels/audio, Linux SDK gameplay, other Block
hooks, general FPS and dedicated-server behavior remain separate scopes.
