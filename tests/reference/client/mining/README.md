# Static mining source identities

`source-pins.json` records static identities from the locally supplied original
client archive. It retains the initial 17 classes and 48 selected method pins,
then adds complete declared-method inventories for client FK (Glass) and VG
(Log): 19 classes and 58 methods in total. Each method includes its Code length
and hash, complete Code-attribute hash, access flags, and maximum stack/local
counts. No original class bytes or decompiled bodies are stored here.

FK's complete three-method inventory has no declared
`a(World,Player,int,int,int,int)` harvest override. This absence is checked
against the whole declared inventory, rather than inferred from a selected
method search. VG's seven-method inventory includes its harvest override.
These are static class facts; they do not establish executed harvest behavior.
The 46 references to existing method pins preserve their original identities
and use repository-relative paths with forward slashes.

Run the verifier with an explicit local archive path:

```sh
python -B tests/reference/client/mining/verify.py --client /path/to/b1.7.3.jar
```

The verifier uses the existing rich classfile parser, reads bounded ZIP entries,
and prints JSON. `PASS` means all selected static identities match, including
both complete added inventories and the Glass absence check. Missing archive
input returns `UNAVAILABLE` with exit 77; invalid pins, parsing or changed inputs
return `FAIL` with exit 1. Before/after fingerprints cover the verifier, pins,
parser, referenced metadata and original archive. The verifier never starts a
JVM or writes original classes.

| Original source entries | Current implementation |
| --- | --- |
| Minecraft `a(I)`, `a(IZ)`, `k`; OS/OB controller entries | [Input callers](../../../../src/client/runtime/gameplay_input.c), [mining controller](../../../../src/client/controller/mining.c) |
| GS/DC Player and IX Inventory mining/swing leaves | [Player mining](../../../../src/entity/player/mining.c), [Inventory mining](../../../../src/inventory/mining.c), [swing](../../../../src/entity/action/input.c) |
| UU Block strength/hooks/harvest; LN Material; CT StepSound | [Player strength/harvest](../../../../src/entity/player/mining.c), [controller harvest](../../../../src/client/controller/mining.c), [StepSound](../../../../src/block/registry/step_sound.c) |
| FD auxiliary/extinguish and notifying removal; N effects | [World auxiliary](../../../../src/world/runtime/auxiliary.c), [WorldAccess](../../../../src/world/runtime/world_access.c), [N event body](../../../../src/client/world/render_global_auxiliary.inc) |
| DN hit/destruction; QM/XW particle bodies | [DN block effects](../../../../src/client/particles/block_effects.inc), [digging particles](../../../../src/entity/particles/digging.c) |
| BB dropped Item and CV inventory Block | [Item drawing](../../../../src/client/render/item_draw.c), [inventory Block geometry](../../../../src/client/render/blocks/inventory.c) |
| OT Dirt, FK Glass and VG Log | [Block constructors](../../../../src/block/registry/constructors.c), [harvest dispatch](../../../../src/client/controller/mining.c) |

The [mining contract](../../../../docs/client/mining.md) records authored native
controls and SDK integration separately. Static matching does not prove original
runtime calls, pixels/audio, successful input, pickup, saving or playability.
