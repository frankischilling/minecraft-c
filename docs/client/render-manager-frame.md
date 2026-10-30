# RenderManager CPU frame

The canonical client RenderManager implements `th.a(fd,ji,sj,ls,kv,float)` on
the existing Manager, World, Entity, Registry and resource owners. The body
stores its frame references, dispatches `Living.N()`, handles the sleeping
World/Bed branch, and publishes the five interpolated camera fields in source
order. The awake/null comparison and native-only sleeping/lifetime controls
pass on Windows MSVC Debug and Linux Clang18 with ASan/UBSan. RenderGlobal's
Entity/TileEntity traversal and renderer draws remain unfinished.

The entry requires the genuinely completed TH singleton. Its constructor owns
all 28 renderers and their actual BW model/RenderBlocks children and manager
backpointers. A failed constructor retains its real candidate and failure;
later active use reports an erroneous class. Frame binding does not admit an
empty manager, a partial roster or a reconstructed constructor result.

## Original observations

The [unchanged-original fixture](../../tests/reference/client/render_manager_frame/README.md)
contains 432 rows from three fresh JDK17 processes. Its
[provenance](../../tests/reference/client/render_manager_frame/provenance.json)
binds the original archive, observer, inputs, tool archives and initialization
logs. The raw fixture SHA-256 is
`09e389fae8521db85b378aa40e0eb1932090c96afa75f6e6f75bdcf92c370c07`.

The cold process genuinely reads TH.a and records the resulting initializer
failure and its cause chain. Those three rows earn no completed-roster credit.
The two prepared processes first execute unchanged `jl.a()V`, the Statistics
entry reached by the original Minecraft constructor at PC186 before its TH.a
read at310. They then initialize all 28 renderers and genuinely construct
`wh(null)` (Pig). Preparation uses the original archive's class-loader
resources. It does not establish complete startup parity.

The prepared awake process observes all 24 authored common Entity pose cases
through the unchanged method, including finite partials, signed zero,
subnormals, overflow and measured NaN operands. World, Engine, Font and Settings
are null. The original Pig inherits `ls.N()Z`, which returns false. The separate
prepared null process records the first null-Living failure after all five
reference stores. Java stack lines remain in the raw evidence; source PC29
comes from the pinned bytecode.

The complete method has 258 Code bytes, SHA-256
`cc312e67247d9c59c80c87ac102f68ed5ad266b397d5e355b797164e74c4c28e`.
The capture also fingerprints all 747 original classes and every method Code
in the pinned client archive. It records actual model/reference stability,
primitive origins and side effects. Original Map iteration and identity hashes
remain diagnostics; logical roster order does not imply JVM bucket parity.

## Ordered stores and arithmetic

The method publishes `g=World` at2, `e=Engine` at7, `k=Settings` at13,
`h=Living` at19 and `p=Font` at24. Settings is stored without a source field
read. Null Living fails at the virtual `N()` call29, preserving those stores
and all prior camera scalars. Ordinary method failure preserves completed
effects and leaves the completed TH class ready.

For an awake Living, the angle operations and stores are:

| Output | Subtract | Multiply | Add | Store |
| --- | --- | --- | --- | --- |
| `i`, yaw |150|153|154|155|
| `j`, pitch |174|177|178|179|

Both angles use binary32. The common position branch then reads the actual
Living fields afresh. Each component subtracts in binary64, widens the
binary32 partial, multiplies, adds the previous coordinate and stores before
starting the next component:

| Output | Subtract | Widen partial | Multiply | Add | Store |
| --- | --- | --- | --- | --- | --- |
| `l`, X |198|201|202|203|204|
| `m`, Y |223|226|227|228|229|
| `n`, Z |248|251|252|253|254|

