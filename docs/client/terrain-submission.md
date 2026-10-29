# Terrain submission

`n.a(Lls;ID)I` submits compiled ChunkRenderer lists through the retained
RenderGlobal graph. Its implementation covers dirty polling, render-distance
rebuilds, recentering and JO sorting, subset selection, the four TG batches,
and the ARB occlusion branch. Eight focused Windows MSVC Debug CTests pass in
4.51 seconds, with zero failures or skips. The original whole-N fixtures record
12 CPU failure profiles and seven device profiles with 35 exact rows. Hosted
qualification is pending; verification applies to the specified native profiles.

The World-frame caller invokes submission at `px.a(FJ)V:486`, passing its
captured camera, pass0 and the float partial widened to double. A normal N
return supplies the source integer result at896. PX discards it at489, sets
FLAT shading at493 and reaches the unfinished `u.b()` lighting-enable child
at496. This contract does not establish a complete World frame or gameplay.

The public [begin/inspect/retire API](../../include/mc/client/world/render_global_submit.h)
keeps the source result separate from native status. Its implementation is in
[render_global_submit.c](../../src/client/world/render_global_submit.c), with
the entered grouping and child operations in the adjacent submission files.

## Outer method and source storage

N uses its spatial DK array `o`, sorted DK array `n`, dirty List `m`, selected
List `S`, and original four-cell TG array `T`. Those identities persist across
the call. TG's direct buffer `g` contains submitted list IDs; N's older integer
array `b` has a different role.

| Source PCs | Operation |
| --- | --- |
|0..74|Perform ten dirty polls, including on pass1. Each poll stores wrapped `(R+1) % o.length` at23, reads that spatial DK at34, and checks u39. A dirty DK enters m65 only when identity-based contains51 finds it absent.|
|77..95|Read current App/Settings render distance and rebuild at95 when it differs from H.|
|98..124|For pass0, reset M/N/O/P/Q at104/109/114/119/124 in that order.|
|127..179|Interpolate the argument camera's previous/current coordinates using separate binary64 subtraction, multiplication and addition. The method receives a double partial.|
|181..299|If squared displacement from N.f/g/h exceeds16, store the new position243/251/259, reach MathHelper floor267/274/281, recenter284, construct JO296 and sort the same n array299. NaN displacement skips this branch.|
|302|Run the whole `u.a()` lighting-disable method, including its class active use.|
|308..341|Select occlusion only for `N.w && Settings.h && !Settings.g && pass==0`, preserving the source short circuit and fresh Settings reads.|
|345..874 or888|Run occlusion subsets or the ordinary whole-range subset. Add returned counts with Java integer wrapping, then return896.|

The ten polls precede Settings, counters and camera work. A zero array length
can therefore fail on remainder22; a wrapped negative cursor can fail at the
later array access after R23 has already changed. Empty geometry does not
remove these effects. Pass1 retains the existing M..Q counters and still uses
N.w/DK.x during subset selection even though it skips query issuance.

## Recenter, rebuild and JO

[Recenter](../../src/client/world/render_global_recenter.c) executes the whole
`n.b(III)V`. It subtracts8 from the input coordinates, initializes B/C/D to
INT_MAX and E/F/G to INT_MIN, then traverses X outer, Z middle, Y inner. X/Z
placement uses wrapped `p*16`, the source negative-distance adjustment and
Java integer division. Y placement comes from the loop's `y*16`.

Each reached spatial cell uses `o[((z*q)+y)*p+x]`309. The method captures its
old dirty flag314, invokes the same DK's position method327 and appends that
DK349 only on a clean-to-dirty transition. Position changes preserve their
bound fields and reached C+2 display-list effects on failure. Recenter can
make compiled sections dirty again; their next compilation belongs to the
regular [chunk update phase](chunk-compilation.md).

The entered rebuild shares the whole `n.a()V` body with World attachment. It
reads fancy leaves and render distance, stores H27, closes old DKs, publishes
new spatial/sorted arrays128/149, clears the source dirty and TileEntity Lists,
then constructs and publishes new DKs. If World and the captured App.i are
nonnull, it floors that camera's coordinates, recenters675 and sorts691 before
storing I=2 at696. It preserves partial array publications and completed DKs
when a later child fails. This entered body leaves WorldAccess and N.u with
their existing attachment owners.

