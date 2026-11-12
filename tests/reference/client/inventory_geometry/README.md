# Specialized Block inventory geometry

The unchanged client executes Cactus (81), Wood Stairs (53), Cobblestone Stairs
(67) and Fence (85) through `cv.a(Block,int,float)` on a real hidden 32x32 LWJGL
Pbuffer. Sixteen returned calls emit 216 NW batches: 864 source vertices, 1296
packed vertices and 10,368 observed backing words. A final Wood Stairs call fails with
its actual nullable source Block. This packet qualifies these finite direct
calls, including their source order and partial effects.

Original KQ, KV, IK, JI, SJ and CV constructors provide the owners. The actual
terrain atlas lookup/upload runs. The Font-used NW singleton retains its prior
backing, including inactive words. No World/cache preparation, RAW/Unsafe
allocation, game subclass, replacement game body, entropy rebinding, forced NW
flags, buffer clearing or capacity replacement is used. Authored bounds,
textures, Stair source references and CV fields are explicit inputs in
`profiles.json`; their cumulative effects persist across profiles.

The profiles cover metadata and brightness operands, both CV.b states,
non-cube Cactus bounds, late textures and Stair source changes, nonzero NW
offsets, six UV rotation fields, flip, texture override and AO colors. Stairs
retains its second part bounds. Fence performs all four part setters and the
actual final unit-bound reset. Cactus performs each additive inset and undo in
source order. The final NULL Stair source fails at `ss.a(I)I:5`, reached from
`cv.a(Block,int,float):836`, after its first bounds setter, GL translation,
NW begin and normal. Its open NW, first-part bounds and translated modelview
remain visible; the failed source call is neither repaired nor retried.

Ordinary and read-only JDI processes agree exactly on initial state, every
declared input, returned source state, final NW words and whole-call GL
matrix/color observations. JDI observes actual source entries, every face,
NW begin/add/draw, bounds setters, Stair side getter, GL translate arguments
and the exception. Twelve live game method Code hashes match the supplied
archive. The debugger neither writes target state nor invokes target methods.
The target denies network attempts and admits only pinned LWJGL/JDK libraries.

`finite.json` retains every full 48-word draw buffer. Active geometry comparison
covers 7596 words. A separate audit checks all 2772 inactive words against that
VM's own prior backing. Original quad conversion does not write the inserted
vertices' normal words; inactive color/reserved words also remain observable.
Native comparison must preserve its own prior backing and compare active words
explicitly. It must not substitute original stale words into native output.

`compile-finite.json` and `compile-observations.json` retain a separate genuine
GL_COMPILE cohort, with another ordinary/JDI pair and independent prior-backing
audit. All 216 complete CPU buffers, draw-entry source states, bounds and PCs
agree exactly with immediate mode. All 17 input and returned CPU/source receipts
also agree. GL poststate is compared separately because compilation defers
color/matrix commands. No list is replayed. The passive native geometry
comparer accepts only submitted bytes, vertices, mode and array mask; it does
not invent native per-draw NW state, bounds or source PCs.

Matrices and current color are actual whole-call GL queries. Per-draw source
translations are observed calls, not device matrix queries at each draw.
No per-draw immediate GL state, raster pixels, complete HUD/held/dropped flow,
audio, arbitrary stale backing, extended play or performance claim follows.
Short NW backing is excluded because this preparation does not replace the
actual singleton backing. Native gameplay and rendering remain separate checks.

The pinned profile uses Windows x64, JDK 17.0.17.10, LWJGL 2.9.0 and the recorded
NVIDIA context. The Java capture, driver and source-audit files were pinned
before both qualified cohorts. `test_modes.py` is a separate post-capture audit
of the two retained cohorts. The first compile-only helper failure remains
recorded in provenance; it did not execute the original game.

Run Java-free controls with:

```text
python -B tests/reference/client/inventory_geometry/test_packet.py
python -B tests/reference/client/inventory_geometry/test_modes.py
```

On the recorded Windows host, the 16 packet controls pass in 3.732 seconds and
the three supplemental controls pass in 0.083 seconds. Corruption checks cover
all 216 batches, every packed vertex position, XYZ/UV/color/normal word roles
and the distinct source-order, state, ownership, schema and failure checks.
These are test elapsed times, not game performance measurements.

To reproduce the original observation, prepare a fresh directory, then observe
it. Serialize these hidden GL checks with other graphics tests:

```text
python -B tests/reference/client/inventory_geometry/capture_reference.py --output .local/inventory-original
python -B tests/reference/client/inventory_geometry/capture_reference.py --output .local/inventory-original --observe
```

Use a second fresh output with `--mode compile` for the supplemental cohort.
