# Arrow NBT and storage

The client Arrow now joins its complete `SL.a(NU)` reader and `SL.b(NU)` writer
to the existing SN parent bodies, Boolean save wrapper, canonical factory, and
native provenance overlay. The factory uses the genuine World-only Arrow
constructor before the complete semantic read. It does not infer a shooter
from saved tags.

Original capture/replay and both-host native qualification for this increment
are pending. The implementation and authored controls described here are not a
loaded encounter, Arrow flight, full-frame, or performance qualification.

The public operations remain [nbt.h](../../include/mc/entity/nbt/nbt.h),
[factory.h](../../include/mc/entity/nbt/factory.h), and
[storage.h](../../include/mc/entity/nbt/storage.h). The complete own hooks are
in [arrow/nbt.c](../../src/entity/arrow/nbt.c); they use actual Arrow storage
without a new constructor-phase, Math, World, or shooter gate.

## Original source map

The unchanged client JAR has SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Its `sl.class` has SHA-256
`2ba2e27fbc558a78b87c26316968cd5e9595656b96c861b2975cae79201d3a45`.
`SL.a(NU)` has 102 Code bytes, return101, and Code SHA-256
`a5e21097040c2511f3c6cc7e3e91415088ef67ee4fc23c1c5c01ea32c61098f0`.
`SL.b(NU)` has 96 Code bytes, return95, and Code SHA-256
`992d99932a9791300a0f85036b6afcc599d32abae2f978fc4807c2494ff88552`.
Neither own hook has an exception handler.

| Key | Read: getter, conversion, immediate store | Write: value, narrowing, setter |
| --- | --- | --- |
| xTile | Short4, signed widen, d7 | d, signed Short, call8 |
| yTile | Short14, signed widen, e17 | e, signed Short, call19 |
| zTile | Short24, signed widen, f27 | f, signed Short, call30 |
| inTile | Byte34, mask255, g41 | g, signed Byte, call41 |
| inData | Byte48, mask255, h55 | h, signed Byte, call52 |
| shake | Byte62, mask255, b69 | b, signed Byte, call63 |
| inGround | Byte76, equal to1, i88 | i, Byte0/1, call82 |
| player | Boolean95, nonzero Byte, a98 | a, Boolean-to-Byte0/1 helper92 |

Missing scalar keys supply zero. Other numeric tag classes are not coerced:
a wrong present tag fails its actual NU checkcast25. Earlier stores survive
the failed getter, and that field and all later fields remain unchanged.
`inData` is an unsigned byte value, with no metadata-nibble clamp. Byte2 and
Byte-1 make `inGround` false and `player` true.

Neither own hook reads or changes shooter c, its separate counted occurrence,
grounded age j, or airborne age k. The parent read also leaves them intact.
No Random draw, shooter lookup, reference replacement, inventory operation,
or constructor is introduced by NBT. The persisted `player` flag does not
change the canonical Arrow Class or establish a shooter identity.

## Parent and factory ordering

The existing SN reader captures Pos, Motion, and Rotation Lists before any
scalar store. It stores all three motion values before the magnitude clamps,
then position/current-history values, current/old angles, FallDistance, Fire,
Air, and OnGround. It rebuilds the same permanent bounds at294 and normalizes
only current angles at306 before invoking SL at311. A late own-tag failure
therefore retains the complete reached parent prefix. Wrong initial named
Lists, absent Lists, wrong vector elements, and null vector elements retain
their existing distinct failure sites.

The full SN writer emits its seven base fields before calling SL at145.
The independent semantic result has **15 keys**. The separate Boolean wrapper
resolves the actual class name before its dead/name test, writes id, then calls
the full writer at27. A true Arrow save has **16 keys**. False changes only the
Boolean output; failed independent builds preserve both caller outputs. The
wrapper's actual pointer capacity is16, including id.

The factory accepts only the genuine canonical client Arrow layout. It runs
`mc_arrow_allocate`, the complete World-only constructor, then the normal
SN/SL reader. NULL World is supported; no Math or shooter dependency is added.
The fresh constructor supplies c=null and j/k=0, whereas reading an existing
Arrow preserves its current values. Constructor ID and default-Random effects
remain actual independent effects.

