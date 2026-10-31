# WorldInfo runtime and persistence contracts

The native runtime now retains an optional complete WorldInfo, with one live
time/spawn authority. This connects the owned metadata to real acquisition,
lifecycle, region transport and metadata files. It remains a composition of
already supplied dependencies; original World construction, provider ordering
and the live-player-list serializer are unfinished.

## Live cells and lifetime

`mc_world_info_live_get` returns a const stable descriptor only for READY Info.
Its four pointers target that exact owner's time/spawn members. Copy
construction copies values into another owner's cells and retains the original
name/Player identities; it never copies the source's pointer bindings.

`mc_world_runtime_live` also returns a const descriptor. Callers now dereference
its pointers to read/write values, including runtime-owned flags. The two native
test/probe callers migrate together; their command and observation schemas stay
unchanged. Generic acquisition/lifecycle interfaces already accept those cell
pointers, so their evaluation order and callback-time reads remain unchanged.

When `runtime_options.world_info` is nonnull, creation validates READY and uses
that Info's seed to validate a supplied generator. It retains the owner before
publishing contexts or cache registration, releases it on later failure, and
releases it only after successful runtime retirement. Metadata reads use all
fifteen fields, including the independently cached Player dimension. A lossy
`mc_level_info` span cannot distinguish a NULL name from nonnull empty text;
identity and nullability remain available through the Info interface.

The runtime never stores its own second live time/spawn values in this mode.
Its legacy NULL binding initializes separate private cells from the existing
store and retains the earlier store-view overlay behavior. Independent Info
holds survive either runtime retirement or persistence document replacement.
Info has no reverse World hold. Existing active-call, chunk-lease and persistent
Entity binding guards still prevent premature World destruction.

## Typed no-list metadata transport

The complete original WorldInfo classes establish no-list serialization;
[their finite graph capture](world-info.md) is separate from these new native
integration contracts. The original one-argument save handler invokes that
serializer, wraps Data and writes the metadata. It does not itself check the
session token or force version 19132. The McRegion list-taking override changes
the version; the actual World save caller checks session and passes live players.

| Original operation | Client / server | Reached order |
| --- | --- | --- |
| One-argument handler | `fm.a(ei)` / `dq.a(ct)` | Info serializer at 1, Data insertion at 17, file handling from 20; no internal session check |
| McRegion list override | `mx.a(ei,List)` / `ie.a(ct,List)` | Version 19132 at 4, inherited list save at 10 |
| World metadata caller | `fd.D()` / `dj.w()` | Session delegate at 1, handler with live players at 16, saved-data save at 25 |
| Region chunk caller | `ld.a(fd,lm)` / `hb.a(dj,hi)` | Close at 61, initiating WorldInfo at 65, size at 74, signed-int growth at 89, conversion/add/set at 92/93/94 |

`mc_world_store_save_world_info` is checked native transport for the one-argument
output. It requires held READY Info, adds this store's session admission, then
calls the actual no-list serializer. No supplied timestamp or metadata callback
replaces its internal clock. It keeps the live version, loaded LastPlayed and
cached dimension. The serializer's same-root Player rename remains visible if
later packaging, compression or file rotation fails.

`mc_level_build_record` requires the thirteen known fields with exact tag types
and optional Player Compound. It borrows those actual nodes until normal codec
use, without inspecting Player.Dimension or replacing clock/version values.
It packages an independent root/Data document and preserves unknown retained
baseline root/Data fields and root name. An absent Player removes an earlier
baseline Player. This preservation is extra native policy; the original creates
a fresh root. Codec limits charge pointer scratch, encoded bytes and retained
document storage. Normal NaN/empty-list canonicalization still applies.

The typed store route shares actual encode/gzip/checked rotation code with the
legacy writer. Only successful rotation replaces the persistence document.
Its borrowed projection gets name/Player pointers from that new document while
retaining the live Info's loaded LastPlayed and cached dimension as convenience
values. It never redecodes Dimension to decide whether an otherwise valid
serialization can commit. A later explicit load still performs the ordinary
schema decode/fallback. The old opaque-Player pointer guard remains on the
legacy entry.

Info-bound World metadata saving explicitly returns an unsupported fatal
outcome at its missing live-player-list operation. It samples no legacy metadata
clock and does not call the standalone no-list route as a substitute. Chunk-only
saving and other reached native operations retain their own behavior.

## Per-World region growth

Typed runtime chunk transport accepts a held READY Info even before metadata
exists on disk. After a successful real region write, it reads that Info's
current size, narrows reported growth to a signed Java int, sign-extends it to
long and performs wrapped long addition. It changes neither a sibling Info nor
the shared store's legacy size counter. Failed transport preserves the Info
size and output growth; reached file effects and known-empty `has_entities`
reset keep the existing native failure policy.

The legacy chunk route still requires store metadata and updates its own
document-independent size value. The two accounting targets are explicit;
sharing a region owner or store never chooses another World's scalar authority.

## Verification scope

All 89 public cases pass on native Windows MSVC 19.51, Linux GCC 13.3 and
Clang 18.1.3. Elapsed times are 126.50, 274.28 and 272.83 seconds respectively.
Linux uses Debug ASan/UBSan with leak checks; Windows uses strict Debug without
sanitizers. All 339 code/tool/fixture source files remain unchanged across each
run. clang-format 18 also passes all 172 current C/header files.

`world.runtime_info_bindings` passes 173 constructed checks per compiler, both
in an initial selected run and again in the complete suite. Its retained driver
captures include real child streams, source/executable fingerprints, metadata
and region files. Each checks all three occupied region timestamp cells against
the explicit native fixture input `fedcba98`. The Info serializer uses its own
real clock; the test compares the saved timestamp with its actual persisted
tag, without substituting a helper clock or asserting independent equality.

The binding test exercises direct cell identity, two copied Infos with
shared storage, callback mutation retained through failure, lifetime after
World/store retirement, real typed chunk and metadata writes, and record
packaging admission/preservation. The existing public suites check the legacy
storage/lifecycle protocols and the captured WorldInfo graph independently.
These controls do not establish original World startup, live-player persistence,
arbitrary allocation/OS failures, historical clock equality or whole population
acceptance.

The complete runs also refresh the existing 8,465-check Info unit and all
750 stable original-derived graph rows, including fourteen rejected verifier
corruptions. The original graph capture remains the earlier pinned execution;
this integration work runs no new original JVM or handler interoperability test.
Hosted CI and the full original World constructor are separate requirements.

Public reproduction needs the native build and Python, without game files:

```sh
cmake --build build --target mc_test_world_info_binding
ctest --test-dir build --output-on-failure -R '^world[.]runtime_info_bindings$'
```

The driver creates a fresh owned world and retains its streams, source copies,
files and report under the build's `runtime-info-contracts` directory.
