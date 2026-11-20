# Native World reload validation

This packet records authored native UI scenarios. It contains no original
session capture and establishes no original whole-frame or pixel equality.
The [ownership contract](../../../../docs/client/block-selection.md) describes
the runtime path and remaining work.

## Reproduce

Build the ordinary client test target with locally imported resources and a
working native desktop. Run the foreground scenario without another application
test competing for focus:

```powershell
python -B tests/tools/client/test_world_selection_loop.py --executable build-windows/mc_test_world_selection_loop.exe --assets .local/imported-icons --artifacts .local/world-reload-check --mode reload-cycle --timeout 600
```

The resource path must identify the user's own imported pack with its SDK icons.
The fixture creates disposable authored save files in a temporary game directory.
It declares Tiny render distance and the existing openInventory statistic before
bootstrap. Runtime World, Player, Timer and Random fields are not repaired.

The actual menu/input loop selects ten Worlds, saves each through Pause ID1,
then selects a conversion-required eleventh save. Every successful graph remains
live. The failed successor acquires no World storage. Independent Python NBT and
region decoding checks each real before/entry/after save snapshot, reopened
Inventory/Dimension/Score, and byte continuity through failed conversion and
native cleanup. Native controls also verify reciprocal Load order, distinct
owners, passive external-hold refusal and retry, then checked retirement.

[native-validation.json](native-validation.json) preserves each run's exact
scope and evidence-binding limits. A retained passing run without execution-time
source/executable hashes cannot qualify the current executable. A later pass
does not erase the recorded initial native-focus failure. Old World reclamation,
failed successors after storage acquisition, unrestricted focus/reselection,
default-Far performance and extended play retain separate requirements.
