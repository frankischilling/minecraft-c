# Ladder World geometry

The packet records 35 unchanged-original `cv.h(Luu;III)Z` calls on genuine
saved-World, loaded-Chunk, EW, CV and singleton NW owners. The client archive
SHA256 is `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Two fresh ordinary JDK17.0.17 VMs and a third fresh read-only JDI VM produce
byte-identical complete stdout. The trace records 1,133 events, 533 method
entries, 160 actual EW queries, 68 static reads and three reached NPEs.
Twelve live method Code fingerprints match the verified archive.

The declared profiles cover all sixteen metadata nibbles, actual dark/sky/block
light, emission-array elements, texture overrides, Java-wrapped mutable bm,
partial/outside bounds, retained AO/rotation/flip/renderAll fields, three
placements and null Block/cache prefixes. `declared-inputs.json` defines the
thirteen integer inputs. Each original VM retains the real 2,097,152-word
constructor backing, one actual begin, per-profile translation setters and all
NIO buffer identities/cursors. Its own-prior audit checks 73,399,360 non-emitted
words. No raw owner allocation, query adapter, source replacement, repaired
batch, drawing, GL context or native-library loading occurs.

The source captures NW at PC0, calls the Block side0 texture getter at7 even
when an override is available, calls inherited Block brightness at34, sets gray
NW color at47 and reads metadata at117. Metadata2/3/4/5 emits one quad each;
other nibble values emit nothing and still return true at684. Twenty profiles
emit eighty source vertices, expanded to 120 packed triangle vertices and
960 words. The three NPE profiles retain the exact reached NW prefix: null
Block fails at CV.h7, while null cache reaches UU.d13 after the texture getter
and emission-array read. The both-null profile fails at CV.h7.

`selected.txt` retains registered inputs, complete primitive CV/NW fields,
bounds, raw words and own-prior backing receipts. `events.jsonl` records actual
arguments, returns, nested source callers, static reads and exception frames.
The semantic validator compares every emitted active word to the original
packed-vertex arguments, UV state, translation and color. The observer never
invokes a target method or changes target state.

The `section-2` through `section-5` profiles use (8,64,8) and NW translation
(-8,-64,-8). The `loaded-2` through `loaded-5` profiles use (3,65,3) and
translation (0,-64,0). `packet.projected_cases(1)` and `(2)` validate the complete
packet before returning those original geometry rows for native integration.

```sh
python -B tests/reference/client/ladder_world/capture_reference.py --jar b1.7.3.jar --libraries LIBRARY_ROOT --output NEW_PRIVATE_DIRECTORY
python -B tests/reference/client/ladder_world/run_native.py --executable BUILD/mc_test_ladder_world --work-dir NEW_NATIVE_DIRECTORY
```

The capture refuses an existing output directory and retains commands, Worlds,
streams and failures. It freezes its producers, inputs, original archive,
libraries, JDK and helper bytes before the first game VM, then verifies them
again afterward. Committed products contain observations and fingerprints,
without original classes, assets or JARs. Native outer-row comparisons remain
separate from a native per-query trace, enclosing World frames, original pixels,
extended play and FPS.