JO captures the negatives of the camera's current coordinates once. Each
comparison forms two binary64 sums of squared center offsets, subtracts them
at103, multiplies by1024 at107 and performs Java d2i108. Sub-integer results,
saturation, NaN and signed zero follow those operations. The
[JDK17 object-sort kernel](../../tests/reference/client/render_global_update_sort/README.md)
preserves comparison order, partial array mutations and comparator-contract
exceptions. JO and the update selector's MD have separate comparison rules.

## Subsets and TG buffers

`n.a(IIID)I` clears S4 before traversing `[start,end)`. On pass0 it increments
M32 and one of Q57 (empty pass), N83 (outside the frustum), O116 (occluded while
N.w) or P129. These statistics describe filtering and can differ from the
returned submission count.

Selection rereads the pass flag143, visibility154 and conditional occlusion
flag174. It invokes `dk.a(pass)I`188 and appends the same DK to S209 only when
the returned ID is nonnegative. The helper then captures current N.t.i228 and
interpolates that camera, even when S is empty. This camera can differ from
the outer argument. A null camera fails235 after earlier S/counter effects
and before any TG reset.

The helper resets every T entry322 and groups selected DKs by coarse i/j/k.
It searches every used TG399; the last matching batch wins. A new group
increments the used count before indexing T433 and configures that entry455.
The fixed array's bounds remain observable. For each DK it calls the list
selector again468 and appends that result471. Finally `n.a(ID)V`484 visits
every T entry and calls its draw method20. Only then does the subset return489.

The [TG methods](../../src/client/world/render_batch.c) operate on the original
Ge direct IntBuffer and expose these source effects:

| Method | Stores and cursor behavior |
| --- | --- |
|`a(IIIDDD)V` configure|Store h=true2 before clear9, then origins15/20/25 and separately narrowed camera floats32/39/46. Preserve i.|
|`a(III)Z` match|Return false8 when h is false; otherwise compare all three integer origins.|
|`b()V` reset|Store h=false2 and i=false7. Preserve coordinates, buffer contents, position and limit.|
|`a(I)V` append|Relative put5, then remaining13. Zero remaining invokes the whole draw method20.|
|`a()V` draw|Return7 when inactive. Otherwise flip19 and store i=true25 only when i was false. Positive remaining32 reaches Push38, float origin-minus-camera translation71, CallLists78 and Pop81.|

CallLists reads position..limit without advancing either cursor. Auto-draw
therefore leaves a flipped buffer ready for replay. A later append overwrites
its first cell; a later draw reads the current suffix. Filling it again while
i=true produces zero remaining and no second flip. Configure clears the
cursor span but retains that i flag. The constructor's capacity is65536.

DK's list leaves preserve their own short circuits. `a(I)I` returns-1 at8
when o is false, before reading p. Otherwise p[pass]14 selects-1 or wrapped
C+pass24. `e()Z` returns false8 before p when E is false, then checks p0 and p1
in order. `d()V` calls the same C+2 bounds list6. These existing-object methods
perform no additional DK/NW class activation.

## Occlusion and query ownership

The occlusion branch first polls `[0,16)`357, marks those DKs visible379 and
draws that subset397. The initial endpoint is16 even for a shorter array.
Only later doubled endpoints are clamped to the current sorted-array length.

For each later range, N disables texture, lighting, alpha and fog, masks color
writes458 and depth writes462, polls470 and pushes the matrix473. Empty DKs
can become invisible517; outside-frustum DKs become query-visible544. Eligible
DKs use source float distance581, MathHelper sqrt584 and Java conversion of
`1 + sqrt(distance)/128`596. Matching Java remainders605/610 select a query.
Integer wrap, nonfinite conversions and a zero interval retain their reached
source failures.

The translation subtracts the outer interpolated camera from each coarse
origin in double, narrows to float and applies the delta from the running
translation707. BeginQuery743 uses the real DK.z and target35092; DK.d753
draws its bounds, EndQuery758 closes the source query, then y=true769 records
the waiting result. After Pop778, fresh anaglyph Settings/PX.b reads determine
the color mask. N restores depth writes828, texture834, alpha840 and fog846,
then submits the range858. Lighting remains disabled.

Polling `n.a(II)V` clears the same N.c buffer23 and requests unsigned
RESULT_AVAILABLE34919 at42. It reads absolute element0 at50. Availability
causes y=false63 before clear70 and unsigned RESULT34918 at89, followed by
x=(result!=0)114. An unavailable query stays pending. Neither wrapper advances
the source buffer. LWJGL2.9 requires one remaining integer for the result span.

