# Arrow construction and heading

The client has genuine `SL extends SN` Arrow storage, all three constructors,
the complete heading helper, and a strong nullable Living-shooter reference.
Arrow is an Entity subtype with no Living or Mob subobject. Its existing
canonical Class and registry identity remain Arrow/id10.

The public component API is
[arrow/construction.h](../../include/mc/entity/arrow/construction.h); the source
body is [construction.c](../../src/entity/arrow/construction.c). Windows and
sanitized Linux pass the native component/terminal controls and the finite
unchanged-original constructor/heading comparison. Complete configured suites
pass Windows 760/760 and sanitized Linux 617/617 without registered skips.

## Source bodies

The unchanged client archive has SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Its `sl.class` has SHA-256
`2ba2e27fbc558a78b87c26316968cd5e9595656b96c861b2975cae79201d3a45`.
The following sites describe the actual bytecode bodies; they are structural
source evidence; the executed finite comparison is described below.

| Source method | Code bytes | Complete return | Behavior |
| --- | ---: | ---: | --- |
| `SL.<init>(World)` | 59 | 58 | SN base constructor, own fields, size-only change |
| `SL.<init>(World,double,double,double)` | 73 | 72 | Same prefix, position64, offset0 at69 |
| `SL.<init>(World,Living)` | 290 | 289 | Shooter/pose/launch offsets, look velocity, heading286 |
| `SL.b()` | 1 | 0 | Empty watcher hook during SN construction |
| `SL.a(double,double,double,float,float)` | 199 | 198 | Normalization, Gaussian spread, velocity and angles |

Allocation fixes the canonical client Arrow layout and enters the real base
Entity Class before allocating its accounted object. It creates a RAW owner
without consuming an ID or constructor entropy. Each constructor admits that
RAW owner once, then uses the existing SN constructor for the ID, permanent
bounds, independent default Random, watcher/Map, World binding, origin position,
index0 Byte0 registration and empty Arrow watcher hook. NULL World is supported;
a supplied World must satisfy the shared Random-profile binding contract.

SL then stores d/e/f=-1 at7/12/17, g/h=0 at22/27, grounded i=false at32,
player-fired a=false at37, shake b=0 at42, and airborne age k=0 at47. Shooter c=null
and grounded age j=0 remain allocation defaults on the plain and coordinate
paths. The size call writes width and height to .5f without rebuilding bounds.
Consequently the world-only constructor deliberately returns with SN's original
.6f-by-1.8f origin bounds. The coordinate constructor instead runs the actual
same-bounds SN.e position child after changing size. Neither path enters Math
or consumes Gaussian draws.

The Living overload stores its nullable shooter at52, player-fired flag60 and
size68 before the first shooter dereference73. A NULL shooter therefore retains
that prefix and fails at73. Eye height is the actual virtual result: LS uses
height*.85f, Wolf height*.8f, and LocalPlayer inherits GS's literal .12f. The
LS/GI multiply is at6 inside the eye-height child; its caller is SL81.

The existing SN.c pose body at98 writes current and historical coordinates and
current angles before calling SN.e. Arrow's later launch offsets change only
current coordinates and bounds; histories remain at the earlier shooter pose.
The first MathHelper active use is cos116, after these stores. X offset124,
Y offset136, Z offset162 and position178 precede offset0 at183. Initial look
velocity stores occur in X216, Z248, Y267 order, followed by heading286 with
speed 1.5f and spread 1f. SN.c gained only admission for the genuine Arrow layout;
its existing source arithmetic, stores and child receipts remain unchanged.

## Heading and observations

Heading normalizes by the widened **float** square-root result at13. It reloads
the Arrow's own Random separately for Gaussian calls43/61/80. Each axis adds
`(draw * .007499999832361937D) * widenedSpread`, then all three locals are scaled
by widened speed. Only after those calls and local calculations finish does it
store velocity X116, Y121 and Z127. Horizontal length is another float root139;
current/previous yaw are stored162/165, pitch187/190. It resets grounded age j195
and leaves airborne age k unchanged.

Zero direction, speed or spread does not skip Gaussian calls. NaN and infinity
are evaluated with the source arithmetic rather than clamped or rejected as
finite-value inputs. A missing Random fails at43 before the first velocity
store. Every completed field and RNG/cache effect survives later failure.

Constructor Random entropy remains independent. Arrow's Random, the shooter's
Random and the shared Math Random are separate owners. Base construction creates
the Arrow's own default Random; heading consumes that instance without reseeding.
Ordinary MathHelper table initialization is also distinct from Math.random.
The small heading receipt copies the already-read Random
before the first reached Gaussian, after each draw, and the actual three return
values. These observations add no getter, RNG call or source callback. A missing
Random43 leaves the receipt absent, and partial receipts retain their real count.
They support per-run verification without pretending that independent constructor
clocks supplied equal seeds across executions.

