# Original cache acquisition evidence

The client cache `ok`, base-server cache `jb` and dedicated cache `he` share the
same acquisition control flow after class/constant-pool binding. Independent
review compares all 231 instructions in `c(II)Llm;` / `c(II)Lhi;`, through return
393. The dedicated cache has different field names and a distinct fast-entry
policy. The C [coordinator contract](../world/cache.md) preserves these choices
through explicit synchronous callbacks; terrain population features and storage
decoding remain separate implementations.

## Method boundaries

| Original location | Observed operation |
| --- | --- |
| Cache `c`, key at 2, pending removal at 14, lookup at 28 | Retain the requested packed key; remove pending before lookup, even on a hit. |
| Cache `c`, load at 46, provider at 78 | A nonnull load skips generation. An absent provider selects the retained fallback; a provider returning null follows a different failure path. |
| Load wrapper `ok/jb.d`, `he.e`, callback at 19, time at 34/37 | Sample current world time after a nonnull load returns, then assign chunk `r:J`. Caught `Exception` becomes a miss; `Error` is outside that catch. |
| Cache `c`, map put at 95, list append at 107 | Publish and append the same retained result unconditionally, including after a recursive callback inserts the same key. |
| Cache `c`, lifecycle at 120/125 | Ordinary first method is empty. Normal activation sets `c=true` before tile values and eight ordered entity lists are registered. Blank overrides are no-ops. |
| Cache `c`, population calls at 175/244/313/388 | Evaluate current, west, north and northwest anchors against live state, retaining short-circuit order and repeated presence checks. |
| Population helper, flag at 19, callback at 36, dirty mark at 43 | Set `n=true` before provider invocation. Successful normal dirty marking follows the callback. No provider changes only the flag. |
| Dedicated `he.b`, hit or force gates | A hit retains pending state. A denied miss returns the exact uninserted fallback. Base `ok/jb.b` instead acquires every miss. |
| Dedicated `he.d` | Add a pending key when either Java-wrapped midpoint distance from spawn lies outside inclusive `[-128,128]`, regardless of map presence. |

All neighbor additions, subtractions, midpoint arithmetic and packed keys use
Java integer behavior. Coordinates can alias the same key without matching the
returned object's coordinates. Present-null imported values remain outside the
C cache contract. Successful callbacks returning null produce a defensive state
error before publication; the original null-generator failure can publish null
and then throw. No general malformed-state parity is claimed.

## Genuine persistent and dedicated contexts

The production observer uses actual constructors from both pinned supplied
[archives](binaries.md). Client storage follows `hi -> mx -> fd -> ok -> ld`,
with `rj/qj` region readers. Dedicated construction follows the real
`MinecraftServer` owner and `ie -> dp -> he -> hb`, with `kq/jx` region readers.
It does not substitute the base `dj/jb` world for the dedicated `dp/he` route.
Every generator is the actual retained `yf/pk` provider already used by that
world's constructor, rather than a fresh replacement generator.

Seed 23 qualifies through a genuine immediate origin setup: spawn `(0,64,0)`,
one cached origin and one insertion, no pending keys or queued light. Constructor
origin identity is retained. The dedicated spawn trace confirms one entry and
zero random-search steps. No original game field, Random, cache, queue or world
object is assigned by the helper. Reflection bindings and measurement flags
belong only to the project-owned observation classes.

Actual `MinecraftServer()` starts one owner-bound daemon `bt`. Its verified
`run()V` bytecode is `140005b8000ba7fffa4ca7fff6`: sleep, loop and interruption
handling, without world or listener access. Dedicated marker inventories permit
only that exact original daemon, owner relationship and bootstrap sleep stack.
The client policy permits no other original-loader frame. Both policies require
one non-daemon measured main, all other threads daemon, complete suspended
inventories and finite 64-thread/64-frame limits. JDK17's direct daemon field and
JDK21's holder field/sleep chain are observed separately. This is a constructor
and API experiment, not a running dedicated-server or graphical-client test.
Server setup, `run` and `main` boundaries are forbidden during this experiment.

## Ordered corpus and C composition

