# Full saves of a held GAME World

`mc_world_runtime_save_detailed` now reaches metadata and SavedData before
saving the actual cache insertion list. The admitted GAME route requires a
valid, genuinely empty live Player list and a genuinely empty named SavedData
list. The named cache, ordered list and ID map have separate retained owners.
An ID map may contain counters while the named list remains empty.

The World checks its session first. McRegion then sets the held Info's version
to 19132 before the serializer reads the actual Player list. That mutation
survives a later failure. Empty Player lists select no Player record even when
Info retains loaded Player NBT. The retained Player, its root name and cached
dimension stay unchanged. Nonempty Player lists stop at the unfinished Player
serializer.

The common Info writer reads seed, spawn, time and SizeOnDisk, samples the
actual platform clock for LastPlayed, then reads name, version and weather.
Writing LastPlayed does not change the live Info's loaded last_played field.
The existing one-argument serializer still selects Info's retained Player.
Bare Info-bound COMPOSITION runtimes have no actual Player list and retain
their explicit unsupported result.

Metadata precedes Chunk saving. The first save of the seed23 origin therefore
writes SizeOnDisk zero; actual region growth reaches live Info only afterward.
A second save can write the updated size while the already-clean Chunks
remain unchanged. The original full-save method discards the cache save's
Boolean; the native detailed API reports the lifecycle status.

SavedData save examines its actual ordered named list. An empty list returns
before consulting storage or ID entries. It does not rewrite idcounts.dat,
reset the retained caught diagnostic, sample a clock or draw Random values.
Named subtype construction, lookup, insertion, dirty handling and nonempty
serialization remain unfinished and cannot report success through this path.
The manager owns its native storage binding independently of the Store's
lifetime.

The source selections are paired client/base-server methods:

| Operation | Client | Base server | Reached order |
| --- | --- | --- | --- |
| Full save | `fd.a(ZLyb;)V` | `dj.a(ZLpj;)V` | Save-enabled4, optional progress20, metadata26, optional progress36, cache47, discard52 |
| Metadata | `fd.D()V` | `dj.w()V` | Session1, handler/Info/Player list5/9/13, list-save16, SavedData25 |
| McRegion list save | `mx.a(Lei;Ljava/util/List;)V` | `ie.a(Lct;Ljava/util/List;)V` | Actual version setter4, superclass10 |
| Info list serializer | `ei.a(Ljava/util/List;)Lnu;` | `ct.a(Ljava/util/List;)Liq;` | Actual size14, optional first Player24/write49, common writer56 |
| Info common writer | `ei.a(Lnu;Lnu;)V` | `ct.a(Liq;Liq;)V` | Clock63, LastPlayed66, name76, version86, optional Player137 |
| SavedData save | `hc.a()V` | `es.a()V` | Actual ordered list size7, empty return52 |

Native checked encoding, file rotation and unknown root/Data retention remain
explicit storage policies. Original handler file exceptions are printed and
swallowed, and its delete/rename return values are ignored. Those failure
paths have not reached full-game parity. The user-approved authentication
exception does not extend to them. Restart, unload, Player serialization and
named SavedData subtype persistence remain acceptance work.

Native regression controls cover session-before-version order, retained version
after serializer failure, empty retained Player-list backing, stored Player
omission without mutation, and completed metadata retained after auxiliary
failure. Separate SavedData controls preserve nonempty ID maps, raw counter
files and prior caught diagnostics across an empty named-list save.
