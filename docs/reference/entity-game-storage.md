# Ordinary GAME ItemEntity persistence

Both pinned original archives use the same source order in their shared Chunk
builder and serializer. Class names below are side-specific. Native disk joins
and separate whole-Chunk original observations have their own checks below;
they do not inherit older leaf-test results.

| Operation | Client | Dedicated server | Reached positions |
|---|---|---|---|
| Shared load | `to.a(fd,nu)lm` | `mg.a(dj,iq)hi` | Entities getter231, length248, get258, cast261, factory269, q277, direct insert289, Tile getter301 |
| Shared save | `to.a(lm,fd,nu)V` | `mg.a(hi,dj,iq)V` | q=false105, current array length125, selected iterator136, next155/cast160, q=true167, Boolean save183, append192, Entities208, Tile values224, Tile save271 |
| Region wrappers | `ld.a(fd,II)lm`, `ld.a(fd,lm)V` | `hb.a(dj,II)hi`, `hb.a(dj,hi)V` | Load builder143, coordinate check152/diagnostic158, normal finish272; save serializer51/transport57 |
| Direct Chunk adoption | `lm.a(sn)V` | `hi.a(lq)V` | q2, x6/z18, wrong-location diagnostic45, slice clamp81, membership stores115/123/129/137, append148 |
| Boolean Entity save | `sn.c(nu)Z` | `lq.c(iq)Z` | Class-name1 before dead6, false16/17, id22, base writer27 |

The complete paired bytecode bodies were inspected from the unchanged pinned
listings. The [Entity NBT reference](entity-nbt.md) records archive identity and
the already-qualified semantic leaf/factory domain. Those observations do not
by themselves verify whole-Chunk coordination or disk persistence.

Each normal factory result sets q, including NULL. A nonnull Item goes directly
into the held Chunk, without World insertion/provider acquisition. Native
provenance and a Store occurrence journal follow that source prefix. Failure
retains q, constructor ID/Random effects and any already adopted Chunk holds;
normalization, cache publication and later Tile work do not run.

Save order comes from the current slice array and List occurrences. It does not
come from input-record order, Entity IDs, World lists or pointer deduplication.
A false/dead call continues, and all-false can leave q=true with no emitted known
records. A late NULL Stack reaches Item writer37 under Boolean wrapper27 and
outer serializer183. Earlier calls and q survive; disk/growth stay unchanged
when composition or transport was not reached.

The original fresh writer drops unknown tags and skipped unknown Entity records.
The native intermediate storage policy preserves unknown Entity/Stack fields
through their actual owners, and unknown factory-NULL records through the exact
Store journal. This policy is explicit and is excluded from an original-writer
equivalence claim. Mapped constructors such as Boat succeed in the original but
still stop at native constructor45; that is missing behavior, not a matched
exception. Other constructors, arbitrary subclass/iterator reentry, mutable NBT
allocation/OOM timing, relocation and complete populated persistence remain open.

`world.game_entities` is a constructed native integration test. It uses real
client/base-server Worlds, bound Class owners, Store/cache/Info/Chunk instances,
actual factories and serializers, real regions and checked joint disposal. It
covers mixed unknown records, distinct slices, duplicates, false/dead, Stack
replacement/RAW fields, separate-owner reload, partial failures and native
journal/session/quota controls. Its bounded Python driver independently decodes
all final region files. This is separate from original execution and natural
dedicated startup, removal/ticking and full population acceptance.

## Executed finite checks

All 151 configured MSVC cases and 153 cases on each Linux GCC/Clang build pass,
with zero skips and local JDK comparisons enabled. Linux uses ASan/UBSan and
leak checks on fresh byte-verified source/dependency copies. `world.game_entities`
passes 468 checks per host; `storage.nbt_overlay` passes 255. The older saving,
Tile, publication and provider-activation regressions also pass. Each host keeps
710 unchanged selected inputs, 254 zlib pins, 185 products and 35 captures.

[`GameEntityStorageObservation.java`](../../tests/reference/entity/GameEntityStorageObservation.java)
executes untouched originals with actual World constructors and independently
copied real saved baselines. It reuses the existing `ChunkObservation` and
`ChunkStorageObservation` helpers. Four fresh processes, two per side, each
complete ten cases and 117 explicit top-level calls on the selected Windows
JDK17 profile. Calls include shared decode/save, direct Chunk adoption, actual
region transport and semantic reload. Source-PC labels are maps to inspected
bytecode; nested source calls are not instrumented.

Cases cover one Item, distinct-slice Items, duplicate records, repeated adoption
of the same actual Item, dead plus NULL Stack, a late NULL Stack, unknown-only
and unknown-after-Item inputs, Boat after Item and a later wrong-type Count.
The source q effects, skipped records, preserved pre208 output on a throwing
serializer, unchanged disk after the caught region-save exception, actual Boat
success and throwing Count read were checked from retained trees and frames.
A throwing decode's local partial Chunk remains inaccessible and unobserved.

Twenty-four comparisons match complete known two-Item NBT lists from those
static/direct-region original outputs against the final two-Item baseline
saves on all three native hosts. They preserve tag types, UTF16 names, scalar
values, floating bits, List order and List metadata. Only Compound key order is
sorted. Native unknown records and the exact EntityExtra/ItemExtra preservation
metadata are separately asserted and excluded from this equality. Full Chunk
arrays, World/cache activation, clocks, other cases' native equality, original
stream/OOM timing and complete game/persistence parity are not established.
