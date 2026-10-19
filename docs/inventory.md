# Original feature inventory

This is the initial discovery inventory for the supplied Beta 1.7.3 binaries,
not a completed feature census. Each row keeps its unresearched behavior in
scope. Define related implementation work from these checklists as its
acceptance tests and reference evidence become concrete. A playable subset
leaves the full inventory incomplete.

## Evidence and state

`C` means the client JAR with SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
`S` means the server JAR with SHA-256
`033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.
See [binary verification](reference/binaries.md) for provenance, tooling, and
limits. A symbol on one side does not name the same class on the other side.

All method descriptors in the table were checked with JDK 17 `javap -c -s -p`.
They are navigation anchors: the broader attribution of a subsystem to a class
comes from inspecting CFR 0.152 output. Descriptor verification does not prove
the whole checklist's behavior. The instruction evidence below states the
narrower observations checked directly in disassembly. No original source or
complete disassembly is stored in this document.

Every row is **under research**. C gameplay implementation and subsystem parity
verification for these checklists are **not started** unless a narrower entry in
[parity.md](parity.md) records otherwise. Compatibility primitives alone do not
complete simulation, world generation, saving, or networking. Unmeasured timing,
ordering, constants, failure handling, and side differences remain requirements.

The following narrower boundaries have production implementations and original
comparisons. They do not complete their parent checklists:

| Parent checklist | Implemented boundary | Remaining behavior |
| --- | --- | --- |
| Terrain, biomes and population | [Unpublished normal generation](reference/generation.md), including climate, base terrain, surface, caves and finishing | Terrain-feature population bodies, other dimensions and general load-order effects |
| Chunks and lighting | [Cached heights and finishing](reference/finishing.md); [retained-provider acquisition](reference/cache.md), with successful-load/natural-population [callback evidence](reference/cache-callbacks.md) passing exact-source MSVC/GCC/Clang acceptance; [owned disk decoding](world/storage.md) with separate exact-source MSVC/GCC/Clang acceptance | General tick-driven scheduling, propagation, simulation updates and entity/tile activation; synchronous [save/unload and retirement](world/lifecycle.md) has separate qualification |
| Saves and recovery | [Raw NBT](reference/nbt.md), [compression](reference/compression.md), [read-only inspection](reference/region.md), newer [metadata](storage/level.md), [region writing](storage/region-writer.md) and [native ownership](world/storage.md) | General tick-driven save scheduling, entity/player and subtype bodies, full running-game restart compatibility and recovery beyond checked host errors |

The [parity matrix](parity.md) records exact tested sources, narrower statuses
and evidence limits. Callback replay imports observed effects within the recorded
cache/world boundary; that replay itself does not implement a disk decoder or
terrain features. The native storage work has [separate accepted finite copied-world scenarios](reference/storage.md#exact-source-storage-checkpoint); it does not complete gameplay saving.

World startup now has a tested native [millisecond wall-clock backend](reference/wall-clock.md)
and a separate [WorldInfo owner](reference/world-info.md) with copied scalars,
shared references and no-list serialization. Native runtime binding and typed
store serialization/accounting now use that owner. A separate
[SavedData ID owner](reference/saved-data-ids.md) loads raw Short counters and
preserves copied UTF-16 keys, wrapping and reached file-failure effects.
Its native contracts and limited original-derived memory replay pass on all
three compilers. Actual HashMap disk ordering and named SavedData remain
unfinished. The [native base-World constructor](world/construction.md) now
retains all 43 fields, ordered Random/clock effects, actual Info bindings,
Dimension/biome owners and a concrete normal provider. Direct, copied and
already-loaded normal paths can reach native constructor completion. Fresh
spawn, full registry/bootstrap/subclass startup, live-player serialization and
terrain-feature execution remain unfinished. The separate finite
[Dimension observations](reference/dimension.md) qualify standalone fields,
light tables and celestial angles; they do not qualify whole-World startup.
These modules do not complete a startup checklist.

The [World mutation foundation](world/mutations.md) adds concrete block setters,
incremental height repair and ordered notifications. Its 218 native checks pass
on all three compilers. Full synchronous handlers and population remain open;
the selected native corpus does not establish original mutation parity.

The [lifecycle boundary](world/lifecycle.md) implements synchronous saving,
metadata-first accounting, history-sensitive pending unloads and explicit native
retirement. Native empty collections and external Boat/Sign callback replay
remain different domains. Accepted finite qualification and its exact-source evidence
are recorded in the [lifecycle ledger](reference/lifecycle.md#exact-source-lifecycle-checkpoint).
Entity/tile simulation, general deferred cleanup, terrain-feature bodies,
world ticking and gameplay restart/recovery stay open.

## Subsystem checklist

| Area | Concrete behavior to research and reproduce | Verified symbol anchors; checklist state |
| --- | --- | --- |
| Startup and screens | Window and applet entry behavior relevant to the native executable; startup failure screens; title screen; single-player and multiplayer navigation; texture-pack screen; quit; pause and return; death screen and respawn; world creation, seed entry, selection, rename and deletion confirmation. Check which menu labels lead to operational behavior. | C `net.minecraft.client.Minecraft.a()V`; `fu.b()V`, `fu.a(Lke;)V`; `fj.a(Lke;)V`; `rq.a(ZI)V`; `oz.b()V`; `ch.b()V`. Under research. |
| Controls and persisted options | Key defaults/rebinding, held/released input, focus loss, mouse capture, invert/sensitivity, sneaking, hotbar/drop/chat/fog controls; difficulty; render distance; bobbing; anaglyph; advanced OpenGL; frame limit; fast/fancy graphics; ambient occlusion; GUI scale; music/effect volumes; options read/write and malformed values. Native Windows/Linux event translation needs separate checks. | C `kv.<init>(Lnet/minecraft/client/Minecraft;Ljava/io/File;)V`, `kv.a()V`, `kv.b()V`, `kv.a(II)V`; `ht.<clinit>()V`; `dc.a(IZ)V`. Under research. |
| Text, GUI and progression | Font widths, color codes, wrapping, shadow, layout/scaling, slot and tooltip interactions; HUD, chat and debug display; achievements, prerequisites, notifications and persistence; statistics display/storage. Measure layout and input at multiple resolutions. | C `sj.a(Ljava/lang/String;IIIZ)V`, `sj.a(Ljava/lang/String;)I`; `ep.<clinit>()V`; `xm.a(IIF)V`; `xi` and `jl` are inspected class anchors whose persistence paths still need method-level mapping. Under research. |
| Clock and simulation order | Tick accumulation, pause, interpolation, render independence, entity/block/chunk ordering, autosave cadence, scheduled versus random updates, skipped or delayed work. Audit both client single-player and dedicated-server control flow. | C `py.a()V`, `fd.l()V`; S `net.minecraft.server.MinecraftServer.run()V`, `h()V`. Under research. |
| Movement, collision and viewpoint | Walking, jumping, sneaking, swimming, ladders, riding, knockback, falls, clipping/step height, water/lava drag, block collision bounds; view rotation, camera bob and third-person behavior; targeting and reach. Establish numeric and update-order fixtures. | C `sn.b(DDD)V`, `ls.a_(FF)V`, `dc.b(DDD)V`; `px.b(F)V`; `ob.b()F`. Under research. |
| Health, death, respawn and player state | Damage sources, armor, healing/food behavior, invulnerability timing, drowning/fire/fall damage, death drops, spawn selection, beds/sleep, respawn, carried inventory and dimension state. Do not import later hunger/experience behavior without evidence. | C `ls.a(Lsn;I)Z`, `gs.a(Lsn;I)Z`, `gs.a(Lnu;)V`, `gs.b(Lnu;)V`; `dc` and `ch` provide local-player/death-screen navigation. Under research. |
| Mining, placement and interaction | Block-breaking progress, harvest/tool checks, drops, placement orientation/metadata, item use, right-click precedence, entity attacks/interactions, buckets, signs, doors, beds and containers; single-player versus network controller behavior. | C `ob.b(IIII)Z`, `ob.a(Lgs;Lfd;Liz;)Z`, `ob.a(Lgs;Lfd;Liz;IIII)Z`; S `ha.a(Lgm;)V`, `ha.a(Leo;)V`. Under research. |
| Blocks, items and metadata | Enumerate registered block/item IDs and metadata variants; stack limits, tool classes, durability, armor, drops, light/opacity, hardness/resistance, collision/render forms; ordinary, inaccessible and legacy entries must be distinguished. Include piston parts, trapdoors, wool/dyes, maps, records and technical blocks when reachable. | C `uu.<clinit>()V`, `gm.<clinit>()V`; S `na.<clinit>()V`, `ej.<clinit>()V`. Registration is observed; each entry's gameplay remains under research. |
| Crafting, smelting and containers | Shaped/shapeless recipes, matching/consumption, outputs and remainders; workbench and player crafting; furnace fuel/cook progress and save state; chest adjacency; dispenser behavior; slot transfer, stack splitting, transaction acceptance and inventory persistence. | C `hk.a(Lmq;)Liz;`, `hk.a(Liz;[Ljava/lang/Object;)V`, `hk.b(Liz;[Ljava/lang/Object;)V`; `sk.n_()V`, `sk.a(Lnu;)V`, `sk.b(Lnu;)V`; `ix.a(ILiz;)V`. Under research. |
| Farming and renewable resources | Crop growth/harvest, farmland moisture/trampling, seeds, reeds, cactus, saplings/trees, mushrooms, grass/leaves decay, bonemeal and animal resource interactions; growth conditions and random-consumption order. Do not infer later farming systems. | C `ni.a(Lfd;IIILjava/util/Random;)V`, `ni.a(Lfd;IIIIF)V`; `uu.<clinit>()V` locates other block classes. Each child behavior is under research. |
| Terrain, biomes and population | Seed transformation and all noise passes; base terrain, surface replacement, caves, ores, lakes, dungeons, vegetation, snow/ice and other features actually generated; generation versus population order; boundaries, negative/large coordinates and load-order effects. Audit whether defined biomes/features can naturally occur. | C `yf.b(II)Llm;`, `yf.a(Lcl;II)V`, `kd.a(FF)Lkd;`; `sq` cave, `uf` noise and `er` dungeon class signatures were inspected as navigation anchors. S world counterpart `dj` needs method-by-method comparison. Under research. |
| Dimensions and portals | Overworld and Nether generation, light, time/sky, spawn rules, beds, portal construction/placement, travel scale/cooldown and return placement; dedicated-server `allow-nether`; saved dimension selection. Separately resolve the included Sky provider's reachability and required compatibility. | C `xa.a(I)Lxa;`, `xa.b()Lcl;`, `wd.a()V`, `wd.b()Lcl;`, `qn.b(II)Llm;`, `ay.a()V`, `ay.b()Lcl;`; S `net.minecraft.server.MinecraftServer.a(I)Ldp;`. Under research. |
| Chunks and lighting | Chunk addressing, height limits, generation/load/unload/save queues, dirty state, skylight and block light, opacity/emission, heightmaps, neighbor propagation and updates across chunk boundaries. Test boundaries and serialization separately from appearance. | C `fd.c(II)Llm;`, `fd.a(Leb;IIIIII)V`; `lm` chunk disassembly; S `dp` and `dj` world disassembly. Queue/state ownership and every lighting method need finer mapping. Under research. |
| Fluids, fire, weather and time | Water/lava levels and scheduling, source creation, mixing, flow vectors; fire ignition/spread/extinguishing; rain/thunder/lightning, snow/ice, sleep/weather reset; time/sky interpolation; interactions with loading, block updates and dimensions. | C `om.a(Lfd;IIILjava/util/Random;)V`, `yq.a(Lfd;IIILjava/util/Random;)V`, `fd.m()V`, `fd.n()V`, `xa.a(JF)F`; `ei.<init>(Lnu;)V`. Under research. |
| Redstone, pistons and mechanical interactions | Wire connectivity/power propagation, torch behavior, lever/button/plates, repeater delay and locking behavior actually present, powered/detector rails; normal/sticky piston extension/retraction, moving blocks, push limits, dropped blocks and ordering; combine with fluids, containers, fire and chunk edges. Do not assume later quasi-connectivity or locking rules; observe them. | C `sm.a(Lfd;IIIIII)V`, `jq.a(Lfd;IIIII)V`, `jq.h(Lfd;III)V`; registries C `uu.<clinit>()V` and S `na.<clinit>()V` locate torches/repeaters/rails. Under research. |
| Entities, spawning and mob behavior | Enumerate registered classes and naturally reachable spawn sets; hostile/passive/water mobs, wolves/taming, despawn, difficulty/light conditions, navigation and attacks; arrows/snowballs, item pickups, paintings, primed TNT/falling blocks, minecarts/boats, riding and entity collisions; serialization and server tracking. Registered Giant/Mob/Monster names do not prove natural spawning. | C `jc.<clinit>()V`, `jc.a(Lnu;Lfd;)Lsn;`, `sn.d(Lnu;)V`, `sn.e(Lnu;)V`, `ls.w_()V`; `cq` spawn class signature inspected. S `ha` packet handling still needs entity-tracker mapping. Under research. |
| Graphics and resources | Texture/font import, texture packs, animated water/lava/fire/portal/clock/compass assets, chunk/block/entity geometry, particles, sky/clouds/stars/sun/moon, rain/snow, fog, translucent sorting, light/shading, ambient occlusion and graphics options; item/hand/UI drawing. Capture original scenes with controlled settings and justified comparisons. | C `ji.b(Ljava/lang/String;)I`, `ji.a()V`, `n.a(F)V`, `px.a(FJ)V`, `dn` particle class signature; `ht.<clinit>()V` lists exposed settings. Texture or model recognition alone does not verify rendering. Under research. |
| Audio | Resource discovery/import, effect/music/stream pools, random variants, triggers, music delay, spatial listener, attenuation, pitch/volume, record playback, stopping/cleanup and options. Locate external audio files and record unavailable resources separately. | C `yo.c()V`, `yo.a(Lls;F)V`, `yo.b(Ljava/lang/String;FFFFF)V`, `yo.a(Ljava/lang/String;FFFFF)V`, `yo.a(Ljava/lang/String;FF)V`. Under research; JAR aggregate inventory contains no audio-suffix files. |
| Saves and recovery | NBT tag types/encoding, gzip and region compression, McRegion addressing, world metadata, block/metadata/light arrays, entities, tile entities, player inventories/dimensions, map/stat data, legacy conversion, locks, rename/delete, unknown fields, truncated/corrupt data and recovery. Test both directions through original-created and rewrite-created copied worlds. | C `as.a(Ljava/io/InputStream;)Lnu;`, `as.a(Lnu;Ljava/io/OutputStream;)V`, `to.a(Llm;Lfd;Lnu;)V`, `to.a(Lfd;Lnu;)Llm;`, `rj.a(Ljava/io/File;II)Lqj;`, `fm.a(Lei;)V`, `hi.a(Ljava/lang/String;Lyb;)Z`; S `mg.a(Lhi;Ldj;Liq;)V`, `hb.a(Ldj;II)Lhi;`. Under research. |
| Multiplayer, server operations and shutdown | Packet IDs/field encodings and limits, connection/login/play/disconnect state, packet ordering, keepalive/timeouts, movement validation, chunks/entities/time/weather/inventory synchronization, chat, respawn/dimension travel; server properties, permissions, bans/allowlist, commands, persistence and clean shutdown. Account authentication is the approved exception; other legacy services require a separate decision if unavailable. | C `nb.a(Lmp;)V`, `pf` transport class; S `ee.a(Liv;)V`, `ha.a(Ljz;)V`, `gt.a(Ljava/io/DataInputStream;Z)Lgt;`, `net.minecraft.server.MinecraftServer.c()Z`, `run()V`, `fc.<init>(Lnet/minecraft/server/MinecraftServer;)V`. Under research; all four runtime interoperability combinations remain unverified. |

## Narrow instruction observations

Offsets are method-relative JVM bytecode offsets. These observations establish
specific original features or representations; they do not verify the C rewrite.

| Side and method | Offsets | Observed evidence and remaining limit |
| --- | --- | --- |
| C `ht.<clinit>()V` | 7, 24, 41, 58, 75, 92, 110, 128, 146, 164, 182, 200, 218 | Loads option keys for music, sound, invert mouse, sensitivity, render distance, bobbing, anaglyph, advanced OpenGL, frame limit, difficulty, graphics, ambient occlusion and GUI scale. Rendering effects and defaults need separate checks. |
| C `kv.<init>(Lnet/minecraft/client/Minecraft;Ljava/io/File;)V` | 366 | Loads `options.txt`; read/write paths are `a()V`/`b()V`. Persistence behavior still needs fixtures. |
| C `uu.<clinit>()V`; S `na.<clinit>()V` | 1147, 1262, 1959, 3270, 3309, 3384 | Loads sticky-piston, piston, redstone-wire, repeater and trapdoor registry names on both sides. IDs/metadata and per-block behavior remain to be exhaustively transcribed and tested. |
| C `ep.<clinit>()V` | 15 through 398 | Inspected initializer creates 16 achievement definitions, starting with `openInventory` and ending with `flyPig`. This is the observed achievements system; no modern advancement schema is established by this audit. |
| C `xa.a(I)Lxa;` | 0-12, 13-24, 25-37 | Returns `wd` for -1, `rh` for 0 and `ay` for 1. Sky/provider code is present; ordinary availability is not proven by the factory alone. |
| C `kd.<clinit>()V` | 277, 300 | Includes Hell and Sky biome names. Other inspected definitions include Rainforest, Swampland, Seasonal Forest, Forest, Savanna, Shrubland, Taiga, Desert, Plains, Ice Desert and Tundra; definition does not prove selection in normal generation. |
| C `ei.<init>(Lnu;)V`; `ei.a(Lnu;Lnu;)V` | 96/90, 106/100, 116/110, 126/120 | Reads/writes `rainTime`, `raining`, `thunderTime`, `thundering`. Weather transitions and lightning behavior still need controlled observations. |
| C `jc.<clinit>()V` | 42, 213, 240 | Registry contains Arrow, Wolf and Minecart names. Other entries were inspected for the entity checklist; spawn reachability is a separate task. |
| C `as.a(Ljava/io/InputStream;)Lnu;`; `as.a(Lnu;Ljava/io/OutputStream;)V` | 4, 9 in each | Constructs gzip input/output wrappers. Exception/close paths and raw NBT behavior require separate fixtures. |
| C `to.a(Llm;Lfd;Lnu;)V` | 35, 205 | Writes `Blocks` and `Entities` fields; `SkyLight` is present at offset 58. The [finite storage schema](reference/storage.md) now verifies known fields/array sizes and opaque preservation; subtype bodies remain unverified. |
| C `rj.a(Ljava/io/File;II)Lqj;` | 45 | Uses the `.mcr` suffix. New [write/ownership observations](world/storage.md) include finite global filename/cache cases; [finite copied-world acceptance](reference/storage.md#exact-source-storage-checkpoint) now passes, with gameplay saving still separate. Local record framing retains separate [region evidence](reference/region.md). |
| C `yo.c()V` | 103, 118, 126 | Calls background-music source creation, volume and playback. Exact timing and resource availability remain unverified. |
| C/S login | See [binary audit](reference/binaries.md#bytecode-observations) | Both use login protocol integer 14 and packet integer 1. This does not establish a successful original/rewrite connection. |

## Reproduction and next discovery work

Run the following from the repository root after the local setup in the binary
audit. Keep output under ignored `.local/reference/`; these commands inspect
classes and do not launch game code:

```powershell
javap -classpath b1.7.3.jar -c -s -p fu rq fj kv sj sn ls gs ix ob hk sk fd yf qn xa wd ay kd jq sm yq om ni jc ow n px ji yo as fm to ld nu hi ep xm ht py dc > .local/reference/client-inventory.javap
javap -classpath b1.7.3.jar -c -s -p uu gm ei rj ij lm ak cq sq uf er > .local/reference/client-inventory-extra.javap
javap -classpath b1.7.3.jar -c -s -p oz ch dn xi jl pf > .local/reference/client-inventory-ui-admin.javap
javap -classpath b1.7.3_server.jar -c -s -p net.minecraft.server.MinecraftServer ee ha gt na ej dj dp mg gs dq hb > .local/reference/server-inventory.javap
javap -classpath b1.7.3_server.jar -c -s -p fc mc ky > .local/reference/server-inventory-ui-admin.javap
```

All five commands exited 0 on 2026-10-17. Client/server login symbols were
additionally disassembled by the commands in the binary audit. No method-body
claims above depend on successful recompilation of decompiled Java.

Next discovery work must enumerate each registry entry and reachable recipe,
feature, entity/spawn rule, packet and save field; map each meaningful behavior
to a scoped implementation task and controlled reference test; and compare
client/server differences.
Inspect additional bytecode or another decompiler where CFR cannot structure a
method or its interpretation is uncertain. The Sky provider, legacy/inaccessible
entries, and external services require explicit reachability or availability
findings. Full completion also requires native Linux/Windows validation, save
round trips and the four-way interoperability matrix from the development brief.
