# Minecart inventory and NBT source map

This page maps the seven inventory operations and semantic NBT work for client
`yl` and server `pr`. The [API contract](../entity/minecart-inventory.md) describes
the native interface. Player usability, interactions, ticks, movement and
physics remain outside this increment.

## Retained authority

The [archive identities](minecart-construction.md#primary-methods) remain the
constructor reference's identities. The retained bytecode authorities are:

| Side | File bytes | SHA256 |
| --- | ---: | --- |
| Client javap | 9,770,684 | `15acc525e12512ba96782b57ead3940bf4cc608ad3dae3b9fca4e5acf96189bd` |
| Server javap | 4,826,184 | `020035f5590871cb3851e460f60013156fd6cb69c0e46bbee9454ebd33851f7e` |

`source-witnesses.json`, SHA256
`b15c42da5e2d41d2f3229cb71370051055239cc881e379693c7b4e1e2a0b1be2`,
indexes 57 complete method pairs. Its ranges use one-based physical LF lines,
retaining original CR bytes and actual LF terminators. Byte offsets are
zero-based, half-open. Each slice starts at the method declaration and ends
before the next method or class close. Instruction-shape agreement alone does
not prove field, class or callee equivalence; both sides' operands are required.

## Inventory methods

Client `yl` implements `lw`; server `pr` implements `hp`. `iz`/`fy` are Stack
classes. The seven concrete bodies occupy client LF288998-289100 and server
LF141902-142004.

| Operation | Client descriptor | Server descriptor | Reached work |
| --- | --- | --- | --- |
| Size | `yl.a()I` | `pr.a()I` | Return27 at2. |
| Get | `yl.f_(I)Liz;` | `pr.d_(I)Lfy;` | Array load5, return6. |
| Split | `yl.a(II)Liz;` | `pr.a(II)Lfy;` | Empty load5; count15; whole clear35; partial call45; count55; conditional clear67. |
| Set | `yl.a(ILiz;)V` | `pr.a(ILfy;)V` | Identity store6; count12; limit calls16/24; clamp store27. |
| Name | `yl.c()Ljava/lang/String;` | `pr.c()Ljava/lang/String;` | Literal Minecart, return2. |
| Limit | `yl.d()I` | `pr.d()I` | Return64 at2. |
| Changed | `yl.y_()V` | `pr.i()V` | Return0. |

Reported27 does not bound constructor h36. Indexed methods use the current
array, including its null/index failure. No Type1, World or Player check enters
these bodies. Set publishes the same identity before its greater-than64 clamp.

Partial split reaches `iz.a(I)Liz;` / `fy.a(I)Lfy;`. That descendant stores the
wrapped subtraction at7, allocates at10, then reads ID15 and damage20 before
numeric construction23. Zero and negative amounts retain their signed behavior.
The [native implementation](../../src/entity/minecart/inventory.c) adds occurrence
holds and whole-result transfer. Retain refusal leaves the slot unchanged;
allocation refusal after subtraction retains the decrement. These are native
lifetime/failure rules, without a JVM OOME timing claim. Private count/split
kernels accept every live phase; construction still requires RAW, and public
READY wrappers retain their admission.

## NBT calls and prefixes

| Work | Client | Server |
| --- | --- | --- |
| Own read | `yl.a(Lnu;)V` | `pr.a(Liq;)V` |
| Own write | `yl.b(Lnu;)V` | `pr.b(Liq;)V` |
| Base read | `sn.e(Lnu;)V` | `lq.e(Liq;)V` |
| Base write | `sn.d(Lnu;)V` | `lq.d(Liq;)V` |
| Boolean save | `sn.c(Lnu;)Z` | `lq.c(Liq;)Z` |
| Named factory | `jc.a(Lnu;Lfd;)Lsn;` | `ga.a(Liq;Ldj;)Llq;` |

Own read is client LF288579-288655/server LF141489-141565; own write is
LF288503-288578/LF141413-141488. Compound `nu`/`iq` and List `sp`/`lr` getters
are included in the witness map.

Base read completes its field prefixes, position294 and rotation306 before
dispatching own read311. Own Type getter4 precedes store7. Type2 reads/stores
PushX22/25, PushZ32/35 and signed Short Fuel42/45. Types other than1 or2 leave
inventory and push/fuel fields untouched.

Type1 gets Items62 before new array71 and h publication74. Missing Items still
produces an empty h27; a wrong Items type fails before replacement. Iteration
uses size81, element89, cast92 and Slot getter101. A null element passes the cast
and fails at that getter. Slot defaults0 and is masked with255. Out-of-range
records skip Stack construction. Valid records capture h126 before new131,
constructor137 and store140. Later duplicates replace earlier values. A late
failure retains the new h and completed slots; the native reader keeps detached
old h owners alive through every borrowed input read.

Stack NBT construction calls its reader at11. Short id getter4, Byte Count14
and Short Damage24 have separate store prefixes. The
[NBT diagnostics](../../include/mc/entity/nbt/errors.h) retain own137, constructor11
and the selected lower getter, with list occurrence separate from decoded slot.
Native wrong-Damage refusal is INVALID_FIELD/Damage and leaves a genuinely
failed Stack's earlier ID/count stores visible. The Minecart reader releases
that unsuccessful new Stack's local hold without installing it in h.

Own write stores Type7. Type2 writes PushX25, PushZ35 and narrowed Fuel46.
Type1 creates List60/64, traverses actual h length75 and nonnull entries84,
creates a fresh Compound88/92, writes Byte Slot103, calls Stack write114 and
appends121; only then does Items134 install the list. Stack write emits Short
id8, Byte Count19 and Short Damage30. It carries no unknown-field overlay.
Base write emits seven fields before own dispatch145 and return148.

Boolean save resolves the name at1 before the dead decision, writes id22 and
calls base write27. Normal false skips serialization. The factory resolves id8
and the name map11; its reflection catch covers2 through51, including
construction45. NBT read66 follows that catch boundary. Factory construction
and semantic read do not publish World or Chunk membership.

## Native ownership and storage

The [compact inventory context](../../include/mc/entity/minecart/inventory_errors.h)
keeps explicit selected/entered sites and the complete selected Stack cause.
PC0 alone is not a selection flag. NBT reuses its existing full Stack and
construction children rather than embedding another full Entity error.

Semantic output uses independent trees. Its peak includes the retained own
tree while copying base-plus-own, then the Boolean leaf while adding id. Outputs
remain unchanged on native failure; this does not undo source read prefixes.

[Optional storage](../../include/mc/entity/nbt/storage.h) follows an actual
successful read. It archives a complete root and attaches last-winning records
to current nonnull slot occurrences. READY alone cannot prove read history.
Generations protect distinct root/h trees and aliased Stacks' distinct metadata.
Set/self-set/take clear occurrence metadata; partial split keeps the source
record. Save emits current generated slots, filters inactive own fields and
never uses Stack.source as fallback. Unknown-metadata/order preservation is
native policy, separate from Java's fresh Compound saves and byte/order equality.

Budgeting counts direct Entity/h bytes, distinct Stack/source allocations,
sidecar storage and distinct generation/tree allocations once. Preservation
keeps old owners live through new generation, tree and sidecar preparation.
Saving also charges overlapping semantic trees, pointer scratch and final copy.
Borrowed input, allocator overhead and bounded C stack are excluded from this
one-Entity requested-byte ceiling.

## Native validation

The sealed B source passed complete local validation on three hosts. Configured
and executed CTest inventories matched, with these results:

| Host/compiler | CTest tests | Failures/errors/skips |
| --- | ---: | --- |
| Windows MSVC 19.51.36260.0 | 180 | 0/0/0 |
| GCC 13.3.0 | 182 | 0/0/0 |
| Clang 18.1.3 | 182 | 0/0/0 |

The total is 544 individual CTest executions. All hosts used strict C17 warning
and floating-point profiles. Linux enabled ASan/UBSan and leak detection. Each
host passed the controller mirrors and lifecycle intervals, including 556
positive and 2,011 negative interval controls with zero failures.

The focused contract totals below are per host:

| Fixture | Native checks | Source cases |
| --- | ---: | ---: |
| [Inventory](../../tests/entity/minecart/test_inventory.c) | 598 | 40 |
| [Semantic NBT](../../tests/entity/minecart/test_nbt.c) | 558 | 40 |
| [Storage](../../tests/entity/minecart/test_storage.c) | 335 | 14 |
| Constructor | 7,270 | 164 |
| Detector | 208 | 24 |

Each storage driver also passed 314 independent disk checks over six region
files and eight level/session owners. The full GAME loader/save/reload route
retained current winning records and unknown metadata. The separately labelled
staged failure used a public preservation ceiling after real factory/Chunk
adoption; it did not inject a full-loader failure.

The B source binding is 339,567 bytes, SHA256
`890aee7031327fbe4c1feaa6db09a5e7cdd3292037af78f963cf427626c46f09`.
It records 1,009 tested source files over the 995-file public parent
`3fb08811713b9eb8ccb71045ae4bd327b9219a4c` and 13 frozen input trees. Closed
audits rejoined source maps, strict compilation, JUnit, driver results and full
streams; the admitted sources stayed stable.

The historical
[constructor comparison at39edf12](minecart-construction.md#finite-constructor-comparison)
retains its null-World constructor inputs and source binding. It supplies no
inventory/NBT/storage equality or whole-game result.

### Storage interpreter portability probe

CMake D resolves only the storage test's Python executable through REALPATH.
A separate GCC configure requested the actual `/usr/bin/python3` symlink;
the registered CTest command and execution used `/usr/bin/python3.12`. That one
storage test passed 335 native checks, 14 source cases and 314 disk checks.
All 354 generated C compile commands and 182 configured test names/properties
matched B after literal root replacement. Production C/H, fixtures and driver
bytes remained identical to B; the 1,009 source files, 995 public-parent files
and 14 frozen input trees stayed stable.

This focused probe covers the interpreter change. The three-host table above
is the full B run. A final committed-source binding and hosted CI result for
this inventory/NBT/storage increment remain pending.
