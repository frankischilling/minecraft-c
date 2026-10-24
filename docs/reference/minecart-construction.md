# Minecart constructor source map

The [Minecart contract](../entity/minecart.md) implements the paired `yl`/`pr`
initializer and two constructors, then admits genuine instances to the existing
World/Chunk publication path. Native construction/publication/detector contracts
pass on MSVC, GCC and Clang. This page records the paired source and native
validation scope. A separate original/Windows MSVC comparison at `39edf12`
matches the complete declared projections for all ten literal null-World
constructor inputs per client/server side, with zero mismatches. Its finite
scope is recorded below.

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
178/192 and WorldAccess range143/204. A nonempty result reaches the live receiver
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

The local D checkpoint passes every configured test with zero failures, errors
or skips. Each host passes 7,270 constructor checks across 164 cases, 196 detector
checks across 24 literal cases and the 575-check Piston regression. These local
results predate the later committed portability checkpoint below.

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
The first Minecart commit, `e7c6d31a856fd69d202d6ad1036b8e1344c49285`,
published 995 files, matching D's native inputs alongside five changed and two
new documentation pages. Its [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/39799946459)
failed the Windows MSVC 19.44 build on C4200 for flexible-array declarations;
Windows tests, mirrors and intervals were skipped. Both 179-test Linux jobs
and 527-file formatting passed. That failure remains a failed checkpoint.

### Committed and hosted checkpoint

Commit `39edf12ae768557cde18531010186386cd0f2b41`, tree
`8c929fd92d1313ae82d76344118b44d6f92daa7c`, adds only scoped `_MSC_VER`
warning push/disable4200/pop guards around four existing flexible-array structs
in [class storage](../../src/entity/minecart/classes_internal.h) and
[instance storage](../../src/entity/minecart/entity_internal.h). Declarations,
layout, allocation expressions and runtime C tokens are unchanged. Warnings
outside those declarations retain their previous state.

The committed 995-file binding is 247,382 bytes, SHA-256
`c2fdae0d0fa73369aa6aac4520b377c8b7d2da4c5f607ea757574bbb49a6240d`.
Every actual raw committed file blob matches the admitted source bytes.
A fresh local MSVC 19.51 check passes exactly the constructor and detector
tests: 164 cases/7,270 checks and 24 literal cases/196 checks. This focused
result is not a new full local-suite run.

