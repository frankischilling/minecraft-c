# Minecart constructor source map

The [Minecart contract](../entity/minecart.md) implements the paired `yl`/`pr`
initializer and two constructors, then admits genuine instances to the existing
World/Chunk publication path. Native construction/publication/detector contracts
pass on MSVC, GCC and Clang. This page records the paired source and native
validation scope. Original Minecart runtime observations and equality remain
unverified.

## Primary methods

The archives are identified by the [binary audit](binaries.md) and its
[manifest](manifest.json). Client `yl` extends `sn` and implements `lw`; server
`pr` extends `lq` and implements `hp`. The inventory interfaces have abstract
declarations and no class initializer. Minecart remains canonical selector22
and EntityList registration `Minecart`/40; no registration or ancestry change is
needed for construction.

The complete client disassembly is 9,770,684 bytes, SHA-256
`15acc525e12512ba96782b57ead3940bf4cc608ad3dae3b9fca4e5acf96189bd`.
The server disassembly is 4,826,184 bytes, SHA-256
`020035f5590871cb3851e460f60013156fd6cb69c0e46bbee9454ebd33851f7e`.
LF ranges are one-based physical records, inclusive, retaining the original CR
bytes and final LF. These source identities are not runtime results.

| Body | Client LF / bytes / SHA-256 | Server LF / bytes / SHA-256 |
| --- | --- | --- |
| Declaration and own fields | 286057-286114 / 839 / `881739cef01113030eaedf3023a17278da416f359116fd6b85a1970a2f167d99` | 139203-139251 / 713 / `39d8e12c85a0539f209336add3a310aa8a13d65be8de739efb0f6fe6f3b16c86` |
| World constructor | 286115-286151 / 1232 / `cb9017e26c36efd8f6706708254c9bfe5ea0417a0a68cdefb4a1a941d92d815e` | 139252-139288 / 1232 / `6585a743bb80135062681b35a4fe6f9c7e4cec7fcb090f189a1dc2d20c23b2c3` |
| Positioned constructor | 286182-286219 / 1187 / `30575251e033d9e8fabbec94dacc31688c3cb0adebdc2d92e04b6bb622dff42d` | 139319-139356 / 1187 / `27ff53266ad787b4c7af1161ad2c4387eb5de125050de7cafdae697769b081ff` |
| Own class initializer | 289274-289670 / 8105 / `4d34d1d02500c870cd93042d3a1cb3aeaa514e171c54544d4b82a7361a619d2e` | 142116-142512 / 8105 / `4635da2729f27ffef83494762281a7e680af740f0aaa5956dce0f9069abe9709` |
| Empty watcher hook | 286158-286162 / 75 / `78ab5703ab4c5579b9fa1b952d025dd1caa11442648fedb296b5d2fc3654727a` | 139295-139299 / 75 / `78ab5703ab4c5579b9fa1b952d025dd1caa11442648fedb296b5d2fc3654727a` |
| Inherited size | 235811-235821 / 283 / `3811a197336423f06db100c855b7e2558b0eefaffb815f8a69d7ad8c3cd15a3e` | 102040-102050 / 283 / `f74b25ea9d2d457f548a5cc8e32357a6ecfd19583aceff2cc2452a8e1e7be750` |
| Inherited position | 235837-235899 / 1667 / `3f76631ad78a8124feb249e54784602a679f759a01c7728ec3462052148c296d` | 102066-102128 / 1667 / `9425cfd80c04a4560159286e3743a29b0af09f236ed4f68f6ee4612345452b60` |
| Detector private update | 207268-207406 / 3960 / `e01c89b1478c88ce42dd7f8e3988ccefccda5a02f15283aba05575f7baf127df` | 87552-87690 / 3960 / `31fca32e3da5737659e1c552a9f5cb55074c9fdb13a574824fcdeddc2380d67f` |

## Reached constructor work

The shared base constructor creates the ID, bounds, private ordinary Random and
watcher, publishes the nullable World at205, positions at zero at212, registers
index0/Byte0 at224 and calls the actual empty `yl.b()`/`pr.b()` hook at228.
There is no Minecart `a()V` watcher hook; the inventory `a()` has return type int.
The existing [base/Item evidence](entity-construction.md) describes those shared
children and keeps its separate Item observations and formula replay scope.

| Own body | Reached order |
| --- | --- |
| World constructor | Base call2; allocate 36 Stack references8 and store h11; a/b/c/i stores16/21/26/31; inherited flag36; size44; binary32 height/2 offset54; return57. |
| Positioned constructor | Delegate2; position17 at x/y+promoted actual offset/z; zero velocities22/27/32; store supplied previous XYZ37/43/49; type55; return58. |
| Class initializer | Entity superclass active use first; 31 distinct array allocations and all literal stores; publish j439; return442. |

Neither constructor calls an inventory operation, constructs a Stack, draws
Item Math entropy, looks up a Block, inserts itself or invokes a Minecart tick.
The World-only size call does not recompute the earlier base bounds. The
positioned overload mutates that same identity, preserving binary32 division
before promotion and the actual binary64 Y addition. No type enum or finite
coordinate guard is inserted.

Own fields are separate from inherited shadowed names. Paths below are relative
to `mc_minecart_entity_snapshot.fields`; `h` is its actual borrowed array owner.