Return is at257. Endpoint partials keep the same operations; there is no
algebraic shortcut or fused multiply-add. The shared interpolation helper
preserves the measured x86-64 JDK17 operand and NaN-payload order. The
[OpenJDK interpreter source](https://github.com/openjdk/jdk17u/blob/jdk-17.0.17%2B10/src/hotspot/cpu/x86/templateTable_x86.cpp)
is the matching primary implementation authority. An operation with two NaNs
and at least one signaling operand remains unmeasured and returns
`UNSUPPORTED_NUMERIC` at that operation. It leaves earlier stores intact.

Each reached arithmetic operation requires strict x86-64/SSE2 floating
controls, `(MXCSR & 0xffc0) == 0x1f80`. The method allows sticky exception flags,
does not repair controls, and reports `UNSUPPORTED_FP` only when a reached
operation needs them. Null-Living failure and preceding reference stores do
not eagerly initialize Math or perform interpolation.

## Sleeping branch and Math lifetime

Sleeping dispatch uses the actual canonical Living subtype. A native
LocalPlayer reads its owned Player `u` through `gs.N()Z`; it does not execute
the original Pig's inherited `ls.N()` body. Other admitted Living types use
their qualified inherited body. An unreached concrete override remains an
explicit dependency.

The sleeping branch floors the current X/Y/Z at41/49/57 before the World block
read60. It then obtains actual Registry `UU.T` at67 and reads the Bed ID at70.
A Bed match reloads the same Living's current coordinates, floors them at
82/90/98, and reads World metadata at101. The method stores
`(metadata & 3) * 90 + 180` as yaw at123 and positive-zero pitch at128. A
non-Bed preserves the previous angle fields. Both branches then perform the
common position interpolation.

Nonnull Worlds must be genuine GAME Worlds sharing TH's Registry, Entity
classes, Random profile and client side. Reads enter the installed provider,
cache and stored Chunk. A missing acquired Chunk is not treated as air. The
Manager retains the complete acquisition diagnostic from the one reached
block or metadata call; it does not repeat a query to reconstruct a cause.

The caller supplies the same live Math class namespace at reached sleeping
floors. Its first active use publishes the zero table at `<clinit>`4 before
the floating-control check20 and fills through37. A failed initializer retains
that published array and its cause; retry reports the sticky failure. Awake
and null-Living paths do not require Math active use. The failed-Math native
controls close their entire Manager/Model/Registry/Entity namespace before
opening a separate ready-Math namespace.

## Native comparisons and controls

The two new CTests pass on Windows in 6.15 seconds and Linux in 77.08 seconds,
including source hashing on the Linux mounted workspace. The frame comparison
reports zero mismatches: 2,738 exact column comparisons, 136 explicitly
unqualified columns, one typed-status comparison and one pinned-PC comparison.
Every one of the 432 raw original rows remains accounted for. These results
apply to the scoped frame comparison and sleeping controls. All 27 selected
targets build on each platform. The broader regression selection passes 35
Windows CTests in 53.07 seconds and 34 sanitized Linux CTests in 79.00 seconds,
with no failures or skips. It includes existing dispatcher, Settings, World,
lighting and terrain checks. The Windows loaded-World test takes 13.06 seconds;
its wrapper enforces all 72 focus histories and save/resource preservation.
The rebuilt executable also completes two menu frames and three ticks, with
all 413 imported assets unchanged and no JVM/JAR runtime.

The authored awake/null runner retains and accounts for all 432 original
rows. Its native operand is a genuine RAW `dc` LocalPlayer with the measured
common pose inputs. It compares the method outputs and reference identities
where the schema permits, with explicit exclusions for the original Pig's
runtime Class and constructor and for the differing `N()` declaring class.
No expected output is supplied to the native executable.

Original Pig construction, Entity IDs and instantiated actor Random, Math
holder and Watcher state remain uncredited. Preservation of native absent
children cannot qualify the original instantiated children. The 9,368
reflected model-field slots and 880 reference arrays are original Java graph
observations. A native typed-object snapshot checks its own graph stability;
it does not reuse those counts as native allocation answers. The cold native
failure has its own typed cause record, separate from the original Java
exception chain and stack lines.

The native-only sleeping/lifetime controls pass all 15 declared rows and
133,388 checks, including both complete 65,536-word zero scans. They use a real
WorldStore, constructed GAME World, provider/cache and loaded populated Chunk. They cover
all four Bed metadata orientations, non-Bed angle preservation, null World
after reached Math floors, published-zero Math failure and sticky retry, and
a real acquisition limit at TH60. A separate detailed metadata failure makes
one query. An isolated TH101 failure is unexercised: the preceding successful
block read and metadata read use the same validated Chunk without an
intervening source effect. These controls do not establish original sleeping
branch, actor-constructor or Bed-placement equality.

## Ownership and integration limits

TH holds the current World and Living occurrences until replacement. Distinct
captured Engines, Fonts and Settings remain held until explicit outer TH
retirement, including displaced source references. Captured Settings does not
gain a new App ownership cycle; its real borrowed App must remain live.
Current and displaced RA hands must finish their separate checked retirement
with their actual Display and resource owners alive before TH's CPU namespace
can retire.

Retirement requires zero outer consumers and no conflicting Manager, renderer
or retained-service operation. It releases Fonts and Settings before Engines
under both TH operation guards. A failed release preserves the remaining
occurrences for retry. The authored lifetime control uses real RAW Engine and
Font owners and default Settings. A deliberate Settings diagnostic refusal
tests the actual active-release failure after font-first cleanup, followed by
retirement retry. Typed source Settings' active preflight is not exercised by
that control. Checked cleanup completes on both hosts. Sources, executable
and resource inputs remain unchanged; the owned World retains its input
region, permits its intentional content change and records the eight-byte
session lock. Native cleanup does not claim an original source reset or
resource constructor completion.

Integrating these owners into RenderGlobal's `I=2` delay, Entity/TileEntity list
traversal, eligibility checks, renderer dispatch/draw
and the full application retirement path remains unfinished. CPU frame
binding does not establish complete World-frame or gameplay parity.
