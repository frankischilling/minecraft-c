# Ordinary Torch removal during Player wake

Two fresh JDK17 interpreters call the unchanged original Player damage method
after the Player enters sleep through its actual method. The occupied Bed head
is at `(8,64,8)` and its foot at `(8,64,7)`. Torch50 at `(7,64,8)` has metadata
one, air at its west attachment `(6,64,8)`, and Stone at `(7,63,8)`.

General Torch survival succeeds through the floor. Its west attachment is
absent. Normal wake clears Bed occupancy, publishes one Torch Item while the
Torch still exists, removes the Torch, then completes wake. A declared Item
observer exception preserves the published Item and Torch and interrupts wake.
The Player already has standing dimensions, while its sleeping flag, timer,
World sleeping flag and position retain their interrupted values.

The finite observations retain Player fields, central Chunk plane hashes,
complete World/Player Random states, actual Item fields, ordered callbacks,
west/floor cells and full callback/exception stacks. The Item publication and
air notification both contain `vm.b` directly. The pinned body reaches its
fresh metadata read152, inherited drop155 and notifying air setter164. Support
query order follows that unchanged bytecode; this capture has no per-query
debugger trace.

Thirty-one original classes are pinned. `vm-pins.json` records the full Torch
class structure and every method's Code hash. Reached methods are neighbor
`b(Lfd;IIII)V` (169 bytes), survival `i(Lfd;III)Z` (43), can-place
`a(Lfd;III)Z` (78), and floor `h(Lfd;III)Z` (34). All game classes load from
the verified original archive. Only the authored observer/helper classes are
compiled. No game class is replaced, patched or recompiled.

The native controller selects 54 values for normal return and 46 for the
publication exception: Player state, committed cells, callback order,
publication state, World/Player seeds, selected Item fields and actual west/floor
cells. Host results are recorded separately in the test documentation. Wider
Item entropy, Gaussian caches, Chunk planes and Java stacks remain original
observations until separately compared. These two
prepared calls execute no ticks, GUI, native library, network or process launch.
They do not qualify natural sleep, whole-game behavior or sustained gameplay.

Run Java-free reader and corruption checks:

```text
python -B tests/reference/entity/torch_wake/packet.py
python -B -m unittest discover -s tests/reference/entity/torch_wake -p test_packet.py
python -B -O -m unittest discover -s tests/reference/entity/torch_wake -p test_packet.py
```

Repeat the original capture with the supplied archive, Launcher libraries and
JDK17. The output directory must be new and inside the worktree's `.local`.

```powershell
python -B tests/reference/entity/torch_wake/capture_reference.py `
  --client-jar PATH_TO_SUPPLIED_CLIENT.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --java-bin "$env:JAVA_HOME/bin" `
  --output .local/torch-wake-original
```

Raw streams, full commands and bounded receipts, original/tool/library hashes,
compiled helpers and isolated work directories remain private. Public
provenance binds executed sources and records both VM stream extents/hashes.
Command hashes bind the private command records without publishing their
paths. The reader rejects duplicate JSON keys, nonfinite JSON numbers, unknown
fields, incorrectly typed values and incomplete process receipts.

`read_reference(directory=HERE)` validates finite/provenance files and returns
the finite dictionary. `check()` performs the same validation and prints its
original-only scope. `P` exposes the shared exact-type comparison helpers.
