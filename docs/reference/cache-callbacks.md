# Original cache callback boundaries

This comparison verifies the coordinator around successful loading and naturally
triggered population. Loader decoding, normal generation and terrain-feature
bodies remain external callbacks. The separate [retained-provider comparison](cache.md)
composes the real C normal generator and finishing stages with acquisition.

Both supplied originals use genuine persistent base worlds: client `fd`/`ok`
with actual `hi` save-format factory, `mx` handler and `ld` region loader; server
`dj`/`jb` with actual `ie` handler and `hb` region loader. These are finite
headless API contexts. The actual dedicated-owner `dp`/`he` constructor and force
gates retain their separate evidence in the retained-provider comparison.
No running server, graphical application, provider replacement or original
game-field assignment is used here.

## Successful original loads

For each side, a fresh seed 23 world genuinely constructs one cached origin.
The helper calls `c(1,0)`, saves through the actual world API and closes the
actual save/region cache. A preserved baseline copy is never passed to an
original constructor. Every traced reload and untouched control receives a new
complete copy. Metadata and region files must remain byte-identical during
reload; only the existing eight-byte session lock may change.

The reload constructor leaves map/list empty. The helper calls the actual
public `world.a(J)V` time setter with 1234 before measuring two `c(1,0)` calls.
Four debugger records bind that call to the same world, metadata and main thread:
helper entry, original offsets 0 and 8, and helper return. This does not run a
world tick or scheduled-update adjustment.

The first acquisition observes one nonnull `ld`/`hb` result with original
last save time 0. The private cache loader wrapper samples live world time and
stamps that same identity with 1234 before map publication, insertion append and
activation. The second call returns the same cached identity unchanged. Both
original generation and finishing entry counts are zero. Generator/cave owners
and raw 48-bit seeds remain unchanged within each original VM.

All five loaded arrays equal their actual saved arrays exactly. The loaded
minimum-height field remains 0 although its pre-save in-memory value was 66.
The C seam must preserve this observed deserialization result rather than
rebuilding local heights. These observations do not implement a C disk decoder,
save writer or general saved-world compatibility.

## Natural population boundary

Each fresh persistent base world retains its actual constructor-cached origin.
The measured schedule is `c(1,0)`, `c(0,1)`, `c(1,1)`, `c(1,1)`. The first three
calls observe actual load misses, normal generation, finishing before publication
and all nine activation registration boundaries. The southeast acquisition
completes a 2×2 square and naturally triggers the northwest population helper.

The trace binds the helper's resolved origin and observes its `n` flag set
before the provider call, with the actual provider receiver and cache-provider
argument checked. The provider returns at offset 2906; the helper resumes at 43, invokes
the original dirty method at offsets 0/5, then reaches 46. Thus the provider call
finishes before the coordinator's dirty call. Generated chunks are already
dirty in this genuine scenario. Constructed checks starting with dirty false separately
make the final dirty assignment observable.

The complete feature-body effects are retained, including changed normal chunk
arrays and the final 1777 queued regions. No nested acquisition or generation
occurs inside this observed body. The final repeated hit preserves the complete
post-population state. This establishes the callback boundary and the observed
live gate order; terrain-feature implementations remain separate work.

## C replay and ownership

`mc_chunk_callback_probe` uses the production synchronous coordinator with a
separate bounded `MCC1` transport. Each external callback must match its complete
observed entry state before importing any effects. The adapter applies only
fields, arrays or memberships that actually changed across the original body.
Unchanged coordinator flags, time, memberships and all backing owners stay live
in C storage. C computes time stamping after loading, publication under the
requested key, insertion order and live population gates. It sets active before
registration, sets populated before calling the provider and marks dirty after
the provider returns.

Output labels every imported load/generation/population effect
`observed-original-replay`; registration effects are labeled `coordinator`.
No imported feature or generation result is reported as independently computed
by this adapter. The real normal generator retains its separate composition and
original comparisons.

