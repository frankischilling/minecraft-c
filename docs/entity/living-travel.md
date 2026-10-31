# Living and Player travel

This client-first module executes original ancestor bodies on the existing
READY canonical LocalPlayer (`dc`). Its real constructor, World, Registry,
permanent bounds, shared bounds/vector pools, attached input, Living fields
and Player fields remain the existing owners. No alternate Player, callback,
World, class owner, tick loop or field-layout overlay is introduced.

| Native entry | Actual original body | Scope |
|---|---|---|
| `mc_living_travel` | `ls.a_(FF)V`, Code 730 | Qualified Living travel including the limb tail |
| `mc_player_travel` | `gs.a_(FF)V`, Code 48 | Entry-coordinate save, Living travel, then actual private distance body |
| `mc_living_on_ladder` | `ls.p()Z`, Code 52 | Actual bounds/World/block-static ladder query |
| `mc_living_jump` | `ls.R()V`, Code 8 | Qualified base vertical-velocity store |
| `mc_player_jump` | `gs.R()V`, Code 13 | Base store, actual `jl.u`, reached canonical statistic call |
| `mc_player_movement_statistics` | `gs.i(DDD)V`, Code 216 | Mounted/eye-fluid/cached-water/ladder/ground/flying dispatch |

These are explicit body entries for future source callers. Calling the base
jump alone does not execute the genuine Player jump wrapper. No active
`ls.o`, `dc.o`, `ls/gs.w_` or World tick is implemented by this module.

Travel keeps the distinction between cached `sn.bx` (`ag()Z`) and querying
lava with the actual pooled bounds (`ah()Z`). The water branch uses 0.02f
relative acceleration, actual movement, widened 0.8f drag and double 0.02
gravity, then conditional offset clearance. Lava uses the same acceleration,
actual movement, double 0.5 drag and double 0.02 gravity. These branches do not
invent a `bx` update. The escape Y expression is evaluated in source order:
`((velocityY + (double)0.6f) - currentY) + savedY`; successful clearance sets
the widened 0.3f velocity. Every descendant failure retains its completed
prefix; later drag, escape and limb stores do not run after failure.

The dry branch reads ground state and live supporting Block.bB twice at the
two original sites 305/404. It retains the three float friction multiplies,
0.16277136f division, grounded 0.1f factor and air 0.02f acceleration. The
second friction result controls final horizontal drag. Ladder clamps X/Z to
the widened 0.15f, resets the actual base fall distance, clamps Y to double
-0.15, and queries the actual `dc.t()Z` input/sleep descendant. It then moves,
rereads horizontal collision and ladder, applies double 0.2 climb, double 0.08
gravity and widened 0.98f vertical drag. The tail first copies `al` to `ak`,
uses actual previous-tick X/Z (`aJ/aL`), double sqrt converted to float,
float 4, clamp 1, float 0.4 smoothing and final `am += al`. It does not use the
Player wrapper's saved entry coordinates for that tail.

The private distance body skips all descendants when mounted. Otherwise it
initializes/reads the actual water Material before eye immersion, then tests
cached water, ladder, grounded and flying in that order. Dive uses XYZ
distance; swim/walk/fly use XZ distance. Distances pass through original
sqrt-to-float and float 100 before `Math.round(F)I`. Ladder reads `jl.o`
before evaluating positive Y times double 100, `Math.round(D)J` and Java l2i.
It calls the statistic even if narrowing yields a negative amount. The
selected canonical fields are `jl.q/m/o/l/p`; jump uses `jl.u`. The real
Registry statistics initializer and descriptor ownership are reused.

Reached statistic calls use the [actual application-owned writer and notice](../client/player-statistics.md)
through the same LocalPlayer. Errors retain the canonical descriptor, original
field name, exact amount, GS caller PC and the actual dc child site. A null
application now reaches dc85; it is no longer a pending substitute for dispatch.
Successful full-App calls complete their statistic operation. Movement/effects
can reach earlier damage descendants, retained in the full child frame; Living
hurt remains unfinished after its real statistic increment at gs158.

The travel context explicitly borrows the caller's actual loaded Registry
namespace and initialized immutable `mc_math_trig`. Null World does not
prevent source bodies that do not read it, such as qualified jump or cached
water with no-clip movement and no escape query. Native admission holds the
Registry without initializing it. Reached statics/nonnull Worlds require the
same real Entity/Registry namespace. The future source caller must provide
its shared arithmetic/code owners; no synthetic World supplies them. Keep
owners, limits and contexts exclusive/live through calls and keep errors
disjoint. Descriptor error pointers borrow the caller's Registry after return.
No operation resets a pool. Actual child native work/allocation limits remain
effective; no successful truncation is added. Only canonical client dc is
admitted; other Living/Player dynamic descendants and server gameplay remain
explicitly unsupported.

Arithmetic follows the captured x86-64 strict SSE profile, including source
operation widths, sqrt conversion and commutative NaN operand priority. The
private round helpers implement the JDK17 significand paths with exact Java
shift/add and saturating conversion helpers. They avoid the double rounding
of a float/double `value + 0.5` approximation. The core math implementation
is unchanged. No FP admission is imposed on the constant-only base jump;
checks occur at reached arithmetic. Output fields are not normalized.

Errors name the requested source body. `living_bytecode` names ls travel,
`ladder_bytecode` names ls.p, `player_bytecode` names `player_method` (gs.a_,
gs.R or private gs.i), and `statistic_bytecode` names dc.a. Passive error
initialization avoids additional large automatic child-frame copies; it has
no source effects. Successful calls return a fresh empty frame. Native
constructor diagnostics are untouched.

Authored tests use real GAME/Store/loaded-Chunk/Registry constructors and the
actual LocalPlayer constructor plus a real uo input. Voxel/source-field
assignments are controlled inputs, not constructed answers or replacements
for source methods. Temporary null source projections are restored before
assertion/teardown while genuine native owner ledgers remain held. Original
observations execute the unchanged supplied kq/Minecraft constructor, actual
xi statistics writer, World/Chunk/dc and source ancestors with MethodHandle
qualified-body dispatch. No graphical startup, fake subclass, Unsafe or class
replacement is used. Original positive-stat records are sampled after the
genuine writer returns. The pinned ordinary `dc.a` branch calls `xi.a(vr,int)`
at 90 without Player motion/limb writes. Those records establish the selected
descriptor/amount and movement prefix; native full dispatch
remains pending. Whole class/method Code metadata and the supplied JAR hash
are checked before original runs. JDK Math class metadata, authored compiled
helper bytes and compiler/runtime identities are retained with each run.

The native-only CTest driver requires exit 0, empty stderr, complete bounded
capture with an explicit empty reader-error dictionary, and a positive final
check count. It uses a unique output child for every invocation. The optional
Windows original controller requires pinned libraries/native LWJGL and one
matching JDK17 runtime/compiler. It compares exact ordered records and keeps
JDK-only rounding records separately labelled. Source assets and original
class bytes are not distributed with this module.
