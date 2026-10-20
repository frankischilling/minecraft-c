# Original World query mapping

The unchanged reference archives have SHA-256 client
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`
and server `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.
Complete paired bytecode supplies these methods and operations. Class and
descriptor identify the evidence; bytecode offsets locate the relevant body.

| Operation | Client | Server | Relevant PCs |
| --- | --- | --- | --- |
| Downward solid/liquid height | `fd.e(II)I` | `dj.e(II)I` | Capture acquired Chunk3/6; block31; air Material41; Block.m47, element52, material53; c60, conditional d68; Y+1 return84 or -1 return89 |
| Saved light | `fd.a(Leb;III)I` | `dj.a(Lco;III)I` | Y clamp0..15; outside Light.c54; presence74; getter87; captured Chunk call105 |
| Plant light without subtraction | `fd.m(III)I` | `dj.k(III)I` | Negative Y return5; upper clamp13; getter23; Chunk call with literal subtraction0 at36 |
| Sky visibility | `fd.l(III)Z` | `dj.j(III)Z` | Getter7; captured Chunk call19; no Y/border gate |
| Absolute block-coordinate Chunk getter | `fd.b(II)Llm;` | `dj.b(II)Lhi;` | Signed shifts; World.c7 |
| Chunk-coordinate World getter | `fd.c(II)Llm;` | `dj.c(II)Lhi;` | Read provider.v1; invoke interface b6 |
| Normal block byte | `lm.a(III)I` | `hi.a(III)I` | Actual b1; `(x<<11)\|(z<<7)\|y`; baload15; unsigned mask19 |
| Normal saved light | `lm.a(Leb;III)I` | `hi.a(Lco;III)I` | Actual Light.a1 and conditional b20; sky Nibble15 or block Nibble34; unmatched reference returns0 |
| Normal combined light | `lm.c(IIII)I` | `hi.c(IIII)I` | Sky Nibble7; positive-sky shared a=true18; wrapped subtraction25; block Nibble35; maximum return53 |
| Normal sky visibility | `lm.c(III)Z` | `hi.c(III)Z` | Actual h2; `(z<<4)\|x`; byte10, unsigned mask14; Y comparison15 |
| Nibble lookup | `wi.a(III)I` | `ob.a(III)I` | OR-composed cell11; signed packed shift16; parity22; actual byte36 or47; final mask39 or52 |

The Blank subclasses `li`/`hg` override saved and combined light with literal0,
and sky visibility with literalfalse. Their normal-header, Nibble and Light-class
operations are not reached. The downward World scan still calls the Blank block
getter and then Material.a; its body has no whole-scan Blank shortcut.

The declaring normal Chunk has public static boolean `a` and no class initializer.
Its initialfalse is the VM default. The combined getter writes true before the
later block Nibble read. The client renderer `dk.a()V` separately resets it at88
and reads it at615. That later renderer method is not implemented here.

Light initialization creates Sky(c15), publishes a12, creates Block(c0),
publishes b26, then publishes its two-entry values array d45. It invokes no
World, Registry or Random operation. The finite native Light owner represents
the actual two handles and c fields; generic enum operations and VM allocation
failure equivalence are outside that interface.

## Executed original API experiments

[`WorldQueriesObservation.java`](../../tests/reference/world/WorldQueriesObservation.java)
uses the unchanged original archives in fresh class contexts. It compiles with
the project-owned Mutation and SolidSupport helpers, whose source digests are
checked before use. Run the helper with `jar client|server fresh-root-A
fresh-root-B`, JDK17 and `--add-opens java.base/java.util=ALL-UNNAMED`.
The classpath must contain all three compiled helpers. Both World roots must
be absent. The observer writes one ASCII JSON object and terminal LF to stdout;
complete setup, call and exception streams go to stderr.

Each side completes 44 calls, 55 ordered operations, 112 snapshots and 170 raw
Context records. Thirty-five immutable array-image versions retain 640,768 full
bytes per side, with owner identity, complete hex, length and SHA-256 joins.
Snapshots also retain typed instance fields, actual cache/fallback/insertion
images, shared Chunk.a and passive Light initialization state. Per-World local
identity namespaces have explicit bridges to the global identity table.

The direct unpublished Chunk prelude proves cold Light behavior. The unchanged
World setup itself initializes Light, so later World cases are explicitly warm.
The first positive-sky late failure sets Chunk.a before a NULL BlockLight read;
the second same-class World observes the retained true value. Prepared instance
arrays and nonfinal reference inputs are recorded as separate operations. No
class-static reset or original bytecode patch supplies these outcomes.

Five JVM/compiler/version children naturally exit zero and are reaped. Before
and after fingerprints match 11 selected archive/helper/JDK inputs and all 24
compiled helper files. Complete streams and fresh World files are retained.
The initial collector verifies framing, counts, raw Context and array hashes,
image joins and source outcomes; eight framing corruptions reject per side.
It does not itself check every identity bridge, typed field or semantic
corruption. Independent structural audit supplies separate evidence; it must
not be represented as a stronger original collector result.

A separate offline audit checks typed field widths, reference owners, complete
array images, local/global identity bridges, World/Chunk closure, all nine
preparations and 787 ordered writes, raw Random state and source outcomes.
Ten semantic corruptions reject per side, including altered payloads, missing
Chunks, changed write order and raw Context hashes. Both positive side audits
and all twenty refusals finish in one naturally successful child, with all 45
selected inputs unchanged. This audits the retained original data; it runs no
new original JVM or native comparison. Earlier failed audit attempts remain
retained separately.

These are separate original direct-API observations and constructed native
contracts, with no native replay/control equivalence. Original World Blank was
not induced; no insertion-only original Chunk occurred. Dedicated construction,
general neighbor brightness, renderer reset, complete save compatibility and
full population remain unfinished. See the [runtime contract](../world/queries.md).
