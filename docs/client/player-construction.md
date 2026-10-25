# Local Player construction

`mc_local_player_allocate` allocates the real RAW client `dc` owner. Its immutable
Class relation is `dc -> gs -> ls -> sn`. Existing literal selectors 0 through 25
retain their values; the client-only `LOCAL_PLAYER` selector is 26. EntityList
still has its original 24 registrations. The server rejects the `dc` selector.

`mc_local_player_construct` executes construction only. It does not attach
MovementInput, tick Living/Player, run collision or controller actions, publish
the Player into a World, or render it. Those concrete operations remain required
before a playable client exists. The runtime is C17 and does not call a JVM.

The base constructor dispatches its actual virtual hook to `gs.b()V`, registering
watcher 16/Byte0 after watcher 0 and before the first Living initializer. The
Living constructor then makes all four calls to the same application Math
profile, in bytecode order. Its four formulas preserve each float narrowing and
the source float constants. A native limit that stops watcher 16 leaves every
Living field at its raw default and the Math holder cold. A profile limit that
stops the first Math call retains the watcher pair and Living stores through PC89.

The Player constructor creates the concrete InventoryPlayer with its actual
Player back-reference, 36 main slots and 4 armor slots. It constructs the concrete
45-Slot Player Container, its actual crafting grid/result and recipe preview.
Fields `d` and `e` refer to that same Container. No alternative recipe callback
is provided. The constructor reads the real World remote flag, then its actual
WorldInfo spawn through the source `fd.u()` getter sequence. Source integer
overflow for `spawnY+1`, the double height offset and the inherited position /
rotation store order are preserved. Previous angles remain their raw values.

The local constructor owns three distinct zeroed MouseFilters and stores the
supplied dimension and borrowed client reference. A nonnull client reference
must identify the actual live application client. Source-supported null clients
are accepted. A nonempty Session username produces the source skin URL as a
new immutable UTF16 String; construction performs no network operation.
Null/empty usernames remain distinct, and embedded NULs and lone surrogates are
preserved. The final username aliases the supplied Session String identity.

Session has a concrete native class owner. Its first active use publishes the
actual creative List and appends the 28 real Block static identities in the
original order. Block initialization and resource failures retain the reached
List prefix and latch that class failure. The Session body retains nullable
username/session-ID Strings, and its password stays null. This is the original
`gr` closure, not an empty Session class substitute.

All World, Entity, Registry and Random-profile bindings must belong to the same
live native application. A World keeps its counted binding through Player
retirement. Session classes retain their exact Entity and Registry owners.
Retire Session instances before their class owner; a Player independently holds
its username String and does not retain the Session instance. Retire the Player
through `mc_entity_release` before World retirement, then release World classes,
Registry, Entity classes and the profile in their established ownership order.
Inventory, Container, filter and String pointers returned by the Player snapshot
are borrowed. Do not retire those children independently. All operations and
borrowed inspection views require exclusive, quiescent owner access.

Failures preserve published constructor children and fields. Null World fails
at `gs.<init>`PC68 after base/Living construction and InventoryPlayer creation.
Null Session fails at `dc.<init>`PC103 after completed Player fields and all
three filters. Native argument and namespace rejection occurs before entering
the source constructor. Native allocation limits are resource policies, not a
claim to reproduce every JVM allocation or OutOfMemoryError point. Strings use
an independent maximum of 1048576 UTF16 units; Inventory/Container and direct
Entity construction retain their modules' separate budgets. Native retirement
does not run gameplay close/drop hooks.

The authority is the verified original client JAR, 1465375 bytes, SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Constructor descriptors and exact class/Code hashes are retained in
`tests/reference/entity/player/source-pins.json`. Authored CPU observers execute
unchanged original classes using actual `fd`, `wy`, `rh`, Session, Inventory and
Container owners and a null Minecraft input. They do not construct Minecraft
or initialize graphics/audio. Native fixtures use a real direct-metadata World,
filesystem Store, Registry, dimension and application profile.

Six successful original observations cover all 156 instance fields, the 45 Slots,
watcher pair, filters, aliases and UTF16 Strings; two more reach the source null
failures. Cross-process comparison excludes absolute Entity IDs and three final
random Living fields. Those three formulas are separately replayed on the
original four Math returns recovered from an explicit-seed JDK Random clone of
the observed holder state. The real holder is neither reset nor replaced. Native
unit checks independently confirm that production consumes four returns from
its actual profile. Replaying scalar formulas does not establish equality of
uncontrolled constructor entropy or native/JVM clock observations.

The separate read-only debugger run records 71 checkpoints on unchanged
constructor bytecode. It directly confirms watcher16 before Living initialization,
each of the four Math poststores, and both actual unpublished failed receivers.
The native failed receivers match 303 deterministic fields and 12 bounds fields
from those original receivers at the two source null failure sites. The observer
writes no target fields and invokes no target methods.

## Integrated validation

The full ABI rebuild passes all 254 Windows MSVC tests and all 236 headless
tests each on Linux Clang 18/GCC 13 with ASan, UBSan and leak checks. There are
no failures, disabled tests or skips. All 730 C/header files pass formatting.
Native constructor tests pass 736 checks per compiler; existing Class and
Minecart checks retain their original paired-side coverage.

The [reproducible reference](../../tests/reference/entity/player/README.md)
regenerates observations in fresh JDK 17 processes, verifies actual class
origins and initialization boundaries, and keeps captured answers private.
Its 32 authored probe controls require exact input-rejection diagnostics before
creating a World or output file; sanitizer failures cannot pass as rejection.
The published reference validation JSON retains its initial staging cohort;
the integrated profiles above are the later root results.
