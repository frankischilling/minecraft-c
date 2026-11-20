# Original cache callback boundaries

This comparison verifies the coordinator around successful loading and naturally
triggered population. Loader decoding, Overworld generation and terrain-feature
bodies remain external callbacks. The separate [retained-provider comparison](cache.md)
composes the real C Overworld generator and finishing stages with acquisition.

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
calls observe actual load misses, Overworld generation, finishing before publication
and all nine activation registration boundaries. The southeast acquisition
completes a 2×2 square and naturally triggers the northwest population helper.

The trace binds the helper's resolved origin and observes its `n` flag set
before the provider call, with the actual provider receiver and cache-provider
argument checked. The provider returns at offset 2906; the helper resumes at 43, invokes
the original dirty method at offsets 0/5, then reaches 46. Thus the provider call
finishes before the coordinator's dirty call. Generated chunks are already
dirty in this genuine scenario. Constructed checks starting with dirty false
separately make the final dirty assignment observable.

The recorded feature effects cover complete normal chunk arrays and the final
1777 queued regions within the observed cache/world boundary. No nested
acquisition or generation occurs inside this observed body. The final repeated
hit preserves the complete post-population state. This establishes the callback boundary and the observed
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
by this adapter. The real Overworld generator retains its separate composition and
original comparisons. Provider numerical buffers are outside this adapter's C
projection; raw Random owners and seeds remain separately observed in the
original.

The separate [fresh World comparison](natural-population.md) computes seed23
origin and ring profiles through the production GAME implementation. The
four-call origin profile retains this neighborhood; the ten-call ring includes
negative coordinates, nine normal Chunks, four populated Chunks and 6,968 final
ordered light regions. Both compare their complete declared numerical and cache
projections. Public ring replay also has a full original-derived numerical
oracle, while public origin replay checks the older cache oracle. Those native
replays use separately declared minimal resources. This MCC1 adapter retains
its original imported-callback scope and historical corpus.

Original observations retain complete map/list/pending/queue contents, all five
normal arrays, flags, time and stable context identities. The debugger links
normal arrays, wrappers, world, TileEntity map and eight entity slices to helper
references. Eight explicit fallback backing tags bind the blank array chain.
Blank TileEntity/entity owners and queue-item identities retain JDI observations
without reflected helper anchors. Blank backing-array values remain outside the C blank dispatch payload.
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

A separate standard-library fixture audit independently decodes all 18 packed
streams and rederives every MCC1 input, all 104 C callback records and complete
strict JSON projections from original snapshots and event seams. It verifies
the four byte-equal controls, 55 source fingerprints, 64 stable inputs, eight
classes and all 44 method bindings parsed directly from the original classfiles.
The report SHA-256 is
`aa46836e34bdf3637588066fcda3dd0825cfb26788bf9f7a70fcdc8a3cc0f4d3`.
This audit executes neither Java nor C and makes no platform acceptance claim.

Public checks pass 34 unit methods and all four genuine C replays, alongside
four constructed cases and 28 malformed/state/owner guards.

## Exact-source platform checkpoint

Source `28a55b52c001c4c114a11ca7df844fc1f286d328` passes all 56 configured
CTests without skips on each toolchain. Each suite contains 47 public tests,
the numerical Java oracle and eight selected original comparisons.

| Environment | Full suite | Callback comparison |
| --- | --- | --- |
| Native Windows, MSVC19.51, Temurin17.0.17 | 529.51 s | 40.71 s |
| WSL GCC13.3, OpenJDK21.0.12.1, ASan/UBSan | 1044.42 s | 82.14 s |
| WSL Clang18.1, OpenJDK21.0.12.1, ASan/UBSan | 1044.86 s | 81.95 s |

Each callback capture passes C/client, C/server and client/server independently
for both loading and population. It retains 12 operations, 16 external callbacks,
394 lifecycle events, 24 inventories and four complete byte-equal controls.
All 65 attributed inputs and eight helper classes remain unchanged.

- Windows manifest:
  `build-generation-windows/reference-chunk-callbacks/capture-6fe8531e96a840efb0a1ad6615850e6a/manifest.json`;
  SHA-256 `2503ceb8aa9fd70be437407d784d3ffbc8db145f860d136ac9bc26bc345e282a`.
- GCC manifest:
  `build-generation-gcc/reference-chunk-callbacks/capture-95f890109d9746e1aee9a3535fad458a/manifest.json`;
  SHA-256 `2155860086069e1f742b31a28c520279702bd336989ea4466a5986cc54cf6c0d`.
- Clang manifest:
  `build-generation-clang/reference-chunk-callbacks/capture-49b0fb2d5fe141ad81a60e5ef9f59f49/manifest.json`;
  SHA-256 `1be63dab4d8ff19d862702ed4aaa9edf263f5bf41c1c2d69db4fcbf4e58ddd85`.

The first Windows full-suite attempt retained one unavailable original chunk
setup: its unseeded spawn search produced 28636 queued regions, exceeding that
older observer's unchanged 16384-entry cap. That attempt is preserved and
excluded from acceptance. A fresh complete 56-test retry passes without skips.
These results describe qualified finite original contexts, not every possible
constructor state.

[Hosted checks at this source](https://github.com/frankischilling/minecraft-c/actions/runs/39089309334)
pass 48 public/numerical tests per compiler and clang-format18 for 89 C/header
files.

A separate standard-library acceptance audit verifies all 55 callback source
files against exact Git objects on each platform, unchanged inputs/classes,
original controls and seams, complete MCC1 inputs and strict actual C outputs.
It checks source attribution and successful completion for all 24 selected
original manifests, binds them to the final archived CTest outputs, and verifies
168 actual passed rows, zero skips and process exit codes of zero. The exact-head
hosted checks are verified separately. The report SHA-256 is
`ef597d13ffa27582e223bd95f692cc890e4ac48f6d4ce61d8f95c77313932b2e`.

Full persistence, terrain features, other dimensions, light propagation and
playable integration retain their separate scope.