Both sides execute the same six calls, retaining provider, manager, world and
cache throughout: `c(1,0)`, `c(0,0)`, `b(0,0)`, `c(32768,0)`, `c(-1,-1)`,
`c(1,0)`. This exercises two generated misses, ordinary hits and a packed-key
alias of the constructor origin. The dedicated-only eight-call schedule is
`b(9,0)`, `d(9,0)`, `b(9,0)`, `c(9,0)`, `d(9,0)`, `b(9,0)`, `c(9,0)`,
`c(0,0)`. It distinguishes denied fallback, pending production, hit retention,
removal before lookup, one generation and ordinary publication/activation.

Across all three contexts this records 20 operations, five generated chunks,
5120 actual ordered height entry/return pairs, 252 lifecycle events and 40
complete marker inventories. Full untouched controls use the same interpreted
debuggee mode and match the entire snapshot files byte for byte. Only the
debugger uses a connected ephemeral loopback socket; the game starts no listener.

The C replay adapter legitimately generates origin once against an empty cache
and queue to bootstrap its retained provider. All 82 noise layers, nine scratch
arrays, biome output, materials and both Random continuations then match the
actual original baseline. Captured manager buffers restore the constructor's
completed biome sampling through their ownership APIs. No generator noise,
scratch or Random state is imported or overwritten. Subsequent calls compose the
real C normal generator, cache coordinator, finishing and ordered lighting queue.
Each generation returns before publication and preserves prior cached payloads.

The captured original loader methods return null in these fresh-world cases;
the adapter supplies an explicit load-miss callback. All nine normal activation
callbacks are compared at their actual original entry boundaries. Exact method
descriptors, receiver/argument identities, original caller frames, all map/list/
pending/queue contents and ownership are checked throughout publication, rather
than comparing collection counts alone. Normal full data, flags, time, manager,
scratch, immutable layers and raw 48-bit JDI/serialized Random states are retained.
Map and pending projections sort signed keys for membership comparison only;
raw original bucket orders remain in the capture. Insertion and queue order
remain exact. Blank dispatch state is separate from unused backing arrays.

Genuine copied-world reload research separately confirms successful client `ld`
and server `hb` loads without generation: all five saved arrays are unchanged,
but the loaded minimum-height field remains zero rather than the generated 66.
This prevents treating deserialization as another height rebuild. Those private
observations do not establish a C disk decoder or writable-world compatibility.
The current production acquisition corpus does not trigger terrain population,
save/unload or recursive generation. Constructed callback tests cover the
coordinator's live population gates, partial failures and deliberate recursion;
they do not assert those injected callbacks occur during gameplay.

## Reproduction and public fixtures

```text
python scripts/reference/cache/compare_acquire.py --client <client.jar> --server <server.jar> --executable <mc_chunk_acquire_probe> --work-dir .local/cache-reference --tested-commit <40-hex-source>
```

Enable `MC_ENABLE_CACHE_REFERENCE_TESTS`, configure both supplied archives and
`MC_REFERENCE_TESTED_COMMIT`, then run `reference.chunk_acquire`. JDK17 or newer
with `jdk.jdi` and `jdk.jdwp.agent` is required; tested layouts are JDK17 and 21.
The controller owns a 90-second deadline; parent processes, heap, stream sizes,
collections and complete output records are bounded. These bounds are not an OS
process-tree or filesystem quota. The runtime itself needs neither Java nor an
original archive.

Missing prerequisites and proven original setup failures before measurement
produce exit77/unavailable. Wrong archives, injected Java environment, renamed
source, failed compilation, unproved child77, any post-marker failure, missing
records or preservation failures produce exit1. No unavailable run is counted
as successful parity. Exact child commands, sources, compiled classes, tools,
input/world inventories and before/after hashes are retained in each report.

The original-only fixture capture is
`.local/acquire-fixture-capture-lf/capture-61d6d0bbd8a149aba53b218b093894eb/manifest.json`,
SHA-256 `ced822e4d0fd7b7c683ef97e02286f93797fba89dfcfb90c965f107c245c97f1`.
It retains 62 unchanged inputs, nine unchanged Java17 helper classes and all
three byte-equal controls. Its source label is unrecorded, so it is fixture
production evidence rather than final exact-source/platform acceptance.

