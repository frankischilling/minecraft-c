# Slime removal and splitting

Slime removal is a separate operation from fatal damage. The inherited Living
fatal-damage body sets the dying flag and performs drops. The later Living
update advances death age and calls Slime removal after age 20. Slime removal
then publishes children before marking the parent dead. World collection
removal belongs to the enclosing World update.

## Original order

The authority is the supplied client archive with SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Its `uw.K()V` Code attribute contains 144 bytes with SHA-256
`d3fa87376c230baf75e13c90f711bed5b071d09ca2890e8576dc66a3dd060cd5`.

| Operation | Client bytecode |
| --- | --- |
| Read the signed size through the actual DataWatcher getter | `uw.K:1`, `uw.v:6` |
| Capture World and read its remote flag | `uw.K:6/9` |
| Require captured size greater than 1 | `uw.K:17` |
| Require health exactly 0 | `uw.K:24` |
| Allocate a new concrete Slime | `uw.K:63` |
| Capture constructor World and invoke the real constructor | `uw.K:68/71` |
| Resize the child to captured size divided by 2 | `uw.K:81` |
| Read current parent coordinates | `uw.K:87/94/102` |
| Read current parent Random and draw child yaw | `uw.K:110/113` |
| Position the child | `uw.K:120` |
| Capture publication World and insert the child | `uw.K:124/129` |
| Discard the ordinary publication Boolean | `uw.K:132` |
| Advance the four-child loop | `uw.K:133` |
| Invoke inherited removal, setting only the dead flag | `uw.K:140`, `sn.K:2` |

The size remains the value captured before the loop. Coordinates, Random and
World are read again at their listed operations. A publication callback may
change them before the next iteration. Child constructors retain their own
private Random and Java Math entropy; they do not consume the parent's yaw
stream. Offsets use binary32 arithmetic before widening to binary64.

A negative health value skips splitting. A null World still fails after the
size getter even when size would otherwise skip the loop. A wrong boxed
watcher value can therefore fail before the World read. An already dead parent
has no extra removal latch: another direct call can publish another four
children when the original gate still holds.

The watcher read keeps the declared record type separate from the boxed value.
Null watcher, Map, record and Byte receivers retain their reached null faults;
a represented Integer or nonnull String reaches the Byte cast and throws
`ClassCastException`. An unrepresented native payload remains a native refusal.
The getter returns a signed Byte, including wrapped size-setter values.

## Native ownership

One removal body serves direct effect dispatch and the Mob removal caller.
Failed work stays on the same parent and holds its current child. Published
children remain in their real World and Chunk containers. Cleanup releases
native occurrences without replaying Source, restoring fields or removing
published children. Pending removal blocks actor re-entry and terminal World
retirement until checked cleanup completes.

The explicit execution entry uses that same body. Constructor and publication
World captures have separate exact execution tokens. The actual insertion
owns its stable pending and Chunk-close results. Copied diagnostics describe
those results and acquire no close rights. The caller keeps the shared Math,
defining code and complete World/provider/producer graph live through checked
unwind. Cleanup closes only recorded tokens after their actual pending children
are drained; an unrelated current descendant must be closed by its own caller.

A native or shared-Math refusal inside the loop stops before the next child
and the parent's dead store, even when the raw Entity or World return is OK.
Ordinary insertion false is a completed Source return whose Boolean is
discarded. Final native cleanup can instead fail after Source has returned and
stored the parent's dead flag. That case keeps the four published children,
the Source-return receipt and the pending owner while returning false with an
independent canonical native error. The public legacy entry reports a native
release refusal through its existing Entity status channel.

Each new iteration clears only completed, released child frames. A later NEW
failure therefore exposes no prior child's insertion, World values or scope
tokens. Active cleanup and repeated K admission refuse without consuming the
current invocation. A rejected call's error can be retired without touching
older work. The public effect entry keeps failed work with the parent even
when its diagnostic output is null.

## Validation scope

The asset-independent fixtures use actual cached Worlds, Chunks, concrete
constructors, damage and publication callbacks. Direct removal, explicit
execution and delayed Mob-tick controls have separate targets. The
[original packet](../../tests/reference/entity/slime_removal/README.md) executes
25 direct removal calls across 24 profiles. Eleven records per native host
match the declared fields of the unchanged-original capture, executed with
Windows JDK17. Child order,
World/Chunk membership, positions, yaw and the parent's Random stream retain
their exact compared values. Child-constructor entropy remains independent.

The expanded strict MSVC group passes all 29 tests in 4.48 seconds. Linux
Clang 18.1.8 Debug ASan/UBSan with leak detection passes the same 29 tests in
12.20 seconds. A later MSVC repeat passes all 29 in 4.65 seconds after proving
that retiring a rejected call preserves the earlier pending work. The eight execution-fixture modes cover ready publication,
sticky Math failure, callback scope drift, exact native closes, legacy release
failure, loop and final release refusal, and a later child allocation limit. A separate public Mob
test advances death age through 21 real ticks, then observes four child
publications before the dead store and twenty explosion particles afterward.

The preceding Clang 18 ASan/UBSan run with leak detection passes 23 tests in
10.09 seconds. It predates the four release/allocation controls.

## Loaded Windows scene

A saved size-two Slime starts at four health with a live Arrow. Ordinary World
ticks reduce health to zero at tick 5 and replace the parent with four size-one
children at tick 26. The monitor-2 scene passes 863 checks, returns terrain and
HUD, pauses and saves through SDK input, and independently reads back exactly
four living Slimes with no parent or Arrow. The three inspected images show
Slime geometry and death particles, the pause menu and the returned title.
The four-child count comes from live membership and committed NBT. The final
repeat passes 712 checks and finds the same four children at tick 26. Its
first observed zero-health frame is at tick 7 with death age 2. All three
repeat images were inspected. The final complete Windows and Linux suites
pass 1,130 and 976 tests, respectively.

This scene supplies causal gameplay evidence without a direct GUI invocation
receipt for Arrow impact or Slime removal. Complete owned Mob environment and
particle continuation, original delayed-tick comparison, extended play and
original audiovisual or FPS equality remain unqualified.
[Testing](../testing.md#slime-removal-and-splitting) keeps the native phases
and failed attempts separate.
