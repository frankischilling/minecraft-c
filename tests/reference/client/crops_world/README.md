# Original Crops World rendering observations

This packet observes the unchanged client `cv.j(Luu;III)Z` wrapper and
`cv.b(Luu;IDDD)V` primitive. The original JAR, dependencies and JDK are local
reference inputs. Public files contain authored controllers, declared operands,
primitive observations and hashes; no original classes, assets or Code bodies.

The 44 profiles cover every World metadata nibble, negative and extreme direct
primitive metadata, live texture-index wrapping, override order, brightness and
emission, three bounds sets, retained CV scratch, translated vertices, four
large-Y float-rounding cases, section and loaded projections, and three null
failure prefixes. A primitive with a null CV cache also verifies that the
primitive does not read that unused field.

Each successful call emits eight quads: 32 source vertices expand to 48 packed
triangle vertices and 384 words. One genuine constructor-backed NW batch spans
the entire matrix. Every call checks its own prior inactive words and buffer
identities, positions, limits and capacities. The helper retains the same real
World, four Chunks, EW, registered Crops and NW owners.

Two fresh ordinary VMs and a third read-only JDI VM must produce identical
stdout. JDI reads original method entries/returns, singleton and emission-array
accesses, arguments, failure locations and NW state. It writes no target state
and invokes no target methods. Frozen helpers and products are checked before
packet replay; live Code fingerprints must equal the archive's method pins.

`packet.py` derives packed active words from the observed original vertex/UV
arguments and NW state. The native renderer is not an expected-value source.
`projected_cases(1)` supplies four section cases at `(8,64,8)`, with NW
translation `(-8,-64,-8)` and a saved-skylight 15 cube. Placement 2 supplies the
same stages 0/7/8/15 at `(3,65,3)`, translation `(0,-64,0)`, and block-light 15.
Each case has `id`, `metadata`, `pass`, `xyz`, `vertices` and `words`, matching
the neighboring World-rendering packets.

Recapture with the pinned original client, libraries and JDK 17.0.17:

```text
python tests/reference/client/crops_world/capture_reference.py --java JAVA17 --javac JAVAC17 --output .local/fresh-crops-original
```

Use a fresh output directory. The controller preserves frozen authored sources,
raw streams, worlds, process receipts and partial failures. An initial attempt
expired its inherited observer watchdog; that partial capture remains private.
The observer watchdog is now 300 seconds, with a 330-second outer bound.

The CPU-only native replay uses genuine C owners and compares all input and
observation rows exactly. Separate controls enter actual CV dispatch and reject
unsupported floating-point modes. These finite comparisons do not establish
original pixels, whole-frame rendering, farming simulation or performance.