`export_acquire.py` accepts only a complete successful original-only capture,
rechecks every retained source/artifact fingerprint, and derives expectations
again from the raw original records. It never uses C output as expected data.
The public fixture is 6768211 bytes, SHA-256
`03d4aa5a128cb6ba4296f202884950f0d77b9ff805c807d2542db0afe04e13b2`.
It retains exact continuous MCA1 inputs, complete expected C projections and
complete original protocol records with only host provenance paths redacted.
The file limit is 8 MiB; expanded input/original/expected limits remain
4/32/64 MiB. No original classes, assets, private worlds or raw process logs are
included.

A separate standard-library audit independently derives all 12 packed streams,
the complete expected projections and all MCA1 imports from retained original
records. It checks 43 snapshots, 86 serialized Random states, 688 subsequent
`nextLong` values and 80 independent JDI seed matches. All 53 project sources,
62 attributed inputs and nine helper classes match this capture. The audit
report SHA-256 is
`f66c585fa8356cad130e6d50ec880787cd0d3ef52a1ff00d69e1561e81f344fd`.

`tools.acquire_reference` runs 23 Java-free protocol/execution guards;
`tools.acquire_fixture` runs five corpus, provenance, compression and export
guards. `tools.acquire_interop` replays all three inputs and checks malformed
binary input rejection.

## Exact-source platform checkpoint

Source `9a50ac67997082c89dbc43cf4274fba1427c1565` passes all 52 configured
CTests without skips on native MSVC19.51 and WSL GCC13.3/Clang18.1. Both Linux
builds use ASan/UBSan. The configuration contains 44 public tests, the numerical
Java oracle and seven selected original comparisons. Full-suite times are
431.61, 891.25 and 892.22 seconds respectively; acquisition comparison times
are 54.29, 91.08 and 91.19 seconds. Windows uses Temurin17.0.17 and both Linux
builds use OpenJDK21.0.12.1.

Each acquisition capture independently passes C/client, C/server and the shared
client/server comparison, plus the separate dedicated-gates C/server case.
It retains 20 operations, five generations, 5120 ordered height-query pairs,
252 lifecycle events, 40 inventories and three complete byte-equal untouched
controls. All 63 attributed inputs and nine helper classes stay unchanged.

- Windows manifest:
  `build-generation-windows/reference-chunk-acquire/capture-81770ac1e7d7462989e5ef4af48bc1b4/manifest.json`;
  SHA-256 `6545fc3b5242b9ce3eef915a066b1375d09ce3d28353e53eed0ec4e6d5d57e53`.
- GCC manifest:
  `build-generation-gcc/reference-chunk-acquire/capture-03e42ed50d58449fb13f653c5b6f41ef/manifest.json`;
  SHA-256 `2abcb7284d37ad120edc4278aabd2dace14c5970d6b679a1fa921e4d4b79f98a`.
- Clang manifest:
  `build-generation-clang/reference-chunk-acquire/capture-7b66a23d9c2243de8c90b7174cffac9b/manifest.json`;
  SHA-256 `26a467c3b5ad0f69a8588e6d82708f3661d6ecfa025b72d52c7899878916711b`.

A separate standard-library audit verifies all 53 project source files against
the exact Git objects, independently derives complete expected projections and
MCA1 imports from the original records, and checks strict JSON types, full
protocol identity graphs, query visibility, Random continuations, controls and
world inventories. It verifies all 156 actual passed CTest rows, the archived
full test logs, process exit codes of zero and zero skips. The audit report SHA-256 is
`09b5c938205ccb40ed4f58e7b6e698dbe9be1797eea49c7064cad0453e4e118b`.

[Hosted checks at this source](https://github.com/frankischilling/minecraft-c/actions/runs/39085300876)
pass 45 public/numerical tests per compiler and clang-format18 for 88 C/header
files. This checkpoint verifies the finite constructor/API corpus above;
successful-load and natural population callback comparisons remain separate
acceptance work. It does not establish a running dedicated server, disk decoding,
terrain features, save/unload, light propagation or complete cache lifecycle.