Public heading requires a READY Arrow and obtains its own native occurrence
before entering. An active operation refuses reentry. The constructor calls the
same entered body while CONSTRUCTING. These native guards do not replace the
source helper or add a health/World-equality condition. Strict FP admission and
missing native Math dependencies remain distinct from genuine source-null
failures. Errors retain the parent constructor site, heading site and deepest
Math/position child independently.

## Ownership and failure

[references.c](../../src/entity/arrow/references.c) maintains two distinct cells:
the actual nullable `SL.c:Lls;` source field and its counted native shooter
occurrence. A getter proves source/hold identity and actual same-namespace Living
storage before borrowing. A store retains the replacement before publishing it
and releasing the old occurrence. Same-value stores are no-ops after provenance
validation; NULL is valid. No shooter READY, health, alive or same-World gate is
inserted. A represented zero-hold shooter or an unproved source/hold mismatch is
a native refusal, not a fabricated Java exception.

A constructor failure permanently retains FAILED_CONSTRUCTION with its exact
field, ID, entropy, World and shooter prefix. In particular a native shooter-hold
refusal at52 leaves own stores through47 intact. Constructor errors and snapshots
are values or borrowed views; they own no graph, cleanup or replay rights. The
actual Arrow keeps its completed children even with NULL error output. Final
destruction releases its shooter occurrence and all base children using the same
accounted concrete object size. The inspected failure view belongs to construction;
public heading returns its own independent diagnostic.

World publication admits only the genuine Arrow layout and preserves the existing
Chunk/list/WorldAccess order, duplicate occurrences, false result and failed
publication prefixes. The terminal collector includes every Arrow shooter edge
in its fast scan, provenance validation, iterative traversal, occurrence counts,
external-owner refusal and final clear. A surviving external Arrow's shooter is
never cleared to make retirement pass. Commit pins keep target-only identities,
aliases and Arrow/Mob cycles live while validated reference fields and container
roots are consumed. No allocation, callback or fallible admission is introduced
below the terminal commit boundary. Cross-World source references remain valid
stores; the narrower World terminal scope may refuse them without repairing them.

## Qualification and remaining consumers

[Native constructor controls](../../tests/entity/test_arrow_construction.c) passed
215 checks in eight groups on Windows and sanitized Linux. They cover all three complete constructors,
size-versus-bounds/history behavior, NULL73 and refused native store52 prefixes,
real Player/Skeleton/Wolf/Ghast shooters, late Math116 failure, independent entropy,
seeded/cache/special heading inputs, actual Random failure, native admissions,
geometry failure prefixes, strong-reference replacement and failed disposal.
The retained opportunity is `arrow-construction-dcqrkrn_`, with unchanged protected
inputs and empty stderr. The bounded parser's positive and 24 corruption refusals
pass normally and under `-O`.

The separate Arrow World ownership controls pass 1,067 checks on each host. The first
production build compiled the Arrow runtime but found two fixture-only issues:
an opaque World access lacked its private header and MSVC rejected static
NAN/INFINITY initializers. The corrected fixture uses the explicit header and
the same runtime-local input cases. The subsequent integrated four-test run
passed in 3.34s without skips. Linux then exposed the fixture's missing explicit
SIMD header. With that test-only include, fresh Windows and ASan/UBSan Linux
four-test runs pass in 3.77s and 1.59s, including leak detection and no skips.
Both earlier compile failures remain retained; runtime bodies are unchanged.

The [executed original packet](../../tests/reference/entity/arrow_construction/README.md)
retains four fresh VMs with 40 calls each: 37 normal returns and three actual
null-reference exceptions per VM. Windows and sanitized Linux each match all
4,396 selected source scalar positions. Default constructor entropy and derived
jitter remain independent and are checked within each run; seeded headings
also compare all three actual Gaussian returns, including zero-speed/spread
cases. Raw excluded values and the 3,256 source-only JDI events remain in their
own scope. These finite comparisons do not qualify downstream consumers.

The later [NBT and storage](arrow-nbt.md) and [MC renderer](arrow-rendering.md)
now have their separate evidence. GS's direct-Arrow branch performs the actual
shooter reads117/127 for the ally-helper local while preserving the original
Arrow for LS158; its new controls are native, separate from this constructor
comparison. Natural SL impact would instead pass current shooter c at737 to
victim damage741 with literal4. Skeleton firing, Arrow's 1537-byte tick/collision
body and pickup remain reached frontiers. Existing-reader NBT preserves c/j/k;
only a genuinely fresh constructor supplies null/zero defaults. Constructor
success and World publication provide no natural Skeleton-play, returned
World frame, complete source Store, audiovisual or FPS qualification.
