# Entity construction

`mc_simulation` provides the Entity and ItemEntity construction API in
[`mc/entity/entity.h`](../../include/mc/entity/entity.h). It represents the client
`sn`/`hl` and server `lq`/`ez` constructor state, owned children and native failure
prefixes. The [reference evidence](../reference/entity-construction.md) separates
native contracts, original constructor snapshots and scalar formula replay.
The separate [NBT and canonical Class contract](nbt.md) covers semantic Item
records, the class-name save wrapper and the world-only NBT factory.

The separate [Minecart module](minecart.md) adds its class initializer,
World-only and positioned constructors, and real detector membership.
The [hostile Mob module](mob-construction.md) adds the five default Overworld
hostile constructors, early watcher hooks and Skeleton's static bow Stack.
Native Minecart construction/publication/detector contracts pass on all three
compiler profiles, including the [committed hosted checkpoint](../reference/minecart-construction.md#committed-and-hosted-checkpoint)
at `39edf12`. A separate [Minecart constructor comparison](../reference/minecart-construction.md#constructor-observations-and-qualification)
matches all ten literal null-World inputs per client/server side against
Windows MSVC, under the complete declared projection with zero mismatches.
The Item observations and formula replay described here retain their own
inputs and do not cover Minecart or rail physics.

## Construction and ownership

One `mc_entity_classes` owner represents one loaded game-class identity across
worlds and subtypes. It borrows the existing application Random profile shared
with those worlds and registries. The profile must outlive every dependent user.
Creating this owner performs no game-class initialization or entropy draw. All
Entity/class access requires exclusive access.

Call `mc_item_entity_allocate` before evaluating constructor arguments. It runs
the Entity counter initializer on first active use and publishes one RAW native
hold. All instance fields start at their Java raw defaults: integer zero, false,
positive floating zero and null references. Allocation consumes no entity ID or
entropy and creates no child owners. Only RAW instances can enter a constructor.

The base constructor consumes the old class counter as the ID and advances the
counter with Java int wrapping. It creates a fresh AABB, applies base defaults,
constructs a separate ordinary default Random at base PC 127 and publishes it at
130. It then creates the watcher, stores the world at PC 205, positions the
entity at zero at PC 212, registers Byte0 at index 0 at PC 224 and invokes
ItemEntity's empty initialization hook. The private Random is not drawn during
construction.

Both ItemEntity overloads then set `item.b=0`, `item.f=5`, obtain their first
shared `Math.random()` value, set size to `0.25f` by `0.25f` and offset to
`height/2.0f`. Their remaining effects and total Math returns are:

| API | Further effects | Shared Math returns (total) |
| --- | --- | ---: |
| `mc_item_entity_construct` | Position at supplied XYZ; retain/store stack at item PC 61; yaw; X motion; literal Y motion; Z motion. | Four, at item PCs 16, 65, 77, 101 |
| `mc_item_entity_construct_world` | Return after the common size/offset prefix. Stack, yaw and motion retain their raw/base values. | One, at item PC 16 |

The world-only overload retains the earlier base AABB computed with width
`0.6f`, height `1.8f` and zero offset. Its later size/offset stores do not update
that box. The full overload updates the same box after changing size/offset.

The private default Random precedes every ItemEntity Math draw. Those Math
draws use the same live application profile's shared Math owner. A cold Math
holder constructs its own separate default owner; a ready holder keeps its
continuation. No world/provider stream is copied, reseeded or substituted.

NULL world and stack are accepted. A nonnull world must borrow the identical
profile. Keep it live until the counted world binding is acquired immediately
before its base PC 205 publication. Keep a supplied stack live until constructor
return; the constructor retains the same identity immediately before item PC 61.
It accepts any live stack phase without inspecting count, ID, damage, animation
or NBT. No constructor world query, insertion, collision or Gaussian draw occurs.

Retain/release, snapshots and native-byte queries support RAW, READY and failed
instances. The final release retires published children and releases any stored
world/stack holds. `mc_item_entity_stack_get` acquires an additional stack hold
before returning it. Class free returns BUSY while any entity or native class
binding remains. [World class bindings](../world/fluids.md#world-dependencies-and-ownership)
retain the exact side/profile owner before the first World allocation without
initializing classes or resetting IDs/Random. Binding holds release separately
from Entity instance holds.
`mc_entity_release(NULL)` and `mc_entity_classes_free(NULL)` are harmless.
Snapshot child pointers are borrowed views requiring the corresponding live
owner and exclusive access; they expire on mutation or final release.

## Failure and resource contract

Native argument/profile-binding rejection preserves RAW. Once construction has
entered, failure leaves FAILED_CONSTRUCTION with the completed field, class,
ID and entropy prefix; it cannot be retried. It retains successful world/stack
holds until final release. A profile failure cannot roll back a committed
uniquifier or clock sample.

The AABB pool and watcher type-map class initializers have independent failure
latches. A failed watcher initializer retains its already published map and
completed ordered entries. A later use returns `MC_ENTITY_CLASS_FAILED_STATUS`
without retry. Ordinary instance allocation/registration failure does not poison
a class that reached READY. Unpublished native children are freed on failure.
These are explicit native resource policies, not JVM exception/heap emulation.

`mc_entity_error` carries the Entity status, static operation name and bytecode
PC, with the exact nested profile/item/world status where applicable. PCs are
local to the named operation. Snapshot `base_bytecode` and `item_bytecode` mark
the last completed constructor store/call. LIMIT, ALLOCATION_FAILED, dependency
failure and invalid argument/phase remain distinct. Ordinary output arguments
are unchanged on failure; diagnostics and retained constructor prefixes are
the documented exceptions. A null diagnostic pointer is permitted.

Limits default to 64 MiB and `SIZE_MAX` live entities. The allocation ledger
charges each actual direct class/entity/pool/bounds/Random/watcher/map/entry/
record request before malloc, using native sizes. It excludes external
world/stack/profile storage, allocator overhead and C stack. Results, errors,
limits and owner/dependency/input-view storage must be mutually disjoint.
Stale-pointer detection, cancellation and a general JVM class-init engine are
outside this API.

## Arithmetic and current identity

Use the strict x86-64/SSE2 FP environment documented by
[`java.h`](../../include/mc/core/java/java.h): nearest-even rounding, gradual
underflow, disabled traps, no FTZ/DAZ, excess precision, reassociation or FMA
contraction. This environment must remain stable through arithmetic. Position
mutation rejects unsupported MXCSR before changing fields; no coordinate or
finite-value clamp is added.

For the successive Math returns `R1` through `R4`, constructor formulas preserve
these narrowing points and associations:

```text
item.d     = float((R1 * 3.141592653589793) * 2.0)
base.yaw   = float(R2 * 360.0)
velocity_x = double(float(R3 * 0.20000000298023224 - 0.10000000149011612))
velocity_y = 0.20000000298023224
velocity_z = double(float(R4 * 0.20000000298023224 - 0.10000000149011612))
```

Position stores current XYZ and updates the same AABB. Half-width is computed
as float `width/2.0f`, then widened for X/Z subtraction/addition. MinY is
`(y - double(offset)) + double(y_size)`. MaxY recomputes that expression and adds
`double(height)` afterward. `y_size` is client `bo`, server `br`. Size mutation
only stores width/height. Position and velocity changes do not copy previous
positions/angles or invoke dirty/world/RNG callbacks. Each velocity setter
writes one component so a caller can preserve later Gaussian/store order.

Equality/hash require READY nonnull receivers; the equality argument may be
NULL. They read the source's current mutable int32 ID, with equality also
requiring the same loaded-class owner. Hash returns that ID unchanged. Distinct
objects can compare equal after ID wrap; ID is not permanent object identity.

## Field mapping

The base table covers all 66 client and 61 server instance fields. Comma lists
align in order. C paths are relative to `mc_entity_snapshot`; `bounds`, `random`,
`watcher` and `stack` are enclosing child references. Unestablished meanings use
the public C label rather than a guessed gameplay name. `raw` means the default
above remains untouched along these constructor paths; `B` and `I` denote base
and ItemEntity constructor PCs.

| C field / meaning | Client `sn` | Server `lq` | Construction |
| --- | --- | --- | --- |
| `base.id` (int32) | `aD` | `aG` | Old class counter, B14 |
| `base.aE` (double) | `aE` | `aH` | `1.0`, B19 |
| `base.aF` (bool) | `aF` | `aI` | false, B24 |
| `base.aG, base.aH` (Entity refs) | `aG, aH` | `aJ, aK` | raw, distinct refs |
| `base.world` (World ref) | `aI` | `aL` | Supplied nullable world, B205 |
| `base.aJ, base.aK, base.aL` (double) | `aJ, aK, aL` | `aM, aN, aO` | raw |
| `base.x, base.y, base.z` (double) | `aM, aN, aO` | `aP, aQ, aR` | Zero position B212; supplied position I55 in full overload |
| `base.velocity_x, base.velocity_z` (double) | `aP, aR` | `aS, aU` | Full overload formulas, I90/I114 |
| `base.velocity_y` (double) | `aQ` | `aT` | Full overload literal, I97 |
| `base.yaw` (float) | `aS` | `aV` | Full overload formula, I73 |
| `base.aT, base.aU, base.aV` (float) | `aT, aU, aV` | `aW, aX, aY` | raw |
| `bounds` (final AABB ref) | `aW` | `aZ` | Fresh zero box B37; same identity subsequently updated |
| `base.aX, base.ba, base.bb, base.be` (bool) | `aX, ba, bb, be` | `ba, bd, be, bh` | false, B42/B47/B52/B62 |
| `base.aY, base.aZ, base.bc` (bool) | `aY, aZ, bc` | `bb, bc, bf` | raw |
| `base.bd` (bool) | `bd` | `bg` | true, B57 |
| `base.offset` (float) | `bf` | `bi` | +0 B67; height/2 I46 |
| `base.width, base.height` (float) | `bg, bh` | `bj, bk` | `0.6f, 1.8f` B73/B79; `0.25f, 0.25f` I36 |
| `base.bi, base.bj, base.bk` (float) | `bi, bj, bk` | `bl, bm, bn` | +0, B84/B89/B94 |
| `base.private_b` (int32) | private `b` | private `b` | 1, B99 |
| `base.bl, base.bm, base.bn` (double) | `bl, bm, bn` | `bo, bp, bq` | raw |
| `base.y_size` (float) | `bo` | `br` | +0, B104; vertical AABB term |
| `base.bp` (float) | `bp` | `bs` | +0, B109 |
| `base.bq` (bool) | `bq` | `bt` | false, B114 |
| `base.br` (float) | `br` | `bu` | +0, B119; separate from `y_size` |
| `random` (ordinary Random ref) | `bs` | `bv` | Fresh default owner, B130 |
| `base.bt, base.bv, base.by` (int32) | `bt, bv, by` | `bw, by, bB` | 0, B135/B145/B162 |
| `base.bu` (int32) | `bu` | `bx` | 1, B140 |
| `base.bw, base.bz` (int32) | `bw, bz` | `bz, bC` | 300, B152/B169 |
| `base.bx` (bool) | `bx` | `bA` | false, B157 |
| `base.private_c` (bool) | private `c` | private `c` | true, B174 |
| `base.client_only.bA, base.client_only.bB` (String refs) | `bA, bB` | **Absent** | raw, distinct refs |
| `base.bC` (bool) | `bC` | `bD` | false, B179 |
| `watcher` (Watcher ref) | `bD` | `bE` | Fresh watcher, B190 |
| `base.bE` (float) | `bE` | `bF` | +0, B195 |
| `base.private_d, base.private_e` (double) | private `d, e` | private `d, e` | raw |
| `base.bF` (bool) | `bF` | `bG` | false, B200 |
| `base.bG, base.bH, base.bI` (int32) | `bG, bH, bI` | `bH, bI, bJ` | raw |
| `base.client_only.bJ, base.client_only.bK, base.client_only.bL` (int32) | `bJ, bK, bL` | **Absent** | raw |
| `base.bM` (bool) | `bM` | `bK` | raw; server `bK` is not client `bK` int |

The five client-only fields are absent on server, even though the common C
snapshot has storage for them; inspect `source` before interpreting them. These
constructor reference fields remain null unless their individual store occurs.
An allocated but unpublished child is not a stored original reference.

| C field | Client `hl` | Server `ez` | Construction |
| --- | --- | --- | --- |
| `stack` (ItemStack ref) | `a` | `a` | Supplied nullable same identity, full overload I61 |
| `item.e` (int32) | private `e` | private `e` | raw |
| `item.b` (int32) | `b` | `b` | 0, I7 |
| `item.c` (int32) | `c` | `c` | raw |
| `item.f` (int32) | private `f` | private `f` | 5, I12 |
| `item.d` (float) | `d` | `d` | First Math formula, I28 |

The base/private and ItemEntity shadow names are separate storage: base `b` is
int32, base `c` bool and base `d/e` doubles; item `b/c/e/f` are int32 and item `d`
is float. Original AABB, Random and watcher fields are the enclosing snapshot
references shown above, not duplicate members hidden in `base`.

| C child/class state | Client original | Server original | Contract |
| --- | --- | --- | --- |
| Bounds `min_x, min_y, min_z` | `eq.a, b, c` | `cz.a, b, c` | Six original bounds fields, positive zero before position |
| Bounds `max_x, max_y, max_z` | `eq.d, e, f` | `cz.d, e, f` | Same box identity is updated |
| Class `next_id` | `sn.a` | `lq.a` | Shared wrapping int counter |
| Class `bounds_pool_identity`, `bounds_pool_cursor` | `eq.g, h` | `cz.g, h` | Empty list published before cursor 0; this factory does not consume pool entries |
| Class `watcher_types_identity` | `ud.a` | `mo.a` | Published type map; ordered seven-entry initialization |
| Watcher instance map | `ud.b` | `mo.b` | Fresh map; constructor-only snapshot exposes its size/record |
| Watcher snapshot `dirty` | `ud.c` | `mo.c` | false |
| Record snapshot `index` | `ma.b` | `ht.b` | 0 |
| Record snapshot `type` | `ma.a` | `ht.a` | 0 |
| Record snapshot `byte_value` | `ma.c` (Object ref) | `ht.c` (Object ref) | Typed Byte0 projection of shared `Byte.valueOf(0)` |
| Record snapshot `record_dirty` | `ma.d` | `ht.d` | true |

The ordered watcher type associations are Byte, Short, Integer, Float, String,
ItemStack (`iz`/`fy`) and `br`/`ay`, with type values 0 through 6. The exposed
constructor record has stable identity, index 0, type 0 and Byte0; the watcher
dirty flag remains false. This API covers that record's construction and has no
generic watcher mutation, serialization or boxed Object/Class alias interface.
Native identity hashes, cache/CDS bootstrap and full JVM behavior are not
claimed by this typed representation. World insertion, ticking and persistence
remain separate Entity integration work.
