# Parity matrix

The original target and full feature scope are tracked by
[#4](https://github.com/frankischilling/minecraft-c/issues/4) and
[the evidence-backed inventory](inventory.md). A row marked verified applies
only to the specified scenario. Neither primitive tests nor a completed scoped
issue proves whole-subsystem parity. Account authentication is the only
[approved gameplay difference](intentional-differences.md).

Statuses: Not started; Under research; Partially implemented; Implemented but
unverified; Verified for specified scenarios; Blocked (with a named dependency).
The inventory expands the broad gameplay rows into concrete child behaviors.

| Feature / issue | Evidence | Implementation | Verification and result | Status / limits |
| --- | --- | --- | --- | --- |
| Reference fingerprints and bytecode tooling [#1](https://github.com/frankischilling/minecraft-c/issues/1) | [Binary audit](reference/binaries.md), [manifest](reference/manifest.json) | `scripts/reference/binaries/inspect_jars.py` | Supplied-input inspector passed; both JARs decompiled locally; targeted `javap` checks | Verified for specified scenarios; server publisher provenance not independently pinned to official manifest |
| Java integer arithmetic, casts, explicit Random [#2](https://github.com/frankischilling/minecraft-c/issues/2) | [Java evidence](reference/java-semantics.md); actual JDK oracle, original Random callsite anchors | `include/mc/core/java/java.h`, `src/core/java/java.c` | `core.java`: 146 checks; opt-in `reference.java_semantics`: 135768 exact records | Verified for specified scenarios; no game random-consumption/order claim; text, collections and further numerical behaviors remain |
| Local asset import [#3](https://github.com/frankischilling/minecraft-c/issues/3) | [Resource routes and availability](reference/resources.md) | `scripts/resources/import_assets.py` | `tools.import_assets`: 16 synthetic tests; real import 409 resources, hashes reread | Verified for specified scenarios; external canonical audio set and native consumption unverified |
| Original headless server availability [#1](https://github.com/frankischilling/minecraft-c/issues/1) | [Launch observation](reference/launch.md) | `scripts/reference/server/run_smoke.py` (research only) | Fresh seed17; loopback offline startup, save-all, stop: pass | Verified for specified scenario; no C server or client join |
| Original client graphical launch [#1](https://github.com/frankischilling/minecraft-c/issues/1) | [Launch limitation](reference/launch.md) | None | Launcher window found; two capture timeouts; no Play action | Blocked: current UI capture cannot observe launcher; headless work continues |
| Startup, menus, settings, input, GUI scaling, achievements [#4](https://github.com/frankischilling/minecraft-c/issues/4) | [Inventory anchors](inventory.md) | None | Original matched scenes and input traces still needed | Under research |
| Movement, collision, targeting, actions, health, inventory [#4](https://github.com/frankischilling/minecraft-c/issues/4) | [Inventory anchors](inventory.md) | None | Controlled input/tick/state comparisons still needed | Under research |
| Blocks/items, recipes, smelting, tools/drops, containers/farming [#4](https://github.com/frankischilling/minecraft-c/issues/4) | [Inventory anchors](inventory.md) | None | Full registries and reference transitions still needed | Under research |
| Generation, noise, biomes, features, chunks, dimensions [#4](https://github.com/frankischilling/minecraft-c/issues/4) | [Inventory anchors](inventory.md) | Numerical prerequisites only | Exact raw/populated/lit chunk comparisons still needed | Under research; dormant Sky provider reachability unresolved |
| Seeded Perlin/octave noise [#16](https://github.com/frankischilling/minecraft-c/issues/16) | [Both-side bytecode and live numerical comparison](reference/noise.md) | `src/world/noise/noise.c`, [interface](world/noise.md) | 4850 public checks; per side 783 requests, 140218 exact double values, 399872 permutation entries and 3132 Random continuation longs | Verified for specified scenarios; full terrain and biome climate remain open |
| Seeded 2D simplex/octave noise [#19](https://github.com/frankischilling/minecraft-c/issues/19) | [Both-side methods, initialized constants and actual numerical comparisons](reference/simplex.md) | `src/world/noise/simplex.c`, [interface](world/simplex.md) | 5005 public checks; per side 812 requests, 204923 exact double values, 348160 permutation entries and 3248 Random continuation longs | Verified for specified scenarios; complete biome climate/classification and terrain remain open |
| Climate blending and biome classification [#21](https://github.com/frankischilling/minecraft-c/issues/21) | [Both-side bytecode, initialized table and actual numerical methods](reference/biome.md) | `src/world/biome/biome.c`, [ownership/interface](world/biome.md) | 33155 public checks; all 2879 shared and 32 client-only snapshots match exact table/array bits, labels and identity masks; controls agree on all three local toolchains | Verified for specified numerical scenarios; no world startup, terrain or complete biome gameplay claim |
| Normal-dimension density and base terrain [#24](https://github.com/frankischilling/minecraft-c/issues/24) | [Both-side bytecode and genuine original contexts](reference/density.md) | `src/world/terrain/terrain.c`, [interface](world/terrain.md) | 535745 public checks; 150 exact snapshots in all three pairings on MSVC/GCC/Clang, 82 original q/scratch checks per side, full state/suffix controls | Verified for specified numerical scenarios; integrated in PR #27 with native public CI; surfaces, caves, population and lighting remain separate |
| Normal surface replacement and chunk seeding [#26](https://github.com/frankischilling/minecraft-c/issues/26) | [Paired methods, seven fixed cases and original stateful corpus](reference/surface.md) | `src/world/terrain/surface.c`, [interface](world/surface.md) | 1114102 public checks; all 290 snapshots match in three pairings on MSVC/GCC/Clang at recorded source; complete state/suffix/Random controls agree | Verified for specified numerical scenarios; public CI/integration pending; caves, population and lighting remain separate requirements |
| Lighting, fluids, fire, weather/time, redstone/pistons/order [#4](https://github.com/frankischilling/minecraft-c/issues/4) | [Inventory anchors](inventory.md) | None | Fixed-tick interaction regressions still needed | Under research |
| Entities, spawning, behavior, vehicles/projectiles/pickups [#4](https://github.com/frankischilling/minecraft-c/issues/4) | [Inventory anchors](inventory.md) | None | Per-entity deterministic scenarios and persistence still needed | Under research |
| Rendering, animation, particles, sky/fog, graphics options [#4](https://github.com/frankischilling/minecraft-c/issues/4) | [Inventory anchors](inventory.md) | None | Matched original captures with justified tolerances still needed | Under research |
| Sounds/music, timing, pitch/volume/spatial behavior [#4](https://github.com/frankischilling/minecraft-c/issues/4) | [Resources](reference/resources.md), [inventory](inventory.md) | Import only | Event/timing/property comparisons still needed | Under research |
| Modified UTF-8 payloads [#9](https://github.com/frankischilling/minecraft-c/issues/9) | [Runtime and format evidence](reference/mutf8.md) | `src/core/text/mutf8.c` | `core.mutf8`: 596848 checks; every canonical UTF-16 unit byte-matches selected Java runtime | Verified for specified scenarios; no packet framing or general Java text claim |
| Raw NBT [#9](https://github.com/frankischilling/minecraft-c/issues/9) | [Both-side bytecode and original-class comparisons](reference/nbt.md) | `src/storage/nbt/nbt.c`, [API](storage/nbt.md) | `storage.nbt`: 394978 checks; per side 194 reads, 40 original writes, 12 packet controls, 157 transfers and 435 re-reads | Verified for specified scenarios; 4 negative-list departures and 20 order-only byte differences recorded; no compression/region/world-write claim |
| Gzip/zlib memory compression [#11](https://github.com/frankischilling/minecraft-c/issues/11) | [Original wrappers, region selectors and container controls](reference/compression.md) | `src/storage/compression/compression.c`, [interface](storage/compression.md) | 2351 C checks, 127 public cross-reader records; 80 containers, per side 181 original/JDK records, 35 original-to-C and 38 C-to-reference transfers | Verified for specified scenarios; 36 explicit malformed-container departure records; no region/world-write claim |
| Original-compatible saves, unknown fields, restart/round trips [#4](https://github.com/frankischilling/minecraft-c/issues/4) | [Inventory anchors](inventory.md) | None | Both-direction copied-world round trips still needed | Under research; no rewrite save writes supported |
| Protocol, synchronization, administration and server operation [#4](https://github.com/frankischilling/minecraft-c/issues/4) | [Login bytecode](reference/binaries.md), [inventory](inventory.md) | None | Full four-way matrix below still needed | Under research |
| Native client/server runtime and performance coverage [#4](https://github.com/frankischilling/minecraft-c/issues/4) | [Testing scope](testing.md) | Shared-core build only | MSVC Windows and GCC/Clang WSL locally; native Windows/Linux CI passed on PR6 and resulting main | Partially implemented; no playable/native game validation or performance claim |

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
