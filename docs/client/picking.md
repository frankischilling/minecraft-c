# Picking and hit-result ownership

Picking follows the original `px.a(F)` through LivingEntity view methods, the
World block ray, the nearby-Entity query and the final App hit field. These are
callable source bodies on the existing App, Entity, World and class owners.
They do not establish a complete rendered World frame or a playable client.
Windows MSVC Debug and Linux GCC13/Clang18 sanitizer profiles match all
26 prepared original OS cases and pass eight ownership/failure controls.

The public interfaces are [picking](../../include/mc/client/world/picking.h),
[LivingEntity view](../../include/mc/entity/living/view.h),
[World raytrace](../../include/mc/world/runtime/raytrace.h),
[HitResult](../../include/mc/entity/hit_result.h),
[Vec3](../../include/mc/entity/vector/vector.h) and
[bounds](../../include/mc/entity/bounds/bounds.h). The original client authority
is Beta 1.7.3 JAR SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Original PX `(F)V` has 453 Code bytes, SHA-256
`4cd5aaca219c77db008832d92b3360ae0022cadfbe4da37bb310e1afc025c66e`.

## One HitResult and its loaded classes

`mc_entity_hit_result` is the actual VF owner. App's `mc_client_source_vf` is an
alias of that same tag. A World result, an intermediate bounds result and a
final Entity result use this owner; App.y receives another native occurrence
of the same object. Its source fields are the JG kind identity, four integers
for block XYZ/face, the BT hit vector and the nullable Entity reference.

JG belongs to the existing loaded Entity-classes namespace. Initialization
constructs TILE with ordinal 0, publishes `a` at PC 10, constructs ENTITY with
ordinal 1, publishes `b` at PC 23, then fills the real two-element values array
before publishing it at PC 42. Enum names are immutable class-owned UTF16
literal objects. This
implements the reached name/ordinal fields; general Enum or String APIs are
outside this increment. A failed native initializer preserves its objects,
statics and first typed cause and remains failed. Native quota failure is not
reported as an invented Java OOM or NoClassDefFoundError.

VF allocation is separate from construction so a caller can retain the actual
RAW object before evaluating later arguments. The block constructor stores its
kind at PC 8, XYZ at 13/18/23 and face at 29 before reading the point at 35. A
null point faults there with those fields intact. The Entity constructor stores
its kind at 8 and Entity at 13 before reading current Entity XYZ at 18/22/26;
null Entity faults at 18. Both constructors then acquire a pooled BT copy, at
48 or 29, before publishing f at 51 or 32. Failed constructors keep their
reached fields and cannot retry.
[Implementation](../../src/entity/hit_result/owner.c)

The final Entity VF contains the target's current position. It does not contain
the nearest bounds-intersection point or an interpolated target position.
Each nonnull g field has its own Entity occurrence, independent of PX.m and the
World query List. The class owner stays busy while any RAW, failed or completed
VF or retained geometry call remains live.

## Geometry and pooled identities

BT and EQ use the existing class-owned vector and bounds pools, shared across
Worlds. A native hold keeps storage live. It does not stop a later source pool
reset and reuse from overwriting the same object's coordinates. Even VF.f can
equal the constructor's input point when the reset cursor reuses that slot.
Client pool clear detaches held objects; their storage survives until the last
native occurrence is released. None of the picking helpers inserts a reset,
clear or coordinate rollback.

BT segment-plane helpers first compute all end-minus-start deltas. They reject
the selected delta only when its square is below
`1.0000000116860974e-7`, the widened float literal. They then compute the plane
parameter, accept both endpoints and evaluate all three result coordinates
before calling the same pooled factory at PC 104. A normal miss consumes no slot.
A returned plane point consumes a slot even if the caller later rejects it.

Distance uses the ordered squared sum followed by `in.a(D)F` at PC 47 and
widens that float to double at 50. The result has a float-rounded square root.
Squared distance omits that root. Native geometry reuses the existing strict
arithmetic kernel with round-to-nearest and gradual underflow. It does not
claim native lazy MathHelper initialization timing; normal bootstrap already
warms the actual shared class.

EQ point inclusion excludes finite faces, edges and corners. Its negative
rejection comparisons allow an unordered NaN component to pass that axis;
rewriting them as positive strict inclusion changes the result. The separate
plane-rectangle helpers use inclusive positive comparisons and reject NaN.

EQ intercept generates all six candidates before filtering them, in Xmin,
Xmax, Ymin, Ymax, Zmin, Zmax order. It keeps the first equal-distance candidate
and replaces it only for a strictly smaller squared distance. Face selection
uses six independent identity comparisons, mapping those planes to 4,5,0,1,2,3.
The successful constructor is `VF(0,0,0,face,point)` at PC 395, so this intermediate
result is TILE with zero block coordinates even when PX is testing an Entity.
The retained [intercept call](../../src/entity/bounds/intercept.c) keeps every
generated vector until retirement, including those whose source locals were
filtered to null. It also preserves a failed RAW VF candidate separately from
the nullable normal result.

