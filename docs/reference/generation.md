# Direct normal generator composition evidence

This increment targets client `yf.b(II)Llm;` and server `pk.b(II)Lhi;`: direct
unpopulated normal chunk generation through full finishing, before cache
publication. The [C contract](../world/generation.md) describes ownership and
resource behavior. The four-seed original-only production capture and public
C replay pass; exact-source platform comparisons are being prepared.

## Original bindings and stage boundaries

The supplied archives retain these SHA-256 fingerprints:

| Side | SHA-256 |
| --- | --- |
| Client | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` |
| Server | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` |

The selected direct entries have matching stage offsets after explicit class
and method correspondence. This does not establish whole-class equivalence.

| Entry offset | Original operation | C composition boundary |
| --- | --- | --- |
| 17 | Reseed retained generator Random `j` from signed chunk coordinates widened separately to long. | `mc_terrain_normal_chunk_seed` and the separate generator Random. |
| 22 / 36 | Allocate a fresh zero block array, then construct the real normal chunk retaining it. | Borrowed output blocks and fresh `mc_normal_chunk` constructor data. |
| 65 / 68 | Direct `xv.a([Lkd;IIII)[Lkd;` / `ph.a([Lgs;IIII)[Lgs;` call at wrapped block origins, then store generator `v`. | `mc_biome_sample_square` into the generator's separate biome output. |
| 81 | Retain the manager's processed temperature array `a`. | Borrow processed manager `a/b` as the base/density climate input. |
| 93 | Generate normal density and base blocks. | `mc_terrain_normal_generate`. |
| 104 | Replace the surface using current biome material fields. | `mc_terrain_normal_surface`. |
| 119 | Invoke the retained cave helper through its public scheduler entry. | `mc_terrain_normal_caves`, with a separate cave Random. |
| 124 | Invoke actual full normal finishing `lm.c()V` / `hi.b()V`. | `mc_chunk_finish_normal`, with the existing world/cache/queue context. |
| 129 | Return the same freshly constructed chunk. | Return the complete unpublished result. |

The cache's later `ok.c(II)Llm;` / `jb.c(II)Lhi;` operation publishes the
returned identity at offset 95 and appends it to the insertion list at 107.
Direct generation performs neither publication nor population. A full cache
acquisition implementation must also preserve pending removal, load/fallback,
lifecycle and population behavior; this composition does not provide it.

## Constructor state, climate ownership and materials

The generator constructor creates three distinct zero surface arrays, a cave
helper and an integer array of shape 32 by 32, then initializes its retained
Random from the world seed. Its eight terrain channels contain
16/16/8/4/4/10/16/8 improved-noise layers in declaration order. All 82 layers and
the constructor Random continuation matter, including channels unused by the
direct entry. The default-seeded cave helper Random is separate and is reseeded
by every public cave pass; its arbitrary pre-pass state must not initialize
terrain channels.

The direct biome call updates the borrowed world manager's `a/b/c` arrays and
stores its biome results in generator `v`. It does not assign manager `d`.
Research observes `d` remaining null; the C contract also preserves a caller's
already populated `d`. A fresh C generator must borrow the world's existing
manager rather than reconstructing its mutable buffers as part of generation.

Observed reusable sizes are 425 doubles for `q/d/e/f`, 25 for `g/h`, and 256
for `r/s/t`; `v` holds 256 biome identities. These arrays and `i` have distinct
identities retained across repeated calls. `i` remains 32 by 32 and `w` remains
null during the measured unpopulated entry. Neither is read by that entry.
The C implementation retains the numerical scratch and biome owner required
by its declared interface; it does not claim to reproduce unused Java object
allocation identities.

Both originals' 13 initialized biome identities expose current public byte
fields `p/q`. Desert `h` and Ice Desert `j` initially have top/filler 12/12;
the other eleven initially have 2/3. These are observations of current registry
state, not immutable material rules. Production composition receives all 13
current pairs explicitly and selects the pair associated with each sampled
biome. It must not substitute the fixed defaults used by a narrower numerical
probe.