The [Display query API](../../src/client/display/occlusion.h) uses its cached
ARB functions, generated-name owner and exact context generation. Begin
publishes a retained Names/context right before the void native call. Source
End records completion while the handle keeps its deletion right until
retirement. A failed, issued Begin can require a native abort on the original
context; an unissued pending handle retires without End. Failed cleanup keeps
the same handle for retry. Query-name deletion and context destruction remain
blocked while that right exists. Result values and readiness come from the
device; wrappers preserve sticky GL errors and do not supply visibility.

A valid query owner with zero remaining Ge integers fails the source minimum1
check before the GL call. Polling reports source IllegalArgumentException at42,
preserves the waiting flag/cursor and records no completed query poll. Missing
native Names has earlier admission priority and remains a native-policy failure.

## Failure lifetime and evidence

An admitted submission retains N, the argument camera and supplied Math owner.
It separately retains the grouping camera when reached. App, World, Display,
arenas and dependent class namespaces stay live and exclusive through checked
retirement. The submit slot blocks overlapping World/cull/sky/update/submit
work, detach and final N retirement. A completed retained World attachment is
allowed; an active or failed attachment blocks admission.

Inspection exposes source PCs and frames, counters, grouping progress,
matrix/query state and separate failure/cleanup causes. Source null, index
and arithmetic exceptions remain distinct from native allocation, FP,
provenance and unsupported-device failures. A failure retains completed
source stores, buffer changes and GL effects. For example, TG CallLists78 can
fail after Push, translation and an earlier list. The source Pop is not run
by failure cleanup. The retained frame is terminal and cannot be replayed to
resume at its last PC. Borrowed views expire on mutation or retirement,
including partial retirement.
The stored source failure remains immutable when retirement writes its separate
cleanup cause, including a refused cleanup that leaves the call inspectable.

The finite observations have separate scopes:

| Reference | Original evidence |
| --- | --- |
|[Recenter and JO](../../tests/reference/client/render_global_recenter/README.md)|12 whole recenter calls and19 JO/JDK-sort cases. Prepared DK origins make the differential recenter calls return before GL; moving-DK/rebuild device controls are separate native tests.|
|[TG and DK leaves](../../tests/reference/client/render_batch/README.md)|78 CPU rows and17 real-LWJGL rows. Source fields, cursor/storage state, current color and modelview are compared. TG constructors execute; DK inputs are prepared RAW objects. The short replacement buffer comes from Ge.d.|
|[Query-poll prefixes](../../tests/reference/client/render_global_occlusion/README.md)|14 unchanged private polling calls that return or throw before any ARB call. They cover flag/cursor retention and the unclamped initial range, without supplying query results.|
|[ARB query surface](../../tests/reference/client/occlusion_queries/README.md)|Unchanged pinned LWJGL wrappers on a real native context, with direct-buffer guards/cursors, actual availability/results, driver identity and native-link receipts. Device timing and sample counts belong to that captured profile.|
|[Whole N CPU failures](../../tests/reference/client/render_global_submit_cpu/README.md)|12 unchanged calls before U302, including exact completed R-store counts and nested rebuild/close failure PCs observed through read-only JDI.|
|[Whole N device calls](../../tests/reference/client/render_global_submit/README.md)|Seven profiles and 35 exact rows, including both passes, separate cameras, four complete TG buffers and center pixels. The real-capability 16-entry occlusion case executes its later empty range; issued queries and whole PX frames remain outside this cohort.|

The original game components bind the supplied JAR SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`
and selected method/class bytes. Their capture scripts also record JDK17,
SDK/helper/input hashes, bounded execution and network/native-link guards.
Public replays use the retained observations without distributing the JAR,
SDK binaries or saved game assets. Exact center-pixel comparison uses the
declared RGBA8/depth16 request and its measured original RGBA8/depth24 format.
The component matrix/pixel profiles do not establish a matched original World frame.

The focused Windows replays pass, including native query ownership, zero-buffer
priority and failed TG prefix cleanup. The loaded-World profile passes 704
checks in 12.69 seconds, preserves all 72 focus histories/resources/saves and
reaches the actual frame frontier 496. Its inspected back buffer shows authored
grass terrain and Sky without HUD. These are scoped native results, without a
matched original full-frame claim. The seven-profile N cohort does not qualify
an issued-query N path, including a 17-entry case. Standalone ARB device controls
have their separate scope. Hosted qualification is pending; no new ALL build or
local Linux runtime suite is recorded. The device comparison requires actual
ARB occlusion support.