JC's reflection catch covers id/constructor work at[2,52). The semantic reader
call66 is outside it. A malformed Arrow tag after construction propagates its
reader failure at66; it does not become an unknown-id skip or a constructor
retry. Existing reached stack-printer limitations remain explicit. The factory
transfers one Entity occurrence on success and balances only its local
occurrence on failure. Unsupported descendants retain their reached refusal.

NBT errors keep the enclosing wrapper/base/factory frame independent of the
selected SL site and deepest NU child. `ARROW_CALL` preserves the complete
constructor receipt by value, including independent heading/Math/position
causes where present. It owns no object and grants no replay rights. Switching
back to an ordinary late-hook arm restores the current Entity cause and fresh
Stack/codec/EntityList causes. No Arrow NBT operation creates pending work or
consumes a shooter's pending owner.

## Provenance and real Chunk ownership

The existing GAME loader performs the complete factory read, sets Chunk q,
adopts the nonnull Entity into an actual Chunk slice, and then attaches native
provenance. Its local occurrence spans adoption and attachment. A later
provenance or stage-journal failure retains the reached real Chunk membership
and source fields; cleanup uses the retained stage and normal terminal owner
protocol rather than rolling the import back.

Arrow provenance copies the entire immutable input root without an Item getter
or Stack dependency. Old input may be borrowed from the previous source through
the final copy. Attachment replaces only after the independent copy succeeds.
Source-mode World saving uses fresh semantic records and omits unknown input
fields. The separate legacy native archive mode overlays all16 generated
id/base/own names onto the retained root. Unknown names such as Item, Shooter,
Owner, j, k, or Age remain opaque tags and never become references or ages.

Overlay matching uses exact UTF16 names. It preserves retained positions,
unknown children, root names, and raw numeric payloads; absent generated names
append in generated order. This is native preservation, not original HashMap
order or compressed-byte equivalence. Normal codec writing retains its existing
Java-stream NaN canonicalization.

Allocation ceilings charge the Arrow's direct bytes, old source, intermediate
semantic/own trees, pointer scratch, and final tree/validation allocations where
they overlap. The separately referenced Living shooter is another Entity and
is outside this one-Entity persistence ledger. False saves need no overlay
allocation, even with a zero operation ceiling. Native budget failures are not
Java OOME observations. Existing terminal inventory, external-owner refusal,
pinning, and shooter cleanup remain responsible for the unchanged c occurrence.

## Finite authored controls and limits

[test_arrow_nbt.c](../../tests/entity/test_arrow_nbt.c) reuses genuine
App/Session/World construction and real Arrow/Skeleton/Wolf/Ghast owners. Its
eight control groups cover own signed/unsigned/Boolean reads; failed scalar
prefixes with untouched shooter/ages; full parent read and 15-key build;
16-key Boolean save and capacity boundaries; genuine factory and reader
failure cleanup; provenance/unknown overlay and budgets; an actual region
Chunk load/adoption; and committed legacy Chunk save with independent decode.
The wrapper retains bounded stdout/stderr, exact ordered markers, protected
input hashes, and first failures. Both Windows and sanitized Linux pass all
166 focused checks. The separate source Store transport is not qualified by
the legacy Store control.

The [original packet](../../tests/reference/entity/arrow_nbt/README.md) retains
192 actual calls in four fresh VMs: 108 normal returns and 84 exceptions. Each
VM runs 44 representable profiles plus four separate source-only faults. The
two passive journals each retain 3,107 events and 38 live Code pins. Windows
and Clang Debug ASan/UBSan Linux each match all 7,659 selected scalar positions,
including complete parent, Boolean-save and factory paths. Default entropy is
independent; actual existing-receiver shooter, age and Random preservation is
checked within each invocation. Fifteen pure packet controls pass on both
hosts, normally and under optimized Python. Native immutable result writing
and the source's mutable destination remain separate contracts.

Native immutable Compound views cannot represent a null Java Map, a named
Map-present-null tag, or a Java-null String payload distinct from empty String.
Those original shapes remain source-only. Independent-result writers also do
not model Java mutable Compound identity, partial setter allocation/failure
prefixes, or HashMap traversal order. None is replaced by a missing key,
native NULL output, or budget refusal. Arrow flight/collision/pickup and their
loaded scheduler continuation remain separate reached work.
