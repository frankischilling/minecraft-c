# Original held Item mining observations

The packet records 19,594 registered Item/Block pairs, including a NULL Block
for every Item. Each row contains six results: actual Item, ItemStack and
Inventory speed and harvest calls. All 202 Items, all 38 runtime Item Classes
and all 96 registered Blocks are included. Three fresh original processes
produce identical selected results and declaration owners.

`MiningObservation.java` invokes unchanged supplied-client methods. It constructs
real ItemStacks with count -3 and damage -7, and a real Inventory with a NULL
Player. It never constructs a World or invokes graphics. `classes.tsv` records
the real reflected declaring classes. `source-pins.json` records full method
declaration inventories and Code hashes for 45 original classes; it includes
the inherited DQ/CP classes and reached IX/IZ/BU/UU/LN methods.

The reference JAR SHA-256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The observations use Temurin 17.0.17+10 with `-Xint -Xverify:all`, a 256 MB heap
and headless AWT. Supplied archives, dependencies, JDK inputs and helpers are
hashed before and after each qualified capture. Generated classes and raw
outputs remain private. No original class is rewritten or replaced.

```powershell
python -B tests/reference/item/mining/capture_reference.py --jar b1.7.3.jar --libraries "$env:APPDATA/.minecraft/libraries" --output .local/mining-reference
python -B tests/reference/item/mining/test_packet.py
ctest --test-dir build-windows -R '^(item\.mining|client\.mining_controller)$' --output-on-failure --no-tests=error
```

The public native test compares all six results per row and checks unchanged
Stack fields. Separate authored controls distinguish singleton identity from
Block IDs, preserve nullable effective-array elements and RAW Item phases,
exercise the ToolMaterial null branch, and check Inventory main/armor routing,
Material short circuits and exact index-failure frames. The controller test
also starts mining real Stone in a real Chunk while holding a Wooden Pickaxe.

The separate destruction corpora contain 1,947 direct Item rows, 384 ItemStack
wrapper rows and six actual singleplayer-controller Stone-removal rows.
`MiningDestroyObservation.java` uses real App, Stats, World, Chunk, Player and
Stack constructors. Every source invocation runs unchanged original methods;
the helper writes only declared preparation inputs outside that invocation.
Two fresh VMs agree on every selected row. Network/listen/native-library
requests are denied, with zero attempts. Timer workers are joined at cleanup.

`destroy-source-pins.json` fingerprints 50 original classes, adding the actual
controller/Player/statistic bodies to the Item inventories. The two destruction
files have independent SHA-256 bindings and strict typed schemas in `packet.py`.
`item.combat` compares all Item/Stack rows and tests failure prefixes through
the shared durability kernel. `client.mining_tools` compares six fresh native
processes with actual controller removal, exact current-Stack/count/damage,
used/broken/mined statistics and published Cobblestone. Profiles cover ordinary
wear, last-Item break, a remaining Item after break, zero-count clearing,
negative-count retention and a broken Sword that cannot harvest Stone.

```powershell
python -B tests/reference/item/mining/capture_reference.py --destroy --jar b1.7.3.jar --libraries "$env:APPDATA/.minecraft/libraries" --output .local/mining-destruction-reference
ctest --test-dir build-windows -R '^(item\.(mining|combat)|client\.mining_tools|tools\.item_mining_packet)$' --output-on-failure --no-tests=error
```

These finite CPU observations do not establish cold class-initializer failures,
general malformed native storage, every concrete Block harvest override,
original World pixels, audio, extended play, dedicated-server behavior or FPS
equality. The controller profiles compare selected in-memory results; they
perform no independent saved-world disk decoding.