The 942-row geometry fixture contains finite inputs. It does not distinguish
two different NaN payloads in the segment multiplications `delta*t` at
BT/BA PCs 82/92/102. A native edge control executes NaN multiplication but checks
NaN classification, not operand payload priority. General two-NaN multiplication
bit parity, including compiler/optimization differences, remains unmeasured
for these helpers. The separately observed LS interpolation correction does
not extend that qualification to segment geometry.

## LivingEntity origin, look and reach

LocalPlayer inherits the original LS view bodies. `ls.e(F)` returns current
coordinates for exact partial 1, using the pooled factory at PC 18. Other partials
compute `previous + (current-previous)*widen(partial)` in double and use the
factory at 83.
The method adds no eye-height constant. It does not clamp partials or skip the
actual arithmetic for partial 0. Exact 1 avoids reading the previous fields.

`ls.f(F)` preserves float angle interpolation and the original MathHelper sine
table. It neither wraps the angle difference nor normalizes the returned
vector. The supplied Math namespace is the same bootstrap-owned namespace;
lookups occur at PCs 17/122 and pooled results at 76/176. `ls.ac()` is the actual
f(1) wrapper. The [view implementation](../../src/entity/living/view.c) contains
a local, observed correction for two-NaN multiplication in position
interpolation. That correction does not alter older shared math bodies.

`ls.a(DF)` obtains position at PC 2, look at 9 and endpoint at 37, then reads the
receiver's actual World field at 43 and invokes the two-vector World overload
at 50. It does not substitute App.active_world. A null Entity.World faults at 50 after
the earlier vector effects. The World DDA can mutate that same pooled origin.
PX later obtains another origin from the independently reread App.i.

The admitted native controller is the actual OS owner, whose reach method
returns 4.0f at PCs 0/2. PX applies its own ordinary Entity limit of 3.0 after
the block ray. Original PJ instead inherits base reach 5.0 and subsequently
takes PX's separate 32-unit Entity branch. A native PJ owner is not implemented.

## World and Block selection

The two-vector World overload forwards false,false at PC 5. The next overload
forwards the supplied liquid flag and false. The core `fd.a(BT,BT,ZZ)` reads
start NaNs before end NaNs, floors the original end and start cells, and tests
the initial cell before entering its bounded traversal. Its source counter
allows 201 traversal bodies in addition to the initial candidate cell.

Every visited candidate performs separate ID and metadata reads:125/137
initially, 838/850 later. The optional second flag invokes the actual movement
box before the canCollide predicate. Only that flag makes a null movement box
a reason to skip selection. The first flag admits source fluids only when
metadata is 0. Fire rejects selection; stairs delegate through their actual
model Block pointer. Registry and acquisition failures retain their typed
causes instead of becoming air or a successful miss.

Traversal mutates the supplied start in place and recomputes fractions from
that current value. Strict comparisons make equal boundary fractions prefer
Z over Y over X. Floor conversion and negative-face cell corrections retain
Java integer conversion and wrapping behavior. The source also makes its
pooled scratch vector at PC 733 and changes that scratch for negative faces; pool
aliasing can make those writes observable through another retained reference.
[World body](../../src/world/runtime/raytrace.c)

The Block ray runs the actual virtual state-shape and wrapper methods. Door,
trapdoor and rail wrappers repeat the shape call before the base body's call;
Torch sets its live singleton bounds from metadata. Selection therefore uses
the same mutable Block bounds as the rest of the World. Stairs select the
state-shape unit cube, while Ladder can retain bounds established by an earlier
movement-box call. The optional movement-box flag can change that history.

Base Block selection generates six planes before filtering, but compares
float-rounded BT distance rather than EQ's squared distance. It allocates RAW
VF at PC 458 before the final world-space point add at 477 and constructor at
480. The result
has actual block XYZ. Substituting EQ intercept would change the result's
coordinates, distance ties, source calls and pool effects.
[Block body](../../src/world/runtime/raytrace_blocks.c)

## PX publication and query order

The [picker](../../src/client/world/entity_renderer/picking.c) stores the block
ray in App.y at PC 50, then independently obtains origin at 64 and measures an
existing hit at 91. It obtains look at 144 and endpoint at 172, clears PX.m at
179, sweeps and expands the camera's actual bounds at 230/242 and queries App's
current World at 245. The query
World and the earlier LivingEntity ray's World are separate source reads.

Each List entry receives actual h_ and m_ dispatch at PCs 284 and 295. Item and pickup
particle are not pickable; FallingSand, Minecart and LocalPlayer return `!be`.
These current descendants inherit border 0.1f. This is h_, not the existing i_
pushability helper, which differs for FallingSand and dead Minecart.

PX computes EQ intercept at PC 325 before point inclusion at 334. An inside
candidate sets the distance sentinel to 0 at 361. Later inside candidates can
replace it, and a later farther intersection can replace it because 0 is the sentinel.
Equal nonzero distances keep the earlier List entry. The actual World query
can contain repeated occurrences of one Entity; it is not sorted or deduplicated.
Final ordinary Entity selection constructs VF(Entity) at 446 and publishes
App.y at 449.

