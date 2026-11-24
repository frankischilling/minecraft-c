# Default Arrow flight

The default client World Entity dispatcher enters the actual Arrow parent through
`mc_arrow_tick_begin`. It follows `sl.w_()V` in source order, using the existing
entered Base, collision, raytrace, nearby-query and presentation implementations.
The original method has 1,537 Code bytes, SHA-256
`f48dde96f4efaaa629c8b097f793f6ed6a2c561293110f9e2b5bd4e2f4e65f32`.
At victim call 741 it enters the [shared owned damage dispatcher](damage-execution.md)
with the actual target, current nullable Living shooter captured at 737 and
literal amount 4. The reached Living, Monster, Wolf and Player bodies share the
caller's execution and Math context.

## Call and lifetime

The caller supplies a genuine held Arrow, the independently held defining World
classes, actual Math namespace and an empty call slot. The World tick context
carries `arrow_code_classes`; the client runtime supplies its actual App World
classes. Defining code is independent of the nullable, mutable Arrow World field.
Reached children retain their exact evaluated World before later arguments.

An admitted call runs synchronously and leaves a retained frame on success or
failure. The frame owns its Arrow occurrence; the Arrow stores a borrowed
backlink. `mc_arrow_tick_inspect` returns borrowed views that expire at checked
retirement. Source completion, ordinary values, raw child status, runtime
ownership failure and reached Math failure remain distinct.

Only a reached hurt call allocates its separate stable damage workspace. That
workspace lives outside the reused child union, and its canonical result remains
available for protected capture, native close and checked actor cleanup. A false
continuation stops the Arrow before later impact work even when raw Entity status
is OK. A completed false gameplay result still takes the original reflection path.

`mc_arrow_tick_retire` drains entered children, actual hit/vector/bounds and Entity
occurrences, exact World tokens and protected producers before releasing Math
and defining code. It clears the backlink before its final Arrow release.
Refusal retains the remaining graph for Native-only retry. The separate
`mc_arrow_tick_native_unwind_step` performs one authorized native action; neither
entry replays a Source getter, callback or body. The World dispatcher retires a
returned call before continuing, or transfers the actual failed call into its
owning diagnostic. World container retirement refuses while that call remains.

At the prior flight-only checkpoint, Windows x64 MSVC measured the retained frame
at 140,088 bytes, including its 106,712-byte work union and 26,520-byte execution
owner. Its existing 2 MiB parent
and diagnostic ceiling remains unchanged; the later reached damage workspace and
protected exception-name copy count against that ceiling. The recorded frame size
does not measure those later allocations, FPS or allocation rate. Child allocations
retain their own contracts and limits.

## Prior recorded native controls

The following table records the earlier flight-only checkpoint, including the
Pig damage frontier that was unentered at that time. It does not qualify the
later owned damage join. Fresh processes constructed real defining owners and
25 stored air Chunks, then used actual Arrow construction, heading and insertion.
No Arrow field, scheduler or Random reset supplied a returned tick. Stone placement
occurred through actual quiet setters before constructing the Arrow.

| Mode | Checks per host | World C entries | Result |
| --- | ---: | ---: | --- |
| Clear air | 191 | 1 | Full common tail and checked retirement |
| Consecutive air | 199 | 8 | Movement, drag, gravity, counters and unchanged Random |
| Stone impact | 199 | 5 | Actual hit and two subsequent embedded ticks |
| Pig frontier | 191 | 1 | Actual target/shooter prefix; damage 741 unentered |
| SDK Source failure | 194 | 3 | Actual callback failure, protected receipt and checked cleanup |
| SDK cleanup retry | 196 | 3 | Separate native capture refusal, retained call and genuine retry |

Windows x64 MSVC Release and Linux Clang 18 Debug with ASan, UBSan and leak
detection each passed these six modes: 1,170 checks across 21 World C entries.
The two SDK modes
each reached one actual sound callback. They placed the real World output directly
in the SDK record and invoked its production cleanup function; they did not execute
the full `Minecraft.k`/App startup/window path. Normal and optimized Python each
passed the separate flight and SDK protocol controls. Five affected regressions
also passed: World ticking, its ABI, Arrow construction, terminal cleanup and NBT.
The complete fifteen-test selection passed in 42.31 seconds on Windows and
21.32 seconds on Linux, without failed or skipped tests. These are finite
component results, with the unfinished behavior below kept separate.

The first Pig control incorrectly expected every victim hold to return before
World retirement. The real nearby query leaves its result in Source `World.R`;
the World ledger owns that surviving Pig occurrence after the local view drains.
The corrected control preserves the list and verifies its identity, exact row,
measured before/after occurrence counts and local-lease balance. Arrow, code,
World-binding and victim-field assertions remain. The earlier failure is retained.

SDK preflight also caught different line endings between a generated bundle and
its digest. No native child launched for that attempt. Public JSON uses canonical
LF with a matching digest; all applied payloads are checked against the candidates.

## Finite unchanged-original observations

Separate fresh processes execute the actual original and native World tick for
one air call, eight consecutive air calls and five calls in the Stone-impact
scenario. Both use
an Arrow starting at `(8.5, 66, 8.5)`, direction `(1, 0, 0)`, speed `0.6f` and
inaccuracy `0.0f`. Constructors, heading, publication and stored-Chunk loading
execute normally. No Source field or Random state is repaired or reset.