Original observations retain complete map/list/pending/queue contents, all five
normal arrays, flags, time and stable context identities. The debugger links
normal arrays, wrappers, world, tile map and eight entity slices to helper
references. Eight explicit fallback backing tags bind the blank ownership chain
as well. Blank backing-array values remain outside the C blank dispatch payload.
Registration collections are genuinely empty in this finite corpus.

Every begin/end inventory suspends all threads and requires exactly the two
project helper frames on the measured main thread, with no original frame.
Every other thread must be a daemon without original-loader frames. The ordinary
server constructor's actual spawn-selection call is observed for fresh worlds;
saved-world reload correctly bypasses that selection. This base-world policy
has no dedicated sleeping-thread exception.

The transport permits 32 identities, 64 map/list/pending entries, 32768 queued
regions, 16 operations and 32 external callbacks. Input and output each have a
64 MiB cap. Original snapshot/trace records each have a 32 MiB cap. Java uses
512 MiB heaps and a 90-second controller deadline; parent commands are bounded.
World polling and final hashes cap 256 entries and 32 MiB. These are bounded
processes and observations, not an OS process-tree or filesystem quota.

## Reproduction and public data

```text
python scripts/reference/cache/compare_callbacks.py --client <client.jar> --server <server.jar> --executable <mc_chunk_callback_probe> --work-dir .local/cache-callbacks --tested-commit <40-hex-source>
```

`MC_ENABLE_CACHE_REFERENCE_TESTS` enables both `reference.chunk_acquire` and
`reference.chunk_callbacks`. JDK17+ with `jdk.jdi` and `jdk.jdwp.agent` and both
locally supplied pinned archives is required. Runtime C code needs neither Java
nor the original archives.

Exit 77 is unavailable only for missing prerequisites or proven genuine
constructor failure before any snapshot, identity tag, time mutation or measured
operation. Wrong archives, compilation failures, unproved child exit 77, post-setup
failures, omitted records or preservation failures return 1. A completed
constructor switches both project observer flags to measurement mode before
initial snapshots or the time API. Controller setup-only proof requires empty
identity maps and zero identity markers. No unavailable result is a parity pass.

The original-only producer is
`.local/cache-callback-fixture-capture-final/capture-6ba895b529044a208b3ff388ec1ab6df/manifest.json`,
SHA-256 `ddbb48bc7955aa23718df29c7446c443c42ca5b11accb582e12d5452bd74c0b2`.
It preserves 55 project sources, 64 attributed inputs, eight Java17 helper classes,
both saved baselines and four complete byte-equal untouched controls. The source
label is unrecorded: this is fixture-production evidence rather than final
exact-source/platform acceptance.

`export_callbacks.py` accepts only a complete successful original-only capture.
It rechecks sources, classes, tools, exact commands, retained worlds and records,
and derives all inputs and expected projections again from original evidence.
It never uses C output as expected data. The public fixture is 1440101 bytes,
SHA-256 `b1868941ab87113cb12ce10793a0f3a6487e73c4e8641fde14ee7013b780cb15`.

The four cases retain 12 operations, 16 external callbacks, 394 lifecycle events,
24 inventories, two successful loads, six opaque generation calls and two opaque
population calls. Complete normal data and callback effects remain lossless.
Public protocol records replace host provenance paths and omit original method
bytecode, retaining pinned class/descriptor/modifiers/length/SHA-256 proofs.
Live validation still requires and rehashes the complete privately captured
method bytes. Public fingerprint-only mode is explicit and rejects raw-byte
extras. No original classes, assets, world files or process logs are published.

Public checks pass 34 unit methods and all four genuine C replays, alongside
four constructed cases and 28 malformed/state/owner guards. Exact-source native
Windows/Linux reference suites, hosted checks and independent acceptance audit
for the expanded callback composition are pending. Full persistence, terrain
features, other dimensions, light propagation and playable integration retain
their separate scope.
