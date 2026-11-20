# Flint and Steel source bodies

This packet declares 104 calls through unchanged Beta 1.7.3 client Flint and
Steel. It covers the direct `YE` body, the real `IZ` wrapper, and actual `OB`
continuation after false Block activation. Fire addition and Portal creation
execute their real children. Natural encounters, rendering, frame rate and
whole-game parity remain separate.

The authored adapter reuses the sealed [Fire World setup](../../world/fire/README.md).
It constructs the original App as `kq(null,null,null,854,480,false,null)`, a real
saved World, four Chunks, a local Player and statistics writer. It prepares only
declared fields and registers a real World observer. It replaces no original
classes, methods or getters. Native App references are explicit method inputs;
their comparison does not qualify App construction, GUI or audio initialization.

The fixed operands cover all six faces, unchanged invalid face values, wrapped
Java integer coordinates, occupied cells, unsupported Fire, remote World input,
false setters and both Portal orientations. Null controls distinguish a reached
dereference from an unused World, Player, Stack, provider, Random, Fire singleton,
Item table or statistic array. Source X upper bounds exclude 32000000 while Z
includes it. An in-domain cell outside the prepared cache is recorded as unknown;
it is never reported as invented air or acquired by observation.

Sound callbacks can change actual source fields or call the real World setter.
They test live Fire loading after sound, discarded setter results, late Stack ID
and statistic reads, and use of the captured air result when a callback places
Stone. Nested callbacks retain their actual order and stacks. Callback exceptions
preserve committed cell, metadata, height, light, queue, durability and statistic
prefixes.

Each row records all five planes of every loaded Chunk, complete scheduled and
light requests, World/Math/Item Random state, the original prepared Item Random
owner, Stack fields, selected Player fields and complete sorted total/unsent
statistic maps with their dirty flag. The saved Random owner remains observed
when the live Item Random is rebound. Class counters and statistic maps keep
their natural sequence across profiles; native processes receive exact original
before values as prepared inputs. No output counters, entropy or effects are
normalized.

The independent Python model derives YE's integer-to-double sound centers,
float pitch, source order, durability overflow and strict break threshold from
unchanged bytecode. Shared authored World mutation and Portal rules come from
the Fire model, never from native answers. The capture driver requires two fresh
sequential JDK 17 VMs with identical complete stdout/stderr, unchanged inputs and
helper products, and closed original class/Code bindings.

```text
python -B tests/reference/item/flint_and_steel/packet.py
python -B tests/reference/item/flint_and_steel/test_packet.py
python -B -O tests/reference/item/flint_and_steel/test_packet.py
```

Native pairing is separate in [run_native.py](run_native.py).