Seventeen inserted/returned snapshot pairs retain all 126 fields. Of 1,666 raw
primitive comparisons, 1,615 match exactly, including every observed Arrow,
inherited Entity and bounds primitive. The 51 differences are the World
constructor's `I` wall clock, temporary-Random `g` and World-Random `Q` values.
Both independently seeded Random states also differ. These values remain in the
records; no tolerance, angle wrapping or signed-zero collapse supplies a match.

All recorded within-process alias relations, 68 World-list sequences, 3,400
Chunk-slice sequences and 425 coordinate/insertion pairs agree. Cross-process
numeric object IDs are not compared. The 476 reference cells retain 391 non-null
Java-class/native-storage representation differences and 85 null/null matches.
Six additional original constructor/heading snapshots, with 756 fields, remain
unpaired. Full heap, watcher contents and per-instruction traces are not compared.

Input files also differ. Native setup writes/loads each Chunk in turn; original
setup authors all 25 first. Native metadata contains four explicit zero weather
tags absent from the original input; nine shared values and types match.
Idcounts contents, region timestamps, session locks, outer getter wrappers and
independent constructor entropy retain their separate identities. Complete
capture, source/product guards and final world copies pass independent review.
This establishes the stated finite observations, with whole-method equality and
matched entropy still unqualified.

## Remaining behavior

The [water controls](../../tests/reference/entity/arrow_water/README.md) separately
exercise wet flight, air/water transitions and reached particle failures. The
common tail emits four bubbles before applying water drag. Actual original
observations retain the small Arrow bounds quirk: the eight-tick wet scene has
four wet ticks, three dry ticks, then a wet tick with a new splash. The tests
preserve that sequence rather than forcing a continuously wet result.

The current victim invocation 741 enters owned damage and retains only the actor
work acquired by that actual call. Living sound and knockback, Wolf group/owner
decisions, Player ally alert, and Mob and Player fatal drop/death continuations use
their existing bodies. The [damage execution contract](damage-execution.md) records
the distinct outcomes, actual failure evidence and cleanup order. The prior Pig
frontier above remains evidence of its earlier checkpoint.

The [Player wake join](player-damage-wake.md#wake-during-an-arrow-hit) now enters
GS35, clears Bed occupancy and removes an orphan head or foot through recursive
neighbor notification. Prepared sleeping-Player hits also publish the foot's
Bed Item and retain an actual publication exception through native unwind.
Other neighbor hooks still need owned descendants.
[Delayed Slime splitting](slime-removal.md) has separate component
and loaded-scene coverage. Arbitrary cold import/generation, foreign owner graphs,
pickup/rider paths and broader natural combat encounters remain unfinished or
unqualified. The existing component controls retain their own scope.

The finite observations and native lifetime controls do not establish whole-method
equality, visible-game qualification, sustained play or FPS. The refused
whole-frame replay remains unchanged and is not retried.

## Saved Arrow/Pig fixture coverage

Two retained monitor-2 failures reject extra live Entities outside the prepared
terrain. The later diagnostic reads an actual Chicken and Sheep in the correct
World and namespace, beyond the authored Chunk range. Pig/Player identities
remain stable; focus and returned rendering are valid. The failed predicate
is the scene's exact global cohort check.

The preceding fixture supplies 289 Chunks in `[-8,8]` on both axes. Initial
spawn positions in that range can move by four cumulative differences of two
`nextInt(6)` draws. Their block-coordinate envelope is `[-148,163]`. The fixture
now authors 441 populated Glowstone Chunks in `[-10,10]`, covering blocks
`[-160,175]` before bootstrap. Every input plane and the full saved halo are
checked independently. Existing callers of the shared builder retain their
289-Chunk default.

The runtime cohort, health, namespace, source and save assertions stay intact.
Native spawning, terrain generation, timing and work budgets are unchanged.
The correction prevents these controlled spawn probes from entering generated
border terrain. Normal and optimized Python reject the old incomplete fixture,
missing halo records and corrupted halo planes alongside the existing controls.
This fixture correction establishes no gameplay FPS gain; the earlier uncaptured Bed
dawn assertion retains its separate evidence.

## Retained frame allocation experiment

The parent still allocates its retained frame with `calloc`. A private
`malloc` candidate zeroed the surrounding frame while leaving the work union
to its unchanged mandatory Base initializer. The layout, 2 MiB ceiling, Source
body, owning occurrences and checked cleanup remained the same.

Before timing, all three variants (baseline, candidate and qualification-only
work poison) passed 36 private controls with 7,092 checks. These included the
real World-to-SDK failure/retry paths and six direct-parent code/Math/nullable
World/admission/FP modes. Inspection retained the actual typed causes; FP was
observed as an unresolved parent classification with a selected lower FP cause.
The private results do not qualify later public test registration.

In a new 24-process comparison, the candidate was slower in three of four pairs
for both one and eight Arrows. Production adoption was rejected. The
[complete process table and method](../performance/arrow-world-tick-baseline.md#rejected-frame-initialization-experiment)
retain all warmups, measurements and empty-control variation. This comparison
does not isolate allocation cost or establish an FPS result; the flight and
victim limitations above remain.
