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

A prior MSVC/GCC/Clang development comparison passes for 17 calls and 18 observed
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
are not independently observed. Native schema 2 reads the actual held Store
token at every boundary. The comparator binds that value to the retained Store,
its session file and the run's measured clock intervals. All eighteen original
and eighteen native Store-token boundaries are checked.

Before public integration, development suites passed 203 Windows tests and 205
on each Linux sanitizer build, with no failures, errors or skips. Both exported
Linux snapshots matched all 925 non-Markdown files at `cbe9b69`; their capture
records retain their original dirty-source attribution. Native paths are checked
as absolute Windows or POSIX paths independently of the verifier host.

## Public replay and fresh reference

The capture, source audit, comparison, fixture and replay tools live under
`scripts/reference/world/population/dedicated/`. The public fixture retains the
original JSON, passive JDI trace and nine-input constructor tape byte for byte.
Its 1,441,184-byte package expands to 25,289,891 bytes of original observations,
213,017 bytes of trace and 338 bytes of inputs. The fixture SHA-256 is
`83bc79d38671b64ab0e46901f700b1b4977c73c630294430ae42770a3c330d49`.
Decoding checks fixed byte counts and hashes, one complete compressed stream,
an 8 MiB package bound, a 32 MiB decoded bound and a maximum compression ratio
of 512.

The original project-owned test-root strings and class-origin URLs remain
unchanged. Replay never opens those captured paths. Original epoch intervals
also remain unchanged; each native run validates its own clocks. The fixture
keeps its dirty `8cb4fe4` baseline, null tested commit and uncommitted-observer
attribution. Its successful source/classfile audit is inherited evidence.
Public loading rechecks available graph, trace, tape and file invariants without
repeating the archive-dependent source audit.

Public replay runs the production native probe with project-authored minimal
translation resources and the input tape. The original graph, expected arrays
and saved files never enter the producer. Eighteen Python tests cover package
and original-semantic rejection controls. A complete actual-native comparison
and eight controls reject changes to used Random state, dedicated E, the Server
role, paths, scheduled order, typed saved NBT, the actual Store token and the
constructor inputs.

The fresh reference workflow captures schema-2 originals, reruns the source and
Random audit against the supplied server archive, extracts exact client
resources and compares a new native run. Schema-1 original fixtures require an
explicit legacy adapter; native schema 1 is rejected. See the
[test commands](../testing.md#dedicated-construction-and-synchronous-fluid-recursion).

One fresh schema-2 development run passes in 82.515 seconds. Its seventeen calls
and eighteen boundaries include 88,315 identity comparisons, 54 Random records,
2,470 arrays, 31,388 lighting records, 8,543 scheduled records, 50,850 source
object observations, 288,860 source field observations and 1,955 spawn records.
The source was dirty at `c376eba`, its tested commit is null and full
qualification remains false. Clean-source build binding, platform qualification
and hosted verification remain separate checks.

Complete server startup, networking, general scheduling, nonempty Entity, Tile,
named-data and index behavior, unrestricted unload/restart and broader game
parity remain unfinished. Native draw callsites, JVM heap identities and daemon
scheduling are outside this finite comparison.
