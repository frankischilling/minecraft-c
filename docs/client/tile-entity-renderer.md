# TileEntityRenderer

The CPU dispatcher implements the original client `ll` class on the same
World, Entity, model and Random-profile owners. Its constructor creates the
three actual renderers: Sign (`po`), MobSpawner (`ag`) and Piston (`hy`). It
publishes the singleton only after all three constructors and their manager
backpointers complete. Sign construction uses the canonical ModelRenderer,
quad and Vec3 graph for its board and pole. MobSpawner owns a separate empty
Map. Piston starts with a null RenderBlocks field.

## Class lookup

World classes own nine passive TileEntity Class identities, including the base
class. Reading or borrowing them does not initialize the source registration
maps, construct a TileEntity, allocate a World or consume Random values. Client
and server cells retain their own original names and superclass metadata.

The dispatcher uses these actual Class identities as Map keys. A missing
subclass renderer recursively queries its real superclass and caches the
result, including null. A cached null result repeats that recursion on the
next lookup. Replacing an existing entry preserves its node identity. The
base TileEntity Class stops recursion before Java Object, which is outside
this native namespace. A null Class reaches the original dereference at
PC26; a null TileEntity returns no renderer immediately.

Native bucket order is an implementation detail. Original Java identity
hashes, bucket chains and iteration order are retained as private diagnostics;
the public comparison sorts only logical Map entries by Class name.

## World callbacks and frame binding

The direct World method stores `ll.f` before iterating the Map's nonnull
renderers. Sign and MobSpawner inherit the complete return-only base callback.
Every reached Piston callback creates a new actual RenderBlocks instance,
even when the supplied World is null or unchanged. Its 65 declared fields use
the canonical constructor. A failed child preserves the earlier World store.

The frame method has a separate World identity check. It calls those callbacks
only when the World reference differs, then stores Engine, Living and Font in
that order. A null Living fails at PC32 after all three stores. The five
interpolated camera fields publish separately at PCs50,74,99,124 and149.
Binary32 angle arithmetic and binary64 position arithmetic keep the original
subtract, multiply and add order; endpoint partials have no shortcut.

The recorded x86-64 JDK17 interpreter profile also preserves observed NaN
payloads, signed zero, subnormal results and overflow. The native operations
make payload selection explicit where compiler reordering could otherwise
change it. The matching
[OpenJDK interpreter source](https://github.com/openjdk/jdk17u/blob/jdk-17.0.17%2B10/src/hotspot/cpu/x86/templateTable_x86.cpp)
records the operand order used for these comparisons. An operation with two
NaNs, at least one signaling, remains unqualified and fails explicitly at that
operation. Unsupported floating controls are rejected only when the first
reached arithmetic operation needs them, after reference stores and the null
Living check. Earlier completed component stores survive a later refusal.

## Native ownership

The LL association prevents its model namespace from being freed and holds a
counted borrow of the canonical TileEntity classes. One independent native
World occurrence backs `ll.f`, and each live RenderBlocks object owns another
for its World field. Replacing Piston's current object releases an unretained
old object immediately. Explicitly retained old objects remain inspectable and
block LL retirement until released.

The current Living occurrence is retained before replacement. Distinct Engine
and Font owners remain held until outer LL retirement, so a source field store
does not invoke resource cleanup. Retirement releases Fonts before Engines
under the LL operation guard and keeps failed cleanup slots for retry. The
actual Display and graphics owners must outlive this cleanup. Holding several
separate application resource graphs in one LL namespace has not been
qualified.

Allocation bounds are native policy. Constructor failure retains the candidate,
partial children and original cause; later active use reports an erroneous
class without retrying the constructor. Native quota timing does not claim
equivalence to a Java heap failure.

## Evidence and remaining work

Nineteen production/test targets build on Windows MSVC Debug and Linux Clang18
with ASan/UBSan. The selected regressions pass all 27 Windows tests in 46.56
seconds and all 26 Linux tests in 47.60 seconds, with no failures or skips.
They include existing TileEntity, RenderManager, lighting and terrain tests.
The Windows loaded-World check retains all 72 focus histories; the rebuilt
native executable opens two menu frames using imported assets without Java.

The [constructor fixture](../../tests/reference/client/tile_entity_renderer/README.md)
contains 254 unchanged-original rows, including all Sign geometry and nullable
lookup paths. The [World fixture](../../tests/reference/client/tile_entity_renderer/WORLD_README.md)
contains 251 rows from three real null-World callbacks and two null-Living
failure contexts. The [frame fixture](../../tests/reference/client/tile_entity_renderer/FRAME_README.md)
contains 24 arithmetic cases and a separate genuine original Pig-constructor
prerequisite. Native replay uses a genuine RAW LocalPlayer and its common
Living/Entity pose fields, because native Pig allocation is unavailable. It
compares all 96 method rows literally, with separate credit limits for runtime
Class, constructor and unconstructed Random/Watcher children. Those channels
do not establish equivalence to the original Pig's constructed state.

These CPU bodies do not yet complete RenderGlobal's Entity/TileEntity pass.
The actual `I=2` delay, RenderManager frame binding, live World-list traversal,
distance/frustum predicates, renderer draw bodies and later World-frame work
remain separate dependencies. No empty dispatcher or successful draw stub
substitutes for them. Full World frames and gameplay remain unfinished.