The [complete hosted run at `39edf12`](https://github.com/frankischilling/minecraft-c/actions/runs/39805310937)
passes every configured test:

| Host | Compiler | Passed tests |
| --- | --- | --- |
| Windows | MSVC 19.44.35229.0 | 177 |
| Linux | GCC 13.3 | 179 |
| Linux | Clang 18.1.3 | 179 |

All 535 individual tests pass with zero failures, errors or skips. Formatting
passes for 527 C/header files. Each native job also passes controller mirrors
and lifecycle intervals, including 556 positive and 2,011 negative interval
cases. The Linux jobs retain the sanitizer profile above.

The hosted checkout used synthetic merge
`b97992ae0fb3a2d01b8fc8a09068fc5dd8abede1`; its tree exactly matches
`39edf12`, with base `a22d25c0` and the Minecart head as parents. The closed
hosted audit summary is 2,018 bytes, SHA-256
`1eabd4b0778ce88811463014ee662236ece078b55657704172bd1e60b010f29c`.
The older bindings and failed runs are preserved. These hashes identify the
recorded checkpoint; they do not extend the native scenario or original
observation scope.

Nested Piston and rail paths initialize private error records in place.
The public by-value APIs and full child/first-cause diagnostics remain
unchanged. The rail constant zero template uses an explicit initializer
accepted by strict MSVC. These changes reduce transient stack use without
raising the stack limit.

The previous [rail results](rails.md#primary-evidence-and-validation-boundary)
predate this constructor increment. Item constructor observations and finite
power comparisons retain their own inputs and source bindings.

## Constructor observations and qualification

Original and native captures have each completed ten literal constructor
inputs per client/server side. Every input supplies a null World. The native
capture uses the exact committed `39edf12` product binding above. Both offline
qualification processes returned zero with `qualified_full10`; all 22 protocol
and ownership rules passed on both sides.

The execution audits rejoined each qualifier's ten before/after seal slots,
complete source and output maps, and the closed report pin. Both execution
records reached `known_closed_qualified_candidate`. Qualification used source
tree SHA-256
`2f2661b36ef81825944518e177bd000ce033b639ed761191a6f6a40460f0f3bf`.
The retained report identities are:

| Engine | Qualification report bytes | SHA-256 |
| --- | --- | --- |
| Original | 643,196 | `28661a4175d03e888c945207b8419692cea17a8782fd19ec5ec5ddfd6e18e874` |
| Native | 4,356,054 | `0851ce3ade788cddbf5ca405f8a69df5ad5ed0b5afe95884ff8e1bb13692a314` |

The records retain the 84-field source inventory with explicit types and field
absence, the actual 36-reference `h` array, all 31 distinct `j` arrays and their
60 observed integer values. They also retain raw Entity IDs and the counter,
Gaussian cache bits and the declared bounded identity-role partition. No
expected constructor result or formula replaces those observations.

### Finite constructor comparison

The original and Windows MSVC projections match for all ten literal inputs on
each client/server side at `39edf12`. Each engine supplies ten complete
snapshots per side. The comparison returned `finite_projection_equal` with
zero mismatches, using the complete typed declared projections described
above. All 60 observed class integers and Gaussian cache bits are included.
Entity numeric IDs and counters are compared raw, without rebasing. No
expected-result oracle, formula, tolerance or entropy normalization replaces
the observed values.

The bounded role projections contain 890 client and 870 server role
occurrences per engine, all compared unchanged. These are role occurrences,
not counts of distinct objects. Absolute object labels and complete
cross-engine JDK object graphs remain outside the projection.

| Observation | Reference environment |
| --- | --- |
| Original client and server | Temurin 17.0.17+10, interpreted mode (`-Xint`) |
| Native client and server profiles | Windows x64, Microsoft C/C++ 19.51.36260; VC toolset directory 14.51.36231 |

This native comparison is separate from the hosted MSVC 19.44.35229.0,
GNU 13.3 and Clang 18.1.3 suites above. It establishes no original-runtime
constructor equality for the GCC or Clang builds.

The actual comparison returned zero after one launch and four empty polls of
the same tool handle. Complete tool objects were saved before output reads.
All 20,340 consumed-file and 51 tree before/after maps were stable, with no
errors or drift. A separate audit rejoined the five-step execution history,
seven external inputs, all 43 comparison-source files and fourteen source
package maps. It checked the entire three-file output map, the four admitted
projections against their qualified input objects and strict client/server
canonical equality. At that audit, all 995 source files still matched the
committed binding recorded above.

| Comparison output | Bytes | SHA-256 |
| --- | --- | --- |
| `report.json` | 16,029,709 | `dfca9761c57e6c688524d08c890ba9d56cb2d2b11b254bd227e5cb5127991331` |
| `admitted-projections.json` | 1,292,597 | `98f19922aeeb1c0923a9374afb5bdf45eefe3b4a1d600c207cd3aa7d3eff4659` |
| `mismatches.jsonl` | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |

The comparison source tree SHA-256 is
`59b983821972de8f1ac353778dd1ebc323cb3ff8dd06c6d579d503d411c0d703`.
Its entry is 27,677 bytes, SHA-256
`12ddf2513ad9863ae36c08b5f8165ad58df27d440f10a5a6fef06e57feb253a7`.
The actual execution receipt is 7,903 bytes, SHA-256
`ff7a5b7d51d7294ed6a7702bc63c5ab206ea67290d9b2ec6e2d79aa3fa408abf`.
The independent audit is 4,548 bytes, SHA-256
`b45c43a23f6e190f7c5d06d65207b6b36aa2f8d405aa0543caf989b23f223432`.

Separate tooling preflights pass six CRC-adapter checks, 38 comparator checks
and 67 projection controls: 59 invalid shapes refused and eight valid shapes
accepted. The earlier 27 pure-helper checks remain unchanged. These checks
exercise the observation and comparison tools, not additional gameplay
scenarios.

## Remaining scope

The constructor captures contain no World publication, Class-query, detector,
movement, NBT or rail call. Native publication and positive detection retain
the separate contract results above. Inventory contents/operations, own ticks,
rail motion, interaction/damage/death, NBT/factory/provenance, concrete movement
and collision virtuals, queue draining and complete gameplay remain unfinished.
Original/native detection, rail physics and whole-game equality remain
unverified.