| C field | Both original classes | Constructor effect |
| --- | --- | --- |
| `h` | Private `h:Stack[]` | Real length36 array, all entries null, store11. |
| `a, b, c` | Int `a, b, c` | 0, 0, 1 at16/21/26. |
| `private_i` | Private boolean `i` | false at31. |
| `d` | Int `d` | World-only default0; positioned supplied signed type at55. |
| `e, private_k` | Int `e`, private int `k` | Raw zero. |
| `f, g` | Double `f, g` | Raw positive zero. |
| `private_l, private_m, private_n, private_o, private_p` | Private doubles `l, m, n, o, p` | Raw positive zero. |
| `client_only.private_q`, `client_only.private_r`, `client_only.private_s` | Client private doubles `q, r, s`; absent on server | Raw positive zero on client; no server field claim. |

The source inventory-size method returns27, independently of the constructor's
36-element array. That method and indexed inventory operations are not supplied
by the constructor API. The class graph's literal endpoints are:

| Row | Endpoint0 | Endpoint1 |
| --- | --- | --- |
| 0 | (0, 0, -1) | (0, 0, 1) |
| 1 | (-1, 0, 0) | (1, 0, 0) |
| 2 | (-1, -1, 0) | (1, 0, 0) |
| 3 | (-1, 0, 0) | (1, -1, 0) |
| 4 | (0, 0, -1) | (0, -1, 1) |
| 5 | (0, -1, -1) | (0, 0, 1) |
| 6 | (0, 0, 1) | (1, 0, 0) |
| 7 | (0, 0, 1) | (-1, 0, 0) |
| 8 | (0, 0, -1) | (-1, 0, 0) |
| 9 | (0, 0, -1) | (1, 0, 0) |

The [class implementation](../../src/entity/minecart/classes.c) creates the
actual graph in source order; the table does not replace its allocations.
The [instance implementation](../../src/entity/minecart/entity.c) reuses the
shared base kernel and owns h separately. Native requested-byte accounting and
failed-class latches are explicit policies, not JVM OOM or Throwable emulation.

## Publication and verification boundary

The only publication admission extension is the canonical Minecart kind in the
existing World wrapper. It retains exact EntityClasses ownership and existing
call/hold/source order. Normal Chunk insertion and Class gathering remain
generic, with no Minecart-specific fake bounds, empty result or callback.
Each duplicate occurrence holds the same real Entity; a fresh query result also
keeps a World lease. A foreign loaded Class is a nonmatch despite equal names.

The detector helper captures its defining Minecart Class23 before pooled
bounds64/query67 and reads the actual captured List size74. Transitions use the
saved metadata at105/166, followed by current/below notifications117/131 or
178/192 and observer range143/204. A nonempty result reaches the live receiver
ID218, delay222 and real schedule225. Contact and tick use their own saved-bit
guards; neither publishes the ignored contact Entity.

The notifying metadata setter also runs its own ordinary notification path.
A failure in that nested call remains at105/166, before the detector's explicit
neighbor calls. Earlier metadata writes and the complete returned child survive.

The separate [constructor test](../../tests/entity/minecart/test_minecart.c)
and [World detector test](../../tests/world/runtime/rails/test_minecart_detector.c)
pass their finite native contracts. The latter uses actual
positioned Minecarts and two GAME Worlds with four stored/acquired normal
Chunks each, covering the real deferred radius-eight presence span. It checks
Class identity/nonintersection, duplicate holds and result leases, positive
metadata/scheduling, deactivation and earlier writes/F insertion retained on
later failure. The [World driver](../../tests/tools/world/test_runtime.py)
binds source inputs, execution and the exact finite fixture layout.

## Native validation

Complete D suites pass every configured test with zero failures, errors or
skips. Each host passes 7,270 constructor checks across 164 cases, 196 detector
checks across 24 literal cases and the 575-check Piston regression.

| Host | Compiler | CTest cases | Test-command seconds |
| --- | --- | --- | --- |
| Windows | MSVC 19.51 | 177 | 315.774949 |
| Linux | GCC 13.3 | 179 | 233.894699 |
| Linux | Clang 18.1.3 | 179 | 232.182826 |

Controller mirrors and lifecycle interval checks return zero on all three
hosts. The Linux profiles enable ASan, UBSan and leak checks. A separate
focused Windows run also passes its single detector test. Actual terminal
observations were retained before controls-first audits checked child waits,
closed sinks, stream seals and configured/executed JUnit inventories. Earlier
A/B/C runs remain recorded as failed; these passing results apply to D.

Validation used the sealed 993-file D source tree with 26 overlay paths.
The source-map record is 162,364 bytes, SHA-256
`568d3e77e2c1b8ced013a5c9d9633325001a836664f339abc504ffd463def3ea`.
A committed-source binding and fresh hosted CI result for the Minecart
increment are pending.

Nested Piston and rail paths initialize private error records in place.
The public by-value APIs and full child/first-cause diagnostics remain
unchanged. The rail constant zero template uses an explicit initializer
accepted by strict MSVC. These changes reduce transient stack use without
raising the stack limit.

The previous [rail results](rails.md#primary-evidence-and-validation-boundary)
predate this constructor increment. Item constructor observations, finite
power comparisons and the passive primary method hashes do not qualify
Minecart runtime behavior. Inventory contents/operations, own ticks, rail
motion, interaction/damage/death, NBT/factory/provenance, concrete movement and
collision virtuals, queue draining and complete gameplay remain unfinished.
No original Minecart execution or original/native constructor, detection or
rail equivalence is claimed.