## Existing direct-generation research

The ignored research directory `.local/cache-lifecycle-research/` retains
`generation-notes.md`, original snapshots/traces, untouched controls and
`generation-audit.json`. Native Windows Temurin 17.0.17 constructed fresh
seed-23 worlds from both supplied archives. A fresh generator then made direct
calls `(1,0)`, `(0,0)`, `(1,0)`, retaining that generator and its climate/scratch
objects across the three calls. Each call returned a distinct normal chunk.

The audit checks six genuine generations and 114 ordered JDI stage events,
57 per side. It verifies fresh zero blocks, constructor/base/surface/finish
handoffs, Random boundaries, scratch shapes and identities, complete returned
local data, repeated-coordinate reproduction, and unchanged nonqueue cached
context. Separate fresh runs without JDWP produce byte-equal entire snapshot
files on each side. Corresponding original block-stage hashes agree directly.
The helpers read original state; they replace no world, provider, cache, queue
or Random, transform no original class and perform no measured publication,
population or queue drain.

| Chunk | Random state immediately after reseed | Generator state after surface | Cave state after its public pass |
| --- | --- | --- | --- |
| `(1,0)` | `319032726373` | `161302643799564` | `23799015621029` |
| `(0,0)` | `25214903917` | `148673822184050` | `146301693162574` |

Constructor, climate and base preserve the reseeded generator Random. Surface
advances it; cave generation and finishing preserve the surface continuation.
Finishing also preserves the cave continuation. The repeated `(1,0)` call
reproduces every recorded block-stage hash and complete final local data after
the intervening origin call and reused scratch contents.

The research audit SHA-256 is
`40956e68acfa4fdf7977240e16f9278f9a34a1b869291eb53e829987ec83704b`.
Its retained source, class, snapshot, trace and original-JAR fingerprints
identify this research evidence. These helpers are research tools, not the
production acceptance observer. This seed-23 result does not prove multi-seed,
current-source C composition or platform coverage.

## Finite setup seed qualification

The separate ignored `seed-search/` directory records a bounded search for
genuine server constructors that accept the origin spawn immediately. It
attempted seed 23 first, then seeds 0 through 51 excluding 23, and stopped upon
finding four qualifiers within the allowed 0 through 255 range. All 52
attempts remain recorded: 23, 24, 26 and 51 qualify; the other 48 have retained
first-random-step rejection evidence.

A qualifying run has one actual `dj.c()V` spawn entry at code index 0 and zero
visits to its random-step instruction at 24. A completed helper marker must
identify the same original world and thread. The actual completed world has
spawn `(0,64,0)`, one map key 0 resolving a normal origin chunk, one matching
ordered insertion identity, and empty pending and lighting queues. A fresh
untouched setup without JDWP must produce the same complete setup record.

| Seed | Complete setup/control SHA-256 |
| --- | --- |
| 23 | `87d919fd7e4e6f44ea454d16230af731ed96a0f81b724aa2fcad84caa23a35d6` |
| 24 | `63c870fcb065c6989f602539b2e679ea6f36f2825444405f7ac72a4bc301018c` |
| 26 | `34a1eba69f6a8b97a912a3d1a629b8e1bd24ff706d0b2361430f5872a4207dd9` |
| 51 | `2fcba4ff116728c362c85729e13891bc9c1cca528e0578b17c8cd6b4f94f82d9` |

Each rejected child was stopped after its first actual random-step visit. Its
remaining walk, final spawn position and final setup state were not observed.
No rejected attempt was converted into a qualifying context by changing
original state.

The search used fresh owned roots, a 512 MiB JVM, loopback JDWP on an ephemeral
port, a 20-second debugger deadline, a 30-second outer process bound, and
20-second untouched controls. Streams and complete setup records were bounded
to 2 MiB. At most three spawn/helper breakpoint events were allowed per attempt;
no per-height trace was installed. These are direct process and observation
bounds, not an operating-system process-tree or filesystem quota.