App.y replacement first obtains an independent occurrence of the new VF, then
releases the old occurrence. A failed right-hand side leaves the old field
unchanged. Null camera returns at 10 and null App.World at 21 before clearing
the old hit or target. Retained calls expose publication flags, exact source
PCs and full child causes. Their large workspaces live on the heap. A checked
retirement consumes only owned rights; it can be retried after cleanup refusal
without repeating source work. Terminal source failure and cleanup failure
remain separate. PX detach releases its actual hit/target occurrences before
World retirement and preserves fields/rights when active or foreign holds
prevent that cleanup.

## Prepared original PX observations

The unchanged original picker returned in 27 authored cases on Temurin 17.0.17+10.
It used a real SaveHandler, World, four loaded Chunks, real LocalPlayers and
target Entities, and actual World publication/query methods. Only the original
Minecraft subclass and PX receivers used Object-only allocation with authored
reachable fields; their constructor bodies did not run. No source picking body
was replaced. The run made no network or native-library-link attempts.

A and B below are genuine FallingSand instances. All cases use OS except the
explicit PJ row. All 26 OS cases match the native comparison on three
compilers. The PJ row remains original-only because its native controller
owner is absent.

| Original case | Observed behavior |
| --- | --- |
| `empty` | Normal null result and target. |
| `block_near` | TILE `(8,64,10)`, face 2. |
| `block_os4_beyond_entity3` | TILE `(8,64,12)` remains within the block reach. |
| `block_beyond_os4` | Block beyond OS reach returns no hit. |
| `falling_hit` | ENTITY A, with the same identity in PX.m and VF.g. |
| `minecart_hit` | ENTITY Minecart, with its current position in VF.f. |
| `player_hit` | ENTITY LocalPlayer, with its current position in VF.f. |
| `entity_behind_block` | Nearer TILE remains; A is present in the query. |
| `entity_before_block` | A replaces the farther TILE. |
| `inside_box` | A contains the origin and is selected. |
| `excluded_self` | The camera is absent from its own nearby query. |
| `dead_not_pickable` | Dead A is queried but not selected. |
| `item_not_pickable` | Live Item is queried but not selected. |
| `equal_distance_first_A` | Query AB selects A. |
| `equal_distance_first_B` | Query BA selects B. |
| `duplicate_order_ABA` | Query retains ABA; A remains selected. |
| `inside_order_AB` | Both contain the origin; B replaces A. |
| `inside_order_BA` | Both contain the origin; A replaces B. |
| `inside_then_far` | Farther B replaces inside A through the 0 sentinel. |
| `far_then_inside` | Inside A replaces farther B. |
| `entity_before_cap3` | The near-side Entity at Z=12 is selected. |
| `entity_beyond_cap3` | Entity at Z=12.25 is not selected. |
| `block4_entity3_cap` | TILE at Z=12 remains with the latter Entity present. |
| `pj_m_without_entity_vf` | PX.m becomes A while App.y remains null; original-only PJ branch. |
| `finite_partial_half` | Normal miss with partial 0.5; exact view/DDA/pool bits recorded. |
| `early_null_camera` | Existing VF, PX.m and pool cursor are preserved. |
| `early_null_world` | Existing VF, PX.m and pool cursor are preserved. |

## Finite qualification

The [geometry fixture](../../tests/reference/entity/hit_result/README.md) has 942
rows whose unchanged client and server outputs are identical. Windows Debug
`entity.hit_result` passed 58,830 checks, including both namespaces, constructor
prefixes, quotas, pool reuse, foreign owners, independent result holds and
class-free refusal. The committed replay requires no JAR or JVM at runtime.

Linux GCC13 and Clang18 ASan/UBSan runs also pass the geometry and World-ray tests.
The 365 World rows match the unchanged original on nullable results, vector
bits, pool cursors and source-bound changes across all 96 registered Block IDs.
They use loaded authored Chunks; unloaded transport and generation are outside
that comparison. The [World fixture](../../tests/reference/world/raytrace/finite.json)
records the original inputs and observation scope.

The [view fixture](../../tests/reference/entity/view/README.md) contains 192
original rows for LS.e/f/ac, including nonfinite inputs and the observed local
NaN-multiplication correction. Linux GCC 13 and Clang 18 sanitizer runs passed
the view test. All four new public CTests pass together on Windows and both
Linux sanitizer profiles. The [prepared PX fixture](../../tests/reference/client/picking/README.md)
compares all 26 OS rows exactly and passes 1,964 native checks, including eight
null/failure, retained-call, field-replacement, foreign-hold and pool-alias
controls. Comparisons include before/after field identities, query order,
partial/reach bits and all used pool slots. A fresh original rerun reproduces
all 27 original rows; a separate one-bit golden corruption is rejected. PJ
remains excluded.

The real READY PX also completes picking after Windows saved-World loading
and generated-terrain entry. Its integration check verifies return452, source
publication, pooled-vector use and checked retirement. Those native UI checks
use imported resources; the four new body tests require no original assets,
JAR or JVM. Rendering, target interaction, World ticking and full client
playability remain separate work.
