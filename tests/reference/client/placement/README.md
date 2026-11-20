# Client Dirt placement identities

`source-pins.json` contains authored identity metadata for the original Beta 1.7.3
client archive. It includes complete declared-method inventories for 124 relevant
classes, superclass links, class and method Code hashes, exact declaring sets for
the five Block placement methods, Item/Stack/controller use methods, and resolved
dispatch for every one of the 69 C Block kinds. It contains no original Code
bytes, game classes, assets, or disassembly.

Each Block kind is mapped independently by its C registration `NEW` PC and the
original `uu.<clinit>` allocation at that same PC. Inherited methods are resolved
through the complete original hierarchy. For example, DetectorRail inherits the
Rail placement predicate, RedstoneTorch inherits Torch placement methods, and
the Flower family inherits `wb.a(World,III)`. Distinct subclasses are not assigned
the base `uu` result merely because they lack their own declaration.

Run the static verifier against the user's original archive:

```text
python tests/reference/client/placement/verify.py --jar b1.7.3.jar
```

The verifier reads ZIP/classfile bytes with Python. It checks the exact archive,
all relevant class and method identities, absence/presence of declarations,
registry allocation bindings and inherited dispatch. It never loads or executes
an original game class and performs no Java or native client launch.
Missing archive input returns `UNAVAILABLE` with exit77; changed identities or
invalid inputs return `FAIL` with exit1. The five local verifier controls include
the unmodified archive and rejected method, declaration, inheritance and
same-length registry-allocation mutations.

The independent SDK fixture is
`tests/client/gameplay/test_placement.c`, driven by
`tests/tools/client/test_placement_loop.py`. It continues the complete mining,
dropped-Item persistence, pickup and held-Dirt route. It then uses ordinary
SDK movement and right-button input, checks actual source action observations,
returns to empty Inventory/HUD/hand state, and performs a third ordinary
save/title action and fourth World selection. The Python driver independently
decodes Dirt at the placed coordinate, the empty Player Inventory, and all saved
incidental Items against owned semantic snapshots from the selected live
pre-leave frame.

The rendering checks observe returned terrain, completed compile calls and a
clean target DK. They do not attribute an individual compile call to the target
DK. Screenshots are retained for visual inspection. Static identities alone do
not establish placement or rendering parity; the executable and its actual SDK
qualification remain separate checks.
