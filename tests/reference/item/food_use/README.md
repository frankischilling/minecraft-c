# Food in-air use observations

This packet records unchanged Beta 1.7.3 client calls from archive SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Four ordinary fresh VMs execute 28 distinct calls: 14 actual ItemStack wrappers
and 14 separately prepared singleplayer controller wrappers. Each group repeats
in two VMs with identical complete stdout. There are no game-class replacements,
debugger writes, network attempts, native-library loads or subprocess attempts.

The helpers construct real App, saved World, Player, Stack and statistics owners.
Each call deliberately passes a NULL World argument. The real World's remote
field alternates independently. Health, healing period and hurt resistance are
declared inputs assigned before each call. Used-statistic deltas are read from
the real writer and registered entries; all observed deltas are zero.

`profiles.tsv` contains seven signed inputs: Item ID, count, health, period,
resistance, NULL-Player flag and remote flag. Every input Stack has damage
`INT_MIN` and animation 9. The controller profiles select hotbar slot 2 and place
that same Stack in the slot before entry. The last three profiles pass a NULL
Player. `declared-inputs.json` repeats these operands explicitly.

## Original method order

`yw.a(iz,fd,gs)iz` reads count at 2, subtracts one at 6 and stores at 7. It reads
the receiver's healing amount at 12 and invokes `gs.c(int)` at 15, then returns
the same Stack at 19. Neither `gs` nor `dc` overrides that inherited method.
`qk` inherits the same Item-use body.

`ls.c(int)` reads health at 1 and returns at 7 when it is nonpositive. Otherwise
it Java-wrap adds the amount and stores health at 15, clamps values above 20 at
30, then stores hurt resistance as signed `E / 2` at 40. Overflow to a negative
health value survives the upper clamp. Consuming food at full health still
changes count and updates resistance; consuming while dead still changes count.

Soup's `bz.a(iz,fd,gs)iz` invokes Food at 4 and discards its returned reference at
7. It allocates a fresh Stack at 8 before reading the actual `gm.C` field at 12,
then calls `iz(gm)` at 15 and returns the new Stack at 18. The constructor reads
the supplied Item's `bf` at 2 before forwarding ID, count 1 and damage 0 to the
numeric constructor at 7. The new Stack has animation 0 and no saved provenance.

`iz.a(fd,gs)iz` resolves the current Item at 1, invokes its actual virtual body at
7 and returns at 10. It has no used-statistic operation. `ob.a(gs,fd,iz)Z` saves
the original count at 1, invokes the Stack wrapper at 9 and compares result
identity at 17. The same-result count check occurs at 27. A changed result or
count stores the returned reference into the actual selected slot at 51, then
checks the returned count at 54; exactly zero clears the selected slot at 75.
Negative counts remain selected. Soup replaces the slot even when the original
soup Stack had count 3 and now has count 2. NULL Player fails in Food after count
store 7, before healing, soup allocation or controller slot replacement.

These bytecode anchors come from `javap -c -p -s` on the verified archive.
`source-pins.json` records whole-class and method Code hashes for the 12 relevant
classes. It contains identities and sizes only.

## Files and byte preservation

`wrapper.stdout` and `controller.stdout` retain exact captured ordinary stdout
bytes. Packet-local attributes preserve those bytes through checkout. Their
provenance files are declared portable projections of private capture receipts:
artifact identities replace absolute input paths, while ordinary output hashes
and unchanged input hashes remain exact. Each projection retains its private
raw receipt hash. The original private receipts remain separate artifacts.
The selected `wrapper.tsv` and `controller.tsv` are explicitly canonical LF
projections: select lines beginning with the corresponding event prefix, split
line endings, join with LF and retain one final LF. Their published hashes are
separate from the private capture's CRLF selection-artifact hashes.

Row columns are prefix, index, input Item ID, old Stack count, Player health,
Player resistance, result ID/count/damage/animation, same-result identity,
exception, used-statistic delta and remote flag. Controller rows insert the
Boolean return or `UNRETURNED` immediately before exception. For controller
rows, result fields describe the actual selected slot after the call. NULL
result fields use -1. Identity and exception distinguish that sentinel from a
real negative source field.

The first authored helper compilation failed because an untyped NULL argument
matched two original overloads. `failed-compile.stderr` is a declared portable
projection of that diagnostic: its private helper path becomes
`<helper>/FoodObservation.java`, and line endings use LF. The manifest retains
the original private raw diagnostic hash; the untouched raw log stays private.
An explicit `(fd)null` selected the intended wrapper. No runtime observation
from that failed compilation contributes to this packet.

Repeat the capture with a local original archive, pinned legacy library JARs
and JDK 17:

```powershell
python -B tests/reference/item/food_use/capture_reference.py --output .local/food-use-repeat
```

`item.food_reference` runs a separate native observer on real App, statistics
writer, Registry, World, Player, controller, Inventory and Stack owners. It
compares all 28 rows exactly, including signed fields, result identity, selected
slot, return value, exception and actual statistic delta. Its checker includes
negative controls for changed fields and reordered profiles. The native receipt
records process completion and before/after hashes of executable, packet,
profiles, resources and source inputs.

This evidence does not cover direct Item-only calls, debugger-observed runtime
ordering, loaded graphical input, saved-inventory persistence, audio playback,
multiplayer or unrestricted food gameplay. Authored native RAW/NULL-bowl and
retention-refusal controls have separate scopes from these original profiles.
