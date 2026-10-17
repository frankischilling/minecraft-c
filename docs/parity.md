# Parity matrix

The [evidence-backed inventory](inventory.md) records the original target and
full feature scope. A row marked verified applies only to the specified scenario.
Neither primitive tests nor a completed scoped increment proves whole-subsystem
parity. Account authentication is the only
[approved gameplay difference](intentional-differences.md).

Statuses: Not started; Under research; Partially implemented; Implemented but
unverified; Verified for specified scenarios; Blocked (with a named dependency).
The inventory expands the broad gameplay rows into concrete child behaviors.

| Feature | Evidence | Implementation | Verification and result | Status / limits |
| --- | --- | --- | --- | --- |
| Reference fingerprints and bytecode tooling | [Binary audit](reference/binaries.md), [manifest](reference/manifest.json) | `scripts/reference/binaries/inspect_jars.py` | Supplied-input inspector passed; both JARs decompiled locally; targeted `javap` checks | Verified for specified scenarios; server publisher provenance not independently pinned to official manifest |
| Java integer arithmetic, casts, explicit Random | [Java evidence](reference/java-semantics.md); actual JDK oracle, original Random callsite anchors | `include/mc/core/java/java.h`, `src/core/java/java.c` | `core.java`: 146 checks; opt-in `reference.java_semantics`: 135768 exact records | Verified for specified scenarios; no game random-consumption/order claim; text, collections and further numerical behaviors remain |
| Float trig lookup and double floor | [Both-side bytecode, initialized table and actual scalar calls](reference/math.md) | `src/core/math/math.c`, [interface](core/math.md) | 903 public checks; all 65536 table entries and 366 scalar calls match in three pairings on MSVC/GCC/Clang at recorded source, with fresh Temurin17/OpenJDK21 observations | Verified for specified numerical scenarios; integrated with native public CI; sampled scalar inputs, measured backends and no cave claim |
| Local asset import | [Resource routes and availability](reference/resources.md) | `scripts/resources/import_assets.py` | `tools.import_assets`: 16 synthetic tests; real import 409 resources, hashes reread | Verified for specified scenarios; external canonical audio set and native consumption unverified |
| Original headless server availability | [Launch observation](reference/launch.md) | `scripts/reference/server/run_smoke.py` (research only) | Fresh seed17; loopback offline startup, save-all, stop: pass | Verified for specified scenario; no C server or client join |
| Original client graphical launch | [Launch limitation](reference/launch.md) | None | Launcher window found; two capture timeouts; no Play action | Blocked: current UI capture cannot observe launcher; headless work continues |
| Startup, menus, settings, input, GUI scaling, achievements | [Inventory anchors](inventory.md) | None | Original matched scenes and input traces still needed | Under research |
| Movement, collision, targeting, actions, health, inventory | [Inventory anchors](inventory.md) | None | Controlled input/tick/state comparisons still needed | Under research |
| Blocks/items, recipes, smelting, tools/drops, containers/farming | [Inventory anchors](inventory.md) | None | Full registries and reference transitions still needed | Under research |
| Generation, noise, biomes, features, chunks, dimensions | [Inventory anchors](inventory.md) | Numerical prerequisites only | Exact raw/populated/lit chunk comparisons still needed | Under research; dormant Sky provider reachability unresolved |
| Seeded Perlin/octave noise | [Both-side bytecode and live numerical comparison](reference/noise.md) | `src/world/noise/noise.c`, [interface](world/noise.md) | 4850 public checks; per side 783 requests, 140218 exact double values, 399872 permutation entries and 3132 Random continuation longs | Verified for specified scenarios; full terrain and biome climate remain open |
| Seeded 2D simplex/octave noise | [Both-side methods, initialized constants and actual numerical comparisons](reference/simplex.md) | `src/world/noise/simplex.c`, [interface](world/simplex.md) | 5005 public checks; per side 812 requests, 204923 exact double values, 348160 permutation entries and 3248 Random continuation longs | Verified for specified scenarios; complete biome climate/classification and terrain remain open |
| Climate blending and biome classification | [Both-side bytecode, initialized table and actual numerical methods](reference/biome.md) | `src/world/biome/biome.c`, [ownership/interface](world/biome.md) | 33155 public checks; all 2879 shared and 32 client-only snapshots match exact table/array bits, labels and identity masks; controls agree on all three local toolchains | Verified for specified numerical scenarios; no world startup, terrain or complete biome gameplay claim |
| Normal-dimension density and base terrain | [Both-side bytecode and genuine original contexts](reference/density.md) | `src/world/terrain/terrain.c`, [interface](world/terrain.md) | 535745 public checks; 150 exact snapshots in all three pairings on MSVC/GCC/Clang, 82 original q/scratch checks per side, full state/suffix controls | Verified for specified numerical scenarios; integrated with native public CI; surfaces, caves, population and lighting remain separate |
| Normal surface replacement and chunk seeding | [Paired methods, seven fixed cases and original stateful corpus](reference/surface.md) | `src/world/terrain/surface.c`, [interface](world/surface.md) | 1114102 public checks; all 290 snapshots match in three pairings on MSVC/GCC/Clang at recorded source; complete state/suffix/Random controls agree | Verified for specified numerical scenarios; integrated with native public CI; caves, population and lighting remain separate requirements |
| Normal cave scheduling and carving | [Paired bytecode, ten natural cases and 44 protected controls](reference/caves.md) | `src/world/terrain/caves.c`, [interface](world/caves.md) | 2021631 natural and 2917405 protected unit checks; all three independent pairings pass on MSVC/GCC/Clang at recorded source, with full state/table/suffix/Random controls | Verified for specified scenarios; integrated with native public CI; finishing, lighting and population remain separate |
| Fresh chunk arrays, heights and direct skylight | [Paired original constructors, packed access and full finishing observations](reference/chunk.md) | `src/world/chunk/data.c`, `src/world/chunk/nibbles.c`, [interface](world/chunk.md) | All five fresh/repeated cases and complete packed-access observations match in three pairings on MSVC/GCC/Clang at the recorded source; 101534 contract checks, 414 nibble checks and 335 full-fixture checks pass | Verified for specified local-state scenarios; cache/queue effects, propagation and whole chunk lifecycle remain separate |
| Lighting, fluids, fire, weather/time, redstone/pistons/order | [Inventory anchors](inventory.md) | None | Fixed-tick interaction regressions still needed | Under research |
| Light-region merging and ordered queue storage | [Actual merge and world-queue observations](reference/lighting.md) | `src/world/lighting/queue.c`, [contract](world/lighting.md) | 48 merge cases, 72 requests and 43 world requests per side; complete C replay and actual original million-entry abort; all 33 local tests pass on MSVC/GCC/Clang, with native public CI | Verified for specified storage scenarios; integrated; world admission, query tracing, finishing dirty effects and propagation remain open |
| Entities, spawning, behavior, vehicles/projectiles/pickups | [Inventory anchors](inventory.md) | None | Per-entity deterministic scenarios and persistence still needed | Under research |
| Rendering, animation, particles, sky/fog, graphics options | [Inventory anchors](inventory.md) | None | Matched original captures with justified tolerances still needed | Under research |
| Sounds/music, timing, pitch/volume/spatial behavior | [Resources](reference/resources.md), [inventory](inventory.md) | Import only | Event/timing/property comparisons still needed | Under research |
| Modified UTF-8 payloads | [Runtime and format evidence](reference/mutf8.md) | `src/core/text/mutf8.c` | `core.mutf8`: 596848 checks; every canonical UTF-16 unit byte-matches selected Java runtime | Verified for specified scenarios; no packet framing or general Java text claim |
| Raw NBT | [Both-side bytecode and original-class comparisons](reference/nbt.md) | `src/storage/nbt/nbt.c`, [API](storage/nbt.md) | `storage.nbt`: 394978 checks; per side 194 reads, 40 original writes, 12 packet controls, 157 transfers and 435 re-reads | Verified for specified scenarios; 4 negative-list departures and 20 order-only byte differences recorded; no compression/region/world-write claim |
| Gzip/zlib memory compression | [Original wrappers, region selectors and container controls](reference/compression.md) | `src/storage/compression/compression.c`, [interface](storage/compression.md) | 2351 C checks, 127 public cross-reader records; 80 containers, per side 181 original/JDK records, 35 original-to-C and 38 C-to-reference transfers | Verified for specified scenarios; 36 explicit malformed-container departure records; no region/world-write claim |
| Original-compatible saves, unknown fields, restart/round trips | [Inventory anchors](inventory.md) | None | Both-direction copied-world round trips still needed | Under research; no rewrite save writes supported |
| Protocol, synchronization, administration and server operation | [Login bytecode](reference/binaries.md), [inventory](inventory.md) | None | Full four-way matrix below still needed | Under research |
| Native client/server runtime and performance coverage | [Testing scope](testing.md) | Shared-core build only | MSVC Windows and GCC/Clang WSL locally; native Windows/Linux CI passed before and after integration | Partially implemented; no playable/native game validation or performance claim |

## Multiplayer interoperability

All scenarios must pin both binaries, configuration, platform, copied world,
username, actions, and restart results. Use loopback and offline configuration
where appropriate. Matching protocol integers are insufficient.

| Client | Server | Result |
| --- | --- | --- |
| Original | Original | Not tested; original server smoke passes without a client |
| C | C | Not started; both runtimes absent |
| C | Original offline | Not started; C client absent |
| Original | C | Not started; C server absent |

## Completion evidence still required

Every inventory behavior needs implementation plus original comparisons; this
table must expand as research discovers distinctions. Required end-state evidence
includes native Linux/Windows graphical sessions and headless operation, exact
world generation checks, both save round-trip directions, all four multiplayer
pairings, extended state-drift runs, and presentation/input/audio comparisons.
Missing prerequisites are unavailable results, never passes. Public tests contain
no original binaries, assets, source, or launcher data.
