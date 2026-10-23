# Dedicated World construction reference map

The authority is the dedicated-server JAR with SHA-256
`033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.
Method descriptors and bytecode PCs below identify the checked original bodies.
Client class names and its cache route are separate.

| Original entry | Relevant locations | Native owner/body |
| --- | --- | --- |
| `MinecraftServer.<init>()V` | q6, g11, h16, r27, s41, k49, thread construction57 | `src/server/runtime/construction.c` |
| `bt.<init>(MinecraftServer)V` / `run()V` | retain Server field2, daemon11, start15; sleep-only loop | Inert native lifetime condition; host scheduling remains unqualified |
| `dp.<init>(MinecraftServer,om,String,int,long)V` | dimension factory7, base10, D15, G construction23/store26, F31 | `src/world/runtime/construction.c` |
| `dp.b()Lbl;` | loader8, normal provider25, cache28, C31, return38 | `src/world/runtime/provider.c`; same owner later stored in base v406 |
| `dp.c(Llq;)V` | base observer2, G6, current Entity ID10, put14 | `src/world/runtime/entity_publication.c` |
| `dj.a(List)V` | entire b.addAll5, input size15/get26/cast31/hook34 | Whole-batch publication before per-element dedicated hooks |
| `dj.b(List)V` | deferred D.addAll5 | Deferred detach; no G removal |
| `go.<init>()V` | factor7, threshold13, table22 | `src/world/runtime/dedicated/entity_index.c` |
| `go.a(ILjava/lang/Object;)V` | replace all matching values42, modification62, insertion helper71 | Updates matching nodes, then always prepends another |
| `go` insertion/resize helpers | node24/publish27/count36, pre-count comparison43/resize54; allocation22/transfer30/table36/threshold48 | Cached-hash transfer reverses bucket chains |
| `go` removal / clear | modification55/count65/unlink82 or90; clear modification7/count35 | First-match removal; clear changes modification even when empty |
| `he.d(int,int)V` | wrapped center/spawn guard, then pending add | Dedicated drop path; inclusive spawn-protection boundary |
| `he.a()Z` | subclass E gate, detach63, main68, extra73, pending/map/list removal81/92/103, loader126/provider135 | Actual dedicated maintenance; client/base cache cannot produce this pending path |

The `og` node stores value6, next12, key17 and cached hash22. The index uses
defined unsigned C operations for Java wrapping and unsigned shifts. Duplicate
keys, NULL values, resize reversal and modification counters remain observable.
Typed native holds are independent for every nonnull occurrence, including
duplicate keys. Node/allocation limits are native resource policies.

The [World contract](../world/construction.md#dedicated-construction) describes
the five subclass fields, separate base fields and terminal lifetime rules.
The [Server contract](../server/construction.md) distinguishes default
construction from full startup. Focused constructed tests exercise the actual
owners and failure prefixes. They do not by themselves establish original-game
construction, unload/reload or complete server parity.

A finite Windows/MSVC development comparison passes for 17 calls and 18 observed
boundaries. It prepares metadata through actual Info/save/readback APIs, opens a
new handler, constructs a nonnull Server and dedicated `dp` World, then follows
protected and far drops, maintenance and reload. The comparator checks 54 Random
records, 2,470 arrays, 31,388 lighting records and 8,543 scheduled records across
those boundaries.

Nine independently audited default-constructor inputs provide measured clock
samples after the actual uniquifier CAS. The comparison preserves the used
`World.r` state and the lazy Math owner, with no supplied Random state or shared
counter. Inline Random cells, embedded noise arrays and native storage paths
have explicit semantic correspondences rather than JVM heap identities.

Within each run, complete file bytes and directory transitions are retained.
Cross-run save comparison checks typed NBT, region allocation and padding;
compressed record bytes differ. Measured clocks and host paths have separate
checked correspondences. Empty Entity, Tile, named-data and index collections
leave their nonempty behavior outside this profile. Native Random draw callsites
and per-boundary private Store-token fields are not independently observed.

The capture and comparison tools remain private `.local` prototypes. This result
has no exact committed-code or platform qualification and supplies no public
replay. Complete dedicated startup, general unload/restart behavior and broader
game parity remain unfinished.