The successful search manifest SHA-256 is
`86c1f2220aacdb8eae00dad8de1985ba0987adce084c4ee0d44c6cbcb340fcb2`.
It retains every attempted command/result, qualifying control, rejected trace,
source/JAR/JDK and compiled-helper fingerprint, with matching before/after
inputs and classes. `validation.json`, SHA-256
`548f52765e54f3d9cb01eee721be31aaf88fd0b27bb6b8e34115cb6eced000a7`,
independently confirms the 52 attempts, four qualifiers and unchanged attributed
bytes. This qualifies finite genuine setup routes on the recorded runtime;
production use must recheck their setup and control evidence.

## Production comparison and acceptance boundary

The production corpus uses all four qualifying world seeds and this ordered
nine-call sequence per retained fresh generator:

1. `(1,0)`
2. `(0,0)`
3. `(1,0)` again after the intervening call
4. `(-1,-1)`
5. `(32768,0)`
6. `(2000000,0)`
7. `(-2000000,0)`
8. `(INT32_MIN + 8,0)`
9. `(INT32_MAX - 9,0)`

The sequence includes ordinary cached/missing contexts, repeated generation,
negative addressing, a packed-key alias, world-border positions and both
endpoints of the accepted ordinary cave-scheduler domain. The wider original
integer-edge scheduler includes empty ranges and possible loop-counter
nontermination; those excluded coordinates are not accepted by this C entry.

Production observation constructs a fresh actual `yf` / `pk` after genuine
world setup and calls its original direct entry. It injects no pre-cave blocks
from C and substitutes no world/cache. The comparison retains
complete returned chunks, all 82 terrain layers, relevant climate and scratch
state, separate Random continuations, actual world queues and 1024 ordered
height queries per call. Each side runs a fresh untouched no-JDWP control of
the complete observations; all eight complete snapshot/control files match
byte for byte.

The arbitrary constructor state of the unconsumed cave Random cannot be
required to match across independently constructed JVMs. Its raw trace remains
evidence; only that initial continuation is excluded from direct equality.
After each public cave call reseeds it, its complete final state is comparable.
Generator construction, current materials, retained manager ownership and
post-call state remain explicit comparison requirements.

The original-only capture is
`.local/reference/normal-fixtures/capture-967f1ecd98204159afd51e1ff5ff2287/manifest.json`,
SHA-256 `66aa2dac085710fd00e177e5570250dae012017a50a9a5b09720ffebd18f0566`.
Its 56 input fingerprints and eight compiled helper fingerprints are unchanged
before and after execution. It records 72 complete generations, 1224 stage
events, 144 suspended marker inventories and 73728 exact height-query pairs.
The helpers run unchanged original classes from the selected supplied archive.

The lossless public fixture `tests/reference/generation/normal_fixtures.json`
is 2032841 bytes, SHA-256
`cd712382a736a394e6d30a11e776b9b585b051faecf32da4bc0a96bd7492b000`.
It contains eight exact MCG1 inputs and four complete original projections,
compressed with zlib/base64. Native Windows C replay passes all eight continuous
nine-call inputs, comparing full chunk arrays, constructor noise, scratch,
biomes, selected materials, manager arrays, raw Random states/continuations,
queues and queries. No expected output is produced by C during export.
The original records and untouched controls remain ignored local artifacts.

Constructed API tests pass 99073 checks on native MSVC19.51 and WSL
GCC13.3/Clang18.1 with ASan/UBSan on Linux. They include allocation rollback,
retained owners/results, explicit distinct material pairs and no-sky behavior.
Exact-source full platform comparisons and hosted checks remain pending.
The existing [density](density.md), [surface](surface.md), [cave](caves.md) and
[finishing](finishing.md) evidence covers their specified stages; it does not
replace a composition comparison or establish cache lifecycle, population,
propagation, other dimensions, saves or playable client/server parity.
