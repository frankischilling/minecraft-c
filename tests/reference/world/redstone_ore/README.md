# Redstone Ore source bodies

This packet covers 67 fixed inputs to unchanged Beta 1.7.3 client Redstone Ore
methods. It observes direct left click, walking and right click, display
particles, active and inactive ticks, scheduled `World.l` dispatch, one genuine
Player's random work, initial mining through `OS`, and right use through `OB`.
The right use case reaches a real Cobblestone Item after Ore returns false.
Natural encounters, graphics, frame rate and whole-game parity remain separate.

The original App is genuinely constructed as `kq(null,null,null,854,480,false,null)`
under the existing no-native-link guard. It receives the prepared Session, World
and local Player fields used by the controllers. Native caller pairing prepares
those method inputs explicitly; it does not qualify an App constructor, GUI or
audio initialization.

The authored Java adapter reuses the sealed [Fire setup](../fire/README.md),
constructing real Worlds, Biomes, Chunks, Players and controllers. It writes only
declared preparation fields and installs a genuine World observer. It does not
replace original classes, getters or methods. Direct wrapper Actor operands are
null because the original methods never dereference them. Explicit controls
prepare null World arguments, captured Random, provider, observer list and the
two Ore static fields. Corrupted fields are restored after the final observation.
The null observer-list controls distinguish an inactive tick that never reads
the list, an active tick stopped at the height range notification, and an active
tick stopped at the cell notification with a prepared height of 65.

Each row records complete hashes of all five planes in every prepared Chunk,
actual ordered callbacks and raw floating point parameters, World and Math Random
state, light requests, scheduled class sequence and entries, Player state,
controller fields, stack count and the selected Item use statistic. Static class
counters and the statistic retain their actual sequence across rows. Fresh native
processes receive those before values as declared preparation; outputs are not
renumbered or reset.

The independent Python model derives particles from the JDK LCG and source float
operations. It retains the source comparison of particle Y with zero, three
draws before each opacity query, the receiver ID checks, and the first genuine
failure with its partial changes. Shared World mutation, height and light rules
come from the authored Fire model, never from native answers.

`capture_reference.py` requires the local unchanged client JAR, pinned libraries
and JDK 17. It retains two fresh VM runs privately and requires byte-identical
complete stdout and stderr, unchanged inputs and helper products, and closed
class and Code pins before publishing original-only qualification.

The additional class pins cover `bs`, `os`, `ob`, the actual generic BlockItem
`ck`, and its step-sound owner `ct`. Original classes and archive bytes remain
private. The separate native reader tests the retained caller diagnostics and
cleanup observations; it does not invent a native Java exception stack.

Run the Java-free reader and corruption controls with:

```text
python -B tests/reference/world/redstone_ore/packet.py
python -B tests/reference/world/redstone_ore/test_packet.py
python -B -O tests/reference/world/redstone_ore/test_packet.py
```

Native pairing is provided separately by [run_native.py](run_native.py).
